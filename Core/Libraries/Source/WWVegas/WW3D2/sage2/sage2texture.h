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

// Tiberium Project @feature DDS textures for the SAGE 2 effects: cube maps (environment
// reflections), volume textures (colour-grading tables) and plain 2D textures, read straight
// from the DDS file (W3D's own loader only makes 2D textures). Compressed (DXT1/3/5), 32-bit
// and half-float formats, with their mip chains. Loaded once, kept for the session.

#pragma once

#include <d3d9.h>

class Sage2TextureClass
{
public:
	/// The texture of that name (<name>.dds through the game's file factory), or null when it is
	/// missing or in a format not handled; a missing texture is asked for only once.
	static IDirect3DBaseTexture9* Get(const char* name);
	/// Releases every texture (before the device goes away).
	static void Shutdown();
};
