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

// Tiberium Project @feature C&C4's deferred lighting, per mesh (see sage2scene.h)

#include "sage2scene.h"

#include <string.h>

#include "camera.h"
#include "dx8polygonrenderer.h"
#include "dx8renderer.h"
#include "dx8wrapper.h"
#include "mesh.h"
#include "sage2effect.h"
#include "sage2globals.h"
#include "sage2material.h"
#include "WWMath/aabox.h"

namespace
{
	bool s_enabled = true;
	enum AvailState { NOT_TRIED, AVAILABLE, UNAVAILABLE };
	AvailState s_available = NOT_TRIED;
	int s_srgbWrite = -1;

	unsigned s_width = 0, s_height = 0;
	IDirect3DTexture9* s_normalSpec = nullptr;		// bumped normal (rgb), specular power (a)
	IDirect3DTexture9* s_depth = nullptr;			// clip-space depth
	IDirect3DTexture9* s_depthLinear = nullptr;		// view-space depth
	IDirect3DTexture9* s_diffuseLight = nullptr;
	IDirect3DTexture9* s_specularLight = nullptr;
	IDirect3DSurface9* s_prepassDepth = nullptr;	// the pre-pass's own depth buffer
	IDirect3DVertexBuffer9* s_quad = nullptr;		// the rectangle the light passes draw
	IDirect3DVertexDeclaration9* s_quadDeclaration = nullptr;
	Sage2EffectClass* s_lighting = nullptr;
	Sage2EffectClass* s_linearDepth = nullptr;
	int s_lightingTechnique = -1, s_linearDepthTechnique = -1;
	bool s_clearedThisFrame = false;

	IDirect3DSurface9* Surface(IDirect3DTexture9* t)
	{
		IDirect3DSurface9* s = nullptr;
		if (t) t->GetSurfaceLevel(0, &s);
		return s;
	}

