#version 450

#extension GL_ARB_separate_shader_objects	: enable
#extension GL_ARB_shading_language_420pack	: enable

layout (binding = 0) uniform sampler2DArray fontMap;
layout (binding = 1) uniform sampler2D		source;

layout (location = 0) in vec4 inColor;		//source rgba
layout (location = 1) in vec3 inFontUV;		//if it is a text drawing, inFontUV.z hold fontMap layer
layout (location = 2) in vec2 inScreenSize;	//user in source painting
layout (location = 3) in vec2 inSrcOffset;	//source offset (set_source x,y)

layout (location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConsts {
	vec2 screenSize;
	vec2 scale;
	vec2 translate;
	vec2 srcOffset;
} pushConsts;

layout (constant_id = 0) const int NUM_SAMPLES = 8;

void main()
{
	vec2 srcUV = (inSrcOffset+gl_FragCoord.xy)/inScreenSize;
	//vec2 srcUV = vec2(0.55);
	vec4 c = texture(source, srcUV);


	if (inFontUV.z >= 0.0)
		c *= texture(fontMap, inFontUV).r;

	outFragColor = c;
}
