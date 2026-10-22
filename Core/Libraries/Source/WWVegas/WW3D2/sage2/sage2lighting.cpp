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

// Tiberium Project @feature C&C4's global lighting presets (see sage2lighting.h).

#include "sage2lighting.h"
#include "sage2globals.h"
#include "sage2post.h"
#include <string.h>

namespace
{
	// Read out of the GlobalLighting chunks of C&C4's skirmish maps: the set the map plays with,
	// plus the daytime sets of the maps that play at night. Light 0 is the sun; 1 and 2 are the
	// fills (no ambient of their own in any C&C4 map).
	const Sage2LightingPreset PRESETS[] =
	{
		// map_mp_10_panamerica (afternoon): the C&C4 temperate look, the default
		{ "temperate", { 0.40f, 0.45f, 0.75f }, {
			{ { 1.30f, 1.30f, 1.30f }, {  0.533f,  0.359f, -0.766f } },
			{ { 0.20f, 0.20f, 0.20f }, {  0.940f,  0.000f, -0.342f } },
			{ { 0.30f, 0.30f, 0.30f }, { -0.211f, -0.916f, -0.342f } } }, "pan_am_lut_vol", 0.745f, 0.09f },
		// the evening set C&C4's maps share
		{ "evening", { 0.18f, 0.14f, 0.09f }, {
			{ { 1.67f, 1.11f, 0.75f }, {  0.673f,  0.545f, -0.500f } },
			{ { 0.32f, 0.50f, 0.78f }, { -0.940f,  0.016f, -0.342f } },
			{ { 0.49f, 0.50f, 0.67f }, {  0.304f, -0.715f, -0.629f } } }, "pan_am_lut_vol", 0.745f, 0.09f },
		// map_mp_10_arctic_owens (plays at night)
		{ "arctic", { 0.24f, 0.24f, 0.40f }, {
			{ { 0.94f, 0.94f, 1.25f }, {  0.041f, -0.586f, -0.809f } },
			{ { 0.36f, 0.25f, 0.31f }, {  0.847f,  0.180f, -0.500f } },
			{ { 0.27f, 0.36f, 0.50f }, { -0.745f,  0.582f, -0.326f } } }, "arctic_lut_vol", 0.327f, 0.13f },
		// map_mp_10_arctic_owens, its morning set
		{ "arctic_day", { 0.14f, 0.13f, 0.13f }, {
			{ { 1.25f, 1.09f, 1.04f }, { -0.629f,  0.349f, -0.695f } },
			{ { 0.69f, 0.67f, 0.69f }, {  0.807f,  0.586f, -0.070f } },
			{ { 0.75f, 0.83f, 0.89f }, {  0.329f, -0.903f, -0.276f } } }, "arctic_lut_vol", 0.327f, 0.13f },
		// map_mp_10_junktown (plays at night)
		{ "junktown", { 0.24f, 0.30f, 0.22f }, {
			{ { 0.94f, 0.94f, 1.25f }, { -0.348f,  0.292f, -0.891f } },
			{ { 0.25f, 0.25f, 0.38f }, {  0.978f,  0.208f,  0.000f } },
			{ { 0.25f, 0.25f, 0.38f }, { -0.052f, -0.999f,  0.000f } } }, "pan_am_lut_vol", 0.5f, 0.18f },
		// map_mp_10_junktown, its morning set
		{ "junktown_day", { 0.25f, 0.25f, 0.25f }, {
			{ { 0.75f, 0.75f, 0.75f }, { -0.629f,  0.349f, -0.695f } },
			{ { 0.50f, 0.50f, 0.50f }, {  0.775f,  0.542f, -0.326f } },
			{ { 0.50f, 0.50f, 0.50f }, {  0.330f, -0.908f, -0.259f } } }, "pan_am_lut_vol", 0.5f, 0.18f },
		// the night set C&C4's temperate maps share
		{ "night", { 0.00f, 0.00f, 0.00f }, {
			{ { 0.94f, 0.94f, 1.25f }, { -0.321f,  0.321f, -0.891f } },
			{ { 0.25f, 0.25f, 0.38f }, {  0.978f,  0.208f,  0.000f } },
			{ { 0.25f, 0.25f, 0.38f }, { -0.052f, -0.999f,  0.000f } } }, "pan_am_lut_vol", 0.745f, 0.09f },
	};
	const int PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);

	enum Mode { MODE_MAP, MODE_AUTO, MODE_FIXED };
	Mode s_mode = MODE_AUTO;
	const Sage2LightingPreset* s_fixed = nullptr;

	bool Equals_No_Case(const char* a, const char* b)
	{
		for (;; ++a, ++b)
		{
			int ca = (*a >= 'A' && *a <= 'Z') ? *a + ('a' - 'A') : *a;
			int cb = (*b >= 'A' && *b <= 'Z') ? *b + ('a' - 'A') : *b;
			if (ca != cb)
				return false;
			if (ca == '\0')
				return true;
		}
	}
}

