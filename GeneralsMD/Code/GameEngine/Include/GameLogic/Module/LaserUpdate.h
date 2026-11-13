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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: LaserUpdate.h //////////////////////////////////////////////////////////////////////////
// Author: Kris Morness, July 2002
// Desc:   Handles laser update processing for render purposes and game control.
// Modifications: Kris Morness - Oct 2002 -- moved to Client update (will rename later)
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/ClientUpdateModule.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Thing;
class Vector3;
enum ParticleSystemID CPP_11(: Int);

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class LaserUpdateModuleData : public ClientUpdateModuleData
{
public:
	AsciiString m_particleSystemName;  ///< Used for the muzzle flare while laser active.

	AsciiString m_targetParticleSystemName;  ///< Used for the target effect while laser active.

	Real m_punchThroughScalar;	///< If non-zero, length modifier when we used to have a target object and now don't

	/// Tiberium Project @feature the particle systems a stream draws along its length (C&C3 and Kane's Wrath's
	/// StreamDraw ParticleSystems: a flamethrower's flames and glow along the stream), each emitted from a share
	/// of the length to another (0 at the beam's start, 1 at its end): StreamParticleSystem = <name> [from to].
	/// Their ortho velocity's Y runs along the stream, X to its right, Z up.
	enum { MAX_STREAM_SYSTEMS = 4 };
	struct StreamSystem
	{
		AsciiString m_name;
		Real m_from;
		Real m_to;
	};
	StreamSystem m_streamSystems[MAX_STREAM_SYSTEMS];
	Int m_streamSystemCount;
	/// Tiberium Project @feature how fast the beam runs out from its start towards its end, distance per frame
	/// (StreamSpeed, per second in the INI: C&C3's StreamDraw Velocity); 0 for all at once
	Real m_streamSpeed;

	LaserUpdateModuleData();
	static void buildFieldParse(MultiIniFieldParse& p);
	static void parseStreamParticleSystem( INI* ini, void *instance, void *store, const void* userData );	///< Tiberium Project

private:

};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class LaserRadiusUpdate
{
public:
	LaserRadiusUpdate();

	void initRadius( Int sizeDeltaFrames );
	bool updateRadius();
	void setDecayFrames( UnsignedInt decayFrames );
	void xfer( Xfer *xfer );
	Real getWidthScale() const { return m_currentWidthScalar; }

private:
	Bool m_widening;
	Bool m_decaying;
	UnsignedInt m_widenStartFrame;
	UnsignedInt m_widenFinishFrame;
	Real m_currentWidthScalar;
	UnsignedInt m_decayStartFrame;
	UnsignedInt m_decayFinishFrame;
};

//-------------------------------------------------------------------------------------------------
/** The laser update module */
//-------------------------------------------------------------------------------------------------
class LaserUpdate : public ClientUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( LaserUpdate, "LaserUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( LaserUpdate, LaserUpdateModuleData );

public:

	LaserUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	//Actually puts the laser in the world.
	void initLaser( const Object *parent, const Object *target, const Coord3D *startPos, const Coord3D *endPos, AsciiString parentBoneName, Int sizeDeltaFrames = 0 );

	const LaserRadiusUpdate& getLaserRadiusUpdate() const { return m_laserRadius; }
	void setDecayFrames( UnsignedInt decayFrames ) { m_laserRadius.setDecayFrames(decayFrames); }
	Real getWidthScale() const { return m_laserRadius.getWidthScale(); }

	const Coord3D* getStartPos() const { return &m_startPos; }
	const Coord3D* getEndPos() const { return &m_endPos; }

	Real getTemplateLaserRadius() const;
	Real getCurrentLaserRadius() const;

	/// Tiberium Project @feature how far the beam has run out towards its end, 0 to 1 (StreamSpeed; 1 without)
	Real getStreamReach() const;

	/// Tiberium Project @feature the bone of the target the beam ends at (else at its feet): a beam kept between two
	/// units (C&C4's Spider Tanks' web, crystal to crystal)
	void setTargetBone( const AsciiString &bone ) { m_targetBoneName = bone; m_dirty = TRUE; }

	void setDirty( Bool dirty ) { m_dirty = dirty; }
	Bool isDirty() const { return m_dirty; }

	virtual void clientUpdate() override;

protected:

	void updateStartPos(); ///< figures out and sets startPos
	void updateEndPos(); ///< figures out and sets endPos
	void updateStreamSystems(); ///< Tiberium Project: the stream's particle systems along the beam as it is now

	//If the master dies, so will this laser (although if it has a fade delay, it'll just skip to the fade)
	Coord3D m_startPos;
	Coord3D m_endPos;

	DrawableID m_parentID;
	DrawableID m_targetID;

	Bool m_dirty;
	ParticleSystemID m_particleSystemID;
	ParticleSystemID m_targetParticleSystemID;
	AsciiString m_parentBoneName;
	AsciiString m_targetBoneName;	///< Tiberium Project: the target's bone it ends at (setTargetBone)

	LaserRadiusUpdate m_laserRadius;

	ParticleSystemID m_streamSystemIDs[LaserUpdateModuleData::MAX_STREAM_SYSTEMS];	///< Tiberium Project: StreamParticleSystem
	UnsignedInt m_streamStartFrame;		///< Tiberium Project: the frame the beam began to run out (StreamSpeed)
};
