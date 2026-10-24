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

// Tiberium Project @feature C&C4's terrain top view (see sage2topview.h)

#include "sage2topview.h"

#include "dx8wrapper.h"
#include "sage2effect.h"
#include "sage2globals.h"
#include "sage2material.h"
#include "texture.h"
#include "textureloader.h"

namespace
{
	bool s_enabled = true;
	unsigned s_size = 2048;
	enum EffectState { NOT_TRIED, READY, UNAVAILABLE };
	EffectState s_state = NOT_TRIED;
	Sage2EffectClass* s_effect = nullptr;
	int s_technique = -1;
	IDirect3DVertexDeclaration9* s_declaration = nullptr;

	IDirect3DTexture9* s_texture = nullptr;
	unsigned s_madeSize = 0;
	bool s_dirty = true;
	bool s_drawn = false;
	bool s_rendering = false;
	float s_offsetX = 0.0f, s_offsetY = 0.0f, s_scaleX = 0.0f, s_scaleY = 0.0f;

	// what Begin() put aside
	IDirect3DSurface9* s_savedTarget = nullptr;
	IDirect3DSurface9* s_savedDepth = nullptr;
	D3DVIEWPORT9 s_savedViewport;
	DWORD s_savedColorWrite = 0xF;

	const unsigned VERTEX_STRIDE = 24;	// XYZDUV1: position, diffuse, one texture coordinate

	template <class T> void Release(T*& p) { if (p) p->Release(); p = nullptr; }

