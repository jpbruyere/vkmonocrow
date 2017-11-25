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
    ctx->pointCount = ctx->vertCount = ctx->indCount = ctx->pathPtr = 0;
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

void vkvg_stroke (vkvg_context* ctx)
{
    _finish_path(ctx);

    if (ctx->pathPtr == 0)//nothing to stroke
        return;

    Vertex v = { .col = ctx->curRGBA };
    float hw = ctx->lineWidth / 2.0;
    int i = 0, ptrPath = 0;



    //vec2 v0,vl,vr;
    uint32_t lastPathPointIdx,iL, ptrVertex = ctx->vertCount;

    while (ptrPath < ctx->pathPtr){

        if (_path_is_closed(ctx,ptrPath)){
            lastPathPointIdx = _get_last_point_of_closed_path(ctx,ptrPath);
            iL = lastPathPointIdx;
        }else{
            lastPathPointIdx = ctx->pathes[ptrPath+1];
            iL = ctx->pathes[ptrPath];
        }
        uint32_t* inds;
        uint32_t ii;
        while (i < lastPathPointIdx){
            _check_vertex_buff_size(ctx);
            _check_index_buff_size(ctx);
            vec2 vp = _v2perp(ctx->points[iL], ctx->points[i+1], hw);
            v.pos = _v2add(ctx->points[i], vp);
            _add_vertex(ctx, v);
            v.pos = _v2sub(ctx->points[i], vp);
            _add_vertex(ctx, v);
            inds = (uint32_t*)(ctx->indices.mapped + ctx->indCount);
            ii = 2*i;
            inds[0] = ii;
            inds[1] = ii+2;
            inds[2] = ii+1;
            inds[3] = ii+1;
            inds[4] = ii+2;
            inds[5] = ii+3;
            ctx->indCount+=6;
            iL = i++;
        }

        if (!_path_is_closed(ctx,ptrPath)){
            vec2 vp = _v2perp(ctx->points[iL], ctx->points[i], hw);
            v.pos = _v2add(ctx->points[i], vp);
            _add_vertex(ctx, v);
            v.pos = _v2sub(ctx->points[i], vp);
            _add_vertex(ctx, v);
            i++;
        }else{
            ii = ctx->pathes[ptrPath]*2;
            inds[1] = ii;
            inds[4] = ii;
            inds[5] = ii+1;
        }

        ptrPath+=2;
    }

}

void vkvg_set_rgba (vkvg_context* ctx, float r, float g, float b, float a)
{
    ctx->curRGBA.x = r;
    ctx->curRGBA.y = g;
    ctx->curRGBA.z = b;
    ctx->curRGBA.w = a;
}


