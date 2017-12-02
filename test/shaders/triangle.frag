#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 0) uniform sampler2DArray fontMap;

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUV;

layout (location = 0) out vec4 outFragColor;

void main()
{
	vec4 c = vec4(inColor.rgb, 1.0);
	if (inUV.z >= 0.0)
		c *= texture(fontMap, inUV).r;
	//c.r+=0.5;
	outFragColor = c;
}
