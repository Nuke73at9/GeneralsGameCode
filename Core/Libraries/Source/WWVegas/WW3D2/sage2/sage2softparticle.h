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

// Tiberium Project @feature Soft particles, as C&C4 draws its particles (Direct3D 9 only).
//
// C&C4 fades a particle where it meets the scene behind it (CommonParticle.fxh,
// ApplySoftParticleBlend): the closer to the ground or a wall, the fainter, so smoke, dust and
// fire blend into what they touch instead of cutting a straight line into it. Zero Hour's
// particles are drawn by W3D's point groups with its sprite shaders; the particle manager marks
// its billboard draws (DX8Wrapper::Set_Soft_Particle, kept with each draw the sorting renderer
// defers), and DX8Wrapper::Draw draws a marked one with the soft-particle shaders
// (shaders/softparticle.hlsl): the texture times the colour as before, faded by the distance to
// the scene's linear depth (the deferred pre-pass's: the ground and every deferred mesh).
// Visual only.

#pragma once

class ShaderClass;

class Sage2SoftParticleClass
{
public:
	/// Options.ini Sage2SoftParticles (on with the C&C4 renderer) and Sage2SoftParticleFade, the
	/// distance in world units over which a particle fades out in front of the scene
	static void Set_Enabled(bool enabled);
	static bool Is_Enabled();
	static void Set_Fade_Distance(float distance);

	/// Right before a marked draw (W3D's states applied): binds the shaders, their constants and
	/// the scene's depth for this draw's blending. False (nothing changed): draw it as W3D does
	/// (off, no depth this frame, a blend it does not fade, alpha-tested, no shader support).
	static bool Begin(const ShaderClass& shader);
	/// After the draw: back to the fixed-function pipeline
	static void End();

	static void Shutdown();
};
