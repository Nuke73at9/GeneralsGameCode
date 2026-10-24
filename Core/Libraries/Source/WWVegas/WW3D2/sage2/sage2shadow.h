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

// Tiberium Project @feature C&C4's shadow map (Direct3D 9 only).
//
// C&C4 draws the sun's shadow map once a frame: every caster (the terrain, props, units,
// structures) rendered from the sun into a depth texture, which the deferred directional light
// pass and the forward techniques then compare against (ShadowMap.fxh: a 2x2 cross of taps, or
// the 3x3 jittered kernel of the soft shadow). Here the map is a depth texture the hardware
// fills: a SAGE 2 mesh draws with its effect's own _CreateShadowMap technique (alpha-tested, as
// C&C4), a plain W3D mesh and the trees with the fixed-function pipeline, the terrain with its
// effect's depth-only technique, all from one camera looking along the sun. The camera is a
// perspective one far along the sun's direction (W3D culls with a perspective frustum), which
// is orthographic to within a degree; what it covers is the drawn terrain window plus the
// height anything standing on it can have. The effects receive the map and the world-to-map
// transform through Sage2GlobalsClass. Visual only.

#pragma once

#include <d3d9.h>

class CameraClass;
class Vector3;

class Sage2ShadowClass
{
public:
	/// Options.ini: Sage2Shadows = no turns the shadow map off; Sage2ShadowMapSize (2048),
	/// Sage2ShadowSoft (kernel size, 0 = C&C4's plain 2x2), Sage2ShadowJitter
	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();
	static void Set_Map_Size(unsigned size);
	static void Set_Soft(float kernel, float jitter);

	/// Starts the shadow map for this frame: the depth texture is the device's depth target,
	/// cleared, colour writes are off, and the camera is set up to look along 'sunTravel' (the
	/// direction the light travels) over the world box. The caller renders the scene with the
	/// camera, then calls End(). False: no shadows this frame (off, or the device cannot).
	static bool Begin(const Vector3& sunTravel, const Vector3& boxMin, const Vector3& boxMax);
	/// The camera to render the shadow pass with (valid between Begin and End)
	static CameraClass* Get_Camera();
	/// Puts the frame's targets back; the map is available to the effects until the next Begin().
	static void End();

	/// The shadow pass is being drawn: casters draw depth only, nothing else draws
	static bool Is_Rendering();
	/// A map was drawn this frame: the effects can take shadows from it
	static bool Is_Available();

	/// For the effects: the depth texture, world -> shadow map (texture xy, depth z), row-major
	/// in the effects' convention, and the map's size
	static IDirect3DBaseTexture9* Get_Texture();
	static const float* Get_World_To_Shadow();
	static unsigned Get_Map_Size();
	static void Get_Soft(float& kernel, float& jitter);

	/// Diagnostics: a caster drawn with its own effect (the terrain, a C&C4 mesh) counts itself;
	/// End() logs the first passes' counts. Draw_Debug() shows the map in the top-left corner of
	/// the frame (depth as grey: near the sun dark, far light) - Options.ini Sage2ShadowDebug = yes.
	static void Count_Draw(bool terrain);
	static void Set_Debug(bool enabled);
	static bool Is_Debug();
	static void Draw_Debug();

	/// Frees the map (before a device reset); made again on the next Begin()
	static void Release_Resources();
	static void Shutdown();
};
