#ifndef VKVG_CONTEXT_INTERNAL_H
#define VKVG_CONTEXT_INTERNAL_H

#include "vkvg_context.h"
#include "vkvg_internal.h"

#define VKVG_BUFF_SIZE				256
#define VKVG_PATHES_SIZE			16
#define VKVG_ARRAY_THRESHOLD		16

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
