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
#include "sage2shadow.h"
#include "sage2texture.h"
#include "sage2topview.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "dx8wrapper.h"
#include "WWMath/quat.h"
#include "ww3d.h"

void Sage2_Log(const char* format, ...)
{
	static FILE* file = nullptr;
	static int lines = 0;
	if (file == nullptr)
	{
		file = fopen("Sage2.log", "w");
		if (file == nullptr)
			return;
	}
	if (++lines > 400)	// enough to see what happened, never a runaway
		return;
	va_list args;
	va_start(args, format);
	vfprintf(file, format, args);
	va_end(args);
	fputc('\n', file);
	fflush(file);
}

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
		{ "ShadowMap", SAGE2_BIND_TEXTURE_SHADOW_MAP },
		{ "ShadowMapWorldToShadow", SAGE2_BIND_SHADOW_WORLD_TO_SHADOW },
		{ "Shadowmap_Zero_Zero_OneOverMapSize_OneOverMapSize", SAGE2_BIND_SHADOW_MAP_TEXEL },
		{ "SoftShadowParms", SAGE2_BIND_SOFT_SHADOW_PARMS },
		{ "TopViewTexture", SAGE2_BIND_TEXTURE_TOP_VIEW },
		{ "TerrainTopViewTexture", SAGE2_BIND_TEXTURE_TOP_VIEW },
		{ "ScorchMarkUVOffsetMultiplier", SAGE2_BIND_SCORCH_UV },
		{ "CloudTexture", SAGE2_BIND_TEXTURE_CLOUD },
		{ "CloudSampler", SAGE2_BIND_TEXTURE_CLOUD },
		{ "Cloud_WorldPositionMultiplier_XYZZ", SAGE2_BIND_CLOUD_MULTIPLIER },
		{ "Cloud_CurrentOffsetUV", SAGE2_BIND_CLOUD_OFFSET },
		{ "MacroSampler", SAGE2_BIND_TEXTURE_MACRO },
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
		{ "Sas.Shadow[0].ShadowMap", SAGE2_BIND_TEXTURE_SHADOW_MAP },
		{ "Sas.Shadow[0].WorldToShadow", SAGE2_BIND_SHADOW_WORLD_TO_SHADOW },
		{ "Sas.Shadow[0].Zero_Zero_OneOverMapSize_OneOverMapSize", SAGE2_BIND_SHADOW_MAP_TEXEL },
		{ "WW3D.SoftShadowParms", SAGE2_BIND_SOFT_SHADOW_PARMS },
		{ "Sas.HasShadow", SAGE2_BIND_HAS_SHADOW },
		{ "Terrain.TopViewTexture", SAGE2_BIND_TEXTURE_TOP_VIEW },
		{ "Terrain.ScorchMarkUVOffsetMultiplier", SAGE2_BIND_SCORCH_UV },
		{ "Terrain.Cloud.Texture", SAGE2_BIND_TEXTURE_CLOUD },
		{ "Terrain.Cloud.WorldPositionMultiplier_XYZZ", SAGE2_BIND_CLOUD_MULTIPLIER },
		{ "Terrain.Cloud.CurrentOffsetUV", SAGE2_BIND_CLOUD_OFFSET },
		{ "WW3D.GDI_Env_cube", SAGE2_BIND_TEXTURE_ENV_GDI },
		{ "WW3D.NOD_Env_cube", SAGE2_BIND_TEXTURE_ENV_NOD },
		{ "Objects.LightSpaceEnvironmentMap", SAGE2_BIND_TEXTURE_ENVIRONMENT },
		{ "Water.LightSpaceEnvironmentMap", SAGE2_BIND_TEXTURE_ENVIRONMENT },
		{ "Terrain.Macro.Texture", SAGE2_BIND_TEXTURE_MACRO },
		{ "Terrain.MacroTexture", SAGE2_BIND_TEXTURE_MACRO },
		{ "WW3D.Shroud", SAGE2_BIND_TEXTURE_WHITE },
		{ "Terrain.Shroud.Texture", SAGE2_BIND_TEXTURE_WHITE },	// the terrain effect sets the map's shroud over it
		{ "WW3D.EnvironmentTexture", SAGE2_BIND_TEXTURE_ENVIRONMENT },
		{ "WW3D.FactionEnvCube", SAGE2_BIND_TEXTURE_ENVIRONMENT },
	};

	bool s_linearLights = false;		// Sage2LinearLights
	std::string s_macroName = "tsnoiseurb";	// Sage2TerrainMacro, empty for none

	// a light colour (display values, as the map stores it) as the effects take it
	void Light_Color(float v[3])
	{
		if (!s_linearLights)
			return;
		for (int k = 0; k < 3; ++k)
			v[k] = v[k] > 0.0f ? powf(v[k], 2.2f) : 0.0f;
	}

	unsigned s_frameWidth = 0, s_frameHeight = 0;
	IDirect3DBaseTexture9* s_deferredDiffuse = nullptr;
	IDirect3DBaseTexture9* s_deferredSpecular = nullptr;
	IDirect3DBaseTexture9* s_depth = nullptr;
	IDirect3DBaseTexture9* s_normalSpec = nullptr;
	IDirect3DBaseTexture9* s_depthBuffer = nullptr;

	IDirect3DTexture9* s_white = nullptr;
	IDirect3DTexture9* s_black = nullptr;
	IDirect3DTexture9* s_grey = nullptr;
	IDirect3DTexture9* s_clear = nullptr;		// transparent black
	IDirect3DCubeTexture9* s_environment = nullptr;

	// the named C&C4 texture from the mod, or the stand-in when it is not there
	IDirect3DBaseTexture9* Texture_Or(const char* name, IDirect3DBaseTexture9* standIn)
	{
		IDirect3DBaseTexture9* t = Sage2TextureClass::Get(name);
		return t != nullptr ? t : standIn;
	}

	bool Apply_Scene_Texture(Sage2EffectClass& effect, int i, int binding)
	{
		switch (binding)
		{
			case SAGE2_BIND_TEXTURE_WHITE: effect.Set_Texture(i, s_white); return true;
			case SAGE2_BIND_TEXTURE_BLACK: effect.Set_Texture(i, s_black); return true;
			case SAGE2_BIND_TEXTURE_GREY: effect.Set_Texture(i, s_grey); return true;
			case SAGE2_BIND_TEXTURE_ENVIRONMENT: effect.Set_Texture(i, Texture_Or("evdefault", s_environment)); return true;
			case SAGE2_BIND_TEXTURE_ENV_GDI: effect.Set_Texture(i, Texture_Or("gdi_env_cube", s_environment)); return true;
			case SAGE2_BIND_TEXTURE_ENV_NOD: effect.Set_Texture(i, Texture_Or("nod_env_cube", s_environment)); return true;
			case SAGE2_BIND_TEXTURE_CLOUD: effect.Set_Texture(i, Texture_Or("tscloudmed", s_white)); return true;
			case SAGE2_BIND_TEXTURE_MACRO: effect.Set_Texture(i, s_macroName.empty() ? s_white : Texture_Or(s_macroName.c_str(), s_white)); return true;
			case SAGE2_BIND_TEXTURE_SHADOW_MAP: effect.Set_Texture(i, Sage2ShadowClass::Is_Available() ? Sage2ShadowClass::Get_Texture() : s_white); return true;
			case SAGE2_BIND_TEXTURE_TOP_VIEW: effect.Set_Texture(i, Sage2TopViewClass::Is_Available() ? Sage2TopViewClass::Get_Texture() : s_clear); return true;
			case SAGE2_BIND_TEXTURE_DEFERRED_DIFFUSE: effect.Set_Texture(i, s_deferredDiffuse ? s_deferredDiffuse : s_grey); return true;
			case SAGE2_BIND_TEXTURE_DEFERRED_SPECULAR: effect.Set_Texture(i, s_deferredSpecular ? s_deferredSpecular : s_black); return true;
			case SAGE2_BIND_TEXTURE_DEPTH: effect.Set_Texture(i, s_depth ? s_depth : s_white); return true;
			case SAGE2_BIND_TEXTURE_NORMAL_SPEC: effect.Set_Texture(i, s_normalSpec ? s_normalSpec : s_grey); return true;
			case SAGE2_BIND_TEXTURE_DEPTH_BUFFER: effect.Set_Texture(i, s_depthBuffer ? s_depthBuffer : s_white); return true;
			default: return false;
		}
	}

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
		s_clear = Make_Flat(device, 0x00000000);
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

	// Tiberium Project @bugfix The effects that undo the projection (the linear depth, the
	// deferred lights) build their clip position from a frame position (the pixel over the frame
	// size), as C&C4 draws the world over the whole frame. Zero Hour draws it in a viewport that
	// stops at the control bar, so the projection's clip space is the viewport's: this maps the
	// viewport's clip space into the frame's, to go in front of the projection before inverting.
	// False (and nothing to do) when the viewport is the whole target.
	bool Viewport_To_Frame(Matrix4x4& f)
	{
		IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
		if (device8 == nullptr)
			return false;
		IDirect3DDevice9* device = device8->Get_D3D9();
		D3DVIEWPORT9 vp;
		if (FAILED(device->GetViewport(&vp)) || vp.Width == 0 || vp.Height == 0)
			return false;
		IDirect3DSurface9* target = nullptr;
		if (FAILED(device->GetRenderTarget(0, &target)) || target == nullptr)
			return false;
		D3DSURFACE_DESC desc;
		HRESULT hr = target->GetDesc(&desc);
		target->Release();
		if (FAILED(hr) || desc.Width == 0 || desc.Height == 0)
			return false;
		if (vp.X == 0 && vp.Y == 0 && vp.Width == desc.Width && vp.Height == desc.Height)
			return false;
		const float fw = (float)desc.Width, fh = (float)desc.Height;
		const float ax = (float)vp.Width / fw, bx = (2.0f * (float)vp.X + (float)vp.Width) / fw - 1.0f;
		const float ay = (float)vp.Height / fh, by = 1.0f - (2.0f * (float)vp.Y + (float)vp.Height) / fh;
		f = Matrix4x4(Vector4(ax, 0.0f, 0.0f, bx),
			Vector4(0.0f, ay, 0.0f, by),
			Vector4(0.0f, 0.0f, 1.0f, 0.0f),
			Vector4(0.0f, 0.0f, 0.0f, 1.0f));
		return true;
	}

	// the map's lights (Set_Scene_Lights) and whether the draws now take them
	bool s_sceneLightsValid = false;
	bool s_useSceneLights = false;
	int s_sceneLightCount = 0;
	float s_sceneAmbient[3] = { 0.0f, 0.0f, 0.0f };
	float s_sceneDiffuse[3][3];
	float s_sceneToLight[3][3];

	bool Scene_Lights_Active()
	{
		return s_useSceneLights && s_sceneLightsValid;
	}

	// Tiberium Project @bugfix the device light that is the effect's n-th directional light. Zero
	// Hour's light environment ranks an object's lights by strength, an explosion's point light
	// among them; taken slot for slot, a point light took the sun's place (DirectionalLight_0, the
	// shadowed one) or a fill's with no direction at all, and the object went flat while it lasted
	int Directional_Device_Light(unsigned n)
	{
		unsigned found = 0;
		for (unsigned i = 0; i < 4; ++i)
			if (DX8Wrapper::Is_Light_Enabled(i) && DX8Wrapper::Peek_Light(i).Type == D3DLIGHT_DIRECTIONAL && found++ == n)
				return (int)i;
		return -1;
	}

	void Set_Light(Sage2EffectClass& effect, int param, unsigned light, bool color)
	{
		float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		const int device = Scene_Lights_Active() ? -1 : Directional_Device_Light(light);
		if (Scene_Lights_Active())
		{
			if ((int)light < s_sceneLightCount)
			{
				const float* src = color ? s_sceneDiffuse[light] : s_sceneToLight[light];
				v[0] = src[0]; v[1] = src[1]; v[2] = src[2]; v[3] = color ? 1.0f : 0.0f;
				if (color)
					Light_Color(v);
			}
		}
		else if (device >= 0)
		{
			const D3DLIGHT8& l = DX8Wrapper::Peek_Light((unsigned)device);
			if (color)
			{
				v[0] = l.Diffuse.r; v[1] = l.Diffuse.g; v[2] = l.Diffuse.b; v[3] = 1.0f;
				Light_Color(v);
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
void Sage2GlobalsClass::Set_Depth_Buffer_Texture(IDirect3DBaseTexture9* depthBuffer)
{
	s_depthBuffer = depthBuffer;
}

//-------------------------------------------------------------------------------------------------
void Sage2GlobalsClass::Apply_Scene_Textures(Sage2EffectClass& effect)
{
	Make_Stand_Ins();
	// Tiberium Project @bugfix only the buffers the deferred passes change: the stand-ins (white,
	// black, grey) are where the caller puts its own texture after Apply(), the terrain its shroud,
	// and re-binding them here undid that (the terrain showed no shroud and no fog of war)
	for (int i = 0; i < effect.Get_Param_Count(); ++i)
	{
		int binding = effect.Get_Param(i).binding;
		switch (binding)
		{
			case SAGE2_BIND_TEXTURE_DEFERRED_DIFFUSE:
			case SAGE2_BIND_TEXTURE_DEFERRED_SPECULAR:
			case SAGE2_BIND_TEXTURE_DEPTH:
			case SAGE2_BIND_TEXTURE_NORMAL_SPEC:
			case SAGE2_BIND_TEXTURE_DEPTH_BUFFER:
				Apply_Scene_Texture(effect, i, binding);
				break;
			default:
				break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
bool Sage2GlobalsClass::Is_Deferred_Lighting_Available()
{
	return s_deferredDiffuse != nullptr && s_deferredSpecular != nullptr;
}

//-------------------------------------------------------------------------------------------------
void Sage2GlobalsClass::Set_Scene_Lights(const float ambient[3], const float diffuse[][3], const float toLight[][3], int count)
{
	if (count < 0) count = 0;
	if (count > 3) count = 3;
	for (int c = 0; c < 3; ++c)
		s_sceneAmbient[c] = ambient[c];
	for (int l = 0; l < count; ++l)
	{
		Vector3 dir(toLight[l][0], toLight[l][1], toLight[l][2]);
		float length = dir.Length();
		if (length > 0.0f)
			dir /= length;
		for (int c = 0; c < 3; ++c)
			s_sceneDiffuse[l][c] = diffuse[l][c];
		s_sceneToLight[l][0] = dir.X; s_sceneToLight[l][1] = dir.Y; s_sceneToLight[l][2] = dir.Z;
	}
	s_sceneLightCount = count;
	s_sceneLightsValid = true;
}

//-------------------------------------------------------------------------------------------------
bool Sage2GlobalsClass::Use_Scene_Lights(bool use)
{
	bool previous = s_useSceneLights;
	s_useSceneLights = use;
	return previous;
}

//-------------------------------------------------------------------------------------------------
Sage2SceneLightsScope::Sage2SceneLightsScope() : m_previous(Sage2GlobalsClass::Use_Scene_Lights(true)) {}
Sage2SceneLightsScope::~Sage2SceneLightsScope() { Sage2GlobalsClass::Use_Scene_Lights(m_previous); }

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

	// the first draws, for Sage2.log: what the scene feeds the effects
	static int logged = 0;
	if (logged < 6)
	{
		++logged;
		DWORD a = DX8Wrapper::Get_DX8_Render_State(D3DRS_AMBIENT);
		Sage2_Log("draw %d: world rows (%.3f %.3f %.3f | %.1f) (%.3f %.3f %.3f | %.1f) (%.3f %.3f %.3f | %.1f) scale %.3f%s, eye (%.1f %.1f %.1f), ambient %08x",
			logged, world[0][0], world[0][1], world[0][2], world[0][3], world[1][0], world[1][1], world[1][2], world[1][3],
			world[2][0], world[2][1], world[2][2], world[2][3], scale, scaled ? " (World path)" : "", eye.X, eye.Y, eye.Z, (unsigned)a);
		for (unsigned l = 0; l < 4; ++l)
		{
			if (!DX8Wrapper::Is_Light_Enabled(l))
				continue;
			const D3DLIGHT8& light = DX8Wrapper::Peek_Light(l);
			Sage2_Log("  light %u: type %d colour (%.2f %.2f %.2f) direction (%.2f %.2f %.2f)", l, (int)light.Type,
				light.Diffuse.r, light.Diffuse.g, light.Diffuse.b, light.Direction.x, light.Direction.y, light.Direction.z);
		}
		Sage2_Log("  opacity %.2f, house colour %06x, damage %.2f, projection row 0 (%.3f %.3f %.3f %.3f) row 2 (%.3f %.3f %.3f %.3f)",
			draw.opacity, draw.houseColor, draw.damage, projection[0][0], projection[0][1], projection[0][2], projection[0][3],
			projection[2][0], projection[2][1], projection[2][2], projection[2][3]);
	}

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
			{
				Matrix4x4 toFrame;
				if (Viewport_To_Frame(toFrame))
					Set_Matrix4_Rows(effect, i, (toFrame * projection).Inverse());
				else
					Set_Matrix4_Rows(effect, i, projection.Inverse());
				break;
			}
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
				if (Scene_Lights_Active())
				{
					v[0] = s_sceneAmbient[0]; v[1] = s_sceneAmbient[1]; v[2] = s_sceneAmbient[2];
				}
				else
				{
					DWORD a = DX8Wrapper::Get_DX8_Render_State(D3DRS_AMBIENT);
					v[0] = ((a >> 16) & 0xFF) / 255.0f; v[1] = ((a >> 8) & 0xFF) / 255.0f; v[2] = (a & 0xFF) / 255.0f;
				}
				Light_Color(v);
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
			case SAGE2_BIND_FOG:
				effect.Set_Float(i, v, 4);
				break;
			case SAGE2_BIND_IS_SHADOW_MAP_ENABLED:
			case SAGE2_BIND_HAS_SHADOW:
				v[0] = Sage2ShadowClass::Is_Available() ? 1.0f : 0.0f;
				effect.Set_Float(i, v, 1);
				break;
			case SAGE2_BIND_SHADOW_WORLD_TO_SHADOW:
				if (Sage2ShadowClass::Is_Available())
					effect.Set_Matrix(p.name.c_str(), Sage2ShadowClass::Get_World_To_Shadow());
				break;
			case SAGE2_BIND_SHADOW_MAP_TEXEL:
			{
				float size = (float)Sage2ShadowClass::Get_Map_Size();
				v[0] = v[1] = 0.0f;
				v[2] = v[3] = size > 0.0f ? 1.0f / size : 0.0f;
				effect.Set_Float(i, v, 4);
				break;
			}
			case SAGE2_BIND_SOFT_SHADOW_PARMS:
				Sage2ShadowClass::Get_Soft(v[0], v[1]);
				v[2] = 1.0f;	// ENABLE_SOFTSHADOW_TOGGLE builds: soft on
				effect.Set_Float(i, v, 4);
				break;
			case SAGE2_BIND_SCORCH_UV:
				if (Sage2TopViewClass::Is_Available())
					Sage2TopViewClass::Get_Mapping(v[0], v[1], v[2], v[3]);
				effect.Set_Float(i, v, 4);
				break;
			case SAGE2_BIND_CLOUD_MULTIPLIER:
				// C&C4's cloud layer: the texture repeats about every 1600 world units
				v[0] = v[1] = 1.0f / 1600.0f; v[2] = v[3] = 0.0f;
				effect.Set_Float(i, v, 4);
				break;
			case SAGE2_BIND_CLOUD_OFFSET:
			{
				float t = WW3D::Get_Sync_Time() * 0.001f;
				v[0] = t * 0.0035f; v[1] = t * 0.0015f;
				effect.Set_Float(i, v, 4);
				break;
			}
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
			default:
				Apply_Scene_Texture(effect, i, p.binding);
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
	if (s_clear) s_clear->Release();
	if (s_environment) s_environment->Release();
	s_white = s_black = s_grey = s_clear = nullptr;
	s_environment = nullptr;
	s_standInsTried = false;
	s_deferredDiffuse = s_deferredSpecular = s_depth = s_normalSpec = s_depthBuffer = nullptr;
}

//-------------------------------------------------------------------------------------------------
void Sage2GlobalsClass::Set_Linear_Lights(bool linear)
{
	s_linearLights = linear;
	Sage2_Log("globals: light colours to the effects %s", linear ? "in linear light (the maps' colours to the power 2.2, as RA3 and C&C4)" : "as the maps store them");
}

bool Sage2GlobalsClass::Get_Linear_Lights() { return s_linearLights; }

//-------------------------------------------------------------------------------------------------
void Sage2GlobalsClass::Set_Macro_Texture(const char* name)
{
	s_macroName.clear();
	if (name != nullptr && *name != '\0' && strcmp(name, "none") != 0 && strcmp(name, "None") != 0)
		for (const char* c = name; *c; ++c)
			s_macroName += (char)((*c >= 'A' && *c <= 'Z') ? *c + ('a' - 'A') : *c);
	Sage2_Log("globals: terrain macro texture %s", s_macroName.empty() ? "none" : s_macroName.c_str());
}
