#ifndef VKVG_CONTEXT_INTERNAL_H
#define VKVG_CONTEXT_INTERNAL_H

#include "vkvg.h"
#include "vkvg_internal.h"
#include "vkvg_buff.h"

#define VKVG_BUFF_SIZE				256
#define VKVG_PATHES_SIZE			16
#define VKVG_ARRAY_THRESHOLD		16

typedef struct _vkvg_context_t {
	VkvgSurface		pSurf;
	VkCommandBuffer cmd;
	VkFence			flushFence;

	//vk buffers, holds data until flush
	vkvg_buff	indices;
	size_t		sizeIndices;
	uint32_t	indCount;

	uint32_t	fillIndCount;

	vkvg_buff	vertices;
	size_t		sizeVertices;
	uint32_t	vertCount;

	//pathes, exists until stroke of fill
	vec2*		points;
	size_t		sizePoints;
	uint32_t	pointCount;
	uint32_t	totalPoints;
	uint32_t	pathPtr;
	uint32_t*	pathes;
	size_t		sizePathes;

	vec2		curPos;
	vec4		curRGBA;
	float		lineWidth;

	void*		curFont;
	VkvgDirection textDirection;
}vkvg_context;

void _check_pathes_array	(vkvg_context* ctx);
void _check_point_array		(vkvg_context* ctx);
void _check_vertex_buff_size(vkvg_context* ctx);
void _check_index_buff_size (vkvg_context* ctx);

void _add_point				(vkvg_context* ctx, float x, float y);
void _add_point_v2			(vkvg_context* ctx, vec2 v);
void _add_curpos			(vkvg_context* ctx);

void _create_vertices_buff	(vkvg_context* ctx);
void _add_vertex			(vkvg_context* ctx, Vertex v);
void _set_vertex			(vkvg_context* ctx, uint32_t idx, Vertex v);
void _add_tri_indices_for_rect	(vkvg_context* ctx, uint32_t i);
void _add_triangle_indices		(vkvg_context* ctx, uint32_t i0, uint32_t i1,uint32_t i2);

void _create_cmd_buff		(vkvg_context* ctx);
void _flush_cmd_buff		(vkvg_context* ctx);
void _init_cmd_buff			(vkvg_context* ctx);

void _finish_path			(vkvg_context* ctx);
void _clear_path			(vkvg_context* ctx);
bool _path_is_closed		(vkvg_context* ctx, uint32_t ptrPath);
uint32_t _get_last_point_of_closed_path (vkvg_context* ctx, uint32_t ptrPath);

#endif
