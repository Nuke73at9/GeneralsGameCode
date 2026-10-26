/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 Electronic Arts Inc.
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

// Tiberium Project @feature Ray-traced passes inside the Direct3D 9 renderer (Direct3D 9 only).
//
// When the Direct3D 9 is DXVK's (Vulkan underneath) and DXVK made its device with acceleration
// structures and ray queries (DXVK 2.6 with our patch, asked through DXVK_ENABLE_RAY_QUERY), the
// render-device interface (Core/Libraries/Source/RHI) runs on that same Vulkan device through
// DXVK's interop: it reads the frame's depth buffer and writes Direct3D 9 textures the frame's
// shaders then sample. The scene it traces is the terrain (from the height map) and the rigid
// meshes drawn this frame (bottom-level structures per mesh model, built once; the top level each
// frame). First pass: ambient occlusion, in place of the screen-space estimate (Sage2SSAOClass
// blurs and applies it as its own). Without a GPU or a DXVK that traces rays, nothing changes.
// Visual only.

#pragma once

#include <d3d9.h>

class MeshModelClass;
class Matrix3D;

class Sage2RayTracingClass
{
public:
	/// Options.ini Sage2RayTracing (default on): asks for it; read before the device is made, since
	/// DXVK has to make its Vulkan device with ray queries
	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();
	/// Options.ini Sage2RayTracingAORadius (world units), Sage2RayTracingAORays, Sage2RayTracingAOIntensity
	static void Set_AO_Params(float radius, int rays, float intensity);

	/// Before Direct3DCreate9 of DXVK's Direct3D 9: asks DXVK for a device that traces rays
	static void Before_Device_Creation();

	/// The map's height map (WorldHeightMap, when a map loads): heights (data * heightScale) on a
	/// grid of cellSize, the first 'border' rows and columns outside the playable area
	static void Set_Height_Map(const unsigned char* data, int width, int height, int border, float cellSize, float heightScale);
	/// A mesh drawn this frame (MeshClass::Render): rigid, opaque meshes go into the scene
	static void Add_Mesh(MeshModelClass* model, const Matrix3D& transform);
	/// The mesh model is going away (its bottom-level structure goes with it)
	static void Forget_Model(MeshModelClass* model);

	/// Traces the frame's ambient occlusion from its depth buffer into a texture of aoWidth x
	/// aoHeight (openness in red, 1 open). The parameters are Sage2SSAOClass's: the view in the frame
	/// (texture coordinates: corner, 1 / size), the viewport's depth range, the view and projection
	/// (Direct3D row-vector matrices, 16 floats each). Null when it can't (off, no ray queries,
	/// the first frames while the scene builds): the caller computes its own.
	static IDirect3DTexture9* Trace_Ambient_Occlusion(IDirect3DDevice9* device, IDirect3DTexture9* depth, unsigned aoWidth, unsigned aoHeight,
		const float viewport[4], float minZ, float maxZ, const float view[16], const float projection[16]);

	/// Whether the ray-traced passes run (device shared, ray queries on); valid after the first frame
	static bool Is_Available();

	/// Device-dependent textures go before a device reset; everything before the device goes
	static void Release_Resources();
	static void Shutdown();
};
