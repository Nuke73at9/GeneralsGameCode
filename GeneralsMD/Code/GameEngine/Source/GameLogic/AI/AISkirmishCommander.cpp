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

// FILE: AISkirmishCommander.cpp //////////////////////////////////////////////////////////////////
// Tiberium Project @feature The skirmish AI's own command of its army and factories, beside its
// scripts. Zero Hour's skirmish AI does what SkirmishScripts.scb tells it: it builds the teams the
// scripts name and sends them along the map's approach paths. A unit no script uses stands where it
// was made, a factory no team needs stands idle, and C&C4's sides (their Crawlers build, nothing else
// does) do nothing at all. Every two seconds the AI now:
//   - defends: an enemy at one of its buildings draws the idle units near its base;
//   - attacks: the units its scripts have left idle gather strength at home, and once there is enough
//     (more for each wave, less on a harder AI) they attack-move on its enemy's nearest building;
//   - pushes: a unit idle out in the field attack-moves on the enemy building nearest it;
// and every three seconds it keeps a factory that has had no work for a while building fighting units
// it can afford (leaving money for its scripts), and deploys a Crawler that is not deployed. Units its
// scripts are still gathering into a team are left alone. The AI's units use their own abilities
// (AIAbilityUseUpdate).
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/BuildAssistant.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/TiberiumTest.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/AI.h"
#include "GameLogic/AISkirmishPlayer.h"
#include "GameLogic/AIStateMachine.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/ReplaceSelfUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"

#include <algorithm>
#include <set>
#include <vector>

namespace
{
	const UnsignedInt ARMY_PERIOD = 2 * LOGICFRAMES_PER_SECOND;			///< how often it looks over its army
	const UnsignedInt PRODUCTION_PERIOD = 3 * LOGICFRAMES_PER_SECOND;	///< and its factories
	const UnsignedInt FREE_AFTER = 8 * LOGICFRAMES_PER_SECOND;			///< idle this long: its scripts have no use for it
	const UnsignedInt STALE_AFTER = 180 * LOGICFRAMES_PER_SECOND;		///< an army idle this long goes with less
	const UnsignedInt FACTORY_IDLE = 6 * LOGICFRAMES_PER_SECOND;		///< a factory idle this long builds on its own
	const Real HOME = 1500.0f;			///< within this of its base, a unit is at home
	const Real GUARD = 450.0f;			///< an enemy this near one of its buildings is in its base
	const Real DEFENDERS = 2500.0f;	///< the idle units this near its base can come to defend it ...
	const Real NEAR_THREAT = 1200.0f;	///< ... those this near the danger (else the nearest four)
	const Int RESERVE = 300;			///< money its own building leaves for its scripts' teams

	/// an enemy that is a danger: what has a weapon and can fire it (not a builder, a harvester, or a building
	/// without a gun: Object::isAbleToAttack alone takes a dozer for one)
	class PartitionFilterDangerous : public PartitionFilter
	{
	public:
		virtual Bool allow( Object *other ) override
		{
			if( other->isKindOf( KINDOF_DOZER ) || other->isKindOf( KINDOF_HARVESTER ) || !other->isAbleToAttack() )
				return FALSE;
			if( other->isKindOf( KINDOF_CAN_ATTACK ) || other->testStatus( OBJECT_STATUS_CAN_ATTACK ) )
				return TRUE;
			for( Int i = 0; i < WEAPONSLOT_COUNT; ++i )
				if( other->getWeaponInWeaponSlot( (WeaponSlotType)i ) != nullptr )
					return TRUE;
			return FALSE;
		}
#if defined(RTS_DEBUG)
		virtual const char* debugGetName() override { return "PartitionFilterDangerous"; }
#endif
	};

