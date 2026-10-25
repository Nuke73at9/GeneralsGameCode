//////////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature Direct3D's fixed-function pipeline as shaders: what W3D still draws
// without shaders of its own (particles, the interface, untextured and texture-mapped meshes, the
// snowfall) goes through these instead, so every draw runs a shader (groundwork for a modern
// backend, which has no fixed-function pipeline at all).
//
// One vertex shader and one pixel shader for every case, steered by constants the Direct3D 8
// adapter (d3d9compat/d3d8on9.cpp) fills from the device's fixed-function state at each draw:
//   vertex: the transforms; lighting as Direct3D defines it (material or vertex colour sources,
//           global ambient, up to four directional, point or spot lights, specular), texture
//           coordinates (as stored, or generated in camera space) with their texture transforms,
//           and the point size;
//   pixel:  two texture stages with Direct3D's colour and alpha operations and arguments, and
//           the specular colour added.
// Fog and vertex blending are not here: draws that use them stay fixed-function.
//
// Not an effect: plain shaders, compiled by build_shaders.py to fixedfunction_vs.h and
// fixedfunction_ps.h.
//////////////////////////////////////////////////////////////////////////////

// ---------------------------------------------------------------------------- vertex shader
float4x4 WorldViewProjection : register(c0);	// Direct3D's row-vector matrices, uploaded transposed (mul(vector, matrix))
float4x4 WorldView : register(c4);
float4 VSFlags : register(c8);		// x lighting, y the vertices have normals, z have a diffuse colour, w have a specular colour
float4 VSFlags2 : register(c9);		// x specular on, y local viewer, z colour vertex, w normalize normals
float4 MaterialSource : register(c10);	// diffuse, ambient, specular, emissive: 0 material, 1 vertex diffuse, 2 vertex specular
float4 MaterialDiffuse : register(c11);
float4 MaterialAmbient : register(c12);
float4 MaterialSpecular : register(c13);
float4 MaterialEmissive : register(c14);
float4 MaterialPower : register(c15);	// x power
float4 GlobalAmbient : register(c16);
float4 LightCount : register(c17);	// x the number of lights on (0..4)
// four lights, seven registers each, in camera space:
//   +0 diffuse, +1 specular, +2 ambient,
//   +3 position (xyz), type in w (1 point, 2 spot, 3 directional),
//   +4 direction the light travels (xyz), range in w,
//   +5 attenuation 0, 1, 2 and spot falloff, +6 cos(theta / 2), cos(phi / 2)
float4 Lights[28] : register(c18);
float4 TexGen[2] : register(c46);	// per stage: x the coordinate set, y generation (0 stored, 1 camera normal, 2 camera position, 3 reflection), z transform count (0 none), w projected
float4x4 TexMatrix0 : register(c48);
float4x4 TexMatrix1 : register(c52);
float4 PointParams : register(c56);	// x size, y scale on, z min, w max
float4 PointScale : register(c57);	// x A, y B, z C, w viewport height

struct VSIn
{
	float4 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Diffuse : COLOR0;
	float4 Specular : COLOR1;
	float4 Tex0 : TEXCOORD0;
	float4 Tex1 : TEXCOORD1;
	float4 Tex2 : TEXCOORD2;
	float4 Tex3 : TEXCOORD3;
};

struct VSOut
{
	float4 Position : POSITION;
	float4 Diffuse : COLOR0;
	float4 Specular : COLOR1;
	float4 Tex0 : TEXCOORD0;
	float4 Tex1 : TEXCOORD1;
	float PointSize : PSIZE;
};

float4 MaterialColor(float source, float4 material, float4 diffuse, float4 specular)
{
	if (VSFlags2.z < 0.5)
		return material;
	if (source > 1.5)
		return VSFlags.w > 0.5 ? specular : material;
	if (source > 0.5)
		return VSFlags.z > 0.5 ? diffuse : material;
	return material;
}

float4 StoredCoordinates(VSIn v, float set)
{
	if (set < 0.5) return v.Tex0;
	if (set < 1.5) return v.Tex1;
	if (set < 2.5) return v.Tex2;
	return v.Tex3;
}

