// Tiberium Project @feature RHI self-test: a compute shader writing a storage image (binding 32, u0 of
// the image slots): red and green follow x and y, blue is fixed.
// dxc -spirv -fspv-target-env=vulkan1.3 -T cs_6_5 -E main

struct Push
{
	uint width;
	uint height;
	uint blue;
	uint pad;
};

[[vk::binding(32, 0)]] [[vk::image_format("rgba8")]] RWTexture2D<float4> gImage;
[[vk::push_constant]] Push gPush;

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= gPush.width || id.y >= gPush.height)
		return;
	gImage[id.xy] = float4(id.x * 4 / 255.0, id.y * 4 / 255.0, gPush.blue / 255.0, 1.0);
}
