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

#include <math.h>
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
		// the night set stored in C&C4's temperate maps; none of them plays at night and its ambient
		// is zero, so the automatic choice takes Junktown's night (a map C&C4 does play at night)
		{ "night", { 0.00f, 0.00f, 0.00f }, {
			{ { 0.94f, 0.94f, 1.25f }, { -0.321f,  0.321f, -0.891f } },
			{ { 0.25f, 0.25f, 0.38f }, {  0.978f,  0.208f,  0.000f } },
			{ { 0.25f, 0.25f, 0.38f }, { -0.052f, -0.999f,  0.000f } } }, "pan_am_lut_vol", 0.745f, 0.09f },

		// Tiberium Project @feature Red Alert 3's look, read out of the GlobalLighting and
		// PostEffectsChunk of RA3's skirmish maps (C&C4 took several of its own sets from them: its
		// evening is RA3's, its Arctic morning too). No sky light of its own by day: a warm sun from
		// lower down, two fills low across the ground, and a table that only saturates (greys stay as
		// they are), where C&C4's Pan-America table also lifts the middle tones by a fifth.
		// the afternoon set most of RA3's maps share (eleven of the forty-five); its table at a third
		// (RA3's maps use 0.2 to 0.7, mostly a half: at a half Zero Hour's pinkish desert tiles went salmon)
		{ "ra3_day", { 0.00f, 0.00f, 0.00f }, {
			{ { 1.60f, 1.45f, 1.45f }, {  0.799f, -0.014f, -0.602f } },
			{ { 0.60f, 0.65f, 0.72f }, {  0.051f,  0.965f, -0.259f } },
			{ { 1.15f, 1.20f, 1.29f }, { -0.266f, -0.929f, -0.259f } } }, "lusaturatecolors_vol", 0.33f, 0.22f },
		// RA3's morning (map_mp_2_feasel11_ep1), the light C&C4's Arctic morning took
		{ "ra3_morning", { 0.14f, 0.13f, 0.13f }, {
			{ { 1.25f, 1.21f, 1.04f }, { -0.629f,  0.349f, -0.695f } },
			{ { 0.69f, 0.67f, 0.69f }, {  0.807f,  0.586f, -0.070f } },
			{ { 0.75f, 0.83f, 0.89f }, {  0.329f, -0.903f, -0.276f } } }, "lusaturatecolors_vol", 0.29f, 0.22f },
		// RA3's evening (map_mp_2_feasel2_edit, the set C&C4's evening took): an orange sun low in
		// the west-south-west under a cool sky. RA3's other evening set (map_mp_2_black1b, a grey sky
		// light and a white sun) did not read as evening on Zero Hour's maps (Final Crusade).
		{ "ra3_evening", { 0.18f, 0.14f, 0.09f }, {
			{ { 1.67f, 1.11f, 0.75f }, {  0.673f,  0.545f, -0.500f } },
			{ { 0.32f, 0.50f, 0.78f }, { -0.940f,  0.016f, -0.342f } },
			{ { 0.49f, 0.50f, 0.67f }, {  0.304f, -0.715f, -0.629f } } }, "lusaturatecolors_vol", 0.33f, 0.22f },
		// RA3's night (map_mp_2_feasel3): blue moonlight and a table that shifts towards blue
		{ "ra3_night", { 0.08f, 0.07f, 0.07f }, {
			{ { 0.80f, 0.80f, 1.00f }, {  0.691f,  0.198f, -0.695f } },
			{ { 0.69f, 0.67f, 0.69f }, { -0.882f,  0.469f, -0.052f } },
			{ { 0.75f, 0.83f, 0.89f }, {  0.330f, -0.908f, -0.259f } } }, "lushiftblue_vol", 0.29f, 0.22f },
	};
	const int PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);

	bool s_ra3Look = true;		// Sage2Look: RA3's light and table for maps that are not snowy
	bool s_treeLight = true;	// Sage2TreeLight

	// Tiberium Project @feature the map's mood: Zero Hour's default terrain light for each time of
	// day (GameData.ini, morning to night), as brightness (luminance, 0..1) of the ambient and of
	// the sun; a map's own lights are measured against these
	const float ZH_DEFAULT_AMBIENT[4] = { 0.409f, 0.205f, 0.233f, 0.106f };
	const float ZH_DEFAULT_SUN[4] = { 0.740f, 1.000f, 0.515f, 0.207f };
	float s_mood = 0.0f;

	float Luminance(const float c[3])
	{
		return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
	}

	float Clamp(float v, float lo, float hi)
	{
		return v < lo ? lo : (v > hi ? hi : v);
	}

	// a light's colour with its brightness taken out (luminance 1), held near white
	void Hue(const float c[3], float out[3])
	{
		const float l = Luminance(c);
		for (int i = 0; i < 3; ++i)
			out[i] = l > 0.001f ? Clamp(c[i] / l, 0.6f, 1.5f) : 1.0f;
	}

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
bool Sage2LightingClass::Uses_Presets() { return s_mode != MODE_MAP; }

