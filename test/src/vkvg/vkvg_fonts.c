#include "vkvg_fonts.h"
#include "vkvg_internal.h"
#include "vkvg_context_internal.h"
#include "vkvg_surface_internal.h"
#include "vkvg_device_internal.h"

#include "vkh_buffer.h"

#include <stdio.h>
#include <string.h>

int ptSize = 16<<6;

void _init_fonts_cache (VkvgDevice dev){
    _font_cache_t* cache = (_font_cache_t*)malloc(sizeof(_font_cache_t));
    _font_cache_t fc = {};
    *cache = fc;

    assert(!FT_Init_FreeType(&cache->library));

    vkh_tex2d_array_create (dev, VK_FORMAT_R8_UNORM, FONT_PAGE_SIZE, FONT_PAGE_SIZE,
                            FONT_CACHE_INIT_LAYERS,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                            &cache->cacheTex);
    vkh_image_create_descriptor (&cache->cacheTex, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_ASPECT_COLOR_BIT,
                                 VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);

    cache->uploadFence = vkh_fence_create(dev->vkDev);

    dev->fontCache = cache;
}
void _destroy_font_cache (VkvgDevice dev){
    _font_cache_t* cache = (_font_cache_t*)dev->fontCache;
    if (cache->curPage != NULL)
        free (cache->curPage);

    for (int i = 0; i < cache->fontsCount; ++i) {
        _vkvg_font_t f = cache->fonts[i];

        for (int g = 0; g < f.face->num_glyphs; ++g) {
            if (f.charLookup[g]!=NULL)
                free(f.charLookup[g]);
        }

        FT_Done_Face (f.face);
        hb_font_destroy (f.hb_font);

        free(f.charLookup);
    }

    free(cache->fonts);

    vkh_image_destroy (&cache->cacheTex);
    vkDestroyFence(dev->vkDev,cache->uploadFence,NULL);
    free (dev->fontCache);
}

void _dump_glyphs (FT_Face face){
    FT_GlyphSlot    slot;
    char gname[256];

    for (int i = 0; i < face->num_glyphs; ++i) {
        assert(!FT_Load_Glyph(face,i,FT_LOAD_RENDER));
        slot = face->glyph;

        FT_Get_Glyph_Name(face,i,gname,256);


        printf("glyph: %s (%d,%d;%d), max advance:%d\n", gname,
               slot->bitmap.width, slot->bitmap.rows, slot->bitmap.pitch,
               face->size->metrics.max_advance/64);
    }
}

