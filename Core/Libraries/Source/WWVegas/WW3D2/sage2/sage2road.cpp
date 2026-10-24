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

// Tiberium Project @feature Zero Hour's roads drawn the C&C4 way (see sage2road.h)

#include "sage2road.h"

#include "dx8wrapper.h"
#include "sage2effect.h"
#include "sage2globals.h"
#include "sage2material.h"
#include "sage2scene.h"
#include "sage2terrain.h"
#include "texture.h"
#include "textureloader.h"

namespace
{
	bool s_enabled = true;
	enum EffectState { NOT_TRIED, READY, UNAVAILABLE };
	struct KindState
	{
		const char* name;
		EffectState state;
		Sage2EffectClass* effect;
		int techDefault, techForward;
		bool logged;
	};
	KindState s_kinds[2] = { { "tiberium_road", NOT_TRIED, nullptr, -1, -1, false }, { "tiberium_tracks", NOT_TRIED, nullptr, -1, -1, false } };
	IDirect3DVertexDeclaration9* s_declaration = nullptr;
	const unsigned VERTEX_STRIDE = 24;	// XYZDUV1

	bool Make_Effect(IDirect3DDevice9* device, KindState& k)
	{
		if (k.state != NOT_TRIED)
			return k.state == READY;
		k.state = UNAVAILABLE;
		k.effect = Sage2MaterialClass::Get_Effect(k.name);
		if (k.effect == nullptr)
			return false;
		k.techDefault = k.effect->Find_Technique("Default");
		k.techForward = k.effect->Find_Technique("Default_M");
		if (k.techForward < 0)
			return false;
		if (s_declaration == nullptr)
		{
			D3DVERTEXELEMENT9 el[] =
			{
				{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
				{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
				{ 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
				D3DDECL_END()
			};
			if (FAILED(device->CreateVertexDeclaration(el, &s_declaration)))
				return false;
		}
		k.state = READY;
		Sage2_Log("%s: ready", k.name);
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
}

//-------------------------------------------------------------------------------------------------
void Sage2RoadClass::Set_Enabled(bool enabled) { s_enabled = enabled; }
bool Sage2RoadClass::Is_Enabled() { return s_enabled; }

//-------------------------------------------------------------------------------------------------
bool Sage2RoadClass::Draw(Kind kind, IDirect3DVertexBuffer9* vertices, IDirect3DIndexBuffer9* indices, TextureClass* texture,
	unsigned vertexCount, unsigned triangleCount, unsigned baseVertex)
{
	if (!s_enabled || !Sage2TerrainClass::Drew_This_Frame() || vertices == nullptr || indices == nullptr || triangleCount == 0)
		return false;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	if (device8 == nullptr)
		return false;
	IDirect3DDevice9* device = device8->Get_D3D9();
	KindState& k = s_kinds[kind == TRACKS ? 1 : 0];
	if (!Make_Effect(device, k))
		return false;
	Sage2EffectClass* s_effect = k.effect;

	DX8Wrapper::Apply_Render_State_Changes();
	Sage2DrawState drawState;
	Sage2GlobalsClass::Apply(*s_effect, drawState);
	s_effect->Set_Texture("BaseSampler", D3D_Texture(texture));
	const Sage2TerrainClass::Shroud& shroud = Sage2TerrainClass::Get_Shroud();
	IDirect3DBaseTexture9* shroudTexture = D3D_Texture(shroud.texture);
	if (shroudTexture != nullptr)
	{
		s_effect->Set_Texture("ShroudTexture", shroudTexture);
		float shroudParams[4] = { shroud.offsetX, shroud.offsetY, shroud.scaleX, shroud.scaleY };
		s_effect->Set_Float("ShroudParams", shroudParams, 4);
	}
	bool deferred = k.techDefault >= 0 && Sage2TerrainClass::Drew_Deferred() && Sage2GlobalsClass::Is_Deferred_Lighting_Available();
	int technique = deferred ? k.techDefault : k.techForward;

	device8->Set_Vertex_Declaration_Override(s_declaration);
	device->SetVertexDeclaration(s_declaration);
	device->SetStreamSource(0, vertices, 0, VERTEX_STRIDE);
	device->SetIndices(indices);
	for (int pass = 0; pass < s_effect->Get_Pass_Count(technique); ++pass)
	{
		if (!s_effect->Begin(device, technique, pass))
			continue;
		// both techniques compute in linear light (the forward one too): written sRGB when the
		// frame buffer can take it
		if (Sage2SceneClass::Supports_SRGB_Write())
			device->SetRenderState(D3DRS_SRGBWRITEENABLE, TRUE);
		device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, (INT)baseVertex, 0, vertexCount, 0, triangleCount);
		s_effect->End(device);
	}
	device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
	device->SetStreamSource(0, nullptr, 0, 0);
	device8->Set_Vertex_Declaration_Override(nullptr);
	DX8Wrapper::Set_Vertex_Buffer((VertexBufferClass*)nullptr);
	DX8Wrapper::Set_Index_Buffer((IndexBufferClass*)nullptr, 0);
	DX8Wrapper::Invalidate_Shader_Constants();
	ShaderClass::Invalidate();

	if (!k.logged)
	{
		k.logged = true;
		Sage2_Log("%s: drawn with %s", k.name, deferred ? "Default (deferred)" : "Default_M (forward)");
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
void Sage2RoadClass::Shutdown()
{
	if (s_declaration) s_declaration->Release();
	s_declaration = nullptr;
	for (int i = 0; i < 2; ++i)
	{
		s_kinds[i].effect = nullptr;	// owned by the effect cache
		s_kinds[i].state = NOT_TRIED;
		s_kinds[i].logged = false;
	}
}
