// Tiberium Project @feature Scene post-processing on the Direct3D 9 renderer: bloom around bright
// light and a C&C4-like colour grade (slightly muted, a touch more contrast).
//
// Compile with shaders/build_shaders.py (fxc; vs_3_0 / ps_3_0).

float4 HalfPixel : register(c0);	// vertex: (-1 / width, 1 / height) of the target, to line texels up with pixels

struct VS_OUT
{
	float4 Position	: POSITION;
	float2 UV		: TEXCOORD0;
};

VS_OUT VSMain(float4 position : POSITION, float2 uv : TEXCOORD0)
{
	VS_OUT o;
	o.Position = float4(position.xy + HalfPixel.xy, 0.0, 1.0);
	o.UV = uv;
	return o;
}

sampler2D Source	: register(s0);
sampler2D Bloom		: register(s1);

float4 Params		: register(c0);	// pixel shaders: see each pass
float4 Params2		: register(c1);

// bright pass: x threshold, y 1 / (1 - threshold), zw source texel size
float4 PSBright(float2 uv : TEXCOORD0) : COLOR
{
	float2 t = Params.zw;
	float3 c = tex2D(Source, uv + float2(-t.x, -t.y)).rgb + tex2D(Source, uv + float2(t.x, -t.y)).rgb
		+ tex2D(Source, uv + float2(-t.x, t.y)).rgb + tex2D(Source, uv + float2(t.x, t.y)).rgb;
	c *= 0.25;
	float luma = dot(c, float3(0.299, 0.587, 0.114));
	float keep = saturate((luma - Params.x) * Params.y);
	return float4(c * keep, 1.0);
}

// separable blur: xy step between taps
float4 PSBlur(float2 uv : TEXCOORD0) : COLOR
{
	const float w0 = 0.227027, w1 = 0.316216, w2 = 0.070270;	// 9-tap Gaussian folded into 5 taps
	float2 s1 = Params.xy * 1.384615;
	float2 s2 = Params.xy * 3.230769;
	float3 c = tex2D(Source, uv).rgb * w0
		+ (tex2D(Source, uv + s1).rgb + tex2D(Source, uv - s1).rgb) * w1
		+ (tex2D(Source, uv + s2).rgb + tex2D(Source, uv - s2).rgb) * w2;
	return float4(c, 1.0);
}

// combine: x saturation, y contrast, z bloom strength, w vignette strength; Params2 rgb tint
float4 PSCombine(float2 uv : TEXCOORD0) : COLOR
{
	float3 c = tex2D(Source, uv).rgb;
	c += tex2D(Bloom, uv).rgb * Params.z;
	float luma = dot(c, float3(0.299, 0.587, 0.114));
	c = lerp(luma.xxx, c, Params.x);
	c = (c - 0.5) * Params.y + 0.5;
	c *= Params2.rgb;
	float2 d = uv - 0.5;
	c *= 1.0 - dot(d, d) * Params.w;
	return float4(saturate(c), 1.0);
}
