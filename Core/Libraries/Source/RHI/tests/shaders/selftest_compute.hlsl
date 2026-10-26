// Tiberium Project @feature RHI self-test: a compute shader writing a storage buffer (binding 24, u0)
// from push constants. dxc -spirv -fspv-target-env=vulkan1.3 -T cs_6_5 -E main

struct Push
{
	uint count;
	uint scale;
	uint bias;
	uint pad;
};

[[vk::binding(24, 0)]] RWStructuredBuffer<uint> gOutput;
[[vk::push_constant]] Push gPush;

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x < gPush.count)
		gOutput[id.x] = id.x * gPush.scale + gPush.bias;
}
