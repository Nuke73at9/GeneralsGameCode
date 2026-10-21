// Tiberium Project @feature C&C4 (SAGE 2) object look on the Direct3D 9 renderer.
//
// Per-pixel lighting with C&C4's normal map (tangent space) and specular map (R = shine,
// G = reflection, B = self-illumination), plus the procedural damage of damagelook.asm.
// Lights, ambient, material and fog are the ones W3D set up for fixed-function drawing, so
// objects sit in the scene like everything else. No tangents in the vertex data: the tangent
// frame comes from screen-space derivatives of position and UVs.
//
// Compile with shaders/build_shaders.py (fxc; vs_3_0 / ps_3_0).

//------------------------------------------------------------------------------------- vertex
row_major float4x4 WorldView	: register(c0);
row_major float4x4 Projection	: register(c4);
row_major float4x4 World		: register(c8);
float4 FogParams				: register(c12);	// x start, y 1 / (end - start), z 1 = fog on

struct VS_IN
{
	float4 Position	: POSITION;
	float3 Normal	: NORMAL;
	float2 UV0		: TEXCOORD0;
	float2 UV1		: TEXCOORD1;
};

struct VS_OUT
{
	float4 Position	: POSITION;
	float4 UV		: TEXCOORD0;	// xy base, zw damage sheet
	float4 WorldPos	: TEXCOORD1;	// xyz world position, w fog (1 = clear)
	float3 Normal	: TEXCOORD2;	// world space
};

VS_OUT VSMain(VS_IN v)
{
	VS_OUT o;
	// same order as fixed-function (world-view, then projection), so depth matches later passes
	float4 viewPos = mul(WorldView, float4(v.Position.xyz, 1.0));
	o.Position = mul(Projection, viewPos);
	o.UV = float4(v.UV0, v.UV1);
	o.WorldPos.xyz = mul(World, float4(v.Position.xyz, 1.0)).xyz;
	// linear fog on depth (clip w is the view depth for perspective projections)
	o.WorldPos.w = FogParams.z > 0.5 ? saturate(1.0 - (o.Position.w - FogParams.x) * FogParams.y) : 1.0;
	o.Normal = normalize(mul((float3x3)World, v.Normal));
	return o;
}

//-------------------------------------------------------------------------------------- pixel
float4 Ambient			: register(c0);		// rgb: scene ambient x material ambient + emissive
float4 LightVector[4]	: register(c1);		// w 0: xyz = direction toward the light; w 1: xyz = position
float4 LightColor[4]	: register(c5);		// rgb diffuse x material diffuse; w 1 = light on
float4 LightAtten[4]	: register(c9);		// point lights: 1 / (x + y d + z d^2) inside range w
float4 EyePosition		: register(c13);
float4 SpecParams		: register(c14);	// x shine, y power, z reflection, w self-illumination
float4 FogColor			: register(c15);	// rgb, w material opacity
float4 DamageA			: register(c16);	// spread, noise scale on damage UVs, on base UVs, sharpness
float4 DamageB			: register(c17);	// ember rgb, ember strength
float4 DamageC			: register(c18);	// hole amount, blob growth, hole noise scale, scorch strength
float4 Options			: register(c19);	// x normal map strength, y 1 = damage sheet present

sampler2D BaseTexture	: register(s0);
sampler2D DamageSheet	: register(s1);
sampler2D Noise			: register(s2);
sampler2D NormalMap		: register(s3);
sampler2D SpecMap		: register(s4);

float3 PerturbNormal(float3 n, float3 p, float2 uv, float3 tangentNormal)
{
	// cotangent frame from derivatives (Schueler, "Normal Mapping Without Precomputed Tangents")
	float3 dp1 = ddx(p);
	float3 dp2 = ddy(p);
	float2 duv1 = ddx(uv);
	float2 duv2 = ddy(uv);
	float3 dp2perp = cross(dp2, n);
	float3 dp1perp = cross(n, dp1);
	float3 t = dp2perp * duv1.x + dp1perp * duv2.x;
	float3 b = dp2perp * duv1.y + dp1perp * duv2.y;
	float scale = rsqrt(max(max(dot(t, t), dot(b, b)), 1e-20));
	float3 world = tangentNormal.x * t * scale + tangentNormal.y * b * scale + tangentNormal.z * n;
	return normalize(world);
}

