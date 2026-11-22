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

// FILE: FXListUpgrade.h //////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's FXListBehavior onUpgraded event (C&C4's upgrade effects: the shield
// generator's flare, the Composite Armor's plates, the stealth generator's shimmer): as the object is
// upgraded, the effect list plays on it.
//
//   Behavior = FXListUpgrade ModuleTag_CompositeArmorFX
//     TriggeredBy = C4Up_Upgrade_GDI_Offense_CrawlerTech2
//     FX          = C4FX_FX_UpgradeGDICompositeArmor
//   End
// Its look only: an effect list changes nothing in the game's logic.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpgradeModule.h"

class FXList;
class Thing;

//-------------------------------------------------------------------------------------------------
class FXListUpgradeModuleData : public UpgradeModuleData
{
public:
	const FXList *m_fx;

	FXListUpgradeModuleData()
	{
		m_fx = nullptr;
	}

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpgradeModuleData::buildFieldParse( p );
		static const FieldParse dataFieldParse[] =
		{
			{ "FX",	INI::parseFXList,	nullptr, offsetof( FXListUpgradeModuleData, m_fx ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add( dataFieldParse );
	}
};

//-------------------------------------------------------------------------------------------------
class FXListUpgrade : public UpgradeModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( FXListUpgrade, "FXListUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( FXListUpgrade, FXListUpgradeModuleData );

public:

	FXListUpgrade( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype defined by MemoryPoolObject

protected:
	virtual void upgradeImplementation() override;
	virtual Bool isSubObjectsUpgrade() override { return false; }

};
