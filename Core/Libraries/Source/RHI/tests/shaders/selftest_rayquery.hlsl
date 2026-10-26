// Tiberium Project @feature RHI self-test: inline ray queries against the top-level acceleration
// structure (binding 36). A grid of rays goes straight up from the ground; each writes the nearest
// instance it hits (InstanceID + 1) or 0, for the test to compare with its own answer.
// dxc -spirv -fspv-target-env=vulkan1.3 -T cs_6_5 -E main

struct Push
{
	uint size;		// the grid is size x size
	float extent;	// over [-extent, extent] in x and z
	uint pad0;
	uint pad1;
};

[[vk::binding(36, 0)]] RaytracingAccelerationStructure gScene;
[[vk::binding(24, 0)]] RWStructuredBuffer<uint> gOutput;
[[vk::push_constant]] Push gPush;

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (any(id.xy >= gPush.size))
		return;
	float2 xz = ((float2(id.xy) + 0.5) / float(gPush.size) * 2.0 - 1.0) * gPush.extent;
	RayDesc ray;
	ray.Origin = float3(xz.x, 0.0, xz.y);
	ray.Direction = float3(0.0, 1.0, 0.0);
	ray.TMin = 0.0;
	ray.TMax = 100.0;
	// the closest hit: no accept-first flag
	RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
	q.TraceRayInline(gScene, RAY_FLAG_NONE, 0xFF, ray);
	while (q.Proceed())
	{
	}
	uint result = 0;
	if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
		result = q.CommittedInstanceID() + 1;
	gOutput[id.y * gPush.size + id.x] = result;
}
