#include <vulkan/vulkan.h>
#include "vkvg.h"
vec2d _v2LineNorm(vec2 a, vec2 b)
{
	vec2d d = {b.x - a.x, b.y - a.y};
	double md = sqrt (d.x*d.x + d.y*d.y);
	d.x/=md;
	d.y/=md;
	return d;
}
static inline double _v2Lengthd(vec2d v){
	return sqrt (v.x*v.x + v.y*v.y);
}
static inline float _v2Length(vec2 v){
	return sqrt (v.x*v.x + v.y*v.y);
}
vec2d _v2Normd(vec2d a)
{
	double m = sqrt (a.x*a.x + a.y*a.y);
	vec2d d = {a.x/m, a.y/m};
	return d;
}
static inline vec2d _v2Multd(vec2d a, double m){
	vec2d r = {a.x*m,a.y*m};
	return r;
}

vec2d _v2LineNormd(vec2d a, vec2d b)
{
	vec2d d = {b.x - a.x, b.y - a.y};
	double md = sqrt (d.x*d.x + d.y*d.y);
	d.x/=md;
	d.y/=md;
	return d;
}
static inline vec2d _v2dPerpd (vec2d a){
	vec2d vp = {a.y, -a.x};
	return vp;
}
vec2d _v2perpNorm(vec2 a, vec2 b)
{
	vec2d d = {b.x - a.x, b.y - a.y};
	double md = sqrt (d.x*d.x + d.y*d.y);
	vec2d vpn = {d.y/md, -d.x/md};
	return vpn;
}
vec2 _vec2dToVec2(vec2d vd){
	vec2 v = {vd.x,vd.y};
	return v;
}
vec2 _v2perpNorm2(vec2 a, vec2 b)
{
	vec2 c = {a.x - b.x, a.y - b.y};
	vec2 d = {-c.y/c.x, 1};
//	double md = sqrt (d.x*d.x + d.y*d.y);
//	d.x/=md;
//	d.y/=md;
	return d;
}
vec2 vec2_add (vec2 a, vec2 b){
	vec2 r = {a.x + b.x, a.y + b.y};
	return r;
}
vec2d _v2addd (vec2d a, vec2d b){
	vec2d r = {a.x + b.x, a.y + b.y};
	return r;
}
vec2 _v2sub (vec2 a, vec2 b){
	vec2 r = {a.x - b.x, a.y - b.y};
	return r;
}
vec2d _v2subd (vec2d a, vec2d b){
	vec2d r = {a.x - b.x, a.y - b.y};
	return r;
}
bool _v2equ (vec2 a, vec2 b){
	if ((a.x == b.x) && (a.y == b.y))
		return true;
	return false;
}

void _check_pathes_array (vkvg_context* ctx){
	if (ctx->sizePathes - ctx->pathPtr > VKVG_ARRAY_THRESHOLD)
		return;
	ctx->sizePathes += VKVG_PATHES_SIZE;
	ctx->pathes = (uint32_t*) realloc (ctx->pathes, ctx->sizePathes*sizeof(uint32_t));
}
void _check_point_array (vkvg_context* ctx){
	if (ctx->sizePoints - ctx->pointCount > VKVG_ARRAY_THRESHOLD)
		return;
	ctx->sizePoints += VKVG_BUFF_SIZE;
	ctx->points = (vec2*) realloc (ctx->points, ctx->sizePoints*sizeof(vec2));
}
void _add_point(vkvg_context* ctx, float x, float y){
	_check_point_array(ctx);
	ctx->curPos.x = x;
	ctx->curPos.y = y;
	ctx->points[ctx->pointCount] = ctx->curPos;
	ctx->pointCount++;
}
void _add_point_v2(vkvg_context* ctx, vec2 v){
	_check_point_array(ctx);
	ctx->curPos = v;
	ctx->points[ctx->pointCount] = ctx->curPos;
	ctx->pointCount++;
}
void _add_curpos (vkvg_context* ctx){
	_check_point_array(ctx);
	ctx->points[ctx->pointCount] = ctx->curPos;
	ctx->pointCount++;
}

