/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

// Tiberium Project @feature the Tiberium skirmish screen: C&C4's match set-up arrangement (the players' cards down
// both sides of the map, the match's options under it, Back and Play at the bottom), drawn by the game in clean lines
// over Zero Hour's skirmish screen, in a GDI or a Nod look. Zero Hour's screen stays underneath, hidden, and does the
// work: a choice here is set in its combo box there and a button here presses its button there, so every rule of the
// stock screen (colours taken, start positions, the map's player count) still holds. Its map list is shown the same
// way. Only where the mod's data turns the Tiberium interface on.

#pragma once

#include "Lib/BaseType.h"

class GameWindow;
class WindowLayout;

class TiberiumSkirmish
{
public:
	static void init( WindowLayout *layout );												///< Zero Hour's skirmish screen comes onto the screen
	static void update( WindowLayout *layout, GameWindow *parent );	///< each frame Zero Hour's skirmish screen is the screen
	static void shutdown();																					///< Zero Hour's skirmish screen leaves the screen
	static Bool isOn();

	// the test harness's: a click on a part, and what the screen shows.
	// "player|faction|team|color", row, entry: that entry of the row's list; "cash", entry; "open", row, field: the list
	// opened only; "start|unstart", position; "limit"; "maps", "options", "reset", "play", "back"; with the map list up:
	// "maprow", row; "maptab", 0 official 1 the player's; "mapuse"; "mapback"
	static Bool testClick( const char *part, Int a, Int b );
	static void describe( char *buffer, Int size );
};
