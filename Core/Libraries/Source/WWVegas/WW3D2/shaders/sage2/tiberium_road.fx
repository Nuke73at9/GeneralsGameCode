//////////////////////////////////////////////////////////////////////////////
// Tiberium Project: Zero Hour's roads drawn the C&C4 way.
//
// C&C4's TerrainRoad.fx draws the road strips with the terrain's own shader family: lit from the
// deferred light buffers, alpha-blended edges, depth-tested against the terrain without writing,
// the scorch marks of the terrain top view folded into the colour. This is that pass for Zero
// Hour's road strips (W3DRoadBuffer: position, an unused vertex colour, a texture coordinate
// into the road type's texture). The terrain's own pre-pass has already written the normal and
// depth under the road and the light buffers hold the light for that surface, so the road reads
// them like the terrain's Default pass does; the forward technique lights a flat road with the
// three lights. The strips sit 0.08 units above the ground (Zero Hour's lift) and go through the
// same transform as the terrain effect, so no depth bias is needed. Shroud and fog as the terrain.
//
// Compiled with shaders/sage2/build_effects.py to tiberium_road.fxo, packed into the mod as
// Art\Shaders\tiberium_road.fxo.
//////////////////////////////////////////////////////////////////////////////

#define SUPPORT_GLOBAL_LIGHTS 1
#define SUPPORT_FOG 1

#include "Common.fxh"
#include "Gamma.fxh"
#include "DeferredLighting.fxh"

SAMPLER_2D_BEGIN( BaseSampler,
	string UIWidget = "None";
	string SasBindAddress = "Terrain.Road.Texture";
	)
	MinFilter = MinFilterBest;
	MagFilter = MagFilterBest;
	MipFilter = MipFilterBest;
	AddressU = Wrap;
	AddressV = Wrap;
SAMPLER_2D_END

// the terrain top view: scorch marks, premultiplied, linear light (Common.fxh's mapping)
SAMPLER_2D_BEGIN( TopViewTexture,
	string UIWidget = "None";
	string SasBindAddress = "Terrain.TopViewTexture";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = None;
	AddressU = Clamp;
	AddressV = Clamp;
SAMPLER_2D_END

SAMPLER_2D_BEGIN( ShroudTexture,
	string UIWidget = "None";
	string SasBindAddress = "Terrain.Shroud.Texture";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = None;
	AddressU = Clamp;
	AddressV = Clamp;
SAMPLER_2D_END

float4 ShroudParams
<
	string UIWidget = "None";
	string SasBindAddress = "Terrain.Shroud.Params";
> = float4(0, 0, 0, 0);

// Zero Hour's road textures were painted for its gamma-space lighting, as its tiles: lifted alike
float AlbedoScale
<
	string UIWidget = "None";
	string SasBindAddress = "Terrain.AlbedoScale";
> = 1.6;

struct VSInput
{
	float3 Position : POSITION;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
	float4 Position : POSITION;
	float4 TexCoord_WorldXY : TEXCOORD0;
	float4 ShroudTexCoord_Fog : TEXCOORD1;
};

VSOutput VS_Road(VSInput In)
{
	VSOutput Out;
	Out.Position = mul(float4(In.Position, 1), ViewProjection);
	Out.TexCoord_WorldXY = float4(In.TexCoord, In.Position.xy);
	Out.ShroudTexCoord_Fog.xy = (In.Position.xy + ShroudParams.xy) * ShroudParams.zw;
	Out.ShroudTexCoord_Fog.z = CalculateFog(Fog, In.Position, EyePosition);
	Out.ShroudTexCoord_Fog.w = 0;
	return Out;
}

// the road's colour (linear), opacity and the marks over it
void RoadSurface(VSOutput In, out float3 color, out float opacity)
{
	float4 road = tex2D(SAMPLER(BaseSampler), In.TexCoord_WorldXY.xy);
	color = GammaToLinear(road.xyz) * AlbedoScale;
	opacity = road.w;
	float4 mark = tex2D(SAMPLER(TopViewTexture), CalcTerrainTopViewTexCoord(In.TexCoord_WorldXY.zw));
	color = color * (1.0 - mark.w) + mark.xyz;
}

float4 Finish(VSOutput In, float3 color, float opacity)
{
	color = lerp(Fog.Color.xyz, color, In.ShroudTexCoord_Fog.z);
	color *= GammaToLinear(tex2D(SAMPLER(ShroudTexture), In.ShroudTexCoord_Fog.xy).xyz);
	return float4(color, opacity);
}

// ----------------------------------------------------------------------------
// Default: lit from the light buffers the terrain's pre-pass produced under the road
// ----------------------------------------------------------------------------
float4 PS_Road(VSOutput In, float2 vPos : PIXELLOC) : COLOR
{
	float3 color;
	float opacity;
	RoadSurface(In, color, opacity);
	float2 screenTexCoord = GetScreenTexcoordCoord(vPos);
	color = (SampleDiffuseLight(screenTexCoord) + AmbientLightColor) * color;
	return Finish(In, color, opacity);
}

technique Default
{
	pass P0
	{
		VertexShader = compile VS_3_0 VS_Road();
		PixelShader = compile PS_3_0 PS_Road();
		ZEnable = true;
		ZWriteEnable = false;
		ZFunc = ZFUNC_INFRONT;
		CullMode = None;
		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		AlphaTestEnable = false;
	}
}

// ----------------------------------------------------------------------------
// Default_M: forward, a flat road under the three lights
// ----------------------------------------------------------------------------
float4 PS_Road_M(VSOutput In) : COLOR
{
	float3 color;
	float opacity;
	RoadSurface(In, color, opacity);
	float3 normal = float3(0, 0, 1);
	float3 light = AmbientLightColor
		+ DirectionalLight_0_Color.xyz * max(0, dot(normal, DirectionalLight_0_Direction.xyz))
		+ DirectionalLight_1_Color.xyz * max(0, dot(normal, DirectionalLight_1_Direction.xyz))
		+ DirectionalLight_2_Color.xyz * max(0, dot(normal, DirectionalLight_2_Direction.xyz));
	return Finish(In, light * color, opacity);
}

technique Default_M
{
	pass P0
	{
		VertexShader = compile VS_3_0 VS_Road();
		PixelShader = compile PS_3_0 PS_Road_M();
		ZEnable = true;
		ZWriteEnable = false;
		ZFunc = ZFUNC_INFRONT;
		CullMode = None;
		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		AlphaTestEnable = false;
	}
}
