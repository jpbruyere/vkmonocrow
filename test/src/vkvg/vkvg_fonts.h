#ifndef VKVG_FONTS_H
#define VKVG_FONTS_H

#include <assert.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#define FONT_PAGE_SIZE 2048
#define FONT_CACHE_INIT_LAYERS  4

#include "vkvg.h"

typedef struct {
	vec4    bounds;
	vec2i16	bmpDiff;
	uint8_t pageIdx;
}_char_ref;

typedef struct {
	uint8_t*    curPage;
	uint8_t     curPageIdx;
	FT_Library  library;
	hb_font_t*  hb_font;
	FT_Face     face;
	vkh_image   cacheTex;
	VkFence     uploadFence;
	_char_ref*  charLookup[0xFFFF];
}_font_cache_t;

void _init_fonts_cache		(vkvg_device* dev);
void _destroy_font_cache	(vkvg_device* dev);
void _select_font_face		(vkvg_context* ctx, const char* name);
void _show_text				(vkvg_context* ctx, const char* text);
#endif
