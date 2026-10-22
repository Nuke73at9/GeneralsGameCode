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

// Tiberium Project @feature C&C4's global lighting, as its maps store it.
//
// A C&C4 map's GlobalLighting chunk is the Zero Hour one grown up: a time of day and, for each
// of the four, three lights with ambient, diffuse and the direction the light travels. One set
// lights terrain and objects alike. The presets below are those values read out of C&C4's own
// skirmish maps, so a Zero Hour map can be lit the way C&C4 lights its maps: the preset replaces
// the map's lights when it is loaded, for terrain, fixed-function objects and the SAGE 2 effects
// alike (they all read W3D's lights).

#pragma once

struct Sage2LightingPreset
{
	const char* name;
	float ambient[3];
	struct Light
	{
		float diffuse[3];
		float direction[3];		// the way the light travels (map convention), unit length
	} light[3];
};

class Sage2LightingClass
{
public:
	/// The preset of that name, or null (names are compared without case)
	static const Sage2LightingPreset* Find(const char* name);
	/// Chooses the preset every map gets from now on; "map" or an unknown name keeps the maps' own lights
	static void Select(const char* name);
	/// The chosen preset, null when the maps' own lights are used
	static const Sage2LightingPreset* Get_Selected();
	/// The preset names, for diagnostics
	static int Get_Preset_Count();
	static const Sage2LightingPreset& Get_Preset(int index);
};
