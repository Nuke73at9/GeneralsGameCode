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

// Tiberium Project @feature The engine-side values SAGE 2 effects ask for (see sage2globals.h)

#include "sage2globals.h"

#include <string.h>

#include "dx8wrapper.h"
#include "WWMath/quat.h"
#include "ww3d.h"

namespace
{
	struct NameBinding { const char* name; int binding; };

	// parameter names shared by all SAGE 2 effects (Common.fxh, GlobalParameters.fxh, Skinning.fxh,
	// DeferredLighting.fxh ...)
	const NameBinding NAME_BINDINGS[] =
	{
		{ "World", SAGE2_BIND_WORLD },
		{ "View", SAGE2_BIND_VIEW },
		{ "Projection", SAGE2_BIND_PROJECTION },
		{ "ViewProjection", SAGE2_BIND_VIEW_PROJECTION },
		{ "WorldView", SAGE2_BIND_WORLD_VIEW },
		{ "WorldViewProjection", SAGE2_BIND_WORLD_VIEW_PROJECTION },
		{ "ViewI", SAGE2_BIND_VIEW_INVERSE },
		{ "ProjectionI", SAGE2_BIND_PROJECTION_INVERSE },
		{ "EyePosition", SAGE2_BIND_EYE_POSITION },
		{ "WorldBones", SAGE2_BIND_WORLD_BONES },
		{ "NumJointsPerVertex", SAGE2_BIND_NUM_JOINTS_PER_VERTEX },
		{ "AmbientLightColor", SAGE2_BIND_AMBIENT },
		{ "DirectionalLight_0_Color", SAGE2_BIND_DIRECTIONAL_0_COLOR },
		{ "DirectionalLight_0_Direction", SAGE2_BIND_DIRECTIONAL_0_DIRECTION },
		{ "DirectionalLight_1_Color", SAGE2_BIND_DIRECTIONAL_1_COLOR },
		{ "DirectionalLight_1_Direction", SAGE2_BIND_DIRECTIONAL_1_DIRECTION },
		{ "DirectionalLight_2_Color", SAGE2_BIND_DIRECTIONAL_2_COLOR },
		{ "DirectionalLight_2_Direction", SAGE2_BIND_DIRECTIONAL_2_DIRECTION },
		{ "NumPointLights", SAGE2_BIND_NUM_POINT_LIGHTS },
		{ "OpacityOverride", SAGE2_BIND_OPACITY_OVERRIDE },
		{ "RecolorColor", SAGE2_BIND_RECOLOR_COLOR },
		{ "RecolorSecondaryColor", SAGE2_BIND_RECOLOR_SECONDARY },
		{ "HasRecolorColors", SAGE2_BIND_HAS_RECOLOR_COLORS },
		{ "TintColor", SAGE2_BIND_TINT_COLOR },
		{ "HouseColorBloomIntensity", SAGE2_BIND_HOUSE_COLOR_BLOOM },
		{ "FlashPct", SAGE2_BIND_FLASH_PCT },
		{ "StealthEnabled", SAGE2_BIND_STEALTH_ENABLED },
		{ "RenderBuffEffectA", SAGE2_BIND_RENDER_BUFF_A },
		{ "RenderBuffEffectB", SAGE2_BIND_RENDER_BUFF_B },
		{ "Time", SAGE2_BIND_TIME },
		{ "OneOverFrameBufferSize", SAGE2_BIND_ONE_OVER_FRAME_BUFFER_SIZE },
		{ "NoCloudMultiplier", SAGE2_BIND_NO_CLOUD_MULTIPLIER },
		{ "IsShadowMapEnabled", SAGE2_BIND_IS_SHADOW_MAP_ENABLED },
		{ "HasShadow", SAGE2_BIND_HAS_SHADOW },
		{ "Fog", SAGE2_BIND_FOG },
		{ "ShadowMap", SAGE2_BIND_TEXTURE_WHITE },
		{ "CloudTexture", SAGE2_BIND_TEXTURE_WHITE },
		{ "CloudSampler", SAGE2_BIND_TEXTURE_WHITE },
		{ "MacroSampler", SAGE2_BIND_TEXTURE_WHITE },
		{ "ShroudTexture", SAGE2_BIND_TEXTURE_WHITE },
		{ "Noise", SAGE2_BIND_TEXTURE_GREY },
		{ "FilteredNoise", SAGE2_BIND_TEXTURE_GREY },
		{ "RecolorScrollTexture", SAGE2_BIND_TEXTURE_GREY },
		{ "Buff_HexGrid", SAGE2_BIND_TEXTURE_BLACK },
		{ "Buff_ShieldRamp", SAGE2_BIND_TEXTURE_BLACK },
		{ "EnvironmentTexture", SAGE2_BIND_TEXTURE_ENVIRONMENT },
		{ "FactionEnvCube", SAGE2_BIND_TEXTURE_ENVIRONMENT },
		{ "DeferredDiffuseLightTexture", SAGE2_BIND_TEXTURE_DEFERRED_DIFFUSE },
		{ "DeferredSpecularLightTexture", SAGE2_BIND_TEXTURE_DEFERRED_SPECULAR },
		{ "DepthTexture", SAGE2_BIND_TEXTURE_DEPTH },
		{ "NormalSpecTexture", SAGE2_BIND_TEXTURE_NORMAL_SPEC },
		{ "DepthBufferSampler", SAGE2_BIND_TEXTURE_DEPTH_BUFFER },
	};

