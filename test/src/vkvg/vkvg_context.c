#include "vkvg.h"
#include "vkvg_context_internal.h"
#include "vkvg_fonts.h"

#ifdef DEBUG
static vec2 debugLinePoints[1000];
static uint32_t dlpCount = 0;
#endif

vkvg_context* vkvg_create (vkvg_surface* surf)
{
    vkvg_context* ctx = (vkvg_context*)malloc(sizeof(vkvg_context));

    ctx->sizeVertices = ctx->sizePoints = VKVG_BUFF_SIZE;
    ctx->sizeIndices = VKVG_BUFF_SIZE * 2;
    ctx->sizePathes = VKVG_PATHES_SIZE;
    ctx->curPos.x = ctx->curPos.y = 0;
    ctx->lineWidth = 1;
    ctx->pSurf = surf;

    ctx->flushFence = vkh_fence_create(ctx->pSurf->dev->vkDev);

    ctx->points = (vec2*)malloc (VKVG_BUFF_SIZE*sizeof(vec2));
    ctx->pathes = (uint32_t*)malloc (VKVG_PATHES_SIZE*sizeof(uint32_t));

    _create_vertices_buff(ctx);
    _create_cmd_buff(ctx);

    _init_cmd_buff(ctx);

    return ctx;
}
void vkvg_flush (vkvg_context* ctx){
    if (ctx->indCount == 0)
        return;
    ctx->fillIndCount = ctx->indCount;

#ifdef DEBUG

    vec4 red = {0,0,1,1};
    vec4 green = {0,1,0,1};
    vec4 white = {1,1,1,1};

    int j = 0;
    while (j < dlpCount) {
        _check_vertex_buff_size(ctx);
        _check_index_buff_size(ctx);
        add_line(ctx, debugLinePoints[j], debugLinePoints[j+1],green);
        j+=2;
        add_line(ctx, debugLinePoints[j], debugLinePoints[j+1],red);
        j+=2;
        add_line(ctx, debugLinePoints[j], debugLinePoints[j+1],white);
        j+=2;
    }
    dlpCount = 0;
#endif

    _flush_cmd_buff(ctx);
}

void vkvg_destroy (vkvg_context* ctx)
{
    vkvg_flush(ctx);

    vkDestroyFence(ctx->pSurf->dev->vkDev,ctx->flushFence,NULL);
    vkFreeCommandBuffers(ctx->pSurf->dev->vkDev, ctx->pSurf->dev->cmdPool, 1, &ctx->cmd);

    vkvg_buffer_destroy(&ctx->indices);
    vkvg_buffer_destroy(&ctx->vertices);

    free(ctx->pathes);
    free(ctx->points);
    free(ctx);
}


void vkvg_close_path (vkvg_context* ctx){
    if (ctx->pathPtr % 2 == 0)//current path is empty
        return;
    //check if at least 3 points are present
    if (ctx->pointCount - ctx->pathes [ctx->pathPtr-1] > 2){
        //set end idx of path to the same as start idx
        ctx->pathes[ctx->pathPtr] = ctx->pathes [ctx->pathPtr-1];
        //start new path
        _check_pathes_array(ctx);
        ctx->pathPtr++;
    }
}

void vkvg_line_to (vkvg_context* ctx, float x, float y)
{
    if (ctx->pathPtr % 2 == 0){//current path is empty
        //set start to current idx in point array
        ctx->pathes[ctx->pathPtr] = ctx->pointCount;
        _check_pathes_array(ctx);
        ctx->pathPtr++;
        _add_curpos(ctx);
    }
    _add_point(ctx, x, y);
}
#define ROUND_DOWN(v,p) (floorf(v * p) / p)

float _normalizeAngle(float a)
{
    float res = ROUND_DOWN(fmod(a,2.0f*M_PI),100);
    if (res < 0.0f)
        return res + 2.0f*M_PI;
    else
        return res;
}
void vkvg_arc (vkvg_context* ctx, float xc, float yc, float radius, float a1, float a2){
    float aDiff = a2 - a1;
    float aa1, aa2;
    float step = M_PI/radius;

    aa1 = _normalizeAngle(a1);
    aa2 = aa1 + aDiff;

    //if (aa1 > aa2)
     //   aa2 += M_PI * 2.0;
    float a = aa1;
    vec2 v = {cos(a)*radius + xc, sin(a)*radius + yc};

    if (ctx->pathPtr % 2 == 0){//current path is empty
        //set start to current idx in point array
        ctx->pathes[ctx->pathPtr] = ctx->pointCount;
        _check_pathes_array(ctx);
        ctx->pathPtr++;
        if (!_v2equ(v,ctx->curPos))
            _add_curpos(ctx);
    }

    if (aDiff < 0){
        while(a > aa2){
            v.x = cos(a)*radius + xc;
            v.y = sin(a)*radius + yc;
            _add_point(ctx,v.x,v.y);
            a-=step;
        }
    }else{
        while(a < aa2){
            v.x = cos(a)*radius + xc;
            v.y = sin(a)*radius + yc;
            _add_point(ctx,v.x,v.y);
            a+=step;
        }
    }
    a = aa2;
    v.x = cos(a)*radius + xc;
    v.y = sin(a)*radius + yc;
    if (!_v2equ (v,ctx->curPos))
        _add_point(ctx,v.x,v.y);
}