float4 PSMain(VS_OUT i, float face : VFACE) : COLOR
{
	float4 base = tex2D(BaseTexture, i.UV.xy);
	float4 spec = tex2D(SpecMap, i.UV.xy);
	float3 tn = tex2D(NormalMap, i.UV.xy).xyz * 2.0 - 1.0;
	tn.xy *= Options.x;

	float3 n = normalize(i.Normal) * (face < 0 ? -1.0 : 1.0);
	n = PerturbNormal(n, i.WorldPos.xyz, i.UV.xy, normalize(tn));
	float3 toEye = normalize(EyePosition.xyz - i.WorldPos.xyz);

	// lighting as fixed-function would do it, but per pixel with the bumped normal
	float3 diffuse = Ambient.rgb;
	float3 shine = 0;
	for (int l = 0; l < 4; ++l)
	{
		float3 L = LightVector[l].xyz;
		float atten = LightColor[l].w;
		if (LightVector[l].w > 0.5)
		{
			L -= i.WorldPos.xyz;
			float d = length(L);
			L /= d;
			atten *= (d < LightAtten[l].w) ? 1.0 / (LightAtten[l].x + LightAtten[l].y * d + LightAtten[l].z * d * d) : 0.0;
		}
		float ndl = saturate(dot(n, L));
		diffuse += LightColor[l].rgb * ndl * atten;
		float3 h = normalize(L + toEye);
		shine += LightColor[l].rgb * pow(saturate(dot(n, h)), SpecParams.y) * (ndl > 0 ? atten : 0);
	}
	diffuse = saturate(diffuse);	// fixed-function clamps the lit colour too

	// a cheap stand-in for C&C4's environment cube: sky above, ground below
	float3 r = reflect(-toEye, n);
	float3 sky = lerp(float3(0.30, 0.28, 0.24), float3(0.55, 0.62, 0.70), saturate(r.z * 0.5 + 0.5));

	float3 color = base.rgb * diffuse
		+ shine * spec.r * SpecParams.x
		+ sky * spec.g * SpecParams.z
		+ base.rgb * spec.b * SpecParams.w;
	float alpha = base.a * FogColor.w;

	// procedural damage (see damagelook.asm for the idea)
	if (DamageA.x > -0.5)
	{
		float4 sheet = Options.y > 0.5 ? tex2D(DamageSheet, i.UV.zw) : float4(1, 1, 1, 1);
		float coarse = tex2D(Noise, i.UV.zw * DamageA.y).r;
		float fine = tex2D(Noise, i.UV.xy * DamageA.z + 0.5).r;
		float holeNoise = tex2D(Noise, i.UV.xy * DamageC.z + 0.25).r;
		float n0 = lerp(fine, coarse, 0.75);
		float q = saturate((DamageA.x - n0) * DamageA.w);
		float f = q - sheet.r;
		float g = saturate(f * DamageC.y) * DamageC.w;
		color *= lerp(float3(1, 1, 1), sheet.rgb, g);
		// scorched metal doesn't shine
		float ember = saturate(1.0 - abs(f) * 5.0) * saturate((1.0 - sheet.r) * 4.0) * (1.0 - 0.6 * q) * saturate(q * 4.0) * sheet.a;
		color += DamageB.rgb * ember * DamageB.w;
		clip(max(sheet.a - 0.5, holeNoise - q * DamageC.x));
	}

	color = lerp(FogColor.rgb, color, i.WorldPos.w);
	return float4(color, alpha);
}
