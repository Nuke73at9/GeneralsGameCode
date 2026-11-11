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

// FILE: OrderAbilityUpdate.h /////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's special powers that give the unit an order Zero Hour gives some
// other way: REPAIR_TARGET (C&C4's AttackTargetSpecialPower SpecialPower_RepairStructure: an
// engineer, a Zero Hour dozer, repairs the structure it is aimed at, as on a right click) and
// RETURN_TO_PRODUCER (C&C4's ReturnToProducerSpecialPower: an aircraft goes back to what made it,
// to its parking place there, with AttributeModifier on itself for the way back).
//
// Tiberium Project @feature RETURN_TO_DOCK (Kane's Wrath's ReturnToDockSpecialPower: a harvester's Return to
// Refinery) takes a harvester to its refinery with what it carries, then back to its field; RECALL_AIRCRAFT
// (Kane's Wrath's RecallUnitsSpecialPower: the Air Tower's and the Hangar's Recall All Aircraft), given by an
// airfield, sends each of its aircraft away from it back to its parking place, the first of them saying its
// way-in line (its VoiceEnter, else its VoiceMove) unless RecalledVoice is No.
//
//   Behavior = SpecialAbility ModuleTag_ReturnPower
//     SpecialPowerTemplate = C4SP_SpecialPower_GDIAircraftH_ReturnToProducer
//   End
//   Behavior = OrderAbilityUpdate ModuleTag_Return
//     SpecialPowerTemplate = C4SP_SpecialPower_GDIAircraftH_ReturnToProducer
//     Order                = RETURN_TO_PRODUCER
//     AttributeModifier    = C4AM_AttributeModifier_GDIAircraftH_ReturnToBase
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/INI.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/Module/UpdateModule.h"

class CommandButton;
class SpecialPowerTemplate;
class Waypoint;

#include "GameLogic/Module/SpecialPowerUpdateModule.h"

enum AbilityOrderType CPP_11(: Int)
{
	ABILITY_ORDER_REPAIR_TARGET = 0,
	ABILITY_ORDER_RETURN_TO_PRODUCER,
	ABILITY_ORDER_RETURN_TO_DOCK,		///< Tiberium Project
	ABILITY_ORDER_RECALL_AIRCRAFT,	///< Tiberium Project
	ABILITY_ORDER_COUNT
};

//-------------------------------------------------------------------------------------------------
class OrderAbilityUpdateModuleData : public UpdateModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;
	AbilityOrderType m_order;
	NameKeyType m_modifier;		///< put on itself when the order is given (none: no modifier)
	Bool m_recalledVoice;			///< RECALL_AIRCRAFT: the first one sent back says its line

	OrderAbilityUpdateModuleData()
	{
		m_specialPowerTemplate = nullptr;
		m_order = ABILITY_ORDER_REPAIR_TARGET;
		m_modifier = NAMEKEY_INVALID;
		m_recalledVoice = TRUE;
	}

	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class OrderAbilityUpdate : public SpecialPowerUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( OrderAbilityUpdate, "OrderAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( OrderAbilityUpdate, OrderAbilityUpdateModuleData )

public:

	OrderAbilityUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	// SpecialPowerUpdateInterface
	virtual Bool initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions ) override;
	// (not a SpecialAbilityUpdate: Object::findSpecialAbilityUpdate takes any special ability for one)
	virtual Bool isSpecialAbility() const override { return false; }
	virtual Bool isSpecialPower() const override { return false; }
	virtual Bool isActive() const override { return false; }
	virtual Bool doesSpecialPowerHaveOverridableDestinationActive() const override { return false; }
	virtual Bool doesSpecialPowerHaveOverridableDestination() const override { return false; }
	virtual void setSpecialPowerOverridableDestination( const Coord3D *loc ) override {}
	virtual SpecialPowerUpdateInterface* getSpecialPowerUpdateInterface() override { return this; }
	virtual CommandOption getCommandOption() const override { return (CommandOption)0; }
	virtual Bool isPowerCurrentlyInUse( const CommandButton *command = nullptr ) const override { return false; }

	virtual UpdateSleepTime update() override { return UPDATE_SLEEP_FOREVER; }

};
