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

// Tiberium Project @feature Zero Hour's roads and vehicle tracks drawn the C&C4 way (Direct3D 9 only).
//
// C&C4's roads are strips over the terrain drawn with the terrain's shader family: lit from
// the deferred light buffers, alpha-blended edges, depth-tested without writing, with the
// terrain top view's scorch marks folded in (TerrainRoad.fx); its tread marks are strips with a
// fading vertex colour, the mark in linear light (TerrainTracks.fx). This draws W3DRoadBuffer's
// strips (one vertex buffer per road type) with tiberium_road.fxo and the track system's with
// tiberium_tracks.fxo, right after the terrain effect drew the terrain, reading the light
// buffers its pre-pass produced under them, and the shroud and fog the terrain used. Visual only.

#pragma once

#include <d3d9.h>

class TextureClass;

class Sage2RoadClass
{
public:
	enum Kind { ROAD, TRACKS };

	/// Options.ini: Sage2Roads = no keeps Zero Hour's fixed-function roads and tracks
	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();

	/// Draws strips (Zero Hour's XYZDUV1 layout; the indices from 'baseVertex' on, 'triangleCount'
	/// triangles over 'vertexCount' vertices) with the kind's effect, lit as the terrain was this
	/// frame. False when the effect is not available or the terrain effect did not draw the
	/// terrain this frame, in which case the caller draws them its own way.
	static bool Draw(Kind kind, IDirect3DVertexBuffer9* vertices, IDirect3DIndexBuffer9* indices, TextureClass* texture,
		unsigned vertexCount, unsigned triangleCount, unsigned baseVertex = 0);

	static void Shutdown();
};
