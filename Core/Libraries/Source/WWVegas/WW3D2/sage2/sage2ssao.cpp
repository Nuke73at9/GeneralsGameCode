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

// Tiberium Project @feature Screen-space ambient occlusion (see sage2ssao.h)

#include "sage2ssao.h"

#include "dx8caps.h"
#include "dx8wrapper.h"
#include "sage2globals.h"

#include "../shaders/ssao_vs.h"	// from shaders/ssao.hlsl (shaders/build_shaders.py)
#include "../shaders/ssao_ao_ps.h"
#include "../shaders/ssao_blur_ps.h"
#include "../shaders/ssao_apply_ps.h"
#include "../shaders/ssao_depth_ps.h"
#include "ww3d.h"

namespace
{
	enum InitState { NOT_TRIED, READY, UNAVAILABLE };

	bool s_enabled = false;
	float s_radius = 12.0f;			// world units
	float s_intensity = 1.0f;
	float s_strength = 0.8f;
	bool s_debug = false;			// the occlusion shown alone, in grey
	const float FADE_START = 900.0f;	// view depth where the occlusion starts to fade, and over how far
	const float FADE_LENGTH = 900.0f;
	const float BLUR_DEPTH_TOLERANCE = 0.04f;	// of the centre's depth

	InitState s_state = NOT_TRIED;
	IDirect3DVertexShader9* s_vertexShader = nullptr;
	IDirect3DPixelShader9* s_aoShader = nullptr;
	IDirect3DPixelShader9* s_blurShader = nullptr;
	IDirect3DPixelShader9* s_applyShader = nullptr;
	IDirect3DPixelShader9* s_depthShader = nullptr;	// the scene's view distance, for the soft particles
	IDirect3DVertexDeclaration9* s_declaration = nullptr;
	IDirect3DStateBlock9* s_savedState = nullptr;
	IDirect3DTexture9* s_aoA = nullptr;		// half size
	IDirect3DTexture9* s_aoB = nullptr;
	IDirect3DTexture9* s_sceneDepth = nullptr;	// R32F, the frame's size (none if the device has no R32F target)
	unsigned int s_sceneDepthFrame = 0xFFFFFFFF;	// the WW3D frame it was written in
	UINT s_width = 0, s_height = 0;		// the frame's
	bool s_loggedOn = false, s_loggedNoDepth = false;

	struct QuadVertex { float x, y, u, v; };

	template <class T> void Release(T*& p) { if (p) { p->Release(); p = nullptr; } }

