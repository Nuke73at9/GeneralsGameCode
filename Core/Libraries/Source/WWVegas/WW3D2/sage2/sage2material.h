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

// Tiberium Project @feature Meshes drawn with their SAGE 2 (C&C4 / RA3) effect (Direct3D 9 only).
//
// A converted model names its material in each mesh's user text (the W3D "user text" chunk, which
// the game never used), one "Key=Value" line each:
//
//     Sage2Effect=cnc4_object_gdi_proceduraldamage     the effect file, Art\Shaders\<name>.fxo
//     Sage2Technique=Default                           optional; Default when missing
//     DiffuseTexture=GUX_CrawlerA.dds                  texture parameters: file names
//     NormalMap=GUX_CrawlerA_NRM.dds
//     SpecMap=GUX_CrawlerA_SPM.dds
//     DamagedTexture=GUX_CrawlerA_D.dds
//     HouseColorUnitType=0                             numeric parameters: ints, floats, bools, vectors
//     EnvMult=1.0
//
// Such a mesh is drawn by the effect: the engine's own values (transforms, lights, house colour,
// opacity, damage) go in through Sage2GlobalsClass, the material's through the lines above, and
// a second vertex stream carries what W3D's vertex format lacks (tangent frame, blend index).
// Meshes without a SAGE 2 material, or whose effect file is missing, fall back to the built-in
// approximation (Sage2BuiltinMaterialClass). Visual only.
//
// Zero Hour's own meshes have no SAGE 2 material. With the legacy mode on, the plain ones (one
// texture, lit, opaque or alpha-tested) are drawn by BasicW3D, the effect C&C4 drew its own plain
// W3D meshes with (Art\Shaders\basicw3d.fxo): lit by the C&C4 lights, under the shadow map and the
// clouds, forward-lit (its "_M" technique) or deferred-lit with the terrain.

#pragma once

class DX8PolygonRendererClass;
class MeshClass;
class RenderObjClass;
class Sage2EffectClass;
class TextureClass;
class VertexMaterialClass;

class Sage2MaterialClass
{
public:
	/// What draws Zero Hour's plain meshes (Options.ini Sage2ObjectShaders)
	enum LegacyMode
	{
		LEGACY_OFF,			///< W3D's fixed-function draw
		LEGACY_FORWARD,		///< BasicW3D, forward-lit
		LEGACY_DEFERRED,	///< BasicW3D, deferred-lit when the scene's deferred lighting is on
	};
	static void Set_Legacy_Mode(LegacyMode mode);
	static LegacyMode Get_Legacy_Mode();

	/// Draws the mesh with its SAGE 2 effect (deferred-lit when the scene allows it, else with the
	/// forward-lit technique), or with the built-in approximation when it has no effect but a normal
	/// map. Returns false (and draws nothing) when the mesh is an ordinary W3D one.
	static bool Render(MeshClass* mesh, TextureClass* base, TextureClass* damageSheet, const VertexMaterialClass* material,
		DX8PolygonRendererClass* renderer);

	/// The effect Art\Shaders\<name>.fxo, loaded once; nullptr when missing or broken.
	static Sage2EffectClass* Get_Effect(const char* name);

	/// Tiberium Project @feature C&C4's scrolling treads. C&C4's tread effects add the vertex colour's
	/// alpha to the texture's U: its tread draw sets, per bone, how far the treads bound to that bone
	/// have scrolled. Here the draw module hands this frame's offsets (0..1) for bones of its render
	/// object (the model, by bone index); the skins drawn with a tread effect carry them in the alpha
	/// of their vertices bound to those bones. Clear_Bone_Scroll when the render object goes.
	static void Set_Bone_Scroll(const RenderObjClass* model, const int* bones, const float* offsets, int count);
	static void Clear_Bone_Scroll(const RenderObjClass* model);

	/// Frees the effects, side streams and caches (before the device goes away).
	static void Shutdown();
};
