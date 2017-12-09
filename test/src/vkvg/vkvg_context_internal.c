#include "vkvg_surface_internal.h"
#include "vkvg_context_internal.h"
#include "vkvg_device_internal.h"


void _check_pathes_array (vkvg_context* ctx){
    if (ctx->sizePathes - ctx->pathPtr > VKVG_ARRAY_THRESHOLD)
        return;
    ctx->sizePathes += VKVG_PATHES_SIZE;
    ctx->pathes = (uint32_t*) realloc (ctx->pathes, ctx->sizePathes*sizeof(uint32_t));
}
void _add_point(vkvg_context* ctx, float x, float y){
    ctx->curPos.x = x;
    ctx->curPos.y = y;
    ctx->points[ctx->pointCount] = ctx->curPos;
    ctx->pointCount++;
}
void _add_point_v2(vkvg_context* ctx, vec2 v){
    ctx->curPos = v;
    ctx->points[ctx->pointCount] = ctx->curPos;
    ctx->pointCount++;
}
void _add_curpos (vkvg_context* ctx){
    ctx->points[ctx->pointCount] = ctx->curPos;
    ctx->pointCount++;
}

void _create_vertices_buff (vkvg_context* ctx){
    vkvg_buffer_create ((VkhDevice*)ctx->pSurf->dev,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        ctx->sizeVertices * sizeof(Vertex), &ctx->vertices);
    vkvg_buffer_create ((VkhDevice*)ctx->pSurf->dev,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        ctx->sizeIndices * sizeof(uint32_t), &ctx->indices);
}
void _add_vertex(vkvg_context* ctx, Vertex v){
    Vertex* pVert = (Vertex*)(ctx->vertices.mapped + ctx->vertCount * sizeof(Vertex));
    *pVert = v;
    ctx->vertCount++;
}
void _set_vertex(vkvg_context* ctx, uint32_t idx, Vertex v){
    Vertex* pVert = (Vertex*)(ctx->vertices.mapped + idx * sizeof(Vertex));
    *pVert = v;
}
void _add_tri_indices_for_rect (vkvg_context* ctx, uint32_t i){
    uint32_t* inds = (uint32_t*)(ctx->indices.mapped + (ctx->indCount * sizeof(uint32_t)));
    inds[0] = i;
    inds[1] = i+2;
    inds[2] = i+1;
    inds[3] = i+1;
    inds[4] = i+2;
    inds[5] = i+3;
    ctx->indCount+=6;
}
void _add_triangle_indices(vkvg_context* ctx, uint32_t i0, uint32_t i1,uint32_t i2){
    uint32_t* inds = (uint32_t*)(ctx->indices.mapped + (ctx->indCount * sizeof(uint32_t)));
    inds[0] = i0;
    inds[1] = i1;
    inds[2] = i2;
    ctx->indCount+=3;
}

void _create_cmd_buff (vkvg_context* ctx){
    ctx->cmd = vkh_cmd_buff_create(ctx->pSurf->dev->vkDev, ctx->pSurf->dev->cmdPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}
void _record_draw_cmd (VkvgContext ctx){
    if (ctx->indCount == ctx->curIndStart)
        return;
    vkCmdDrawIndexed(ctx->cmd, ctx->indCount - ctx->curIndStart, 1, ctx->curIndStart, 0, 1);
    ctx->curIndStart = ctx->indCount;
}

void _submit_wait_and_reset_cmd (VkvgContext ctx){
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                 .commandBufferCount = 1,
                                 .signalSemaphoreCount = 0,
                                 .pSignalSemaphores = NULL,
                                 .waitSemaphoreCount = 0,
                                 .pWaitSemaphores = NULL,
                                 .pWaitDstStageMask = &dstStageMask,
                                 .pCommandBuffers = &ctx->cmd};
    VK_CHECK_RESULT(vkQueueSubmit(ctx->pSurf->dev->queue, 1, &submit_info, ctx->flushFence));
    vkWaitForFences(ctx->pSurf->dev->vkDev,1,&ctx->flushFence,VK_TRUE,UINT64_MAX);
    vkResetFences(ctx->pSurf->dev->vkDev,1,&ctx->flushFence);
    vkResetCommandBuffer(ctx->cmd,0);
}

void _flush_cmd_buff (vkvg_context* ctx){
    _record_draw_cmd(ctx);
    vkCmdEndRenderPass (ctx->cmd);
    vkh_cmd_end (ctx->cmd);
    _submit_wait_and_reset_cmd(ctx);
}
void _init_cmd_buff (vkvg_context* ctx){
    ctx->vertCount = ctx->indCount = ctx->totalPoints = ctx->curIndStart = 0;

    VkClearValue clearValues[1] = {{ { 0.0f, 1.0f, 0.0f, 1.0f } }};
    VkRenderPassBeginInfo renderPassBeginInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                  .renderPass = ctx->pSurf->dev->renderPass,
                                                  .framebuffer = ctx->pSurf->fb,
                                                  .renderArea.extent = {ctx->pSurf->width,ctx->pSurf->height},
                                                  .clearValueCount = 1,
                                                  .pClearValues = clearValues};
    vkh_cmd_begin (ctx->cmd,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkCmdBeginRenderPass (ctx->cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport = {0,0,ctx->pSurf->width,ctx->pSurf->height,0,1};
    vkCmdSetViewport(ctx->cmd, 0, 1, &viewport);

    VkRect2D scissor = {{0,0},{ctx->pSurf->width,ctx->pSurf->height}};
    vkCmdSetScissor(ctx->cmd, 0, 1, &scissor);

    push_constants pc = {
        {(float)ctx->pSurf->width,(float)ctx->pSurf->height},
        {2.0f/(float)ctx->pSurf->width,2.0f/(float)ctx->pSurf->height},
        {-1.f,-1.f},
        {0,0}
    };
    vkCmdPushConstants(ctx->cmd, ctx->pSurf->dev->pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants),&pc);
    //vkCmdPushConstants(ctx->cmd, ctx->pSurf->dev->pipelineLayout,
    //                   VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants),&pc);

    vkCmdSetStencilReference(ctx->cmd,VK_STENCIL_FRONT_AND_BACK, ctx->stencilRef);

    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipeline);
    vkCmdBindDescriptorSets(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipelineLayout,
                            0, 1, &ctx->descriptorSet, 0, NULL);
    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &ctx->vertices.buffer, offsets);
    vkCmdBindIndexBuffer(ctx->cmd, ctx->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
}

void _finish_path (vkvg_context* ctx){
    if (ctx->pathPtr % 2 == 0)//current path is empty
        return;
    //set end index of current path to last point in points array
    ctx->pathes[ctx->pathPtr] = ctx->pointCount - 1;
    _check_pathes_array(ctx);
    ctx->pathPtr++;
}
void _clear_path (vkvg_context* ctx){
    ctx->pathPtr = 0;
    ctx->totalPoints += ctx->pointCount;
    ctx->pointCount = 0;
}
bool _path_is_closed (vkvg_context* ctx, uint32_t ptrPath){
    return (ctx->pathes[ptrPath] == ctx->pathes[ptrPath+1]);
}
uint32_t _get_last_point_of_closed_path(vkvg_context* ctx, uint32_t ptrPath){
    if (ptrPath+2 < ctx->pathPtr)			//this is not the last path
        return ctx->pathes[ptrPath+2]-1;    //last p is p prior to first idx of next path
    return ctx->pointCount-1;				//last point of path is last point of point array
}
