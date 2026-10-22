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
// computes for each vertex rides in a second vertex stream kept here.

#pragma once

#include <d3d9.h>

class DX8VertexBufferClass;
class DX8IndexBufferClass;
class TextureClass;

class Sage2TerrainClass
{
public:
	/// One vertex buffer tile of the terrain and the normals of its vertices (3 floats each);
	/// 'version' changes whenever the renderer rewrote the tile's vertices
	struct Tile
	{
		DX8VertexBufferClass* vertexBuffer;
		const float* normals;
		unsigned version;
	};

	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();

	/// Draws the tiles (all with 'vertexCount' vertices and 'polygonCount' triangles, sharing the
	/// index buffer) textured from the atlas. Returns false when the effect is not available, in
	/// which case the caller draws the terrain its own way. Leaves no vertex streams bound.
	static bool Draw(TextureClass* atlas, DX8IndexBufferClass* indexBuffer, const Tile* tiles, int tileCount,
		unsigned vertexCount, unsigned polygonCount);

	/// Frees the normal stream (before a device reset or a new map); made again on the next Draw().
	static void Release_Resources();
	/// Frees everything (before the device goes away).
	static void Shutdown();
};
