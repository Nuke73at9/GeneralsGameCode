//////////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature Screen-space ambient occlusion: the creases, corners and contact lines
// of the scene darkened by how much of the space around each point the geometry closes off, from
// the scene's own depth buffer (read as a texture, INTZ) after the opaque geometry is drawn.
//
// C&C4 never had it (its SSAO.fxh returns 1); this is the scalable form McGuire, Mara and Luebke
// published (SAO, 2012, after the Alchemy AO of 2011): a spiral of samples within a world-space
// radius, each weighted by how far it rises over the point's surface and by a smooth falloff with
// distance, at half resolution; then a depth-aware blur, and the result multiplied onto the frame.
//
// Not an effect: plain shaders, compiled by build_shaders.py to ssao_*.h, which
// sage2/sage2ssao.cpp embeds.
//////////////////////////////////////////////////////////////////////////////

float4 HalfPixel : register(c0);	// vertex: (-1 / width, 1 / height) of the target

struct VS_OUT
{
	float4 Position : POSITION;
	float2 UV : TEXCOORD0;
};

VS_OUT VSMain(float4 position : POSITION, float2 uv : TEXCOORD0)
{
	VS_OUT o;
	o.Position = float4(position.xy + HalfPixel.xy, 0.0, 1.0);
	o.UV = uv;
	return o;
}

// pixel shader constants
float4 Viewport : register(c0);		// the 3D view in the frame (texture coordinates): xy its corner, zw 1 / its size
float4 Projection : register(c1);	// x: P11, y: P22, z: s P31, w: s P32 (row-vector projection; s: the sign of P34)
float4 DepthParams : register(c2);	// x: s P33, y: P43, z: viewport MinZ, w: 1 / (MaxZ - MinZ)
float4 AOParams : register(c3);		// x: radius (world units), y: intensity / radius^5, z: bias (world units)
float4 Frame : register(c4);		// xy: 1 / the frame's size in pixels, zw: 1 / the AO target's size
float4 BlurParams : register(c5);	// xy: the step between taps (texture coordinates), z: depth tolerance
float4 ApplyParams : register(c6);	// x: strength, y: fade start, z: 1 / fade length (view depth)

sampler2D DepthSampler : register(s0);	// the scene's depth buffer (INTZ, point sampled)
sampler2D AOSampler : register(s1);		// the AO so far

// the distance along the view of a depth buffer value d (positive whichever way the camera's
// space is handed), and the view-space position of the pixel at frame texture coordinate uv, with
// z that distance
float ViewZ(float d)
{
	float z = (d - DepthParams.z) * DepthParams.w;
	return DepthParams.y / (z - DepthParams.x);
}

float3 ViewPosition(float2 uv, float d)
{
	float2 ndc = (uv - Viewport.xy) * Viewport.zw;
	ndc = float2(ndc.x * 2.0 - 1.0, 1.0 - ndc.y * 2.0);
	float z = ViewZ(d);
	return float3((ndc.x - Projection.z) * z / Projection.x, (ndc.y - Projection.w) * z / Projection.y, z);
}

float SceneDepth(float2 uv)
{
	return tex2Dlod(DepthSampler, float4(uv, 0, 0)).r;
}

// the ambient occlusion at a pixel of the half-size target (r: AO, 1 open, 0 closed; g: unused)
// Tiberium Project: the scene's view distance at each pixel (the sky far away), which the soft
// particles fade against: the deferred pre-pass's depth has the ground and the deferred meshes only
float4 PSSceneDepth(float2 uv : TEXCOORD0) : COLOR
{
	float d = SceneDepth(uv);
	return float4(d >= 1.0 ? 1.0e6 : ViewZ(d), 0.0, 0.0, 1.0);
}