	/// a unit that fights for it: alive, out (not inside something), built, armed, and not a worker, a
	/// factory, a drone or a part of something else
	Bool isSoldier( Object *obj )
	{
		if( obj->isEffectivelyDead() || obj->isContained() || obj->getAIUpdateInterface() == nullptr )
			return FALSE;
		if( obj->isKindOf( KINDOF_STRUCTURE ) || obj->isKindOf( KINDOF_PROJECTILE ) || obj->isKindOf( KINDOF_DOZER )
			|| obj->isKindOf( KINDOF_HARVESTER ) || obj->isKindOf( KINDOF_DRONE ) || obj->isKindOf( KINDOF_IGNORED_IN_GUI )
			|| obj->isKindOf( KINDOF_IMMOBILE ) )
			return FALSE;
		if( obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) || obj->getProductionUpdateInterface() != nullptr )
			return FALSE;
		if( !obj->isSelectable() || !obj->isAbleToAttack() )
			return FALSE;
		return TRUE;
	}

	/// what the player has, looked over once
	struct Muster
	{
		std::vector<Object *> soldiers;
		std::vector<Object *> structures;
		std::vector<Object *> producers;	///< its factories (and deployed Crawlers)
		std::vector<Object *> deployers;	///< what deploys into a factory (C&C4's Crawlers, packed up)
	};

	/// the ReplaceSelfUpdate that turns obj into a factory, if it has one
	ReplaceSelfUpdate *deploysIntoFactory( Object *obj )
	{
		static const NameKeyType key = NAMEKEY( "ReplaceSelfUpdate" );
		ReplaceSelfUpdate *replace = (ReplaceSelfUpdate *)obj->findUpdateModule( key );
		if( replace == nullptr || replace->getReplacePower() == nullptr )
			return nullptr;
		const ThingTemplate *into = TheThingFactory->findTemplate( replace->getReplacementName() );
		if( into == nullptr )
			return nullptr;
		static const NameKeyType production = NAMEKEY( "ProductionUpdate" );
		const ModuleInfo &modules = into->getBehaviorModuleInfo();
		for( Int i = 0; i < modules.getCount(); ++i )
		{
			if( NAMEKEY( modules.getNthName( i ) ) == production )
				return replace;
		}
		return nullptr;
	}

	void musterOne( Object *obj, void *data )
	{
		Muster *muster = (Muster *)data;
		if( obj->isEffectivelyDead() || obj->isKindOf( KINDOF_PROJECTILE ) )
			return;
		if( obj->isKindOf( KINDOF_STRUCTURE ) && !obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
			muster->structures.push_back( obj );
		if( obj->getProductionUpdateInterface() != nullptr && !obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
			muster->producers.push_back( obj );
		else if( !obj->isKindOf( KINDOF_STRUCTURE ) && !obj->isContained() && deploysIntoFactory( obj ) != nullptr )
			muster->deployers.push_back( obj );
		if( isSoldier( obj ) )
			muster->soldiers.push_back( obj );
	}

	/// the enemy building nearest pos: of enemy if it has one, else of any enemy of player; else the
	/// nearest enemy unit; FALSE when there is no enemy at all
	struct Nearest
	{
		const Player *player;
		const Player *only;
		Coord3D from;
		Bool structuresOnly;
		Real best;
		Coord3D at;
		Bool found;
	};

	void nearestOne( Object *obj, void *data )
	{
		Nearest *n = (Nearest *)data;
		if( obj->isEffectivelyDead() || obj->isKindOf( KINDOF_PROJECTILE ) || obj->isContained() || !obj->isSelectable() )
			return;
		if( n->structuresOnly ? !obj->isKindOf( KINDOF_STRUCTURE ) : obj->isKindOf( KINDOF_STRUCTURE ) )
			return;
		if( obj->testStatus( OBJECT_STATUS_STEALTHED ) && !obj->testStatus( OBJECT_STATUS_DETECTED ) )
			return;
		const Coord3D *pos = obj->getPosition();
		const Real d = ( pos->x - n->from.x ) * ( pos->x - n->from.x ) + ( pos->y - n->from.y ) * ( pos->y - n->from.y );
		if( d < n->best )
		{
			n->best = d;
			n->at = *pos;
			n->found = TRUE;
		}
	}

	Bool nearestEnemy( const Player *player, const Player *enemy, const Coord3D &from, Coord3D *at )
	{
		for( Int pass = 0; pass < 3; ++pass )
		{
			Nearest n;
			n.player = player;
			n.only = pass == 0 ? enemy : nullptr;
			n.from = from;
			n.structuresOnly = pass < 2;
			n.best = HUGE_DIST * HUGE_DIST;
			n.found = FALSE;
			if( pass == 0 && enemy == nullptr )
				continue;
			for( Int i = 0; i < ThePlayerList->getPlayerCount(); ++i )
			{
				Player *other = ThePlayerList->getNthPlayer( i );
				if( other == nullptr || other == player || ( n.only != nullptr && other != n.only ) )
					continue;
				if( player->getRelationship( other->getDefaultTeam() ) != ENEMIES || other->isPlayerObserver() )
					continue;
				other->iterateObjects( nearestOne, &n );
			}
			if( n.found )
			{
				*at = n.at;
				return TRUE;
			}
		}
		return FALSE;
	}

	Real distanceSquared( const Coord3D &a, const Coord3D *b )
	{
		return ( a.x - b->x ) * ( a.x - b->x ) + ( a.y - b->y ) * ( a.y - b->y );
	}

	void attackMove( const std::vector<Object *> &units, const Coord3D &to )
	{
		if( units.empty() )
			return;
		AIGroupPtr group = TheAI->createGroup();
		if( !group )
			return;
		for( size_t i = 0; i < units.size(); ++i )
			group->add( units[i] );
		Coord3D at = to;
		at.z = TheTerrainLogic->getGroundHeight( at.x, at.y );
		group->groupAttackMoveToPosition( &at, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI );
	}
}

//----------------------------------------------------------------------------------------------------------
/** Tiberium Project @feature where its base is: its build list's middle, else its buildings', else its start */
//----------------------------------------------------------------------------------------------------------
Bool AISkirmishPlayer::getCommandBase( Coord3D *pos ) const
{
	if( m_baseCenterSet )
	{
		*pos = m_baseCenter;
		return TRUE;
	}
	AsciiString name;
	name.format( "Player_%d_Start", m_player->getMpStartIndex() + 1 );
	Waypoint *start = TheTerrainLogic != nullptr ? TheTerrainLogic->getWaypointByName( name ) : nullptr;
	if( start != nullptr )
	{
		*pos = *start->getLocation();
		return TRUE;
	}
	return FALSE;
}

//----------------------------------------------------------------------------------------------------------
/** Tiberium Project @feature its army: defend, attack in waves, push (see the top of this file) */
//----------------------------------------------------------------------------------------------------------
void AISkirmishPlayer::commandArmy()
{
	const UnsignedInt now = TheGameLogic->getFrame();
	if( now < m_nextArmyFrame || m_player == nullptr || !m_player->isPlayerActive() )
		return;
	m_nextArmyFrame = now + ARMY_PERIOD;

	Muster muster;
	m_player->iterateObjects( musterOne, &muster );

	Coord3D base;
	if( !getCommandBase( &base ) )
	{
		if( muster.structures.empty() && muster.soldiers.empty() )
			return;
		base = muster.structures.empty() ? *muster.soldiers.front()->getPosition() : *muster.structures.front()->getPosition();
	}
	// (a base the build list placed, but whose buildings are gone or were never built: its buildings', if any)
	if( !muster.structures.empty() && !m_baseCenterSet )
	{
		Real sx = 0, sy = 0;
		for( size_t i = 0; i < muster.structures.size(); ++i )
		{
			sx += muster.structures[i]->getPosition()->x;
			sy += muster.structures[i]->getPosition()->y;
		}
		base.x = sx / muster.structures.size();
		base.y = sy / muster.structures.size();
	}

	// the teams its scripts are still gathering: their members are theirs
	std::set<const Team *> gathering;
	for( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamBuildQueue(); !iter.done(); iter.advance() )
		if( iter.cur()->m_team != nullptr )
			gathering.insert( iter.cur()->m_team );
	for( DLINK_ITERATOR<TeamInQueue> iter = iterate_TeamReadyQueue(); !iter.done(); iter.advance() )
		if( iter.cur()->m_team != nullptr )
			gathering.insert( iter.cur()->m_team );

	// who is idle, and since when
	std::vector<Object *> idleHome, freeHome, freeAfield;
	std::set<ObjectID> seen;
	UnsignedInt oldestFree = now;
	for( size_t i = 0; i < muster.soldiers.size(); ++i )
	{
		Object *unit = muster.soldiers[i];
		seen.insert( unit->getID() );
		AIUpdateInterface *ai = unit->getAIUpdateInterface();
		const Bool home = distanceSquared( base, unit->getPosition() ) <= HOME * HOME;
		// idle, or standing guard at home (where its scripts' teams wait between their orders): its to command
		const StateID state = ai->getCurrentStateID();
		const Bool guarding = home && ( state == AI_GUARD || state == AI_GUARD_RETALIATE ) && !ai->isAttacking();
		if( !( ai->isIdle() || guarding ) || gathering.count( unit->getTeam() ) > 0 )
		{
			m_idleSince.erase( unit->getID() );
			continue;
		}
		std::map<ObjectID, UnsignedInt>::iterator it = m_idleSince.find( unit->getID() );
		if( it == m_idleSince.end() )
			it = m_idleSince.insert( std::make_pair( unit->getID(), now ) ).first;
		if( distanceSquared( base, unit->getPosition() ) <= DEFENDERS * DEFENDERS )
			idleHome.push_back( unit );
		if( now - it->second >= FREE_AFTER )
		{
			( home ? freeHome : freeAfield ).push_back( unit );
			if( home )
				oldestFree = min( oldestFree, it->second );
		}
	}
	for( std::map<ObjectID, UnsignedInt>::iterator it = m_idleSince.begin(); it != m_idleSince.end(); )
	{
		if( seen.count( it->first ) == 0 )
			m_idleSince.erase( it++ );
		else
			++it;
	}

	// defend: an enemy that is a danger at one of its buildings
	// (enemies only: rejecting allies, the neutral and its own, as isLocationSafe does; the filter matches its own
	// player's objects too, so ALLOW_ENEMIES with a match took the AI's own units for intruders)
	PartitionFilterPlayerAffiliation filterEnemy( m_player, ALLOW_ALLIES | ALLOW_NEUTRAL, false );
	PartitionFilterAlive filterAlive;
	PartitionFilterRejectByObjectStatus filterStealth( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_STEALTHED ),
		MAKE_OBJECT_STATUS_MASK2( OBJECT_STATUS_DETECTED, OBJECT_STATUS_DISGUISED ) );
	PartitionFilterDangerous filterDangerous;
	PartitionFilter *filters[] = { &filterEnemy, &filterAlive, &filterStealth, &filterDangerous, nullptr };
	Object *threat = nullptr;
	for( size_t i = 0; i < muster.structures.size() && i < 48 && threat == nullptr; ++i )
	{
		Object *building = muster.structures[i];
		threat = ThePartitionManager->getClosestObject( building, GUARD + building->getGeometryInfo().getBoundingCircleRadius(),
			FROM_BOUNDINGSPHERE_2D, filters );
	}
	std::set<ObjectID> defending;
	if( threat != nullptr )
	{
		// the idle units near the danger (else the nearest few) meet it; the rest stay for a wave
		const Coord3D at = *threat->getPosition();
		std::vector<Object *> defenders;
		for( size_t i = 0; i < idleHome.size(); ++i )
			if( distanceSquared( at, idleHome[i]->getPosition() ) <= NEAR_THREAT * NEAR_THREAT )
				defenders.push_back( idleHome[i] );
		if( defenders.empty() )
		{
			// (ties by id: every machine's sort then picks the same units)
			std::vector<Object *> byDistance = idleHome;
			std::sort( byDistance.begin(), byDistance.end(), [&at]( Object *a, Object *b )
				{
					const Real da = distanceSquared( at, a->getPosition() ), db = distanceSquared( at, b->getPosition() );
					return da != db ? da < db : a->getID() < b->getID();
				} );
			for( size_t i = 0; i < byDistance.size() && i < 4; ++i )
				defenders.push_back( byDistance[i] );
		}
		if( TiberiumTest::isActive() && !defenders.empty() )
			TiberiumTest::log( "frame %u: AI player %d defends with %d units against %s at %.0f %.0f (player %d's, team %s, relationship %d)", now, (Int)m_player->getPlayerIndex(),
				(Int)defenders.size(), threat->getTemplate()->getName().str(), at.x, at.y,
				threat->getControllingPlayer() != nullptr ? (Int)threat->getControllingPlayer()->getPlayerIndex() : -1,
				threat->getTeam() != nullptr ? threat->getTeam()->getName().str() : "none", (Int)m_player->getRelationship( threat->getTeam() ) );
		attackMove( defenders, at );
		for( size_t i = 0; i < defenders.size(); ++i )
		{
			m_idleSince.erase( defenders[i]->getID() );
			defending.insert( defenders[i]->getID() );
		}
		// (those out in the field push on as before; a wave still goes from what the defence leaves)
	}

	// push: an idle unit out in the field goes on to the enemy building nearest it
	Player *enemy = getAiEnemy();
	if( !freeAfield.empty() )
	{
		Coord3D middle;
		middle.zero();
		for( size_t i = 0; i < freeAfield.size(); ++i )
		{
			middle.x += freeAfield[i]->getPosition()->x;
			middle.y += freeAfield[i]->getPosition()->y;
		}
		middle.x /= freeAfield.size();
		middle.y /= freeAfield.size();
		Coord3D to;
		if( nearestEnemy( m_player, enemy, middle, &to ) )
		{
			if( TiberiumTest::isActive() )
				TiberiumTest::log( "frame %u: AI player %d pushes on with %d units to %.0f %.0f", now, (Int)m_player->getPlayerIndex(),
					(Int)freeAfield.size(), to.x, to.y );
			attackMove( freeAfield, to );
			for( size_t i = 0; i < freeAfield.size(); ++i )
				m_idleSince.erase( freeAfield[i]->getID() );
		}
	}

	// attack: a wave from home, once there is strength enough (what the defence took stays)
	std::vector<Object *> wave;
	for( size_t i = 0; i < freeHome.size(); ++i )
		if( defending.count( freeHome[i]->getID() ) == 0 )
			wave.push_back( freeHome[i] );
	freeHome.swap( wave );
	const GameDifficulty difficulty = getAIDifficulty();
	Real wanted = difficulty == DIFFICULTY_EASY ? 3000.0f : difficulty == DIFFICULTY_HARD ? 1500.0f : 2000.0f;
	wanted *= 1.0f + 0.2f * min( m_wavesSent, 10 );
	Real strength = 0.0f;
	for( size_t i = 0; i < freeHome.size(); ++i )
		strength += (Real)freeHome[i]->getTemplate()->calcCostToBuild( m_player );
	const Bool stale = !freeHome.empty() && now - oldestFree >= STALE_AFTER;
	if( TiberiumTest::isActive() && ( now / ARMY_PERIOD ) % 15 == 0 )
	{
		// (and what its soldiers are doing: their AI states, how many in each, and how many in teams being gathered)
		std::map<Int, Int> states;
		Int inGathering = 0;
		for( size_t i = 0; i < muster.soldiers.size(); ++i )
		{
			++states[ (Int)muster.soldiers[i]->getAIUpdateInterface()->getCurrentStateID() ];
			if( gathering.count( muster.soldiers[i]->getTeam() ) > 0 )
				++inGathering;
		}
		AsciiString list;
		for( std::map<Int, Int>::const_iterator it = states.begin(); it != states.end(); ++it )
		{
			AsciiString one;
			one.format( " %d:%d", it->first, it->second );
			list.concat( one );
		}
		TiberiumTest::log( "frame %u: AI player %d: %d soldiers (states%s; %d in teams being gathered), %d idle near home, %d free at home, %d afield, %d defending; strength %.0f of %.0f%s",
			now, (Int)m_player->getPlayerIndex(), (Int)muster.soldiers.size(), list.str(), inGathering, (Int)idleHome.size(), (Int)freeHome.size(),
			(Int)freeAfield.size(), (Int)defending.size(), strength, wanted, stale ? " (stale)" : "" );
	}
	if( freeHome.empty() )
		return;
	const UnsignedInt between = ( difficulty == DIFFICULTY_EASY ? 90 : difficulty == DIFFICULTY_HARD ? 40 : 60 ) * LOGICFRAMES_PER_SECOND;
	if( now - m_lastWaveFrame < between )
		return;
	if( strength < wanted && !( stale && strength >= wanted * 0.4f ) )
		return;
	if( freeHome.size() < 3 && strength < wanted * 1.5f )
		return;
	Coord3D to;
	if( !nearestEnemy( m_player, enemy, base, &to ) )
		return;
	attackMove( freeHome, to );
	for( size_t i = 0; i < freeHome.size(); ++i )
		m_idleSince.erase( freeHome[i]->getID() );
	m_lastWaveFrame = now;
	++m_wavesSent;
	AsciiString msg;
	msg.format( "%s sends wave %d: %d units", TheNameKeyGenerator->keyToName( m_player->getPlayerNameKey() ).str(), m_wavesSent, (Int)freeHome.size() );
	TheScriptEngine->AppendDebugMessage( msg, false );
	if( TiberiumTest::isActive() )
		TiberiumTest::log( "frame %u: AI player %d sends wave %d: %d units, strength %.0f of %.0f, at %.0f %.0f", now,
			(Int)m_player->getPlayerIndex(), m_wavesSent, (Int)freeHome.size(), strength, wanted, to.x, to.y );
}

