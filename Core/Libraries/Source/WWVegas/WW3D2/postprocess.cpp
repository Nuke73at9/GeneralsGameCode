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

// Tiberium Project @feature Scene post-processing (see postprocess.h)

#include "postprocess.h"

#include "dx8caps.h"
#include "dx8wrapper.h"
#include "sage2/sage2post.h"

#include "shaders/postprocess_vs.h"	// from shaders/postprocess.hlsl (shaders/build_shaders.py)
#include "shaders/postprocess_bright_ps.h"
#include "shaders/postprocess_blur_ps.h"
#include "shaders/postprocess_combine_ps.h"

bool PostProcessClass::Enabled = true;

namespace
{
	enum InitState { NOT_TRIED, READY, UNAVAILABLE };

	InitState					s_state = NOT_TRIED;
	IDirect3DVertexShader9*	s_vertexShader = nullptr;
	IDirect3DPixelShader9*	s_brightShader = nullptr;
	IDirect3DPixelShader9*	s_blurShader = nullptr;
	IDirect3DPixelShader9*	s_combineShader = nullptr;
	IDirect3DVertexDeclaration9* s_declaration = nullptr;
	IDirect3DStateBlock9*		s_savedState = nullptr;

	// render targets, made for the current back buffer size
	IDirect3DTexture9*		s_scene = nullptr;		// copy of the frame
	IDirect3DTexture9*		s_bloomA = nullptr;	// quarter size
	IDirect3DTexture9*		s_bloomB = nullptr;
	UINT						s_width = 0;
	UINT						s_height = 0;

	// Look tuning, against C&C4 footage
	const float BLOOM_THRESHOLD	= 0.72f;
	const float BLOOM_STRENGTH	= 0.45f;
	const float SATURATION		= 0.82f;
	const float CONTRAST			= 1.08f;
	const float VIGNETTE			= 0.35f;
	const float TINT[3]			= { 1.0f, 0.985f, 0.95f };	// slightly warm, like C&C4's sun

	struct QuadVertex { float x, y, u, v; };

	template <class T> void Release(T*& p) { if (p) { p->Release(); p = nullptr; } }

	IDirect3DDevice9* Device()
	{
		IDirect3DDevice8* device = DX8Wrapper::_Get_D3D_Device8();
		return device ? device->Get_D3D9() : nullptr;
	}

	bool Init()
	{
		s_state = UNAVAILABLE;
		IDirect3DDevice9* device = Device();
		if (device == nullptr
			|| DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Major_Version() < 3
			|| DX8Wrapper::Get_Current_Caps()->Get_Vertex_Shader_Major_Version() < 3)
			return false;
		const D3DVERTEXELEMENT9 elements[] =
		{
			{ 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			D3DDECL_END()
		};
		if (FAILED(device->CreateVertexShader(PostProcessVertexShader, &s_vertexShader))
			|| FAILED(device->CreatePixelShader(PostProcessBrightPixelShader, &s_brightShader))
			|| FAILED(device->CreatePixelShader(PostProcessBlurPixelShader, &s_blurShader))
			|| FAILED(device->CreatePixelShader(PostProcessCombinePixelShader, &s_combineShader))
			|| FAILED(device->CreateVertexDeclaration(elements, &s_declaration)))
		{
			PostProcessClass::Shutdown();
			s_state = UNAVAILABLE;
			return false;
		}
		s_state = READY;
		return true;
	}

	bool Make_Targets(IDirect3DDevice9* device, UINT width, UINT height)
	{
		if (s_scene != nullptr && width == s_width && height == s_height)
			return true;
		PostProcessClass::Release_Resources();
		UINT qw = width / 4 > 0 ? width / 4 : 1;
		UINT qh = height / 4 > 0 ? height / 4 : 1;
		if (FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &s_scene, nullptr))
			|| FAILED(device->CreateTexture(qw, qh, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &s_bloomA, nullptr))
			|| FAILED(device->CreateTexture(qw, qh, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &s_bloomB, nullptr)))
		{
			PostProcessClass::Release_Resources();
			return false;
		}
		s_width = width;
		s_height = height;
		return true;
	}

	void Draw_Pass(IDirect3DDevice9* device, IDirect3DSurface9* target, UINT width, UINT height,
		IDirect3DPixelShader9* shader, IDirect3DTexture9* source, IDirect3DTexture9* second, const float params[2][4])
	{
		device->SetRenderTarget(0, target);
		D3DVIEWPORT9 vp = { 0, 0, width, height, 0.0f, 1.0f };
		device->SetViewport(&vp);
		const float halfPixel[4] = { -1.0f / width, 1.0f / height, 0.0f, 0.0f };
		device->SetVertexShaderConstantF(0, halfPixel, 1);
		device->SetPixelShaderConstantF(0, &params[0][0], 2);
		device->SetPixelShader(shader);
		device->SetTexture(0, source);
		device->SetTexture(1, second);
		const QuadVertex quad[4] =
		{
			{ -1.0f,  1.0f, 0.0f, 0.0f },
			{  1.0f,  1.0f, 1.0f, 0.0f },
			{ -1.0f, -1.0f, 0.0f, 1.0f },
			{  1.0f, -1.0f, 1.0f, 1.0f },
		};
		device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(QuadVertex));
	}
}

