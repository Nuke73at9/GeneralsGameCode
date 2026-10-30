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

// FILE: BaseConstructionUpdate.h /////////////////////////////////////////////////////////////////
// Tiberium Project @feature Kane's Wrath's build-then-place base: a construction yard builds a
// structure (paid when it starts, one at a time), holds it ready, and the player places it where
// it may stand; there it rises over UnpackTime, with no builder. The structures it builds are the
// DOZER_CONSTRUCT buttons of its CommandSet (the sidebar's Base tab).
//
//   Behavior = BaseConstructionUpdate ModuleTag_Base
//     CommandSet = AmericaDozerCommandSet   ; the structures it builds
//     UnpackTime = 3000                     ; msec a placed structure takes to rise
//     BuildRadius = 400                     ; how near one of the player's structures it must stand (0: no limit
//                                           ; beyond the build areas, BuildAreaBehavior)
//     ExcludeKindOf = FS_BASE_DEFENSE        ; ... but not these (a builder on site builds them)
//   End
//
// Messages (the sidebar's): MSG_TIBERIUM_BASE_QUEUE, MSG_TIBERIUM_BASE_CANCEL, MSG_TIBERIUM_BASE_PLACE.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpdateModule.h"

#include <vector>

class ThingTemplate;

//-------------------------------------------------------------------------------------------------
class BaseConstructionUpdateModuleData : public UpdateModuleData
{
public:
	AsciiString m_commandSetName;
	UnsignedInt m_unpackTime;
	Real m_buildRadius;
	KindOfMaskType m_excludeKindOf;

	BaseConstructionUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class BaseConstructionUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( BaseConstructionUpdate, "BaseConstructionUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( BaseConstructionUpdate, BaseConstructionUpdateModuleData )

public:

	BaseConstructionUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	/// the object's, or null
	static BaseConstructionUpdate *find( Object *obj );

	const AsciiString& getCommandSetName() const { return getBaseConstructionUpdateModuleData()->m_commandSetName; }
	/// whether it builds this (its command set lists it, and it is not excluded)
	Bool builds( const ThingTemplate *what ) const { return findButton( what ) != nullptr; }
	/// whether its owner may have it built now: its prerequisites, its button's upgrade, the most of it
	/// a player may have (the money aside)
	Bool canBuildNow( const ThingTemplate *what ) const;
	const ThingTemplate *getBuilding() const { return m_building; }
	Bool isReady() const { return m_building != nullptr && m_framesLeft == 0; }
	/// how far the one building is, 0 to 100
	Real getPercent() const;

	Bool queue( const ThingTemplate *what );					///< pays and starts it; false when busy or short of money
	void cancel();																		///< the one building or ready, refunded
	Bool place( const Coord3D *where, Real angle );		///< the ready one, there, rising
	/// whether a structure of the yard's may stand there as far as the base's reach goes: in one of the
	/// player's build areas (BuildAreaBehavior), and within BuildRadius of one of its structures when set
	/// (the ground itself is BuildAssistant's question)
	Bool isWithinBuildRadius( const Coord3D *where ) const;

	virtual UpdateSleepTime update() override;

private:

	const class CommandButton *findButton( const ThingTemplate *what ) const;

	const ThingTemplate *m_building;		///< building or ready (null: idle)
	UnsignedInt m_framesLeft;						///< until ready
	UnsignedInt m_framesTotal;
	Int m_paid;
	std::vector<ObjectID> m_rising;			///< placed, rising

};