float4 StageCoordinates(VSIn v, float4 gen, float4x4 m, float3 viewPos, float3 viewNormal)
{
	float4 t;
	if (gen.y < 0.5)
		t = StoredCoordinates(v, gen.x);
	else if (gen.y < 1.5)
		t = float4(viewNormal, 1.0);
	else if (gen.y < 2.5)
		t = float4(viewPos, 1.0);
	else
	{
		float3 e = normalize(viewPos);
		t = float4(e - 2.0 * dot(viewNormal, e) * viewNormal, 1.0);
	}
	if (gen.z > 0.5)
	{
		// Direct3D's texture matrix takes the coordinate as a row vector, w = 1 past what it has
		t = mul(t, m);
		if (gen.w < 0.5)
		{
			// not projected: what the count leaves out is unused
			if (gen.z < 2.5) t.zw = float2(0.0, 1.0);
			else if (gen.z < 3.5) t.w = 1.0;
		}
		else
		{
			// projected: the last coordinate the count keeps divides the others (in the pixel shader)
			if (gen.z < 2.5) t = float4(t.x, 0.0, 0.0, t.y);
			else if (gen.z < 3.5) t = float4(t.x, t.y, 0.0, t.z);
		}
	}
	else
		t.w = 1.0;
	return t;
}

VSOut VSMain(VSIn v)
{
	VSOut o;
	float4 position = float4(v.Position.xyz, 1.0);
	o.Position = mul(position, WorldViewProjection);
	float3 viewPos = mul(position, WorldView).xyz;
	float3 viewNormal = float3(0.0, 0.0, 0.0);
	if (VSFlags.y > 0.5)
	{
		viewNormal = mul(v.Normal, (float3x3)WorldView);
		if (VSFlags2.w > 0.5)
			viewNormal = normalize(viewNormal);
	}
	float4 vertexDiffuse = VSFlags.z > 0.5 ? v.Diffuse : float4(1.0, 1.0, 1.0, 1.0);
	float4 vertexSpecular = VSFlags.w > 0.5 ? v.Specular : float4(0.0, 0.0, 0.0, 0.0);

	if (VSFlags.x > 0.5)
	{
		float4 cd = MaterialColor(MaterialSource.x, MaterialDiffuse, v.Diffuse, v.Specular);
		float4 ca = MaterialColor(MaterialSource.y, MaterialAmbient, v.Diffuse, v.Specular);
		float4 cs = MaterialColor(MaterialSource.z, MaterialSpecular, v.Diffuse, v.Specular);
		float4 ce = MaterialColor(MaterialSource.w, MaterialEmissive, v.Diffuse, v.Specular);
		float3 diffuse = float3(0.0, 0.0, 0.0);
		float3 ambient = GlobalAmbient.rgb;
		float3 specular = float3(0.0, 0.0, 0.0);
		float3 toEye = VSFlags2.y > 0.5 ? normalize(-viewPos) : float3(0.0, 0.0, 1.0);
		for (int i = 0; i < 4; ++i)
		{
			if (i >= (int)LightCount.x)
				break;
			float4 ld = Lights[i * 7 + 0];
			float4 ls = Lights[i * 7 + 1];
			float4 la = Lights[i * 7 + 2];
			float4 lp = Lights[i * 7 + 3];
			float4 ldir = Lights[i * 7 + 4];
			float4 latt = Lights[i * 7 + 5];
			float4 lspot = Lights[i * 7 + 6];
			float3 l;
			float atten = 1.0;
			if (lp.w > 2.5)
				l = -ldir.xyz;		// directional
			else
			{
				float3 d = lp.xyz - viewPos;
				float dist = length(d);
				l = d / max(dist, 1e-6);
				atten = dist > ldir.w ? 0.0 : 1.0 / max(latt.x + latt.y * dist + latt.z * dist * dist, 1e-6);
				if (lp.w > 1.5)
				{
					// spot: full inside theta, none past phi, the falloff power between
					float rho = dot(-l, ldir.xyz);
					float spot = rho > lspot.x ? 1.0 : (rho <= lspot.y ? 0.0
						: pow(saturate((rho - lspot.y) / max(lspot.x - lspot.y, 1e-6)), latt.w));
					atten *= spot;
				}
			}
			float nl = VSFlags.y > 0.5 ? dot(viewNormal, l) : 0.0;
			ambient += la.rgb * atten;
			if (nl > 0.0)
			{
				diffuse += ld.rgb * nl * atten;
				if (VSFlags2.x > 0.5)
				{
					float3 h = normalize(l + toEye);
					specular += ls.rgb * pow(saturate(dot(viewNormal, h)), MaterialPower.x) * atten;
				}
			}
		}
		o.Diffuse = float4(saturate(ce.rgb + ambient * ca.rgb + diffuse * cd.rgb), cd.a);
		o.Specular = float4(saturate(specular * cs.rgb), cs.a);
	}
	else
	{
		o.Diffuse = vertexDiffuse;
		o.Specular = vertexSpecular;
	}

	o.Tex0 = StageCoordinates(v, TexGen[0], TexMatrix0, viewPos, viewNormal);
	o.Tex1 = StageCoordinates(v, TexGen[1], TexMatrix1, viewPos, viewNormal);

	float size = PointParams.x;
	if (PointParams.y > 0.5)
	{
		float d = length(viewPos);
		size *= PointScale.w * rsqrt(max(PointScale.x + PointScale.y * d + PointScale.z * d * d, 1e-6));
	}
	o.PointSize = clamp(size, PointParams.z, PointParams.w);
	return o;
}