	// SAS bind addresses, for parameters an effect names differently
	const NameBinding SAS_BINDINGS[] =
	{
		{ "Sas.Camera.Position", SAGE2_BIND_EYE_POSITION },
		{ "Sas.Camera.WorldToView", SAGE2_BIND_VIEW },
		{ "Sas.Camera.Projection", SAGE2_BIND_PROJECTION },
		{ "Sas.Time.Now", SAGE2_BIND_TIME },
		{ "Sas.ShadowMappingEnabled", SAGE2_BIND_IS_SHADOW_MAP_ENABLED },
		{ "WW3D.Time", SAGE2_BIND_TIME },
		{ "WW3D.OneOverFrameBufferSize", SAGE2_BIND_ONE_OVER_FRAME_BUFFER_SIZE },
		{ "WW3D.OpacityOverride", SAGE2_BIND_OPACITY_OVERRIDE },
		{ "WW3D.DeferredLighting.DiffuseLightTexture", SAGE2_BIND_TEXTURE_DEFERRED_DIFFUSE },
		{ "WW3D.DeferredLighting.SpecularLightTexture", SAGE2_BIND_TEXTURE_DEFERRED_SPECULAR },
		{ "WW3D.DeferredLighting.NormalSpec", SAGE2_BIND_TEXTURE_NORMAL_SPEC },
		{ "WW3D.DepthTexture", SAGE2_BIND_TEXTURE_DEPTH },
		{ "PostEffect.DepthBufferTexture", SAGE2_BIND_TEXTURE_DEPTH_BUFFER },
		{ "WW3D.FilteredNoise", SAGE2_BIND_TEXTURE_GREY },
		{ "WW3D.Noise", SAGE2_BIND_TEXTURE_GREY },
		{ "Sas.Shadow[0].ShadowMap", SAGE2_BIND_TEXTURE_WHITE },
		{ "Terrain.Cloud.Texture", SAGE2_BIND_TEXTURE_WHITE },
		{ "Terrain.Macro.Texture", SAGE2_BIND_TEXTURE_WHITE },
		{ "WW3D.Shroud", SAGE2_BIND_TEXTURE_WHITE },
		{ "WW3D.EnvironmentTexture", SAGE2_BIND_TEXTURE_ENVIRONMENT },
		{ "WW3D.FactionEnvCube", SAGE2_BIND_TEXTURE_ENVIRONMENT },
	};

