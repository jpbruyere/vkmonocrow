#ifndef VKVG_FONTS_H
#define VKVG_FONTS_H

#include <assert.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <fontconfig/fontconfig.h>

#define FONT_PAGE_SIZE 4096
#define FONT_CACHE_INIT_LAYERS  8

#include "vkvg.h"
#include "vkh_image.h"

typedef struct {
    vec4    bounds;
    vec2i16	bmpDiff;
    uint8_t pageIdx;
}_char_ref;

typedef struct {
    hb_font_t*  hb_font;
    FT_Face     face;
    _char_ref** charLookup;
}_vkvg_font_t;

typedef struct {
    FT_Library		library;
    FcConfig*       config;
    uint8_t*		curPage;	//current page data
    uint8_t			curPageIdx;	//current page index in VkImage array
    VkhImage		cacheTex;	//tex 2d array
    VkFence			uploadFence;
    _vkvg_font_t*	fonts;
    uint8_t			fontsCount;
}_font_cache_t;

void _init_fonts_cache		(VkvgDevice dev);
void _destroy_font_cache	(VkvgDevice dev);
void _select_font_face		(VkvgContext ctx, const char* name);
void _show_text				(VkvgContext ctx, const char* text);
#endif
