//////////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature Ray-traced ambient occlusion (sage2/sage2raytracing.cpp), in place of the
// screen-space estimate when the GPU traces rays: for each pixel of the half-size AO target, the
// surface the depth buffer saw there (world position and face normal), and short rays over the
// hemisphere above it against the scene's acceleration structure (the terrain and the rigid
// meshes drawn this frame). What a screen-space pass cannot know, a ray finds: a wall behind a
// unit, the underside of a bridge, a cliff off the side of the screen.
//
// Writes the openness (1 open, 0 closed) at the target's pixel; sage2ssao.cpp blurs it against the
// depth buffer and multiplies it onto the frame, as it does its own.
//
// HLSL for DXC (SPIR-V, Vulkan 1.3): scripts/tooling/rhi/build_spirv.py with manifest.txt here.
// Bindings are the RHI's table (Core/Libraries/Source/RHI/include/rhi/rhi.h).
//////////////////////////////////////////////////////////////////////////////

struct Constants
{
	row_major float4x4 ClipToWorld;	// inverse(view * projection), row vectors (Direct3D's)
	float4 Viewport;		// the 3D view in the frame (texture coordinates): xy its corner, zw 1 / its size
	float4 DepthRange;		// x: viewport MinZ, y: 1 / (MaxZ - MinZ)
	float4 Frame;			// xy: 1 / the frame's size, zw: the AO target's size
	float4 Camera;			// xyz: the camera's world position
	float4 Params;			// x: ray length (world units), y: rays per pixel, z: intensity, w: bias (world units)
};

[[vk::binding(0, 0)]] ConstantBuffer<Constants> g;
[[vk::combinedImageSampler]] [[vk::binding(8, 0)]] Texture2D<float> gDepth;
[[vk::combinedImageSampler]] [[vk::binding(8, 0)]] SamplerState gDepthSampler;
[[vk::binding(32, 0)]] [[vk::image_format("r32f")]] RWTexture2D<float> gAO;
[[vk::binding(36, 0)]] RaytracingAccelerationStructure gScene;

float SceneDepth(float2 uv)
{
	return gDepth.SampleLevel(gDepthSampler, uv, 0);
}

float3 WorldPosition(float2 uv, float d)
{
	float2 v = (uv - g.Viewport.xy) * g.Viewport.zw;
	float4 clip = float4(v.x * 2.0 - 1.0, 1.0 - v.y * 2.0, (d - g.DepthRange.x) * g.DepthRange.y, 1.0);
	float4 w = mul(clip, g.ClipToWorld);
	return w.xyz / w.w;
}

uint Hash(uint x)
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= (uint)g.Frame.z || id.y >= (uint)g.Frame.w)
		return;
	// the full-size texel at this half-size pixel's centre (as the screen-space pass reads it)
	float2 uv = (float2(id.xy) + 0.5) / g.Frame.zw;
	uv = (floor(uv / g.Frame.xy) + 0.5) * g.Frame.xy;
	float d = SceneDepth(uv);
	if (d >= 0.999999)
	{
		gAO[id.xy] = 1.0;	// nothing drawn: the sky, past the map's edge
		return;
	}
	float3 p = WorldPosition(uv, d);
	// the face's normal from the depth around it, each way from the side nearer in depth (so an
	// edge does not tilt it)
	float2 fx = float2(g.Frame.x, 0.0), fy = float2(0.0, g.Frame.y);
	float3 px1 = WorldPosition(uv + fx, SceneDepth(uv + fx)) - p;
	float3 px0 = p - WorldPosition(uv - fx, SceneDepth(uv - fx));
	float3 py1 = WorldPosition(uv + fy, SceneDepth(uv + fy)) - p;
	float3 py0 = p - WorldPosition(uv - fy, SceneDepth(uv - fy));
	float3 dx = dot(px1, px1) < dot(px0, px0) ? px1 : px0;
	float3 dy = dot(py1, py1) < dot(py0, py0) ? py1 : py0;
	float3 n = cross(dx, dy);
	float nl = length(n);
	if (nl < 1e-8)
	{
		gAO[id.xy] = 1.0;
		return;
	}
	n /= nl;
	float3 toCamera = g.Camera.xyz - p;
	if (dot(n, toCamera) < 0.0)
		n = -n;
	// off the surface, more the further it is (the depth buffer's precision falls with distance)
	float distance = length(toCamera);
	float3 origin = p + n * (g.Params.w + distance * 0.002);

	// cosine-weighted directions over the hemisphere, a fixed pattern per pixel turned by a hash
	// (stable from frame to frame; the blur evens it out)
	float3 t = normalize(abs(n.z) < 0.99 ? cross(n, float3(0, 0, 1)) : cross(n, float3(1, 0, 0)));
	float3 b = cross(n, t);
	uint seed = Hash(id.x * 1973u + id.y * 9277u + 26699u);
	float turn = (seed & 0xFFFFu) / 65536.0;
	uint rays = (uint)g.Params.y;
	float rayLength = g.Params.x;
	// the sky the surface sees, not the ground below it: each ray counts by how far it points up
	// (world z), so a sunlit wall is not darkened by the ground at its foot (the frame is multiplied
	// by this, sun and all), while an overhang, a wall beside it or a roof above still close it off
	float occlusion = 0.0, weight = 0.0;
	for (uint i = 0; i < rays; ++i)
	{
		float u1 = (i + 0.5) / rays;
		float u2 = frac(i * 0.618034 + turn);
		float r = sqrt(u1);
		float phi = 6.2831853 * u2;
		float3 dir = t * (r * cos(phi)) + b * (r * sin(phi)) + n * sqrt(1.0 - u1);
		float up = dir.z;
		if (up <= 0.02)
			continue;
		weight += up;
		RayDesc ray;
		ray.Origin = origin;
		ray.Direction = dir;
		ray.TMin = 0.0;
		ray.TMax = rayLength;
		// the nearest hit, for a falloff with distance
		RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
		q.TraceRayInline(gScene, RAY_FLAG_NONE, 0xFF, ray);
		while (q.Proceed())
		{
		}
		if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
		{
			float f = q.CommittedRayT() / rayLength;
			occlusion += up * (1.0 - f * f);
		}
	}
	gAO[id.xy] = weight > 0.0 ? saturate(1.0 - occlusion / weight * g.Params.z) : 1.0;
}
