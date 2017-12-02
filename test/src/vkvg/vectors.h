#ifndef VKVG_VECTORS_H
#define VKVG_VECTORS_H

#include <math.h>
#include <stdint.h>
#include <stdbool.h>

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

typedef struct {
	uint16_t x;
	uint16_t y;
	uint16_t z;
	uint16_t w;
}vec4i16;

typedef struct {
	uint16_t x;
	uint16_t y;
}vec2i16;

vec2d		_v2LineNorm	(vec2 a, vec2 b);
double		_v2Lengthd	(vec2d v);
float		_v2Length	(vec2 v);
vec2d		_v2Normd	(vec2d a);
vec2d		_v2Multd	(vec2d a, double m);

vec2d		 _v2LineNormd(vec2d a, vec2d b);
vec2d		_v2dPerpd	(vec2d a);
vec2d		_v2perpNorm	(vec2 a, vec2 b);
vec2		_vec2dToVec2(vec2d vd);
vec2		_v2perpNorm2(vec2 a, vec2 b);
vec2		vec2_add	(vec2 a, vec2 b);
vec2d		_v2addd		(vec2d a, vec2d b);
vec2		_v2sub		(vec2 a, vec2 b);
vec2d		_v2subd		(vec2d a, vec2d b);
bool		_v2equ		(vec2 a, vec2 b);

#endif
