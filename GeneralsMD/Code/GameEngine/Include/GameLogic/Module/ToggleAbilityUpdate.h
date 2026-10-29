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

// FILE: ToggleAbilityUpdate.h ////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's ToggleStatusSpecialAbilityUpdate (and MovementSpecialPower, a
// toggle that turns itself back): a special power that switches a unit between its normal state and a
// second one: deployed, burrowed, armoured up, sprinting. Each state can set model conditions, and
// the second can set the ABILITY_TOGGLED weapon set, the upgraded locomotor (SET_NORMAL_UPGRADED: a
// faster one for a sprint, one that does not move for a deployed form) and object status (CAN_STEALTH
// for a burrowed unit). Switching takes TransitionTime (UNPACKING into the second state, PACKING back,
// or the ToggleInConditions and ToggleOutConditions given; TransitionBackTime for the way back, when it
// differs), the unit standing still; with AutoRevertTime the second state ends by itself. The second
// state can carry an attribute modifier (ToggledAttributeModifier: a burrowed unit's armour).
//
//   Behavior = SpecialAbility ModuleTag_DeployPower
//     SpecialPowerTemplate = C4SP_Deploy
//   End
//   Behavior = ToggleAbilityUpdate ModuleTag_Deploy
//     SpecialPowerTemplate = C4SP_Deploy
//     TransitionTime       = 1000
//     ToggledConditions    = DEPLOYED
//     ToggledWeaponSet     = Yes
//     ToggledLocomotor     = Yes
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/INI.h"
#include "Common/ModelState.h"
#include "Common/ObjectStatusTypes.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/Module/UpdateModule.h"

class CommandButton;
class SpecialPowerTemplate;
class Waypoint;

#include "GameLogic/Module/SpecialPowerUpdateModule.h"

//-------------------------------------------------------------------------------------------------
class ToggleAbilityUpdateModuleData : public UpdateModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;
	UnsignedInt m_transitionTime;		///< frames
	UnsignedInt m_transitionBackTime;	///< frames back to the first state (when m_hasBackTime)
	Bool m_hasBackTime;
	ModelConditionFlags m_toggleInConditions;	///< shown while switching into the second state (none given: UNPACKING)
	ModelConditionFlags m_toggleOutConditions;	///< and back (none given: PACKING)
	UnsignedInt m_autoRevertTime;		///< frames in the second state before it ends by itself (0: until toggled)
	ModelConditionFlags m_normalConditions;	///< set in the first state
	ModelConditionFlags m_toggledConditions;	///< set in the second
	ObjectStatusMaskType m_toggledStatus;		///< set in the second
	Bool m_toggledWeaponSet;			///< WEAPONSET_ABILITY_TOGGLED in the second
	Bool m_toggledLocomotor;			///< SET_NORMAL_UPGRADED in the second
	NameKeyType m_toggledModifier;		///< an attribute modifier the second carries (AttributeModifier.h)

	ToggleAbilityUpdateModuleData()
	{
		m_specialPowerTemplate = nullptr;
		m_transitionTime = 0;
		m_transitionBackTime = 0;
		m_hasBackTime = FALSE;
		m_autoRevertTime = 0;
		m_toggledWeaponSet = FALSE;
		m_toggledLocomotor = FALSE;
		m_toggledModifier = NAMEKEY_INVALID;
	}

	static void parseBackTime( INI *ini, void *instance, void *store, const void *userData )
	{
		ToggleAbilityUpdateModuleData *self = (ToggleAbilityUpdateModuleData *)instance;
		INI::parseDurationUnsignedInt( ini, instance, &self->m_transitionBackTime, userData );
		self->m_hasBackTime = TRUE;
	}

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpdateModuleData::buildFieldParse( p );
		static const FieldParse dataFieldParse[] =
		{
			{ "SpecialPowerTemplate",	INI::parseSpecialPowerTemplate,	nullptr, offsetof( ToggleAbilityUpdateModuleData, m_specialPowerTemplate ) },
			{ "TransitionTime",			INI::parseDurationUnsignedInt,	nullptr, offsetof( ToggleAbilityUpdateModuleData, m_transitionTime ) },
			{ "AutoRevertTime",			INI::parseDurationUnsignedInt,	nullptr, offsetof( ToggleAbilityUpdateModuleData, m_autoRevertTime ) },
			{ "TransitionBackTime",		parseBackTime,					nullptr, 0 },
			{ "ToggleInConditions",		ModelConditionFlags::parseFromINI,	nullptr, offsetof( ToggleAbilityUpdateModuleData, m_toggleInConditions ) },
			{ "ToggleOutConditions",	ModelConditionFlags::parseFromINI,	nullptr, offsetof( ToggleAbilityUpdateModuleData, m_toggleOutConditions ) },
			{ "NormalConditions",		ModelConditionFlags::parseFromINI,	nullptr, offsetof( ToggleAbilityUpdateModuleData, m_normalConditions ) },
			{ "ToggledConditions",		ModelConditionFlags::parseFromINI,	nullptr, offsetof( ToggleAbilityUpdateModuleData, m_toggledConditions ) },
			{ "ToggledStatus",			ObjectStatusMaskType::parseFromINI,	nullptr, offsetof( ToggleAbilityUpdateModuleData, m_toggledStatus ) },
			{ "ToggledWeaponSet",		INI::parseBool,					nullptr, offsetof( ToggleAbilityUpdateModuleData, m_toggledWeaponSet ) },
			{ "ToggledLocomotor",		INI::parseBool,					nullptr, offsetof( ToggleAbilityUpdateModuleData, m_toggledLocomotor ) },
			{ "ToggledAttributeModifier",	AttributeModifierStore::parseModifierName,	nullptr, offsetof( ToggleAbilityUpdateModuleData, m_toggledModifier ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add( dataFieldParse );
	}
};

//-------------------------------------------------------------------------------------------------
class ToggleAbilityUpdate : public SpecialPowerUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ToggleAbilityUpdate, "ToggleAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ToggleAbilityUpdate, ToggleAbilityUpdateModuleData )

public:

	ToggleAbilityUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	// SpecialPowerUpdateInterface
	virtual Bool initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions ) override;
	// (not a SpecialAbilityUpdate: Object::findSpecialAbilityUpdate takes any special ability for one)
	virtual Bool isSpecialAbility() const override { return false; }
	virtual Bool isSpecialPower() const override { return false; }
	virtual Bool isActive() const override { return m_toggled || m_switching; }
	virtual Bool doesSpecialPowerHaveOverridableDestinationActive() const override { return false; }
	virtual Bool doesSpecialPowerHaveOverridableDestination() const override { return false; }
	virtual void setSpecialPowerOverridableDestination( const Coord3D *loc ) override {}
	virtual SpecialPowerUpdateInterface* getSpecialPowerUpdateInterface() override { return this; }
	virtual CommandOption getCommandOption() const override { return (CommandOption)0; }
	virtual Bool isPowerCurrentlyInUse( const CommandButton *command = nullptr ) const override { return m_switching; }

	virtual UpdateSleepTime update() override;

private:

	void beginSwitch();
	void applyState( Bool toggled );
	ModelConditionFlags switchingConditions( Bool intoToggled ) const;

	Bool m_initialized;		///< the first state's conditions are set (once there is a drawable)
	Bool m_toggled;			///< in the second state (or switching from it)
	Bool m_switching;
	UnsignedInt m_switchEndFrame;
	UnsignedInt m_revertFrame;	///< when an auto-reverting second state ends (0: none)

};
