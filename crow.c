#include "crow.h"
#include "cairo.h"

static MonoDomain *domain;
static MonoAssembly *assembly;
static MonoImage *image;
static MonoClass *klass;
static MonoObject *obj;
static MonoMethod *loadiface = NULL, *resize = NULL, *update = NULL,
                    *mouseMove = NULL, *mouseUp = NULL, *mouseDown = NULL;
static MonoClassField *fldIsDirty = NULL, *fldDirtyRect = NULL, *fldDirtyBmp = NULL;


static pthread_t crowThread;
pthread_mutex_t crow_load_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t crow_update_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t crow_event_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint8_t needResize = FALSE;
static uint8_t loadFlag = FALSE; //true when a file is triggered to be loaded
static cairo_rectangle_int_t crowBounds;
static char* fileName = NULL;

volatile Rectangle dirtyRect = {};
volatile uint8_t* dirtyBmp = NULL;
volatile uint32_t offsetY = 0;
volatile uint32_t length = 0;
volatile size_t dirtyBmpSize;

static CrowEvent * crow_evt_queue_start = NULL;
static CrowEvent * crow_evt_queue_end = NULL;

CrowEvent* crow_create_evt(uint32_t eventType, uint32_t data0, uint32_t data1){
    CrowEvent* pEvt = (CrowEvent*)malloc(sizeof(CrowEvent));
    pEvt->eventType = eventType;
    pEvt->data0 = data0;
    pEvt->data1 = data1;
    pEvt->pNext = NULL;
    return pEvt;
}

void crow_evt_enqueue(CrowEvent *pEvt){
    pthread_mutex_lock(&crow_event_mutex);
    if (crow_evt_queue_end){
        crow_evt_queue_end->pNext = pEvt;
        crow_evt_queue_end = pEvt;
    }else{
        crow_evt_queue_start = pEvt;
        crow_evt_queue_end = pEvt;
    }
    pthread_mutex_unlock(&crow_event_mutex);
}

CrowEvent* crow_evt_dequeue(){
    pthread_mutex_lock(&crow_event_mutex);
    CrowEvent* next = crow_evt_queue_start;
    if (next){
        if (crow_evt_queue_start->pNext)
            crow_evt_queue_start = crow_evt_queue_start->pNext;
        else
            crow_evt_queue_start = crow_evt_queue_end = NULL;
    }
    pthread_mutex_unlock(&crow_event_mutex);
    return next;
}

bool crow_evt_pending () {
    return (bool)(crow_evt_queue_start!=NULL);
}

void printException (MonoObject* exc){
    MonoMethodDesc* mdesc = mono_method_desc_new (":ToString()", FALSE);
    MonoClass* klass = mono_get_exception_class();
    MonoMethod* toString = mono_method_desc_search_in_class(mdesc,klass);

    MonoString* str = (MonoString*)mono_runtime_invoke (toString, exc, NULL, NULL);
    char* p = mono_string_to_utf8 (str);
    printf ("Error: %s\n", p);
    mono_free (p);
}

void crow_resize (int width, int height){
    crowBounds.width = width;
    crowBounds.height = height;
    if(dirtyBmp)
        free(dirtyBmp);
    dirtyBmpSize = (size_t)(crowBounds.width*crowBounds.height*4);
    dirtyBmp = (uint8_t*)malloc(dirtyBmpSize);
    /*printf("resize: (%d,%d), bmpPtr:%lu, length=%lu\n",dirtyRect.width,dirtyRect.height,
           dirtyBmp, dirtyBmpSize);
    fflush(stdout);*/
    length = offsetY = 0;
    needResize = TRUE;
}

void crow_load () {
    while(loadFlag)
        continue;
    pthread_mutex_lock(&crow_load_mutex);
    loadFlag = TRUE;
    pthread_mutex_unlock(&crow_load_mutex);
}

void crow_lock_update_mutex(){
    pthread_mutex_lock(&crow_update_mutex);
}
void crow_release_update_mutex(){
    pthread_mutex_unlock(&crow_update_mutex);
}

