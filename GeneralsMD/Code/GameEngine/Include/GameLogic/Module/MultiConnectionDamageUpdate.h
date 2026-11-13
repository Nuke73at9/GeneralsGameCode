/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

// FILE: MultiConnectionDamageUpdate.h ////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's MultiConnectionDamageUpdate: C&C4's Spider Tanks' web. The object links with
// its partners (allies of the kinds named in Partner) within MaxRange, MaxConnections at most, looking for new ones
// every RescanDelay; a beam (LaserName, an object with a LaserUpdate and a LifetimeUpdate longer than UpdateDelay)
// runs from its BeamStartBone to each partner's BeamEndBone for as long as they are linked. Every DamageDelay its
// Weapon is fired at each enemy standing on a link's line (within half of LineWidth of it, by the enemy's own
// width; of VictimKindOf, on the ground). Each pair is linked once, by the one of the two made first. Nothing is
// linked from or to one inside a transport or with a ForbiddenStatus (burrowed).
//
//   Behavior = MultiConnectionDamageUpdate ModuleTag_Web
//     Partner         = C4_NODVehicleC
//     MaxConnections  = 9
//     MaxRange        = 340
//     Weapon          = C4W_NODVehicleCWebWeapon
//     LineWidth       = 17
//     DamageDelay     = 500
//     RescanDelay     = 3000
//     UpdateDelay     = 500
//     VictimKindOf    = INFANTRY VEHICLE STRUCTURE HUGE_VEHICLE
//     ForbiddenStatus = BURROWING
//     LaserName       = C4LZ_NODVehicleC_0
//     BeamStartBone   = NS_CRYSTAL
//     BeamEndBone     = NS_CRYSTAL
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "Common/KindOf.h"
#include "Common/ObjectStatusTypes.h"
#include "GameLogic/Module/UpdateModule.h"

class WeaponTemplate;

//-------------------------------------------------------------------------------------------------
class MultiConnectionDamageUpdateModuleData : public UpdateModuleData
{
public:
	std::vector<AsciiString> m_partnerNames;	///< the kinds it links with
	Int m_maxConnections;											///< links it makes at most
	Real m_maxRange;													///< how far a partner may stand
	const WeaponTemplate *m_weapon;						///< fired at each enemy on a link
	Real m_lineWidth;													///< how wide a link's line is
	UnsignedInt m_damageDelay;								///< frames between its weapon's shots
	UnsignedInt m_rescanDelay;								///< frames between its looks for new partners
	UnsignedInt m_updateDelay;								///< frames between its looks at the links it has
	KindOfMaskType m_victimKindOf;						///< the kinds its weapon is fired at (none named: any)
	Bool m_hitsNeutrals;											///< and at what is no one's
	ObjectStatusMaskType m_forbiddenStatus;		///< no link from or to one with any of these
	AsciiString m_laserName;									///< each link's beam
	AsciiString m_beamStartBone;
	AsciiString m_beamEndBone;

	MultiConnectionDamageUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse &p );
};

//-------------------------------------------------------------------------------------------------
class MultiConnectionDamageUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( MultiConnectionDamageUpdate, "MultiConnectionDamageUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( MultiConnectionDamageUpdate, MultiConnectionDamageUpdateModuleData )

public:
	MultiConnectionDamageUpdate( Thing *thing, const ModuleData *moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

	Int getConnectionCount() const { return (Int)m_connections.size(); }	///< the links it holds (the test's reading)
	UnsignedInt getShotCount() const { return m_shotCount; }							///< shots its links have fired

private:
	struct Connection
	{
		ObjectID m_partnerID;
		ObjectID m_laserID;
	};

	Bool canLink( const Object *obj ) const;
	Bool isLinkedTo( ObjectID partnerID ) const;
	void rescan();
	void dropConnection( size_t index );
	void keepBeam( Connection &connection, Object *partner );
	void fireAlong( Object *partner );

	std::vector<Connection> m_connections;
	UnsignedInt m_nextRescanFrame;
	UnsignedInt m_nextDamageFrame;
	UnsignedInt m_shotCount;
};
