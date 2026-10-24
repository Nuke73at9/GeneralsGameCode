//////////////////////////////////////////////////////////////////////////////
// Tiberium Project: Zero Hour's water drawn the C&C4 way.
//
// Built from C&C4's CNC4_Water.fx (its "ultra high" path): the surface normal from two octaves
// of a ripple normal map, rotated 55 degrees apart and drifting with time; the water's colour
// from the frame drawn so far (the refraction, offset by the ripples), tinted by the water's
// colour; the sun's sparkle from the light-space environment cube by the Fresnel term, shadowed
// by the shadow map; foam from the foam texture where the water is shallow; the shroud over the
// lit terms, fog over all. Red Alert 3 and C&C4 tint what lies under their water before the
// water draws (UnderwaterDeferred.fx, CommonUnderwater.fxh): the floor is multiplied by a depth
// table (FXDepthLut: white at the surface, teal by twenty units, near black by two hundred) and
// lit by caustics (FXCaustics: two drifting, crossed samples of its pattern, split per colour,
// strongest some ten to thirty units down, as the table's alpha says). Here that is folded into
// the water, at the floor seen under each pixel (the scene's linear depth along the eye ray);
// without the table the floor fades into the water's colour by an absorption per colour,
// red first. Zero Hour's parts: the surface is its water polygons (position, normal, vertex
// colour = its lit water colour and opacity by time of day, two texture coordinates), the
// water's colour is its standing-water texture (C&C4's PuddleColor texture), its
// TransparentWaterDepth sets how clear the water is and TransparentWaterMinOpacity how much the
// floor may vanish (both only without the table), and the shallows come from the same depth
// (C&C4 authors foam heights into its water meshes). Rivers are the same water: their ripples
// and foam run along the river's own texture coordinates, which Zero Hour scrolls as the river
// flows (as RiverWater.fx does), and their banks fade by Zero Hour's river edge texture
// (TWAlphaEdge). No reflection texture: C&C4 adds at most a tenth of a dim one, the cube
// carries the sky.
//
// Compiled with shaders/sage2/build_effects.py to tiberium_water.fxo, packed into the mod as
// Art\Shaders\tiberium_water.fxo.
//////////////////////////////////////////////////////////////////////////////

#define SUPPORT_GLOBAL_LIGHTS 1
#define SUPPORT_FOG 1

#include "Common.fxh"
#include "Gamma.fxh"
#include "ShadowMap.fxh"

float4x4 View : View;
float Time : Time;

