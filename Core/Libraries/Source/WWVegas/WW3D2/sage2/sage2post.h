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

	/// Runs the chain on the current render target (between the 3D scene and the interface).
	/// Returns false when the effects or textures are not available, so a fallback can run.
	static bool Apply();

	/// Frees the render targets (before a device reset); made again on the next Apply().
	static void Release_Resources();
	/// Frees everything (before the device goes away).
	static void Shutdown();
};
