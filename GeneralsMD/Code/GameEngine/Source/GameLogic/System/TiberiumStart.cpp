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

// FILE: TiberiumStart.cpp ////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's match start and redeployment (see TiberiumStart.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/TiberiumStart.h"

#include "Common/GlobalData.h"
#include "Common/MapObject.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/TiberiumTest.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/ReplaceSelfUpdate.h"
#include "GameLogic/Module/TiberiumArrivalUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/TiberiumZones.h"
#include "GameNetwork/GameInfo.h"

#include <vector>

namespace
{
	const Real MARKER_RADIUS = 125.0f;							///< C&C4's SpecialPower_Redeployment: its radius cursor
	const Real FORBIDDEN_RADIUS = 115.0f;						///< and no structure of its own, an ally's or the neutral's this near
	const UnsignedInt LOOK_EVERY = LOGICFRAMES_PER_SECOND / 2;	///< how often each player's Crawler is looked for
	const UnsignedInt GRACE = LOGICFRAMES_PER_SECOND * 30;	///< kept in after decommissioning its last object, past its wait

	struct Entry
	{
		Int player;							///< the player's index
		Coord3D home;						///< its start (the map's Player_N_Start)
		Int chosen;							///< the start's pick (-1: none yet)
		Coord3D spot;						///< and its spot
		Bool hasSpot;
		Bool landed;						///< the start's Crawler is made
		Bool crawler;						///< one of its list is on the map (as last looked)
		UnsignedInt lostFrame;	///< when it was last found without one (0: it has one)
		ObjectID leaving;				///< decommissioned, gone at leaveFrame
		UnsignedInt leaveFrame;
		UnsignedInt graceUntil;	///< not defeated before this (a decommission)
		ObjectID first;					///< the start's pick, its StartingUnits due beside it once it is out
		Bool unitsDue;
		Bool powerCrawler;			///< one of its Crawler power's list is on the map (as last looked)
		UnsignedInt powerReady;	///< the frame its Crawler power may be used from (0: at once, once it has the upgrade)
		UnsignedInt tierStart;	///< the Crawler power's upgrade being researched: since, and done on tierDone (0: not)
		UnsignedInt tierDone;
	};

	std::vector<Entry> s_entries;
	UnsignedInt s_introEnd = 0;
	UnsignedInt s_deployFrame = 0;
	Bool s_deployed = FALSE;		///< the start's arrivals are made
	Bool s_loaded = FALSE;			///< the state came with a saved game

	//-----------------------------------------------------------------------------------------------
	Entry *entryOf( const Player *player )
	{
		if( player == nullptr )
			return nullptr;
		for( size_t i = 0; i < s_entries.size(); ++i )
			if( s_entries[i].player == (Int)player->getPlayerIndex() )
				return &s_entries[i];
		return nullptr;
	}

	Player *playerOf( const Entry &e )
	{
		return ThePlayerList != nullptr ? ThePlayerList->getNthPlayer( e.player ) : nullptr;
	}

	const PlayerTemplate *templateOf( const Player *player )
	{
		return player != nullptr ? player->getPlayerTemplate() : nullptr;
	}

	Int listCount( const PlayerTemplate *pt, Int list )
	{
		if( pt == nullptr )
			return 0;
		return list == TiberiumStart::LIST_POWER ? pt->getCrawlerPowerCount() : pt->getStartArrivalCount();
	}

	AsciiString listName( const PlayerTemplate *pt, Int list, Int i )
	{
		return list == TiberiumStart::LIST_POWER ? pt->getCrawlerPower( i ) : pt->getStartArrival( i );
	}

	/// the upgrade that gives the player its Crawler power, if one is named
	const UpgradeTemplate *powerUpgrade( const Player *player )
	{
		const PlayerTemplate *pt = player != nullptr ? player->getPlayerTemplate() : nullptr;
		if( pt == nullptr || pt->getCrawlerPowerUpgrade().isEmpty() || TheUpgradeCenter == nullptr )
			return nullptr;
		return TheUpgradeCenter->findUpgrade( pt->getCrawlerPowerUpgrade() );
	}

	/// the player has the upgrade that gives its Crawler power (none named: it has the power outright)
	Bool powerUnlocked( const Player *player )
	{
		const PlayerTemplate *pt = player != nullptr ? player->getPlayerTemplate() : nullptr;
		if( listCount( pt, TiberiumStart::LIST_POWER ) <= 0 )
			return FALSE;
		if( pt->getCrawlerPowerUpgrade().isEmpty() )
			return TRUE;
		const UpgradeTemplate *up = powerUpgrade( player );
		return up != nullptr && player->hasUpgradeComplete( up );
	}

	Bool isFriend( const Player *me, const Player *other )
	{
		if( me == nullptr || other == nullptr )
			return FALSE;
		if( me == other )
			return TRUE;
		const Team *team = other->getDefaultTeam();
		return team != nullptr && me->getRelationship( team ) == ALLIES;
	}

