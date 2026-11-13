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

// FILE: NotifyWhenAttackingBehavior.h ////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's NotifyWhenAttackingBehavior and AttackAssistResponseBehavior: defences that
// fire together. While the object attacks with the weapon of WeaponSlot, every SleepTime it calls on its nearest
// friends (allies of the kinds named in Friend, within NotifyRadius, MaxFriendsToNotify at most); each friend with
// an AttackAssistResponseBehavior fires the weapon of its own WeaponSlot at the caller when that weapon is ready
// and in reach. C&C4's Obelisks of Light, as Red Alert 2's prism towers: the friends' link beams each raise the
// attacker's damage for its shot.
//
//   Behavior = NotifyWhenAttackingBehavior ModuleTag_NotifyFriends
//     NotifyRadius       = 544
//     SleepTime          = 100
//     MaxFriendsToNotify = 2
//     Friend             = C4_NODStructureG C4_NODCrawlerAStructure C4_NODMegaliskObelisk
//     WeaponSlot         = PRIMARY
//   End
//   Behavior = AttackAssistResponseBehavior ModuleTag_AttackAssistResponse
//     WeaponSlot = SECONDARY
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/WeaponSet.h"

//-------------------------------------------------------------------------------------------------
class NotifyWhenAttackingBehaviorModuleData : public UpdateModuleData
{
public:
	Real m_notifyRadius;											///< how far its friends may stand
	UnsignedInt m_sleepTime;									///< frames between its calls while it attacks
	Int m_maxFriends;													///< how many it calls at most
	std::vector<AsciiString> m_friendNames;		///< the kinds it calls
	WeaponSlotType m_weaponSlot;							///< its weapon whose attack calls them

	NotifyWhenAttackingBehaviorModuleData();
	static void buildFieldParse( MultiIniFieldParse &p );
};

//-------------------------------------------------------------------------------------------------
class NotifyWhenAttackingBehavior : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( NotifyWhenAttackingBehavior, "NotifyWhenAttackingBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( NotifyWhenAttackingBehavior, NotifyWhenAttackingBehaviorModuleData )

public:
	NotifyWhenAttackingBehavior( Thing *thing, const ModuleData *moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

	Int getLastFriendCount() const { return m_lastFriendCount; }	///< how many it called last (the test's reading)

private:
	Bool isAttacking() const;

	Int m_lastFriendCount;		///< how many it called on its last call (not saved: the next call sets it)
};

//-------------------------------------------------------------------------------------------------
class AttackAssistResponseBehaviorModuleData : public UpdateModuleData
{
public:
	WeaponSlotType m_weaponSlot;							///< the weapon it fires at who calls it

	AttackAssistResponseBehaviorModuleData();
	static void buildFieldParse( MultiIniFieldParse &p );
};

//-------------------------------------------------------------------------------------------------
class AttackAssistResponseBehavior : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AttackAssistResponseBehavior, "AttackAssistResponseBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AttackAssistResponseBehavior, AttackAssistResponseBehaviorModuleData )

public:
	AttackAssistResponseBehavior( Thing *thing, const ModuleData *moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override { return UPDATE_SLEEP_FOREVER; }

	Bool canAssist() const;						///< standing, built and not disabled
	Bool assist( Object *attacker );	///< fires its weapon at the caller if it is ready and in reach

	UnsignedInt getAssistCount() const { return m_assistCount; }	///< shots it has fired for others (the test's reading)

private:
	UnsignedInt m_assistCount;
};