// the ripple normal map (C&C4's FXRiverA_Nrm), two drifting octaves
SAMPLER_2D_BEGIN( NormalTexture,
	string UIWidget = "None";
	string SasBindAddress = "Water.NormalTexture";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
SAMPLER_2D_END

// the water's own colour: Zero Hour's standing-water texture (C&C4: the PuddleColor texture)
SAMPLER_2D_BEGIN( WaterTexture,
	string UIWidget = "None";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
SAMPLER_2D_END

// foam (C&C4's OceanFoam)
SAMPLER_2D_BEGIN( FoamTexture,
	string UIWidget = "None";
	string SasBindAddress = "Water.FoamTexture";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
SAMPLER_2D_END

// the frame so far, under the water
SAMPLER_2D_BEGIN( RefractionTexture,
	string UIWidget = "None";
	string SasBindAddress = "Water.RefractionTexture";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = None;
	AddressU = Clamp;
	AddressV = Clamp;
SAMPLER_2D_END

// the scene's linear (view-space) depth, from the deferred pre-pass
SAMPLER_2D_BEGIN( DepthTexture,
	string UIWidget = "None";
	string SasBindAddress = "WW3D.DepthTexture";
	)
	MinFilter = Point;
	MagFilter = Point;
	MipFilter = None;
	AddressU = Clamp;
	AddressV = Clamp;
SAMPLER_2D_END

SAMPLER_CUBE_BEGIN( EnvironmentTexture,
	string UIWidget = "None";
	string SasBindAddress = "Water.LightSpaceEnvironmentMap";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
SAMPLER_CUBE_END

// the colour of what lies under the water by its depth (RA3's and C&C4's FXDepthLut): v = (1 +
// depth) / 200, alpha = how strong the caustics are there
SAMPLER_2D_BEGIN( DepthLutTexture,
	string UIWidget = "None";
	string SasBindAddress = "Water.DepthLutTexture";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
SAMPLER_2D_END

// the caustics (FXCaustics): rgb a normal map that bends them, alpha the pattern
SAMPLER_2D_BEGIN( CausticsTexture,
	string UIWidget = "None";
	string SasBindAddress = "Water.CausticsTexture";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
SAMPLER_2D_END

// the sky the water reflects: Zero Hour's sky texture for the time of day (its reflective water's)
SAMPLER_2D_BEGIN( SkyTexture,
	string UIWidget = "None";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
SAMPLER_2D_END

// a river's banks: Zero Hour's TWAlphaEdge, alpha across the river (its second coordinates)
SAMPLER_2D_BEGIN( EdgeTexture,
	string UIWidget = "None";
	)
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
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

// x: ripple strength (C&C4: 50 with its displacement texture), y: refraction offset (C&C4: 0.15),
// z: world units of water depth over which the edge fades in, w: depth over which foam fades out
float4 WaterParams
<
	string UIWidget = "None";
> = float4(20, 0.03, 1.5, 12);

// how light fades in the water, per colour (red first), per TransparentWaterDepth of depth: by
// that depth (where Zero Hour's water turns opaque) a tenth of the floor still shows
float4 WaterAbsorption
<
	string UIWidget = "None";
> = float4(3.0, 2.2, 1.8, 0);

// Zero Hour's water transparency: x TransparentWaterDepth (world units: how clear the water is),
// y TransparentWaterMinOpacity (how much of the floor the deep water hides, 1 = all)
float4 WaterTransparency
<
	string UIWidget = "None";
> = float4(3, 1, 0, 0);

// x: ripple texture scale per world unit (C&C4: 0.0025), y: ripple drift, z: foam scale, w: foam drift
float4 WaterScales
<
	string UIWidget = "None";
> = float4(0.0025, 0.0025, 0.005, 0.011);

// x: how much the water colour tints the refraction (C&C4: 0.4), y: sparkle strength (C&C4: 2),
// z: foam strength (C&C4: 0.2 of its height factor), w: frame width in pixels / height, for the half pixel
float4 WaterLook
<
	string UIWidget = "None";
> = float4(0.4, 2, 0.2, 0);

// x: 1 with the depth table bound (else the absorption above), y: the table per unit of depth
// (RA3 and C&C4: 1/200, their water level), z: caustics strength (0 without the texture), w: how
// much deeper the table takes Zero Hour's water to be (its rivers and lakes are shallower)
float4 WaterDepthLut
<
	string UIWidget = "None";
> = float4(0, 0.005, 0, 1);

// shallow water: x the depth in the table (after the scale) its colour reaches at least, y over how
// many units of the floor's depth it gets there
float4 WaterShallow
<
	string UIWidget = "None";
> = float4(0, 3, 0, 0);

// the sky's reflection (RA3's ocean adds half its reflection texture), by the Fresnel term: x strength (0: none), y the
// sky texture's repeats per world unit, zw its scroll per second
float4 WaterSky
<
	string UIWidget = "None";
> = float4(0, 0, 0, 0);

// the sky's colour for the time of day (rgb, Zero Hour's sky corner colours), w the height of the
// sky plane over the water, in world units
float4 WaterSkyColor
<
	string UIWidget = "None";
> = float4(1, 1, 1, 400);

// a river: x 1 (else still water), y world units per unit of its first u (across), z per unit of
// v (along), w 1 with the edge texture bound
float4 WaterRiver
<
	string UIWidget = "None";
> = float4(0, 0, 0, 0);

float4 OneOverFrameBufferSizeW
<
	string UIWidget = "None";
	string SasBindAddress = "WW3D.OneOverFrameBufferSize";
> = float4(0, 0, 0, 0);

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Color : COLOR0;
	float2 TexCoord0 : TEXCOORD0;
	float2 TexCoord1 : TEXCOORD1;
};

struct VSOutput
{
	float4 Position : POSITION;
	float4 Color : COLOR0;
	float4 RippleTexCoords : TEXCOORD0;
	float4 RippleTexCoords_Body : TEXCOORD6;
	float4 FoamTexCoords : TEXCOORD1;
	float4 WorldPosition_ViewZ : TEXCOORD2;
	float4 ShroudTexCoord_Fog : TEXCOORD3;
	float4 ScreenPosition : TEXCOORD4;
	float4 ShadowMapTexCoord : TEXCOORD5;
};

VSOutput VS_Water(VSInput In)
{
	VSOutput Out;
	float3 worldPosition = In.Position;
	Out.Position = mul(float4(worldPosition, 1), ViewProjection);
	Out.Color = In.Color;

	// where the ripples and foam lie: still water by its world position; a river by its own
	// texture coordinates in world units (across, along), which Zero Hour scrolls as it flows
	float isRiver = WaterRiver.x;
	float2 surface = lerp(worldPosition.xy, In.TexCoord0 * WaterRiver.yz, isRiver);

	// C&C4's two octaves: the ripple coordinates rotated 55 degrees either way, drifting (still
	// water; a river's drift is its flow)
	float octaveSpeed = Time * WaterScales.y * (1.0 - isRiver);
	float2 ripple = surface * WaterScales.x;
	float2x2 rotate = { 0.573576436351f, -0.819152044288f, 0.819152044288f, 0.573576436351f };
	float2 diverge = mul(ripple, rotate);
	diverge.x += octaveSpeed;
	float2 divergeInv = mul(ripple, transpose(rotate));
	divergeInv.x += octaveSpeed;
	Out.RippleTexCoords = float4(diverge, divergeInv);
	Out.RippleTexCoords_Body = float4(In.TexCoord1, In.TexCoord0);	// xy: a river's edge texture, zw: the water texture

	// the foam, two drifts
	float4 foamSpeeds = Time * WaterScales.w * float4(1.0, 0.0, -1.0, 0.0) * (1.0 - isRiver);
	Out.FoamTexCoords = surface.xyxy * WaterScales.z + foamSpeeds;

	Out.WorldPosition_ViewZ.xyz = worldPosition;
	Out.WorldPosition_ViewZ.w = mul(float4(worldPosition, 1), View).z;
	Out.ShroudTexCoord_Fog.xy = (worldPosition.xy + ShroudParams.xy) * ShroudParams.zw;
	Out.ShroudTexCoord_Fog.z = CalculateFog(Fog, worldPosition, EyePosition);
	Out.ShroudTexCoord_Fog.w = 0;
	Out.ScreenPosition = Out.Position;
	Out.ShadowMapTexCoord = CalculateShadowMapTexCoord(worldPosition, SHADOWMAP_SAMPLE_DEFAULT);
	return Out;
}

// RA3's and C&C4's caustics (CommonUnderwater.fxh, GetCausticsColor), on the floor at
// floorPosition: the pattern cast along the sun, two crossed drifting samples bent by the
// texture's own normals, split per colour; lutAlpha is the depth table's alpha there
float3 Caustics(float3 floorPosition, float3 floorNormal, float lutAlpha)
{
	float3 toSun = DirectionalLight_0_Direction.xyz;
	float2 causticTexCoord = (floorPosition.xy - floorPosition.z * toSun.xy / max(toSun.z, 0.1)) * 0.004;
	float2x2 rotate = { 0.406736643f, -0.913545458f, 0.913545458f, 0.406736643f };	// 66 degrees
	float2 diverge = mul(causticTexCoord, rotate);
	diverge.x += Time * 0.12;
	float2 divergeInv = mul(causticTexCoord, transpose(rotate));
	divergeInv.x += Time * 0.12;
	float2 distort = tex2D(SAMPLER(CausticsTexture), diverge).xy + tex2D(SAMPLER(CausticsTexture), divergeInv).xy;
	distort = (distort - 1.0) * 0.3;
	float3 samp;
	samp.x = tex2D(SAMPLER(CausticsTexture), diverge + distort * 0.9).w;
	samp.y = tex2D(SAMPLER(CausticsTexture), diverge + distort).w;
	samp.z = tex2D(SAMPLER(CausticsTexture), diverge + distort * 1.1).w;
	samp.x *= tex2D(SAMPLER(CausticsTexture), -divergeInv - distort * 0.9).w;
	samp.y *= tex2D(SAMPLER(CausticsTexture), -divergeInv - distort).w;
	samp.z *= tex2D(SAMPLER(CausticsTexture), -divergeInv - distort * 1.1).w;
	samp = pow(samp, 2);
	float sunFalloff = max(0, dot(floorNormal, toSun));
	float3 caustics = samp * lutAlpha * lerp(DirectionalLight_0_Color.xyz, float3(1, 1, 1), 0.75) * sunFalloff;
	return GammaToLinear(caustics * 5);
}

float4 PS_Water(VSOutput In, uniform bool hasShadow, float2 vPos : PIXELLOC) : COLOR
{
	float3 worldPosition = In.WorldPosition_ViewZ.xyz;
	float3 worldEyeDir = normalize(EyePosition - worldPosition);

	// the surface normal from the two octaves
	float3 n0 = tex2D(SAMPLER(NormalTexture), In.RippleTexCoords.xy).xyz * 2.0 - 1.0;
	float3 n1 = tex2D(SAMPLER(NormalTexture), In.RippleTexCoords.zw).xyz * 2.0 - 1.0;
	float2 nDisp = (n0.xy + n1.xy) * 0.5;
	float3 Nn = normalize(float3(nDisp * WaterParams.x, 50.0));

	// screen position of this pixel, and of the refracted one
	float2 screenTexCoord = In.ScreenPosition.xy / In.ScreenPosition.w * float2(0.5, -0.5) + 0.5 + 0.5 * OneOverFrameBufferSizeW.xy;
	float2 refractedTexCoord = screenTexCoord + Nn.xy * WaterParams.y;

	// the water under this pixel: the scene behind the surface along the eye ray (view depth
	// for the shallows, as tuned), and that floor's place and height below the surface
	float waterViewZ = abs(In.WorldPosition_ViewZ.w);
	float sceneViewZ = abs(tex2D(SAMPLER(DepthTexture), screenTexCoord).x);
	float depth = max(sceneViewZ - waterViewZ, 0.0);
	float3 floorPosition = EyePosition + (worldPosition - EyePosition) * (max(sceneViewZ, waterViewZ) / max(waterViewZ, 0.001));
	float floorDepth = max(worldPosition.z - floorPosition.z, 0.0);
	// something in front of the surface at the refracted pixel (a unit standing in the water):
	// take the undistorted colour, as C&C4 guards its refraction
	float refractedSceneZ = abs(tex2D(SAMPLER(DepthTexture), refractedTexCoord).x);
	if (refractedSceneZ < waterViewZ)
		refractedTexCoord = screenTexCoord;
	// the frame holds display (sRGB) values: back to linear light, as everything here is
	float3 refraction = GammaToLinear(tex2D(SAMPLER(RefractionTexture), refractedTexCoord).xyz);

	float shroud = GammaToLinear(tex2D(SAMPLER(ShroudTexture), In.ShroudTexCoord_Fog.xy).xyz).x;

	// the water's colour: Zero Hour's water texture lit as its water polygons are (a river's
	// darkened by a quarter, as RiverWater.fx does its vertex colour), under the shroud as the floor is
	float3 body = GammaToLinear(tex2D(SAMPLER(WaterTexture), In.RippleTexCoords_Body.zw).xyz * In.Color.xyz)
		* lerp(1.0, 0.75, WaterRiver.x) * shroud;

	float3 underwater;
	float opacity;
	if (WaterDepthLut.x > 0)
	{
		// RA3 and C&C4: the floor times the depth table, plus the caustics, the surface opaque
		// (it carries the floor)
		// the table's colour at the floor's depth, Zero Hour's shallow water tinted at least as the
		// table is some way down (reached over the first units of depth, so the shore still blends)
		float tableDepth = floorDepth * WaterDepthLut.w + WaterShallow.x * saturate(floorDepth / max(WaterShallow.y, 0.1));
		float lutCoord = (1.0 + tableDepth) * WaterDepthLut.y;
		float4 lut = tex2Dlod(SAMPLER(DepthLutTexture), float4(lutCoord, lutCoord, 0, 0));
		// the caustics as strong as the table says at the floor's own depth
		float causticsCoord = (1.0 + floorDepth * WaterDepthLut.w) * WaterDepthLut.y;
		float causticsStrength = tex2Dlod(SAMPLER(DepthLutTexture), float4(causticsCoord, causticsCoord, 0, 0)).w;
		float3 floorNormal = normalize(cross(ddy(floorPosition), ddx(floorPosition)));
		float3 caustics = Caustics(floorPosition, floorNormal, causticsStrength) * WaterDepthLut.z * shroud;
		underwater = refraction * GammaToLinear(lut.xyz) + caustics;
		opacity = 1.0;
	}
	else
	{
		// the floor fades into the water's colour with depth, red first
		float3 floorSeen = exp(-depth * WaterAbsorption.xyz / max(WaterTransparency.x, 0.5));
		underwater = lerp(body, refraction, 1.0 - WaterTransparency.y * (1.0 - floorSeen));
		opacity = In.Color.w;
	}

	// C&C4: tinted by the water colour (its PuddleColor)
	float3 waterColor = lerp(underwater, body, WaterLook.x);

	// the sun's sparkle: the light-space cube by the Fresnel term, shadowed
	float fresnel = pow(1.0 - saturate(dot(worldEyeDir, Nn)), 1.0);
	float3 reflVect = -reflect(worldEyeDir, Nn);
	float shadowMap = 1.0;
	if (hasShadow)
		shadowMap = shadow(SAMPLER(ShadowMap), In.ShadowMapTexCoord + float4(nDisp * 0.01, 0, 0), vPos, SHADOWMAP_SAMPLE_DEFAULT);
	float3 envcolor = GammaToLinear(texCUBE(SAMPLER(EnvironmentTexture), reflVect).xyz) * DirectionalLight_0_Color.xyz * fresnel * WaterLook.y * shadowMap;

	// foam where the water is shallow
	float4 foamTexCoords = In.FoamTexCoords;
	foamTexCoords.xy += nDisp;
	float3 foamSamp = tex2D(SAMPLER(FoamTexture), 0.5 * foamTexCoords.xy).xyz * tex2D(SAMPLER(FoamTexture), 0.5 * foamTexCoords.zw).xyz;
	float foamFactor = 1.0 - saturate(depth / WaterParams.w);
	foamSamp = pow(foamSamp, 2) * foamFactor * WaterLook.z * DirectionalLight_0_Color.xyz;

	// the sky, reflected: the reflected ray up to Zero Hour's sky plane, by the square of the
	// Fresnel term above (RA3 adds half its reflection texture flat; over Zero Hour's pale desert
	// skies that, and the plain term too on the user's PC, turned Golden Oasis's lake to ice)
	float3 skyColor = 0;
	if (WaterSky.x > 0)
	{
		float3 up = reflVect;
		up.z = max(up.z, 0.05);
		float2 skyPoint = worldPosition.xy + up.xy * (WaterSkyColor.w / up.z);
		float2 skyTexCoord = skyPoint * WaterSky.y + Time * WaterSky.zw;
		skyColor = GammaToLinear(tex2D(SAMPLER(SkyTexture), skyTexCoord).xyz * WaterSkyColor.xyz) * WaterSky.x * fresnel * fresnel;
	}

	float3 color = waterColor + shroud * (foamSamp + envcolor + skyColor);
	color = lerp(Fog.Color.xyz, color, In.ShroudTexCoord_Fog.z);

	// the edge fades in over the first units of depth, a river's banks by its edge texture
	opacity *= saturate(depth / WaterParams.z);
	if (WaterRiver.w > 0)
		opacity *= tex2D(SAMPLER(EdgeTexture), In.RippleTexCoords_Body.xy).w;
	return float4(color, opacity);
}

technique Default
{
	pass P0
	{
		VertexShader = compile VS_3_0 VS_Water();
		PixelShader = compile PS_3_0 PS_Water(true);
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

technique Default_M
{
	pass P0
	{
		VertexShader = compile VS_3_0 VS_Water();
		PixelShader = compile PS_3_0 PS_Water(false);
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