	unsigned s_frameWidth = 0, s_frameHeight = 0;
	IDirect3DBaseTexture9* s_deferredDiffuse = nullptr;
	IDirect3DBaseTexture9* s_deferredSpecular = nullptr;
	IDirect3DBaseTexture9* s_depth = nullptr;
	IDirect3DBaseTexture9* s_normalSpec = nullptr;

	IDirect3DTexture9* s_white = nullptr;
	IDirect3DTexture9* s_black = nullptr;
	IDirect3DTexture9* s_grey = nullptr;
	IDirect3DCubeTexture9* s_environment = nullptr;
	bool s_standInsTried = false;

	IDirect3DTexture9* Make_Flat(IDirect3DDevice9* device, DWORD argb)
	{
		IDirect3DTexture9* t = nullptr;
		if (FAILED(device->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, nullptr)))
			return nullptr;
		D3DLOCKED_RECT lr;
		if (SUCCEEDED(t->LockRect(0, &lr, nullptr, 0)))
		{
			for (int y = 0; y < 4; ++y)
				for (int x = 0; x < 4; ++x)
					((DWORD*)((char*)lr.pBits + y * lr.Pitch))[x] = argb;
			t->UnlockRect(0);
		}
		return t;
	}

	IDirect3DCubeTexture9* Make_Environment(IDirect3DDevice9* device)
	{
		// a plain sky-above / ground-below cube until the real environment maps are in
		IDirect3DCubeTexture9* t = nullptr;
		if (FAILED(device->CreateCubeTexture(4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, nullptr)))
			return nullptr;
		static const DWORD faceColor[6] = { 0xFF706858, 0xFF706858, 0xFF8A98A8, 0xFF403828, 0xFF706858, 0xFF706858 };
		for (int f = 0; f < 6; ++f)
		{
			D3DLOCKED_RECT lr;
			if (FAILED(t->LockRect((D3DCUBEMAP_FACES)f, 0, &lr, nullptr, 0)))
				continue;
			for (int y = 0; y < 4; ++y)
				for (int x = 0; x < 4; ++x)
					((DWORD*)((char*)lr.pBits + y * lr.Pitch))[x] = faceColor[f];
			t->UnlockRect((D3DCUBEMAP_FACES)f, 0);
		}
		return t;
	}

	void Make_Stand_Ins()
	{
		if (s_standInsTried)
			return;
		s_standInsTried = true;
		IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
		if (device8 == nullptr)
			return;
		IDirect3DDevice9* device = device8->Get_D3D9();
		s_white = Make_Flat(device, 0xFFFFFFFF);
		s_black = Make_Flat(device, 0xFF000000);
		s_grey = Make_Flat(device, 0xFF808080);
		s_environment = Make_Environment(device);
	}

	void Set_Matrix3D_Rows(Sage2EffectClass& effect, int param, const Matrix3D& m)
	{
		// the effect keeps World as three registers = the rows of the Westwood matrix
		float rows[16];
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 4; ++c)
				rows[r * 4 + c] = m[r][c];
		rows[12] = rows[13] = rows[14] = 0.0f;
		rows[15] = 1.0f;
		effect.Set_Matrix(effect.Get_Param(param).name.c_str(), rows);
	}

	void Set_Matrix4_Rows(Sage2EffectClass& effect, int param, const Matrix4x4& m)
	{
		effect.Set_Matrix(effect.Get_Param(param).name.c_str(), &m[0][0]);
	}

	void Set_Light(Sage2EffectClass& effect, int param, unsigned light, bool color)
	{
		float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		if (DX8Wrapper::Is_Light_Enabled(light))
		{
			const D3DLIGHT8& l = DX8Wrapper::Peek_Light(light);
			if (color)
			{
				v[0] = l.Diffuse.r; v[1] = l.Diffuse.g; v[2] = l.Diffuse.b; v[3] = 1.0f;
			}
			else if (l.Type == D3DLIGHT_DIRECTIONAL)
			{
				// SAGE 2 lights point at the light; Direct3D lights point along the light
				Vector3 toLight(-l.Direction.x, -l.Direction.y, -l.Direction.z);
				toLight.Normalize();
				v[0] = toLight.X; v[1] = toLight.Y; v[2] = toLight.Z;
			}
		}
		effect.Set_Float(param, v, 4);
	}
}

