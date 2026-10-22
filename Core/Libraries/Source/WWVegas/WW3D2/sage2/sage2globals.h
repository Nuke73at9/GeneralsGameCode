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

// Tiberium Project @feature The engine-side values SAGE 2 effects ask for.
//
// Every C&C4 / RA3 effect names the engine values it wants by parameter name (World,
// ViewProjection, EyePosition, DirectionalLight_0_Color, WorldBones, Time ...) and, for most of
// them, a DirectX SAS "SasBindAddress" annotation. Resolve() turns a parameter into one of the
// bindings below once, when the effect is loaded; Apply() then fills every bound parameter from
// W3D's state (transforms, lights, fog, time) and the draw's own values (house colour, opacity,
// damage) before each draw.

#pragma once

#include "sage2effect.h"
#include "WWMath/matrix3d.h"
#include "WWMath/matrix4.h"

class TextureClass;

enum Sage2Binding
{
	SAGE2_BIND_NONE = 0,

	// transforms
	SAGE2_BIND_WORLD,					// float4x3, the mesh transform
	SAGE2_BIND_VIEW,
	SAGE2_BIND_PROJECTION,
	SAGE2_BIND_VIEW_PROJECTION,
	SAGE2_BIND_WORLD_VIEW,
	SAGE2_BIND_WORLD_VIEW_PROJECTION,
	SAGE2_BIND_VIEW_INVERSE,
	SAGE2_BIND_PROJECTION_INVERSE,
	SAGE2_BIND_EYE_POSITION,
	SAGE2_BIND_WORLD_BONES,				// quaternion + translation / opacity per bone
	SAGE2_BIND_NUM_JOINTS_PER_VERTEX,

	// lights
	SAGE2_BIND_AMBIENT,
	SAGE2_BIND_DIRECTIONAL_0_COLOR,
	SAGE2_BIND_DIRECTIONAL_0_DIRECTION,
	SAGE2_BIND_DIRECTIONAL_1_COLOR,
	SAGE2_BIND_DIRECTIONAL_1_DIRECTION,
	SAGE2_BIND_DIRECTIONAL_2_COLOR,
	SAGE2_BIND_DIRECTIONAL_2_DIRECTION,
	SAGE2_BIND_NUM_POINT_LIGHTS,

	// per draw
	SAGE2_BIND_OPACITY_OVERRIDE,
	SAGE2_BIND_RECOLOR_COLOR,
	SAGE2_BIND_RECOLOR_SECONDARY,
	SAGE2_BIND_HAS_RECOLOR_COLORS,
	SAGE2_BIND_TINT_COLOR,
	SAGE2_BIND_HOUSE_COLOR_BLOOM,
	SAGE2_BIND_FLASH_PCT,
	SAGE2_BIND_STEALTH_ENABLED,
	SAGE2_BIND_RENDER_BUFF_A,
	SAGE2_BIND_RENDER_BUFF_B,

	// scene
	SAGE2_BIND_TIME,
	SAGE2_BIND_ONE_OVER_FRAME_BUFFER_SIZE,
	SAGE2_BIND_NO_CLOUD_MULTIPLIER,
	SAGE2_BIND_IS_SHADOW_MAP_ENABLED,
	SAGE2_BIND_HAS_SHADOW,
	SAGE2_BIND_FOG,						// zero while W3D fog is off
	SAGE2_BIND_CLOUD_MULTIPLIER,		// Cloud_WorldPositionMultiplier_XYZZ: world position to cloud texture coordinates
	SAGE2_BIND_CLOUD_OFFSET,			// Cloud_CurrentOffsetUV: the clouds' drift
	SAGE2_BIND_SHADOW_WORLD_TO_SHADOW,	// ShadowMapWorldToShadow: world -> shadow map texture coordinates and depth
	SAGE2_BIND_SHADOW_MAP_TEXEL,		// Shadowmap_Zero_Zero_OneOverMapSize_OneOverMapSize
	SAGE2_BIND_SOFT_SHADOW_PARMS,		// SoftShadowParms: kernel size, jitter
	SAGE2_BIND_SCORCH_UV,				// ScorchMarkUVOffsetMultiplier: world xy -> the terrain top view's texel

	// textures the scene provides (stand-ins until the matching passes exist)
	SAGE2_BIND_TEXTURE_WHITE,			// shadow map, cloud, macro, shroud: "no effect"
	SAGE2_BIND_TEXTURE_BLACK,
	SAGE2_BIND_TEXTURE_GREY,			// noise
	SAGE2_BIND_TEXTURE_ENVIRONMENT,		// reflection cube (C&C4's EVDefault)
	SAGE2_BIND_TEXTURE_ENV_GDI,			// GDI faction reflection cube
	SAGE2_BIND_TEXTURE_ENV_NOD,			// Nod faction reflection cube
	SAGE2_BIND_TEXTURE_CLOUD,			// cloud shadow layer
	SAGE2_BIND_TEXTURE_SHADOW_MAP,		// the sun's shadow map (white while there is none)
	SAGE2_BIND_TEXTURE_TOP_VIEW,		// the terrain top view: scorch marks (transparent while there is none)
	SAGE2_BIND_TEXTURE_DEFERRED_DIFFUSE,
	SAGE2_BIND_TEXTURE_DEFERRED_SPECULAR,
	SAGE2_BIND_TEXTURE_DEPTH,
	SAGE2_BIND_TEXTURE_NORMAL_SPEC,
	SAGE2_BIND_TEXTURE_DEPTH_BUFFER,

	SAGE2_BIND_COUNT
};

/// What one draw brings along besides W3D's transforms and lights.
struct Sage2DrawState
{
	float opacity;				// 0..1
	unsigned houseColor;		// 0x00RRGGBB, 0 for none
	float damage;				// 0 (pristine) .. 1 (wrecked)
	bool stealth;
	Sage2DrawState() : opacity(1.0f), houseColor(0), damage(0.0f), stealth(false) {}
};

/// Diagnostics: a few lines in Sage2.log next to the game (what loaded, what path each mesh took).
void Sage2_Log(const char* format, ...);

class Sage2GlobalsClass
{
public:
	/// The binding for an effect parameter, from its name and SasBindAddress; SAGE2_BIND_NONE when
	/// the engine has nothing for it (material constants, or values not supported yet).
	static int Resolve(const Sage2EffectClass::Param& param);
	/// Resolves every parameter of a freshly loaded effect.
	static void Resolve_All(Sage2EffectClass& effect);

	/// The frame-level values the scene renderer updates (light buffers come later).
	static void Set_Frame_Buffer_Size(unsigned width, unsigned height);
	static void Set_Scene_Textures(IDirect3DBaseTexture9* deferredDiffuse, IDirect3DBaseTexture9* deferredSpecular,
		IDirect3DBaseTexture9* depth, IDirect3DBaseTexture9* normalSpec);
	/// The clip-space depth buffer the linear-depth pass reads (PostEffect.DepthBufferTexture)
	static void Set_Depth_Buffer_Texture(IDirect3DBaseTexture9* depthBuffer);
	/// Whether the scene has rendered the deferred light buffers this frame (else the forward techniques are used)
	static bool Is_Deferred_Lighting_Available();
	/// Re-binds only the scene's texture parameters (after Set_Scene_Textures changed between passes)
	static void Apply_Scene_Textures(Sage2EffectClass& effect);

	/// Fills every bound parameter of the effect from W3D's current state and the draw's values.
	static void Apply(Sage2EffectClass& effect, const Sage2DrawState& draw);

	/// Releases the stand-in textures (before the device goes away).
	static void Shutdown();
};
