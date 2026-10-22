//////////////////////////////////////////////////////////////////////////////
// Tiberium Project: Zero Hour's terrain drawn the C&C4 way.
//
// Built from C&C4's Terrain.fxh (the GPL modding-support sources) for the vertex data Zero Hour's
// height map renderer already has: position, the baked vertex colour, the base tile's atlas
// coordinates and the blend tile's (alpha-edged, in the same atlas), plus a second stream with
// the height-field normal the engine adds. The deferred pre-pass writes the normal and depth,
// the Default pass reads C&C4's light buffers; Default_M lights forward with the three
// directional lights and the cloud layer. Same lighting and gamma handling as C&C4's terrain.
//
// Compiled with shaders/sage2/build_effects.py (the C preprocessor, then fxc) to
// tiberium_terrain.fxo, packed into the mod as Art\Shaders\tiberium_terrain.fxo.
//////////////////////////////////////////////////////////////////////////////

#define SUPPORT_GLOBAL_LIGHTS 1
#define SUPPORT_CLOUDS 1
#define SUPPORT_FOG 1

#include "Common.fxh"
#include "Gamma.fxh"
#include "DeferredLighting.fxh"

static const float3 SpecularColor = float3(0.5, 0.5, 0.5);
static const float SpecularExponent = 40;

// Time (the cloud layer drifts)
float Time : Time;

// ----------------------------------------------------------------------------
// The terrain atlas: every tile the map uses, base tiles opaque, blend tiles alpha-edged.
// ----------------------------------------------------------------------------
SAMPLER_2D_BEGIN( BaseSampler,
	string UIWidget = "None";
	string SasBindAddress = "Terrain.Atlas";
	)
	MinFilter = MinFilterBest;
	MagFilter = MagFilterBest;
	MipFilter = MipFilterBest;
	AddressU = Clamp;
	AddressV = Clamp;
SAMPLER_2D_END

