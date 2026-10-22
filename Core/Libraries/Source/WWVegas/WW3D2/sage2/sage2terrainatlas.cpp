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

// Tiberium Project @feature C&C4's terrain textures for Zero Hour's terrain (see sage2terrainatlas.h)

#include "sage2terrainatlas.h"

#include <ctype.h>
#include <map>
#include <string>
#include <string.h>
#include <vector>

#include "dx8wrapper.h"
#include "sage2globals.h"
#include "sage2texture.h"
#include "WWLib/ffactory.h"
#include "WWLib/WWFILE.h"

namespace
{
	const unsigned SLOT_PIXELS = 256;		// C&C4's terrain textures
	const unsigned BORDER = 8;				// duplicated edge, for filtering and the first mip levels
	const unsigned PITCH = SLOT_PIXELS + 2 * BORDER;
	const unsigned MIP_LEVELS = 4;			// 256 .. 32 pixels a slot; deeper levels would bleed across slots

	IDirect3DTexture9* s_color = nullptr;
	IDirect3DTexture9* s_normal = nullptr;
	unsigned s_atlasSize = 0;
	int s_slotsPerRow = 0;

	std::string Lower(const std::string& s)
	{
		std::string out(s);
		for (size_t i = 0; i < out.size(); ++i)
			out[i] = (char)tolower((unsigned char)out[i]);
		return out;
	}

	// Data\TerrainCNC4.txt: "<Zero Hour terrain>=<C&C4 texture>" per line
	bool Read_Mapping(std::map<std::string, std::string>& mapping)
	{
		file_auto_ptr file(_TheFileFactory, "Data\\TerrainCNC4.txt");
		if (!file->Is_Available() || !file->Open())
			return false;
		std::vector<char> text((size_t)file->Size() + 1, '\0');
		if (!text.empty() && file->Size() > 0)
			file->Read(&text[0], (int)file->Size());
		file->Close();
		std::string line;
		for (size_t i = 0; i < text.size(); ++i)
		{
			char c = text[i];
			if (c != '\n' && c != '\r' && c != '\0')
			{
				line += c;
				continue;
			}
			size_t eq = line.find('=');
			if (eq != std::string::npos)
			{
				std::string k = line.substr(0, eq), v = line.substr(eq + 1);
				while (!k.empty() && isspace((unsigned char)k[k.size() - 1])) k.erase(k.size() - 1);
				while (!v.empty() && isspace((unsigned char)v[0])) v.erase(0, 1);
				while (!v.empty() && isspace((unsigned char)v[v.size() - 1])) v.erase(v.size() - 1);
				if (!k.empty() && !v.empty())
					mapping[Lower(k)] = Lower(v);
			}
			line.clear();
		}
		return !mapping.empty();
	}

	// writes a 256x256 BGRA image into its slot, with the border filled from the opposite edge
	// (the textures tile), on level 0 of the locked atlas
	void Blit_Slot(unsigned char* atlas, unsigned pitchBytes, int slot, const std::vector<unsigned char>& bgra, unsigned w, unsigned h)
	{
		unsigned sx = (slot % s_slotsPerRow) * PITCH, sy = (slot / s_slotsPerRow) * PITCH;
		for (unsigned y = 0; y < PITCH; ++y)
		{
			unsigned srcY = ((y + SLOT_PIXELS - BORDER) % SLOT_PIXELS) * h / SLOT_PIXELS;	// textures of another size are stretched
			unsigned char* row = atlas + (size_t)(sy + y) * pitchBytes + (size_t)sx * 4;
			for (unsigned x = 0; x < PITCH; ++x)
			{
				unsigned srcX = ((x + SLOT_PIXELS - BORDER) % SLOT_PIXELS) * w / SLOT_PIXELS;
				const unsigned char* p = &bgra[((size_t)srcY * w + srcX) * 4];
				row[x * 4] = p[0]; row[x * 4 + 1] = p[1]; row[x * 4 + 2] = p[2]; row[x * 4 + 3] = p[3];
			}
		}
	}

