#ifndef SURFACE_INTERNAL_H
#define SURFACE_INTERNAL_H

#include "vkvg.h"
#include "vkh_image.h"

typedef struct _vkvg_surface_t {
	VkvgDevice	dev;
	uint32_t	width;
	uint32_t	height;
	VkFramebuffer fb;
	vkh_image	img;
	vkh_image	imgMS;
}vkvg_surface;
#endif