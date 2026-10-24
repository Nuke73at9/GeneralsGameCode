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
#include <vector>

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
	int s_pointLightTechnique = -1;
	bool s_clearedThisFrame = false;

	// Tiberium Project @feature this frame's point lights
	const unsigned MAX_POINT_LIGHTS = 64;
	std::vector<Sage2SceneClass::PointLight> s_pointLights;
	float s_pointLightStrength = 2.0f;	// Zero Hour lit the ground with a pulse's ambient and diffuse both
	bool s_pointLightsLogged = false;

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
		s_pointLightTechnique = s_lighting->Find_Technique("PointLight");	// optional: no point lights without it
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
		// Tiberium Project @bugfix The rectangle is in the scene viewport's coordinates, which is
		// smaller than the frame while the control bar is up (Zero Hour draws the world in the top
		// 80%). The buffers are frame-sized and the effects address them by frame position, so the
		// texture coordinates are the frame's: with the viewport's own 0..1 the linear depth read
		// the pre-pass 1.25x too low down, and below 64% of the screen read rows the pre-pass never
		// drew - the dark band over the bottom of the view, and shadows sliding with the camera.
		D3DVIEWPORT9 vp;
		if (FAILED(device->GetViewport(&vp)) || vp.Width == 0 || vp.Height == 0)
		{
			vp.X = vp.Y = 0;
			vp.Width = s_width;
			vp.Height = s_height;
		}
		const float fw = (float)s_width, fh = (float)s_height;
		const float hx = 1.0f / (float)vp.Width, hy = 1.0f / (float)vp.Height;	// half a pixel: texel centres on pixel centres
		const float u0 = ((float)vp.X + (x0 * 0.5f + 0.5f) * (float)vp.Width) / fw;
		const float u1 = ((float)vp.X + (x1 * 0.5f + 0.5f) * (float)vp.Width) / fw;
		const float v0 = ((float)vp.Y + (0.5f - y0 * 0.5f) * (float)vp.Height) / fh;
		const float v1 = ((float)vp.Y + (0.5f - y1 * 0.5f) * (float)vp.Height) / fh;
		float v[4][5] =
		{
			{ x0 - hx, y0 + hy, 0.0f, u0, v0 },
			{ x1 - hx, y0 + hy, 0.0f, u1, v0 },
			{ x0 - hx, y1 + hy, 0.0f, u0, v1 },
			{ x1 - hx, y1 + hy, 0.0f, u1, v1 },
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

	// Tiberium Project @feature the clip-space rectangle a point light's sphere covers on screen;
	// false when it is off screen. A sphere that reaches behind the eye covers the whole screen.
	bool Light_Rect(const Matrix4x4& viewProjection, const Sage2SceneClass::PointLight& light, float& x0, float& y0, float& x1, float& y1)
	{
		const float r = light.outerRadius;
		float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
		for (int c = 0; c < 8; ++c)
		{
			const float x = light.position[0] + ((c & 1) ? r : -r);
			const float y = light.position[1] + ((c & 2) ? r : -r);
			const float z = light.position[2] + ((c & 4) ? r : -r);
			const float cx = viewProjection[0][0] * x + viewProjection[0][1] * y + viewProjection[0][2] * z + viewProjection[0][3];
			const float cy = viewProjection[1][0] * x + viewProjection[1][1] * y + viewProjection[1][2] * z + viewProjection[1][3];
			const float cw = viewProjection[3][0] * x + viewProjection[3][1] * y + viewProjection[3][2] * z + viewProjection[3][3];
			if (cw <= 0.001f)
			{
				x0 = y0 = -1.0f;
				x1 = y1 = 1.0f;
				return true;
			}
			const float px = cx / cw, py = cy / cw;
			if (px < minX) minX = px;
			if (px > maxX) maxX = px;
			if (py < minY) minY = py;
			if (py > maxY) maxY = py;
		}
		x0 = minX < -1.0f ? -1.0f : minX;
		y0 = minY < -1.0f ? -1.0f : minY;
		x1 = maxX > 1.0f ? 1.0f : maxX;
		y1 = maxY > 1.0f ? 1.0f : maxY;
		return x0 < x1 && y0 < y1;
	}

	// Tiberium Project @feature this frame's point lights into the bound light buffers (diffuse,
	// specular), added over what the directional light wrote, each over its own rectangle within
	// the batch's (x0,y0)-(x1,y1): C&C4's PointLight technique, its volume a screen rectangle (the
	// vertex shader's transform set to identity; the pixel shader works from the pixel position)
	void Draw_Point_Lights(IDirect3DDevice9* device, float x0, float y0, float x1, float y1)
	{
		if (s_pointLights.empty() || s_pointLightTechnique < 0 || s_pointLightStrength <= 0.0f)
			return;
		Matrix4x4 view, projection;
		DX8Wrapper::Get_Transform(D3DTS_VIEW, view);
		DX8Wrapper::Get_Transform(D3DTS_PROJECTION, projection);
		const Matrix4x4 viewProjection = projection * view;
		const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		s_lighting->Set_Float("WorldViewProjection", identity, 16);
		unsigned drawn = 0;
		for (size_t i = 0; i < s_pointLights.size(); ++i)
		{
			const Sage2SceneClass::PointLight& light = s_pointLights[i];
			float lx0, ly0, lx1, ly1;
			if (!Light_Rect(viewProjection, light, lx0, ly0, lx1, ly1))
				continue;
			if (lx0 < x0) lx0 = x0;
			if (ly0 < y0) ly0 = y0;
			if (lx1 > x1) lx1 = x1;
			if (ly1 > y1) ly1 = y1;
			if (lx0 >= lx1 || ly0 >= ly1)
				continue;
			const float position[4] = { light.position[0], light.position[1], light.position[2], light.innerRadius * light.innerRadius };
			const float color[4] = { light.color[0] * s_pointLightStrength, light.color[1] * s_pointLightStrength,
				light.color[2] * s_pointLightStrength, light.outerRadius * light.outerRadius };
			s_lighting->Set_Float("PointLightPosition", position, 4);
			s_lighting->Set_Float("PointLightColor", color, 4);
			Draw_Rect(device, *s_lighting, s_pointLightTechnique, lx0, ly0, lx1, ly1);
			++drawn;
		}
		if (!s_pointLightsLogged && drawn > 0)
		{
			s_pointLightsLogged = true;
			const Sage2SceneClass::PointLight& l = s_pointLights[0];
			Sage2_Log("point lights: %u drawn into the light buffers (of %u this frame); the first at (%.0f %.0f %.0f), colour (%.2f %.2f %.2f), radius %.0f..%.0f",
				drawn, (unsigned)s_pointLights.size(), l.position[0], l.position[1], l.position[2], l.color[0], l.color[1], l.color[2], l.innerRadius, l.outerRadius);
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
			Sage2_Log("deferred lighting: %s (%u render targets, pixel shader %x)", s_available == AVAILABLE ? "available" : "unavailable",
				(unsigned)caps.NumSimultaneousRTs, (unsigned)caps.PixelShaderVersion);
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
			// the swap chain's back buffer, not whatever target is current: the first query can come
			// from a pass that draws into its own target (the shadow map's R32F one, say), and the
			// answer is kept for the session. The frame's offscreen target is A8R8G8B8, as checked below.
			IDirect3DSurface9* back = nullptr;
			D3DSURFACE_DESC desc;
			if (SUCCEEDED(device8->Get_D3D9()->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) && back)
			{
				if (SUCCEEDED(back->GetDesc(&desc)))
				{
					D3DDISPLAYMODE mode;
					if (FAILED(d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode)))
						mode.Format = D3DFMT_X8R8G8B8;
					if (SUCCEEDED(d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mode.Format,
						D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBWRITE, D3DRTYPE_SURFACE, desc.Format))
						&& SUCCEEDED(d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mode.Format,
						D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBWRITE, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8)))
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
	class MeshDraw : public PrepassDrawClass
	{
	public:
		MeshDraw(MeshClass* m, DX8PolygonRendererClass* r) : mesh(m), renderer(r) {}
		virtual void Draw(IDirect3DDevice9*) { renderer->Render(mesh->Get_Base_Vertex_Offset()); }
		MeshClass* mesh;
		DX8PolygonRendererClass* renderer;
	};
	if (!s_enabled || !Is_Available() || prepassTechnique < 0)
		return false;
	float x0, y0, x1, y1;
	Screen_Rect(mesh, x0, y0, x1, y1);
	MeshDraw draw(mesh, renderer);
	if (!Light_Batch(effect, prepassTechnique, draw, x0, y0, x1, y1))
		return false;
	// the mesh's streams as the caller had them
	IDirect3DDevice9* device = DX8Wrapper::_Get_D3D_Device8()->Get_D3D9();
	device->SetVertexDeclaration(meshDeclaration);
	device->SetStreamSource(0, meshBuffer, 0, meshStride);
	Sage2GlobalsClass::Apply_Scene_Textures(effect);
	return true;
}

//-------------------------------------------------------------------------------------------------
bool Sage2SceneClass::Light_Batch(Sage2EffectClass& effect, int prepassTechnique, PrepassDrawClass& draw, float x0, float y0, float x1, float y1)
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
		Sage2_Log("deferred lighting: could not make the %ux%u buffers", (unsigned)desc.Width, (unsigned)desc.Height);
		target->Release();
		if (depth) depth->Release();
		return false;
	}
	Sage2GlobalsClass::Set_Frame_Buffer_Size(desc.Width, desc.Height);

	// Direct3D 9 resets the viewport (and scissor rectangle) to the whole target on every
	// SetRenderTarget; the scene's viewport may be smaller than the frame (the 3D view above the
	// control bar, letterboxing), so it is kept for every pass here and put back at the end, or
	// everything drawn after this mesh would land stretched and shifted
	D3DVIEWPORT9 viewport;
	device->GetViewport(&viewport);
	{
		static D3DVIEWPORT9 logged = { 0, 0, 0, 0, 0.0f, 0.0f };
		static unsigned loggedCount = 0;
		if (loggedCount < 8 && (logged.X != viewport.X || logged.Y != viewport.Y || logged.Width != viewport.Width || logged.Height != viewport.Height))
		{
			++loggedCount;
			logged = viewport;
			Sage2_Log("deferred lighting: scene viewport %u,%u %ux%u in a %ux%u frame",
				(unsigned)viewport.X, (unsigned)viewport.Y, (unsigned)viewport.Width, (unsigned)viewport.Height, (unsigned)desc.Width, (unsigned)desc.Height);
		}
	}
	RECT scissor = { 0, 0, 0, 0 };
	DWORD scissorEnabled = FALSE;
	device->GetRenderState(D3DRS_SCISSORTESTENABLE, &scissorEnabled);
	if (scissorEnabled)
		device->GetScissorRect(&scissor);

	IDirect3DSurface9* sNormalSpec = Surface(s_normalSpec);
	IDirect3DSurface9* sDepth = Surface(s_depth);
	IDirect3DSurface9* sDepthLinear = Surface(s_depthLinear);
	IDirect3DSurface9* sDiffuse = Surface(s_diffuseLight);
	IDirect3DSurface9* sSpecular = Surface(s_specularLight);

	// 1. the depth / normal pre-pass; every channel written (the specular power is the normal
	// buffer's alpha), whatever colour mask Zero Hour's own passes left
	DWORD savedColorWrite = 0xF, savedColorWrite1 = 0xF;
	device->GetRenderState(D3DRS_COLORWRITEENABLE, &savedColorWrite);
	device->GetRenderState(D3DRS_COLORWRITEENABLE1, &savedColorWrite1);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->SetRenderState(D3DRS_COLORWRITEENABLE1, 0xF);
	device->SetRenderTarget(0, sNormalSpec);
	device->SetRenderTarget(1, sDepth);
	device->SetDepthStencilSurface(s_prepassDepth);
	device->SetViewport(&viewport);
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
		draw.Draw(device);
		effect.End(device);
	}
	device->SetRenderTarget(1, nullptr);

	// 2. linear depth and the directional light over the rectangle
	Sage2DrawState drawState;
	Sage2GlobalsClass::Set_Scene_Textures(nullptr, nullptr, s_depthLinear, s_normalSpec);
	Sage2GlobalsClass::Set_Depth_Buffer_Texture(s_depth);
	Sage2GlobalsClass::Apply(*s_linearDepth, drawState);
	Sage2GlobalsClass::Apply(*s_lighting, drawState);
	device->SetDepthStencilSurface(nullptr);
	device->SetRenderTarget(0, sDepthLinear);
	device->SetViewport(&viewport);
	Draw_Rect(device, *s_linearDepth, s_linearDepthTechnique, x0, y0, x1, y1);
	device->SetRenderTarget(0, sDiffuse);
	device->SetRenderTarget(1, sSpecular);
	device->SetViewport(&viewport);
	Draw_Rect(device, *s_lighting, s_lightingTechnique, x0, y0, x1, y1);
	Draw_Point_Lights(device, x0, y0, x1, y1);
	device->SetRenderTarget(1, nullptr);

	// 3. back to the frame
	device->SetRenderState(D3DRS_COLORWRITEENABLE, savedColorWrite);
	device->SetRenderState(D3DRS_COLORWRITEENABLE1, savedColorWrite1);
	device->SetRenderTarget(0, target);
	device->SetDepthStencilSurface(depth);
	device->SetViewport(&viewport);
	if (scissorEnabled)
		device->SetScissorRect(&scissor);
	device->SetStreamSource(0, nullptr, 0, 0);
	Sage2GlobalsClass::Set_Scene_Textures(s_diffuseLight, s_specularLight, s_depthLinear, s_normalSpec);
	Sage2GlobalsClass::Apply_Scene_Textures(effect);

	sNormalSpec->Release(); sDepth->Release(); sDepthLinear->Release(); sDiffuse->Release(); sSpecular->Release();
	target->Release();
	if (depth) depth->Release();
	return true;
}

