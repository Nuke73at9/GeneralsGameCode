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

// FILE: DecoyArmyUpdate.h ////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature the decoy armies (Kane's Wrath's StoreObjectsSpecialPower and
// CloneStoredObjectsSpecialPowerUpdate, C&C4's Decoy Army): copies of its player's units that look
// the same to the enemy and are worth nothing. ClonedModifier is put on each copy for
// ClonedObjectLifetime (C&C4's: 400% damage taken and none dealt; Kane's Wrath's: 1% of the health
// and damage, half the sight), and a copy dies as it runs out (the modifier's KillOnExpire).
//
//   Mode = COPY  : copies what is within Radius of the spot, each beside the one it copies (C&C4)
//   Mode = STORE : remembers what is within Radius of the spot, for its player's SPAWN (Kane's
//                  Wrath's first click: its player's objects with a STORE module all remember it)
//   Mode = SPAWN : copies what was last remembered, around the spot, between MinDestinationRadius
//                  and MaxDestinationRadius of it (Kane's Wrath's second click); refused while
//                  nothing is remembered
//
//   Behavior = SpecialAbility ModuleTag_DecoySpawnPower
//     SpecialPowerTemplate = KWSP_SpecialPowerCloneObjectSpawn
//   End
//   Behavior = DecoyArmyUpdate ModuleTag_DecoySpawn
//     SpecialPowerTemplate = KWSP_SpecialPowerCloneObjectSpawn
//     Mode                 = SPAWN
//     MinDestinationRadius = 10
//     MaxDestinationRadius = 100
//     ClonedObjectLifetime = 59000
//     ClonedModifier       = KWAM_AttributeModifier_Cloned
//     TriggerFX            = FX_NodDecoyArmy
//   End
//
// Affects (default ALLIES), KindOf and ForbiddenKindOf choose what it copies (SAGE's ObjectFilter).
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/INI.h"
#include "Common/KindOf.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/SpecialPowerUpdateModule.h"

class FXList;
class SpecialPowerTemplate;
class Waypoint;

enum DecoyArmyMode CPP_11(: Int)
{
	DECOY_ARMY_COPY = 0,
	DECOY_ARMY_STORE,
	DECOY_ARMY_SPAWN,
	DECOY_ARMY_MODE_COUNT
};

//-------------------------------------------------------------------------------------------------
class DecoyArmyUpdateModuleData : public UpdateModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;
	Int m_mode;										///< DecoyArmyMode
	Real m_radius;								///< what it copies or remembers: within this of the spot
	UnsignedInt m_affects;				///< WeaponAffectsMaskType: of these relationships (default ALLIES)
	KindOfMaskType m_kindOf;			///< any of these (none: all)
	KindOfMaskType m_forbiddenKindOf;	///< none of these
	Real m_minDestinationRadius;	///< SPAWN: the copies between this ...
	Real m_maxDestinationRadius;	///< ... and this of the spot
	UnsignedInt m_lifetime;				///< how long a copy lasts (frames; 0: for good)
	NameKeyType m_modifier;				///< put on each copy for its lifetime
	const FXList *m_triggerFX;		///< at the spot, as it acts

	DecoyArmyUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class DecoyArmyUpdate : public SpecialPowerUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( DecoyArmyUpdate, "DecoyArmyUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( DecoyArmyUpdate, DecoyArmyUpdateModuleData )

public:

	DecoyArmyUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	// SpecialPowerUpdateInterface
	virtual Bool initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions ) override;
	virtual Bool refusesSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos ) const override;
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

	Bool isStoring() const;	///< a STORE module
	Bool hasStored() const { return !m_stored.empty(); }
	void setStored( const std::vector<AsciiString> &names, const std::vector<Int> &levels );	///< (STORE) what to copy
	void clearStored();

private:

	struct Copy
	{
		const ThingTemplate *tmpl;
		Int level;				///< VeterancyLevel
		Coord3D beside;		///< COPY: beside here
		Real angle;
	};

	void gather( const Coord3D *spot, std::vector<Copy> &out ) const;	///< what it copies within Radius
	void makeCopies( const std::vector<Copy> &copies, const Coord3D *spot ) const;
	const DecoyArmyUpdate *findStore() const;	///< (SPAWN) the STORE module on its object

	std::vector<AsciiString> m_stored;	///< (STORE) what it remembers: the things' names ...
	std::vector<Int> m_storedLevels;		///< ... and their veterancy

};
