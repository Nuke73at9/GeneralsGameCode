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

#pragma once

class DX8PolygonRendererClass;
class MeshClass;
class Sage2EffectClass;
class TextureClass;
class VertexMaterialClass;

class Sage2MaterialClass
{
public:
	/// Draws the mesh with its SAGE 2 effect (deferred-lit when the scene allows it, else with the
	/// forward-lit technique), or with the built-in approximation when it has no effect but a normal
	/// map. Returns false (and draws nothing) when the mesh is an ordinary W3D one.
	static bool Render(MeshClass* mesh, TextureClass* base, TextureClass* damageSheet, const VertexMaterialClass* material,
		DX8PolygonRendererClass* renderer);

	/// The effect Art\Shaders\<name>.fxo, loaded once; nullptr when missing or broken.
	static Sage2EffectClass* Get_Effect(const char* name);

	/// Frees the effects, side streams and caches (before the device goes away).
	static void Shutdown();
};
