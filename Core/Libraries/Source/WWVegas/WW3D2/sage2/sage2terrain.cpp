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

// Tiberium Project @feature The terrain drawn the C&C4 way (see sage2terrain.h)

#include "sage2terrain.h"

#include <string.h>
#include <vector>

#include "dx8indexbuffer.h"
#include "dx8vertexbuffer.h"
#include "dx8wrapper.h"
#include "sage2effect.h"
#include "sage2globals.h"
#include "sage2material.h"
#include "sage2scene.h"
#include "sage2terrainatlas.h"
#include "shader.h"
#include "texture.h"
#include "textureloader.h"

namespace
{
	enum InitState { NOT_TRIED, READY, UNAVAILABLE };

	bool s_enabled = true;
	float s_albedoScale = 1.6f;
	float s_tileWorldSize = 80.0f;		// a C&C4 texture spans eight cells
	float s_bumpSign = 1.0f;			// -1 reads the normal maps' green channel flipped
	float s_bumpScale = 0.75f;			// C&C4's terrain, High
	InitState s_state = NOT_TRIED;
	Sage2EffectClass* s_effect = nullptr;
	int s_techDefault = -1, s_techForward = -1, s_techPrepass = -1;

	const unsigned TERRAIN_STRIDE = 32;		// x y z, diffuse, u1 v1, u2 v2
	const unsigned NORMAL_STREAM = 1;
	const unsigned NORMAL_STRIDE = Sage2TerrainClass::SIDE_FLOATS * 4;	// normal, base slot, blend slot

	IDirect3DVertexDeclaration9* s_declaration = nullptr;
	IDirect3DVertexBuffer9* s_normals = nullptr;		// every tile's normals, one after the other
	unsigned s_normalsVertexCount = 0;					// per tile
	std::vector<unsigned> s_tileVersions;				// what the stream holds, per tile
	std::vector<unsigned> s_tileVertexBuffer;			// which tile the stream slot was last filled from (its vertex buffer)

	template <class T> void Release(T*& p) { if (p) { p->Release(); p = nullptr; } }

