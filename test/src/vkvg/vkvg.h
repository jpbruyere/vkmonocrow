#ifndef VKVG_H
#define VKVG_H

#include <vulkan/vulkan.h>

#include "vkvg_device.h"
#include "vkvg_surface.h"

typedef struct {
	float x;
	float y;
}vec2;
typedef struct {
	double x;
	double y;
}vec2d;

typedef struct {
	float x;
	float y;
	float z;
}vec3;

typedef struct {
	float x;
	float y;
	float z;
	float w;
}vec4;

#include "vkvg_buff.h"
#include "vkvg_context.h"

#endif