void vkvg_move_to (vkvg_context* ctx, float x, float y)
{
    _finish_path(ctx);

    ctx->curPos.x = x;
    ctx->curPos.y = y;
}

void add_line(vkvg_context* ctx, vec2 p1, vec2 p2, vec4 col){
    Vertex v = {{p1.x,p1.y},col,{0,0,-1}};
    _add_vertex(ctx, v);
    v.pos = p2;
    _add_vertex(ctx, v);
    uint32_t* inds = (uint32_t*)(ctx->indices.mapped + (ctx->indCount * sizeof(uint32_t)));
    inds[0] = ctx->vertCount - 2;
    inds[1] = ctx->vertCount - 1;
    ctx->indCount+=2;
}

void _build_vb_step (vkvg_context* ctx, Vertex v, double hw, uint32_t iL, uint32_t i, uint32_t iR){
    _check_vertex_buff_size(ctx);
    _check_index_buff_size(ctx);

    double alpha = 0;
    vec2d v0n = _v2LineNorm(ctx->points[iL], ctx->points[i]);
    vec2d v1n = _v2LineNorm(ctx->points[i], ctx->points[iR]);

    vec2d bisec = _v2addd(v0n,v1n);
    bisec = _v2Normd(bisec);
    alpha = acos(v0n.x*v1n.x+v0n.y*v1n.y)/2.0;

    double lh = (double)hw / cos(alpha);
    bisec = _v2dPerpd(bisec);
    bisec = _v2Multd(bisec,lh);

#ifdef DEBUG

    debugLinePoints[dlpCount] = ctx->points[i];
    debugLinePoints[dlpCount+1] = _v2add(ctx->points[i], _vec2dToVec2(_v2Multd(v0n,10)));
    dlpCount+=2;
    debugLinePoints[dlpCount] = ctx->points[i];
    debugLinePoints[dlpCount+1] = _v2add(ctx->points[i], _vec2dToVec2(_v2Multd(v1n,10)));
    dlpCount+=2;
    debugLinePoints[dlpCount] = ctx->points[i];
    debugLinePoints[dlpCount+1] = ctx->points[iR];
    dlpCount+=2;
#endif
    uint32_t firstIdx = ctx->vertCount;
    v.pos = vec2_add(ctx->points[i], _vec2dToVec2(bisec));
    _add_vertex(ctx, v);
    v.pos = _v2sub(ctx->points[i], _vec2dToVec2(bisec));
    _add_vertex(ctx, v);
    _add_tri_indices_for_rect(ctx, firstIdx);
}
typedef struct _ear_clip_point{
    vec2 pos;
    uint32_t idx;
    struct _ear_clip_point* next;
}ear_clip_point;

bool ptInTriangle(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    float dX = p.x-p2.x;
    float dY = p.y-p2.y;
    float dX21 = p2.x-p1.x;
    float dY12 = p1.y-p2.y;
    float D = dY12*(p0.x-p2.x) + dX21*(p0.y-p2.y);
    float s = dY12*dX + dX21*dY;
    float t = (p2.y-p0.y)*dX + (p0.x-p2.x)*dY;
    if (D<0)
        return (s<=0) && (t<=0) && (s+t>=D);
    return (s>=0) && (t>=0) && (s+t<=D);
}

static inline float vec2_zcross (vec2 v1, vec2 v2){
    return v1.x*v2.y-v1.y*v2.x;
}
static inline float ecp_zcross (ear_clip_point* p0, ear_clip_point* p1, ear_clip_point* p2){
    return vec2_zcross (_v2sub (p1->pos, p0->pos), _v2sub (p2->pos, p0->pos));
}

