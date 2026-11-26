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

// FILE: SquadMemberUpdate.h //////////////////////////////////////////////////////////////////////
// Tiberium Project @feature a soldier of a C&C3 squad (SquadUpdate): the slave its squad's SpawnBehavior
// makes. It takes a place in the squad's formation as it joins; the squad moves it. Selected, it stands
// for its squad: the squad is selected and ordered in its stead (InGameUI, GameLogic's selected group).
// If its squad is gone while it lives, it is a soldier of its own.
//
//   Behavior = SquadMemberUpdate ModuleTag_SquadMember
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpdateModule.h"

//-------------------------------------------------------------------------------------------------
class SquadMemberUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpdateModuleData::buildFieldParse( p );
	}
};

//-------------------------------------------------------------------------------------------------
class SquadMemberUpdate : public UpdateModule, public SlavedUpdateInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( SquadMemberUpdate, "SquadMemberUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( SquadMemberUpdate, SquadMemberUpdateModuleData )

public:
	SquadMemberUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual SlavedUpdateInterface* getSlavedUpdateInterface() override { return this; }

	virtual ObjectID getSlaverID() const override { return m_squad; }
	virtual void onEnslave( const Object *slaver ) override;
	virtual void onSlaverDie( const DamageInfo *info ) override;
	virtual void onSlaverDamage( const DamageInfo *info ) override {}
	virtual Bool isSelfTasking() const override { return FALSE; }

	virtual UpdateSleepTime update() override;

	/// the squad obj stands for when it is selected (itself when it is in none)
	static const Object *getSelectionStandIn( const Object *obj );
	/// a soldier that joins a squad without being made by it (its banner carrier, BannerCarrierUpdate): one with this module
	static void enlist( Object *member, Object *squad );

private:
	ObjectID m_squad;
};
