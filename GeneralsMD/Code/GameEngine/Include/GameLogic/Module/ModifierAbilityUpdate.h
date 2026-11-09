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
//
// KindOf and ForbiddenKindOf narrow what it reaches in an area (C&C4's and Kane's Wrath's player powers'
// ObjectFilter: Include and Exclude); SamePlayerOnly keeps to its own player's (SAGE's SAME_PLAYER);
// Global reaches what is anywhere on the map (C&C4's Voice of Kane, Kane's Wrath's scans).
//
// The SAGE 2 area powers do more to what they reach (Tiberium Project): UpgradeToGrant gives each an
// object upgrade (Kane's Wrath's GrantUpgradeAreaOfEffectSpecialPower: the Sonic Repulsion Field, Laser
// Fencing); GrantStealth hides each (with a StealthUpdate) for StealthTime, or for good (the Cloaking
// Fields); RefreshRecharge ends the recharges under way of each one's abilities (C&C4's Voice of Kane);
// RevealTime lifts the shroud over each for its player, RevealRadius past its edge (Kane's Wrath's
// Power Signature Scan). JamEnemyRadar darkens the radars of its player's enemies for that long (the
// Radar Jamming Missile); Weapon is fired at the spot (the infantry the Cloaking Field kills); Delay
// makes it act that long after its power is used (Kane's Wrath's unpacking).
//
//   Behavior = ModifierAbilityUpdate ModuleTag_LaserFencing
//     SpecialPowerTemplate = KWSP_SpecialPower_LaserFencing
//     Radius               = 10
//     Affects              = ALLIES
//     SamePlayerOnly       = Yes
//     KindOf               = STRUCTURE
//     UpgradeToGrant       = KWUpgrade_LaserFencing
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/INI.h"
#include "Common/KindOf.h"
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
	KindOfMaskType m_kindOf;					///< in an area: any of these (none: all)
	KindOfMaskType m_forbiddenKindOf;	///< in an area: none of these
	// Tiberium Project @feature the SAGE 2 area powers
	Bool m_global;										///< what is anywhere on the map, not in an area
	Bool m_samePlayerOnly;						///< only its own player's
	std::vector<AsciiString> m_upgradeNames;	///< object upgrades given to each object it reaches
	AsciiString m_weaponName;					///< fired at the spot as it acts
	Bool m_grantStealth;							///< each object it reaches hidden (with a StealthUpdate) ...
	UnsignedInt m_stealthFrames;			///< ... this long (0: for good)
	Bool m_refreshRecharge;						///< the recharges under way of each one's abilities end
	UnsignedInt m_jamRadarFrames;			///< the radars of its player's enemies dark this long
	UnsignedInt m_revealFrames;				///< the shroud lifted over each object it reaches this long, for its player
	Real m_revealRadius;							///< ... this far past its edge
	UnsignedInt m_delayFrames;				///< it acts this long after its power is used

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
		m_kindOf.clear();
		m_forbiddenKindOf.clear();
		m_global = FALSE;
		m_samePlayerOnly = FALSE;
		m_grantStealth = FALSE;
		m_stealthFrames = 0;
		m_refreshRecharge = FALSE;
		m_jamRadarFrames = 0;
		m_revealFrames = 0;
		m_revealRadius = 0.0f;
		m_delayFrames = 0;
	}

	static void parseUpgradeNames( INI *ini, void *instance, void *store, const void *userData );

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
			{ "KindOf",									KindOfMaskType::parseFromINI,		nullptr, offsetof( ModifierAbilityUpdateModuleData, m_kindOf ) },
			{ "ForbiddenKindOf",				KindOfMaskType::parseFromINI,		nullptr, offsetof( ModifierAbilityUpdateModuleData, m_forbiddenKindOf ) },
			{ "Global",									INI::parseBool,									nullptr, offsetof( ModifierAbilityUpdateModuleData, m_global ) },
			{ "SamePlayerOnly",					INI::parseBool,									nullptr, offsetof( ModifierAbilityUpdateModuleData, m_samePlayerOnly ) },
			{ "UpgradeToGrant",					parseUpgradeNames,							nullptr, offsetof( ModifierAbilityUpdateModuleData, m_upgradeNames ) },
			{ "Weapon",									INI::parseAsciiString,					nullptr, offsetof( ModifierAbilityUpdateModuleData, m_weaponName ) },
			{ "GrantStealth",						INI::parseBool,									nullptr, offsetof( ModifierAbilityUpdateModuleData, m_grantStealth ) },
			{ "StealthTime",						INI::parseDurationUnsignedInt,	nullptr, offsetof( ModifierAbilityUpdateModuleData, m_stealthFrames ) },
			{ "RefreshRecharge",				INI::parseBool,									nullptr, offsetof( ModifierAbilityUpdateModuleData, m_refreshRecharge ) },
			{ "JamEnemyRadar",					INI::parseDurationUnsignedInt,	nullptr, offsetof( ModifierAbilityUpdateModuleData, m_jamRadarFrames ) },
			{ "RevealTime",							INI::parseDurationUnsignedInt,	nullptr, offsetof( ModifierAbilityUpdateModuleData, m_revealFrames ) },
			{ "RevealRadius",						INI::parseReal,									nullptr, offsetof( ModifierAbilityUpdateModuleData, m_revealRadius ) },
			{ "Delay",									INI::parseDurationUnsignedInt,	nullptr, offsetof( ModifierAbilityUpdateModuleData, m_delayFrames ) },
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
	virtual void onDelete() override;

private:

	void act();
	Bool targetSpot( Coord3D *spot ) const;	///< where it acts; FALSE when its target is gone
	Bool reaches( const Object *other ) const;	///< Tiberium Project: an object it takes in (Affects, SamePlayerOnly, KindOf)
	void lowerReveals();	///< Tiberium Project: the shroud it lifted falls again
	UpdateSleepTime nextWake() const;

	Bool m_approaching;			///< on its way to within StartAbilityRange of its target
	Bool m_hasTarget;				///< an object or a spot was given (not itself)
	ObjectID m_targetID;		///< the object given
	Coord3D m_targetPos;		///< the spot given
	UnsignedInt m_actFrame;	///< Tiberium Project: when it acts (Delay); 0: not waiting
	UnsignedInt m_revealUntil;	///< Tiberium Project: when the shroud it lifted falls again; 0: none lifted
	UnsignedInt m_revealMask;		///< ... for whom (a player mask)
	std::vector<Coord3D> m_reveals;	///< ... where: x, y and the radius (as z)

};