void vkvg_fill (vkvg_context* ctx){

    if (ctx->pathPtr == 0)//nothing to stroke
        return;
    if (ctx->pathPtr % 2 != 0)//current path is no close
        vkvg_close_path(ctx);

    int i = 0, ptrPath = 0;

    uint32_t lastPathPointIdx, iL, iR;
    Vertex v = { .col = ctx->curRGBA };
    v.uv.z = -1;

    while (ptrPath < ctx->pathPtr){
        if (!_path_is_closed(ctx,ptrPath)){
            ptrPath+=2;
            continue;
        }
        lastPathPointIdx = _get_last_point_of_closed_path (ctx, ptrPath);
        uint32_t pathPointCount = lastPathPointIdx - ctx->pathes[ptrPath] + 1;
        uint32_t firstVertIdx = ctx->vertCount;

        ear_clip_point ecps[pathPointCount];
        uint32_t ecps_count = pathPointCount;

        while (i < lastPathPointIdx){
            v.pos = ctx->points[i];
            ear_clip_point ecp = {
                v.pos,
                i+firstVertIdx,
                &ecps[i+1]
            };
            ecps[i] = ecp;
            _add_vertex(ctx, v);
            i++;
        }
        v.pos = ctx->points[i];
        ear_clip_point ecp = {
            v.pos,
            i+firstVertIdx,
            ecps
        };
        ecps[i] = ecp;
        _add_vertex(ctx, v);

        ear_clip_point* ecp_current = ecps;

        while (ecps_count > 3) {
            ear_clip_point* v0 = ecp_current->next,
                    *v1 = ecp_current, *v2 = ecp_current->next->next;
            if (ecp_zcross (v0, v2, v1)<0){
                ecp_current = ecp_current->next;
                continue;
            }
            ear_clip_point* vP = v2->next;
            bool isEar = true;
            while (vP!=v1){
                if (ptInTriangle(vP->pos,v0->pos,v2->pos,v1->pos)){
                    isEar = false;
                    break;
                }
                vP = vP->next;
            }
            if (isEar){
                _add_triangle_indices(ctx, v0->idx,v1->idx,v2->idx);
                v1->next = v2;
                ecps_count --;
            }else
                ecp_current = ecp_current->next;
        }
        if (ecps_count == 3){
            _add_triangle_indices(ctx, ecp_current->next->idx,ecp_current->idx,ecp_current->next->next->idx);
        }

        ptrPath+=2;
    }
    _clear_path(ctx);
}

void vkvg_stroke (vkvg_context* ctx)
{
    _finish_path(ctx);

    if (ctx->pathPtr == 0)//nothing to stroke
        return;

    Vertex v = { .col = ctx->curRGBA };
    v.uv.z = -1;

    float hw = ctx->lineWidth / 2.0;
    int i = 0, ptrPath = 0;

    uint32_t lastPathPointIdx, iL, iR;



    while (ptrPath < ctx->pathPtr){
        uint32_t firstIdx = ctx->vertCount;

        if (_path_is_closed(ctx,ptrPath)){
            lastPathPointIdx = _get_last_point_of_closed_path(ctx,ptrPath);
            iL = lastPathPointIdx;
        }else{
            lastPathPointIdx = ctx->pathes[ptrPath+1];
            vec2d bisec = _v2LineNorm(ctx->points[i], ctx->points[i+1]);
            bisec = _v2dPerpd(bisec);
            bisec = _v2Multd(bisec,hw);

            v.pos = vec2_add(ctx->points[i], _vec2dToVec2(bisec));
            _add_vertex(ctx, v);
            v.pos = _v2sub(ctx->points[i], _vec2dToVec2(bisec));
            _add_vertex(ctx, v);
            _add_tri_indices_for_rect(ctx, firstIdx);

            iL = i++;
        }

        while (i < lastPathPointIdx){
            iR = i+1;
            _build_vb_step(ctx,v,hw,iL,i,iR);
            iL = i++;
        }

        if (!_path_is_closed(ctx,ptrPath)){
            vec2d bisec = _v2LineNorm(ctx->points[i-1], ctx->points[i]);
            bisec = _v2dPerpd(bisec);
            bisec = _v2Multd(bisec,hw);

            v.pos = vec2_add(ctx->points[i], _vec2dToVec2(bisec));
            _add_vertex(ctx, v);
            v.pos = _v2sub(ctx->points[i], _vec2dToVec2(bisec));
            _add_vertex(ctx, v);

            i++;
        }else{
            iR = ctx->pathes[ptrPath];
            _build_vb_step(ctx,v,hw,iL,i,iR);

            uint32_t* inds = (uint32_t*)(ctx->indices.mapped + ((ctx->indCount-6) * sizeof(uint32_t)));
            uint32_t ii = firstIdx;
            inds[1] = ii;
            inds[4] = ii;
            inds[5] = ii+1;
            i++;
        }

        ptrPath+=2;
    }

    _clear_path(ctx);
}

void vkvg_set_rgba (vkvg_context* ctx, float r, float g, float b, float a)
{
    ctx->curRGBA.x = r;
    ctx->curRGBA.y = g;
    ctx->curRGBA.z = b;
    ctx->curRGBA.w = a;
}


void vkvg_select_font_face (vkvg_context* ctx, const char* name){

    _select_font_face (ctx, name);
}
void vkvg_set_font_size (vkvg_context* ctx, uint32_t size){

}
void vkvg_show_text (vkvg_context* ctx, const char* text){
    _show_text(ctx,text);
}