void crow_cairo_region_clear(cairo_t *ctx, cairo_region_t *reg){
    for (int i = 0; i < cairo_region_num_rectangles(reg); ++i) {
        cairo_rectangle_int_t r = {};
        cairo_region_get_rectangle (reg, i, &r);
        cairo_rectangle (ctx, r.x, r.y, r.width, r.height);
    }
    cairo_clip_preserve (ctx);
    cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
    cairo_fill (ctx);
    cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);
}

void crow_mono_update (cairo_region_t* reg, MonoArray* bmp){
    pthread_mutex_lock(&crow_update_mutex);

    if (needResize){
        pthread_mutex_unlock(&crow_update_mutex);
        return;
    }

    cairo_region_get_extents(reg, &dirtyRect);
    if (dirtyRect.x < 0)
        dirtyRect.x = 0;
    if (dirtyRect.y < 0)
        dirtyRect.y = 0;
    if (dirtyRect.width + dirtyRect.x > crowBounds.width)
        dirtyRect.width = crowBounds.width - dirtyRect.x;
    if (dirtyRect.height + dirtyRect.y > crowBounds.height)
        dirtyRect.height = crowBounds.height - dirtyRect.y;

    uint8_t* ptrBmp = (uint8_t*)mono_array_addr(bmp,uint8_t,0);
    int stride = crowBounds.width * 4;
    offsetY = dirtyRect.y * stride;
    length = dirtyRect.height * stride;

    /*printf("update: (%d,%d,%d,%d),offset=%d, length=%d, bmpPtr:%lu, dirtyPtr=%lu\n",dirtyRect.x,
           dirtyRect.y,dirtyRect.width,dirtyRect.height,offsetY,length,
           dirtyBmp, ptrBmp);
    fflush(stdout);*/

    memcpy(dirtyBmp + offsetY, ptrBmp + offsetY, length);

    pthread_mutex_unlock(&crow_update_mutex);
}