	// box-filtered mip levels from level 0 (the border keeps slots apart for the levels made here)
	void Make_Mips(IDirect3DTexture9* t, unsigned size)
	{
		std::vector<unsigned char> prev;
		{
			D3DLOCKED_RECT lr;
			if (FAILED(t->LockRect(0, &lr, nullptr, D3DLOCK_READONLY)))
				return;
			prev.resize((size_t)size * size * 4);
			for (unsigned y = 0; y < size; ++y)
				memcpy(&prev[(size_t)y * size * 4], (char*)lr.pBits + y * lr.Pitch, (size_t)size * 4);
			t->UnlockRect(0);
		}
		unsigned s = size;
		for (unsigned level = 1; level < MIP_LEVELS; ++level)
		{
			unsigned ns = s / 2;
			std::vector<unsigned char> next((size_t)ns * ns * 4);
			for (unsigned y = 0; y < ns; ++y)
				for (unsigned x = 0; x < ns; ++x)
					for (unsigned c = 0; c < 4; ++c)
					{
						unsigned sum = prev[((size_t)(2 * y) * s + 2 * x) * 4 + c] + prev[((size_t)(2 * y) * s + 2 * x + 1) * 4 + c]
							+ prev[((size_t)(2 * y + 1) * s + 2 * x) * 4 + c] + prev[((size_t)(2 * y + 1) * s + 2 * x + 1) * 4 + c];
						next[((size_t)y * ns + x) * 4 + c] = (unsigned char)((sum + 2) / 4);
					}
			D3DLOCKED_RECT lr;
			if (SUCCEEDED(t->LockRect(level, &lr, nullptr, 0)))
			{
				for (unsigned y = 0; y < ns; ++y)
					memcpy((char*)lr.pBits + y * lr.Pitch, &next[(size_t)y * ns * 4], (size_t)ns * 4);
				t->UnlockRect(level);
			}
			prev.swap(next);
			s = ns;
		}
	}
}

