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

// Tiberium Project @feature C&C4-style procedural damage (Direct3D 9 renderer only).
//
// A mesh whose material has a damage sheet in its second texture stage (C&C4's GUObject_Holes,
// mapped by the model's second UV set) is drawn with a pixel shader that scorches it in patches,
// with glowing embers along the spreading edge and burnt-through holes when badly damaged, by
// how damaged its render object looks (RenderObjClass::Set_Damage_Look). Visual only.

#pragma once

class MeshClass;
class TextureClass;

class DamageLookClass
{
public:
	/// Sets up the damage shader for this mesh if it should be drawn damaged; the caller then
	/// draws the mesh and calls End(). Returns false (and changes nothing) otherwise.
	static bool Begin(MeshClass* mesh, TextureClass* damageSheet);
	static void End();

	/// Frees the shader and noise texture (before the device goes away).
	static void Shutdown();
};