void embed_cairo_init() {
    mono_add_internal_call ("Crow.Interface::crow_mono_update", crow_mono_update);
    mono_add_internal_call ("Cairo.NativeMethods::crow_cairo_region_clear", crow_cairo_region_clear);

    mono_add_internal_call ("Cairo.NativeMethods::cairo_create", cairo_create);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_destroy", cairo_destroy);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_curve_to", cairo_curve_to);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_arc", cairo_arc);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_arc_negative", cairo_arc_negative);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_clip", cairo_clip);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_clip_preserve", cairo_clip_preserve);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_clip_extents", cairo_clip_extents);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_close_path", cairo_close_path);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_copy_page", cairo_copy_page);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_copy_path", cairo_copy_path);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_append_path", cairo_append_path);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_curve_to", cairo_curve_to);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_fill", cairo_fill);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_fill_extents", cairo_fill_extents);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_fill_preserve", cairo_fill_preserve);

    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_extents", cairo_font_extents);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_face_destroy", cairo_font_face_destroy);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_face_get_type", cairo_font_face_get_type);

    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_create", cairo_font_options_create);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_copy", cairo_font_options_copy);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_destroy", cairo_font_options_destroy);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_equal", cairo_font_options_equal);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_get_antialias", cairo_font_options_get_antialias);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_get_hint_metrics", cairo_font_options_get_hint_metrics);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_get_hint_style", cairo_font_options_get_hint_style);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_get_subpixel_order", cairo_font_options_get_subpixel_order);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_hash", cairo_font_options_hash);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_set_antialias", cairo_font_options_set_antialias);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_set_hint_metrics", cairo_font_options_set_hint_metrics);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_set_hint_style", cairo_font_options_set_hint_style);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_set_subpixel_order", cairo_font_options_set_subpixel_order);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_font_options_status", cairo_font_options_status);

    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_antialias", cairo_get_antialias);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_current_point", cairo_get_current_point);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_fill_rule", cairo_get_fill_rule);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_font_face", cairo_get_font_face);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_font_matrix", cairo_get_font_matrix);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_font_options", cairo_get_font_options);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_group_target", cairo_get_group_target);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_line_cap", cairo_get_line_cap);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_line_join", cairo_get_line_join);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_line_width", cairo_get_line_width);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_matrix", cairo_get_matrix);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_miter_limit", cairo_get_miter_limit);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_operator", cairo_get_operator);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_reference_count", cairo_get_reference_count);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_source", cairo_get_source);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_target", cairo_get_target);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_tolerance", cairo_get_tolerance);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_glyph_extents", cairo_glyph_extents);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_has_current_point", cairo_has_current_point);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_identity_matrix", cairo_identity_matrix);

    mono_add_internal_call ("Cairo.NativeMethods::cairo_image_surface_create", cairo_image_surface_create);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_destroy", cairo_surface_destroy);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_image_surface_create_for_data", cairo_image_surface_create_for_data);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_image_surface_create_for_data", cairo_image_surface_create_for_data);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_image_surface_create_from_png", cairo_image_surface_create_from_png);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_image_surface_get_data", cairo_image_surface_get_data);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_image_surface_get_format", cairo_image_surface_get_format);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_image_surface_get_height", cairo_image_surface_get_height);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_image_surface_get_stride", cairo_image_surface_get_stride);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_image_surface_get_width", cairo_image_surface_get_width);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_in_clip", cairo_in_clip);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_in_fill", cairo_in_fill);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_in_stroke", cairo_in_stroke);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_line_to", cairo_line_to);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_mask", cairo_mask);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_mask_surface", cairo_mask_surface);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_matrix_init", cairo_matrix_init);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_matrix_init_identity", cairo_matrix_init_identity);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_move_to", cairo_move_to);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_new_path", cairo_new_path);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_new_sub_path", cairo_new_sub_path);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_paint", cairo_paint);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_paint_with_alpha", cairo_paint_with_alpha);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_path_destroy", cairo_path_destroy);

    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_add_color_stop_rgb", cairo_pattern_add_color_stop_rgb);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_add_color_stop_rgba", cairo_pattern_add_color_stop_rgba);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_get_color_stop_count", cairo_pattern_get_color_stop_count);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_get_color_stop_rgba", cairo_pattern_get_color_stop_rgba);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_create_for_surface", cairo_pattern_create_for_surface);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_get_surface", cairo_pattern_get_surface);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_create_linear", cairo_pattern_create_linear);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_get_linear_points", cairo_pattern_get_linear_points);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_create_radial", cairo_pattern_create_radial);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_get_radial_circles", cairo_pattern_get_radial_circles);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_create_rgb", cairo_pattern_create_rgb);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_create_rgba", cairo_pattern_create_rgba);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_get_rgba", cairo_pattern_get_rgba);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_destroy", cairo_pattern_destroy);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_get_extend", cairo_pattern_get_extend);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_get_filter", cairo_pattern_get_filter);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_get_matrix", cairo_pattern_get_matrix);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_get_type", cairo_pattern_get_type);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_reference", cairo_pattern_reference);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_set_extend", cairo_pattern_set_extend);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_set_filter", cairo_pattern_set_filter);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_set_matrix", cairo_pattern_set_matrix);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pattern_status", cairo_pattern_status);

    mono_add_internal_call ("Cairo.NativeMethods::cairo_pop_group", cairo_pop_group);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_pop_group_to_source", cairo_pop_group_to_source);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_push_group", cairo_push_group);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_push_group_with_content", cairo_push_group_with_content);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_rectangle", cairo_rectangle);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_reference", cairo_reference);

    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_contains_point", cairo_region_contains_point);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_contains_rectangle", cairo_region_contains_rectangle);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_copy", cairo_region_copy);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_create", cairo_region_create);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_create_rectangle", cairo_region_create_rectangle);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_create_rectangles", cairo_region_create_rectangles);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_destroy", cairo_region_destroy);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_equal", cairo_region_equal);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_get_extents", cairo_region_get_extents);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_get_rectangle", cairo_region_get_rectangle);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_intersect", cairo_region_intersect);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_intersect_rectangle", cairo_region_intersect_rectangle);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_is_empty", cairo_region_is_empty);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_num_rectangles", cairo_region_num_rectangles);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_reference", cairo_region_reference);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_status", cairo_region_status);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_subtract", cairo_region_subtract);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_subtract_rectangle", cairo_region_subtract_rectangle);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_translate", cairo_region_translate);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_union", cairo_region_union);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_union_rectangle", cairo_region_union_rectangle);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_xor", cairo_region_xor);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_region_xor_rectangle", cairo_region_xor_rectangle);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_rel_curve_to", cairo_rel_curve_to);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_rel_line_to", cairo_rel_line_to);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_rel_move_to", cairo_rel_move_to);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_reset_clip", cairo_reset_clip);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_restore", cairo_restore);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_rotate", cairo_rotate);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_save", cairo_save);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_scale", cairo_scale);

    mono_add_internal_call ("Cairo.NativeMethods::cairo_select_font_face", cairo_select_font_face);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_antialias", cairo_set_antialias);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_dash", cairo_set_dash);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_dash", cairo_get_dash);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_get_dash_count", cairo_get_dash_count);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_fill_rule", cairo_set_fill_rule);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_font_face", cairo_set_font_face);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_font_matrix", cairo_set_font_matrix);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_font_options", cairo_set_font_options);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_font_size", cairo_set_font_size);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_line_cap", cairo_set_line_cap);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_line_join", cairo_set_line_join);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_line_width", cairo_set_line_width);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_matrix", cairo_set_matrix);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_miter_limit", cairo_set_miter_limit);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_operator", cairo_set_operator);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_source", cairo_set_source);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_source_rgb", cairo_set_source_rgb);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_source_rgba", cairo_set_source_rgba);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_source_surface", cairo_set_source_surface);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_set_tolerance", cairo_set_tolerance);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_show_glyphs", cairo_show_glyphs);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_show_page", cairo_show_page);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_show_text", cairo_show_text);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_status", cairo_status);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_status_to_string", cairo_status_to_string);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_stroke", cairo_stroke);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_stroke_extents", cairo_stroke_extents);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_stroke_preserve", cairo_stroke_preserve);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_rectangle_list_destroy", cairo_rectangle_list_destroy);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_copy_clip_rectangle_list", cairo_copy_clip_rectangle_list);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_create_similar", cairo_surface_create_similar);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_finish", cairo_surface_finish);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_flush", cairo_surface_flush);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_get_content", cairo_surface_get_content);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_get_device_offset", cairo_surface_get_device_offset);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_get_font_options", cairo_surface_get_font_options);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_get_reference_count", cairo_surface_get_reference_count);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_get_type", cairo_surface_get_type);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_mark_dirty", cairo_surface_mark_dirty);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_mark_dirty_rectangle", cairo_surface_mark_dirty_rectangle);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_reference", cairo_surface_reference);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_set_device_offset", cairo_surface_set_device_offset);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_set_fallback_resolution", cairo_surface_set_fallback_resolution);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_surface_status", cairo_surface_status);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_text_extents", cairo_text_extents);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_text_path", cairo_text_path);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_transform", cairo_transform);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_translate", cairo_translate);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_user_to_device", cairo_user_to_device);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_user_to_device_distance", cairo_user_to_device_distance);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_version", cairo_version);
    mono_add_internal_call ("Cairo.NativeMethods::cairo_version_string", cairo_version_string);
}

