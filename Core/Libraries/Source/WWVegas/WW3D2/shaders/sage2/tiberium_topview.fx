//////////////////////////////////////////////////////////////////////////////
// Tiberium Project: the terrain's top view - scorch marks painted into a map-wide texture.
//
// C&C4's TerrainScorch.fx draws every scorch mark from a top-down camera into
// TerrainTopViewTexture, which the ground shaders then read at the pixel's world position and
// bake into the albedo before lighting (Terrain.fxh). This is that pass for Zero Hour's scorch
// quads (position, vertex colour, texture coordinate into the scorch atlas): the position is
// mapped straight to the texture, the colour taken from the atlas in linear light with its
// alpha, written premultiplied (ONE / INVSRCALPHA, alpha accumulating as coverage), which is
// what C&C4's SrcAlpha blend onto the cleared texture amounts to.
//
// Compiled with shaders/sage2/build_effects.py to tiberium_topview.fxo, packed into the mod as
// Art\Shaders\tiberium_topview.fxo.
//////////////////////////////////////////////////////////////////////////////

#include "Common.fxh"
#include "Gamma.fxh"

// world x,y -> clip space of the top view
float4x4 TopViewTransform
<
	string UIWidget = "None";
>;

SAMPLER_2D_BEGIN( MarkTexture,
	string UIWidget = "None";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
SAMPLER_2D_END

struct VSOutput
{
	float4 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
};

VSOutput VS_TopView(float3 Position : POSITION, float4 Color : COLOR0, float2 TexCoord : TEXCOORD0)
{
	VSOutput Out;
	Out.Position = mul(float4(Position.xy, 0, 1), TopViewTransform);
	Out.TexCoord = TexCoord;
	return Out;
}

float4 PS_TopView(VSOutput In) : COLOR
{
	float4 mark = tex2D(SAMPLER(MarkTexture), In.TexCoord);
	return float4(GammaToLinear(mark.xyz) * mark.w, mark.w);
}

technique Default
{
	pass P0
	{
		VertexShader = compile VS_3_0 VS_TopView();
		PixelShader = compile PS_3_0 PS_TopView();
		ZEnable = false;
		ZWriteEnable = false;
		CullMode = None;
		AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = InvSrcAlpha;
		SeparateAlphaBlendEnable = true;
		SrcBlendAlpha = One;
		DestBlendAlpha = InvSrcAlpha;
		AlphaTestEnable = false;
	}
}
