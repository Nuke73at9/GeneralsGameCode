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

// FILE: StoredResourceUpdate.h ///////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's StoredResourceBehavior and StoredResourceUpdate in one: a store
// the object fills, up to Capacity. Every UpdateInterval it gains UpdateValue, while it is whole
// (not being built or sold, not disabled) and has RequiredStatus (none of ForbiddenStatus); a
// weapon adds to it or empties it (ModifyStoredResource). Each Level is reached at its amount:
// while the store holds that much the object has the level's object Upgrade and attribute
// Modifier; its OCL is made once as it is reached; with Reset the store empties then. With the
// store full the object shows ConditionFull. ShowInUI draws the store as a bar under the health
// bar, with a mark at each level.
//
// C&C4's superweapons charge so: the Ion Cannon's six levels (each one a stronger strike, the
// strike object made with the structure's upgrades), the Temple of Nod's three; firing takes the
// charge back to nothing (the weapon's ModifyStoredResource), the levels' upgrades with it.
//
//   Behavior = StoredResourceUpdate ModuleTag_Charge
//     Capacity       = 100
//     UpdateInterval = 2000
//     UpdateValue    = 1
//     ConditionFull  = USER_8
//     ShowInUI       = Yes
//     Level          = 15 Upgrade:C4Up_Upgrade_GDIStructureJSuperweapon_Level1
//     Level          = 30 Upgrade:C4Up_Upgrade_GDIStructureJSuperweapon_Level2
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "Common/INI.h"
#include "Common/ModelState.h"
#include "Common/ObjectStatusTypes.h"
#include "GameLogic/Module/UpdateModule.h"

class Object;

//-------------------------------------------------------------------------------------------------
struct StoredResourceLevel
{
	Int m_amount;						///< reached with this much in the store
	AsciiString m_upgradeName;		///< an object upgrade had while it is reached (found when used)
	NameKeyType m_modifier;			///< an attribute modifier on while it is reached
	AsciiString m_oclName;			///< made once, as it is reached
	Bool m_reset;						///< the store empties as it is reached
};

//-------------------------------------------------------------------------------------------------
class StoredResourceUpdateModuleData : public UpdateModuleData
{
public:
	Int m_capacity;
	UnsignedInt m_interval;			///< frames between gains (0: none, a weapon fills it)
	Int m_value;						///< gained each interval
	ObjectStatusMaskType m_requiredStatus;
	ObjectStatusMaskType m_forbiddenStatus;
	ModelConditionFlagType m_conditionFull;
	Bool m_showInUI;
	std::vector<StoredResourceLevel> m_levels;

	StoredResourceUpdateModuleData();

	static void parseLevel( INI *ini, void *instance, void *store, const void *userData );
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class StoredResourceUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( StoredResourceUpdate, "StoredResourceUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( StoredResourceUpdate, StoredResourceUpdateModuleData )

public:

	StoredResourceUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	static StoredResourceUpdate *find( const Object *obj );

	virtual UpdateSleepTime update() override;
	// (it looks at whether the object is disabled itself: no gain then, the count goes on)
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }

	void addAmount( Int delta );		///< a weapon's ModifyStoredResource: within 0 to Capacity
	Int getAmount() const { return m_amount; }
	Int getCapacity() const { return getStoredResourceUpdateModuleData()->m_capacity; }
	Real getFillRatio() const;			///< 0 to 1
	Bool showsInUI() const { return getStoredResourceUpdateModuleData()->m_showInUI; }
	Int getLevelCount() const { return (Int)getStoredResourceUpdateModuleData()->m_levels.size(); }
	Int getLevelAmount( Int i ) const { return getStoredResourceUpdateModuleData()->m_levels[i].m_amount; }

private:

	void applyLevels();

	Int m_amount;
	UnsignedInt m_reachedMask;	///< the levels reached (a bit per Level)
	Bool m_full;						///< ConditionFull is shown

};