void main_loop(){
    while(1){
        MonoObject *exception = NULL;

        if (crow_evt_pending()){
            CrowEvent* evt = crow_evt_dequeue();
            if (evt->eventType == CROW_MOUSE_MOVE){
                void* args[2];
                args[0] = &evt->data0;
                args[1] = &evt->data1;
                mono_runtime_invoke (mouseMove, obj, args, &exception);
            }else if (evt->eventType == CROW_MOUSE_DOWN){
                void* args[1];
                args[0] = &evt->data0;
                mono_runtime_invoke (mouseDown, obj, args, &exception);
            }else if (evt->eventType == CROW_MOUSE_UP){
                void* args[1];
                args[0] = &evt->data0;
                mono_runtime_invoke (mouseUp, obj, args, &exception);
            }
            free(evt);
        }else if (needResize){
            pthread_mutex_lock(&crow_update_mutex);
            void* args[1];
            args[0] = &crowBounds;
            mono_runtime_invoke (resize, obj, args, &exception);
            needResize = FALSE;
            pthread_mutex_unlock(&crow_update_mutex);
        }else if (loadFlag){
            pthread_mutex_lock(&crow_load_mutex);
            //MonoString *str = mono_string_new (domain, "/mnt/devel/gts/libvk/crow/Tests/Interfaces/GraphicObject/0.crow");
            MonoString *str = mono_string_new (domain, "/mnt/devel/gts/libvk/crow/Tests/Interfaces/Divers/0.crow");
            mono_runtime_invoke (loadiface, obj, &str, &exception);
            loadFlag = FALSE;
            pthread_mutex_unlock(&crow_load_mutex);
        }else{
            mono_runtime_invoke (update, obj, NULL, &exception);
        }
        if (exception)
            printException(exception);
    }
}

