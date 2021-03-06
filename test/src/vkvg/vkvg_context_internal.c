#include "vkvg_surface_internal.h"
#include "vkvg_context_internal.h"
#include "vkvg_device_internal.h"

void _check_pathes_array (VkvgContext ctx){
    if (ctx->sizePathes - ctx->pathPtr > VKVG_ARRAY_THRESHOLD)
        return;
    ctx->sizePathes += VKVG_PATHES_SIZE;
    ctx->pathes = (uint32_t*) realloc (ctx->pathes, ctx->sizePathes*sizeof(uint32_t));
}
void _add_point(VkvgContext ctx, float x, float y){
    ctx->curPos.x = x;
    ctx->curPos.y = y;
    ctx->points[ctx->pointCount] = ctx->curPos;
    ctx->pointCount++;
}
void _add_point_v2(VkvgContext ctx, vec2 v){
    ctx->curPos = v;
    ctx->points[ctx->pointCount] = ctx->curPos;
    ctx->pointCount++;
}
void _add_curpos (VkvgContext ctx){
    ctx->points[ctx->pointCount] = ctx->curPos;
    ctx->pointCount++;
}
float _normalizeAngle(float a)
{
    float res = ROUND_DOWN(fmod(a,2.0f*M_PI),100);
    if (res < 0.0f)
        return res + 2.0f*M_PI;
    else
        return res;
}
void _create_vertices_buff (VkvgContext ctx){
    vkvg_buffer_create ((VkhDevice*)ctx->pSurf->dev,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        ctx->sizeVertices * sizeof(Vertex), &ctx->vertices);
    vkvg_buffer_create ((VkhDevice*)ctx->pSurf->dev,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        ctx->sizeIndices * sizeof(uint32_t), &ctx->indices);
}
void _add_vertex(VkvgContext ctx, Vertex v){
    Vertex* pVert = (Vertex*)(ctx->vertices.mapped + ctx->vertCount * sizeof(Vertex));
    *pVert = v;
    ctx->vertCount++;
}
void _set_vertex(VkvgContext ctx, uint32_t idx, Vertex v){
    Vertex* pVert = (Vertex*)(ctx->vertices.mapped + idx * sizeof(Vertex));
    *pVert = v;
}
void _add_tri_indices_for_rect (VkvgContext ctx, uint32_t i){
    uint32_t* inds = (uint32_t*)(ctx->indices.mapped + (ctx->indCount * sizeof(uint32_t)));
    inds[0] = i;
    inds[1] = i+2;
    inds[2] = i+1;
    inds[3] = i+1;
    inds[4] = i+2;
    inds[5] = i+3;
    ctx->indCount+=6;
}
void _add_triangle_indices(VkvgContext ctx, uint32_t i0, uint32_t i1, uint32_t i2){
    uint32_t* inds = (uint32_t*)(ctx->indices.mapped + (ctx->indCount * sizeof(uint32_t)));
    inds[0] = i0;
    inds[1] = i1;
    inds[2] = i2;
    ctx->indCount+=3;
}
void _create_cmd_buff (VkvgContext ctx){
    ctx->cmd = vkh_cmd_buff_create(ctx->pSurf->dev->vkDev, ctx->pSurf->dev->cmdPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}
void _record_draw_cmd (VkvgContext ctx){
    if (ctx->indCount == ctx->curIndStart)
        return;
    vkCmdDrawIndexed(ctx->cmd, ctx->indCount - ctx->curIndStart, 1, ctx->curIndStart, 0, 1);
    ctx->curIndStart = ctx->indCount;
}

void _submit_ctx_cmd (VkvgContext ctx){
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
}
void _wait_and_reset_ctx_cmd (VkvgContext ctx){
    vkWaitForFences(ctx->pSurf->dev->vkDev,1,&ctx->flushFence,VK_TRUE,UINT64_MAX);
    vkResetFences(ctx->pSurf->dev->vkDev,1,&ctx->flushFence);
    vkResetCommandBuffer(ctx->cmd,0);
}

void _submit_wait_and_reset_cmd (VkvgContext ctx){
    _submit_ctx_cmd(ctx);
    _wait_and_reset_ctx_cmd(ctx);
}
void _explicit_ms_resolve (VkvgContext ctx){
    vkh_image_set_layout (ctx->cmd, ctx->pSurf->imgMS, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    vkh_image_set_layout (ctx->cmd, ctx->pSurf->img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    VkImageResolve re = {
        .extent = {ctx->pSurf->width, ctx->pSurf->height,1},
        .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}
    };

    vkCmdResolveImage(ctx->cmd,
                      ctx->pSurf->imgMS->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      ctx->pSurf->img->image ,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      1,&re);
    vkh_image_set_layout (ctx->cmd, ctx->pSurf->imgMS, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

void _flush_cmd_buff (VkvgContext ctx){
    if (ctx->indCount == 0){
        vkResetCommandBuffer(ctx->cmd,0);
        return;
    }
    _record_draw_cmd        (ctx);
    vkCmdEndRenderPass      (ctx->cmd);
    //_explicit_ms_resolve    (ctx);
    vkh_cmd_end             (ctx->cmd);

    _submit_wait_and_reset_cmd(ctx);
}
void _init_cmd_buff (VkvgContext ctx){
    ctx->vertCount = ctx->indCount = ctx->curIndStart = 0;
    //VkClearValue clearValues[2];
    //clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    //clearValues[1].depthStencil = { 1.0f, 0 };
    VkClearValue clearValues[4] = {
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 1.0f, 0 },
        { 1.0f, 0 }
    };
    VkRenderPassBeginInfo renderPassBeginInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                  .renderPass = ctx->pSurf->dev->renderPass,
                                                  .framebuffer = ctx->pSurf->fb,
                                                  .renderArea.extent = {ctx->pSurf->width,ctx->pSurf->height},
                                                };
                                                  //.clearValueCount = 4,
                                                  //.pClearValues = clearValues};
    vkh_cmd_begin (ctx->cmd,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkCmdBeginRenderPass (ctx->cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport = {0,0,ctx->pSurf->width,ctx->pSurf->height,0,1};
    vkCmdSetViewport(ctx->cmd, 0, 1, &viewport);
    VkRect2D scissor = {{0,0},{ctx->pSurf->width,ctx->pSurf->height}};
    vkCmdSetScissor(ctx->cmd, 0, 1, &scissor);

    /*push_constants pc = {
        {(float)ctx->pSurf->width,(float)ctx->pSurf->height},
        {2.0f/(float)ctx->pSurf->width,2.0f/(float)ctx->pSurf->height},
        {-1.f,-1.f},
    };*/

    vkCmdPushConstants(ctx->cmd, ctx->pSurf->dev->pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants),&ctx->pushConsts);
    //vkCmdPushConstants(ctx->cmd, ctx->pSurf->dev->pipelineLayout,
    //                   VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants),&pc);

    VkDescriptorSet dss[] = {ctx->dsFont,ctx->dsSrc};
    vkCmdBindDescriptorSets(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipelineLayout,
                            0, 2, dss, 0, NULL);
    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &ctx->vertices.buffer, offsets);
    vkCmdBindIndexBuffer(ctx->cmd, ctx->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipeline);
    vkCmdSetStencilReference(ctx->cmd,VK_STENCIL_FRONT_AND_BACK, ctx->stencilRef);
}

void _finish_path (VkvgContext ctx){
    if (ctx->pathPtr % 2 == 0)//current path is empty
        return;
    //set end index of current path to last point in points array
    ctx->pathes[ctx->pathPtr] = ctx->pointCount - 1;
    _check_pathes_array(ctx);
    ctx->pathPtr++;
}
void _clear_path (VkvgContext ctx){
    ctx->pathPtr = 0;
    ctx->pointCount = 0;
}
bool _path_is_closed (VkvgContext ctx, uint32_t ptrPath){
    return (ctx->pathes[ptrPath] == ctx->pathes[ptrPath+1]);
}
uint32_t _get_last_point_of_closed_path(VkvgContext ctx, uint32_t ptrPath){
    if (ptrPath+2 < ctx->pathPtr)			//this is not the last path
        return ctx->pathes[ptrPath+2]-1;    //last p is p prior to first idx of next path
    return ctx->pointCount-1;				//last point of path is last point of point array
}
void _update_source_descriptor_set (VkvgContext ctx){
    VkvgDevice dev = ctx->pSurf->dev;
    VkDescriptorImageInfo descSrcTex = { .imageView = ctx->source->view,
                                          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                          .sampler = ctx->source->sampler };

    VkWriteDescriptorSet writeDescriptorSet = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ctx->dsSrc,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &descSrcTex
    };
    vkUpdateDescriptorSets(dev->vkDev, 1, &writeDescriptorSet, 0, NULL);
}
void _update_font_descriptor_set (VkvgContext ctx){
    VkvgDevice dev = ctx->pSurf->dev;
    _font_cache_t* cache = dev->fontCache;
    VkDescriptorImageInfo descFontTex = { .imageView = cache->cacheTex->view,
                                          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                          .sampler = cache->cacheTex->sampler };

    VkWriteDescriptorSet writeDescriptorSet = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ctx->dsFont,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &descFontTex
    };
    vkUpdateDescriptorSets(dev->vkDev, 1, &writeDescriptorSet, 0, NULL);
}