SAMPLER_2D_BEGIN( CloudTexture,
	string UIWidget = "None";
	string SasBindAddress = "Terrain.Cloud.Texture";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
SAMPLER_2D_END

SAMPLER_2D_BEGIN( MacroSampler,
	string UIWidget = "None";
	string SasBindAddress = "Terrain.MacroTexture";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
SAMPLER_2D_END

// Zero Hour's tiles were painted for its gamma-space fixed-function lighting; lit in linear light
// they read dark next to C&C4's art, so their albedo is lifted (1 = as painted)
float AlbedoScale
<
	string UIWidget = "None";
	string SasBindAddress = "Terrain.AlbedoScale";
> = 1.6;

// world units per macro texture repeat
float MacroScale
<
	string UIWidget = "None";
	string SasBindAddress = "Terrain.MacroScale";
> = 1.0 / 660.0;

// ----------------------------------------------------------------------------
// Vertex input: Zero Hour's terrain vertex (stream 0) and the engine's normal (stream 1)
// ----------------------------------------------------------------------------
struct VSInput
{
	float3 Position : POSITION;
	float4 Color : COLOR0;
	float2 BaseTexCoord : TEXCOORD0;
	float2 BlendTexCoord : TEXCOORD1;
	float3 Normal : NORMAL;
};

// the atlas: the base tile, then the blend tile over it by the product of its alpha edge and the
// vertex alpha (how Zero Hour's second fixed-function pass blends it)
float4 SampleTerrain(float2 baseTexCoord, float2 blendTexCoord, float vertexAlpha)
{
	float4 base = tex2D(SAMPLER(BaseSampler), baseTexCoord);
	float4 blend = tex2D(SAMPLER(BaseSampler), blendTexCoord);
	return float4(lerp(base.xyz, blend.xyz, blend.w * vertexAlpha), 1.0);
}

// ============================================================================
// Deferred pre-pass: normal, specular power and depth
// ============================================================================
struct VSOutput_CreateDepthNormalMap
{
	float4 Position : POSITION;
	float3 Normal : TEXCOORD0;
	float Depth : TEXCOORD1;
};

struct PSOutput_CreateDepthNormalMap
{
	float4 NormalSpec : COLOR0;
	float4 Depth : COLOR1;
};

VSOutput_CreateDepthNormalMap VS_CreateDepthNormalMap(VSInput In)
{
	VSOutput_CreateDepthNormalMap Out;
	Out.Position = mul(float4(In.Position, 1), ViewProjection);
	Out.Normal = In.Normal;
	Out.Depth = Out.Position.z / Out.Position.w;
	return Out;
}

PSOutput_CreateDepthNormalMap PS_CreateDepthNormalMap(VSOutput_CreateDepthNormalMap In)
{
	PSOutput_CreateDepthNormalMap Out;
	float3 normal = normalize(In.Normal);
	Out.NormalSpec = float4((normal + 1.0f) * 0.5f, saturate(SpecularExponent / 255.0f));
	Out.Depth = In.Depth;
	return Out;
}

technique _CreateDepthNormalMap
{
	pass P0
	{
		VertexShader = compile VS_3_0 VS_CreateDepthNormalMap();
		PixelShader = compile PS_3_0 PS_CreateDepthNormalMap();
		ZEnable = true;
		ZWriteEnable = true;
		ZFunc = ZFUNC_INFRONT;
		CullMode = CW;
		AlphaBlendEnable = false;
		AlphaTestEnable = false;
	}
}

// ============================================================================
// Default: the C&C4 High path, lit from the deferred light buffers
// ============================================================================
struct VSOutputDefault
{
	float4 Position : POSITION;
	float4 BaseTexCoord_BlendTexCoord : TEXCOORD0;
	float4 MacroTexCoord_Fog_BlendAlpha : TEXCOORD1;
};

VSOutputDefault VS_Default(VSInput In)
{
	VSOutputDefault Out;
	Out.Position = mul(float4(In.Position, 1), ViewProjection);
	Out.BaseTexCoord_BlendTexCoord = float4(In.BaseTexCoord, In.BlendTexCoord);
	Out.MacroTexCoord_Fog_BlendAlpha.xy = In.Position.xy * MacroScale;
	Out.MacroTexCoord_Fog_BlendAlpha.z = CalculateFog(Fog, In.Position, EyePosition);
	Out.MacroTexCoord_Fog_BlendAlpha.w = In.Color.w;
	return Out;
}

float4 PS_Default(VSOutputDefault In, float2 vPos : PIXELLOC) : COLOR
{
	float4 terrain = SampleTerrain(In.BaseTexCoord_BlendTexCoord.xy, In.BaseTexCoord_BlendTexCoord.zw, In.MacroTexCoord_Fog_BlendAlpha.w);
	float3 baseColor = GammaToLinear(terrain.xyz) * AlbedoScale;
	float2 screenTexCoord = GetScreenTexcoordCoord(vPos);
	float3 color = (SampleDiffuseLight(screenTexCoord) + AmbientLightColor) * baseColor
		+ SampleSpecularLight(screenTexCoord) * SpecularColor * 0.15;
	float3 macro = GammaToLinear(tex2D(SAMPLER(MacroSampler), In.MacroTexCoord_Fog_BlendAlpha.xy).xyz);
	color *= macro;
	color = lerp(Fog.Color.xyz, color, In.MacroTexCoord_Fog_BlendAlpha.z);
	return float4(color, 1.0);
}

technique Default
{
	pass P0
	{
		VertexShader = compile VS_3_0 VS_Default();
		PixelShader = compile PS_3_0 PS_Default();
		ZEnable = true;
		ZWriteEnable = true;
		ZFunc = ZFUNC_INFRONT;
		CullMode = CW;
		AlphaBlendEnable = false;
		AlphaTestEnable = false;
	}
}

// ============================================================================
// Default_M: forward lighting, when the deferred buffers are not available
// ============================================================================
struct VSOutput_M
{
	float4 Position : POSITION;
	float4 BaseTexCoord_BlendTexCoord : TEXCOORD0;
	float4 CloudTexCoord_MacroTexCoord : TEXCOORD1;
	float4 Normal_Fog : TEXCOORD2;
	float4 WorldPosition_BlendAlpha : TEXCOORD3;
};

VSOutput_M VS_Default_M(VSInput In)
{
	VSOutput_M Out;
	Out.Position = mul(float4(In.Position, 1), ViewProjection);
	Out.BaseTexCoord_BlendTexCoord = float4(In.BaseTexCoord, In.BlendTexCoord);
	Out.CloudTexCoord_MacroTexCoord.xy = CalculateCloudTexCoord(Cloud_WorldPositionMultiplier_XYZZ, Cloud_CurrentOffsetUV, In.Position, Time);
	Out.CloudTexCoord_MacroTexCoord.zw = In.Position.xy * MacroScale;
	Out.Normal_Fog.xyz = In.Normal;
	Out.Normal_Fog.w = CalculateFog(Fog, In.Position, EyePosition);
	Out.WorldPosition_BlendAlpha = float4(In.Position, In.Color.w);
	return Out;
}

float4 PS_Default_M(VSOutput_M In) : COLOR
{
	float4 terrain = SampleTerrain(In.BaseTexCoord_BlendTexCoord.xy, In.BaseTexCoord_BlendTexCoord.zw, In.WorldPosition_BlendAlpha.w);
	float3 baseColor = GammaToLinear(terrain.xyz) * AlbedoScale;
	float3 normal = normalize(In.Normal_Fog.xyz);
	float3 cloud = GammaToLinear(tex2D(SAMPLER(CloudTexture), In.CloudTexCoord_MacroTexCoord.xy).xyz);
	float3 light = AmbientLightColor
		+ DirectionalLight_0_Color.xyz * cloud * max(0, dot(normal, DirectionalLight_0_Direction.xyz))
		+ DirectionalLight_1_Color.xyz * max(0, dot(normal, DirectionalLight_1_Direction.xyz))
		+ DirectionalLight_2_Color.xyz * max(0, dot(normal, DirectionalLight_2_Direction.xyz));
	float3 color = light * baseColor;
	float3 macro = GammaToLinear(tex2D(SAMPLER(MacroSampler), In.CloudTexCoord_MacroTexCoord.zw).xyz);
	color *= macro;
	color = lerp(Fog.Color.xyz, color, In.Normal_Fog.w);
	return float4(color, 1.0);
}

technique Default_M
{
	pass P0
	{
		VertexShader = compile VS_3_0 VS_Default_M();
		PixelShader = compile PS_3_0 PS_Default_M();
		ZEnable = true;
		ZWriteEnable = true;
		ZFunc = ZFUNC_INFRONT;
		CullMode = CW;
		AlphaBlendEnable = false;
		AlphaTestEnable = false;
	}
}