//-------------------------------------------------------------------------------------------------
const Sage2LightingPreset* Sage2LightingClass::Choose(int timeOfDay, bool snowy)
{
	const Sage2LightingPreset* preset = nullptr;
	if (s_mode == MODE_FIXED)
		preset = s_fixed;
	else if (s_mode == MODE_AUTO)
	{
		// Zero Hour's map themes to C&C4's: snow maps are Arctic, the rest Pan-America; the map's
		// time of day picks the set, as it does in C&C4 (1 morning, 2 afternoon, 3 evening, 4 night).
		// Under RA3's look the maps that are not snowy take RA3's set for their time of day instead
		// (snow keeps C&C4's Arctic, whose light already is RA3's).
		const char* name;
		if (snowy)
			name = timeOfDay == 4 ? "arctic" : "arctic_day";
		else if (s_ra3Look)
			name = timeOfDay == 1 ? "ra3_morning" : timeOfDay == 3 ? "ra3_evening" : timeOfDay == 4 ? "ra3_night" : "ra3_day";
		else if (timeOfDay == 3)
			name = "evening";
		else if (timeOfDay == 4)
			name = "junktown";	// C&C4's own night: moonlight from above, a green-grey ambient
		else
			name = "temperate";
		preset = Find(name);
	}
	if (preset != nullptr)
	{
		Sage2PostClass::Set_Look(preset->lookupTexture, preset->lookupBlend, preset->bloomIntensity);
		Sage2_Log("lighting: map (time of day %d%s) gets preset '%s': ambient (%.2f %.2f %.2f), sun (%.2f %.2f %.2f) along (%.3f %.3f %.3f), table %s %.2f, bloom %.2f",
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
void Sage2LightingClass::Set_Look(const char* look)
{
	s_ra3Look = look == nullptr || !Equals_No_Case(look, "cnc4");
	Sage2_Log("lighting: %s look for the maps that are not snowy", s_ra3Look ? "Red Alert 3's" : "C&C4's");
}

bool Sage2LightingClass::Is_RA3_Look() { return s_ra3Look; }

//-------------------------------------------------------------------------------------------------
void Sage2LightingClass::Set_Mood(float mood)
{
	s_mood = Clamp(mood, 0.0f, 1.0f);
}

float Sage2LightingClass::Get_Mood()
{
	return s_mood;
}

//-------------------------------------------------------------------------------------------------
Sage2LightingPreset Sage2LightingClass::Adapt(const Sage2LightingPreset& preset, int timeOfDay, const float mapAmbient[3], const float mapDiffuse[3][3])
{
	Sage2LightingPreset out = preset;
	const int t = timeOfDay < 1 ? 0 : (timeOfDay > 4 ? 3 : timeOfDay - 1);
	const float mapLevel = Luminance(mapAmbient) + Luminance(mapDiffuse[0]);
	const float defaultLevel = ZH_DEFAULT_AMBIENT[t] + ZH_DEFAULT_SUN[t];
	const float ratio = Clamp(mapLevel / defaultLevel, 0.25f, 1.0f);
	Sage2_Log("lighting: the map's own: ambient (%.2f %.2f %.2f), sun (%.2f %.2f %.2f), fills (%.2f %.2f %.2f) (%.2f %.2f %.2f); %.0f%% of Zero Hour's default for its time of day",
		mapAmbient[0], mapAmbient[1], mapAmbient[2], mapDiffuse[0][0], mapDiffuse[0][1], mapDiffuse[0][2],
		mapDiffuse[1][0], mapDiffuse[1][1], mapDiffuse[1][2], mapDiffuse[2][0], mapDiffuse[2][1], mapDiffuse[2][2],
		100.0f * mapLevel / defaultLevel);
	if (s_mood <= 0.0f)
		return out;

	// The map's colours lit Zero Hour's gamma-space textures directly; C&C4's light works in linear
	// light, so a map half as bright as the default is a quarter or so as bright here (power 2.2)
	const float scale = 1.0f + (powf(ratio, 2.2f) - 1.0f) * s_mood;
	float sunHue[3], ambientHue[3];
	Hue(mapDiffuse[0], sunHue);
	Hue(mapAmbient, ambientHue);
	for (int i = 0; i < 3; ++i)
	{
		out.ambient[i] *= scale * (1.0f + (ambientHue[i] - 1.0f) * s_mood);
		out.light[0].diffuse[i] *= scale * (1.0f + (sunHue[i] - 1.0f) * s_mood);
		for (int j = 1; j < 3; ++j)
			out.light[j].diffuse[i] *= scale * (1.0f + (ambientHue[i] - 1.0f) * s_mood);	// the fills, as the sky
	}
	Sage2_Log("lighting: with the map's mood (%.2f): light x%.2f, ambient (%.2f %.2f %.2f), sun (%.2f %.2f %.2f)",
		s_mood, scale, out.ambient[0], out.ambient[1], out.ambient[2], out.light[0].diffuse[0], out.light[0].diffuse[1], out.light[0].diffuse[2]);
	return out;
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

void Sage2LightingClass::Set_Tree_Light(bool enabled)
{
	s_treeLight = enabled;
}

bool Sage2LightingClass::Is_Tree_Light_Enabled()
{
	return s_treeLight;
}
