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

// FILE: AttributeModifierUpgrade.h ///////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's AttributeModifierUpgrade (C&C4's tech upgrades: the range, speed
// and reload a player buys for a class of units): once upgraded, the object carries its
// AttributeModifier (for the modifier's Duration, or for good when it has none).
//
//   Behavior = AttributeModifierUpgrade ModuleTag_RangeUpgrade
//     TriggeredBy       = C4Up_Upgrade_GDI_Offense_Range
//     AttributeModifier = C4AM_Modifier_Generic_GDIWeaponRange_T1
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/AttributeModifier.h"
#include "GameLogic/Module/UpgradeModule.h"

class Thing;

//-------------------------------------------------------------------------------------------------
class AttributeModifierUpgradeModuleData : public UpgradeModuleData
{
public:
	NameKeyType m_modifier;

	AttributeModifierUpgradeModuleData()
	{
		m_modifier = NAMEKEY_INVALID;
	}

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpgradeModuleData::buildFieldParse( p );
		static const FieldParse dataFieldParse[] =
		{
			{ "AttributeModifier",	AttributeModifierStore::parseModifierName,	nullptr, offsetof( AttributeModifierUpgradeModuleData, m_modifier ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add( dataFieldParse );
	}
};

//-------------------------------------------------------------------------------------------------
class AttributeModifierUpgrade : public UpgradeModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AttributeModifierUpgrade, "AttributeModifierUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AttributeModifierUpgrade, AttributeModifierUpgradeModuleData );

public:

	AttributeModifierUpgrade( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype defined by MemoryPoolObject

protected:
	virtual void upgradeImplementation() override;
	virtual Bool isSubObjectsUpgrade() override { return false; }

};
