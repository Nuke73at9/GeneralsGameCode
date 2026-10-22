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

// Tiberium Project @feature C&C4's terrain top view: scorch marks baked into the ground (Direct3D 9 only).
//
// C&C4 does not draw scorch marks over the terrain. A top-down camera paints every mark into a
// map-wide texture before the frame (TerrainTopView.scrapeh, TerrainScorch.fx), and the ground
// shaders read that texture at the pixel's world position and fold it into the albedo before
// lighting (Terrain.fxh: base = base * (1 - mark.a) + mark.rgb, no specular under the burn). A
// mark is then lit, shadowed and bumped like the ground, never fights its depth, and costs one
// texture read. This keeps that texture for Zero Hour's scorch quads: redrawn only when a mark is
// added or the map changes, read by tiberium_terrain.fx through Sage2GlobalsClass. Visual only.

#pragma once

#include <d3d9.h>

class TextureClass;

class Sage2TopViewClass
{
public:
	/// Options.ini: Sage2TopView = no leaves Zero Hour's scorch quads; Sage2TopViewSize (2048)
	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();
	static void Set_Size(unsigned size);

	/// The marks changed (one added, the map reset): the texture is redrawn at the next Update()
	static void Invalidate();
	/// True when Update() has something to do (enabled, device ready, marks changed)
	static bool Needs_Update();

	/// Redraws the texture: 'minX,minY'-'maxX,maxY' is the world rectangle the map covers; the
	/// caller draws the marks (W3DScorch::drawScorches, which calls Draw() below) between Begin()
	/// and End(). Begin() false: nothing to do or the device cannot.
	static bool Begin(float minX, float minY, float maxX, float maxY);
	/// Draws scorch quads (Zero Hour's XYZDUV1 layout: position, colour, atlas coordinate) into the
	/// top view with 'marks' as the atlas. Only between Begin() and End().
	static void Draw(IDirect3DVertexBuffer9* vertices, IDirect3DIndexBuffer9* indices, TextureClass* marks,
		unsigned vertexCount, unsigned triangleCount);
	static void End();
	/// The top view is being drawn (W3DScorch draws into it instead of over the terrain)
	static bool Is_Rendering();

	/// The texture, for the ground shaders (nullptr while there is none), and how a world position
	/// reaches its texel: (xy - offset) * scale, v flipped (C&C4's ScorchMarkUVOffsetMultiplier)
	static IDirect3DBaseTexture9* Get_Texture();
	static void Get_Mapping(float& offsetX, float& offsetY, float& scaleX, float& scaleY);
	/// A top view exists: the ground shaders show the marks, W3DScorch draws none over the terrain
	static bool Is_Available();

	/// Frees the texture (before a device reset or a new map); redrawn on the next Update()
	static void Release_Resources();
	static void Shutdown();
};
