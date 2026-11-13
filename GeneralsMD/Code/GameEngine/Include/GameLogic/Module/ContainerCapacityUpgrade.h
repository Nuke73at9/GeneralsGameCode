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

// FILE: ContainerCapacityUpgrade.h ////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's ContainerCapacityUpgrade: an upgrade that changes how many its object's
// container holds (C&C4's Widow carries four more once it has its crate upgrade). With UndoWhenRemoved the slots
// go again with the upgrade, and those then left without a place get out (OnRemoving = EJECT_EXTRA_PASSENGERS,
// the default) or die (KILL_EXTRA_PASSENGERS).
//
//   Behavior = ContainerCapacityUpgrade ModuleTag_MoreRoom
//     TriggeredBy  = C4Up_Upgrade_Crate_Level1
//     SlotModifier = 4
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpgradeModule.h"

class Thing;

//-------------------------------------------------------------------------------------------------
class ContainerCapacityUpgradeModuleData : public UpgradeModuleData
{
public:
	Int m_slotModifier;			///< slots added (fewer with a negative number)
	Bool m_killExtra;				///< as the slots go again, those without a place die (else they get out)

	ContainerCapacityUpgradeModuleData();
	static void buildFieldParse( MultiIniFieldParse &p );
};

//-------------------------------------------------------------------------------------------------
class ContainerCapacityUpgrade : public UpgradeModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ContainerCapacityUpgrade, "ContainerCapacityUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ContainerCapacityUpgrade, ContainerCapacityUpgradeModuleData );

public:

	ContainerCapacityUpgrade( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype defined by MemoryPoolObject

protected:
	virtual void upgradeImplementation() override;			///< the slots added
	virtual void undoUpgradeImplementation() override;		///< and taken back (UndoWhenRemoved)
	virtual Bool isSubObjectsUpgrade() override { return false; }

};