//-------------------------------------------------------------------------------------------------
int Sage2GlobalsClass::Resolve(const Sage2EffectClass::Param& param)
{
	const char* sas = param.Bind_Address();
	if (sas != nullptr && sas[0] != '\0')
	{
		for (size_t i = 0; i < sizeof(SAS_BINDINGS) / sizeof(SAS_BINDINGS[0]); ++i)
			if (strcmp(SAS_BINDINGS[i].name, sas) == 0)
				return SAS_BINDINGS[i].binding;
	}
	for (size_t i = 0; i < sizeof(NAME_BINDINGS) / sizeof(NAME_BINDINGS[0]); ++i)
		if (param.name == NAME_BINDINGS[i].name)
			return NAME_BINDINGS[i].binding;
	return SAGE2_BIND_NONE;
}

//-------------------------------------------------------------------------------------------------
void Sage2GlobalsClass::Resolve_All(Sage2EffectClass& effect)
{
	for (int i = 0; i < effect.Get_Param_Count(); ++i)
		effect.Set_Binding(i, Resolve(effect.Get_Param(i)));
}

//-------------------------------------------------------------------------------------------------
void Sage2GlobalsClass::Set_Frame_Buffer_Size(unsigned width, unsigned height)
{
	s_frameWidth = width;
	s_frameHeight = height;
}

//-------------------------------------------------------------------------------------------------
void Sage2GlobalsClass::Set_Scene_Textures(IDirect3DBaseTexture9* deferredDiffuse, IDirect3DBaseTexture9* deferredSpecular,
	IDirect3DBaseTexture9* depth, IDirect3DBaseTexture9* normalSpec)
{
	s_deferredDiffuse = deferredDiffuse;
	s_deferredSpecular = deferredSpecular;
	s_depth = depth;
	s_normalSpec = normalSpec;
}

//-------------------------------------------------------------------------------------------------
bool Sage2GlobalsClass::Is_Deferred_Lighting_Available()
{
	return s_deferredDiffuse != nullptr && s_deferredSpecular != nullptr;
}