//-------------------------------------------------------------------------------------------------
void Sage2SceneClass::Clear_Point_Lights()
{
	s_pointLights.clear();
}

void Sage2SceneClass::Add_Point_Light(const PointLight& light)
{
	if (s_pointLights.size() < MAX_POINT_LIGHTS && light.outerRadius > 0.0f)
		s_pointLights.push_back(light);
}

unsigned Sage2SceneClass::Get_Point_Light_Count()
{
	return (unsigned)s_pointLights.size();
}

void Sage2SceneClass::Set_Point_Light_Strength(float strength)
{
	s_pointLightStrength = strength < 0.0f ? 0.0f : (strength > 4.0f ? 4.0f : strength);
}

float Sage2SceneClass::Get_Point_Light_Strength()
{
	return s_pointLightStrength;
}

void Sage2SceneClass::Get_Point_Light_Ambient(const float position[3], float ambient[3])
{
	ambient[0] = ambient[1] = ambient[2] = 0.0f;
	if (s_pointLightStrength <= 0.0f)
		return;
	for (size_t i = 0; i < s_pointLights.size(); ++i)
	{
		const PointLight& light = s_pointLights[i];
		if (!light.lightsObjects)
			continue;
		const float dx = position[0] - light.position[0], dy = position[1] - light.position[1], dz = position[2] - light.position[2];
		const float d2 = dx * dx + dy * dy + dz * dz;
		const float outer2 = light.outerRadius * light.outerRadius, inner2 = light.innerRadius * light.innerRadius;
		if (d2 >= outer2)
			continue;
		// C&C4's smooth step between the radii, half as strong as on the ground: it lights every side
		float t = outer2 > inner2 ? (outer2 - d2) / (outer2 - inner2) : 1.0f;
		t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
		const float k = t * t * (3.0f - 2.0f * t) * 0.5f * s_pointLightStrength;
		ambient[0] += light.color[0] * k;
		ambient[1] += light.color[1] * k;
		ambient[2] += light.color[2] * k;
	}
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