// ---------------------------------------------------------------------------- pixel shader
// per stage, colour then alpha: (operation, argument 1, argument 2, 0) and the arguments'
// modifiers (argument 1 complemented, alpha replicated, argument 2 complemented, replicated);
// an argument: 0 diffuse, 1 current, 2 texture, 3 texture factor, 4 specular
float4 StageColor0 : register(c0);
float4 StageColorMods0 : register(c1);
float4 StageAlpha0 : register(c2);
float4 StageAlphaMods0 : register(c3);
float4 StageColor1 : register(c4);
float4 StageColorMods1 : register(c5);
float4 StageAlpha1 : register(c6);
float4 StageAlphaMods1 : register(c7);
float4 TextureFactor : register(c8);
float4 PSFlags : register(c9);		// x the specular colour added, y stage 1 on, z stage 0 projected, w stage 1 projected
float4 PSTextures : register(c10);	// x stage 0 has a texture, y stage 1 has one (none reads as white)

sampler2D Stage0 : register(s0);
sampler2D Stage1 : register(s1);

struct PSIn
{
	float4 Diffuse : COLOR0;
	float4 Specular : COLOR1;
	float4 Tex0 : TEXCOORD0;
	float4 Tex1 : TEXCOORD1;
};

float4 Argument(float which, float complement, float replicate, float4 current, float4 texel, float4 diffuse, float4 specular)
{
	float4 v = which < 0.5 ? diffuse : (which < 1.5 ? current : (which < 2.5 ? texel : (which < 3.5 ? TextureFactor : specular)));
	v = replicate > 0.5 ? v.aaaa : v;
	return complement > 0.5 ? 1.0 - v : v;
}

