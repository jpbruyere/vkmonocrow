#ifndef VKVG_CONTEXT_INTERNAL_H
#define VKVG_CONTEXT_INTERNAL_H

#include "vkvg_internal.h"
#include "vkvg.h"
#include "vkvg_buff.h"
#include "vkh_image.h"
#include "vkvg_fonts.h"

#define VKVG_PTS_SIZE				4096
#define VKVG_VBO_SIZE				VKVG_PTS_SIZE * 2
#define VKVG_IBO_SIZE				VKVG_VBO_SIZE * 2
#define VKVG_PATHES_SIZE			128
#define VKVG_ARRAY_THRESHOLD		4

typedef struct{
    vec2 pos;
    vec4 col;
    vec3 uv;
}Vertex;

typedef struct _vkvg_context_t {
    VkvgContext     pPrev;      //double linked list of contexts
    VkvgContext     pNext;

    VkvgSurface		pSurf;
    VkCommandBuffer cmd;
    VkFence			flushFence;
    uint32_t        stencilRef;
    VkhImage        source;
    VkDescriptorSet	descriptorSet;

    //vk buffers, holds data until flush
    vkvg_buff	indices;
    size_t		sizeIndices;
    uint32_t	indCount;

    uint32_t	curIndStart;

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

    _vkvg_font_t  selectedFont;     //hold current face and size before cache addition
    _vkvg_font_t* currentFont;      //font ready for lookup
    VkvgDirection textDirection;
}vkvg_context;

void _check_pathes_array	(VkvgContext ctx);

void _add_point				(VkvgContext ctx, float x, float y);
void _add_point_v2			(VkvgContext ctx, vec2 v);
void _add_curpos			(VkvgContext ctx);

void _create_vertices_buff	(VkvgContext ctx);
void _add_vertex			(VkvgContext ctx, Vertex v);
void _set_vertex			(VkvgContext ctx, uint32_t idx, Vertex v);
void _add_tri_indices_for_rect	(VkvgContext ctx, uint32_t i);
void _add_triangle_indices		(VkvgContext ctx, uint32_t i0, uint32_t i1,uint32_t i2);

void _create_cmd_buff		(VkvgContext ctx);
void _init_cmd_buff			(VkvgContext ctx);
void _flush_cmd_buff		(VkvgContext ctx);
void _record_draw_cmd		(VkvgContext ctx);
void _submit_wait_and_reset_cmd            (VkvgContext ctx);

void _finish_path			(VkvgContext ctx);
void _clear_path			(VkvgContext ctx);
bool _path_is_closed		(VkvgContext ctx, uint32_t ptrPath);
uint32_t _get_last_point_of_closed_path (VkvgContext ctx, uint32_t ptrPath);

void _update_descriptor_sets(VkvgDevice dev, VkvgContext ctx, _font_cache_t* cache);
#endif
