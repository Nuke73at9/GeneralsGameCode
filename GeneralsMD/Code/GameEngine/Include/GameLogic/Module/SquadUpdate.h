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

// FILE: SquadUpdate.h ////////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature a squad of C&C3 (Tiberium Wars, Kane's Wrath: SAGE's HordeContain): one unit
// the player selects and orders, made of soldiers that stand in its formation. The squad is an object
// of its own, unseen, with a SpawnBehavior that makes its soldiers (AggregateHealth: their health is
// its own; it is gone with the last of them) and this module, which keeps them in their places: a
// soldier (SquadMemberUpdate) takes the first free Position, turned with the squad, as it joins.
//   - the squad moved (or attack-moved): each soldier goes (or attack-moves) to its place at the goal,
//     the formation turned towards it;
//   - the squad stands: an idle soldier away from its place goes back to it;
//   - its soldiers fight (their own targets, or the squad's, which SpawnBehavior hands them): they
//     leave their places, and the squad follows the middle of those fighting when it is farther
//     than FollowRadius, so they form up round it again once the fight is over.
// The squad's stance is its soldiers'. Selecting a soldier selects the squad (SquadMemberUpdate).
//
//   Behavior = SquadUpdate ModuleTag_Squad
//     Position = X:0 Y:10          ; the places, in the order the soldiers join (C&C3's RankInfo)
//     Position = X:10 Y:0
//     SlotTolerance = 8            ; how far from its place a standing soldier may be
//     FollowRadius = 40
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpdateModule.h"

#include <vector>

//-------------------------------------------------------------------------------------------------
struct AICommandParms;

//-------------------------------------------------------------------------------------------------
class SquadUpdateModuleData : public UpdateModuleData
{
public:
	std::vector<Coord2D> m_positions;
	Real m_slotTolerance;
	Real m_followRadius;

	SquadUpdateModuleData()
	{
		m_slotTolerance = 8.0f;
		m_followRadius = 40.0f;
	}

	static void parsePosition( INI *ini, void *instance, void *store, const void *userData );

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpdateModuleData::buildFieldParse( p );
		static const FieldParse dataFieldParse[] =
		{
			{ "Position",				SquadUpdateModuleData::parsePosition,	nullptr, 0 },
			{ "SlotTolerance",	INI::parseReal,												nullptr, offsetof( SquadUpdateModuleData, m_slotTolerance ) },
			{ "FollowRadius",		INI::parseReal,												nullptr, offsetof( SquadUpdateModuleData, m_followRadius ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add( dataFieldParse );
	}
};

//-------------------------------------------------------------------------------------------------
class SquadUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( SquadUpdate, "SquadUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( SquadUpdate, SquadUpdateModuleData )

public:
	SquadUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;
	/// the squad changed hands (captured, a script): its soldiers go with it
	virtual void onCapture( Player *oldOwner, Player *newOwner ) override;

	/// a soldier joins (SquadMemberUpdate::onEnslave): the first place nobody living holds
	void addMember( const Object *member );
	Int getMemberCount() const;									///< its soldiers alive
	void getSoldiers( std::vector<Object *> &out ) const;	///< its soldiers alive and not inside anything, by place
	/// the squad a soldier is in (its SquadMemberUpdate's slaver, while that has this module), or null
	static Object *getSquadOf( const Object *member );
	static SquadUpdate *getSquadUpdate( const Object *squad );

	/// an order given to the squad (AIUpdateInterface::aiDoCommand, whoever gives it: the player, a script, its AI team):
	/// an attack is its soldiers', as it has no weapon of its own; so is getting into a transport or a building, which the
	/// squad itself then takes no part in (TRUE: the squad does nothing with it)
	Bool passOrder( const AICommandParms *parms );
	/// what to hit for an attack on a squad (AIUpdateInterface::aiDoCommand): its soldier nearest the attacker; null when
	/// 'target' is no squad or has none left
	static Object *getSoldierToAttack( const Object *target, const Object *attacker );

private:
	/// where place k is with the squad's middle at 'center', the formation facing (c, s)
	Coord3D placeAt( Int k, const Coord3D &center, Real c, Real s ) const;

	std::vector<ObjectID> m_members;	///< place k's soldier (INVALID_ID: free)
	Coord3D m_lastGoal;								///< the goal the soldiers were last sent to
	Bool m_hadGoal;
	Bool m_selfMove;									///< the squad itself moved to follow its fighting soldiers
	Coord3D m_selfGoal;
	ObjectID m_targetSquad;						///< the squad it was told to attack: its soldiers go on until all of that one's are dead
	Bool m_targetForced;
	Int m_targetSource;								///< (CommandSourceType) who told it
	Bool m_ownCommand;								///< (not saved) a command it gives itself, not an order to pass on
};
