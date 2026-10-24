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

// Tiberium Project @feature C&C4's shadow map (see sage2shadow.h)

#include "sage2shadow.h"

#include <math.h>

#include "camera.h"
#include "dx8wrapper.h"
#include "sage2globals.h"
#include "WWMath/matrix3d.h"
#include "WWMath/matrix4.h"
#include "WWMath/vector3.h"
#include "WWMath/vector4.h"

namespace
{
	const D3DFORMAT FORMAT_INTZ = (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z');	// a depth buffer the shaders can read

	bool s_enabled = true;
	unsigned s_mapSize = 2048;
	float s_kernel = 1.0f;		// C&C4's soft shadow: the 3x3 kernel's step, in texels
	float s_jitter = 0.5f;		// and its jitter, in texels
	enum AvailState { NOT_TRIED, AVAILABLE, UNAVAILABLE };
	AvailState s_available = NOT_TRIED;

	IDirect3DTexture9* s_depthTexture = nullptr;	// the map: the depth target itself
	IDirect3DSurface9* s_depthSurface = nullptr;
	IDirect3DSurface9* s_colorTarget = nullptr;		// Direct3D wants a colour target along; nothing is written to it
	unsigned s_madeSize = 0;
	CameraClass* s_camera = nullptr;
	bool s_rendering = false;
	bool s_mapReady = false;
	float s_worldToShadow[16];
	bool s_debug = false;
	int s_passesLogged = 0;
	int s_effectDraws = 0;
	unsigned s_fixedFunctionDraws = 0, s_fixedFunctionPolygons = 0;	// DX8Wrapper::Draw calls during the pass
	bool s_terrainDrawn = false;
	DWORD s_savedColorWrite = 0xF;

	template <class T> void Release(T*& p) { if (p) p->Release(); p = nullptr; }

	bool Make_Resources(IDirect3DDevice9* device)
	{
		if (s_depthTexture != nullptr && s_madeSize == s_mapSize)
			return true;
		Sage2ShadowClass::Release_Resources();
		if (s_available == NOT_TRIED)
		{
			s_available = UNAVAILABLE;
			IDirect3D9* d3d = nullptr;
			D3DDEVICE_CREATION_PARAMETERS cp;
			D3DDISPLAYMODE mode;
			if (SUCCEEDED(device->GetDirect3D(&d3d)) && d3d != nullptr)
			{
				if (SUCCEEDED(device->GetCreationParameters(&cp)) && SUCCEEDED(d3d->GetAdapterDisplayMode(cp.AdapterOrdinal, &mode))
					&& SUCCEEDED(d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, mode.Format, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, FORMAT_INTZ)))
					s_available = AVAILABLE;
				d3d->Release();
			}
			if (s_available != AVAILABLE)
				Sage2_Log("shadow map: the device has no readable depth texture (INTZ); no shadow map");
		}
		if (s_available != AVAILABLE)
			return false;
		if (FAILED(device->CreateTexture(s_mapSize, s_mapSize, 1, D3DUSAGE_DEPTHSTENCIL, FORMAT_INTZ, D3DPOOL_DEFAULT, &s_depthTexture, nullptr))
			|| FAILED(s_depthTexture->GetSurfaceLevel(0, &s_depthSurface))
			|| FAILED(device->CreateRenderTarget(s_mapSize, s_mapSize, D3DFMT_R32F, D3DMULTISAMPLE_NONE, 0, FALSE, &s_colorTarget, nullptr)))
		{
			Sage2_Log("shadow map: could not make the %ux%u map", s_mapSize, s_mapSize);
			Sage2ShadowClass::Release_Resources();
			return false;
		}
		s_madeSize = s_mapSize;
		Sage2_Log("shadow map: %ux%u depth texture", s_mapSize, s_mapSize);
		return true;
	}
}

//-------------------------------------------------------------------------------------------------
void Sage2ShadowClass::Set_Enabled(bool enabled) { s_enabled = enabled; if (!enabled) s_mapReady = false; }
bool Sage2ShadowClass::Is_Enabled() { return s_enabled; }
void Sage2ShadowClass::Set_Map_Size(unsigned size)
{
	unsigned s = 512;
	while (s < size && s < 4096)
		s *= 2;
	s_mapSize = s;
}
void Sage2ShadowClass::Set_Soft(float kernel, float jitter)
{
	s_kernel = kernel >= 0.0f ? kernel : 0.0f;
	s_jitter = jitter >= 0.0f ? jitter : 0.0f;
}
bool Sage2ShadowClass::Is_Rendering() { return s_rendering; }
bool Sage2ShadowClass::Is_Available() { return s_mapReady && !s_rendering && s_depthTexture != nullptr; }
IDirect3DBaseTexture9* Sage2ShadowClass::Get_Texture() { return s_depthTexture; }
const float* Sage2ShadowClass::Get_World_To_Shadow() { return s_worldToShadow; }
unsigned Sage2ShadowClass::Get_Map_Size() { return s_madeSize; }
void Sage2ShadowClass::Get_Soft(float& kernel, float& jitter) { kernel = s_kernel; jitter = s_jitter; }
CameraClass* Sage2ShadowClass::Get_Camera() { return s_camera; }

