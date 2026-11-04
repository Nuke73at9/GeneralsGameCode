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

// FILE: AbramSpawnMenu.h /////////////////////////////////////////////////////////////////////////
// Abram's Personal Test @feature The cheats' spawn menu (the team menu's Spawn Menu, Ctrl+G, when the
// match's Enable Cheats is on): every faction's units and structures by their pictures, in a panel on
// the left of the screen. The cursor on one shows its name, price and description; a click on one puts it
// in hand, its see-through model following the cursor as a structure being placed does. A left click on
// the ground spawns it there for the player (a drag turns it first) and the next one stays in hand; a
// right click puts it away and closes the menu.
//
// The menu only sends MSG_ABRAM_CHEAT_SPAWN; the logic makes the object (GameLogic/AbramCheats.h).
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/BuildAssistant.h"
#include "Lib/BaseType.h"

class AbramSpawnMenu
{
public:
	static void open();				///< shows the menu, on the player's own faction
	static void close();			///< takes it down (and what is in hand)
	static Bool isOpen();
	static void update();			///< once a frame in a game
	static void reset();			///< the game's end

	/// a unit or structure of the menu's is in hand (its model at the cursor)
	static Bool isPlacing();
	/// the left click on the ground with one in hand: spawns it there (or says why it may not stand there)
	static void spawnAt( const Coord3D *where, Real angle );
	/// whether the one in hand may be spawned there (the placed model's tint)
	static LegalBuildCode checkPlacement( const Coord3D *where, Real angle );

	/// test runs: a click on the picture of that object (its faction and kind chosen first); whether found
	static Bool testPick( const char *templateName );
	/// test runs: what the menu shows, for the log
	static void describe( char *buffer, Int size );
};
