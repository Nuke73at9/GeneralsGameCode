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

// Tiberium Project @feature Direct3D 9 renderer: forward declarations of the Direct3D 8 interfaces.
// With SAGE_USE_D3D9 the Direct3D 8 names stand for Direct3D 9 types (d3d9compat/d3d8on9.h), which
// can't be forward-declared under those names, so the adapter header is included instead.

#pragma once

#ifdef RTS_RENDERER_D3D9
#include <d3d8.h>
#else
struct IDirect3D8;
struct IDirect3DDevice8;
struct IDirect3DSwapChain8;
struct IDirect3DResource8;
struct IDirect3DBaseTexture8;
struct IDirect3DTexture8;
struct IDirect3DCubeTexture8;
struct IDirect3DVolumeTexture8;
struct IDirect3DSurface8;
struct IDirect3DVolume8;
struct IDirect3DVertexBuffer8;
struct IDirect3DIndexBuffer8;
#endif

// The pointer type a vertex or index buffer's Lock() hands back (BYTE* in Direct3D 8, void* in 9)
#ifdef RTS_RENDERER_D3D9
typedef void* D3DLockPointer;
#else
typedef unsigned char* D3DLockPointer;
#endif
