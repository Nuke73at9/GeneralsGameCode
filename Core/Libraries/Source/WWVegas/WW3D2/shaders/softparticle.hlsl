//////////////////////////////////////////////////////////////////////////////
// Tiberium Project: soft particles, as C&C4 draws its particles (CommonParticle.fxh,
// ApplySoftParticleBlend): a particle fades where it meets the scene behind it, by how far in
// front of the scene it is, so smoke, dust and fire no longer cut hard lines into the ground.
//
// Zero Hour's particles are camera-facing quads built on the CPU in view space (PointGroupClass),
// drawn with W3D's sprite shaders: the texture times the vertex colour, blended additively, by
// alpha or multiplied. This draws them the same way with the fade on top, from the scene's linear
// depth (the deferred pre-pass's, view-space distance; the ground and every deferred mesh).
//   additive: the colour fades;  alpha blended: the alpha fades;  multiplied: it fades to white.
//
// Not an effect: two plain shaders, compiled by build_shaders.py to softparticle_vs.h and
// softparticle_ps.h, which sage2/sage2softparticle.cpp embeds.
//////////////////////////////////////////////////////////////////////////////

// vertex shader constants (row-vector matrices, transposed on upload)
float4x4 WorldViewProjection : register(c0);
float4x4 WorldView : register(c4);

// pixel shader constants
float4 SoftParams : register(c0);	// x: 1 / fade distance (world units), y: additive, z: multiplied, w: alpha blended
float4 FrameParams : register(c1);	// xy: 1 / the frame's size in pixels

sampler2D ParticleSampler : register(s0);	// the particle's texture, as W3D bound it
sampler2D DepthSampler : register(s3);		// the scene's linear depth (R32F, view-space z)

struct VSInput
{
	float4 Position : POSITION;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
	float4 Position : POSITION;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
	float ViewDepth : TEXCOORD1;
};

struct PSInput
{
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
	float ViewDepth : TEXCOORD1;
};

VSOutput VS_Soft(VSInput In)
{
	VSOutput Out;
	float4 position = float4(In.Position.xyz, 1.0);
	Out.Position = mul(position, WorldViewProjection);
	Out.Color = In.Color;
	Out.TexCoord = In.TexCoord;
	Out.ViewDepth = abs(mul(position, WorldView).z);
	return Out;
}

float4 PS_Soft(PSInput In, float2 Pixel : VPOS) : COLOR
{
	float4 color = tex2D(ParticleSampler, In.TexCoord) * In.Color;
	float sceneDepth = abs(tex2D(DepthSampler, (Pixel + 0.5) * FrameParams.xy).x);
	float fade = saturate((sceneDepth - In.ViewDepth) * SoftParams.x);
	color.rgb = lerp(color.rgb, color.rgb * fade, SoftParams.y);
	color.rgb = lerp(color.rgb, lerp(float3(1.0, 1.0, 1.0), color.rgb, fade), SoftParams.z);
	color.a = lerp(color.a, color.a * fade, SoftParams.w);
	return color;
}
