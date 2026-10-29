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

// FILE: ModifierAbilityUpdate.h //////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's special powers that work by attribute modifiers (C&C4's
// SpecialPower AttributeModifier, a berserk or an armour-up on the unit itself, and its
// PurgeBuffsSpecialPower, the cleanse of an area): when its power is used, the object puts its
// AttributeModifier on what is in Radius of the spot (of itself, with no target; only the target
// or itself, with no Radius) and Affects takes in, and purges up to MaxBuffsToPurgePerUnit of the
// enemies' modifiers of the classes in PurgeEnemyBuffs and of the allies' in PurgeAlliedBuffs.
// With StartAbilityRange, it first goes within that of its target.
//
//   Behavior = SpecialAbility ModuleTag_PurgePower
//     SpecialPowerTemplate = C4SP_PurgeBuffs
//   End
//   Behavior = ModifierAbilityUpdate ModuleTag_Purge
//     SpecialPowerTemplate   = C4SP_PurgeBuffs
//     Radius                 = 200
//     Affects                = SELF ALLIES ENEMIES
//     PurgeEnemyBuffs        = OFFENSE_POSITIVE DEFENSE_POSITIVE
//     PurgeAlliedBuffs       = OFFENSE_NEGATIVE DEFENSE_NEGATIVE
//     MaxBuffsToPurgePerUnit = 1
//     StartAbilityRange      = 200
//     TargetFX               = FX_SystemRestore
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/INI.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/UpdateModule.h"

class CommandButton;
class FXList;
class SpecialPowerTemplate;
class Waypoint;

#include "GameLogic/Module/SpecialPowerUpdateModule.h"

//-------------------------------------------------------------------------------------------------
class ModifierAbilityUpdateModuleData : public UpdateModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;
	NameKeyType m_modifier;						///< put on what it reaches (none: it only purges)
	Real m_radius;										///< around the spot (0: the target, or itself)
	UnsignedInt m_affects;						///< WeaponAffectsMaskType: SELF ALLIES ENEMIES NEUTRALS
	UnsignedInt m_purgeEnemyBuffs;		///< classes (1 << AttributeModifierBuff) purged from enemies
	UnsignedInt m_purgeAlliedBuffs;		///< and from allies (itself among them)
	Int m_maxPurgePerUnit;						///< 0: all
	Real m_startAbilityRange;					///< 0: where it is
	const FXList *m_initiateFX;				///< on itself, when it acts
	const FXList *m_targetFX;					///< on each object it reaches

	ModifierAbilityUpdateModuleData()
	{
		m_specialPowerTemplate = nullptr;
		m_modifier = NAMEKEY_INVALID;
		m_radius = 0.0f;
		m_affects = WEAPON_AFFECTS_SELF;
		m_purgeEnemyBuffs = 0;
		m_purgeAlliedBuffs = 0;
		m_maxPurgePerUnit = 0;
		m_startAbilityRange = 0.0f;
		m_initiateFX = nullptr;
		m_targetFX = nullptr;
	}

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpdateModuleData::buildFieldParse( p );
		static const FieldParse dataFieldParse[] =
		{
			{ "SpecialPowerTemplate",		INI::parseSpecialPowerTemplate,	nullptr, offsetof( ModifierAbilityUpdateModuleData, m_specialPowerTemplate ) },
			{ "AttributeModifier",			AttributeModifierStore::parseModifierName,	nullptr, offsetof( ModifierAbilityUpdateModuleData, m_modifier ) },
			{ "Radius",									INI::parseReal,									nullptr, offsetof( ModifierAbilityUpdateModuleData, m_radius ) },
			{ "Affects",								INI::parseBitString32,					TheWeaponAffectsMaskNames, offsetof( ModifierAbilityUpdateModuleData, m_affects ) },
			{ "PurgeEnemyBuffs",				INI::parseBitString32,					TheAttributeModifierBuffNames, offsetof( ModifierAbilityUpdateModuleData, m_purgeEnemyBuffs ) },
			{ "PurgeAlliedBuffs",				INI::parseBitString32,					TheAttributeModifierBuffNames, offsetof( ModifierAbilityUpdateModuleData, m_purgeAlliedBuffs ) },
			{ "MaxBuffsToPurgePerUnit",	INI::parseInt,									nullptr, offsetof( ModifierAbilityUpdateModuleData, m_maxPurgePerUnit ) },
			{ "StartAbilityRange",			INI::parseReal,									nullptr, offsetof( ModifierAbilityUpdateModuleData, m_startAbilityRange ) },
			{ "InitiateFX",							INI::parseFXList,								nullptr, offsetof( ModifierAbilityUpdateModuleData, m_initiateFX ) },
			{ "TargetFX",								INI::parseFXList,								nullptr, offsetof( ModifierAbilityUpdateModuleData, m_targetFX ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add( dataFieldParse );
	}
};

//-------------------------------------------------------------------------------------------------
class ModifierAbilityUpdate : public SpecialPowerUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ModifierAbilityUpdate, "ModifierAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ModifierAbilityUpdate, ModifierAbilityUpdateModuleData )

public:

	ModifierAbilityUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	// SpecialPowerUpdateInterface
	virtual Bool initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions ) override;
	// (not a SpecialAbilityUpdate: Object::findSpecialAbilityUpdate takes any special ability for one)
	virtual Bool isSpecialAbility() const override { return false; }
	virtual Bool isSpecialPower() const override { return false; }
	virtual Bool isActive() const override { return m_approaching; }
	virtual Bool doesSpecialPowerHaveOverridableDestinationActive() const override { return false; }
	virtual Bool doesSpecialPowerHaveOverridableDestination() const override { return false; }
	virtual void setSpecialPowerOverridableDestination( const Coord3D *loc ) override {}
	virtual SpecialPowerUpdateInterface* getSpecialPowerUpdateInterface() override { return this; }
	virtual CommandOption getCommandOption() const override { return (CommandOption)0; }
	virtual Bool isPowerCurrentlyInUse( const CommandButton *command = nullptr ) const override { return m_approaching; }

	virtual UpdateSleepTime update() override;

private:

	void act();
	Bool targetSpot( Coord3D *spot ) const;	///< where it acts; FALSE when its target is gone

	Bool m_approaching;			///< on its way to within StartAbilityRange of its target
	Bool m_hasTarget;				///< an object or a spot was given (not itself)
	ObjectID m_targetID;		///< the object given
	Coord3D m_targetPos;		///< the spot given

};
