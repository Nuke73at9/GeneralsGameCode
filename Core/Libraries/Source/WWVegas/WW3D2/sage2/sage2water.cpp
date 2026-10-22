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

// Tiberium Project @feature Zero Hour's water drawn the C&C4 way (see sage2water.h)

#include "sage2water.h"

#include "dx8indexbuffer.h"
#include "dx8vertexbuffer.h"
#include "dx8wrapper.h"
#include "sage2effect.h"
#include "sage2globals.h"
#include "sage2material.h"
#include "sage2scene.h"
#include "sage2shadow.h"
#include "sage2terrain.h"
#include "sage2texture.h"
#include "texture.h"
#include "textureloader.h"

namespace
{
	bool s_enabled = true;
	enum EffectState { NOT_TRIED, READY, UNAVAILABLE };
	EffectState s_state = NOT_TRIED;
	Sage2EffectClass* s_effect = nullptr;
	int s_techDefault = -1, s_techNoShadow = -1;
	IDirect3DVertexDeclaration9* s_declaration = nullptr;
	const unsigned VERTEX_STRIDE = 44;	// XYZNDUV2

	IDirect3DTexture9* s_refraction = nullptr;	// the frame, copied
	unsigned s_width = 0, s_height = 0;
	bool s_begun = false;
	bool s_logged = false;