//-------------------------------------------------------------------------------------------------
bool Sage2TerrainAtlasClass::Build(const char* const* terrainNames, int count, int* slots)
{
	Release();
	for (int i = 0; i < count; ++i)
		slots[i] = -1;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	if (device8 == nullptr || count <= 0)
		return false;
	std::map<std::string, std::string> mapping;
	if (!Read_Mapping(mapping))
	{
		Sage2_Log("terrain atlas: no Data\\TerrainCNC4.txt in the mod, Zero Hour's tiles stay");
		return false;
	}

	// which C&C4 textures this map needs, one slot each
	std::vector<std::string> textures;
	std::vector<int> slotOf(count, -1);
	for (int i = 0; i < count; ++i)
	{
		std::map<std::string, std::string>::const_iterator it = mapping.find(Lower(terrainNames[i] ? terrainNames[i] : ""));
		if (it == mapping.end())
			continue;
		int slot = -1;
		for (size_t t = 0; t < textures.size(); ++t)
			if (textures[t] == it->second) { slot = (int)t; break; }
		if (slot < 0)
		{
			slot = (int)textures.size();
			textures.push_back(it->second);
		}
		slotOf[i] = slot;
	}
	if (textures.empty())
	{
		Sage2_Log("terrain atlas: none of the map's %d terrains has a C&C4 texture", count);
		return false;
	}
	s_atlasSize = 2048;
	while ((s_atlasSize / PITCH) * (s_atlasSize / PITCH) < textures.size() && s_atlasSize < 8192)
		s_atlasSize *= 2;
	s_slotsPerRow = (int)(s_atlasSize / PITCH);
	if ((size_t)s_slotsPerRow * s_slotsPerRow < textures.size())
	{
		Sage2_Log("terrain atlas: %u textures do not fit", (unsigned)textures.size());
		return false;
	}
	IDirect3DDevice9* device = device8->Get_D3D9();
	if (FAILED(device->CreateTexture(s_atlasSize, s_atlasSize, MIP_LEVELS, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &s_color, nullptr))
		|| FAILED(device->CreateTexture(s_atlasSize, s_atlasSize, MIP_LEVELS, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &s_normal, nullptr)))
	{
		Release();
		return false;
	}
	D3DLOCKED_RECT lrColor, lrNormal;
	if (FAILED(s_color->LockRect(0, &lrColor, nullptr, 0)))
	{
		Release();
		return false;
	}
	if (FAILED(s_normal->LockRect(0, &lrNormal, nullptr, 0)))
	{
		s_color->UnlockRect(0);
		Release();
		return false;
	}
	// a flat normal and mid grey where a texture is missing
	for (unsigned y = 0; y < s_atlasSize; ++y)
	{
		unsigned char* c = (unsigned char*)lrColor.pBits + y * lrColor.Pitch;
		unsigned char* n = (unsigned char*)lrNormal.pBits + y * lrNormal.Pitch;
		for (unsigned x = 0; x < s_atlasSize; ++x)
		{
			c[x * 4] = c[x * 4 + 1] = c[x * 4 + 2] = 128; c[x * 4 + 3] = 255;
			n[x * 4] = 255; n[x * 4 + 1] = 128; n[x * 4 + 2] = 128; n[x * 4 + 3] = 255;
		}
	}
	int loaded = 0;
	std::vector<unsigned char> pixels;
	for (size_t t = 0; t < textures.size(); ++t)
	{
		unsigned w = 0, h = 0;
		if (Sage2TextureClass::Load_Pixels(textures[t].c_str(), w, h, pixels) && w > 0 && h > 0)
		{
			Blit_Slot((unsigned char*)lrColor.pBits, lrColor.Pitch, (int)t, pixels, w, h);
			++loaded;
		}
		else
			Sage2_Log("terrain atlas: %s.dds missing", textures[t].c_str());
		if (Sage2TextureClass::Load_Pixels((textures[t] + "_nrm").c_str(), w, h, pixels) && w > 0 && h > 0)
			Blit_Slot((unsigned char*)lrNormal.pBits, lrNormal.Pitch, (int)t, pixels, w, h);
	}
	s_normal->UnlockRect(0);
	s_color->UnlockRect(0);
	Make_Mips(s_color, s_atlasSize);
	Make_Mips(s_normal, s_atlasSize);
	for (int i = 0; i < count; ++i)
		slots[i] = slotOf[i];
	Sage2_Log("terrain atlas: %u C&C4 textures (%d loaded) for %d terrains, %ux%u", (unsigned)textures.size(), loaded, count, s_atlasSize, s_atlasSize);
	return loaded > 0;
}

//-------------------------------------------------------------------------------------------------
IDirect3DBaseTexture9* Sage2TerrainAtlasClass::Get_Color() { return s_color; }
IDirect3DBaseTexture9* Sage2TerrainAtlasClass::Get_Normal() { return s_normal; }
int Sage2TerrainAtlasClass::Get_Slots_Per_Row() { return s_slotsPerRow; }
float Sage2TerrainAtlasClass::Get_Slot_Size() { return s_atlasSize ? (float)SLOT_PIXELS / (float)s_atlasSize : 0.0f; }
float Sage2TerrainAtlasClass::Get_Slot_Border() { return s_atlasSize ? (float)BORDER / (float)s_atlasSize : 0.0f; }

//-------------------------------------------------------------------------------------------------
void Sage2TerrainAtlasClass::Release()
{
	if (s_color) s_color->Release();
	if (s_normal) s_normal->Release();
	s_color = s_normal = nullptr;
	s_atlasSize = 0;
	s_slotsPerRow = 0;
}
