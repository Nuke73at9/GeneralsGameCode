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
	static float Get_Albedo_Scale();
	/// World units one C&C4 terrain texture spans (80: eight cells, as in C&C4)
	static void Set_Tile_World_Size(float size);
	/// The bumps: 'flipY' reads the normal maps' green channel the other way round (if the ridges
	/// come out lit from the wrong side), 'scale' their strength (0.75 as C&C4, 0 flat)
	static void Set_Bump(bool flipY, float scale);
	/// Tiberium Project @feature How much of the fill lights' highlight the ground keeps (Options.ini
	/// Sage2TerrainFillSpecular): 1 as C&C4's light pass gives it, 0 as Red Alert 3's terrain (the sun's alone)
	static void Set_Fill_Specular(float amount);

	/// Tiberium Project @feature The map's cells for blending terrains across the cell borders (see
	/// CrossCellTerrains in tiberium_terrain.fx): 'cells' holds width x height entries of four bytes,
	/// row by row from the map's first cell: the base terrain's C&C4 slot, the blend terrain's and the
	/// extra blend's (the third terrain Zero Hour's extra pass lays on; 255: none), and flags: 64 where
	/// the cell keeps Zero Hour's own edges (paving and other built ground, special edge tiles,
	/// terrains without a C&C4 texture), plus 4 x the quarters of the cell the blend covers, plus the
	/// quarters the extra blend covers.
	/// 'border' is the map's border in cells (cell index = world / cell size + border). Null: none.
	static void Set_Cell_Map(const unsigned char* cells, int width, int height, int border, float cellSize);
	/// Options.ini Sage2TerrainCrossBlend (on), Sage2TerrainCrossNoise (how far a soft noise moves
	/// the edges, 0.5), Sage2TerrainCrossSoftness (the edges' softness, 0.25; 0.5 a plain ramp)
	static void Set_Cross_Blend(bool enabled, float noise, float softness);

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

	/// What the last Draw() did, for the passes drawn over the terrain afterwards (the roads): whether
	/// the effect drew it this frame, deferred-lit (the light buffers hold the terrain's light) or
	/// forward, and the shroud it used. Reset_Draw() clears it before the terrain is drawn again.
	static bool Drew_This_Frame();
	static bool Drew_Deferred();
	static const Shroud& Get_Shroud();
	static void Reset_Draw();

	/// Tiberium Project @bugfix Zero Hour's three-way blend cells (a third terrain over the base and
	/// blend ones) the C&C4 way: the quads its extra blend pass just put in the current (dynamic)
	/// vertex and index buffers, XYZNDUV2 with the tile coordinates in the first set and the third
	/// terrain's C&C4 slot (or -1) in the second, drawn over the ground with 'tiles' (the pass's
	/// tile texture, whose alpha with the vertex alpha blends them), lit from the light buffers of
	/// this frame's terrain. Only after a deferred Draw(); false: the caller draws them its way.
	static bool Draw_Extra_Blend(TextureClass* tiles, unsigned vertexCount, unsigned triangleCount);

	/// The terrain as a shadow caster: the tiles drawn depth-only with the effect's _CreateShadowMap
	/// technique (the shadow map's camera is set). False when the effect is not available.
	static bool Draw_Shadow(DX8IndexBufferClass* indexBuffer, const Tile* tiles, int tileCount, unsigned vertexCount, unsigned polygonCount);

	/// Frees the normal stream (before a device reset or a new map); made again on the next Draw().
	static void Release_Resources();
	/// Frees everything (before the device goes away).
	static void Shutdown();
};
