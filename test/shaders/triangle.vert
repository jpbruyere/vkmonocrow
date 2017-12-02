#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec3 inUV;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec3 outUV;

out gl_PerVertex
{
	vec4 gl_Position;
};

const vec2 scale=vec2(2.0/1024.0,2.0/800.0);
const vec2 translate=vec2(-1.0);

void main()
{
	outColor = inColor;
	outUV = inUV;
	gl_Position = vec4(inPos.xy*scale+translate,0.0, 1.0);
}
