/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Tiberium Project @feature C&C4's post-processing chain, with C&C4's own effects.
//
// C&C4 finishes every frame with its bloom (postfx_bloom.fxo: a bright-pass downsample to a
// 256-wide buffer, a 64-wide one, Gaussian blurs, accumulated back over the frame) and its
// look-up table pass (postfx_lookuptable.fxo: exposure, the film tone curve, then a 32x32x32
// colour-grading volume texture the map names, blended in by the map's factor). The frame
// those effects expect is linear light; Zero Hour's is display-encoded, so it is decoded on
// the way in (sRGB texture reads) and the tone curve produces display values on the way out,
// as C&C4's High path does. The look (table, blend, bloom) comes from the lighting preset.

#pragma once

class Sage2PostClass
{
public:
	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();

	/// The map look: the grading table texture (Art\Textures\<name>.dds, a volume texture), how much
	/// of it to blend in, and the bloom intensity, as C&C4 maps store them.
	static void Set_Look(const char* lookupTexture, float lookupBlend, float bloomIntensity);
	/// Exposure applied before the tone curve (1 = as C&C4)
	static void Set_Exposure(float exposure);
	/// Tiberium Project @feature How strongly the grading table applies, against the map look's own
	/// blend (Options.ini Sage2LookupStrength: 0 none, 1 as the look sets it, up to 2)
	static void Set_Lookup_Strength(float strength);
	/// Tiberium Project @feature exposure for the map's ground, the way a photographer exposes for
	/// snow: the map's average albedo (linear luminance, weighted by how much of the map each
	/// texture covers, -1 unknown). A map brighter than a mid-grey key is exposed down by
	/// (key / albedo) to the power of the strength (Sage2AutoExposure, 0 = C&C4's exposure as it is,
	/// 1 = exposed fully for the ground), never brighter than C&C4's, and never below half.
	static void Set_Scene_Albedo(float albedo);
	static void Set_Auto_Exposure(float strength);
	/// Sage2KeepBlack (default on): black before the grade stays black after it (the mod's
	/// tiberium_keepblack.fxo), so the world past the map's edge and under the shroud is not lifted
	static void Set_Keep_Black(bool enabled);

	/// Tiberium Project @feature Sage2AntiAliasing (default on): edge anti-aliasing (shaders/fxaa.hlsl)
	/// over the graded frame, in place of the multisampling RA3 and C&C4 had; the sub-pixel blending
	/// (Sage2AntiAliasingSubpixel, 0..1) softens lone bright pixels too
	static void Set_Anti_Aliasing(bool enabled, float subpixel);

	/// Runs the chain on the current render target (between the 3D scene and the interface).
	/// Returns false when the effects or textures are not available, so a fallback can run.
	static bool Apply();

	/// Frees the render targets (before a device reset); made again on the next Apply().
	static void Release_Resources();
	/// Frees everything (before the device goes away).
	static void Shutdown();
};
