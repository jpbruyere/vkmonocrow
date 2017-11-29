#ifndef SURFACE_H
#define SURFACE_H

#include "vkh_image.h"
#include "vkvg_device.h"

#define VKVG_SAMPLES VK_SAMPLE_COUNT_4_BIT

typedef struct vkvg_surface_t {
	vkvg_device* dev;
	uint32_t width;
	uint32_t height;
	VkFramebuffer fb;
	vkh_image img;
	vkh_image imgMS;
}vkvg_surface;

void vkvg_surface_create(vkvg_device* dev, int32_t width, uint32_t height, vkvg_surface * surf);
void vkvg_surface_destroy(vkvg_surface* surf);
#endif