//-------------------------------------------------------------------------------------------------
const Sage2LightingPreset* Sage2LightingClass::Find(const char* name)
{
	if (name == nullptr)
		return nullptr;
	for (int i = 0; i < PRESET_COUNT; ++i)
		if (Equals_No_Case(PRESETS[i].name, name))
			return &PRESETS[i];
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
void Sage2LightingClass::Select(const char* mode)
{
	s_fixed = Find(mode);
	if (s_fixed != nullptr)
		s_mode = MODE_FIXED;
	else if (mode != nullptr && Equals_No_Case(mode, "map"))
		s_mode = MODE_MAP;
	else
		s_mode = MODE_AUTO;
	Sage2_Log("lighting: %s", s_mode == MODE_FIXED ? s_fixed->name : s_mode == MODE_MAP ? "the maps' own lights" : "C&C4 style, matched to each map");
}

//-------------------------------------------------------------------------------------------------
const Sage2LightingPreset* Sage2LightingClass::Choose(int timeOfDay, bool snowy)
{
	const Sage2LightingPreset* preset = nullptr;
	if (s_mode == MODE_FIXED)
		preset = s_fixed;
	else if (s_mode == MODE_AUTO)
	{
		// Zero Hour's map themes to C&C4's: snow maps are Arctic, the rest Pan-America; the map's
		// time of day picks the set, as it does in C&C4 (1 morning, 2 afternoon, 3 evening, 4 night)
		const char* name;
		if (snowy)
			name = timeOfDay == 4 ? "arctic" : "arctic_day";
		else if (timeOfDay == 3)
			name = "evening";
		else if (timeOfDay == 4)
			name = "night";
		else
			name = "temperate";
		preset = Find(name);
	}
	if (preset != nullptr)
	{
		Sage2PostClass::Set_Look(preset->lookupTexture, preset->lookupBlend, preset->bloomIntensity);
		Sage2_Log("lighting: map (time of day %d%s) gets C&C4 preset '%s': ambient (%.2f %.2f %.2f), sun (%.2f %.2f %.2f) along (%.3f %.3f %.3f), table %s %.2f, bloom %.2f",
			timeOfDay, snowy ? ", snow" : "", preset->name, preset->ambient[0], preset->ambient[1], preset->ambient[2],
			preset->light[0].diffuse[0], preset->light[0].diffuse[1], preset->light[0].diffuse[2],
			preset->light[0].direction[0], preset->light[0].direction[1], preset->light[0].direction[2],
			preset->lookupTexture, preset->lookupBlend, preset->bloomIntensity);
	}
	else
		Sage2_Log("lighting: map (time of day %d%s) keeps its own lights", timeOfDay, snowy ? ", snow" : "");
	return preset;
}

//-------------------------------------------------------------------------------------------------
int Sage2LightingClass::Get_Preset_Count()
{
	return PRESET_COUNT;
}

//-------------------------------------------------------------------------------------------------
const Sage2LightingPreset& Sage2LightingClass::Get_Preset(int index)
{
	return PRESETS[index];
}