float4 PSAO(float2 uv : TEXCOORD0, float2 vpos : VPOS) : COLOR
{
	// on the centre of a full-size texel (a half-size pixel's centre falls between four, and the
	// neighbours one texel away could round onto the same one)
	uv = (floor(uv / Frame.xy) + 0.5) * Frame.xy;
	float d = SceneDepth(uv);
	if (d >= 0.999999)
		return float4(1, 1, 1, 1);	// nothing drawn: the sky, past the map's edge
	float3 p = ViewPosition(uv, d);

	// the surface's normal from the depth around it, each way from the side nearer in depth (so
	// an edge does not tilt it)
	float2 fx = float2(Frame.x, 0.0), fy = float2(0.0, Frame.y);
	float3 px1 = ViewPosition(uv + fx, SceneDepth(uv + fx)) - p;
	float3 px0 = p - ViewPosition(uv - fx, SceneDepth(uv - fx));
	float3 py1 = ViewPosition(uv + fy, SceneDepth(uv + fy)) - p;
	float3 py0 = p - ViewPosition(uv - fy, SceneDepth(uv - fy));
	float3 dx = abs(px1.z) < abs(px0.z) ? px1 : px0;
	float3 dy = abs(py1.z) < abs(py0.z) ? py1 : py0;
	float3 n = normalize(cross(dy, dx));
	if (n.z > 0.0)
		n = -n;		// towards the camera (it looks along +z)

	// the radius on screen; a per-pixel turn of the spiral (interleaved gradient noise), which the
	// blur evens out
	const int SAMPLES = 12;
	float radius = AOParams.x;
	float2 radiusUV = float2(radius * Projection.x / p.z * 0.5 / Viewport.z, radius * Projection.y / p.z * 0.5 / Viewport.w);
	// no wider than a tenth of the view (close up, the samples would scatter across the screen)
	float limit = min(1.0, 0.1 / max(radiusUV.x * Viewport.z, 1e-4));
	radiusUV *= limit;
	float turn = 6.2831853 * frac(52.9829189 * frac(dot(vpos, float2(0.06711056, 0.00583715))));
	float sum = 0.0;
	float radius2 = radius * radius;

	[unroll]
	for (int i = 0; i < SAMPLES; ++i)
	{
		float alpha = (i + 0.5) / SAMPLES;
		float angle = alpha * (7.0 * 6.2831853) + turn;		// seven turns of the spiral
		float2 suv = uv + float2(cos(angle), sin(angle)) * radiusUV * alpha;
		float3 q = ViewPosition(suv, SceneDepth(suv));
		float3 v = q - p;
		float vv = dot(v, v);
		float vn = dot(v, n);
		float f = max(radius2 - vv, 0.0);
		sum += f * f * f * max((vn - AOParams.z) / (0.01 + vv), 0.0);
	}
	float ao = max(0.0, 1.0 - sum * AOParams.y * (5.0 / SAMPLES));
	return float4(ao, ao, ao, 1.0);
}

// depth-aware blur along BlurParams.xy (nine taps, Gaussian, each dropped by how far its depth
// lies from the centre's)
float4 PSBlur(float2 uv : TEXCOORD0) : COLOR
{
	const float weights[5] = { 0.2270270, 0.1945946, 0.1216216, 0.0540541, 0.0162162 };
	float z = ViewZ(SceneDepth(uv));
	float total = weights[0];
	float ao = tex2Dlod(AOSampler, float4(uv, 0, 0)).r * weights[0];
	[unroll]
	for (int i = 1; i < 5; ++i)
	{
		[unroll]
		for (int s = -1; s <= 1; s += 2)
		{
			float2 t = uv + BlurParams.xy * (i * s);
			float zt = ViewZ(SceneDepth(t));
			float w = weights[i] * saturate(1.0 - abs(zt - z) / (BlurParams.z * z));
			ao += tex2Dlod(AOSampler, float4(t, 0, 0)).r * w;
			total += w;
		}
	}
	ao /= total;
	return float4(ao, ao, ao, 1.0);
}

// onto the frame (multiplied: the blend takes the destination times this): the AO at the given
// strength, fading out with distance
float4 PSApply(float2 uv : TEXCOORD0) : COLOR
{
	float d = SceneDepth(uv);
	if (ApplyParams.w > 0.5)
		return float4(tex2Dlod(AOSampler, float4(uv, 0, 0)).rgb, 1.0);	// Sage2SSAODebug: the occlusion alone
	float ao = tex2Dlod(AOSampler, float4(uv, 0, 0)).r;
	float fade = d >= 0.999999 ? 0.0 : 1.0 - saturate((ViewZ(d) - ApplyParams.y) * ApplyParams.z);
	float k = lerp(1.0, ao, ApplyParams.x * fade);
	return float4(k, k, k, 1.0);
}