	IDirect3DTexture9* Make_Target(IDirect3DDevice9* device, unsigned w, unsigned h, D3DFORMAT format)
	{
		IDirect3DTexture9* t = nullptr;
		if (FAILED(device->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &t, nullptr)))
			return nullptr;
		return t;
	}

	bool Make_Resources(IDirect3DDevice9* device, unsigned w, unsigned h)
	{
		if (s_normalSpec && s_width == w && s_height == h)
			return true;
		Sage2SceneClass::Release_Resources();
		s_width = w;
		s_height = h;
		D3DFORMAT lightFormat = D3DFMT_A16B16G16R16F;
		IDirect3D9* d3d = DX8Wrapper::_Get_D3D8()->Get_D3D9();
		D3DDISPLAYMODE mode;
		if (FAILED(d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode)))
			mode.Format = D3DFMT_X8R8G8B8;
		if (FAILED(d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, lightFormat)))
			lightFormat = D3DFMT_A8R8G8B8;
		s_normalSpec = Make_Target(device, w, h, D3DFMT_A8R8G8B8);
		s_depth = Make_Target(device, w, h, D3DFMT_R32F);
		s_depthLinear = Make_Target(device, w, h, D3DFMT_R32F);
		s_diffuseLight = Make_Target(device, w, h, lightFormat);
		s_specularLight = Make_Target(device, w, h, lightFormat);
		if (FAILED(device->CreateDepthStencilSurface(w, h, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &s_prepassDepth, nullptr)))
			s_prepassDepth = nullptr;
		if (FAILED(device->CreateVertexBuffer(4 * 20, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &s_quad, nullptr)))
			s_quad = nullptr;
		if (s_quadDeclaration == nullptr)
		{
			D3DVERTEXELEMENT9 el[] =
			{
				{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
				{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
				D3DDECL_END()
			};
			if (FAILED(device->CreateVertexDeclaration(el, &s_quadDeclaration)))
				s_quadDeclaration = nullptr;
		}
		if (!s_normalSpec || !s_depth || !s_depthLinear || !s_diffuseLight || !s_specularLight || !s_prepassDepth || !s_quad || !s_quadDeclaration)
		{
			Sage2SceneClass::Release_Resources();
			return false;
		}
		return true;
	}

	bool Make_Effects()
	{
		if (s_lighting && s_linearDepth)
			return true;
		s_lighting = Sage2MaterialClass::Get_Effect("deferredlighting");
		s_linearDepth = Sage2MaterialClass::Get_Effect("postfx_lineardepth");
		if (s_lighting == nullptr || s_linearDepth == nullptr)
			return false;
		s_lightingTechnique = s_lighting->Find_Technique("Directional");
		s_linearDepthTechnique = s_linearDepth->Find_Technique("LinearDepth");
		return s_lightingTechnique >= 0 && s_linearDepthTechnique >= 0;
	}

	// the mesh's screen rectangle, in clip space, from its world bounding box
	void Screen_Rect(MeshClass* mesh, float& x0, float& y0, float& x1, float& y1)
	{
		x0 = y0 = -1.0f;
		x1 = y1 = 1.0f;
		CameraClass* camera = TheDX8MeshRenderer.Peek_Camera();
		if (camera == nullptr)
			return;
		const AABoxClass& box = mesh->Get_Bounding_Box();
		float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
		for (int c = 0; c < 8; ++c)
		{
			Vector3 corner(box.Center.X + ((c & 1) ? box.Extent.X : -box.Extent.X),
				box.Center.Y + ((c & 2) ? box.Extent.Y : -box.Extent.Y),
				box.Center.Z + ((c & 4) ? box.Extent.Z : -box.Extent.Z));
			Vector3 p;
			if (camera->Project(p, corner) == CameraClass::OUTSIDE_NEAR_CLIP)
				return;	// crosses the near plane: the whole screen
			if (p.X < minX) minX = p.X;
			if (p.X > maxX) maxX = p.X;
			if (p.Y < minY) minY = p.Y;
			if (p.Y > maxY) maxY = p.Y;
		}
		// a little slack for the pixel grid
		float sx = 2.0f / (float)s_width, sy = 2.0f / (float)s_height;
		x0 = minX - 2 * sx; x1 = maxX + 2 * sx;
		y0 = minY - 2 * sy; y1 = maxY + 2 * sy;
		if (x0 < -1.0f) x0 = -1.0f;
		if (y0 < -1.0f) y0 = -1.0f;
		if (x1 > 1.0f) x1 = 1.0f;
		if (y1 > 1.0f) y1 = 1.0f;
	}

	// draws the rectangle with the effect's technique (vPos-based effects take their screen
	// position from the pixel; the texcoord-based one gets matching coordinates)
	void Draw_Rect(IDirect3DDevice9* device, Sage2EffectClass& effect, int technique, float x0, float y0, float x1, float y1)
	{
		float hx = 1.0f / (float)s_width, hy = 1.0f / (float)s_height;	// half-pixel: texel centres on pixel centres
		float v[4][5] =
		{
			{ x0 - hx, y0 + hy, 0.0f, x0 * 0.5f + 0.5f, 0.5f - y0 * 0.5f },
			{ x1 - hx, y0 + hy, 0.0f, x1 * 0.5f + 0.5f, 0.5f - y0 * 0.5f },
			{ x0 - hx, y1 + hy, 0.0f, x0 * 0.5f + 0.5f, 0.5f - y1 * 0.5f },
			{ x1 - hx, y1 + hy, 0.0f, x1 * 0.5f + 0.5f, 0.5f - y1 * 0.5f },
		};
		void* p = nullptr;
		if (FAILED(s_quad->Lock(0, 0, &p, D3DLOCK_DISCARD)))
			return;
		memcpy(p, v, sizeof(v));
		s_quad->Unlock();
		device->SetVertexDeclaration(s_quadDeclaration);
		device->SetStreamSource(0, s_quad, 0, 20);
		for (int pass = 0; pass < effect.Get_Pass_Count(technique); ++pass)
		{
			if (!effect.Begin(device, technique, pass))
				continue;
			device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
			effect.End(device);
		}
	}
}

