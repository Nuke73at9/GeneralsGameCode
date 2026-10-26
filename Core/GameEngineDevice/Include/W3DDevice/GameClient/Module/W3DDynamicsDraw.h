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

// FILE: W3DDynamicsDraw.h ////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature A model that falls apart: each listed bone is a rigid piece thrown out
// when the object appears (the wreck of a unit or a building), tumbling under gravity, bouncing and
// sliding on the terrain, then fading. SAGE 2's DynamicsDraw (C&C4, Red Alert 3), drawn by the
// client only: the pieces never touch the game logic.
//
//   Draw = W3DDynamicsDraw ModuleTag_Draw
//     DefaultConditionState ... End              (as W3DModelDraw: the model, ParticleSysBone trails)
//     Explodiness = 4                            how hard the pieces are thrown out from the middle
//     FlingPerturbation = 10                     random speed added to each piece (units a second)
//     Fling = 1 2 75 90                          the whole wreck thrown: force min max (units a frame), pitch min max (degrees)
//     Gravity = 250                              units a second squared
//     LifetimeDelay = 4000                       ms before the pieces start to fade
//     LifetimeFade = 3000                        ms the fade takes
//     BoneVolume = Bone_D_01 Mass:801 Spin:6 Center:0.27,-0.07,12.34 Half:4.2,3.0,0.7 [Rotation:x,y,z,w]
//                                                one per piece: its bone, mass, how fast it spins
//                                                (radians a second), its box in model space (scaled
//                                                as the model), optionally turned
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "W3DDevice/GameClient/Module/W3DModelDraw.h"
#include <vector>

//-------------------------------------------------------------------------------------------------
struct W3DDynamicsBoneVolume
{
	AsciiString m_boneName;
	Real m_mass;
	Real m_spin;
	Real m_center[3];		///< box centre, model space
	Real m_half[3];			///< box half sizes
	Real m_rotation[4];		///< box orientation in model space (x, y, z, w)
};

//-------------------------------------------------------------------------------------------------
class W3DDynamicsDrawModuleData : public W3DModelDrawModuleData
{
public:
	Real m_explodiness;
	Real m_flingPerturbation;
	Real m_flingMin, m_flingMax;			///< units a logic frame
	Real m_flingPitchMin, m_flingPitchMax;	///< radians
	Real m_gravity;
	UnsignedInt m_lifetimeDelay;			///< ms
	UnsignedInt m_lifetimeFade;				///< ms
	std::vector<W3DDynamicsBoneVolume> m_volumes;

	W3DDynamicsDrawModuleData();
	virtual ~W3DDynamicsDrawModuleData() override;
	static void buildFieldParse(MultiIniFieldParse& p);

private:
	static void parseBoneVolume(INI* ini, void* instance, void* store, const void* userData);
	static void parseFling(INI* ini, void* instance, void* store, const void* userData);
};

//-------------------------------------------------------------------------------------------------
class W3DDynamicsDraw : public W3DModelDraw
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( W3DDynamicsDraw, "W3DDynamicsDraw" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( W3DDynamicsDraw, W3DDynamicsDrawModuleData )

public:
	W3DDynamicsDraw( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual void doDrawModule(const Matrix3D* transformMtx) override;
	virtual void reactToGeometryChange() override { }

protected:
	virtual void onRenderObjRecreated() override;

private:
	struct Piece
	{
		Int bone;
		Int parentPiece;		///< index of the piece the bone hangs from, or -1
		Real invMass;
		Real invInertia;		///< a box's inertia, taken as the same about every axis
		Matrix3D world;			///< the bone's transform now (world)
		Matrix3D restObject;	///< the bone's transform at rest (object space)
		Vector3 comLocal;		///< the box centre in the bone's space
		Vector3 corners[8];		///< the box corners in the bone's space
		Vector3 velocity;		///< of the box centre, units a second
		Vector3 spin;			///< angular velocity, radians a second (world)
		Real restTime;			///< how long it has lain still
		Bool asleep;
	};

	void start(const Matrix3D& objectTm);
	void step(Real dt);
	void collide(Piece& p);
	void applyBones(const Matrix3D& objectTm);

	std::vector<Piece> m_pieces;
	RenderObjClass* m_startedOn;	///< the render object the pieces were set up for
	Real m_age;						///< seconds since the pieces were thrown
	Bool m_started;
};
