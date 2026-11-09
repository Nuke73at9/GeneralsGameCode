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


// FILE: UpgradeCrateCollide.h ////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's UpgradeCrateCollide and VeterancyCrateCollide (C&C4's crates, which a unit drops
// as it dies): the unit that picks the crate up gets UpgradeToGrant, an upgrade of its own (C&C4's crate upgrade: the
// Attack Bike's better rockets), or, without one, LevelsToGain veterancy levels. Only the units named in
// AllowedCollectors take it (none named: any unit), none of ForbiddenCollectors. As it gains, it heals HealPerLevel of its
// health; a unit that has the upgrade already, or can rise no further, still takes the crate and heals HealAtMaxLevel
// (none: it leaves the crate where it is). PickupFX plays on it beside ExecuteFX (C&C4's crate has a flash and a sound).
//
//   Behavior = UpgradeCrateCollide ModuleTag_Upgrade
//     UpgradeToGrant = C4Up_Upgrade_Crate_Level1
//     AllowedCollectors = C4_NODVehicleB C4_GDIVehicleA
//     HealPerLevel = 10%
//     HealAtMaxLevel = 10%
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/Module.h"
#include "GameLogic/Module/CrateCollide.h"

class Thing;

//-------------------------------------------------------------------------------------------------
class UpgradeCrateCollideModuleData : public CrateCollideModuleData
{
public:
	AsciiString m_upgradeName;										///< the upgrade it gives (none: veterancy)
	Int m_levelsToGain;														///< the veterancy levels it gives, without an upgrade
	std::vector<AsciiString> m_allowedCollectors;	///< only these take it (none named: any)
	std::vector<AsciiString> m_forbiddenCollectors;	///< none of these
	Real m_healPerLevel;													///< of the collector's health, as it gains
	Real m_healAtMaxLevel;												///< ... as it cannot (0: it does not take the crate)
	const FXList *m_pickupFX;											///< played on the collector too (ExecuteFX's beside it: a sound)

	UpgradeCrateCollideModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class UpgradeCrateCollide : public CrateCollide
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( UpgradeCrateCollide, "UpgradeCrateCollide" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( UpgradeCrateCollide, UpgradeCrateCollideModuleData );

public:
	UpgradeCrateCollide( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

protected:
	virtual Bool isValidToExecute( const Object *other ) const override;
	virtual Bool executeCrateBehavior( Object *other ) override;

private:
	Bool canGain( const Object *other ) const;	///< it would get the upgrade or a level
};
