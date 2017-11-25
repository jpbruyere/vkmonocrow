#include <vulkan/vulkan.h>
vec2 _v2perp(vec2 a, vec2 b, float length)
{
	vec2 d = {a.x - b.x, a.y - b.y};
	float m = sqrtf (d.x*d.x + d.y*d.y);
	vec2 r = {-d.y/m*length, d.x/m*length};
	return r;
}

vec2 _v2add (vec2 a, vec2 b){
	vec2 r = {a.x + b.x, a.y + b.y};
	return r;
}
vec2 _v2sub (vec2 a, vec2 b){
	vec2 r = {a.x - b.x, a.y - b.y};
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

void _create_cmd_buff (vkvg_context* ctx){
	ctx->cmd = vkh_cmd_buff_create(ctx->pSurf->dev->vkDev, ctx->pSurf->dev->cmdPool);
}
void _init_cmd_buff (vkvg_context* ctx){
	VkClearValue clearValues[1] = {{ { 1.0f, 0.0f, 0.0f, 1.0f } }};
	VkRenderPassBeginInfo renderPassBeginInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
												  .renderPass = ctx->pSurf->dev->renderPass,
												  .framebuffer = ctx->pSurf->fb,
												  .renderArea.extent = {ctx->pSurf->width,ctx->pSurf->height},
												  .clearValueCount = 1,
												  .pClearValues = clearValues };
	vkh_cmd_begin (ctx->cmd,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vkCmdBeginRenderPass (ctx->cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	//vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pSurf->dev->pipeline);

	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &ctx->vertices.buffer, offsets);
	vkCmdBindIndexBuffer(ctx->cmd, ctx->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
}
void _flush_cmd_buff (vkvg_context* ctx){
	vkCmdDraw (ctx->cmd, ctx->vertCount, 1, 0, 0);
	vkCmdDrawIndexed(ctx->cmd, ctx->indCount, 1, 0, 0, 1);
	vkCmdEndRenderPass (ctx->cmd);
	vkh_cmd_end (ctx->cmd);
}
void _finish_path (vkvg_context* ctx){
	if (ctx->pathPtr % 2 == 0)//current path is empty
		return;
	//set end index of current path to last point in points array
	ctx->pathes[ctx->pathPtr] = ctx->pointCount - 1;
	_check_pathes_array(ctx);
	ctx->pathPtr++;
}
static inline bool _path_is_closed (vkvg_context* ctx, uint32_t ptrPath){
	return (ctx->pathes[ptrPath] == ctx->pathes[ptrPath+1]);
}
uint32_t _get_last_point_of_closed_path(vkvg_context* ctx, uint32_t ptrPath){
	if (ptrPath+2 < ctx->pathPtr)      //this is the last path
		return ctx->pathes[ptrPath+2]-1;    //last p is p prior to first idx of next path
	return ctx->pointCount-1;       //last point of path is last point of point array
}