	//-----------------------------------------------------------------------------------------------
	/** whether obj is one of the player's list (the start's or its Crawler power's), either form (the vehicle, or
		what it unpacks into: that packs up into one of the list) */
	Bool isCrawlerOf( const Object *obj, const PlayerTemplate *pt, Int list )
	{
		if( obj == nullptr || pt == nullptr || obj->isEffectivelyDead() )
			return FALSE;
		const ThingTemplate *t = obj->getTemplate();
		const Int count = listCount( pt, list );
		for( Int i = 0; i < count; ++i )
		{
			const ThingTemplate *c = TheThingFactory->findTemplate( listName( pt, list, i ) );
			if( c != nullptr && t->isEquivalentTo( c ) )
				return TRUE;
		}
		static const NameKeyType replaceKey = NAMEKEY( "ReplaceSelfUpdate" );
		const ReplaceSelfUpdate *replace = (const ReplaceSelfUpdate *)obj->findUpdateModule( replaceKey );
		if( replace != nullptr )
		{
			for( Int i = 0; i < count; ++i )
				if( replace->getReplacementName().compareNoCase( listName( pt, list, i ) ) == 0 )
					return TRUE;
		}
		return FALSE;
	}

	struct LookFor
	{
		const PlayerTemplate *pt;
		Int list;
		Bool found;
	};

	void lookOne( Object *obj, void *data )
	{
		LookFor *look = (LookFor *)data;
		if( !look->found && isCrawlerOf( obj, look->pt, look->list ) )
			look->found = TRUE;
	}

	Bool lookForCrawler( const Entry &e, Int list )
	{
		Player *player = playerOf( e );
		LookFor look = { templateOf( player ), list, FALSE };
		if( player == nullptr || look.pt == nullptr || listCount( look.pt, list ) <= 0 )
			return FALSE;
		player->iterateObjects( lookOne, &look );
		return look.found;
	}

	//-----------------------------------------------------------------------------------------------
	/** a spot near 'from' the player may land at: 'from' itself, else round it, farther out; 'from' when none */
	Coord3D findSpotNear( const Player *player, const Coord3D &from )
	{
		static const Real RINGS[] = { 0.0f, 150.0f, 300.0f, 450.0f };
		static const Real DX[8] = { 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f, 0.0f, 0.7071f };
		static const Real DY[8] = { 0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f };
		for( Int r = 0; r < 4; ++r )
		{
			const Int steps = r == 0 ? 1 : 8;
			for( Int k = 0; k < steps; ++k )
			{
				Coord3D at = from;
				at.x += RINGS[r] * DX[k];
				at.y += RINGS[r] * DY[k];
				at.z = TheTerrainLogic->getGroundHeight( at.x, at.y );
				if( TiberiumStart::isSpotAllowed( player, &at, nullptr ) )
					return at;
			}
		}
		Coord3D at = from;
		at.z = TheTerrainLogic->getGroundHeight( at.x, at.y );
		return at;
	}