void _init_descriptor_sets (VkvgContext ctx){
    VkvgDevice dev = ctx->pSurf->dev;
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                              .descriptorPool = dev->descriptorPool,
                                                              .descriptorSetCount = 1,
                                                              .pSetLayouts = &dev->dslFont };
    VK_CHECK_RESULT(vkAllocateDescriptorSets(dev->vkDev, &descriptorSetAllocateInfo, &ctx->dsFont));
    descriptorSetAllocateInfo.pSetLayouts = &dev->dslSrc;
    VK_CHECK_RESULT(vkAllocateDescriptorSets(dev->vkDev, &descriptorSetAllocateInfo, &ctx->dsSrc));
}
void add_line(vkvg_context* ctx, vec2 p1, vec2 p2, vec4 col){
    Vertex v = {{p1.x,p1.y},{0,0,-1}};
    _add_vertex(ctx, v);
    v.pos = p2;
    _add_vertex(ctx, v);
    uint32_t* inds = (uint32_t*)(ctx->indices.mapped + (ctx->indCount * sizeof(uint32_t)));
    inds[0] = ctx->vertCount - 2;
    inds[1] = ctx->vertCount - 1;
    ctx->indCount+=2;
}

void _build_vb_step (vkvg_context* ctx, Vertex v, double hw, uint32_t iL, uint32_t i, uint32_t iR){
    double alpha = 0;
    vec2 v0n = vec2_line_norm(ctx->points[iL], ctx->points[i]);
    vec2 v1n = vec2_line_norm(ctx->points[i], ctx->points[iR]);

    vec2 bisec = vec2_add(v0n,v1n);
    bisec = vec2_norm(bisec);
    alpha = acos(v0n.x*v1n.x+v0n.y*v1n.y)/2.0;

    float lh = (float)hw / cos(alpha);
    bisec = vec2_perp(bisec);
    bisec = vec2_mult(bisec,lh);

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
    v.pos = vec2_add(ctx->points[i], bisec);
    _add_vertex(ctx, v);
    v.pos = vec2_sub(ctx->points[i], bisec);
    _add_vertex(ctx, v);
    _add_tri_indices_for_rect(ctx, firstIdx);
}

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
