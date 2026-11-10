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


// FILE: GrabSpecialAbilityUpdate.h ///////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's GrabSpecialAbilityUpdate with its GrabUpdate (C&C4's Nod Scorpion takes a unit
// in its claw): aimed at a unit (an enemy's, or a neutral one with AllowNeutralVictims; of VictimKindOf, none of
// VictimForbiddenKindOf, not held already), the unit closes to StartAbilityRange of it (edge to edge) and takes it.
// It drags it to the first of its DragToBone bones its model has (else just ahead of it) at DragRate (distance a
// second) and holds it there, LiftHeight off the ground, firing GrabWeapon at it every GrabWeaponInterval.
//
// While it holds it, the unit carries StatusSelf, ModelConditionsSelf, OwnerAttributeModifier and the command set
// CommandSetGrabbing; the victim is held still (it does nothing of its own: Zero Hour's held state), carries
// StatusVictim (MASKED: nothing else can target it) and VictimAttributeModifier, and no longer collides. The first
// VictimBehavior whose KindOf the victim has any of says what holding it takes: StatusSelf added (C&C4's medium
// vehicle keeps the Scorpion where it stands: IMMOBILE), and its own DragToBone, DragRate and LiftHeight.
//
// The grip ends with StopSpecialPowerTemplate (the claw opens), as either dies, as the unit is disabled or shows any
// of ReleaseConditions (it burrows), or as the victim is no longer one it may hold; OnReleaseRechargeSpecialPower
// (the grab's own power) then starts its reload.
//
//   Behavior = SpecialAbility ModuleTag_GrabPower
//     SpecialPowerTemplate     = C4SP_SpecialPower_NODVehicleTGrab
//     UpdateModuleStartsAttack = Yes
//   End
//   Behavior = SpecialAbility ModuleTag_GrabStopPower
//     SpecialPowerTemplate     = C4SP_SpecialPower_NodVehicleTGrabStop
//   End
//   Behavior = GrabSpecialAbilityUpdate ModuleTag_Grab
//     SpecialPowerTemplate          = C4SP_SpecialPower_NODVehicleTGrab
//     StopSpecialPowerTemplate      = C4SP_SpecialPower_NodVehicleTGrabStop
//     OnReleaseRechargeSpecialPower = C4SP_SpecialPower_NODVehicleTGrab
//     StartAbilityRange             = 16
//     VictimKindOf                  = VEHICLE INFANTRY
//     VictimForbiddenKindOf         = AIRCRAFT HEAVY_UNIT FS_FACTORY STRUCTURE
//     StatusSelf                    = NO_ATTACK
//     StatusVictim                  = NO_ATTACK UNSELECTABLE MASKED
//     ModelConditionsSelf           = USER_1
//     CommandSetGrabbing            = C4CS_NODVehicleT_Grabbing
//     VictimAttributeModifier       = C4AM_AttributeModifier_NodVehicleTGrabVictim
//     OwnerAttributeModifier        = C4AM_AttributeModifier_NodVehicleTGrabbedVictimSelf
//     VictimBehavior                = KindOf:MEDIUM_UNIT StatusSelf:IMMOBILE DragToBone:GrabVictimPosition DragRate:32
//     VictimBehavior                = KindOf:VEHICLE,INFANTRY DragToBone:GrabVictimPosition DragRate:32
//     GrabWeapon                    = C4W_NODVehicleTGrabbedWeapon
//     GrabWeaponInterval            = 200
//     ReleaseConditions             = BURROWING BURROWING_TRANSITION_ENTER
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "Common/INI.h"
#include "Common/KindOf.h"
#include "Common/ModelState.h"
#include "Common/ObjectStatusTypes.h"
#include "GameLogic/Module/SpecialPowerUpdateModule.h"
#include "GameLogic/Module/UpdateModule.h"

class SpecialPowerTemplate;
class Waypoint;

//-------------------------------------------------------------------------------------------------
struct GrabVictimBehavior
{
	KindOfMaskType m_kindOf;								///< a victim with any of these (none: any)
	ObjectStatusMaskType m_statusSelf;				///< added to the unit while it holds one
	std::vector<AsciiString> m_dragToBones;	///< the first of these its model has
	Real m_dragRate;													///< distance a frame
	Real m_liftHeight;												///< above the ground
};

