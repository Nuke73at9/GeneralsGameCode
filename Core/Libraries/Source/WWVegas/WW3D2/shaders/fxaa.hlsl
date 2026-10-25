// Tiberium Project @feature Edge anti-aliasing for the C&C4 post chain (Direct3D 9).
//
// RA3 and C&C4 shipped with multisampling; Zero Hour's frame here goes through a deferred light
// pre-pass and a post chain built on plain render targets, which multisampling does not reach.
// This is a fast approximate anti-aliasing pass in the manner of Timothy Lottes' FXAA (quality
// variant), run on the finished, graded frame: it finds where the brightness steps sharply
// between neighbouring pixels, follows the edge both ways to see where it ends, and blends each
// pixel across the edge by how far along the stair step it sits (plus a little for lone pixels).
//
// Compile with shaders/build_shaders.py (ps_3_0; drawn with postprocess.hlsl's VSMain).

sampler2D Source : register(s0);	// the graded frame (display values), linear filtering, clamped

float4 Params : register(c0);		// xy: 1 / the frame's size, z: sub-pixel blending (0..1), w: edge threshold
float4 Params2 : register(c1);		// x: edge threshold in the darks

float Luma(float2 uv)
{
	return dot(tex2Dlod(Source, float4(uv, 0.0, 0.0)).rgb, float3(0.299, 0.587, 0.114));
}

float4 PSFXAA(float2 uv : TEXCOORD0) : COLOR
{
	const float2 px = Params.xy;
	float3 rgbM = tex2Dlod(Source, float4(uv, 0.0, 0.0)).rgb;
	float lumaM = dot(rgbM, float3(0.299, 0.587, 0.114));
	float lumaN = Luma(uv + float2(0.0, -px.y));
	float lumaS = Luma(uv + float2(0.0, px.y));
	float lumaW = Luma(uv + float2(-px.x, 0.0));
	float lumaE = Luma(uv + float2(px.x, 0.0));

	// a sharp enough step anywhere around this pixel, or nothing to do
	float rangeMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
	float rangeMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
	float range = rangeMax - rangeMin;
	if (range < max(Params2.x, rangeMax * Params.w))
		return float4(rgbM, 1.0);

	float lumaNW = Luma(uv + float2(-px.x, -px.y));
	float lumaNE = Luma(uv + float2(px.x, -px.y));
	float lumaSW = Luma(uv + float2(-px.x, px.y));
	float lumaSE = Luma(uv + float2(px.x, px.y));

	// sub-pixel blending: how much this pixel stands apart from its neighbourhood
	float average = (2.0 * (lumaN + lumaS + lumaW + lumaE) + (lumaNW + lumaNE + lumaSW + lumaSE)) / 12.0;
	float apart = saturate(abs(average - lumaM) / range);
	float smooth = apart * apart * (3.0 - 2.0 * apart);
	float subpixel = smooth * smooth * Params.z;

	// the edge runs horizontally when the brightness changes more up and down than sideways
	float horizontal = abs(lumaNW + lumaSW - 2.0 * lumaW) + 2.0 * abs(lumaN + lumaS - 2.0 * lumaM) + abs(lumaNE + lumaSE - 2.0 * lumaE);
	float vertical = abs(lumaNW + lumaNE - 2.0 * lumaN) + 2.0 * abs(lumaW + lumaE - 2.0 * lumaM) + abs(lumaSW + lumaSE - 2.0 * lumaS);
	bool isHorizontal = horizontal >= vertical;

	// which side of the pixel the edge is on: the neighbour across it that differs most
	float luma1 = isHorizontal ? lumaN : lumaW;
	float luma2 = isHorizontal ? lumaS : lumaE;
	float gradient1 = abs(luma1 - lumaM);
	float gradient2 = abs(luma2 - lumaM);
	bool towards1 = gradient1 >= gradient2;
	float stepLength = isHorizontal ? px.y : px.x;
	float edgeLuma = towards1 ? 0.5 * (luma1 + lumaM) : 0.5 * (luma2 + lumaM);
	float gradientScaled = 0.25 * max(gradient1, gradient2);
	if (towards1)
		stepLength = -stepLength;

	// follow the edge both ways (on the boundary between the two rows of pixels) to its ends
	float2 edgeUV = uv;
	float2 along;
	if (isHorizontal)
	{
		edgeUV.y += 0.5 * stepLength;
		along = float2(px.x, 0.0);
	}
	else
	{
		edgeUV.x += 0.5 * stepLength;
		along = float2(0.0, px.y);
	}
	float2 uvN = edgeUV - along;
	float2 uvP = edgeUV + along;
	float endN = Luma(uvN) - edgeLuma;
	float endP = Luma(uvP) - edgeLuma;
	bool doneN = abs(endN) >= gradientScaled;
	bool doneP = abs(endP) >= gradientScaled;
	const float stride[10] = { 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0 };
	[loop]
	for (int i = 0; i < 10 && !(doneN && doneP); ++i)
	{
		if (!doneN)
		{
			uvN -= along * stride[i];
			endN = Luma(uvN) - edgeLuma;
			doneN = abs(endN) >= gradientScaled;
		}
		if (!doneP)
		{
			uvP += along * stride[i];
			endP = Luma(uvP) - edgeLuma;
			doneP = abs(endP) >= gradientScaled;
		}
	}

	// how far along the stair step this pixel is: the nearer end decides, if its step goes the
	// way this pixel does
	float distanceN = isHorizontal ? (uv.x - uvN.x) : (uv.y - uvN.y);
	float distanceP = isHorizontal ? (uvP.x - uv.x) : (uvP.y - uv.y);
	bool nearN = distanceN < distanceP;
	float distanceNear = min(distanceN, distanceP);
	float span = distanceN + distanceP;
	bool centreBelow = lumaM < edgeLuma;
	bool consistent = ((nearN ? endN : endP) < 0.0) != centreBelow;
	float edgeOffset = consistent ? (0.5 - distanceNear / span) : 0.0;
	float offset = max(edgeOffset, subpixel);

	float2 finalUV = uv;
	if (isHorizontal)
		finalUV.y += offset * stepLength;
	else
		finalUV.x += offset * stepLength;
	return float4(tex2Dlod(Source, float4(finalUV, 0.0, 0.0)).rgb, 1.0);
}