//-------------------------------------------------------------------------------------------------
bool Sage2ShadowClass::Begin(const Vector3& sunTravel, const Vector3& boxMin, const Vector3& boxMax)
{
	s_mapReady = false;
	if (!s_enabled || s_rendering)
		return false;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	if (device8 == nullptr)
		return false;
	IDirect3DDevice9* device = device8->Get_D3D9();
	if (!Make_Resources(device))
		return false;
	Vector3 forward = sunTravel;
	if (forward.Length2() < 1e-6f)
		return false;
	forward.Normalize();
	if (forward.Z > -0.05f)	// the sun below the horizon: nothing to cast
		return false;
	if (s_camera == nullptr)
		s_camera = NEW_REF(CameraClass, ());

	// the camera: far along the sun's direction, looking back along it over the box
	Vector3 center = (boxMin + boxMax) * 0.5f;
	Vector3 extent = (boxMax - boxMin) * 0.5f;
	float radius = extent.Length();
	if (radius < 1.0f)
		return false;
	float distance = radius * 40.0f;	// a field of view under three degrees: orthographic to within a degree
	Vector3 zAxis = -forward;			// W3D cameras look down -Z
	Vector3 up = fabsf(forward.Z) > 0.99f ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(0.0f, 0.0f, 1.0f);
	Vector3 xAxis;
	Vector3::Cross_Product(up, zAxis, &xAxis);
	xAxis.Normalize();
	Vector3 yAxis;
	Vector3::Cross_Product(zAxis, xAxis, &yAxis);
	Vector3 position = center - forward * distance;
	Matrix3D transform(xAxis, yAxis, zAxis, position);
	s_camera->Set_Transform(transform);

	// the box's corners in camera space set the view plane (at depth 1) and the clip planes
	Matrix3D inverse;
	transform.Get_Inverse(inverse);
	float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f, minD = 1e30f, maxD = -1e30f;
	for (int c = 0; c < 8; ++c)
	{
		Vector3 corner((c & 1) ? boxMax.X : boxMin.X, (c & 2) ? boxMax.Y : boxMin.Y, (c & 4) ? boxMax.Z : boxMin.Z);
		Vector3 v;
		Matrix3D::Transform_Vector(inverse, corner, &v);
		float depth = -v.Z;
		if (depth < 1.0f)
			depth = 1.0f;
		float x = v.X / depth, y = v.Y / depth;
		if (x < minX) minX = x; if (x > maxX) maxX = x;
		if (y < minY) minY = y; if (y > maxY) maxY = y;
		if (depth < minD) minD = depth; if (depth > maxD) maxD = depth;
	}
	// square, so the map's texels are square too, with a texel of margin; the view plane's
	// position is snapped to the texel so the map does not shimmer as the camera moves
	float half = (maxX - minX > maxY - minY ? maxX - minX : maxY - minY) * 0.5f;
	float texel = 2.0f * half / (float)s_madeSize;
	half += texel;
	float cx = floorf((minX + maxX) * 0.5f / texel) * texel;
	float cy = floorf((minY + maxY) * 0.5f / texel) * texel;
	s_camera->Set_View_Plane(Vector2(cx - half, cy - half), Vector2(cx + half, cy + half));
	s_camera->Set_Clip_Planes(minD * 0.98f, maxD * 1.02f);
	s_camera->Set_Viewport(Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));

	// the targets: the depth texture as the depth buffer, colour writes off
	DX8Wrapper::Set_Render_Target(s_colorTarget, s_depthSurface);
	device->GetRenderState(D3DRS_COLORWRITEENABLE, &s_savedColorWrite);
	device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFFFFFFFF, 1.0f, 0);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
	s_camera->Apply();

	// world -> the map's texture coordinates and depth, from the transforms the camera set:
	// clip x,y to 0..1 (y down), plus half a texel for Direct3D 9's pixel centres
	Matrix4x4 view, projection;
	DX8Wrapper::Get_Transform(D3DTS_VIEW, view);
	DX8Wrapper::Get_Transform(D3DTS_PROJECTION, projection);
	float halfTexel = 0.5f / (float)s_madeSize;
	Matrix4x4 toTexture(Vector4(0.5f, 0.0f, 0.0f, 0.5f + halfTexel),
		Vector4(0.0f, -0.5f, 0.0f, 0.5f + halfTexel),
		Vector4(0.0f, 0.0f, 1.0f, 0.0f),
		Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	Matrix4x4 worldToShadow = toTexture * projection * view;
	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c)
			s_worldToShadow[r * 4 + c] = worldToShadow[r][c];
	s_rendering = true;
	s_effectDraws = 0;
	s_fixedFunctionDraws = 0;
	s_fixedFunctionPolygons = 0;
	s_terrainDrawn = false;

	static bool logged = false;
	if (!logged)
	{
		logged = true;
		Sage2_Log("shadow map: sun travels (%.2f %.2f %.2f), box (%.0f %.0f %.0f)-(%.0f %.0f %.0f), camera %.0f away, view plane %.4f, depth %.0f..%.0f",
			forward.X, forward.Y, forward.Z, boxMin.X, boxMin.Y, boxMin.Z, boxMax.X, boxMax.Y, boxMax.Z, distance, half, minD, maxD);
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
void Sage2ShadowClass::End()
{
	if (!s_rendering)
		return;
	IDirect3DDevice9* device = DX8Wrapper::_Get_D3D_Device8()->Get_D3D9();
	device->SetRenderState(D3DRS_COLORWRITEENABLE, s_savedColorWrite);
	DX8Wrapper::Set_Render_Target((IDirect3DSurface8*)nullptr);
	s_rendering = false;
	s_mapReady = true;
	// the first three passes, and one well into the game (the first ones can come before the map's
	// objects are drawn)
	static unsigned passes = 0;
	++passes;
	if (s_passesLogged < 3 || passes == 150)
	{
		++s_passesLogged;
		DWORD scissor = 0, zenable = 0;
		device->GetRenderState(D3DRS_SCISSORTESTENABLE, &scissor);
		device->GetRenderState(D3DRS_ZENABLE, &zenable);
		Sage2_Log("shadow pass %u: %u fixed-function draws (%u polygons), %d drawn with their own effect, terrain %s (scissor %u, z %u)",
			passes, s_fixedFunctionDraws, s_fixedFunctionPolygons, s_effectDraws, s_terrainDrawn ? "drawn" : "not drawn",
			(unsigned)scissor, (unsigned)zenable);
	}
}

//-------------------------------------------------------------------------------------------------
void Sage2ShadowClass::Count_Draw(bool terrain)
{
	if (!s_rendering)
		return;
	++s_effectDraws;
	if (terrain)
		s_terrainDrawn = true;
}

//-------------------------------------------------------------------------------------------------
void Sage2ShadowClass::Count_Fixed_Function(unsigned polygons)
{
	if (!s_rendering)
		return;
	++s_fixedFunctionDraws;
	s_fixedFunctionPolygons += polygons;
}

void Sage2ShadowClass::Set_Debug(bool enabled) { s_debug = enabled; }
bool Sage2ShadowClass::Is_Debug() { return s_debug; }

//-------------------------------------------------------------------------------------------------
void Sage2ShadowClass::Draw_Debug()
{
	if (!s_debug || !Is_Available())
		return;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	if (device8 == nullptr)
		return;
	IDirect3DDevice9* device = device8->Get_D3D9();
	DX8Wrapper::Apply_Render_State_Changes();
	IDirect3DStateBlock9* saved = nullptr;
	if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &saved)))
		return;
	// the map, fixed-function: a depth texture read this way returns its depth in every channel
	const float size = 480.0f, x0 = 8.0f - 0.5f, y0 = 8.0f - 0.5f;
	struct V { float x, y, z, rhw, u, v; };
	V quad[4] = { { x0, y0, 0, 1, 0, 0 }, { x0 + size, y0, 0, 1, 1, 0 }, { x0, y0 + size, 0, 1, 0, 1 }, { x0 + size, y0 + size, 0, 1, 1, 1 } };
	device->SetVertexShader(nullptr);
	device->SetPixelShader(nullptr);
	device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
	device->SetTexture(0, s_depthTexture);
	device->SetTexture(1, nullptr);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);
	device->SetRenderState(D3DRS_FOGENABLE, FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(V));
	saved->Apply();
	saved->Release();
	device8->Invalidate_State();
	DX8Wrapper::Invalidate_Shader_Constants();
	ShaderClass::Invalidate();
}

//-------------------------------------------------------------------------------------------------
void Sage2ShadowClass::Release_Resources()
{
	Release(s_depthSurface);
	Release(s_depthTexture);
	Release(s_colorTarget);
	s_madeSize = 0;
	s_mapReady = false;
	s_rendering = false;
}

//-------------------------------------------------------------------------------------------------
void Sage2ShadowClass::Shutdown()
{
	Release_Resources();
	REF_PTR_RELEASE(s_camera);
	s_available = NOT_TRIED;
}
