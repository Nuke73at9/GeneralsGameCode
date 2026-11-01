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

// FILE: TiberiumZones.h //////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's zones in a skirmish: the land around each start (allies' starts near
// each other share theirs) and around the supplies and tech buildings, where a player's base may stand.
// The match's zone mode (AbramGameRules::getZoneMode, the skirmish's Match Options) decides:
//   Pre-zones       structures only in the player's start zone (Crawlers deploy and make units anywhere)
//   Expansion zones the start zone, and the zones taken: their node captured (C&C4's TCN), an outpost
//                   built in one that is nobody's, a Crawler deployed in it
//   Free zones      structures anywhere, as Zero Hour; the zones are taken as above
// In every mode a Crawler arrives in one of its player's zones (TiberiumArrivalUpdate).
// The zones are made from the map when the game starts (the same on every machine and after a load);
// who holds an expansion zone is worked out from the objects in it, so nothing needs saving.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/AbramGameRules.h"
#include "Common/GameType.h"

class Object;
class Player;
class ThingTemplate;

class TiberiumZones
{
public:
	enum
	{
		MAX_ZONES = 64,
		NO_ZONE = 0xFF
	};

	static void build( Bool loadingSaveGame );	///< GameLogic::startNewGame, once the start buildings are placed
	static void reset();
	static void update();												///< each logic frame (who holds what, every second)

	static AbramZoneMode getMode();							///< off when there are no zones (not a skirmish, a map without starts)
	static Bool isOn() { return getMode() != ABRAM_ZONES_OFF; }

	static Int getCount();
	static Int zoneAt( const Coord3D *pos );		///< -1: in none
	static Bool isHome( Int zone );							///< a start zone
	static PlayerMaskType getOwners( Int zone );	///< who may build there (a start zone: its players)
	static const Coord3D *getCenter( Int zone );
	static Bool isOwnedBy( Int zone, const Player *player );

	/// whether 'player' may put 'build' at 'pos' (TRUE for what the zones do not restrict)
	static Bool mayBuild( const Player *player, const ThingTemplate *build, const Coord3D *pos );
	/// whether 'build' takes a zone that is nobody's (an outpost, a factory, a deployed Crawler)
	static Bool isClaimer( const ThingTemplate *build );
	/// where one of the player's Crawlers arrives: in its zone nearest 'near'; FALSE if it has none
	static Bool findArrival( const Player *player, const Coord3D *near, Coord3D *spot );

	// the grid, for drawing
	static Int getGridWidth();
	static Int getGridHeight();
	static Real getCellSize();
	static void getGridOrigin( Real *x, Real *y );
	static Int cellZone( Int x, Int y );				///< -1: in none
};
