#ifndef SURFACE_INTERNAL_H
#define SURFACE_INTERNAL_H

#include "vkvg_internal.h"
#include "vkvg.h"
#include "vkh_image.h"

typedef struct _vkvg_surface_t {
    VkvgDevice	dev;
    uint32_t	width;
    uint32_t	height;
    VkFramebuffer fb;
    VkhImage	img;
    VkhImage	imgMS;
    VkhImage	stencilMS;
    VkSemaphore semaphore;
    VkCommandBuffer cmd;
}vkvg_surface;

void _clear_stencil (VkvgSurface surf);
#endif
