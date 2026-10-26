// Tiberium Project @feature RHI self-test: a textured, tinted triangle. Every resource kind a draw
// binds: vertex and index buffers, a uniform buffer (binding 0), a texture with its sampler
// (binding 8, t0) and push constants. Positions are in Direct3D's clip space (y up); the device's
// viewport puts y = +1 at the top of the image, as Direct3D does.
//   dxc -spirv -fspv-target-env=vulkan1.3 -T vs_6_5 -E VSMain / -T ps_6_5 -E PSMain

struct Tint
{
	float4 color;
};

struct Push
{
	float2 offset;
	float depth;
	float pad;
};

[[vk::binding(0, 0)]] ConstantBuffer<Tint> gTint;
[[vk::combinedImageSampler]] [[vk::binding(8, 0)]] Texture2D<float4> gTexture;
[[vk::combinedImageSampler]] [[vk::binding(8, 0)]] SamplerState gSampler;
[[vk::push_constant]] Push gPush;

struct VSIn
{
	[[vk::location(0)]] float2 pos : POSITION;
	[[vk::location(1)]] float2 uv : TEXCOORD0;
};

struct VSOut
{
	float4 pos : SV_Position;
	float2 uv : TEXCOORD0;
};

VSOut VSMain(VSIn i)
{
	VSOut o;
	o.pos = float4(i.pos + gPush.offset, gPush.depth, 1.0);
	o.uv = i.uv;
	return o;
}

float4 PSMain(VSOut i) : SV_Target0
{
	return gTexture.Sample(gSampler, i.uv) * gTint.color;
}