	bool Make_Effect(IDirect3DDevice9* device)
	{
		if (s_state != NOT_TRIED)
			return s_state == READY;
		s_state = UNAVAILABLE;
		s_effect = Sage2MaterialClass::Get_Effect("tiberium_water");
		if (s_effect == nullptr)
			return false;
		s_techDefault = s_effect->Find_Technique("Default");
		s_techNoShadow = s_effect->Find_Technique("Default_M");
		if (s_techDefault < 0 || s_techNoShadow < 0)
			return false;
		D3DVERTEXELEMENT9 el[] =
		{
			{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
			{ 0, 24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
			{ 0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			{ 0, 36, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
			D3DDECL_END()
		};
		if (FAILED(device->CreateVertexDeclaration(el, &s_declaration)))
			return false;
		s_state = READY;
		Sage2_Log("water: C&C4-style water ready");
		return true;
	}

	IDirect3DBaseTexture9* D3D_Texture(TextureClass* texture)
	{
		if (texture == nullptr)
			return nullptr;
		if (!texture->Is_Initialized())
			TextureLoader::Request_Foreground_Loading(texture);
		return texture->Peek_D3D_Base_Texture();
	}

	// the frame so far, into the refraction texture
	bool Copy_Frame(IDirect3DDevice9* device)
	{
		IDirect3DSurface9* target = nullptr;
		if (FAILED(device->GetRenderTarget(0, &target)) || target == nullptr)
			return false;
		D3DSURFACE_DESC desc;
		target->GetDesc(&desc);
		if (s_refraction != nullptr && (s_width != desc.Width || s_height != desc.Height))
			Sage2WaterClass::Release_Resources();
		if (s_refraction == nullptr)
		{
			if (FAILED(device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &s_refraction, nullptr)))
			{
				target->Release();
				return false;
			}
			s_width = desc.Width;
			s_height = desc.Height;
		}
		IDirect3DSurface9* copy = nullptr;
		bool ok = SUCCEEDED(s_refraction->GetSurfaceLevel(0, &copy)) && SUCCEEDED(device->StretchRect(target, nullptr, copy, nullptr, D3DTEXF_NONE));
		if (copy) copy->Release();
		target->Release();
		return ok;
	}
}

//-------------------------------------------------------------------------------------------------
void Sage2WaterClass::Set_Enabled(bool enabled) { s_enabled = enabled; }
bool Sage2WaterClass::Is_Enabled() { return s_enabled; }

//-------------------------------------------------------------------------------------------------
bool Sage2WaterClass::Begin()
{
	s_begun = false;
	// the shallows need the scene's linear depth, which the deferred path makes
	if (!s_enabled || !Sage2TerrainClass::Drew_Deferred() || !Sage2GlobalsClass::Is_Deferred_Lighting_Available())
		return false;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	if (device8 == nullptr)
		return false;
	IDirect3DDevice9* device = device8->Get_D3D9();
	if (!Make_Effect(device))
		return false;
	DX8Wrapper::Apply_Render_State_Changes();
	if (!Copy_Frame(device))
		return false;
	s_begun = true;
	return true;
}

//-------------------------------------------------------------------------------------------------
bool Sage2WaterClass::Draw(unsigned vertexCount, unsigned triangleCount)
{
	if (!s_begun || triangleCount == 0)
		return false;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DDevice9* device = device8->Get_D3D9();

	// the water renderer's dynamic buffers, as the wrapper has them
	RenderStateStruct state;
	DX8Wrapper::Get_Render_State(state);
	if (state.vertex_buffers[0] == nullptr || state.index_buffer == nullptr
		|| (state.vertex_buffer_types[0] != BUFFER_TYPE_DYNAMIC_DX8 && state.vertex_buffer_types[0] != BUFFER_TYPE_DX8)
		|| (state.index_buffer->Type() != BUFFER_TYPE_DYNAMIC_DX8 && state.index_buffer->Type() != BUFFER_TYPE_DX8))
		return false;
	IDirect3DVertexBuffer9* vb = static_cast<DX8VertexBufferClass*>(state.vertex_buffers[0])->Get_DX8_Vertex_Buffer();
	IDirect3DIndexBuffer9* ib = static_cast<DX8IndexBufferClass*>(state.index_buffer)->Get_DX8_Index_Buffer();
	if (vb == nullptr || ib == nullptr)
		return false;
	unsigned baseVertex = state.vba_offset + state.index_base_offset;
	unsigned startIndex = state.iba_offset;

	Sage2DrawState drawState;
	Sage2GlobalsClass::Apply(*s_effect, drawState);
	s_effect->Set_Texture("RefractionTexture", s_refraction);
	s_effect->Set_Texture("NormalTexture", Sage2TextureClass::Get("fxrivera_nrm"));
	s_effect->Set_Texture("FoamTexture", Sage2TextureClass::Get("oceanfoam"));
	IDirect3DBaseTexture9* ocean = Sage2TextureClass::Get("fxoceancubemap");
	if (ocean != nullptr)
		s_effect->Set_Texture("EnvironmentTexture", ocean);
	const Sage2TerrainClass::Shroud& shroud = Sage2TerrainClass::Get_Shroud();
	IDirect3DBaseTexture9* shroudTexture = D3D_Texture(shroud.texture);
	if (shroudTexture != nullptr)
	{
		s_effect->Set_Texture("ShroudTexture", shroudTexture);
		float shroudParams[4] = { shroud.offsetX, shroud.offsetY, shroud.scaleX, shroud.scaleY };
		s_effect->Set_Float("ShroudParams", shroudParams, 4);
	}
	int technique = Sage2ShadowClass::Is_Available() ? s_techDefault : s_techNoShadow;

	device8->Set_Vertex_Declaration_Override(s_declaration);
	device->SetVertexDeclaration(s_declaration);
	device->SetStreamSource(0, vb, 0, VERTEX_STRIDE);
	device->SetIndices(ib);
	for (int pass = 0; pass < s_effect->Get_Pass_Count(technique); ++pass)
	{
		if (!s_effect->Begin(device, technique, pass))
			continue;
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, TRUE);	// linear light, as the terrain under it
		device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, (INT)baseVertex, 0, vertexCount, startIndex, triangleCount);
		s_effect->End(device);
	}
	device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
	device->SetStreamSource(0, nullptr, 0, 0);
	device8->Set_Vertex_Declaration_Override(nullptr);
	DX8Wrapper::Invalidate_Shader_Constants();
	ShaderClass::Invalidate();
	device8->Invalidate_State();
	if (!s_logged)
	{
		s_logged = true;
		Sage2_Log("water: drawn with tiberium_water %s", technique == s_techDefault ? "(shadowed)" : "(no shadow map)");
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
void Sage2WaterClass::End()
{
	s_begun = false;
}

//-------------------------------------------------------------------------------------------------
void Sage2WaterClass::Release_Resources()
{
	if (s_refraction) s_refraction->Release();
	s_refraction = nullptr;
	s_width = s_height = 0;
	s_begun = false;
}

//-------------------------------------------------------------------------------------------------
void Sage2WaterClass::Shutdown()
{
	Release_Resources();
	if (s_declaration) s_declaration->Release();
	s_declaration = nullptr;
	s_effect = nullptr;	// owned by the effect cache
	s_state = NOT_TRIED;
}