//-------------------------------------------------------------------------------------------------
void PostProcessClass::Apply()
{
	if (!Enabled)
		return;
	if (Sage2PostClass::Apply())	// Tiberium Project @feature C&C4's own bloom and grading when its effects are in the mod
		return;
	if (s_state == NOT_TRIED)
		Init();
	if (s_state != READY)
		return;

	IDirect3DDevice9* device = Device();
	IDirect3DSurface9* frame = nullptr;
	if (FAILED(device->GetRenderTarget(0, &frame)))
		return;
	D3DSURFACE_DESC desc;
	frame->GetDesc(&desc);
	if (!Make_Targets(device, desc.Width, desc.Height))
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

	IDirect3DSurface9* scene = nullptr;
	IDirect3DSurface9* bloomA = nullptr;
	IDirect3DSurface9* bloomB = nullptr;
	s_scene->GetSurfaceLevel(0, &scene);
	s_bloomA->GetSurfaceLevel(0, &bloomA);
	s_bloomB->GetSurfaceLevel(0, &bloomB);
	device->StretchRect(frame, nullptr, scene, nullptr, D3DTEXF_NONE);

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
		device->SetSamplerState(s, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		device->SetSamplerState(s, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		device->SetSamplerState(s, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(s, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(s, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		device->SetSamplerState(s, D3DSAMP_SRGBTEXTURE, FALSE);
	}
	device->SetVertexDeclaration(s_declaration);
	device->SetVertexShader(s_vertexShader);

	UINT qw = s_width / 4 > 0 ? s_width / 4 : 1;
	UINT qh = s_height / 4 > 0 ? s_height / 4 : 1;
	const float bright[2][4] = { { BLOOM_THRESHOLD, 1.0f / (1.0f - BLOOM_THRESHOLD), 1.0f / s_width, 1.0f / s_height }, { 0, 0, 0, 0 } };
	Draw_Pass(device, bloomA, qw, qh, s_brightShader, s_scene, nullptr, bright);
	const float blurH[2][4] = { { 1.0f / qw, 0.0f, 0.0f, 0.0f }, { 0, 0, 0, 0 } };
	Draw_Pass(device, bloomB, qw, qh, s_blurShader, s_bloomA, nullptr, blurH);
	const float blurV[2][4] = { { 0.0f, 1.0f / qh, 0.0f, 0.0f }, { 0, 0, 0, 0 } };
	Draw_Pass(device, bloomA, qw, qh, s_blurShader, s_bloomB, nullptr, blurV);
	const float combine[2][4] = { { SATURATION, CONTRAST, BLOOM_STRENGTH, VIGNETTE }, { TINT[0], TINT[1], TINT[2], 1.0f } };
	Draw_Pass(device, frame, desc.Width, desc.Height, s_combineShader, s_scene, s_bloomA, combine);

	device->SetRenderTarget(0, frame);
	s_savedState->Apply();

	scene->Release();
	bloomA->Release();
	bloomB->Release();
	frame->Release();
}

//-------------------------------------------------------------------------------------------------
void PostProcessClass::Release_Resources()
{
	Release(s_scene);
	Release(s_bloomA);
	Release(s_bloomB);
	s_width = s_height = 0;
	Release(s_savedState);	// it holds references to whatever was bound; made again on the next Apply()
}

//-------------------------------------------------------------------------------------------------
void PostProcessClass::Shutdown()
{
	Release_Resources();
	Release(s_vertexShader);
	Release(s_brightShader);
	Release(s_blurShader);
	Release(s_combineShader);
	Release(s_declaration);
	s_state = NOT_TRIED;
}