	bool Init(IDirect3DDevice9* device)
	{
		s_state = UNAVAILABLE;
		if (DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Major_Version() < 3
			|| DX8Wrapper::Get_Current_Caps()->Get_Vertex_Shader_Major_Version() < 3)
			return false;
		const D3DVERTEXELEMENT9 elements[] =
		{
			{ 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			D3DDECL_END()
		};
		if (FAILED(device->CreateVertexShader(SSAOVertexShader, &s_vertexShader))
			|| FAILED(device->CreatePixelShader(SSAOPixelShader, &s_aoShader))
			|| FAILED(device->CreatePixelShader(SSAOBlurPixelShader, &s_blurShader))
			|| FAILED(device->CreatePixelShader(SSAOApplyPixelShader, &s_applyShader))
			|| FAILED(device->CreatePixelShader(SSAODepthPixelShader, &s_depthShader))
			|| FAILED(device->CreateVertexDeclaration(elements, &s_declaration)))
		{
			Sage2SSAOClass::Shutdown();
			s_state = UNAVAILABLE;
			Sage2_Log("ssao: the shaders could not be made; off");
			return false;
		}
		s_state = READY;
		return true;
	}

	bool Make_Targets(IDirect3DDevice9* device, UINT width, UINT height)
	{
		if (s_aoA != nullptr && width == s_width && height == s_height)
			return true;
		Release(s_aoA);
		Release(s_aoB);
		Release(s_sceneDepth);
		const UINT hw = width / 2 > 0 ? width / 2 : 1, hh = height / 2 > 0 ? height / 2 : 1;
		if (FAILED(device->CreateTexture(hw, hh, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &s_aoA, nullptr))
			|| FAILED(device->CreateTexture(hw, hh, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &s_aoB, nullptr)))
		{
			Release(s_aoA);
			Release(s_aoB);
			return false;
		}
		// the scene's depth for the soft particles; without an R32F target they keep the deferred
		// pre-pass's (the ground and the deferred meshes only)
		if (FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &s_sceneDepth, nullptr)))
			s_sceneDepth = nullptr;
		s_width = width;
		s_height = height;
		return true;
	}

	/// a quad over the whole of the bound viewport, its texture coordinates u0..u1, v0..v1
	void Draw_Quad(IDirect3DDevice9* device, UINT viewportWidth, UINT viewportHeight, float u0, float v0, float u1, float v1)
	{
		const float halfPixel[4] = { -1.0f / (float)viewportWidth, 1.0f / (float)viewportHeight, 0.0f, 0.0f };
		device->SetVertexShaderConstantF(0, halfPixel, 1);
		const QuadVertex quad[4] =
		{
			{ -1.0f,  1.0f, u0, v0 },
			{  1.0f,  1.0f, u1, v0 },
			{ -1.0f, -1.0f, u0, v1 },
			{  1.0f, -1.0f, u1, v1 },
		};
		device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(QuadVertex));
	}

	void Set_Target(IDirect3DDevice9* device, IDirect3DTexture9* texture, UINT width, UINT height)
	{
		IDirect3DSurface9* surface = nullptr;
		texture->GetSurfaceLevel(0, &surface);
		device->SetRenderTarget(0, surface);
		surface->Release();
		D3DVIEWPORT9 vp = { 0, 0, width, height, 0.0f, 1.0f };
		device->SetViewport(&vp);
	}
}

//-------------------------------------------------------------------------------------------------
void Sage2SSAOClass::Set_Enabled(bool enabled)
{
	s_enabled = enabled;
	D3D8On9_Set_Readable_Depth(enabled);	// the next device made gets a depth buffer the shaders can read
}

bool Sage2SSAOClass::Is_Enabled() { return s_enabled; }

void Sage2SSAOClass::Set_Debug(bool debug) { s_debug = debug; }

void Sage2SSAOClass::Set_Params(float radius, float intensity, float strength)
{
	s_radius = radius > 0.5f ? radius : 0.5f;
	s_intensity = intensity > 0.0f ? intensity : 0.0f;
	s_strength = strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength);
}

