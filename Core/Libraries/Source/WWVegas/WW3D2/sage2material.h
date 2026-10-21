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

// Tiberium Project @feature C&C4 (SAGE 2) object look (Direct3D 9 renderer only).
//
// A mesh whose base texture has a C&C4 normal map next to it (<texture>_NRM.dds, and optionally a
// specular map <texture>_SPM.dds) is drawn per pixel: bumped lighting from the scene's own lights,
// specular shine, a reflection and self-illumination from the specular map, and the procedural
// damage of DamageLookClass. Everything else keeps fixed-function drawing. Visual only.

#pragma once

class MeshClass;
class TextureClass;
class VertexMaterialClass;

class Sage2MaterialClass
{
public:
	/// Sets up the C&C4 object shaders for this mesh if its texture has a normal map; the caller then
	/// draws the mesh and calls End(). Returns false (and changes nothing) otherwise.
	static bool Begin(MeshClass* mesh, TextureClass* base, TextureClass* damageSheet, const VertexMaterialClass* material);
	static void End();

	/// Frees the shaders and the normal map cache (before the device goes away).
	static void Shutdown();
};