	bool Make_Effect(IDirect3DDevice9* device)
	{
		if (s_state != NOT_TRIED)
			return s_state == READY;
		s_state = UNAVAILABLE;
		s_effect = Sage2MaterialClass::Get_Effect("tiberium_topview");
		if (s_effect == nullptr)
			return false;
		s_technique = s_effect->Find_Technique("Default");
		if (s_technique < 0)
			return false;
		D3DVERTEXELEMENT9 el[] =
		{
			{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
			{ 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			D3DDECL_END()
		};
		if (FAILED(device->CreateVertexDeclaration(el, &s_declaration)))
			return false;
		s_state = READY;
		return true;
	}

	bool Make_Texture(IDirect3DDevice9* device)
	{
		if (s_texture != nullptr && s_madeSize == s_size)
			return true;
		Sage2TopViewClass::Release_Resources();
		// half floats: the marks are dark, premultiplied, in linear light; 8 bits would band them
		if (FAILED(device->CreateTexture(s_size, s_size, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &s_texture, nullptr)))
		{
			Sage2_Log("top view: could not make the %ux%u texture", s_size, s_size);
			return false;
		}
		s_madeSize = s_size;
		Sage2_Log("top view: %ux%u texture for the scorch marks", s_size, s_size);
		return true;
	}
}

//-------------------------------------------------------------------------------------------------
void Sage2TopViewClass::Set_Enabled(bool enabled) { s_enabled = enabled; }
bool Sage2TopViewClass::Is_Enabled() { return s_enabled; }
void Sage2TopViewClass::Set_Size(unsigned size)
{
	unsigned s = 512;
	while (s < size && s < 4096)
		s *= 2;
	s_size = s;
}
void Sage2TopViewClass::Invalidate() { s_dirty = true; }
bool Sage2TopViewClass::Needs_Update() { return s_enabled && s_dirty && !s_rendering && s_state != UNAVAILABLE; }
bool Sage2TopViewClass::Is_Rendering() { return s_rendering; }
bool Sage2TopViewClass::Is_Available() { return s_enabled && s_drawn && s_texture != nullptr && !s_rendering; }
IDirect3DBaseTexture9* Sage2TopViewClass::Get_Texture() { return s_texture; }
void Sage2TopViewClass::Get_Mapping(float& offsetX, float& offsetY, float& scaleX, float& scaleY)
{
	offsetX = s_offsetX; offsetY = s_offsetY; scaleX = s_scaleX; scaleY = s_scaleY;
}

//-------------------------------------------------------------------------------------------------
bool Sage2TopViewClass::Begin(float minX, float minY, float maxX, float maxY)
{
	if (!s_enabled || s_rendering || maxX <= minX || maxY <= minY)
		return false;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	if (device8 == nullptr)
		return false;
	IDirect3DDevice9* device = device8->Get_D3D9();
	if (!Make_Effect(device) || !Make_Texture(device))
		return false;
	IDirect3DSurface9* surface = nullptr;
	if (FAILED(s_texture->GetSurfaceLevel(0, &surface)))
		return false;

	s_offsetX = minX; s_offsetY = minY;
	s_scaleX = 1.0f / (maxX - minX); s_scaleY = 1.0f / (maxY - minY);

	device->GetRenderTarget(0, &s_savedTarget);
	device->GetDepthStencilSurface(&s_savedDepth);
	device->GetViewport(&s_savedViewport);
	DX8Wrapper::Apply_Render_State_Changes();
	device->SetRenderTarget(0, surface);
	device->SetDepthStencilSurface(nullptr);
	surface->Release();
	D3DVIEWPORT9 viewport = { 0, 0, s_madeSize, s_madeSize, 0.0f, 1.0f };
	device->SetViewport(&viewport);
	// Tiberium Project @bugfix the marks' coverage is the alpha channel: Zero Hour leaves colour
	// writes at red, green and blue after its alpha-mask passes, which kept every mark out
	device->GetRenderState(D3DRS_COLORWRITEENABLE, &s_savedColorWrite);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);

	// world x,y -> clip: x to -1..1 across the map, y likewise (clip y up = texture v 0 at the
	// top, which is what the ground shaders' flipped v expects), plus half a texel for Direct3D
	// 9's pixel centres; the effect takes the rows as registers
	float halfTexel = 1.0f / (float)s_madeSize;	// in clip units (2 / size / 2)
	float m[16] =
	{
		2.0f * s_scaleX, 0.0f, 0.0f, -1.0f - 2.0f * s_scaleX * minX + halfTexel,
		0.0f, 2.0f * s_scaleY, 0.0f, -1.0f - 2.0f * s_scaleY * minY - halfTexel,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	s_effect->Set_Matrix("TopViewTransform", m);
	s_rendering = true;
	return true;
}

//-------------------------------------------------------------------------------------------------
void Sage2TopViewClass::Draw(IDirect3DVertexBuffer9* vertices, IDirect3DIndexBuffer9* indices, TextureClass* marks,
	unsigned vertexCount, unsigned triangleCount)
{
	if (!s_rendering || vertices == nullptr || indices == nullptr || triangleCount == 0)
		return;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DDevice9* device = device8->Get_D3D9();
	IDirect3DBaseTexture9* atlas = nullptr;
	if (marks != nullptr)
	{
		if (!marks->Is_Initialized())
			TextureLoader::Request_Foreground_Loading(marks);
		atlas = marks->Peek_D3D_Base_Texture();
	}
	s_effect->Set_Texture("MarkTexture", atlas);
	device8->Set_Vertex_Declaration_Override(s_declaration);
	device->SetVertexDeclaration(s_declaration);
	device->SetStreamSource(0, vertices, 0, VERTEX_STRIDE);
	device->SetIndices(indices);
	for (int pass = 0; pass < s_effect->Get_Pass_Count(s_technique); ++pass)
	{
		if (!s_effect->Begin(device, s_technique, pass))
			continue;
		device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, vertexCount, 0, triangleCount);
		s_effect->End(device);
	}
	device->SetStreamSource(0, nullptr, 0, 0);
	device8->Set_Vertex_Declaration_Override(nullptr);
	static int logged = 0;
	if (logged < 3)
	{
		++logged;
		Sage2_Log("top view: %u mark triangles painted (%u vertices), mark texture %s", triangleCount, vertexCount, atlas != nullptr ? "ready" : "missing");
	}
	DX8Wrapper::Set_Vertex_Buffer((VertexBufferClass*)nullptr);
	DX8Wrapper::Set_Index_Buffer((IndexBufferClass*)nullptr, 0);
	DX8Wrapper::Invalidate_Shader_Constants();
	ShaderClass::Invalidate();
}

//-------------------------------------------------------------------------------------------------
void Sage2TopViewClass::End()
{
	if (!s_rendering)
		return;
	IDirect3DDevice9* device = DX8Wrapper::_Get_D3D_Device8()->Get_D3D9();
	device->SetRenderTarget(0, s_savedTarget);
	device->SetDepthStencilSurface(s_savedDepth);
	device->SetViewport(&s_savedViewport);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, s_savedColorWrite);
	Release(s_savedTarget);
	Release(s_savedDepth);
	s_rendering = false;
	s_dirty = false;
	s_drawn = true;
}

//-------------------------------------------------------------------------------------------------
void Sage2TopViewClass::Release_Resources()
{
	Release(s_texture);
	s_madeSize = 0;
	s_drawn = false;
	s_dirty = true;
	s_rendering = false;
}

//-------------------------------------------------------------------------------------------------
void Sage2TopViewClass::Shutdown()
{
	Release_Resources();
	Release(s_declaration);
	s_effect = nullptr;	// owned by the effect cache
	s_state = NOT_TRIED;
}