//-------------------------------------------------------------------------------------------------
class GrabSpecialAbilityUpdateModuleData : public UpdateModuleData
{
public:
	const SpecialPowerTemplate *m_specialPowerTemplate;		///< the grab
	const SpecialPowerTemplate *m_stopSpecialPowerTemplate;	///< the claw opens (none: it holds on until something ends it)
	const SpecialPowerTemplate *m_rechargeOnRelease;			///< reloads from the release on
	Real m_startAbilityRange;															///< edge to edge
	KindOfMaskType m_victimKindOf;												///< any of these (none: any)
	KindOfMaskType m_victimForbiddenKindOf;								///< none of these
	Bool m_allowNeutralVictims;
	ObjectStatusMaskType m_statusSelf;
	ObjectStatusMaskType m_statusVictim;
	ModelConditionFlags m_modelConditionsSelf;
	ModelConditionFlags m_releaseConditions;							///< the unit lets go as it shows any of these
	AsciiString m_commandSetGrabbing;
	NameKeyType m_victimModifier;
	NameKeyType m_ownerModifier;
	std::vector<GrabVictimBehavior> m_victimBehaviors;
	AsciiString m_grabWeaponName;													///< found when it fires (the weapons load before the objects)
	UnsignedInt m_grabWeaponInterval;											///< frames

	GrabSpecialAbilityUpdateModuleData();
	static void parseVictimBehavior( INI *ini, void *instance, void *store, const void *userData );
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class GrabSpecialAbilityUpdate : public SpecialPowerUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( GrabSpecialAbilityUpdate, "GrabSpecialAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( GrabSpecialAbilityUpdate, GrabSpecialAbilityUpdateModuleData )

public:

	GrabSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	// SpecialPowerUpdateInterface
	virtual Bool initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions ) override;
	// (not a SpecialAbilityUpdate: Object::findSpecialAbilityUpdate takes any special ability for one)
	virtual Bool isSpecialAbility() const override { return false; }
	virtual Bool isSpecialPower() const override { return false; }
	virtual Bool isActive() const override { return m_stage != STAGE_IDLE; }
	virtual Bool doesSpecialPowerHaveOverridableDestinationActive() const override { return false; }
	virtual Bool doesSpecialPowerHaveOverridableDestination() const override { return false; }
	virtual void setSpecialPowerOverridableDestination( const Coord3D *loc ) override {}
	virtual SpecialPowerUpdateInterface* getSpecialPowerUpdateInterface() override { return this; }
	virtual CommandOption getCommandOption() const override { return (CommandOption)0; }
	virtual Bool isPowerCurrentlyInUse( const CommandButton *command = nullptr ) const override { return m_stage != STAGE_IDLE; }
	virtual Bool refusesSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos ) const override;
	virtual Bool isSpecialPowerUnavailable( const SpecialPowerTemplate *specialPowerTemplate ) const override;

	virtual UpdateSleepTime update() override;
	virtual void onDelete() override;
	// (its grip is its to end: it goes on while the unit is disabled, and lets go)
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return m_stage != STAGE_IDLE ? DISABLEDMASK_ALL : DISABLEDMASK_NONE; }

	Bool isHolding() const { return m_stage == STAGE_HOLD; }
	ObjectID getVictimID() const { return m_stage == STAGE_HOLD ? m_victimID : INVALID_ID; }

private:

	enum Stage CPP_11(: Int)
	{
		STAGE_IDLE = 0,
		STAGE_APPROACH,
		STAGE_HOLD,
	};

	Bool isValidVictim( const Object *victim, Bool held = FALSE ) const;	///< one it may take (held: hold on to)
	Bool showsReleaseCondition() const;
	const GrabVictimBehavior *behaviorFor( const Object *victim ) const;
	void grab( Object *victim );
	void release( Bool recharge );
	void stop();																					///< the approach given up
	Bool holdPoint( const GrabVictimBehavior *behavior, const Object *victim, Coord3D *pos ) const;

	Stage m_stage;
	ObjectID m_victimID;
	UnsignedInt m_stageStartFrame;
	UnsignedInt m_nextShotFrame;
	Int m_behaviorIndex;														///< the VictimBehavior it holds by (-1: none)
	Bool m_moveIssued;
	Bool m_attached;																///< dragged all the way in: carried at the bone from then on
	ObjectStatusMaskType m_selfStatusAdded;					///< what it set on itself (and so clears)
	ObjectStatusMaskType m_victimStatusAdded;				///< ... on the victim
	ModelConditionFlags m_selfConditionsAdded;
	AsciiString m_formerCommandSetOverride;
	Bool m_commandSetChanged;
};
