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

// Tiberium Project @feature C&C4-style procedural damage (see damagelook.h)

#include "damagelook.h"

#include <math.h>
#include <string.h>

#include "dx8caps.h"
#include "dx8wrapper.h"
#include "mesh.h"
#include "meshmdl.h"
#include "rendobj.h"
#include "texture.h"

#include "shaders/damagelook_ps.h"	// from shaders/damagelook.asm (shaders/d3dasm.py)

namespace
{
	enum InitState { NOT_TRIED, READY, UNAVAILABLE };

	InitState					s_state = NOT_TRIED;
	DWORD						s_pixelShader = 0;
	IDirect3DTexture8*		s_noise = nullptr;

	// Tuning; tools/dmg_preview.py in the extractor renders the same maths offline.
	// Tuned against C&C4 footage: a Crawler stays nearly clean to about a third down, then darkens in
	// soot patches; faint embers, and small holes only right before it dies.
	const float SPREAD_START		= 0.15f;	// damage below this shows nothing
	const float SPREAD_SCALE		= 1.3f;	// damage -> how far the burning has spread
	const float NOISE_SCALE_DAMAGE_UV	= 0.9f;
	const float NOISE_SCALE_BASE_UV	= 5.0f;
	const float SHARPNESS			= 4.0f;
	const float EMBER_COLOR[3]		= { 1.0f, 0.45f, 0.1f };
	const float EMBER_STRENGTH		= 0.35f;
	const float HOLE_START			= 0.8f;	// holes burn through from this damage...
	const float HOLE_RAMP			= 0.15f;	// ...fully open this much later
	const float HOLE_MAX			= 0.15f;
	const float BLOB_GROWTH			= 3.0f;
	const float HOLE_NOISE_SCALE		= 5.0f;
	const float SCORCH_STRENGTH		= 0.65f;
	const float MIN_VISIBLE_DAMAGE	= 0.002f;

	// The noise texture: tiling value noise, three octaves. Deterministic, so every machine draws
	// the same patterns (it is visual only either way).
	const int NOISE_SIZE = 128;

	float Noise_Hash(unsigned x, unsigned y, unsigned seed)
	{
		unsigned h = (x * 73856093u) ^ (y * 19349663u) ^ (seed * 83492791u);
		h *= 2654435761u;
		h ^= h >> 15;
		h *= 2246822519u;
		h ^= h >> 13;
		return (float)(h & 0xFFFF) / 65535.0f;
	}

	float Value_Noise(int px, int py, int cells, unsigned seed)
	{
		float tx = ((float)px + 0.5f) * (float)cells / (float)NOISE_SIZE;
		float ty = ((float)py + 0.5f) * (float)cells / (float)NOISE_SIZE;
		int x0 = (int)floorf(tx), y0 = (int)floorf(ty);
		float fx = tx - (float)x0, fy = ty - (float)y0;
		fx = fx * fx * (3.0f - 2.0f * fx);
		fy = fy * fy * (3.0f - 2.0f * fy);
		int x1 = (x0 + 1) % cells, y1 = (y0 + 1) % cells;
		x0 %= cells; y0 %= cells;
		float a = Noise_Hash(x0, y0, seed), b = Noise_Hash(x1, y0, seed);
		float c = Noise_Hash(x0, y1, seed), d = Noise_Hash(x1, y1, seed);
		return (a * (1.0f - fx) + b * fx) * (1.0f - fy) + (c * (1.0f - fx) + d * fx) * fy;
	}

	IDirect3DTexture8* Create_Noise_Texture()
	{
		static float values[NOISE_SIZE * NOISE_SIZE];
		float lo = 1e9f, hi = -1e9f;
		for (int y = 0; y < NOISE_SIZE; ++y)
		{
			for (int x = 0; x < NOISE_SIZE; ++x)
			{
				float n = 0.6f * Value_Noise(x, y, 8, 1) + 0.3f * Value_Noise(x, y, 16, 2) + 0.1f * Value_Noise(x, y, 32, 3);
				values[y * NOISE_SIZE + x] = n;
				lo = n < lo ? n : lo;
				hi = n > hi ? n : hi;
			}
		}

		IDirect3DTexture8* tex = DX8Wrapper::_Create_DX8_Texture(NOISE_SIZE, NOISE_SIZE, WW3D_FORMAT_A8R8G8B8, MIP_LEVELS_1);
		if (tex == nullptr)
			return nullptr;

		D3DLOCKED_RECT locked;
		if (FAILED(tex->LockRect(0, &locked, nullptr, 0)))
		{
			tex->Release();
			return nullptr;
		}
		for (int y = 0; y < NOISE_SIZE; ++y)
		{
			unsigned* row = (unsigned*)((unsigned char*)locked.pBits + y * locked.Pitch);
			for (int x = 0; x < NOISE_SIZE; ++x)
			{
				float n = (values[y * NOISE_SIZE + x] - lo) / (hi - lo);
				unsigned v = (unsigned)(n * 255.0f + 0.5f);
				row[x] = 0xFF000000u | (v << 16) | (v << 8) | v;
			}
		}
		tex->UnlockRect(0);
		return tex;
	}

