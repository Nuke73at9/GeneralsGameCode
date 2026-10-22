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

// Tiberium Project @feature C&C4's terrain textures for Zero Hour's terrain.
//
// C&C4 textures the ground with 256x256 colour + normal-map pairs laid over the world every
// eight cells. Zero Hour's maps instead name a Terrain.ini texture per cell and blend between
// cells with alpha edges. This keeps both: Data\TerrainCNC4.txt (in the mod) says which C&C4
// texture stands in for each Zero Hour terrain; the ones a map uses are packed into a colour
// atlas and a normal atlas (one 256-pixel slot each, with a border for filtering), and the
// terrain effect samples them world-aligned by slot while Zero Hour's own blend edges decide
// where each one shows.

#pragma once

#include <d3d9.h>

class Sage2TerrainAtlasClass
{
public:
	/// Builds the atlases for the map's terrains (Zero Hour's Terrain.ini names, in the map's
	/// texture-class order) and fills slots[i] with the atlas slot of each, -1 for terrains that
	/// keep their Zero Hour tile. False when the mapping file or the textures are not there.
	static bool Build(const char* const* terrainNames, int count, int* slots);

	static IDirect3DBaseTexture9* Get_Color();
	static IDirect3DBaseTexture9* Get_Normal();
	/// The slot grid, for the effect: slots per row, a slot's inner size and its border, in texture
	/// coordinates of the atlas
	static int Get_Slots_Per_Row();
	static float Get_Slot_Size();
	static float Get_Slot_Border();

	/// Frees the atlases (a new map, or the device going away)
	static void Release();
};