//-------------------------------------------------------------------------------------------------
bool Sage2SceneClass::Is_Available()
{
	if (s_available == NOT_TRIED)
	{
		s_available = UNAVAILABLE;
		IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
		if (device8 != nullptr)
		{
			D3DCAPS9 caps;
			if (SUCCEEDED(device8->Get_D3D9()->GetDeviceCaps(&caps)) && caps.NumSimultaneousRTs >= 2
				&& caps.PixelShaderVersion >= D3DPS_VERSION(3, 0) && Make_Effects())
				s_available = AVAILABLE;
		}
	}
	return s_available == AVAILABLE;
}

//-------------------------------------------------------------------------------------------------
void Sage2SceneClass::Set_Enabled(bool enabled) { s_enabled = enabled; }
bool Sage2SceneClass::Is_Enabled() { return s_enabled; }

//-------------------------------------------------------------------------------------------------
bool Sage2SceneClass::Supports_SRGB_Write()
{
	if (s_srgbWrite < 0)
	{
		s_srgbWrite = 0;
		IDirect3D9* d3d = DX8Wrapper::_Get_D3D8() ? DX8Wrapper::_Get_D3D8()->Get_D3D9() : nullptr;
		IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
		if (d3d && device8)
		{
			IDirect3DSurface9* back = nullptr;
			D3DSURFACE_DESC desc;
			if (SUCCEEDED(device8->Get_D3D9()->GetRenderTarget(0, &back)) && back)
			{
				if (SUCCEEDED(back->GetDesc(&desc)))
				{
					D3DDISPLAYMODE mode;
					if (FAILED(d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode)))
						mode.Format = D3DFMT_X8R8G8B8;
					if (SUCCEEDED(d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mode.Format,
						D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBWRITE, D3DRTYPE_SURFACE, desc.Format)))
						s_srgbWrite = 1;
				}
				back->Release();
			}
		}
	}
	return s_srgbWrite == 1;
}

//-------------------------------------------------------------------------------------------------
void Sage2SceneClass::Begin_Frame()
{
	s_clearedThisFrame = false;
	Sage2GlobalsClass::Set_Scene_Textures(nullptr, nullptr, nullptr, nullptr);
}