//-------------------------------------------------------------------------------------------------
void Sage2SSAOClass::Apply()
{
	if (!s_enabled || s_strength <= 0.0f || s_state == UNAVAILABLE)
		return;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	if (device8 == nullptr)
		return;
	IDirect3DTexture9* depthTexture = device8->Get_Readable_Depth();
	IDirect3DSurface9* depthSurface = device8->Get_Readable_Depth_Surface();
	if (depthTexture == nullptr || depthSurface == nullptr)
	{
		if (!s_loggedNoDepth)
		{
			s_loggedNoDepth = true;
			Sage2_Log("ssao: no readable depth buffer (no INTZ on this device, or a multisampled frame); off");
		}
		return;
	}
	IDirect3DDevice9* device = device8->Get_D3D9();
	// only over the scene drawn with that depth buffer (not into a render-to-texture pass, nor the
	// pillar-boxed frame's own target)
	IDirect3DSurface9* currentDepth = nullptr;
	device->GetDepthStencilSurface(&currentDepth);
	const bool ours = currentDepth == depthSurface;
	if (currentDepth != nullptr)
		currentDepth->Release();
	if (!ours)
		return;
	IDirect3DSurface9* frame = nullptr;
	if (FAILED(device->GetRenderTarget(0, &frame)) || frame == nullptr)
		return;
	D3DSURFACE_DESC frameDesc, depthDesc;
	frame->GetDesc(&frameDesc);
	depthTexture->GetLevelDesc(0, &depthDesc);
	if (frameDesc.Width != depthDesc.Width || frameDesc.Height != depthDesc.Height
		|| (s_state == NOT_TRIED && !Init(device)) || s_state != READY
		|| !Make_Targets(device, frameDesc.Width, frameDesc.Height))
	{
		frame->Release();
		return;
	}
	if (s_savedState == nullptr && FAILED(device->CreateStateBlock(D3DSBT_ALL, &s_savedState)))
	{
		s_savedState = nullptr;
		frame->Release();
		return;
	}
	s_savedState->Capture();

	// the scene's view in the frame, its projection and depth range
	D3DVIEWPORT9 view;
	device->GetViewport(&view);
	Matrix4x4 projectionMatrix;
	DX8Wrapper::_Get_DX8_Transform(D3DTS_PROJECTION, projectionMatrix);
	const float* p = (const float*)&projectionMatrix;	// D3DMATRIX order: _11 _12 .. _44
	const float W = (float)frameDesc.Width, H = (float)frameDesc.Height;
	const float viewport[4] = { (float)view.X / W, (float)view.Y / H, W / (float)view.Width, H / (float)view.Height };
	// W3D's camera is right-handed (it looks along -z, _34 = -1); the shaders work with the distance
	// along the view (positive), so the terms that carry the handedness take its sign
	const float hand = p[11] < 0.0f ? -1.0f : 1.0f;
	const float projection[4] = { p[0], p[5], hand * p[8], hand * p[9] };
	const float depthRange = view.MaxZ - view.MinZ;
	const float depthParams[4] = { hand * p[10], p[14], view.MinZ, depthRange > 0.0f ? 1.0f / depthRange : 1.0f };
	const float r2 = s_radius * s_radius;
	// SAO's intensity / radius^6, times the radius: its v.n / v.v term has the dimension of 1 / length,
	// so the occlusion would hang on the world's units (SAO's were metres, with radii about 1)
	const float aoParams[4] = { s_radius, s_intensity * s_radius / (r2 * r2 * r2), s_radius * 0.03f, 0.0f };
	const UINT hw = s_width / 2 > 0 ? s_width / 2 : 1, hh = s_height / 2 > 0 ? s_height / 2 : 1;
	const float frameParams[4] = { 1.0f / W, 1.0f / H, 1.0f / (float)hw, 1.0f / (float)hh };
	const float applyParams[4] = { s_strength, FADE_START, 1.0f / FADE_LENGTH, s_debug ? 1.0f : 0.0f };
	if (!s_loggedOn)
	{
		s_loggedOn = true;
		Sage2_Log("ssao: on, radius %.1f, intensity %.2f, strength %.2f; %ux%u at half size; view %u,%u %ux%u depth %.2f..%.2f; "
			"projection P11 %.3f P22 %.3f P31 %.3f P32 %.3f P33 %.5f P34 %.3f P43 %.3f", s_radius, s_intensity, s_strength,
			(unsigned)hw, (unsigned)hh, (unsigned)view.X, (unsigned)view.Y, (unsigned)view.Width, (unsigned)view.Height, view.MinZ, view.MaxZ,
			p[0], p[5], p[8], p[9], p[10], p[11], p[14]);
	}

	// the depth buffer is read, not bound
	device->SetDepthStencilSurface(nullptr);
	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_FOGENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);
	device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
	for (DWORD s = 0; s < 2; ++s)
	{
		const DWORD filter = s == 0 ? D3DTEXF_POINT : D3DTEXF_LINEAR;
		device->SetSamplerState(s, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		device->SetSamplerState(s, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		device->SetSamplerState(s, D3DSAMP_MINFILTER, filter);
		device->SetSamplerState(s, D3DSAMP_MAGFILTER, filter);
		device->SetSamplerState(s, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		device->SetSamplerState(s, D3DSAMP_SRGBTEXTURE, FALSE);
	}
	device->SetVertexDeclaration(s_declaration);
	device->SetVertexShader(s_vertexShader);
	device->SetPixelShaderConstantF(0, viewport, 1);
	device->SetPixelShaderConstantF(1, projection, 1);
	device->SetPixelShaderConstantF(2, depthParams, 1);
	device->SetPixelShaderConstantF(3, aoParams, 1);
	device->SetPixelShaderConstantF(4, frameParams, 1);
	device->SetPixelShaderConstantF(6, applyParams, 1);
	device->SetTexture(0, depthTexture);

	// 1. the occlusion, at half size over the whole frame
	Set_Target(device, s_aoA, hw, hh);
	device->SetTexture(1, nullptr);
	device->SetPixelShader(s_aoShader);
	Draw_Quad(device, hw, hh, 0.0f, 0.0f, 1.0f, 1.0f);
	// 2., 3. blurred across, then down, keeping to surfaces of about the same depth
	device->SetPixelShader(s_blurShader);
	const float blurAcross[4] = { 1.0f / (float)hw, 0.0f, BLUR_DEPTH_TOLERANCE, 0.0f };
	Set_Target(device, s_aoB, hw, hh);
	device->SetTexture(1, s_aoA);
	device->SetPixelShaderConstantF(5, blurAcross, 1);
	Draw_Quad(device, hw, hh, 0.0f, 0.0f, 1.0f, 1.0f);
	const float blurDown[4] = { 0.0f, 1.0f / (float)hh, BLUR_DEPTH_TOLERANCE, 0.0f };
	Set_Target(device, s_aoA, hw, hh);
	device->SetTexture(1, s_aoB);
	device->SetPixelShaderConstantF(5, blurDown, 1);
	Draw_Quad(device, hw, hh, 0.0f, 0.0f, 1.0f, 1.0f);
	// Tiberium Project @feature the scene's view distance, the whole frame, for the soft particles
	if (s_sceneDepth != nullptr)
	{
		Set_Target(device, s_sceneDepth, s_width, s_height);
		device->SetTexture(1, nullptr);
		device->SetPixelShader(s_depthShader);
		Draw_Quad(device, s_width, s_height, 0.0f, 0.0f, 1.0f, 1.0f);
		s_sceneDepthFrame = WW3D::Get_Frame_Count();
	}
	// 4. onto the scene's view: the frame times the occlusion
	device->SetRenderTarget(0, frame);
	device->SetViewport(&view);
	device->SetTexture(1, s_aoA);
	device->SetPixelShader(s_applyShader);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, s_debug ? FALSE : TRUE);
	device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
	Draw_Quad(device, view.Width, view.Height, viewport[0], viewport[1],
		(float)(view.X + view.Width) / W, (float)(view.Y + view.Height) / H);

	device->SetTexture(0, nullptr);
	device->SetTexture(1, nullptr);
	device->SetRenderTarget(0, frame);
	device->SetDepthStencilSurface(depthSurface);
	s_savedState->Apply();
	frame->Release();
}

//-------------------------------------------------------------------------------------------------
IDirect3DTexture9* Sage2SSAOClass::Get_Scene_Depth_Texture()
{
	return s_sceneDepth != nullptr && s_sceneDepthFrame == WW3D::Get_Frame_Count() ? s_sceneDepth : nullptr;
}

//-------------------------------------------------------------------------------------------------
void Sage2SSAOClass::Release_Resources()
{
	Release(s_aoA);
	Release(s_aoB);
	Release(s_sceneDepth);
	s_sceneDepthFrame = 0xFFFFFFFF;
	s_width = s_height = 0;
	Release(s_savedState);	// it holds references to whatever was bound
}

void Sage2SSAOClass::Shutdown()
{
	Release_Resources();
	Release(s_vertexShader);
	Release(s_aoShader);
	Release(s_blurShader);
	Release(s_applyShader);
	Release(s_depthShader);
	Release(s_declaration);
	s_state = NOT_TRIED;
}
