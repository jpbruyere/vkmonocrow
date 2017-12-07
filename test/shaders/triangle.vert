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

layout(push_constant) uniform PushConsts {
	vec2 scale;
	vec2 translate;
} pushConsts;

void main()
{
	outColor = inColor;
	outUV = inUV;
	gl_Position = vec4(inPos.xy*pushConsts.scale+pushConsts.translate,0.0, 1.0);
}