	bool Make_Effect()
	{
		if (s_state != NOT_TRIED)
			return s_state == READY;
		s_state = UNAVAILABLE;
		D3DCAPS9 caps;
		IDirect3DDevice9* device = DX8Wrapper::_Get_D3D_Device8()->Get_D3D9();
		if (FAILED(device->GetDeviceCaps(&caps)) || caps.PixelShaderVersion < D3DPS_VERSION(3, 0))
			return false;
		s_effect = Sage2MaterialClass::Get_Effect("tiberium_terrain");
		if (s_effect == nullptr)
			return false;
		s_techDefault = s_effect->Find_Technique("Default");
		s_techForward = s_effect->Find_Technique("Default_M");
		s_techPrepass = s_effect->Find_Technique("_CreateDepthNormalMap");
		if (s_techForward < 0)
			return false;
		D3DVERTEXELEMENT9 el[] =
		{
			{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
			{ 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
			{ (WORD)NORMAL_STREAM, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
			{ (WORD)NORMAL_STREAM, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },
			D3DDECL_END()
		};
		if (FAILED(device->CreateVertexDeclaration(el, &s_declaration)))
			return false;
		s_state = READY;
		Sage2_Log("terrain: C&C4-style terrain ready (deferred %s)", s_techPrepass >= 0 && s_techDefault >= 0 ? "yes" : "no");
		return true;
	}

	// the normal stream, with every tile whose vertices changed re-uploaded
	bool Update_Normals(IDirect3DDevice9* device, const Sage2TerrainClass::Tile* tiles, int tileCount, unsigned vertexCount)
	{
		if (s_normals != nullptr && (s_normalsVertexCount != vertexCount || (int)s_tileVersions.size() != tileCount))
			Sage2TerrainClass::Release_Resources();
		if (s_normals == nullptr)
		{
			if (FAILED(device->CreateVertexBuffer(tileCount * vertexCount * NORMAL_STRIDE, D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &s_normals, nullptr)))
				return false;
			s_normalsVertexCount = vertexCount;
			s_tileVersions.assign(tileCount, ~0u);
			s_tileVertexBuffer.assign(tileCount, 0);
		}
		for (int t = 0; t < tileCount; ++t)
		{
			unsigned id = (unsigned)(size_t)tiles[t].vertexBuffer;
			if (s_tileVersions[t] == tiles[t].version && s_tileVertexBuffer[t] == id)
				continue;
			void* p = nullptr;
			if (FAILED(s_normals->Lock(t * vertexCount * NORMAL_STRIDE, vertexCount * NORMAL_STRIDE, &p, 0)))
				return false;
			memcpy(p, tiles[t].normals, vertexCount * NORMAL_STRIDE);
			s_normals->Unlock();
			s_tileVersions[t] = tiles[t].version;
			s_tileVertexBuffer[t] = id;
		}
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

	// draws every tile with the current pass
	void Draw_Tiles(IDirect3DDevice9* device, IDirect3DIndexBuffer9* indices, const Sage2TerrainClass::Tile* tiles, int tileCount,
		unsigned vertexCount, unsigned polygonCount)
	{
		device->SetIndices(indices);
		for (int t = 0; t < tileCount; ++t)
		{
			device->SetStreamSource(0, tiles[t].vertexBuffer->Get_DX8_Vertex_Buffer(), 0, TERRAIN_STRIDE);
			device->SetStreamSource(NORMAL_STREAM, s_normals, t * vertexCount * NORMAL_STRIDE, NORMAL_STRIDE);
			device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, vertexCount, 0, polygonCount);
		}
	}

	class TerrainPrepass : public Sage2SceneClass::PrepassDrawClass
	{
	public:
		TerrainPrepass(IDirect3DIndexBuffer9* ib, const Sage2TerrainClass::Tile* t, int n, unsigned v, unsigned p)
			: indices(ib), tiles(t), tileCount(n), vertexCount(v), polygonCount(p) {}
		virtual void Draw(IDirect3DDevice9* device)
		{
			device->SetVertexDeclaration(s_declaration);
			Draw_Tiles(device, indices, tiles, tileCount, vertexCount, polygonCount);
		}
		IDirect3DIndexBuffer9* indices;
		const Sage2TerrainClass::Tile* tiles;
		int tileCount;
		unsigned vertexCount, polygonCount;
	};
}

//-------------------------------------------------------------------------------------------------
void Sage2TerrainClass::Set_Enabled(bool enabled) { s_enabled = enabled; }
bool Sage2TerrainClass::Is_Enabled() { return s_enabled; }
void Sage2TerrainClass::Set_Albedo_Scale(float scale) { s_albedoScale = scale > 0.0f ? scale : 1.0f; }
void Sage2TerrainClass::Set_Tile_World_Size(float size) { s_tileWorldSize = size > 1.0f ? size : 80.0f; }
void Sage2TerrainClass::Set_Bump(bool flipY, float scale) { s_bumpSign = flipY ? -1.0f : 1.0f; s_bumpScale = scale >= 0.0f ? scale : 0.75f; }

//-------------------------------------------------------------------------------------------------
bool Sage2TerrainClass::Draw(TextureClass* atlas, DX8IndexBufferClass* indexBuffer, const Tile* tiles, int tileCount,
	unsigned vertexCount, unsigned polygonCount, const Shroud& shroud)
{
	if (!s_enabled || tileCount <= 0 || atlas == nullptr || indexBuffer == nullptr)
		return false;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	if (device8 == nullptr || !Make_Effect())
		return false;
	IDirect3DDevice9* device = device8->Get_D3D9();
	IDirect3DBaseTexture9* atlasTexture = D3D_Texture(atlas);
	if (atlasTexture == nullptr || !Update_Normals(device, tiles, tileCount, vertexCount))
		return false;
	IDirect3DIndexBuffer9* indices = indexBuffer->Get_DX8_Index_Buffer();

	// the wrapper's pending state first, then the effect's own
	DX8Wrapper::Apply_Render_State_Changes();
	Sage2DrawState drawState;
	Sage2GlobalsClass::Apply(*s_effect, drawState);
	s_effect->Set_Texture("BaseSampler", atlasTexture);
	s_effect->Set_Float1("AlbedoScale", s_albedoScale);
	// the C&C4 textures of this map, world-aligned by slot; none: every slot is -1 and the tiles show
	IDirect3DBaseTexture9* c4Color = Sage2TerrainAtlasClass::Get_Color();
	IDirect3DBaseTexture9* c4Normal = Sage2TerrainAtlasClass::Get_Normal();
	s_effect->Set_Texture("AtlasColor", c4Color);
	s_effect->Set_Texture("AtlasNormal", c4Normal);
	float atlasParams[4] = { (float)Sage2TerrainAtlasClass::Get_Slots_Per_Row(), Sage2TerrainAtlasClass::Get_Slot_Size(),
		Sage2TerrainAtlasClass::Get_Slot_Border(), s_tileWorldSize };
	s_effect->Set_Float("AtlasParams", atlasParams, 4);
	float bumpParams[2] = { s_bumpSign, s_bumpScale };
	s_effect->Set_Float("BumpParams", bumpParams, 2);
	// the shroud, over the white stand-in the globals bound
	IDirect3DBaseTexture9* shroudTexture = D3D_Texture(shroud.texture);
	if (shroudTexture != nullptr)
	{
		s_effect->Set_Texture("ShroudTexture", shroudTexture);
		float shroudParams[4] = { shroud.offsetX, shroud.offsetY, shroud.scaleX, shroud.scaleY };
		s_effect->Set_Float("ShroudParams", shroudParams, 4);
	}
	device8->Set_Vertex_Declaration_Override(s_declaration);

	// the deferred chain over the whole terrain, then the tiles lit from the light buffers
	TerrainPrepass prepass(indices, tiles, tileCount, vertexCount, polygonCount);
	bool deferred = s_techDefault >= 0 && Sage2SceneClass::Supports_SRGB_Write()
		&& Sage2SceneClass::Light_Batch(*s_effect, s_techPrepass, prepass, -1.0f, -1.0f, 1.0f, 1.0f);
	int technique = deferred ? s_techDefault : s_techForward;
	device8->Set_Vertex_Declaration_Override(s_declaration);
	device->SetVertexDeclaration(s_declaration);
	for (int pass = 0; pass < s_effect->Get_Pass_Count(technique); ++pass)
	{
		if (!s_effect->Begin(device, technique, pass))
			continue;
		if (deferred)
			device->SetRenderState(D3DRS_SRGBWRITEENABLE, TRUE);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);	// the tiles' winding is the renderer's business
		Draw_Tiles(device, indices, tiles, tileCount, vertexCount, polygonCount);
		s_effect->End(device);
	}
	device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
	device->SetStreamSource(NORMAL_STREAM, nullptr, 0, 0);
	device->SetStreamSource(0, nullptr, 0, 0);
	device8->Set_Vertex_Declaration_Override(nullptr);
	// the wrapper's caches know nothing of what the effect set
	DX8Wrapper::Set_Vertex_Buffer((VertexBufferClass*)nullptr);
	DX8Wrapper::Invalidate_Shader_Constants();
	ShaderClass::Invalidate();

	static bool logged = false;
	if (!logged)
	{
		logged = true;
		Sage2_Log("terrain: %d tiles drawn with tiberium_terrain %s", tileCount, deferred ? "Default (deferred, sRGB)" : "Default_M (forward)");
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
void Sage2TerrainClass::Release_Resources()
{
	Release(s_normals);
	s_normalsVertexCount = 0;
	s_tileVersions.clear();
	s_tileVertexBuffer.clear();
}

//-------------------------------------------------------------------------------------------------
void Sage2TerrainClass::Shutdown()
{
	Release_Resources();
	Release(s_declaration);
	s_effect = nullptr;	// owned by the effect cache
	s_state = NOT_TRIED;
}
