#ifndef VKVG_H
#define VKVG_H

#include <vulkan/vulkan.h>
#include "vectors.h"

#define VKVG_SAMPLES VK_SAMPLE_COUNT_8_BIT

typedef enum VkvgDirection {
	VKVG_HORIZONTAL	= 0,
	VKVG_VERTICAL	= 1
}VkvgDirection;

typedef struct _vkvg_context_t* VkvgContext;
typedef struct _vkvg_surface_t* VkvgSurface;
typedef struct _vkvg_device_t* VkvgDevice;

VkvgDevice	vkvg_device_create			(VkDevice vkdev, VkQueue queue, uint32_t qFam,
										VkPhysicalDeviceMemoryProperties memprops);
void		vkvg_device_destroy			(VkvgDevice dev);

VkvgSurface vkvg_surface_create			(VkvgDevice dev, int32_t width, uint32_t height);
void		vkvg_surface_destroy		(VkvgSurface surf);
VkImage		vkvg_surface_get_vk_image	(VkvgSurface surf);

/*Context*/
VkvgContext vkvg_create		(VkvgSurface surf);
void vkvg_destroy			(VkvgContext ctx);

void vkvg_flush				(VkvgContext ctx);

void vkvg_close_path		(VkvgContext ctx);
void vkvg_line_to			(VkvgContext ctx, float x, float y);
void vkvg_move_to			(VkvgContext ctx, float x, float y);
void vkvg_arc				(VkvgContext ctx, float xc, float yc, float radius, float a1, float a2);
void vkvg_stroke			(VkvgContext ctx);
void vkvg_fill				(VkvgContext ctx);
void vkvg_set_rgba			(VkvgContext ctx, float r, float g, float b, float a);
void vkvg_set_linewidth		(VkvgContext ctx, float width);

void vkvg_select_font_face	(VkvgContext ctx, const char* name);
void vkvg_set_font_size		(VkvgContext ctx, uint32_t size);
void vkvg_show_text			(VkvgContext ctx, const char* text);
#endif
