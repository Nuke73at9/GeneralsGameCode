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

// Tiberium Project @feature Zero Hour's water drawn the C&C4 way (Direct3D 9 only).
//
// C&C4's water (CNC4_Water.fx) colours its surface from the frame drawn so far, seen through
// drifting ripples, tinted by the water's colour, with the sun's sparkle from the environment
// cube and foam in the shallows; before the water draws, the frame is copied into a texture
// for it (WaterReflection.scrapeh, updateWaterRefraction). This draws Zero Hour's water
// polygons (WaterRenderObjClass's translucent water, the trapezoids of every water area) with
// tiberium_water.fxo: Begin() copies the frame once per water draw, Draw() draws the polygons
// the water renderer just put in its dynamic buffers, with C&C4's ripple normal map, foam and
// ocean cube from the mod, the shroud and fog the terrain used, and the scene's linear depth
// for the shallows. Under the surface, Red Alert 3's and C&C4's depth table and caustics
// (FXDepthLut, FXCaustics, from the mod; both games ship the same two). Rivers draw with it
// too, their ripples along the river. Visual only.

#pragma once

#include <d3d9.h>

class TextureClass;

class Sage2WaterClass
{
public:
	/// Options.ini: Sage2Water = no keeps Zero Hour's water
	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();
	/// Options.ini: Sage2WaterDepthScale (how much deeper the depth table takes the water to be:
	/// Zero Hour's water is shallower than RA3's), Sage2WaterTint (how much the water's own colour
	/// tints it; below 0: 0.3 with the depth table, C&C4's 0.4 without), Sage2WaterCaustics
	static void Set_Look(float depthScale, float tint, float caustics);
	/// Options.ini: Sage2WaterShallow, the depth in RA3's table that Zero Hour's shallow water is
	/// tinted at least as (reached over its first three units of depth; 0: as deep as it is)
	static void Set_Shallow(float tableDepth);
	/// Options.ini: Sage2WaterSky, how much of the sky the water reflects (by the Fresnel term)
	static void Set_Sky_Strength(float strength);
	/// Options.ini: Sage2WaterReflections, how strongly what stands over the water (cliffs,
	/// buildings, units, trees) is reflected in screen space (by the Fresnel term; 0: not at all)
	static void Set_Reflection_Strength(float strength);
	/// The sky the water reflects this frame: Zero Hour's sky texture for the time of day, its
	/// repeats per world unit, scroll per second (u, v) and colour (its four corners' average)
	static void Set_Sky(TextureClass* sky, float texelsPerUnit, float uScrollPerSecond, float vScrollPerSecond, const float color[3]);

	/// Before a water surface is drawn: copies the frame for the refraction. False: the water
	/// draws its own way this frame (off, no deferred buffers, no effect)
	static bool Begin();
	/// Draws the polygons the water renderer set as the current vertex and index buffers
	/// (XYZNDUV2, dynamic) with the water effect; false when Begin() did not succeed. The water's
	/// colour is Zero Hour's standing-water texture; transparentDepth and minOpacity are its
	/// WaterTransparency settings (how clear the water is, how much of the floor it may hide).
	/// A river: its first texture coordinates run across it (u) and along it (v, which Zero Hour
	/// scrolls as the river flows); the ripples and foam follow them, at these world units per
	/// unit. The edge texture's alpha, at the second coordinates, fades the banks.
	struct River
	{
		float worldPerU;
		float worldPerV;
		TextureClass* edgeTexture;
	};
	static bool Draw(unsigned vertexCount, unsigned triangleCount, TextureClass* waterTexture, float transparentDepth, float minOpacity,
		const River* river = nullptr);
	static void End();

	/// Frees the frame copy (before a device reset); made again on the next Begin()
	static void Release_Resources();
	static void Shutdown();
};