void* crow_thread (void *arg) {
    printf ("Crow Interface Thread\n");

    int retval;
    const char *crowDllPath = "/mnt/devel/gts/libvk/crow/build/Debug/Crow.dll";

    domain = mono_jit_init (crowDllPath);
    mono_config_parse ("/etc/mono/config");
    assembly = mono_domain_assembly_open (domain, crowDllPath);

    if (!assembly)
        return EXIT_FAILURE;

    image = mono_assembly_get_image (assembly);

    embed_cairo_init();

    klass = mono_class_from_name (image, "Crow", "Interface");
    if (!klass) {
        fprintf (stderr, "Can't find Interface in assembly %s\n", mono_image_get_filename (image));
        exit (1);
    }

    MonoMethod *m = NULL,
            *ctor = NULL;

    void* iter = NULL;
    while ((m = mono_class_get_methods (klass, &iter))) {
        if (strcmp (mono_method_get_name (m), ".ctor") == 0)
            ctor = m;
        else if (strcmp (mono_method_get_name (m), "LoadInterface") == 0)
            loadiface = m;
        else if (strcmp (mono_method_get_name (m), "Update") == 0)
            update = m;
        else if (strcmp (mono_method_get_name (m), "ProcessResize") == 0)
            resize = m;
        else if (strcmp (mono_method_get_name (m), "ProcessMouseMove") == 0)
            mouseMove = m;
        else if (strcmp (mono_method_get_name (m), "ProcessMouseButtonDown") == 0)
            mouseDown = m;
        else if (strcmp (mono_method_get_name (m), "ProcessMouseButtonUp") == 0)
            mouseUp = m;
    }

    fldIsDirty = mono_class_get_field_from_name (klass, "IsDirty");
    fldDirtyRect = mono_class_get_field_from_name (klass, "DirtyRect");
    fldDirtyBmp = mono_class_get_field_from_name (klass, "dirtyBmp");

    obj = mono_object_new (domain, klass);
    //mono_runtime_object_init (obj);
    MonoObject *exc = NULL;
    mono_runtime_invoke (ctor, obj, NULL, &exc);
    if (exc)
        printException(exc);

    main_loop();

    retval = mono_environment_exitcode_get ();

    mono_jit_cleanup (domain);

    pthread_exit(retval);
}

void crow_init () {
    if(pthread_create(&crowThread, NULL, crow_thread, NULL) == -1) {
        perror("pthread_create");
        exit (EXIT_FAILURE);
    }

    //return retval;
}

