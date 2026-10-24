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

// Tiberium Project @feature C&C4's deferred lighting, per mesh (Direct3D 9 only).
//
// C&C4's "High" object techniques do not light the surface themselves: a depth / normal pre-pass
// writes every object's bumped normal, specular power and depth into screen-space buffers,
// deferredlighting.fxo turns those into diffuse and specular light buffers, and the object's
// own pass reads the light at its pixel. Here that happens mesh by mesh, right before the mesh
// is drawn: its pre-pass into the buffers, the light computed over its screen rectangle, then
// the draw. Meshes drawn later overwrite the buffers where they are in front, as the ordinary
// depth test will show them, so the order W3D draws in does not matter. Terrain and everything
// else is untouched. Visual only.

#pragma once

#include <d3d9.h>

class Sage2EffectClass;
class MeshClass;
class DX8PolygonRendererClass;

class Sage2SceneClass
{
public:
	/// Whether the device and the shipped effects allow the deferred path at all.
	static bool Is_Available();
	/// Sage2Deferred = no in Options.ini turns it off (default on).
	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();

	/// Called at the start of every frame (DX8Wrapper::Begin_Scene).
	static void Begin_Frame();

	/// Pre-pass of this mesh (the effect's _CreateDepthNormalMap technique; vertex streams and the
	/// effect's parameters are already set up by the caller) and the lighting over its screen
	/// rectangle. On return the current render targets are back and the light buffers are bound
	/// in Sage2GlobalsClass for the mesh's own pass. Returns false when the deferred path can't run
	/// this frame (no buffers, no effects), in which case the caller draws forward-lit.
	static bool Light_Mesh(Sage2EffectClass& effect, int prepassTechnique, MeshClass* mesh, DX8PolygonRendererClass* renderer,
		IDirect3DVertexBuffer9* meshBuffer, unsigned meshStride, IDirect3DVertexDeclaration9* meshDeclaration);

	/// What a pre-pass draws: the caller's geometry, with the effect's pass already begun
	class PrepassDrawClass
	{
	public:
		virtual ~PrepassDrawClass() {}
		virtual void Draw(IDirect3DDevice9* device) = 0;
	};
	/// The same for any batch of geometry (the terrain): the pre-pass draws through 'draw', the
	/// lighting runs over the clip-space rectangle (x0,y0)-(x1,y1), the whole screen for the terrain.
	/// On return the frame's targets and viewport are back, the light buffers are bound, and no
	/// vertex streams are set.
	static bool Light_Batch(Sage2EffectClass& effect, int prepassTechnique, PrepassDrawClass& draw, float x0, float y0, float x1, float y1);

	/// Tiberium Project @feature point lights, RA3's and C&C4's way: drawn after the directional
	/// light into the light buffers with C&C4's own PointLight technique (deferredlighting.fxo),
	/// one screen rectangle each, so the ground and every deferred mesh take them. The scene
	/// gathers them each frame (Zero Hour's light pulses: explosions, muzzle flashes, ...).
	struct PointLight
	{
		float position[3];
		float color[3];		///< at the centre, as the lights' colours are
		float innerRadius;	///< full strength inside this
		float outerRadius;	///< nothing beyond this (C&C4: a smooth step between the two)
	};
	static void Clear_Point_Lights();
	static void Add_Point_Light(const PointLight& light);
	static unsigned Get_Point_Light_Count();
	/// Options.ini: Sage2PointLights, how strong they are (0: none)
	static void Set_Point_Light_Strength(float strength);
	static float Get_Point_Light_Strength();

	/// sRGB writes make the linear-space High techniques come out right on the ordinary frame buffer.
	static bool Supports_SRGB_Write();

	/// Frees the frame buffers (before a device reset); they come back on demand.
	static void Release_Resources();
	static void Shutdown();
};