	//-----------------------------------------------------------------------------------------------
	/** one of the player's list made at the spot, as the map's start objects are, then its arrival begun there */
	Object *makeArrival( Player *player, const ThingTemplate *t, const Coord3D &spot )
	{
		Team *team = player->getDefaultTeam();
		if( t == nullptr || team == nullptr )
			return nullptr;
		Object *obj = TheThingFactory->newObject( t, team );
		if( obj == nullptr )
			return nullptr;
		Coord3D pos = spot;
		pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );
		obj->setOrientation( t->getPlacementViewAngle() );
		obj->setPosition( &pos );
		for( BehaviorModule **m = obj->getBehaviorModules(); *m != nullptr; ++m )
		{
			CreateModuleInterface *create = ( *m )->getCreate();
			if( create != nullptr )
				create->onBuildComplete();
		}
		team->setActive();
		TheAI->pathfinder()->addObjectToPathfindMap( obj );
		player->onUnitCreated( nullptr, obj );
		static const NameKeyType arrivalKey = NAMEKEY( "TiberiumArrivalUpdate" );
		TiberiumArrivalUpdate *arrival = (TiberiumArrivalUpdate *)obj->findUpdateModule( arrivalKey );
		if( arrival != nullptr )
			arrival->beginPlaced();
		else if( obj->getAIUpdateInterface() != nullptr && !obj->isKindOf( KINDOF_IMMOBILE )
			&& TheAI->pathfinder()->adjustDestination( obj, obj->getAIUpdateInterface()->getLocomotorSet(), &pos ) )
		{
			TheAI->pathfinder()->updateGoal( obj, &pos, LAYER_GROUND );
			obj->setPosition( &pos );
		}
		if( TiberiumTest::isActive() )
			TiberiumTest::log( "frame %u: start: player %d's %s made at %.0f %.0f%s", TheGameLogic->getFrame(),
				(int)player->getPlayerIndex(), t->getName().str(), pos.x, pos.y, arrival != nullptr ? " (arriving)" : "" );
		return obj;
	}

	//-----------------------------------------------------------------------------------------------
	/** the faction's StartingUnits round the start's pick, as Zero Hour puts them round its start building */
	void placeStartUnits( Player *player, const PlayerTemplate *pt, const Coord3D &center, Real radius )
	{
		for( Int i = 0; i < MAX_MP_STARTING_UNITS; ++i )
		{
			const AsciiString name = pt->getStartingUnit( i );
			const ThingTemplate *t = name.isNotEmpty() ? TheThingFactory->findTemplate( name ) : nullptr;
			if( t == nullptr )
				continue;
			FindPositionOptions options;
			options.minRadius = radius * 0.7f;
			options.maxRadius = radius * 1.3f;
			Coord3D at = center;
			if( !ThePartitionManager->findPositionAround( &center, &options, &at ) )
				continue;
			Object *obj = TheThingFactory->newObject( t, player->getDefaultTeam() );
			if( obj == nullptr )
				continue;
			at.z = TheTerrainLogic->getGroundHeight( at.x, at.y );
			obj->setOrientation( t->getPlacementViewAngle() );
			obj->setPosition( &at );
			for( BehaviorModule **m = obj->getBehaviorModules(); *m != nullptr; ++m )
			{
				CreateModuleInterface *create = ( *m )->getCreate();
				if( create != nullptr )
					create->onBuildComplete();
			}
			player->onUnitCreated( nullptr, obj );
			if( obj->getAIUpdateInterface() != nullptr && !obj->isKindOf( KINDOF_IMMOBILE )
				&& TheAI->pathfinder()->adjustDestination( obj, obj->getAIUpdateInterface()->getLocomotorSet(), &at ) )
			{
				TheAI->pathfinder()->updateGoal( obj, &at, LAYER_GROUND );
				obj->setPosition( &at );
			}
			if( TiberiumTest::isActive() )
				TiberiumTest::log( "frame %u: start: player %d's start unit %s out at %.0f %.0f", TheGameLogic->getFrame(),
					(int)player->getPlayerIndex(), t->getName().str(), at.x, at.y );
		}
	}

	Int randomChoice( const PlayerTemplate *pt, Int list )
	{
		const Int count = listCount( pt, list );
		return count > 1 ? GameLogicRandomValue( 0, count - 1 ) : 0;
	}

	Bool isAI( const Player *player )
	{
		return player != nullptr && player->getPlayerType() == PLAYER_COMPUTER;
	}

	//-----------------------------------------------------------------------------------------------
	/** the start's arrivals, every player's on this frame: its pick, at its spot (or the first of its list, at its
		start; the AI: at random, at its start) */
	void deployAll()
	{
		for( size_t i = 0; i < s_entries.size(); ++i )
		{
			Entry &e = s_entries[i];
			Player *player = playerOf( e );
			const PlayerTemplate *pt = templateOf( player );
			if( player == nullptr || pt == nullptr || e.landed || pt->getStartArrivalCount() <= 0 )
				continue;
			if( e.chosen < 0 || e.chosen >= pt->getStartArrivalCount() )
				e.chosen = 0;
			// (the spots are looked at while the start is still on: nobody's lands on another's)
			if( !e.hasSpot || !TiberiumStart::isSpotAllowed( player, &e.spot, nullptr ) )
			{
				e.spot = findSpotNear( player, e.home );
				e.hasSpot = TRUE;
			}
		}
		s_deployed = TRUE;
		for( size_t i = 0; i < s_entries.size(); ++i )
		{
			Entry &e = s_entries[i];
			Player *player = playerOf( e );
			const PlayerTemplate *pt = templateOf( player );
			if( player == nullptr || pt == nullptr || e.landed || pt->getStartArrivalCount() <= 0 )
				continue;
			e.landed = TRUE;
			Object *made = makeArrival( player, TheThingFactory->findTemplate( pt->getStartArrival( e.chosen ) ), e.spot );
			if( made != nullptr )
			{
				e.crawler = TRUE;
				e.lostFrame = 0;
				e.first = made->getID();
			}
			e.unitsDue = pt->hasStartArrivalUnits();
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** the players of the game who start this way, made again from the map after a load of a save without them */
	void rebuild()
	{
		s_entries.clear();
		if( TheGameInfo == nullptr || ThePlayerList == nullptr )
			return;
		for( Int i = 0; i < MAX_SLOTS; ++i )
		{
			const GameSlot *slot = TheGameInfo->getConstSlot( i );
			if( slot == nullptr || !slot->isOccupied() || slot->getStartPos() < 0 )
				continue;
			Player *player = ThePlayerList->getPlayerFromSlotIndex( i );
			if( player == nullptr || player->isPlayerObserver() || !TiberiumStart::hasEntryData( templateOf( player ) ) )
				continue;
			AsciiString name;
			name.format( "Player_%d_Start", slot->getStartPos() + 1 );
			Waypoint *wp = TheTerrainLogic->getWaypointByName( name );
			Coord3D home;
			home.zero();
			if( wp != nullptr )
				home = *wp->getLocation();
			TiberiumStart::addPlayer( player, templateOf( player ), &home );
		}
		for( size_t i = 0; i < s_entries.size(); ++i )
			s_entries[i].landed = TRUE;
		s_deployed = TRUE;
	}
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::usesArrival( const PlayerTemplate *pt )
{
	return pt != nullptr && pt->getStartArrivalCount() > 0;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::hasEntryData( const PlayerTemplate *pt )
{
	return usesArrival( pt ) || listCount( pt, LIST_POWER ) > 0;
}

//-------------------------------------------------------------------------------------------------
void TiberiumStart::addPlayer( Player *player, const PlayerTemplate *pt, const Coord3D *home )
{
	if( player == nullptr || !hasEntryData( pt ) || entryOf( player ) != nullptr )
		return;
	Entry e;
	e.player = (Int)player->getPlayerIndex();
	e.home = *home;
	e.chosen = -1;
	e.spot.zero();
	e.hasSpot = FALSE;
	e.landed = FALSE;
	e.crawler = FALSE;
	e.lostFrame = 0;
	e.leaving = INVALID_ID;
	e.leaveFrame = 0;
	e.graceUntil = 0;
	e.first = INVALID_ID;
	e.unitsDue = FALSE;
	e.powerCrawler = FALSE;
	e.powerReady = 0;
	e.tierStart = e.tierDone = 0;
	// (a faction that starts as Zero Hour's, with a Crawler power only: as if it had landed)
	e.landed = !usesArrival( pt );
	s_entries.push_back( e );
}

//-------------------------------------------------------------------------------------------------
void TiberiumStart::begin( Bool loadingSaveGame )
{
	if( loadingSaveGame || s_entries.empty() || TheGameLogic == nullptr )
		return;
	const UnsignedInt now = TheGameLogic->getFrame();
	Bool arrivals = FALSE;
	for( size_t i = 0; i < s_entries.size(); ++i )
		arrivals = arrivals || !s_entries[i].landed;
	if( !arrivals )
	{
		// (Crawler powers only: no match start)
		s_introEnd = s_deployFrame = now;
		s_deployed = TRUE;
		return;
	}
	s_introEnd = now + TheGlobalData->m_startIntro;
	s_deployFrame = s_introEnd + TheGlobalData->m_startCountdown;
	s_deployed = FALSE;
	// the AI picks now: at random, at its start
	for( size_t i = 0; i < s_entries.size(); ++i )
	{
		Entry &e = s_entries[i];
		Player *player = playerOf( e );
		if( !isAI( player ) )
			continue;
		e.chosen = randomChoice( templateOf( player ), LIST_START );
	}
	if( TiberiumTest::isActive() )
		TiberiumTest::log( "frame %u: start: %d players arrive; the class box from frame %u, the arrivals on frame %u", now,
			(int)s_entries.size(), s_introEnd, s_deployFrame );
}

//-------------------------------------------------------------------------------------------------
void TiberiumStart::reset()
{
	s_entries.clear();
	s_introEnd = s_deployFrame = 0;
	s_deployed = FALSE;
	s_loaded = FALSE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumStart::update()
{
	if( s_entries.empty() || TheGameLogic == nullptr )
		return;
	const UnsignedInt now = TheGameLogic->getFrame();
	if( !s_deployed )
	{
		if( now >= s_deployFrame )
			deployAll();
		return;
	}

	for( size_t i = 0; i < s_entries.size(); ++i )
	{
		Entry &e = s_entries[i];
		Player *player = playerOf( e );
		const PlayerTemplate *pt = templateOf( player );
		if( player == nullptr || pt == nullptr )
			continue;

		// a decommissioned Crawler goes
		if( e.leaving != INVALID_ID && now >= e.leaveFrame )
		{
			Object *obj = TheGameLogic->findObjectByID( e.leaving );
			e.leaving = INVALID_ID;
			if( obj != nullptr && !obj->isEffectivelyDead() && obj->getControllingPlayer() == player )
			{
				if( TiberiumTest::isActive() )
					TiberiumTest::log( "frame %u: start: player %d's %s decommissioned", now, (int)e.player, obj->getTemplate()->getName().str() );
				obj->kill( DAMAGE_UNRESISTABLE, DEATH_SUICIDED );
			}
		}

		// Tiberium Project @feature the Crawler power's upgrade, researched (TiberiumStart::researchCrawlerPower)
		if( e.tierDone != 0 && now >= e.tierDone )
		{
			e.tierStart = e.tierDone = 0;
			const UpgradeTemplate *up = powerUpgrade( player );
			if( up != nullptr && !player->hasUpgradeComplete( up ) )
			{
				player->addUpgrade( up, UPGRADE_STATUS_COMPLETE );
				if( TiberiumTest::isActive() )
					TiberiumTest::log( "frame %u: start: player %d has %s: its Crawler power", now, (int)e.player, up->getUpgradeName().str() );
			}
		}

		// the start's units, beside its pick once it is out (or where it was to land, if it is gone)
		if( e.unitsDue )
		{
			Object *first = TheGameLogic->findObjectByID( e.first );
			static const NameKeyType arrivalKey = NAMEKEY( "TiberiumArrivalUpdate" );
			const TiberiumArrivalUpdate *arrival = first != nullptr ? (const TiberiumArrivalUpdate *)first->findUpdateModule( arrivalKey ) : nullptr;
			if( first == nullptr || first->isEffectivelyDead() || arrival == nullptr || !arrival->isArriving() )
			{
				e.unitsDue = FALSE;
				const Coord3D center = first != nullptr && !first->isEffectivelyDead() ? *first->getPosition() : e.spot;
				const Real radius = first != nullptr ? max( 40.0f, first->getGeometryInfo().getBoundingSphereRadius() ) : 60.0f;
				placeStartUnits( player, pt, center, radius );
			}
		}

		if( !e.landed || ( now % LOOK_EVERY ) != ( (UnsignedInt)i % LOOK_EVERY ) )
			continue;
		if( usesArrival( pt ) )
		{
			const Bool had = e.crawler;
			e.crawler = lookForCrawler( e, LIST_START );
			if( !e.crawler && e.lostFrame == 0 )
			{
				e.lostFrame = now;
				if( TiberiumTest::isActive() && had )
					TiberiumTest::log( "frame %u: start: player %d has no Crawler now%s", now, (int)e.player,
						redeploys( player ) ? "; it may call another" : "" );
			}
			else if( e.crawler )
				e.lostFrame = 0;

			// the AI calls its next as soon as it may, at its start
			if( !e.crawler && isAI( player ) && redeploys( player ) && player->isPlayerActive() && now >= getRedeployFrame( player, LIST_START ) )
			{
				const Coord3D spot = findSpotNear( player, e.home );
				pick( player, randomChoice( pt, LIST_START ), &spot, TRUE, LIST_START );
			}
		}

		// Tiberium Project @feature its Crawler power: another may be used its CrawlerPowerWait after one is lost (or decommissioned)
		if( listCount( pt, LIST_POWER ) > 0 )
		{
			// (the AI researches its upgrade once it can)
			const UpgradeTemplate *up = powerUpgrade( player );
			if( isAI( player ) && up != nullptr && e.tierDone == 0 && !player->hasUpgradeComplete( up ) && player->isPlayerActive()
				&& TheUpgradeCenter->canAffordUpgrade( player, up, FALSE ) )
				researchCrawlerPower( player );
			const Bool had = e.powerCrawler;
			e.powerCrawler = lookForCrawler( e, LIST_POWER );
			if( had && !e.powerCrawler )
			{
				e.powerReady = now + pt->getCrawlerPowerWait();
				if( TiberiumTest::isActive() )
					TiberiumTest::log( "frame %u: start: player %d's Crawler power's Crawler is gone; another from frame %u", now, (int)e.player,
						e.powerReady );
			}
			// the AI uses it as soon as it may, at its start
			const UnsignedInt ready = getRedeployFrame( player, LIST_POWER );
			if( !e.powerCrawler && ready != 0 && now >= ready && isAI( player ) && player->isPlayerActive() )
			{
				const Coord3D spot = findSpotNear( player, e.home );
				pick( player, randomChoice( pt, LIST_POWER ), &spot, TRUE, LIST_POWER );
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumStart::pick( Player *player, Int choice, const Coord3D *spot, Bool hasSpot, Int list )
{
	Entry *e = entryOf( player );
	const PlayerTemplate *pt = templateOf( player );
	if( e == nullptr || pt == nullptr || TheGameLogic == nullptr )
		return;
	const UnsignedInt now = TheGameLogic->getFrame();
	const Bool validChoice = choice >= 0 && choice < listCount( pt, list );

	// Tiberium Project @feature the Crawler power: after the start, with its upgrade, none of its list standing, its wait over
	if( list == LIST_POWER )
	{
		if( !s_deployed || !validChoice || !hasSpot || spot == nullptr || !player->isPlayerActive() || !powerUnlocked( player ) )
			return;
		e->powerCrawler = lookForCrawler( *e, LIST_POWER );
		const char *why = nullptr;
		if( e->powerCrawler || now < e->powerReady || !isSpotAllowed( player, spot, &why ) )
		{
			if( TiberiumTest::isActive() )
				TiberiumTest::log( "frame %u: start: player %d's Crawler power at %.0f %.0f: refused (%s)", now, (int)e->player, spot->x, spot->y,
					why != nullptr ? why : ( e->powerCrawler ? "it has one" : "not yet" ) );
			return;
		}
		if( makeArrival( player, TheThingFactory->findTemplate( listName( pt, LIST_POWER, choice ) ), *spot ) != nullptr )
			e->powerCrawler = TRUE;
		return;
	}

	// the start: its choice and spot, until the countdown ends
	if( !s_deployed )
	{
		if( validChoice )
			e->chosen = choice;
		if( hasSpot && spot != nullptr )
		{
			const char *why = nullptr;
			// (its own spot is not in its way)
			const Bool had = e->hasSpot;
			e->hasSpot = FALSE;
			if( isSpotAllowed( player, spot, &why ) )
			{
				e->spot = *spot;
				e->spot.z = TheTerrainLogic->getGroundHeight( spot->x, spot->y );
				e->hasSpot = TRUE;
			}
			else
				e->hasSpot = had;
			if( TiberiumTest::isActive() )
				TiberiumTest::log( "frame %u: start: player %d picks %d at %.0f %.0f: %s", now, (int)e->player, (int)e->chosen,
					spot->x, spot->y, why == nullptr ? "taken" : why );
		}
		else if( TiberiumTest::isActive() )
			TiberiumTest::log( "frame %u: start: player %d picks %d", now, (int)e->player, (int)e->chosen );
		return;
	}

	// after it: another Crawler, when it has none and its wait is over (and it is still in the game)
	if( !validChoice || !hasSpot || spot == nullptr || !redeploys( player ) || !player->isPlayerActive() )
		return;
	e->crawler = lookForCrawler( *e, LIST_START );
	if( e->crawler && e->lostFrame == 0 )
	{
		if( TiberiumTest::isActive() )
			TiberiumTest::log( "frame %u: start: player %d calls another: refused, it has one", now, (int)e->player );
		return;
	}
	if( !e->crawler && e->lostFrame == 0 )
		e->lostFrame = now;
	const char *why = nullptr;
	if( e->crawler || now < getRedeployFrame( player, LIST_START ) || !isSpotAllowed( player, spot, &why ) )
	{
		if( TiberiumTest::isActive() )
			TiberiumTest::log( "frame %u: start: player %d calls another at %.0f %.0f: refused (%s)", now, (int)e->player, spot->x, spot->y,
				why != nullptr ? why : ( e->crawler ? "it has one" : "not yet" ) );
		return;
	}
	if( makeArrival( player, TheThingFactory->findTemplate( pt->getStartArrival( choice ) ), *spot ) != nullptr )
	{
		e->crawler = TRUE;
		e->lostFrame = 0;
		e->graceUntil = 0;
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumStart::decommission( Player *player, ObjectID id )
{
	Entry *e = entryOf( player );
	if( e == nullptr || !e->landed || e->leaving != INVALID_ID || TheGameLogic == nullptr )
		return;
	const PlayerTemplate *pt = templateOf( player );
	const UnsignedInt now = TheGameLogic->getFrame();
	for( Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject() )
	{
		if( obj->getControllingPlayer() != player || ( id != INVALID_ID && obj->getID() != id ) )
			continue;
		// one of the start's list (when it may call another), or of its Crawler power's
		const Bool start = isCrawlerOf( obj, pt, LIST_START ) && redeploys( player );
		if( !start && !isCrawlerOf( obj, pt, LIST_POWER ) )
			continue;
		// (still coming: not yet)
		static const NameKeyType arrivalKey = NAMEKEY( "TiberiumArrivalUpdate" );
		const TiberiumArrivalUpdate *arrival = (const TiberiumArrivalUpdate *)obj->findUpdateModule( arrivalKey );
		if( arrival != nullptr && arrival->isArriving() )
			return;
		e->leaving = obj->getID();
		e->leaveFrame = now + DECOMMISSION_TIME;
		if( start )
			e->graceUntil = e->leaveFrame + pt->getStartRedeployTime() + GRACE;
		obj->setDisabledUntil( DISABLED_EMP, e->leaveFrame );
		if( TiberiumTest::isActive() )
			TiberiumTest::log( "frame %u: start: player %d decommissions its %s: gone on frame %u", now, (int)e->player,
				obj->getTemplate()->getName().str(), e->leaveFrame );
		return;
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumStart::researchCrawlerPower( Player *player )
{
	Entry *e = entryOf( player );
	const UpgradeTemplate *up = powerUpgrade( player );
	if( e == nullptr || up == nullptr || TheGameLogic == nullptr || !s_deployed || e->tierDone != 0 || !player->isPlayerActive()
		|| player->hasUpgradeComplete( up ) || !TheUpgradeCenter->canAffordUpgrade( player, up, FALSE ) )
		return;
	const UnsignedInt now = TheGameLogic->getFrame();
	player->getMoney()->withdraw( up->calcCostToBuild( player ) );
	e->tierStart = now;
	e->tierDone = now + max( 1, up->calcTimeToBuild( player ) );
	if( TiberiumTest::isActive() )
		TiberiumTest::log( "frame %u: start: player %d researches %s: done on frame %u", now, (int)e->player, up->getUpgradeName().str(),
			e->tierDone );
}

//-------------------------------------------------------------------------------------------------
Int TiberiumStart::getCrawlerPowerResearch( const Player *player, Real *progress )
{
	const Entry *e = entryOf( player );
	const UpgradeTemplate *up = powerUpgrade( player );
	if( progress != nullptr )
		*progress = 0.0f;
	if( e == nullptr || up == nullptr )
		return RESEARCH_NONE;
	if( player->hasUpgradeComplete( up ) )
		return RESEARCH_DONE;
	if( e->tierDone != 0 )
	{
		if( progress != nullptr && TheGameLogic != nullptr && e->tierDone > e->tierStart )
		{
			const Real t = (Real)( TheGameLogic->getFrame() - e->tierStart ) / (Real)( e->tierDone - e->tierStart );
			*progress = t < 0.0f ? 0.0f : ( t > 1.0f ? 1.0f : t );
		}
		return RESEARCH_RUNNING;
	}
	return RESEARCH_OPEN;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::isActive()
{
	return !s_entries.empty();
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::isStartPending()
{
	return !s_entries.empty() && !s_deployed;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt TiberiumStart::getIntroEndFrame()
{
	return s_introEnd;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt TiberiumStart::getDeployFrame()
{
	return s_deployFrame;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::isKeptIn( const Player *player )
{
	const Entry *e = entryOf( player );
	if( e == nullptr )
		return FALSE;
	// its start is yet to land
	if( !e->landed )
		return TRUE;
	// its Crawler, either form, stands for its base (C&C4: a player is never out while it has one; Zero Hour's
	// rule counts structures only)
	if( lookForCrawler( *e, LIST_START ) || lookForCrawler( *e, LIST_POWER ) )
		return TRUE;
	// one that may call another is out only with nothing left
	if( redeploys( player ) && player->hasAnyObjects() )
		return TRUE;
	// a little while after decommissioning its last
	return TheGameLogic != nullptr && TheGameLogic->getFrame() < e->graceUntil;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::hasEntry( const Player *player )
{
	return entryOf( player ) != nullptr;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::startsByArrival( const Player *player )
{
	return entryOf( player ) != nullptr && usesArrival( templateOf( player ) );
}

//-------------------------------------------------------------------------------------------------
Int TiberiumStart::getChoiceCount( const Player *player, Int list )
{
	const PlayerTemplate *pt = entryOf( player ) != nullptr ? templateOf( player ) : nullptr;
	return min( (Int)MOST_CHOICES, listCount( pt, list ) );
}

//-------------------------------------------------------------------------------------------------
const ThingTemplate *TiberiumStart::getChoice( const Player *player, Int i, Int list )
{
	if( i < 0 || i >= getChoiceCount( player, list ) || TheThingFactory == nullptr )
		return nullptr;
	return TheThingFactory->findTemplate( listName( templateOf( player ), list, i ) );
}

//-------------------------------------------------------------------------------------------------
Int TiberiumStart::getChosen( const Player *player )
{
	const Entry *e = entryOf( player );
	return e != nullptr ? e->chosen : -1;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::getSpot( const Player *player, Coord3D *spot )
{
	const Entry *e = entryOf( player );
	if( e == nullptr || !e->hasSpot )
		return FALSE;
	*spot = e->spot;
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::getHome( const Player *player, Coord3D *home )
{
	const Entry *e = entryOf( player );
	if( e == nullptr )
		return FALSE;
	*home = e->home;
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::isSpotAllowed( const Player *player, const Coord3D *spot, const char **why )
{
	const char *dummy = nullptr;
	const char **reason = why != nullptr ? why : &dummy;
	*reason = nullptr;
	const Entry *me = entryOf( player );
	if( me == nullptr || spot == nullptr || TheTerrainLogic == nullptr || ThePlayerList == nullptr )
	{
		*reason = "no landing for this player";
		return FALSE;
	}
	Region3D extent;
	TheTerrainLogic->getMaximumPathfindExtent( &extent );
	if( spot->x < extent.lo.x + MARKER_RADIUS || spot->y < extent.lo.y + MARKER_RADIUS
		|| spot->x > extent.hi.x - MARKER_RADIUS || spot->y > extent.hi.y - MARKER_RADIUS )
	{
		*reason = "off the map";
		return FALSE;
	}
	if( TheTerrainLogic->isUnderwater( spot->x, spot->y ) || TheTerrainLogic->isCliffCell( spot->x, spot->y ) )
	{
		*reason = "not on clear ground";
		return FALSE;
	}

	// its own zones and its allies' (C&C4: its team's deploy zones), or, with no zones, near its own or an ally's start
	if( TiberiumZones::isOn() )
	{
		const Int zone = TiberiumZones::zoneAt( spot );
		const PlayerMaskType owners = zone >= 0 ? TiberiumZones::getOwners( zone ) : 0;
		Bool friendly = FALSE;
		for( Int i = 0; i < ThePlayerList->getPlayerCount() && !friendly; ++i )
		{
			const Player *p = ThePlayerList->getNthPlayer( i );
			friendly = p != nullptr && ( owners & p->getPlayerMask() ) != 0 && isFriend( player, p );
		}
		if( !friendly )
		{
			*reason = "not in a zone of its own or its allies'";
			return FALSE;
		}
	}
	else
	{
		const Real reach = TheGlobalData->m_zoneHomeRadius;
		Bool near = FALSE;
		for( size_t i = 0; i < s_entries.size() && !near; ++i )
		{
			const Entry &e = s_entries[i];
			if( !isFriend( player, playerOf( e ) ) )
				continue;
			const Real dx = spot->x - e.home.x, dy = spot->y - e.home.y;
			near = dx * dx + dy * dy <= reach * reach;
		}
		if( !near )
		{
			*reason = "not near its start or an ally's";
			return FALSE;
		}
	}

	// no structure of its own, an ally's or the neutral's in the way (C&C4's forbidden objects)
	if( ThePartitionManager != nullptr )
	{
		SimpleObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( spot, FORBIDDEN_RADIUS, FROM_CENTER_2D );
		MemoryPoolObjectHolder hold( iter );
		for( Object *obj = iter->first(); obj != nullptr; obj = iter->next() )
		{
			if( !obj->isKindOf( KINDOF_STRUCTURE ) || obj->isEffectivelyDead() )
				continue;
			const Team *team = obj->getTeam();
			if( team != nullptr && player->getRelationship( team ) == ENEMIES )
				continue;
			*reason = "a structure is in the way";
			return FALSE;
		}
	}

	// the start: nobody else's spot there
	if( !s_deployed )
	{
		for( size_t i = 0; i < s_entries.size(); ++i )
		{
			const Entry &e = s_entries[i];
			if( &e == me || !e.hasSpot )
				continue;
			const Real dx = spot->x - e.spot.x, dy = spot->y - e.spot.y;
			if( dx * dx + dy * dy < MARKER_RADIUS * MARKER_RADIUS )
			{
				*reason = "another player lands there";
				return FALSE;
			}
		}
	}
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Real TiberiumStart::getMarkerRadius()
{
	return MARKER_RADIUS;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::redeploys( const Player *player )
{
	const PlayerTemplate *pt = entryOf( player ) != nullptr ? templateOf( player ) : nullptr;
	return pt != nullptr && pt->getStartRedeployTime() > 0;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::hasCrawler( const Player *player, Int list )
{
	const Entry *e = entryOf( player );
	if( e == nullptr )
		return FALSE;
	return list == LIST_POWER ? e->powerCrawler : e->crawler;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::isCrawler( const Object *obj )
{
	const Player *player = obj != nullptr ? obj->getControllingPlayer() : nullptr;
	return entryOf( player ) != nullptr
		&& ( isCrawlerOf( obj, templateOf( player ), LIST_START ) || isCrawlerOf( obj, templateOf( player ), LIST_POWER ) );
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::hasCrawlerPower( const Player *player )
{
	return entryOf( player ) != nullptr && powerUnlocked( player );
}

//-------------------------------------------------------------------------------------------------
UnsignedInt TiberiumStart::getRedeployFrame( const Player *player, Int list )
{
	const Entry *e = entryOf( player );
	if( e == nullptr || !e->landed )
		return 0;
	if( list == LIST_POWER )
	{
		if( e->powerCrawler || !powerUnlocked( player ) )
			return 0;
		return max( (UnsignedInt)1, e->powerReady );
	}
	if( e->crawler || e->lostFrame == 0 || !redeploys( player ) )
		return 0;
	return e->lostFrame + templateOf( player )->getStartRedeployTime();
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStart::isDecommissioning( const Player *player )
{
	const Entry *e = entryOf( player );
	return e != nullptr && e->leaving != INVALID_ID;
}

//-------------------------------------------------------------------------------------------------
// Version Info:
// 1: Initial version
// 2: the Crawler power (powerCrawler, powerReady, tierStart, tierDone)
//-------------------------------------------------------------------------------------------------
void TiberiumStart::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	xfer->xferUnsignedInt( &s_introEnd );
	xfer->xferUnsignedInt( &s_deployFrame );
	xfer->xferBool( &s_deployed );
	UnsignedShort count = (UnsignedShort)s_entries.size();
	xfer->xferUnsignedShort( &count );
	if( xfer->getXferMode() == XFER_LOAD )
		s_entries.assign( count, Entry() );
	for( UnsignedShort i = 0; i < count; ++i )
	{
		Entry &e = s_entries[i];
		xfer->xferInt( &e.player );
		xfer->xferCoord3D( &e.home );
		xfer->xferInt( &e.chosen );
		xfer->xferCoord3D( &e.spot );
		xfer->xferBool( &e.hasSpot );
		xfer->xferBool( &e.landed );
		xfer->xferBool( &e.crawler );
		xfer->xferUnsignedInt( &e.lostFrame );
		xfer->xferObjectID( &e.leaving );
		xfer->xferUnsignedInt( &e.leaveFrame );
		xfer->xferUnsignedInt( &e.graceUntil );
		xfer->xferObjectID( &e.first );
		xfer->xferBool( &e.unitsDue );
		if( version >= 2 )
		{
			xfer->xferBool( &e.powerCrawler );
			xfer->xferUnsignedInt( &e.powerReady );
			xfer->xferUnsignedInt( &e.tierStart );
			xfer->xferUnsignedInt( &e.tierDone );
		}
		else if( xfer->getXferMode() == XFER_LOAD )
		{
			e.powerCrawler = FALSE;
			e.powerReady = 0;
			e.tierStart = e.tierDone = 0;
		}
	}
	if( xfer->getXferMode() == XFER_LOAD )
		s_loaded = TRUE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumStart::loadPostProcess()
{
	// a save made without the state (a retail-compatible save): the players made again, landed
	if( !s_loaded )
		rebuild();
	s_loaded = FALSE;
}
