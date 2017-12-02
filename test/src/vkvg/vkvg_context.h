#ifndef VKVG_CONTEXT_H
#define VKVG_CONTEXT_H

#include "vkvg.h"


typedef struct vkvg_context_t {
	vkvg_surface* pSurf;
	VkCommandBuffer cmd;
	VkFence flushFence;

	//vk buffers, holds data until flush
	vkvg_buff indices;
	size_t sizeIndices;
	uint32_t indCount;

	uint32_t fillIndCount;

	vkvg_buff vertices;
	size_t sizeVertices;
	uint32_t vertCount;

	//pathes, exists until stroke of fill
	vec2* points;
	size_t sizePoints;
	uint32_t pointCount;
	uint32_t totalPoints;
	uint32_t pathPtr;
	uint32_t* pathes;
	size_t sizePathes;

	vec2 curPos;
	vec4 curRGBA;
	float lineWidth;
}vkvg_context;

vkvg_context* vkvg_create	(vkvg_surface* surf);
void vkvg_destroy			(vkvg_context* ctx);

void vkvg_flush				(vkvg_context* ctx);

void vkvg_close_path		(vkvg_context* ctx);
void vkvg_line_to			(vkvg_context* ctx, float x, float y);
void vkvg_move_to			(vkvg_context* ctx, float x, float y);
void vkvg_arc				(vkvg_context* ctx, float xc, float yc, float radius, float a1, float a2);
void vkvg_stroke			(vkvg_context* ctx);
void vkvg_fill				(vkvg_context* ctx);
void vkvg_set_rgba			(vkvg_context* ctx, float r, float g, float b, float a);

void vkvg_select_font_face	(vkvg_context* ctx, const char* name);
void vkvg_set_font_size		(vkvg_context* ctx, uint32_t size);
void vkvg_show_text			(vkvg_context* ctx, const char* text);
#endif
