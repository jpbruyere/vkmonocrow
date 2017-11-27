#include "vkvg.h"

#include <math.h>
#include "vkvg_internal.h"


#include "vkvg_context_internal.h"

vkvg_context* vkvg_create (vkvg_surface* surf)
{
    vkvg_context* ctx = (vkvg_context*)malloc(sizeof(vkvg_context));

    ctx->sizeVertices = ctx->sizePoints = VKVG_BUFF_SIZE;
    ctx->sizeIndices = VKVG_BUFF_SIZE * 2;
    ctx->sizePathes = VKVG_PATHES_SIZE;
    ctx->pointCount = ctx->vertCount = ctx->indCount = ctx->pathPtr = ctx->totalPoints = 0;
    ctx->curPos.x = ctx->curPos.y = 0;
    ctx->lineWidth = 1;
    ctx->pSurf = surf;

    ctx->points = (vec2*)malloc (VKVG_BUFF_SIZE*sizeof(vec2));
    ctx->pathes = (uint32_t*)malloc (VKVG_PATHES_SIZE*sizeof(uint32_t));

    _create_vertices_buff(ctx);
    _create_cmd_buff(ctx);
    _init_cmd_buff(ctx);

    return ctx;
}
void vkvg_flush (vkvg_context* ctx){
    _flush_cmd_buff(ctx);
}

void vkvg_destroy (vkvg_context* ctx)
{
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


void vkvg_move_to (vkvg_context* ctx, float x, float y)
{
    _finish_path(ctx);

    ctx->curPos.x = x;
    ctx->curPos.y = y;
}

void add_line(vkvg_context* ctx, vec2 p1, vec2 p2, vec4 col){
    Vertex v = {{p1.x,p1.y},col,{0,0}};
    _add_vertex(ctx, v);
    v.pos = p2;
    _add_vertex(ctx, v);
    uint32_t* inds = (uint32_t*)(ctx->indices.mapped + (ctx->indCount * sizeof(uint32_t)));
    inds[0] = ctx->vertCount - 2;
    inds[1] = ctx->vertCount - 1;
    ctx->indCount+=2;
}
void _add_tri_indices_for_rect (vkvg_context* ctx, uint32_t i){
    uint32_t* inds = (uint32_t*)(ctx->indices.mapped + (ctx->indCount * sizeof(uint32_t)));
    uint32_t ii = 2*i;
    inds[0] = ii;
    inds[1] = ii+2;
    inds[2] = ii+1;
    inds[3] = ii+1;
    inds[4] = ii+2;
    inds[5] = ii+3;
    ctx->indCount+=6;
}
#ifdef DEBUG
static vec2 debugLinePoints[100];
static uint32_t dlpCount = 0;
#endif

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
    debugLinePoints[dlpCount+1] = _v2add(ctx->points[i], _vec2dToVec2(_v2Multd(v0n,100)));
    dlpCount+=2;
    debugLinePoints[dlpCount] = ctx->points[i];
    debugLinePoints[dlpCount+1] = _v2add(ctx->points[i], _vec2dToVec2(_v2Multd(v1n,100)));
    dlpCount+=2;
#endif
    v.pos = _v2add(ctx->points[i], _vec2dToVec2(bisec));
    _add_vertex(ctx, v);
    v.pos = _v2sub(ctx->points[i], _vec2dToVec2(bisec));
    _add_vertex(ctx, v);
    _add_tri_indices_for_rect(ctx, i+ctx->totalPoints);
}

void vkvg_stroke (vkvg_context* ctx)
{
    _finish_path(ctx);

#ifdef DEBUG
    dlpCount = 0;
#endif

    if (ctx->pathPtr == 0)//nothing to stroke
        return;

    Vertex v = { .col = ctx->curRGBA };
    float hw = ctx->lineWidth / 2.0;
    int i = 0, ptrPath = 0;

    uint32_t lastPathPointIdx, iL, iR;

    while (ptrPath < ctx->pathPtr){

        if (_path_is_closed(ctx,ptrPath)){
            lastPathPointIdx = _get_last_point_of_closed_path(ctx,ptrPath);
            iL = lastPathPointIdx;
        }else{
            lastPathPointIdx = ctx->pathes[ptrPath+1];
            vec2d bisec = _v2LineNorm(ctx->points[i], ctx->points[i+1]);
            bisec = _v2dPerpd(bisec);
            bisec = _v2Multd(bisec,hw);

            v.pos = _v2add(ctx->points[i], _vec2dToVec2(bisec));
            _add_vertex(ctx, v);
            v.pos = _v2sub(ctx->points[i], _vec2dToVec2(bisec));
            _add_vertex(ctx, v);
            _add_tri_indices_for_rect(ctx, i+ctx->totalPoints);

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

            v.pos = _v2add(ctx->points[i], _vec2dToVec2(bisec));
            _add_vertex(ctx, v);
            v.pos = _v2sub(ctx->points[i], _vec2dToVec2(bisec));
            _add_vertex(ctx, v);

            i++;
        }else{
            iR = ctx->pathes[ptrPath];
            _build_vb_step(ctx,v,hw,iL,i,iR);

            uint32_t* inds = (uint32_t*)(ctx->indices.mapped + ((ctx->indCount-6) * sizeof(uint32_t)));
            uint32_t ii = (ctx->totalPoints + ctx->pathes[ptrPath])*2;
            inds[1] = ii;
            inds[4] = ii;
            inds[5] = ii+1;
            i++;
        }

        ptrPath+=2;
    }

    ctx->fillIndCount = ctx->indCount;

#ifdef DEBUG

    vec4 red = {0,0,1,1};
    vec4 green = {0,1,0,1};

    int j = 0;
    while (j < dlpCount) {
        add_line(ctx, debugLinePoints[j], debugLinePoints[j+1],green);
        j+=2;
        add_line(ctx, debugLinePoints[j], debugLinePoints[j+1],red);
        j+=2;
    }
#endif

    _clear_path(ctx);
}

void vkvg_set_rgba (vkvg_context* ctx, float r, float g, float b, float a)
{
    ctx->curRGBA.x = r;
    ctx->curRGBA.y = g;
    ctx->curRGBA.z = b;
    ctx->curRGBA.w = a;
}


