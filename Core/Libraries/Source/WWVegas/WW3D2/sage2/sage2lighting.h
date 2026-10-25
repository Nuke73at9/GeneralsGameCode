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
// alike (they all read W3D's lights). Each preset also carries the map's post-processing look
// (grading table, blend and bloom intensity, from its PostEffectsChunk) for Sage2PostClass.

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
	// the map's post-processing look (its PostEffectsChunk): grading table, blend, bloom
	const char* lookupTexture;
	float lookupBlend;
	float bloomIntensity;
};

class Sage2LightingClass
{
public:
	/// The preset of that name, or null (names are compared without case)
	static const Sage2LightingPreset* Find(const char* name);
	/// How maps are lit from now on: "auto" picks the C&C4 preset matching each map's own time of
	/// day and weather (the C&C4 style), a preset name forces that one, "map" keeps the maps' lights.
	static void Select(const char* mode);
	/// Whether maps get C&C4 presets (auto or a fixed preset) rather than their own lights
	static bool Uses_Presets();
	/// The preset for a map being loaded (its Zero Hour time of day, 1 = morning .. 4 = night, and
	/// whether it is a snow map), under the selected mode; null to keep the map's lights. Also sets
	/// the post-processing look that goes with it.
	static const Sage2LightingPreset* Choose(int timeOfDay, bool snowy);
	/// Tiberium Project @feature Whose look the automatic choice gives the maps that are not snowy
	/// (Options.ini Sage2Look): "ra3" (the default) takes Red Alert 3's light and saturating table
	/// for the map's time of day, "cnc4" C&C4's Pan-America sets. Snow maps take C&C4's Arctic either way.
	static void Set_Look(const char* look);
	static bool Is_RA3_Look();
	/// How much of each map's own mood the C&C4 preset takes on (Options.ini Sage2MapMood, 0..1):
	/// 0 is C&C4's light as it is; 1 is as dim as the map's own lights are next to Zero Hour's
	/// default light for that time of day (never brighter), and tinted with their colours.
	static void Set_Mood(float mood);
	static float Get_Mood();
	/// Tiberium Project @tweak How strong the moonlight is on the maps that play at night (Options.ini
	/// Sage2NightLight, a scale on the night set's ambient and lights; the lamps, windows and fires
	/// keep their strength) and how much of the map's own night colour it takes (Sage2NightTint, 0..1)
	static void Set_Night(float light, float tint);
	/// Tiberium Project @feature The light a particle takes from the scene, as C&C4 and RA3 light
	/// theirs (the colour times the directional lights): the ambient and half of each light, against
	/// the same of RA3's afternoon, for which Zero Hour's particle colours read right. Gives, per
	/// colour, that ratio (sceneLight), the afternoon's sum (referenceLight, to put point lights on the
	/// same scale) and the ratio clamped to 0..1 on the display's curve (displayFactor).
	static void Get_Particle_Light(const float ambient[3], const float diffuse[3][3], float sceneLight[3], float referenceLight[3],
		float displayFactor[3]);
	/// The preset to light a map with, given the map's own lights for the time of day it plays at
	/// (ambient, and the diffuse colour of its three lights, as the map stores them): the preset
	/// itself at mood 0, darkened and tinted towards the map's own light as the mood rises.
	static Sage2LightingPreset Adapt(const Sage2LightingPreset& preset, int timeOfDay, const float mapAmbient[3], const float mapDiffuse[3][3]);
	/// Tiberium Project @bugfix Options.ini Sage2TreeLight: the trees (Zero Hour's tree buffer, lit
	/// per vertex) take the presets' light as the SAGE 2 effects do, in linear light
	static void Set_Tree_Light(bool enabled);
	static bool Is_Tree_Light_Enabled();
	/// The preset names, for diagnostics
	static int Get_Preset_Count();
	static const Sage2LightingPreset& Get_Preset(int index);
};
