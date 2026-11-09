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

// FILE: CompositeStructureUpdate.h ///////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's CompositeStructureInfoModule: Kane's Wrath's Nod defences are a hub
// with turrets around it (the Shredder Turret's, the Laser Turret's and the SAM Site's three). Kane's
// Wrath has the player place each turret within BuildableDistance of the hub; here the hub stands them
// in an arc before itself (its facing, as the player turned it when placing it) once it is built, each
// rising over UnpackTime as Kane's Wrath unpacks it (its StructureUnpackUpdate). A turret destroyed is put
// back where it stood RepairDelay later while the hub stands (Kane's Wrath's InitiateRepairDie: repaired
// after 20 s). The turrets die with the hub, destroyed or sold (Kane's Wrath's DieOnMastersDeath, SUICIDED),
// and change sides with it.
//
//   Behavior = CompositeStructureUpdate ModuleTag_Turrets
//     ThingTemplate     = KW_NODLaserCannonSpawn
//     Count             = 3
//     BuildableDistance = 204     ; how far from the hub they may stand
//     Radius            = 102     ; how far they are put (default: half of BuildableDistance)
//     Spread            = 60      ; degrees between them, centred on the hub's facing
//     UnpackTime        = 1500    ; msec each takes to rise
//     RepairDelay       = 20000   ; msec before a destroyed one is put back
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/INI.h"
#include "GameLogic/Module/UpdateModule.h"

#include <vector>

class ThingTemplate;

//-------------------------------------------------------------------------------------------------
class CompositeStructureUpdateModuleData : public UpdateModuleData
{
public:
	AsciiString m_thingTemplateName;	///< the turret
	Int m_count;
	Real m_buildableDistance;
	Real m_radius;
	Real m_spread;										///< radians between neighbours
	UnsignedInt m_unpackTime;					///< frames
	UnsignedInt m_repairDelay;				///< frames

	CompositeStructureUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class CompositeStructureUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( CompositeStructureUpdate, "CompositeStructureUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( CompositeStructureUpdate, CompositeStructureUpdateModuleData )

public:

	CompositeStructureUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;
	virtual void onDelete() override;
	// (also while the hub is unpowered or otherwise disabled: its turrets are placed, and go with it, all the same)
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }

private:

	struct Slot
	{
		ObjectID m_id;							///< its turret (INVALID_ID: none standing)
		UnsignedInt m_readyFrame;		///< when one may be put (back) up
		Coord3D m_pos;							///< where it stands (once m_placed)
		Real m_angle;
		Bool m_placed;
		Bool m_rising;
	};

	Bool choosePlace( Slot &slot, Int index, const ThingTemplate *tmpl );
	Bool isFree( const Coord3D *pos, Real angle, const ThingTemplate *tmpl ) const;
	void releaseTurrets();						///< they die with the hub (SUICIDED)

	std::vector<Slot> m_slots;
	Bool m_released;									///< the hub is gone: its turrets were killed

};
