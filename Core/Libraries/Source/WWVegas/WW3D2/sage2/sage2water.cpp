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
	bool s_loggedRiver = false;
	float s_depthScale = 2.5f;	// Zero Hour's rivers and lakes are a third as deep as RA3's shores
	float s_tint = -1.0f;
	float s_caustics = 0.6f;
	float s_skyStrength = 1.0f;
	TextureClass* s_sky = nullptr;	// the water renderer's, for this frame
	float s_skyParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	float s_skyColor[4] = { 1.0f, 1.0f, 1.0f, 400.0f };

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

void Sage2WaterClass::Set_Sky_Strength(float strength)
{
	s_skyStrength = strength < 0.0f ? 0.0f : (strength > 2.0f ? 2.0f : strength);
}

void Sage2WaterClass::Set_Sky(TextureClass* sky, float texelsPerUnit, float uScrollPerSecond, float vScrollPerSecond, const float color[3])
{
	s_sky = sky;
	s_skyParams[1] = texelsPerUnit;
	s_skyParams[2] = uScrollPerSecond * texelsPerUnit;
	s_skyParams[3] = vScrollPerSecond * texelsPerUnit;
	s_skyColor[0] = color[0];
	s_skyColor[1] = color[1];
	s_skyColor[2] = color[2];
}

void Sage2WaterClass::Set_Look(float depthScale, float tint, float caustics)
{
	s_depthScale = depthScale < 0.1f ? 0.1f : (depthScale > 10.0f ? 10.0f : depthScale);
	s_tint = tint > 1.0f ? 1.0f : tint;
	s_caustics = caustics < 0.0f ? 0.0f : (caustics > 4.0f ? 4.0f : caustics);
}

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
bool Sage2WaterClass::Draw(unsigned vertexCount, unsigned triangleCount, TextureClass* waterTexture, float transparentDepth, float minOpacity,
	const River* river)
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

	Sage2SceneLightsScope sceneLights;	// the map's lights, not the last object's
	Sage2DrawState drawState;
	Sage2GlobalsClass::Apply(*s_effect, drawState);
	s_effect->Set_Texture("RefractionTexture", s_refraction);
	s_effect->Set_Texture("NormalTexture", Sage2TextureClass::Get("fxrivera_nrm"));
	s_effect->Set_Texture("FoamTexture", Sage2TextureClass::Get("oceanfoam"));
	s_effect->Set_Texture("WaterTexture", D3D_Texture(waterTexture));
	float transparency[4] = { transparentDepth, minOpacity < 0.0f ? 0.0f : (minOpacity > 1.0f ? 1.0f : minOpacity), 0.0f, 0.0f };
	s_effect->Set_Float("WaterTransparency", transparency, 4);
	IDirect3DBaseTexture9* ocean = Sage2TextureClass::Get("fxoceancubemap");
	if (ocean != nullptr)
		s_effect->Set_Texture("EnvironmentTexture", ocean);
	// RA3's and C&C4's underwater: the depth table and the caustics (the absorption without them)
	IDirect3DBaseTexture9* depthLut = Sage2TextureClass::Get("fxdepthlut");
	IDirect3DBaseTexture9* caustics = Sage2TextureClass::Get("fxcaustics");
	if (depthLut != nullptr)
		s_effect->Set_Texture("DepthLutTexture", depthLut);
	if (caustics != nullptr)
		s_effect->Set_Texture("CausticsTexture", caustics);
	float lutParams[4] = { depthLut != nullptr ? 1.0f : 0.0f, 1.0f / 200.0f, caustics != nullptr ? s_caustics : 0.0f, s_depthScale };
	s_effect->Set_Float("WaterDepthLut", lutParams, 4);
	// how much the water's own colour tints it: C&C4's 0.4 over its PuddleColor; with the table
	// (which carries the water's colour, as RA3's ocean has none of its own) a touch of the map's
	float tint = s_tint >= 0.0f ? s_tint : (depthLut != nullptr ? 0.15f : 0.4f);
	float look[4] = { tint, 2.0f, 0.2f, 0.0f };
	s_effect->Set_Float("WaterLook", look, 4);
	IDirect3DBaseTexture9* sky = D3D_Texture(s_sky);
	if (sky != nullptr)
		s_effect->Set_Texture("SkyTexture", sky);
	float skyParams[4] = { sky != nullptr ? s_skyStrength : 0.0f, s_skyParams[1], s_skyParams[2], s_skyParams[3] };
	s_effect->Set_Float("WaterSky", skyParams, 4);
	s_effect->Set_Float("WaterSkyColor", s_skyColor, 4);
	IDirect3DBaseTexture9* edge = river != nullptr ? D3D_Texture(river->edgeTexture) : nullptr;
	if (edge != nullptr)
		s_effect->Set_Texture("EdgeTexture", edge);
	float riverParams[4] = { river != nullptr ? 1.0f : 0.0f, river != nullptr ? river->worldPerU : 0.0f,
		river != nullptr ? river->worldPerV : 0.0f, edge != nullptr ? 1.0f : 0.0f };
	s_effect->Set_Float("WaterRiver", riverParams, 4);
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
		Sage2_Log("water: drawn with tiberium_water %s, %s", technique == s_techDefault ? "(shadowed)" : "(no shadow map)",
			depthLut != nullptr ? (caustics != nullptr ? "depth table and caustics" : "depth table, no caustics") : "no depth table (absorption)");
		Sage2_Log("water: the sky reflected at %.2f (%s), colour (%.2f %.2f %.2f), %.4f repeats a unit", skyParams[0],
			sky != nullptr ? "sky texture" : "no sky texture", s_skyColor[0], s_skyColor[1], s_skyColor[2], s_skyParams[1]);
	}
	if (river != nullptr && !s_loggedRiver)
	{
		s_loggedRiver = true;
		Sage2_Log("water: rivers drawn with tiberium_water (%.0f x %.0f world units a texture unit, %s)", river->worldPerU, river->worldPerV,
			edge != nullptr ? "edge texture" : "no edge texture");
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
	s_sky = nullptr;
	if (s_declaration) s_declaration->Release();
	s_declaration = nullptr;
	s_effect = nullptr;	// owned by the effect cache
	s_state = NOT_TRIED;
}