void _upload_font_curPage (VkvgDevice dev){
    _font_cache_t*  cache = (_font_cache_t*)dev->fontCache;
    vkh_buffer buff = {};
    uint32_t buffLength = FONT_PAGE_SIZE*FONT_PAGE_SIZE*sizeof(uint8_t);
    vkh_buffer_create(dev,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      buffLength, &buff);

    vkh_buffer_map(&buff);
    memcpy(buff.mapped, cache->curPage, buffLength);
    vkh_buffer_unmap(&buff);

    VkCommandBuffer cmd = vkh_cmd_buff_create(dev->vkDev,dev->cmdPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    vkh_cmd_begin (cmd,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    set_image_layout (cmd, cache->cacheTex.image, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy bufferCopyRegion = { .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,cache->curPageIdx,1},
                                           .imageExtent = {FONT_PAGE_SIZE,FONT_PAGE_SIZE,1}};

    vkCmdCopyBufferToImage(cmd, buff.buffer, cache->cacheTex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

    set_image_layout(cmd, cache->cacheTex.image, VK_IMAGE_ASPECT_COLOR_BIT,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    VK_CHECK_RESULT(vkEndCommandBuffer(cmd));

    vkh_cmd_submit(dev->queue,&cmd,cache->uploadFence);
    vkWaitForFences(dev->vkDev,1,&cache->uploadFence,VK_TRUE,UINT64_MAX);

    vkFreeCommandBuffers(dev->vkDev,dev->cmdPool, 1, &cmd);

    vkh_buffer_destroy(&buff);
}

void _build_face_tex (VkvgDevice dev, const char* name){
    _font_cache_t*  cache = (_font_cache_t*)dev->fontCache;
    cache->fontsCount++;
    if (cache->fontsCount == 1)
        cache->fonts = (_vkvg_font_t*) malloc (cache->fontsCount * sizeof(_vkvg_font_t));
    else
        cache->fonts = (_vkvg_font_t*) realloc (cache->fonts, cache->fontsCount * sizeof(_vkvg_font_t));

    _vkvg_font_t f = {};

    assert(!FT_New_Face(cache->library, name, 0, &f.face));
    assert(!FT_Set_Char_Size(f.face, 0, ptSize, dev->hdpi, dev->vdpi ));
    f.hb_font = hb_ft_font_create(f.face, NULL);
    f.charLookup = (_char_ref*)calloc(f.face->num_glyphs,sizeof(_char_ref));

    FT_GlyphSlot    slot;
    FT_Bitmap       bmp;

    int penX = 0,
        penY = 0,
        maxBmpHeight = 0;

    if (cache->curPage == NULL)
        cache->curPage = (uint8_t*)malloc(FONT_PAGE_SIZE*FONT_PAGE_SIZE*sizeof(uint8_t));

    uint8_t* data = cache->curPage;

    FT_UInt   gindex;
    FT_ULong charcode = FT_Get_First_Char( f.face, &gindex );
    while ( gindex != 0)
    {
        assert(!FT_Load_Glyph(f.face,gindex,FT_LOAD_RENDER));

        slot = f.face->glyph;
        bmp = slot->bitmap;

        if (penX+bmp.width > FONT_PAGE_SIZE){
            penX = 0;
            penY += maxBmpHeight;
            maxBmpHeight = 0;
        }

        if (penY+bmp.rows > FONT_PAGE_SIZE){
            _upload_font_curPage(dev);
            cache->curPageIdx++;
            memset(cache->curPage,0,FONT_PAGE_SIZE*FONT_PAGE_SIZE*sizeof(uint8_t));

            penX = 0;
            penY = 0;
            maxBmpHeight = 0;
        }

        for(int y=0; y<bmp.rows; y++) {
            for(int x=0; x<bmp.width; x++)
                data[ penX + x + (penY + y)* FONT_PAGE_SIZE ] =
                    bmp.buffer[x + y * bmp.width];
        }

        _char_ref* cr = (_char_ref*)malloc(sizeof(_char_ref));
        vec4 uvBounds = {
            (float)penX / (float)FONT_PAGE_SIZE,
            (float)penY / (float)FONT_PAGE_SIZE,
            bmp.width,
            bmp.rows};
        cr->bounds = uvBounds;
        cr->pageIdx = cache->curPageIdx;
        cr->bmpDiff.x = slot->bitmap_left;
        cr->bmpDiff.y = slot->bitmap_top;

        f.charLookup[gindex] = cr;

        if (bmp.rows>maxBmpHeight)
            maxBmpHeight=bmp.rows;

        penX += bmp.width;

        charcode = FT_Get_Next_Char( f.face, charcode, &gindex );
    }

    if (penX | penY) {
        _upload_font_curPage(dev);
    }
    cache->fonts[cache->fontsCount-1] = f;
    cache->curPage = data;
}

void _select_font_face (VkvgContext ctx, const char* name){
    _font_cache_t*  cache = (_font_cache_t*)ctx->pSurf->dev->fontCache;

    _build_face_tex(ctx->pSurf->dev, name);

    ctx->curFont =  &cache->fonts[cache->fontsCount-1];
}
void _show_texture (vkvg_context* ctx){
    Vertex vs[] = {
        {{0,0},      {1,1,1,1},{0,0,0}},
        {{0,2048},    {1,1,1,1},{0,1,0}},
        {{2048,0},   {1,1,1,1},{1,0,0}},
        {{2048,2048}, {1,1,1,1},{1,1,0}}
    };

    _add_vertex(ctx,vs[0]);
    _add_vertex(ctx,vs[1]);
    _add_vertex(ctx,vs[2]);
    _add_vertex(ctx,vs[3]);

    _add_tri_indices_for_rect (ctx, 0);
}


void _show_text (VkvgContext ctx, const char* text){
    _vkvg_font_t* f = ctx->curFont;

    hb_buffer_t *buf = hb_buffer_create();

    const char *lng  = "fr";
    hb_script_t script = HB_SCRIPT_LATIN;
    script = hb_script_from_string(text,strlen(text));
    hb_direction_t dir = hb_script_get_horizontal_direction(script);
    //dir = HB_DIRECTION_TTB;
    hb_buffer_set_direction (buf, dir);
    hb_buffer_set_script    (buf, script);
    hb_buffer_set_language  (buf, hb_language_from_string(lng,strlen(lng)));
    hb_buffer_add_utf8      (buf, text, strlen(text), 0, strlen(text));

    hb_shape (f->hb_font, buf, NULL, 0);

    unsigned int         glyph_count;
    hb_glyph_info_t     *glyph_info   = hb_buffer_get_glyph_infos (buf, &glyph_count);
    hb_glyph_position_t *glyph_pos    = hb_buffer_get_glyph_positions (buf, &glyph_count);

    unsigned int string_width_in_pixels = 0;
    for (int i=0; i < glyph_count; ++i)
        string_width_in_pixels += glyph_pos[i].x_advance >> 6;


    Vertex v = { .col = ctx->curRGBA };
    vec2 pen = ctx->curPos;

    //_show_texture(ctx); return;

    for (int i=0; i < glyph_count; ++i) {
        _check_vertex_buff_size(ctx);
        _check_index_buff_size(ctx);

        _char_ref* cr = f->charLookup[glyph_info[i].codepoint];

        if (cr!=NULL){
            float uvWidth = cr->bounds.z / (float)FONT_PAGE_SIZE;
            float uvHeight = cr->bounds.w / (float)FONT_PAGE_SIZE;
            vec2 p0 = {pen.x + cr->bmpDiff.x + (glyph_pos[i].x_offset >> 6),
                       pen.y - cr->bmpDiff.y + (glyph_pos[i].y_offset >> 6)};
            v.pos = p0;

            uint32_t firstIdx = ctx->vertCount;

            v.uv.x = cr->bounds.x;
            v.uv.y = cr->bounds.y;
            v.uv.z = 0;
            _add_vertex(ctx,v);

            v.pos.y += cr->bounds.w;
            v.uv.y += uvHeight;
            _add_vertex(ctx,v);

            v.pos.x += cr->bounds.z;
            v.pos.y = p0.y;
            v.uv.x += uvWidth;
            v.uv.y = cr->bounds.y;
            _add_vertex(ctx,v);

            v.pos.y += cr->bounds.w;
            v.uv.y += uvHeight;
            _add_vertex(ctx,v);

            _add_tri_indices_for_rect (ctx, firstIdx);
        }

        pen.x += (glyph_pos[i].x_advance >> 6);
        pen.y -= (glyph_pos[i].y_advance >> 6);
    }

}
/*void testfonts(){
    FT_Library      library;
    FT_Face         face;
    FT_GlyphSlot    slot;

    assert(!FT_Init_FreeType(&library));
    assert(!FT_New_Face(library, "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 0, &face));
    assert(!FT_Set_Char_Size(face, 0, ptSize, device_hdpi, device_vdpi ));

    //_build_face_tex(face);

    hb_font_t *hb_font = hb_ft_font_create(face, NULL);
    hb_buffer_t *buf = hb_buffer_create();

    const char *text = "Ленивый рыжий кот";
    const char *lng  = "en";
    //"كسول الزنجبيل القط","懶惰的姜貓",


    hb_buffer_set_direction (buf, HB_DIRECTION_LTR);
    hb_buffer_set_script    (buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language  (buf, hb_language_from_string(lng,strlen(lng)));
    hb_buffer_add_utf8      (buf, text, strlen(text), 0, strlen(text));

    hb_unicode_funcs_t * unifc = hb_unicode_funcs_get_default();
    hb_script_t sc = hb_buffer_get_script(buf);

    sc = hb_unicode_script(unifc,0x0260);

    FT_CharMap* cm = face->charmap;

    //hb_script_to_iso15924_tag()


    FT_Done_Face    ( face );
    FT_Done_FreeType( library );
}*/


//void main(){
//    testfonts();
//}