	bool Init()
	{
		s_state = UNAVAILABLE;
		if (DX8Wrapper::_Get_D3D_Device8() == nullptr || DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Major_Version() < 2)
			return false;
		if (FAILED(DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader(DamageLookPixelShader, &s_pixelShader)))
		{
			s_pixelShader = 0;
			return false;
		}
		s_noise = Create_Noise_Texture();
		if (s_noise == nullptr)
			return false;
		s_state = READY;
		return true;
	}

	float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
}

//-------------------------------------------------------------------------------------------------
float DamageLookClass::Get_Amount(MeshClass* mesh)
{
	// the damage amount lives on the outermost render object (the unit's model)
	RenderObjClass* top = mesh;
	while (top->Get_Container() != nullptr)
		top = top->Get_Container();
	return Clamp01(top->Get_Damage_Look());
}

//-------------------------------------------------------------------------------------------------
bool DamageLookClass::Get_Constants(float amount, float constants[3][4])
{
	if (amount <= MIN_VISIBLE_DAMAGE)
		return false;
	float gate = Clamp01((amount - HOLE_START) / HOLE_RAMP);
	const float values[3][4] =
	{
		{ (amount - SPREAD_START) * SPREAD_SCALE, NOISE_SCALE_DAMAGE_UV, NOISE_SCALE_BASE_UV, SHARPNESS },
		{ EMBER_COLOR[0], EMBER_COLOR[1], EMBER_COLOR[2], EMBER_STRENGTH },
		{ gate * HOLE_MAX, BLOB_GROWTH, HOLE_NOISE_SCALE, SCORCH_STRENGTH },
	};
	memcpy(constants, values, sizeof(values));
	return true;
}

//-------------------------------------------------------------------------------------------------
bool DamageLookClass::Bind_Noise(unsigned stage)
{
	if (s_state == NOT_TRIED)
		Init();
	if (s_state != READY)
		return false;
	DX8Wrapper::Set_DX8_Texture(stage, s_noise);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MIPFILTER, D3DTEXF_NONE);
	return true;
}

//-------------------------------------------------------------------------------------------------
bool DamageLookClass::Begin(MeshClass* mesh, TextureClass* damageSheet)
{
	if (damageSheet == nullptr || mesh == nullptr)
		return false;

	float constants[3][4];
	if (!Get_Constants(Get_Amount(mesh), constants))
		return false;

	MeshModelClass* model = mesh->Peek_Model();
	if (model == nullptr || model->Get_UV_Array_Count() < 2)
		return false;

	if (!Bind_Noise(2))
		return false;
	DX8Wrapper::Set_Pixel_Shader_Constant(0, constants, 3);
	DX8Wrapper::Set_Pixel_Shader(s_pixelShader);
	return true;
}

//-------------------------------------------------------------------------------------------------
void DamageLookClass::End()
{
	DX8Wrapper::Set_Pixel_Shader(0);
	DX8Wrapper::Set_DX8_Texture(2, nullptr);
}

//-------------------------------------------------------------------------------------------------
void DamageLookClass::Shutdown()
{
	if (s_pixelShader != 0 && DX8Wrapper::_Get_D3D_Device8() != nullptr)
	{
		DX8Wrapper::Set_Pixel_Shader(0);
		DX8Wrapper::_Get_D3D_Device8()->DeletePixelShader(s_pixelShader);
	}
	s_pixelShader = 0;
	if (s_noise != nullptr)
	{
		DX8Wrapper::Set_DX8_Texture(2, nullptr);
		s_noise->Release();
		s_noise = nullptr;
	}
	s_state = NOT_TRIED;
}
