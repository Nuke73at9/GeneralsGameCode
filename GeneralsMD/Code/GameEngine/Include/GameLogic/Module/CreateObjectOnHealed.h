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

// FILE: CreateObjectOnHealed.h ///////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's CreateObjectOnHealed: healed whole, the object makes its CreationList and
// goes (DestroyParent): a husk mended back by its player's Backup Systems becomes the vehicle again, for that
// player. UpgradeRequired and UpgradeForbidden name the object's or its player's upgrades it waits on.
//
//   Behavior = CreateObjectOnHealed ModuleTag_Revive
//     CreationList = C4OCL_OCL_GDIVehicleAGHuskReplace
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/DamageModule.h"

class ObjectCreationList;

//-------------------------------------------------------------------------------------------------
class CreateObjectOnHealedModuleData : public DamageModuleData
{
public:
	const ObjectCreationList*	m_ocl;
	Bool											m_destroyParent;			///< gone once it has made it (Yes: C&C4's DESTROY_PARENT_ON_CREATE)
	std::vector<AsciiString>	m_upgradeRequired;
	std::vector<AsciiString>	m_upgradeForbidden;

	CreateObjectOnHealedModuleData();

	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class CreateObjectOnHealed : public DamageModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( CreateObjectOnHealed, "CreateObjectOnHealed" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( CreateObjectOnHealed, CreateObjectOnHealedModuleData )

public:

	CreateObjectOnHealed( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual void onDamage( DamageInfo *damageInfo ) override { }
	virtual void onHealing( DamageInfo *damageInfo ) override;
	virtual void onBodyDamageStateChange( const DamageInfo *damageInfo, BodyDamageType oldState, BodyDamageType newState ) override { }

};
