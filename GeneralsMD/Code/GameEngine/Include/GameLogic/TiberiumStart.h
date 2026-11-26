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

// FILE: TiberiumStart.h //////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's match start and redeployment, for the factions whose PlayerTemplate
// lists StartArrival (opt-in; every other faction starts as in Zero Hour):
//
//   PlayerTemplate FactionAmericaSuperWeaponGeneral
//     StartArrival = C4_GDICrawlerBVehicle C4_GDICrawlerAVehicle C4_GDICrawlerCVehicle   ; the classes, in the box's order
//     StartRedeployTime = 30000                ; ms after its Crawler is lost before another may be called (0: never)
//     StartArrivalUnits = Yes                  ; its StartingUnits come out beside the start's pick once it is out
//   End
//
// In a skirmish or a network game such a player starts with nothing on the map: no StartingBuilding, and
// its StartingUnits only with StartArrivalUnits. While the match intro and its countdown run (TiberiumGameData StartIntro, StartCountdown:
// C&C4's MatchIntroSettings, 1 s held and 5 s of camera pan, then 10 s), each picks one of the list (C&C4's
// SELECT YOUR CLASS) and a landing spot in its own or an ally's zone (TiberiumZones; with no zones, near its
// own or an ally's start). When the countdown ends every player's pick is made at its spot on the same
// frame, and arrives as its TiberiumArrivalUpdate has it (C&C4: the flare, the drop pod or the burrow pod,
// then the Crawler). One who has not picked gets the first of the list at its start; the AI picks at random.
//
// With a StartRedeployTime the player has one of the list at a time (C&C4's rules): when it is lost (killed,
// or decommissioned: held, then gone after DECOMMISSION_TIME), another may be called that much later, in the
// same way, anywhere the start allowed. The AI calls its own at its start.
//
// Defeat: Zero Hour's skirmish counts structures only. Such a player is not out while its start is yet to land,
// while one of its list stands (either form: it is its base, as in C&C4), while it may call another and still has
// anything on the map, or for a short while after it decommissions its last object.
//
// Tiberium Project @feature the Crawler power (the Tiberium factions' Tier IV), opt-in in the same way:
//   PlayerTemplate FactionAmerica
//     CrawlerPower = C4_GDICrawlerBVehicle C4_GDICrawlerAVehicle C4_GDICrawlerCVehicle
//     CrawlerPowerUpgrade = Upgrade_TibTierIV_GDI   ; the player upgrade that gives it (none: from the start)
//     CrawlerPowerWait = 360000                    ; ms after its Crawler is lost or decommissioned before another
//   End
// Such a player (whatever its start) may call one of that list as the start's are called, one at a time. The
// upgrade is researched here (MSG_TIBERIUM_CRAWLER_RESEARCH: its price, its time, its NeededObjects first).
//
// The picks are messages (MSG_TIBERIUM_CRAWLER_PICK, MSG_TIBERIUM_CRAWLER_DECOMMISSION), so every machine
// makes the same; the state is saved with the game logic (it is made again from the players after a load of
// a save that has none).
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/GameCommon.h"

class Object;
class Player;
class PlayerTemplate;
class ThingTemplate;
class Xfer;

class TiberiumStart
{
public:
	enum
	{
		LIST_START = 0,		///< the start's list (StartArrival)
		LIST_POWER = 1		///< the Crawler power's (CrawlerPower)
	};

	// the logic's side
	static Bool usesArrival( const PlayerTemplate *pt );		///< the faction starts and redeploys this way
	static Bool hasEntryData( const PlayerTemplate *pt );		///< it starts this way, or has a Crawler power
	/// GameLogic::startNewGame, in place of the start buildings ('home' is the player's start); and for a faction that
	/// starts as Zero Hour's with a Crawler power, after them
	static void addPlayer( Player *player, const PlayerTemplate *pt, const Coord3D *home );
	static void begin( Bool loadingSaveGame );							///< once the players are placed (and after a load)
	static void reset();
	static void update();																	///< each logic frame
	static void xfer( Xfer *xfer );
	static void loadPostProcess();
	/// a player's pick (MSG_TIBERIUM_CRAWLER_PICK): before the start's end, its choice and spot; after, a Crawler called
	/// (the start's list's, or the Crawler power's)
	static void pick( Player *player, Int choice, const Coord3D *spot, Bool hasSpot, Int list = LIST_START );
	/// MSG_TIBERIUM_CRAWLER_DECOMMISSION: its Crawler 'id' (INVALID_ID: the start's list's first it has)
	static void decommission( Player *player, ObjectID id );
	/// MSG_TIBERIUM_CRAWLER_RESEARCH: the Crawler power's upgrade bought (its NeededObjects standing, its price paid), done
	/// after its research time
	static void researchCrawlerPower( Player *player );

	// what the client and the victory check ask (read only)
	static Bool isActive();																///< someone in this game starts this way
	static Bool isStartPending();													///< the start's countdown runs
	static UnsignedInt getIntroEndFrame();									///< the class box from here
	static UnsignedInt getDeployFrame();										///< the arrivals then
	/// not to be defeated now (Zero Hour's rule counts structures): its start is yet to land, it has one of its list
	/// (either form), it may call another and still has something, or it has just decommissioned its last
	static Bool isKeptIn( const Player *player );
	static Bool hasEntry( const Player *player );
	static Bool startsByArrival( const Player *player );		///< it has an entry and its faction starts this way (StartArrival)
	static Int getChoiceCount( const Player *player, Int list = LIST_START );
	static const ThingTemplate *getChoice( const Player *player, Int i, Int list = LIST_START );
	static Int getChosen( const Player *player );						///< -1: not yet
	static Bool getSpot( const Player *player, Coord3D *spot );	///< its start's landing spot, once picked
	static Bool getHome( const Player *player, Coord3D *home );
	/// whether 'player' may land at 'spot' (its or an ally's zone, clear ground, no structure near); 'why' (may be
	/// null) gets a reason's label
	static Bool isSpotAllowed( const Player *player, const Coord3D *spot, const char **why );
	static Real getMarkerRadius();												///< C&C4's landing ring (125)
	static Bool redeploys( const Player *player );					///< it may call another after a loss
	static Bool hasCrawler( const Player *player, Int list = LIST_START );	///< one of its list, either form, is on the map
	static Bool isCrawler( const Object *obj );							///< one of its player's lists, either form
	static Bool hasCrawlerPower( const Player *player );		///< it has its Crawler power's upgrade
	enum { RESEARCH_NONE, RESEARCH_OPEN, RESEARCH_RUNNING, RESEARCH_DONE };
	/// where its Crawler power's upgrade stands ('progress': the research's share done)
	static Int getCrawlerPowerResearch( const Player *player, Real *progress );
	/// when it may call one (0: not now: it has one, or never; the Crawler power: once it has its upgrade)
	static UnsignedInt getRedeployFrame( const Player *player, Int list = LIST_START );
	/// Tiberium Project @feature when it may call one of that class (0: not now); the Crawler power's with Tier V: each class
	/// its own, else as getRedeployFrame
	static UnsignedInt getClassReadyFrame( const Player *player, Int list, Int choice );
	/// its Tier V (the Crawler power and CrawlerPowerNodes control nodes held): whether it has it, the nodes it holds and needs
	static Bool getTierFive( const Player *player, Int *held, Int *needed );
	static Bool isDecommissioning( const Player *player );

	enum
	{
		MOST_CHOICES = 4,
		DECOMMISSION_TIME = LOGICFRAMES_PER_SECOND * 10		///< C&C4's RecycleLifetimeUpdate: 10 s held, then gone
	};
};
