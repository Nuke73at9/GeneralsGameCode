//////////////////////////////////////////////////////////////////////////////
// Tiberium Project: Zero Hour's black kept black through C&C4's grading table.
//
// C&C4's colour tables lift black (the Pan-American one to a dark purple, 32/7/32 of 255): its own
// maps never show pure black, but Zero Hour leaves the world black past the map's edge and under
// the shroud, and a far or low camera showed a purple sky there. This pass runs after the grade:
// where the scene before the bloom and the grade (the post chain's copy of the frame) was black,
// it blends the graded pixel back to black, fading out over the darkest couple of levels so
// nothing else changes.
//
// Compiled with shaders/sage2/build_effects.py to tiberium_keepblack.fxo, packed into the mod as
// Art\Shaders\tiberium_keepblack.fxo; Sage2PostClass draws it when it is there.
//////////////////////////////////////////////////////////////////////////////

#include "Common.fxh"

SAMPLER_2D_BEGIN( SourceTexture,
	string UIWidget = "None";
	)
	MinFilter = Point;
	MagFilter = Point;
	MipFilter = None;
	AddressU = Clamp;
	AddressV = Clamp;
SAMPLER_2D_END

// the frame's value below which a pixel counts as black, and where the grade is fully back
float2 BlackRange
<
	string UIWidget = "None";
> = float2(0.0015, 0.006);

struct VSOutput
{
	float4 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
};

VSOutput VS_KeepBlack(float3 Position : POSITION, float2 TexCoord : TEXCOORD0)
{
	VSOutput Out;
	Out.Position = float4(Position.xy, 0, 1);
	Out.TexCoord = TexCoord;
	return Out;
}

float4 PS_KeepBlack(VSOutput In) : COLOR
{
	float3 c = tex2D(SAMPLER(SourceTexture), In.TexCoord).xyz;
	float level = max(c.x, max(c.y, c.z));
	return float4(0, 0, 0, 1.0 - smoothstep(BlackRange.x, BlackRange.y, level));
}

technique Default
{
	pass P0
	{
		VertexShader = compile VS_3_0 VS_KeepBlack();
		PixelShader = compile PS_3_0 PS_KeepBlack();
		ZEnable = false;
		ZWriteEnable = false;
		CullMode = None;
		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		AlphaTestEnable = false;
	}
}