void _create_vertices_buff (vkvg_context* ctx){
	vkvg_buffer_create ((vkh_device*)ctx->pSurf->dev,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		ctx->sizeVertices * sizeof(Vertex), &ctx->vertices);
	vkvg_buffer_create ((vkh_device*)ctx->pSurf->dev,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		ctx->sizeIndices * sizeof(uint32_t), &ctx->indices);
}
void _create_cmd_buff (vkvg_context* ctx){
	ctx->cmd = vkh_cmd_buff_create(ctx->pSurf->dev->vkDev, ctx->pSurf->dev->cmdPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}
void _flush_cmd_buff (vkvg_context* ctx){
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &ctx->vertices.buffer, offsets);
	vkCmdBindIndexBuffer(ctx->cmd, ctx->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(ctx->cmd, ctx->fillIndCount, 1, 0, 0, 1);
#ifdef DEBUG
	vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipelineLineList);
	vkCmdDrawIndexed(ctx->cmd, ctx->indCount - ctx->fillIndCount, 1, ctx->fillIndCount, 0, 1);
#endif
	vkCmdEndRenderPass (ctx->cmd);
	vkh_cmd_end (ctx->cmd);
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
	vkResetCommandBuffer(ctx->cmd,0);
	_init_cmd_buff(ctx);
}
void _init_cmd_buff (vkvg_context* ctx){
	ctx->pointCount = ctx->vertCount = ctx->indCount = ctx->pathPtr = ctx->totalPoints = 0;
	VkClearValue clearValues[1] = {{ { 0.0f, 0.0f, 0.0f, 0.0f } }};
	VkRenderPassBeginInfo renderPassBeginInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
												  .renderPass = ctx->pSurf->dev->renderPass,
												  .framebuffer = ctx->pSurf->fb,
												  .renderArea.extent = {ctx->pSurf->width,ctx->pSurf->height},
												  .clearValueCount = 1,
												  .pClearValues = clearValues };
	vkh_cmd_begin (ctx->cmd,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vkCmdBeginRenderPass (ctx->cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport = {0,0,ctx->pSurf->width,ctx->pSurf->height,0,1};
	vkCmdSetViewport(ctx->cmd, 0, 1, &viewport);

	VkRect2D scissor = {{0,0},{ctx->pSurf->width,ctx->pSurf->height}};
	vkCmdSetScissor(ctx->cmd, 0, 1, &scissor);
	//vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipelineWired);
}
void _check_vertex_buff_size (vkvg_context* ctx){
	if (ctx->sizeVertices - ctx->vertCount > VKVG_ARRAY_THRESHOLD)
		return;
	ctx->sizeVertices += VKVG_BUFF_SIZE;
	vkvg_buffer_increase_size(&ctx->vertices, VKVG_BUFF_SIZE * sizeof(Vertex));
}
void _check_index_buff_size (vkvg_context* ctx){
	if (ctx->sizeIndices - ctx->indCount > VKVG_ARRAY_THRESHOLD)
		return;
	ctx->sizeIndices += VKVG_BUFF_SIZE * 2;
	vkvg_buffer_increase_size(&ctx->indices, VKVG_BUFF_SIZE * sizeof(uint32_t) * 2);
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
static inline bool _path_is_closed (vkvg_context* ctx, uint32_t ptrPath){
	return (ctx->pathes[ptrPath] == ctx->pathes[ptrPath+1]);
}
uint32_t _get_last_point_of_closed_path(vkvg_context* ctx, uint32_t ptrPath){
	if (ptrPath+2 < ctx->pathPtr)			//this is not the last path
		return ctx->pathes[ptrPath+2]-1;    //last p is p prior to first idx of next path
	return ctx->pointCount-1;				//last point of path is last point of point array
}
