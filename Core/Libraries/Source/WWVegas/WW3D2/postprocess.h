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

// Tiberium Project @feature Scene post-processing (Direct3D 9 renderer only): bloom around bright
// light and a C&C4-like colour grade, applied to the 3D view before the interface is drawn.
// Every Direct3D state it touches is restored afterwards. Visual only.

#pragma once

class PostProcessClass
{
public:
	/// Grades the current render target in place (call between the 3D scene and the interface).
	static void Apply();

	static void Set_Enabled(bool enabled) { Enabled = enabled; }
	static bool Is_Enabled() { return Enabled; }

	/// Frees the render targets (before a device reset); they are made again on the next Apply().
	static void Release_Resources();
	/// Frees everything (before the device goes away).
	static void Shutdown();

private:
	static bool Enabled;
};