// Direct3D's texture stage operations (D3DTEXTUREOP values) on two arguments
float4 Operation(float op, float4 a1, float4 a2, float4 current, float4 texel, float4 diffuse)
{
	if (op < 2.5) return op < 1.5 ? current : a1;			// 1 disable, 2 select argument 1
	if (op < 3.5) return a2;										// 3 select argument 2
	if (op < 4.5) return a1 * a2;									// 4 modulate
	if (op < 5.5) return a1 * a2 * 2.0;								// 5 modulate 2x
	if (op < 6.5) return a1 * a2 * 4.0;								// 6 modulate 4x
	if (op < 7.5) return a1 + a2;									// 7 add
	if (op < 8.5) return a1 + a2 - 0.5;								// 8 add signed
	if (op < 9.5) return (a1 + a2 - 0.5) * 2.0;						// 9 add signed 2x
	if (op < 10.5) return a1 - a2;									// 10 subtract
	if (op < 11.5) return a1 + a2 - a1 * a2;						// 11 add smooth
	if (op < 12.5) return lerp(a2, a1, diffuse.a);					// 12 blend by the diffuse alpha
	if (op < 13.5) return lerp(a2, a1, texel.a);					// 13 blend by the texture alpha
	if (op < 14.5) return lerp(a2, a1, TextureFactor.a);			// 14 blend by the texture factor alpha
	if (op < 15.5) return a1 + a2 * (1.0 - texel.a);				// 15 blend by the texture alpha, premultiplied
	if (op < 16.5) return lerp(a2, a1, current.a);					// 16 blend by the current alpha
	if (op < 17.5) return a1;										// 17 premodulate (next stage): as select
	if (op < 18.5) return float4(a1.rgb + a1.a * a2.rgb, a1.a);		// 18 modulate alpha, add colour
	if (op < 19.5) return float4(a1.rgb * a2.rgb + a1.a, a1.a);		// 19 modulate colour, add alpha
	if (op < 20.5) return float4(a1.rgb + (1.0 - a1.a) * a2.rgb, a1.a);	// 20 modulate inverse alpha, add colour
	if (op < 21.5) return float4((1.0 - a1.rgb) * a2.rgb + a1.a, a1.a);	// 21 modulate inverse colour, add alpha
	return dot(a1.rgb - 0.5, a2.rgb - 0.5).xxxx * 4.0;				// 24 dot product 3
}

float4 Stage(float4 color, float4 colorMods, float4 alpha, float4 alphaMods, float4 current, float4 texel, float4 diffuse, float4 specular)
{
	float4 c1 = Argument(color.y, colorMods.x, colorMods.y, current, texel, diffuse, specular);
	float4 c2 = Argument(color.z, colorMods.z, colorMods.w, current, texel, diffuse, specular);
	float4 a1 = Argument(alpha.y, alphaMods.x, alphaMods.y, current, texel, diffuse, specular);
	float4 a2 = Argument(alpha.z, alphaMods.z, alphaMods.w, current, texel, diffuse, specular);
	float3 rgb = Operation(color.x, c1, c2, current, texel, diffuse).rgb;
	float a = Operation(alpha.x, a1, a2, current, texel, diffuse).a;
	if (color.x > 23.5 && color.x < 24.5)
		a = rgb.r;	// dot product 3 fills the alpha too
	return saturate(float4(rgb, a));
}

float4 PSMain(PSIn p) : COLOR
{
	float4 diffuse = p.Diffuse;
	float4 specular = p.Specular;
	float2 uv0 = PSFlags.z > 0.5 ? p.Tex0.xy / p.Tex0.w : p.Tex0.xy;
	float4 texel0 = PSTextures.x > 0.5 ? tex2D(Stage0, uv0) : float4(1.0, 1.0, 1.0, 1.0);
	float4 current = Stage(StageColor0, StageColorMods0, StageAlpha0, StageAlphaMods0, diffuse, texel0, diffuse, specular);
	if (PSFlags.y > 0.5)
	{
		float2 uv1 = PSFlags.w > 0.5 ? p.Tex1.xy / p.Tex1.w : p.Tex1.xy;
		float4 texel1 = PSTextures.y > 0.5 ? tex2D(Stage1, uv1) : float4(1.0, 1.0, 1.0, 1.0);
		current = Stage(StageColor1, StageColorMods1, StageAlpha1, StageAlphaMods1, current, texel1, diffuse, specular);
	}
	if (PSFlags.x > 0.5)
		current.rgb = saturate(current.rgb + specular.rgb);
	return current;
}

// ---------------------------------------------------------------------------- the common cases
// Three set-ups cover nearly every draw W3D leaves to the fixed-function pipeline (counted on a
// night battle: particles, the interface, untextured meshes, the snowfall): one stage, colour and
// alpha alike, the texture times the vertex colour, the vertex colour alone, or the texture alone,
// no specular. Their own small shaders (the general one above branches through every operation
// for each pixel, which smoke and fire covering the screen pay for).
float4 PSModulate(PSIn p) : COLOR
{
	return saturate(tex2D(Stage0, p.Tex0.xy) * p.Diffuse);
}

float4 PSDiffuse(PSIn p) : COLOR
{
	return saturate(p.Diffuse);
}

float4 PSTexture(PSIn p) : COLOR
{
	return tex2D(Stage0, p.Tex0.xy);
}
