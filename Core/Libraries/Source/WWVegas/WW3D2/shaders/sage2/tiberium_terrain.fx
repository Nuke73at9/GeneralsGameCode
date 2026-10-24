//////////////////////////////////////////////////////////////////////////////
// Tiberium Project: Zero Hour's terrain drawn the C&C4 way.
//
// Built from C&C4's Terrain.fxh (the GPL modding-support sources) for the vertex data Zero Hour's
// height map renderer already has: position, the baked vertex colour, the base tile's atlas
// coordinates and the blend tile's (alpha-edged, in the same atlas), plus a second stream with
// the height-field normal the engine adds. The deferred pre-pass writes the normal and depth,
// the Default pass reads C&C4's light buffers; Default_M lights forward with the three
// directional lights and the cloud layer. Same lighting and gamma handling as C&C4's terrain.
// Zero Hour's shroud is multiplied in at the end of both.
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

// C&C4's own terrain textures standing in for the map's terrains, one 256-pixel slot each with a
// border (Sage2TerrainAtlasClass); sampled world-aligned by slot, every AtlasParams.w world units
SAMPLER_2D_BEGIN( AtlasColor,
	string UIWidget = "None";
	string SasBindAddress = "Terrain.CNC4.Color";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
SAMPLER_2D_END

SAMPLER_2D_BEGIN( AtlasNormal,
	string UIWidget = "None";
	string SasBindAddress = "Terrain.CNC4.Normal";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
SAMPLER_2D_END

// x: slots per atlas row, y: a slot's inner size, z: its border (both in atlas texture coordinates),
// w: world units one texture spans (80 = eight cells, as in C&C4)
float4 AtlasParams
<
	string UIWidget = "None";
	string SasBindAddress = "Terrain.CNC4.AtlasParams";
> = float4(7, 0.125, 0.00390625, 80);

// x: the sign of the normal maps' green channel (1 as stored, -1 to flip the convention),
// y: bump scale (0.75: C&C4's terrain, High; 0 flattens the bumps)
float2 BumpParams
<
	string UIWidget = "None";
	string SasBindAddress = "Terrain.CNC4.BumpParams";
> = float2(1, 0.75);

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

// Zero Hour's shroud (one texel a cell, black beyond the map's border, grey under fog of war):
// Zero Hour multiplies it over the finished terrain in an extra pass that needs an equal depth,
// so it is folded into this pass instead. White when the map has none.
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

// xy: added to the world position, zw: then scaled by, to reach the shroud texel
float4 ShroudParams
<
	string UIWidget = "None";
	string SasBindAddress = "Terrain.Shroud.Params";
> = float4(0, 0, 0, 0);

float2 ShroudUV(float2 worldXY)
{
	return (worldXY + ShroudParams.xy) * ShroudParams.zw;
}

// C&C4's terrain top view: the scorch marks painted into a map-wide texture before the frame
// (premultiplied, linear light), folded into the albedo here before lighting as Terrain.fxh does;
// transparent when the map has none
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

// the ground's colour and specular with the marks over it (Terrain.fxh: no specular under a burn);
// ScorchMarkUVOffsetMultiplier and CalcTerrainTopViewTexCoord are Common.fxh's
void ApplyTopView(float2 worldXY, inout float3 color, inout float spec)
{
	float4 mark = tex2D(SAMPLER(TopViewTexture), CalcTerrainTopViewTexCoord(worldXY));
	color = color * (1.0 - mark.w) + mark.xyz;
	spec *= 1.0 - mark.w;
}

// Zero Hour's tiles were painted for its gamma-space fixed-function lighting; lit in linear light
// they read dark next to C&C4's art, so their albedo is lifted (1 = as painted)
float AlbedoScale
<
	string UIWidget = "None";
	string SasBindAddress = "Terrain.AlbedoScale";
> = 1.6;

// Tiberium Project @feature How much of the fill lights' highlight the ground keeps. 1 is C&C4's
// deferred light pass, where all three directional lights shine in the specular; 0 is Red Alert 3's
// terrain, where only the sun does and the fills light it diffusely (RA3's fills are strong and low
// across the ground: under Zero Hour's camera they glared off it). The light buffer holds the sum,
// so the fills' part is worked out again here, with the light pass's own terms, and taken back out.
float FillSpecular
<
	string UIWidget = "None";
	string SasBindAddress = "Terrain.FillSpecular";
> = 1.0;

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
	float2 Slots : TEXCOORD2;		// C&C4 atlas slots of the base and blend terrains, -1 for none
};

// one projection of a world-aligned C&C4 texture: the slot's tile at plane coordinates p
void SampleC4Plane(float slot, float2 p, out float4 c, out float4 n)
{
	float2 w = p / AtlasParams.w;
	float2 f = frac(w);
	f.y = 1.0 - f.y;		// the texture's top row lies at the larger world y, as C&C4 maps it
	float pitch = AtlasParams.y + 2.0 * AtlasParams.z;
	float row = floor((slot + 0.5) / AtlasParams.x);
	float col = slot - row * AtlasParams.x;
	float2 uv = float2(col, row) * pitch + AtlasParams.z + f * AtlasParams.y;
	// gradients from the unwrapped coordinate, so the mip level does not jump at the wrap
	float2 dx = ddx(w) * AtlasParams.y;
	float2 dy = ddy(w) * AtlasParams.y;
	c = tex2Dgrad(SAMPLER(AtlasColor), uv, dx, dy);
	n = tex2Dgrad(SAMPLER(AtlasNormal), uv, dx, dy);
}

// Tiberium Project @feature one world-aligned C&C4 texture: colour (linear), specular mask and
// tangent-space normal. Projected from above, and on steep ground from the two sides as well,
// weighted by the slope (triplanar): seen only from above, a cliff face stretches the texture into
// long streaks.
void SampleC4(float slot, float3 worldPos, float3 worldNormal, out float3 color, out float spec, out float3 normalTS)
{
	float4 c, n, cx, nx, cy, ny;
	SampleC4Plane(slot, worldPos.xy, c, n);
	SampleC4Plane(slot, worldPos.yz, cx, nx);	// seen along x
	SampleC4Plane(slot, worldPos.xz, cy, ny);	// seen along y
	// weights from the slope; a vertex without a normal (some of Zero Hour's blend quads) takes
	// the view from above alone rather than dividing by zero
	float3 b = pow(abs(worldNormal), 4.0) + float3(0.0, 0.0, 1e-6);
	b /= (b.x + b.y + b.z);
	c = c * b.z + cx * b.x + cy * b.y;
	n = n * b.z + nx * b.x + ny * b.y;
	color = GammaToLinear(c.xyz);
	spec = c.w;
	normalTS = n.xyz * 2.0 - 1.0;
	normalTS.y *= BumpParams.x;
}

// The ground at a pixel: Zero Hour's base tile and blend tile from the atlas, each replaced by its
// C&C4 texture where the map's terrain has one, blended by the tile's alpha edge times the vertex
// alpha (how Zero Hour's second fixed-function pass blends). Colour in linear light, with the
// specular mask and the tangent-space normal (flat where a Zero Hour tile stays).
void SampleGround(float2 baseTexCoord, float2 blendTexCoord, float vertexAlpha, float2 slots, float3 worldPos, float3 worldNormal,
	out float3 color, out float spec, out float3 normalTS)
{
	float4 zhBase = tex2D(SAMPLER(BaseSampler), baseTexCoord);
	float4 zhBlend = tex2D(SAMPLER(BaseSampler), blendTexCoord);
	float3 cBase, cBlend, nBase, nBlend;
	float sBase, sBlend;
	SampleC4(max(slots.x, 0.0), worldPos, worldNormal, cBase, sBase, nBase);
	SampleC4(max(slots.y, 0.0), worldPos, worldNormal, cBlend, sBlend, nBlend);
	float hasBase = slots.x >= 0.0 ? 1.0 : 0.0;
	float hasBlend = slots.y >= 0.0 ? 1.0 : 0.0;
	float3 flat = float3(0, 0, 1);
	cBase = lerp(GammaToLinear(zhBase.xyz) * AlbedoScale, cBase, hasBase);
	cBlend = lerp(GammaToLinear(zhBlend.xyz) * AlbedoScale, cBlend, hasBlend);
	nBase = lerp(flat, nBase, hasBase);
	nBlend = lerp(flat, nBlend, hasBlend);
	sBase = lerp(0.0, sBase, hasBase);
	sBlend = lerp(0.0, sBlend, hasBlend);
	float w = zhBlend.w * vertexAlpha;
	color = lerp(cBase, cBlend, w);
	spec = lerp(sBase, sBlend, w);
	normalTS = normalize(lerp(nBase, nBlend, w));
}

// the bumped world normal, with C&C4's terrain tangent frame
float3 BumpedNormal(float3 normalTS, float3 worldNormal)
{
	float3 n = normalize(worldNormal);
	float3 tangent = cross(n, float3(-1, 0, 0));
	float3 binormal = cross(n, float3(0, 1, 0));
	normalTS.xy *= BumpParams.y;
	return normalize(mul(normalTS, float3x3(-binormal, -tangent, n)));
}

// the specular light of the two fills at a point, as the deferred light pass adds it
float3 FillSpecularLight(float3 normal, float3 worldPos)
{
	float3 eye = normalize(EyePosition - worldPos);
	float4 fill1 = lit(dot(normal, DirectionalLight_1_Direction.xyz),
		dot(normal, normalize(DirectionalLight_1_Direction.xyz + eye)), SpecularExponent);
	float4 fill2 = lit(dot(normal, DirectionalLight_2_Direction.xyz),
		dot(normal, normalize(DirectionalLight_2_Direction.xyz + eye)), SpecularExponent);
	return DirectionalLight_1_Color.rgb * fill1.z + DirectionalLight_2_Color.rgb * fill2.z;
}

// the light buffer's specular, less the fills' share FillSpecular takes out
float3 GroundSpecularLight(float2 screenTexCoord, float3 normalTS, float3 worldNormal, float3 worldPos)
{
	float3 specular = SampleSpecularLight(screenTexCoord);
	if (FillSpecular < 1.0)
		specular = max(0.0, specular - (1.0 - FillSpecular) * FillSpecularLight(BumpedNormal(normalTS, worldNormal), worldPos));
	return specular;
}

// ============================================================================
// Deferred pre-pass: normal, specular power and depth
// ============================================================================
struct VSOutput_CreateDepthNormalMap
{
	float4 Position : POSITION;
	float3 Normal : TEXCOORD0;
	float Depth : TEXCOORD1;
	float4 BaseTexCoord_BlendTexCoord : TEXCOORD2;
	float4 WorldXY_Slots : TEXCOORD3;
	float BlendAlpha : TEXCOORD4;
	float WorldZ : TEXCOORD5;
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
	Out.BaseTexCoord_BlendTexCoord = float4(In.BaseTexCoord, In.BlendTexCoord);
	Out.WorldXY_Slots = float4(In.Position.xy, In.Slots);
	Out.BlendAlpha = In.Color.w;
	Out.WorldZ = In.Position.z;
	return Out;
}

PSOutput_CreateDepthNormalMap PS_CreateDepthNormalMap(VSOutput_CreateDepthNormalMap In)
{
	PSOutput_CreateDepthNormalMap Out;
	float3 color, normalTS;
	float spec;
	SampleGround(In.BaseTexCoord_BlendTexCoord.xy, In.BaseTexCoord_BlendTexCoord.zw, In.BlendAlpha, In.WorldXY_Slots.zw,
		float3(In.WorldXY_Slots.xy, In.WorldZ), In.Normal, color, spec, normalTS);
	float3 normal = BumpedNormal(normalTS, In.Normal);
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
// _CreateShadowMap: the terrain as a shadow caster (depth only, as C&C4's terrain)
// ============================================================================
float4 VS_CreateShadowMap(float3 Position : POSITION) : POSITION
{
	return mul(float4(Position, 1), ViewProjection);
}

float4 PS_CreateShadowMap() : COLOR
{
	return float4(1, 1, 1, 1);
}

technique _CreateShadowMap
{
	pass P0
	{
		VertexShader = compile VS_3_0 VS_CreateShadowMap();
		PixelShader = compile PS_3_0 PS_CreateShadowMap();
		ZEnable = true;
		ZWriteEnable = true;
		ZFunc = ZFUNC_INFRONT;
		CullMode = None;
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
	float4 WorldXY_Slots : TEXCOORD2;
	float2 ShroudTexCoord : TEXCOORD3;
	float4 Normal_WorldZ : TEXCOORD4;
};

VSOutputDefault VS_Default(VSInput In)
{
	VSOutputDefault Out;
	Out.Position = mul(float4(In.Position, 1), ViewProjection);
	Out.BaseTexCoord_BlendTexCoord = float4(In.BaseTexCoord, In.BlendTexCoord);
	Out.MacroTexCoord_Fog_BlendAlpha.xy = In.Position.xy * MacroScale;
	Out.MacroTexCoord_Fog_BlendAlpha.z = CalculateFog(Fog, In.Position, EyePosition);
	Out.MacroTexCoord_Fog_BlendAlpha.w = In.Color.w;
	Out.WorldXY_Slots = float4(In.Position.xy, In.Slots);
	Out.ShroudTexCoord = ShroudUV(In.Position.xy);
	Out.Normal_WorldZ = float4(In.Normal, In.Position.z);
	return Out;
}

float4 PS_Default(VSOutputDefault In, float2 vPos : PIXELLOC) : COLOR
{
	float3 baseColor, normalTS;
	float spec;
	SampleGround(In.BaseTexCoord_BlendTexCoord.xy, In.BaseTexCoord_BlendTexCoord.zw, In.MacroTexCoord_Fog_BlendAlpha.w,
		In.WorldXY_Slots.zw, float3(In.WorldXY_Slots.xy, In.Normal_WorldZ.w), In.Normal_WorldZ.xyz, baseColor, spec, normalTS);
	ApplyTopView(In.WorldXY_Slots.xy, baseColor, spec);
	float2 screenTexCoord = GetScreenTexcoordCoord(vPos);
	float3 color = (SampleDiffuseLight(screenTexCoord) + AmbientLightColor) * baseColor
		+ GroundSpecularLight(screenTexCoord, normalTS, In.Normal_WorldZ.xyz, float3(In.WorldXY_Slots.xy, In.Normal_WorldZ.w)) * SpecularColor * spec;
	float3 macro = GammaToLinear(tex2D(SAMPLER(MacroSampler), In.MacroTexCoord_Fog_BlendAlpha.xy).xyz);
	color *= macro;
	color = lerp(Fog.Color.xyz, color, In.MacroTexCoord_Fog_BlendAlpha.z);
	color *= GammaToLinear(tex2D(SAMPLER(ShroudTexture), In.ShroudTexCoord).xyz);
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
// ExtraBlend: Zero Hour's third texture where three terrains meet a cell
// ============================================================================
// Zero Hour draws those cells again, fixed-function, with the third terrain's tile and the
// blend's vertex alpha. Here they are its quads (XYZNDUV2: the tile coordinates in the first
// set, the third terrain's C&C4 slot in the second) drawn over the finished ground, lit from the
// light buffers the terrain's own pass produced under them (the albedo is the only thing that
// changes), with the same top view, macro, fog and shroud.
struct VSInput_ExtraBlend
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
	float2 Slot : TEXCOORD1;		// x: the C&C4 atlas slot, -1 for none
};

struct VSOutput_ExtraBlend
{
	float4 Position : POSITION;
	float4 TexCoord_WorldXY : TEXCOORD0;
	float4 MacroTexCoord_Fog_Alpha : TEXCOORD1;
	float4 ShroudTexCoord_Slot : TEXCOORD2;
	float4 Normal_WorldZ : TEXCOORD3;
};

VSOutput_ExtraBlend VS_ExtraBlend(VSInput_ExtraBlend In)
{
	VSOutput_ExtraBlend Out;
	Out.Position = mul(float4(In.Position, 1), ViewProjection);
	Out.TexCoord_WorldXY = float4(In.TexCoord, In.Position.xy);
	Out.MacroTexCoord_Fog_Alpha.xy = In.Position.xy * MacroScale;
	Out.MacroTexCoord_Fog_Alpha.z = CalculateFog(Fog, In.Position, EyePosition);
	Out.MacroTexCoord_Fog_Alpha.w = In.Color.w;
	Out.ShroudTexCoord_Slot = float4(ShroudUV(In.Position.xy), In.Slot.x, 0);
	Out.Normal_WorldZ = float4(In.Normal, In.Position.z);
	return Out;
}

float4 PS_ExtraBlend(VSOutput_ExtraBlend In, float2 vPos : PIXELLOC) : COLOR
{
	float4 zh = tex2D(SAMPLER(BaseSampler), In.TexCoord_WorldXY.xy);
	float slot = In.ShroudTexCoord_Slot.z;
	float3 c4, normalTS;
	float spec;
	SampleC4(max(slot, 0.0), float3(In.TexCoord_WorldXY.zw, In.Normal_WorldZ.w), In.Normal_WorldZ.xyz, c4, spec, normalTS);
	float hasC4 = slot >= 0.0 ? 1.0 : 0.0;
	float3 baseColor = lerp(GammaToLinear(zh.xyz) * AlbedoScale, c4, hasC4);
	spec = lerp(0.0, spec, hasC4);
	ApplyTopView(In.TexCoord_WorldXY.zw, baseColor, spec);
	float2 screenTexCoord = GetScreenTexcoordCoord(vPos);
	float3 color = (SampleDiffuseLight(screenTexCoord) + AmbientLightColor) * baseColor
		+ GroundSpecularLight(screenTexCoord, normalTS, In.Normal_WorldZ.xyz, float3(In.TexCoord_WorldXY.zw, In.Normal_WorldZ.w)) * SpecularColor * spec;
	color *= GammaToLinear(tex2D(SAMPLER(MacroSampler), In.MacroTexCoord_Fog_Alpha.xy).xyz);
	color = lerp(Fog.Color.xyz, color, In.MacroTexCoord_Fog_Alpha.z);
	color *= GammaToLinear(tex2D(SAMPLER(ShroudTexture), In.ShroudTexCoord_Slot.xy).xyz);
	// Zero Hour's blend: the tile's alpha by the vertex alpha
	return float4(color, zh.w * In.MacroTexCoord_Fog_Alpha.w);
}

technique ExtraBlend
{
	pass P0
	{
		VertexShader = compile VS_3_0 VS_ExtraBlend();
		PixelShader = compile PS_3_0 PS_ExtraBlend();
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
	float2 Slots : TEXCOORD4;
	float2 ShroudTexCoord : TEXCOORD5;
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
	Out.Slots = In.Slots;
	Out.ShroudTexCoord = ShroudUV(In.Position.xy);
	return Out;
}

float4 PS_Default_M(VSOutput_M In) : COLOR
{
	float3 baseColor, normalTS;
	float spec;
	SampleGround(In.BaseTexCoord_BlendTexCoord.xy, In.BaseTexCoord_BlendTexCoord.zw, In.WorldPosition_BlendAlpha.w,
		In.Slots, In.WorldPosition_BlendAlpha.xyz, In.Normal_Fog.xyz, baseColor, spec, normalTS);
	ApplyTopView(In.WorldPosition_BlendAlpha.xy, baseColor, spec);
	float3 normal = BumpedNormal(normalTS, In.Normal_Fog.xyz);
	float3 cloud = GammaToLinear(tex2D(SAMPLER(CloudTexture), In.CloudTexCoord_MacroTexCoord.xy).xyz);
	float3 light = AmbientLightColor
		+ DirectionalLight_0_Color.xyz * cloud * max(0, dot(normal, DirectionalLight_0_Direction.xyz))
		+ DirectionalLight_1_Color.xyz * max(0, dot(normal, DirectionalLight_1_Direction.xyz))
		+ DirectionalLight_2_Color.xyz * max(0, dot(normal, DirectionalLight_2_Direction.xyz));
	float3 color = light * baseColor;
	float3 macro = GammaToLinear(tex2D(SAMPLER(MacroSampler), In.CloudTexCoord_MacroTexCoord.zw).xyz);
	color *= macro;
	color = lerp(Fog.Color.xyz, color, In.Normal_Fog.w);
	color *= GammaToLinear(tex2D(SAMPLER(ShroudTexture), In.ShroudTexCoord).xyz);
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