//-------------------------------------------------------------------------------------------------
bool Sage2SceneClass::Light_Mesh(Sage2EffectClass& effect, int prepassTechnique, MeshClass* mesh, DX8PolygonRendererClass* renderer,
	IDirect3DVertexBuffer9* meshBuffer, unsigned meshStride, IDirect3DVertexDeclaration9* meshDeclaration)
{
	if (!s_enabled || !Is_Available() || prepassTechnique < 0)
		return false;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DDevice9* device = device8->Get_D3D9();

	// the frame's targets, to size the buffers and to come back to
	IDirect3DSurface9* target = nullptr;
	IDirect3DSurface9* depth = nullptr;
	device->GetRenderTarget(0, &target);
	device->GetDepthStencilSurface(&depth);
	if (target == nullptr)
	{
		if (depth) depth->Release();
		return false;
	}
	D3DSURFACE_DESC desc;
	target->GetDesc(&desc);
	if (!Make_Resources(device, desc.Width, desc.Height))
	{
		target->Release();
		if (depth) depth->Release();
		return false;
	}
	Sage2GlobalsClass::Set_Frame_Buffer_Size(desc.Width, desc.Height);

	IDirect3DSurface9* sNormalSpec = Surface(s_normalSpec);
	IDirect3DSurface9* sDepth = Surface(s_depth);
	IDirect3DSurface9* sDepthLinear = Surface(s_depthLinear);
	IDirect3DSurface9* sDiffuse = Surface(s_diffuseLight);
	IDirect3DSurface9* sSpecular = Surface(s_specularLight);

	// 1. the mesh's depth / normal pre-pass
	device->SetRenderTarget(0, sNormalSpec);
	device->SetRenderTarget(1, sDepth);
	device->SetDepthStencilSurface(s_prepassDepth);
	if (!s_clearedThisFrame)
	{
		device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFFFFFFFF, 1.0f, 0);
		s_clearedThisFrame = true;
	}
	// the light buffers are not inputs here; keep the effect from binding them to a target it writes
	Sage2GlobalsClass::Set_Scene_Textures(nullptr, nullptr, nullptr, nullptr);
	Sage2GlobalsClass::Apply_Scene_Textures(effect);
	for (int pass = 0; pass < effect.Get_Pass_Count(prepassTechnique); ++pass)
	{
		if (!effect.Begin(device, prepassTechnique, pass))
			continue;
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		renderer->Render(mesh->Get_Base_Vertex_Offset());
		effect.End(device);
	}
	device->SetRenderTarget(1, nullptr);

	// 2. linear depth and the directional light over the mesh's rectangle
	float x0, y0, x1, y1;
	Screen_Rect(mesh, x0, y0, x1, y1);
	Sage2DrawState draw;
	Sage2GlobalsClass::Set_Scene_Textures(nullptr, nullptr, s_depthLinear, s_normalSpec);
	Sage2GlobalsClass::Set_Depth_Buffer_Texture(s_depth);
	Sage2GlobalsClass::Apply(*s_linearDepth, draw);
	Sage2GlobalsClass::Apply(*s_lighting, draw);
	device->SetDepthStencilSurface(nullptr);
	device->SetRenderTarget(0, sDepthLinear);
	Draw_Rect(device, *s_linearDepth, s_linearDepthTechnique, x0, y0, x1, y1);
	device->SetRenderTarget(0, sDiffuse);
	device->SetRenderTarget(1, sSpecular);
	Draw_Rect(device, *s_lighting, s_lightingTechnique, x0, y0, x1, y1);
	device->SetRenderTarget(1, nullptr);

	// 3. back to the frame, with the mesh's streams as the caller had them
	device->SetRenderTarget(0, target);
	device->SetDepthStencilSurface(depth);
	device->SetVertexDeclaration(meshDeclaration);
	device->SetStreamSource(0, meshBuffer, 0, meshStride);
	Sage2GlobalsClass::Set_Scene_Textures(s_diffuseLight, s_specularLight, s_depthLinear, s_normalSpec);
	Sage2GlobalsClass::Apply_Scene_Textures(effect);

	sNormalSpec->Release(); sDepth->Release(); sDepthLinear->Release(); sDiffuse->Release(); sSpecular->Release();
	target->Release();
	if (depth) depth->Release();
	return true;
}

//-------------------------------------------------------------------------------------------------
void Sage2SceneClass::Release_Resources()
{
	if (s_normalSpec) s_normalSpec->Release();
	if (s_depth) s_depth->Release();
	if (s_depthLinear) s_depthLinear->Release();
	if (s_diffuseLight) s_diffuseLight->Release();
	if (s_specularLight) s_specularLight->Release();
	if (s_prepassDepth) s_prepassDepth->Release();
	if (s_quad) s_quad->Release();
	s_normalSpec = s_depth = s_depthLinear = s_diffuseLight = s_specularLight = nullptr;
	s_prepassDepth = nullptr;
	s_quad = nullptr;
	s_width = s_height = 0;
	s_srgbWrite = -1;
	Sage2GlobalsClass::Set_Scene_Textures(nullptr, nullptr, nullptr, nullptr);
}

//-------------------------------------------------------------------------------------------------
void Sage2SceneClass::Shutdown()
{
	Release_Resources();
	if (s_quadDeclaration) s_quadDeclaration->Release();
	s_quadDeclaration = nullptr;
	s_lighting = s_linearDepth = nullptr;	// owned by the effect cache
	s_lightingTechnique = s_linearDepthTechnique = -1;
	s_available = NOT_TRIED;
}
