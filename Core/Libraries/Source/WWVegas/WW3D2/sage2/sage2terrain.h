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

// Tiberium Project @feature The terrain drawn the C&C4 way.
//
// Zero Hour's height-map renderer keeps the terrain in vertex buffer tiles (position, baked
// vertex colour, the base tile's atlas coordinates, the alpha-edged blend tile's) and draws them
// fixed-function. This draws the same tiles with tiberium_terrain.fxo (shaders/sage2, built on
// C&C4's own terrain shader): the deferred depth/normal pre-pass over every tile, C&C4's
// directional light over the screen, then the tiles lit from the light buffers in linear light
// with the cloud layer, written sRGB like the objects. The height-field normal the renderer
// computes for each vertex rides in a second vertex stream kept here, with the atlas slots of
// the C&C4 textures that stand in for the cell's terrains (Sage2TerrainAtlasClass), which the
// effect samples world-aligned, every eight cells, as C&C4 does.

#pragma once

#include <d3d9.h>

class DX8VertexBufferClass;
class DX8IndexBufferClass;
class TextureClass;

class Sage2TerrainClass
{
public:
	/// One vertex buffer tile of the terrain and its side data: per vertex the normal (3 floats)
	/// and the C&C4 atlas slots of the cell's base and blend terrains (2 floats, -1 for none);
	/// 'version' changes whenever the renderer rewrote the tile's vertices
	struct Tile
	{
		DX8VertexBufferClass* vertexBuffer;
		const float* normals;
		unsigned version;
	};
	/// floats per vertex in 'normals'
	enum { SIDE_FLOATS = 5 };

	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();
	/// How much Zero Hour's tile colours are lifted for linear-light rendering (1 = as painted)
	static void Set_Albedo_Scale(float scale);
	/// World units one C&C4 terrain texture spans (80: eight cells, as in C&C4)
	static void Set_Tile_World_Size(float size);

	/// The map's shroud, multiplied over the terrain at the end of the pass (Zero Hour draws it
	/// as an extra pass that needs an equal depth, which a different vertex pipeline cannot
	/// give it): the texture, one texel a cell, and how a world position reaches its texel:
	/// (world.xy + offset) * scale. No texture: no shroud.
	struct Shroud
	{
		TextureClass* texture;
		float offsetX, offsetY;
		float scaleX, scaleY;
	};

	/// Draws the tiles (all with 'vertexCount' vertices and 'polygonCount' triangles, sharing the
	/// index buffer) textured from the atlas, with the shroud over them. Returns false when the
	/// effect is not available, in which case the caller draws the terrain its own way. Leaves no
	/// vertex streams bound.
	static bool Draw(TextureClass* atlas, DX8IndexBufferClass* indexBuffer, const Tile* tiles, int tileCount,
		unsigned vertexCount, unsigned polygonCount, const Shroud& shroud);

	/// Frees the normal stream (before a device reset or a new map); made again on the next Draw().
	static void Release_Resources();
	/// Frees everything (before the device goes away).
	static void Shutdown();
};
