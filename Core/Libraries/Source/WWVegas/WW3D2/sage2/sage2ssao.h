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

// Tiberium Project @feature Screen-space ambient occlusion (shaders/ssao.hlsl): after the opaque
// geometry, the creases, corners and contact lines of the scene darkened from the scene's own depth
// buffer, which the Direct3D 8 adapter makes readable (INTZ) when this is on. C&C4 shipped without
// ambient occlusion; this is our own (SAO, McGuire et al. 2012).

#pragma once

struct IDirect3DTexture9;

class Sage2SSAOClass
{
public:
	/// Options.ini Sage2SSAO: on or off; read before the device is made (it needs the readable depth)
	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();
	/// radius: how far around a point the occluders count (world units); intensity: how dark a
	/// closed-off point gets; strength: how much of it goes onto the frame (0..1)
	static void Set_Params(float radius, float intensity, float strength);
	/// Options.ini Sage2SSAODebug: the occlusion shown alone (grey) in place of the scene
	static void Set_Debug(bool debug);
	/// The scene's opaque geometry is drawn: darken it (the frame's colour times the occlusion).
	/// Nothing happens without a readable depth buffer bound (the adapter's), or when off.
	static void Apply();
	/// Tiberium Project @feature the opaque scene's view distance at each pixel (R32F, the frame's
	/// size), written with the occlusion from the same depth buffer: the ground, the objects and the
	/// trees. Null when it was not written this frame (SSAO off, no readable depth). The soft
	/// particles fade against it, so smoke no longer cuts a hard line into units and buildings.
	static IDirect3DTexture9* Get_Scene_Depth_Texture();
	/// Render targets go before a device reset, everything at shutdown
	static void Release_Resources();
	static void Shutdown();
};
