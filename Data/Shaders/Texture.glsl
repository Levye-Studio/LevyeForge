#type vertex
#version 410 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in float a_TilingFactor;
layout(location = 5) in float a_Shape;
layout(location = 6) in int a_EntityID;

layout(std140) uniform Camera
{
	mat4 u_ViewProjection;
};

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
	float TilingFactor;
	float Shape;
};

layout (location = 0) out VertexOutput v_Output;
layout (location = 4) flat out float v_TexIndex;
layout (location = 5) flat out int v_EntityID;

void main()
{
	v_Output.Color = a_Color;
	v_Output.TexCoord = a_TexCoord;
	v_Output.TilingFactor = a_TilingFactor;
	v_Output.Shape = a_Shape;
	v_TexIndex = a_TexIndex;
	v_EntityID = a_EntityID;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 410 core

layout(location = 0) out vec4 color;
layout(location = 1) out int color2;
// layout(location = 1) out vec4 color2;

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
	float TilingFactor;
	float Shape;
};

layout (location = 0) in VertexOutput v_Output;
layout (location = 4) flat in float v_TexIndex;
layout (location = 5) flat in int v_EntityID;

uniform sampler2D u_Textures[16];

void main()
{
	vec4 texColor = v_Output.Color;

	if(v_Output.Shape > 0.5){
	  
	float dist = length(v_Output.TexCoord - vec2(0.5));
	if(dist > 0.5)
	  discard;
	}

	switch(int(v_TexIndex))
	{
		case  0: texColor *= texture(u_Textures[ 0], v_Output.TexCoord * v_Output.TilingFactor); break;
		case  1: texColor *= texture(u_Textures[ 1], v_Output.TexCoord * v_Output.TilingFactor); break;
		case  2: texColor *= texture(u_Textures[ 2], v_Output.TexCoord * v_Output.TilingFactor); break;
		case  3: texColor *= texture(u_Textures[ 3], v_Output.TexCoord * v_Output.TilingFactor); break;
		case  4: texColor *= texture(u_Textures[ 4], v_Output.TexCoord * v_Output.TilingFactor); break;
		case  5: texColor *= texture(u_Textures[ 5], v_Output.TexCoord * v_Output.TilingFactor); break;
		case  6: texColor *= texture(u_Textures[ 6], v_Output.TexCoord * v_Output.TilingFactor); break;
		case  7: texColor *= texture(u_Textures[ 7], v_Output.TexCoord * v_Output.TilingFactor); break;
		case  8: texColor *= texture(u_Textures[ 8], v_Output.TexCoord * v_Output.TilingFactor); break;
		case  9: texColor *= texture(u_Textures[ 9], v_Output.TexCoord * v_Output.TilingFactor); break;
		case 10: texColor *= texture(u_Textures[10], v_Output.TexCoord * v_Output.TilingFactor); break;
		case 11: texColor *= texture(u_Textures[11], v_Output.TexCoord * v_Output.TilingFactor); break;
		case 12: texColor *= texture(u_Textures[12], v_Output.TexCoord * v_Output.TilingFactor); break;
		case 13: texColor *= texture(u_Textures[13], v_Output.TexCoord * v_Output.TilingFactor); break;
		case 14: texColor *= texture(u_Textures[14], v_Output.TexCoord * v_Output.TilingFactor); break;
		case 15: texColor *= texture(u_Textures[15], v_Output.TexCoord * v_Output.TilingFactor); break;
	}

	if(texColor.a < 0.1)
        discard;

	vec4 debugColor = vec4(v_TexIndex / 32.0, 0.0, 1.0, 1.0);
	color = texColor;
	// color = debugColor;

	color2 = v_EntityID;
	// color2 = 50;
}