//-------------------------------------------------------------------------------------------------
void Sage2GlobalsClass::Apply(Sage2EffectClass& effect, const Sage2DrawState& draw)
{
	Make_Stand_Ins();

	// transforms, Westwood convention (column vectors); the effects want the rows as registers
	Matrix4x4 world4, view, projection;
	DX8Wrapper::Get_Transform(D3DTS_WORLD, world4);
	DX8Wrapper::Get_Transform(D3DTS_VIEW, view);
	DX8Wrapper::Get_Transform(D3DTS_PROJECTION, projection);
	Matrix3D world(world4[0][0], world4[0][1], world4[0][2], world4[0][3],
		world4[1][0], world4[1][1], world4[1][2], world4[1][3],
		world4[2][0], world4[2][1], world4[2][2], world4[2][3]);
	Matrix4x4 viewInverse = view.Inverse();
	Vector3 eye(viewInverse[0][3], viewInverse[1][3], viewInverse[2][3]);

	// the mesh as C&C4 skins everything: one bone, rotation as a quaternion, translation, and the
	// bone's opacity, which the damage shaders read as "how intact". A scaled transform can't be a
	// quaternion, so such meshes go through World instead (and show no damage).
	Vector3 axisX(world[0][0], world[1][0], world[2][0]);
	float scale = axisX.Length();
	bool scaled = scale < 0.999f || scale > 1.001f;

	for (int i = 0; i < effect.Get_Param_Count(); ++i)
	{
		const Sage2EffectClass::Param& p = effect.Get_Param(i);
		float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		switch (p.binding)
		{
			case SAGE2_BIND_NONE:
				break;
			case SAGE2_BIND_WORLD:
				Set_Matrix3D_Rows(effect, i, world);
				break;
			case SAGE2_BIND_VIEW:
				Set_Matrix4_Rows(effect, i, view);
				break;
			case SAGE2_BIND_PROJECTION:
				Set_Matrix4_Rows(effect, i, projection);
				break;
			case SAGE2_BIND_VIEW_PROJECTION:
				Set_Matrix4_Rows(effect, i, projection * view);
				break;
			case SAGE2_BIND_WORLD_VIEW:
				Set_Matrix4_Rows(effect, i, view * world4);
				break;
			case SAGE2_BIND_WORLD_VIEW_PROJECTION:
				Set_Matrix4_Rows(effect, i, projection * view * world4);
				break;
			case SAGE2_BIND_VIEW_INVERSE:
				Set_Matrix4_Rows(effect, i, viewInverse);
				break;
			case SAGE2_BIND_PROJECTION_INVERSE:
				Set_Matrix4_Rows(effect, i, projection.Inverse());
				break;
			case SAGE2_BIND_EYE_POSITION:
				v[0] = eye.X; v[1] = eye.Y; v[2] = eye.Z;
				effect.Set_Float(i, v, 3);
				break;
			case SAGE2_BIND_WORLD_BONES:
			{
				float bones[8];
				if (scaled)
				{
					bones[0] = bones[1] = bones[2] = 0.0f; bones[3] = 1.0f;
					bones[4] = bones[5] = bones[6] = 0.0f;
				}
				else
				{
					Quaternion q = Build_Quaternion(world);
					bones[0] = q.X; bones[1] = q.Y; bones[2] = q.Z; bones[3] = q.W;
					bones[4] = world[0][3]; bones[5] = world[1][3]; bones[6] = world[2][3];
				}
				bones[7] = 1.0f - draw.damage;
				effect.Set_Float(i, bones, 8);
				break;
			}
			case SAGE2_BIND_NUM_JOINTS_PER_VERTEX:
				v[0] = scaled ? 0.0f : 1.0f;
				effect.Set_Float(i, v, 1);
				break;
			case SAGE2_BIND_AMBIENT:
			{
				DWORD a = DX8Wrapper::Get_DX8_Render_State(D3DRS_AMBIENT);
				v[0] = ((a >> 16) & 0xFF) / 255.0f; v[1] = ((a >> 8) & 0xFF) / 255.0f; v[2] = (a & 0xFF) / 255.0f;
				effect.Set_Float(i, v, 3);
				break;
			}
			case SAGE2_BIND_DIRECTIONAL_0_COLOR: Set_Light(effect, i, 0, true); break;
			case SAGE2_BIND_DIRECTIONAL_0_DIRECTION: Set_Light(effect, i, 0, false); break;
			case SAGE2_BIND_DIRECTIONAL_1_COLOR: Set_Light(effect, i, 1, true); break;
			case SAGE2_BIND_DIRECTIONAL_1_DIRECTION: Set_Light(effect, i, 1, false); break;
			case SAGE2_BIND_DIRECTIONAL_2_COLOR: Set_Light(effect, i, 2, true); break;
			case SAGE2_BIND_DIRECTIONAL_2_DIRECTION: Set_Light(effect, i, 2, false); break;
			case SAGE2_BIND_NUM_POINT_LIGHTS:
				effect.Set_Float(i, v, 1);
				break;
			case SAGE2_BIND_OPACITY_OVERRIDE:
				v[0] = draw.opacity;
				effect.Set_Float(i, v, 1);
				break;
			case SAGE2_BIND_RECOLOR_COLOR:
				v[0] = ((draw.houseColor >> 16) & 0xFF) / 255.0f;
				v[1] = ((draw.houseColor >> 8) & 0xFF) / 255.0f;
				v[2] = (draw.houseColor & 0xFF) / 255.0f;
				effect.Set_Float(i, v, 3);
				break;
			case SAGE2_BIND_RECOLOR_SECONDARY:
				v[0] = v[1] = v[2] = 1.0f;
				effect.Set_Float(i, v, 3);
				break;
			case SAGE2_BIND_HAS_RECOLOR_COLORS:
				v[0] = draw.houseColor != 0 ? 1.0f : 0.0f;
				effect.Set_Float(i, v, 1);
				break;
			case SAGE2_BIND_TINT_COLOR:
			case SAGE2_BIND_NO_CLOUD_MULTIPLIER:
				v[0] = v[1] = v[2] = 1.0f;
				effect.Set_Float(i, v, 3);
				break;
			case SAGE2_BIND_HOUSE_COLOR_BLOOM:
				v[0] = v[1] = v[2] = v[3] = 1.0f;
				effect.Set_Float(i, v, 4);
				break;
			case SAGE2_BIND_FLASH_PCT:
				v[0] = 1.0f;
				effect.Set_Float(i, v, 1);
				break;
			case SAGE2_BIND_STEALTH_ENABLED:
				v[0] = draw.stealth ? 1.0f : 0.0f;
				effect.Set_Float(i, v, 1);
				break;
			case SAGE2_BIND_RENDER_BUFF_A:
			case SAGE2_BIND_RENDER_BUFF_B:
			case SAGE2_BIND_IS_SHADOW_MAP_ENABLED:
			case SAGE2_BIND_HAS_SHADOW:
			case SAGE2_BIND_FOG:
				effect.Set_Float(i, v, 4);
				break;
			case SAGE2_BIND_TIME:
				v[0] = WW3D::Get_Sync_Time() * 0.001f;
				effect.Set_Float(i, v, 1);
				break;
			case SAGE2_BIND_ONE_OVER_FRAME_BUFFER_SIZE:
			{
				unsigned w = s_frameWidth, h = s_frameHeight;
				if (w == 0 || h == 0)
				{
					int rw = 0, rh = 0, bits = 0;
					bool windowed = false;
					WW3D::Get_Render_Target_Resolution(rw, rh, bits, windowed);
					w = (unsigned)rw;
					h = (unsigned)rh;
				}
				v[0] = w ? 1.0f / w : 0.0f;
				v[1] = h ? 1.0f / h : 0.0f;
				effect.Set_Float(i, v, 2);
				break;
			}
			case SAGE2_BIND_TEXTURE_WHITE: effect.Set_Texture(i, s_white); break;
			case SAGE2_BIND_TEXTURE_BLACK: effect.Set_Texture(i, s_black); break;
			case SAGE2_BIND_TEXTURE_GREY: effect.Set_Texture(i, s_grey); break;
			case SAGE2_BIND_TEXTURE_ENVIRONMENT: effect.Set_Texture(i, s_environment); break;
			case SAGE2_BIND_TEXTURE_DEFERRED_DIFFUSE: effect.Set_Texture(i, s_deferredDiffuse ? s_deferredDiffuse : s_grey); break;
			case SAGE2_BIND_TEXTURE_DEFERRED_SPECULAR: effect.Set_Texture(i, s_deferredSpecular ? s_deferredSpecular : s_black); break;
			case SAGE2_BIND_TEXTURE_DEPTH: effect.Set_Texture(i, s_depth ? s_depth : s_white); break;
			case SAGE2_BIND_TEXTURE_NORMAL_SPEC: effect.Set_Texture(i, s_normalSpec ? s_normalSpec : s_grey); break;
			case SAGE2_BIND_TEXTURE_DEPTH_BUFFER: effect.Set_Texture(i, s_depth ? s_depth : s_white); break;
			default:
				break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void Sage2GlobalsClass::Shutdown()
{
	if (s_white) s_white->Release();
	if (s_black) s_black->Release();
	if (s_grey) s_grey->Release();
	if (s_environment) s_environment->Release();
	s_white = s_black = s_grey = nullptr;
	s_environment = nullptr;
	s_standInsTried = false;
	s_deferredDiffuse = s_deferredSpecular = s_depth = s_normalSpec = nullptr;
}