//----------------------------------------------------------------------------------------------------------
/** Tiberium Project @feature its factories: an idle one builds a fighting unit it can afford; a Crawler that is
	not deployed deploys (see the top of this file) */
//----------------------------------------------------------------------------------------------------------
void AISkirmishPlayer::commandProduction()
{
	const UnsignedInt now = TheGameLogic->getFrame();
	if( now < m_nextProductionFrame || m_player == nullptr || !m_player->isPlayerActive() )
		return;
	m_nextProductionFrame = now + PRODUCTION_PERIOD;

	Muster muster;
	m_player->iterateObjects( musterOne, &muster );

	// a Crawler that is not deployed, standing still, deploys where it stands
	for( size_t i = 0; i < muster.deployers.size(); ++i )
	{
		Object *crawler = muster.deployers[i];
		AIUpdateInterface *ai = crawler->getAIUpdateInterface();
		if( ai == nullptr || !ai->isIdle() || crawler->isDisabled() )
			continue;
		ReplaceSelfUpdate *replace = deploysIntoFactory( crawler );
		if( replace == nullptr || replace->isActive() )
			continue;
		SpecialPowerModuleInterface *power = crawler->getSpecialPowerModule( replace->getReplacePower() );
		if( power != nullptr && power->isReady() )
		{
			if( TiberiumTest::isActive() )
				TiberiumTest::log( "frame %u: AI player %d deploys its %s", now, (Int)m_player->getPlayerIndex(), crawler->getTemplate()->getName().str() );
			power->doSpecialPower( 0 );
		}
	}

	// an idle factory builds
	std::set<ObjectID> seen;
	Money *money = m_player->getMoney();
	for( size_t i = 0; i < muster.producers.size(); ++i )
	{
		Object *factory = muster.producers[i];
		seen.insert( factory->getID() );
		ProductionUpdateInterface *pu = factory->getProductionUpdateInterface();
		if( pu == nullptr || pu->getProductionCount() > 0 || factory->isDisabled() )
		{
			m_factoryIdleSince.erase( factory->getID() );
			continue;
		}
		std::map<ObjectID, UnsignedInt>::iterator it = m_factoryIdleSince.find( factory->getID() );
		if( it == m_factoryIdleSince.end() )
			it = m_factoryIdleSince.insert( std::make_pair( factory->getID(), now ) ).first;
		if( now - it->second < FACTORY_IDLE )
			continue;
		const CommandSet *set = TheControlBar != nullptr ? TheControlBar->findCommandSet( factory->getCommandSetString() ) : nullptr;
		if( set == nullptr )
			continue;
		const CommandSet *pages[ ControlBar::MAX_COMMAND_SET_PAGES ];
		const Int pageCount = TheControlBar->getCommandSetPages( set, pages, ControlBar::MAX_COMMAND_SET_PAGES );
		std::vector<const ThingTemplate *> choices;
		for( Int b = 0; b < MAX_COMMANDS_PER_SET * pageCount; ++b )
		{
			const CommandButton *button = pages[ b / MAX_COMMANDS_PER_SET ]->getCommandButton( b % MAX_COMMANDS_PER_SET );
			if( button == nullptr || button->getCommandType() != GUI_COMMAND_UNIT_BUILD || button->getThingTemplate() == nullptr )
				continue;
			const ThingTemplate *unit = button->getThingTemplate();
			if( unit->isKindOf( KINDOF_DOZER ) || unit->isKindOf( KINDOF_HARVESTER ) || unit->isKindOf( KINDOF_STRUCTURE )
				|| !unit->canPossiblyHaveAnyWeapon() )
				continue;
			if( (Int)money->countMoney() - unit->calcCostToBuild( m_player ) < RESERVE )
				continue;
			if( TheBuildAssistant->canMakeUnit( factory, unit ) != CANMAKE_OK )
				continue;
			Bool already = FALSE;
			for( size_t c = 0; c < choices.size() && !already; ++c )
				already = choices[c] == unit;
			if( !already )
				choices.push_back( unit );
		}
		if( choices.empty() )
			continue;
		const ThingTemplate *unit = choices[ GameLogicRandomValue( 0, (Int)choices.size() - 1 ) ];
		if( pu->queueCreateUnit( unit, pu->requestUniqueUnitID() ) )
		{
			m_factoryIdleSince.erase( factory->getID() );
			if( TiberiumTest::isActive() )
				TiberiumTest::log( "frame %u: AI player %d's idle %s builds %s", now, (Int)m_player->getPlayerIndex(),
					factory->getTemplate()->getName().str(), unit->getName().str() );
		}
	}
	for( std::map<ObjectID, UnsignedInt>::iterator it = m_factoryIdleSince.begin(); it != m_factoryIdleSince.end(); )
	{
		if( seen.count( it->first ) == 0 )
			m_factoryIdleSince.erase( it++ );
		else
			++it;
	}
}
