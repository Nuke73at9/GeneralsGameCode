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

// Tiberium Project @feature Soft particles (see sage2softparticle.h).

#include "sage2softparticle.h"

#include "dx8caps.h"
#include "dx8wrapper.h"
#include "sage2globals.h"
#include "sage2scene.h"
#include "shader.h"

#include "../shaders/softparticle_ps.h"
#include "../shaders/softparticle_vs.h"

namespace
{
	enum State { NOT_TRIED, READY, UNAVAILABLE };
	State s_state = NOT_TRIED;
	bool s_enabled = true;
	float s_fadeDistance = 10.0f;
	IDirect3DVertexShader9* s_vertexShader = nullptr;
	IDirect3DPixelShader9* s_pixelShader = nullptr;
	bool s_logged = false;

	bool Init()
	{
		s_state = UNAVAILABLE;
		IDirect3DDevice8* device = DX8Wrapper::_Get_D3D_Device8();
		if (device == nullptr
			|| DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Major_Version() < 3
			|| DX8Wrapper::Get_Current_Caps()->Get_Vertex_Shader_Major_Version() < 3)
		{
			Sage2_Log("soft particles: unavailable (needs shader model 3)");
			return false;
		}
		IDirect3DDevice9* device9 = device->Get_D3D9();
		if (FAILED(device9->CreateVertexShader(SoftParticleVertexShader, &s_vertexShader)))
		{
			s_vertexShader = nullptr;
			Sage2_Log("soft particles: could not make the vertex shader");
			return false;
		}
		if (FAILED(device9->CreatePixelShader(SoftParticlePixelShader, &s_pixelShader)))
		{
			s_pixelShader = nullptr;
			s_vertexShader->Release();
			s_vertexShader = nullptr;
			Sage2_Log("soft particles: could not make the pixel shader");
			return false;
		}
		s_state = READY;
		return true;
	}

	// row-vector 4x4 matrices as Direct3D stores them (the transforms W3D sets)
	void Multiply(const float a[16], const float b[16], float out[16])
	{
		for (int r = 0; r < 4; ++r)
			for (int c = 0; c < 4; ++c)
				out[r * 4 + c] = a[r * 4] * b[c] + a[r * 4 + 1] * b[4 + c] + a[r * 4 + 2] * b[8 + c] + a[r * 4 + 3] * b[12 + c];
	}

	void Transposed(const float m[16], float out[16])
	{
		for (int r = 0; r < 4; ++r)
			for (int c = 0; c < 4; ++c)
				out[r * 4 + c] = m[c * 4 + r];
	}
}

//-------------------------------------------------------------------------------------------------
void Sage2SoftParticleClass::Set_Enabled(bool enabled)
{
	s_enabled = enabled;
}

bool Sage2SoftParticleClass::Is_Enabled()
{
	return s_enabled;
}

void Sage2SoftParticleClass::Set_Fade_Distance(float distance)
{
	s_fadeDistance = distance < 0.5f ? 0.5f : (distance > 200.0f ? 200.0f : distance);
}

//-------------------------------------------------------------------------------------------------
bool Sage2SoftParticleClass::Begin(const ShaderClass& shader)
{
	if (!s_enabled)
		return false;
	// the blending this fades: additive (the colour), alpha (the alpha), multiplied (to white);
	// alpha-tested particles keep their hard cut-out
	if (shader.Get_Alpha_Test() != ShaderClass::ALPHATEST_DISABLE || shader.Get_Texturing() == ShaderClass::TEXTURING_DISABLE)
		return false;
	const ShaderClass::SrcBlendFuncType src = shader.Get_Src_Blend_Func();
	const ShaderClass::DstBlendFuncType dst = shader.Get_Dst_Blend_Func();
	float soft[4] = { 1.0f / s_fadeDistance, 0.0f, 0.0f, 0.0f };
	if (src == ShaderClass::SRCBLEND_ONE && dst == ShaderClass::DSTBLEND_ONE)
		soft[1] = 1.0f;
	else if (src == ShaderClass::SRCBLEND_ZERO && dst == ShaderClass::DSTBLEND_SRC_COLOR)
		soft[2] = 1.0f;
	else if (src == ShaderClass::SRCBLEND_SRC_ALPHA && dst == ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA)
		soft[3] = 1.0f;
	else
		return false;
	IDirect3DTexture9* depth = Sage2SceneClass::Get_Linear_Depth_Texture();
	if (depth == nullptr)
		return false;
	if (s_state == NOT_TRIED)
		Init();
	if (s_state != READY)
		return false;
	D3DSURFACE_DESC desc;
	if (FAILED(depth->GetLevelDesc(0, &desc)) || desc.Width == 0 || desc.Height == 0)
		return false;
	if (!s_logged)
	{
		s_logged = true;
		Sage2_Log("soft particles: on, fading over %.1f units in front of the scene's depth (%ux%u)", s_fadeDistance,
			(unsigned)desc.Width, (unsigned)desc.Height);
	}

	// the draw's transforms, as W3D set them for it (particles: in view space, world and view
	// the identity)
	Matrix4x4 world, view, projection;
	DX8Wrapper::_Get_DX8_Transform(D3DTS_WORLD, world);
	DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, view);
	DX8Wrapper::_Get_DX8_Transform(D3DTS_PROJECTION, projection);
	float worldView[16], worldViewProjection[16];
	Multiply((const float*)&world, (const float*)&view, worldView);
	Multiply(worldView, (const float*)&projection, worldViewProjection);
	float constants[32];
	Transposed(worldViewProjection, constants);
	Transposed(worldView, constants + 16);
	// z: the colour's scale, 2 for a sprite shader that modulates by twice the colour (the particles'
	// BrightColors)
	const float frame[4] = { 1.0f / (float)desc.Width, 1.0f / (float)desc.Height,
		shader.Get_Primary_Gradient() == ShaderClass::GRADIENT_MODULATE2X ? 2.0f : 1.0f, 0.0f };

	IDirect3DDevice9* device = DX8Wrapper::_Get_D3D_Device8()->Get_D3D9();
	device->SetVertexShader(s_vertexShader);
	device->SetPixelShader(s_pixelShader);
	device->SetVertexShaderConstantF(0, constants, 8);
	device->SetPixelShaderConstantF(0, soft, 1);
	device->SetPixelShaderConstantF(1, frame, 1);
	device->SetTexture(3, depth);
	device->SetSamplerState(3, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	device->SetSamplerState(3, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	device->SetSamplerState(3, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	device->SetSamplerState(3, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	device->SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	device->SetSamplerState(3, D3DSAMP_SRGBTEXTURE, FALSE);
	return true;
}

//-------------------------------------------------------------------------------------------------
void Sage2SoftParticleClass::End()
{
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DDevice9* device = device8->Get_D3D9();
	device->SetTexture(3, nullptr);
	device->SetPixelShader(nullptr);
	device->SetVertexShader(nullptr);
	// the adapter sets the vertex format and its vertex shader override again on the next draw
	device8->Invalidate_State();
}

//-------------------------------------------------------------------------------------------------
void Sage2SoftParticleClass::Shutdown()
{
	if (s_vertexShader) s_vertexShader->Release();
	if (s_pixelShader) s_pixelShader->Release();
	s_vertexShader = nullptr;
	s_pixelShader = nullptr;
	s_state = NOT_TRIED;
	s_logged = false;
}
