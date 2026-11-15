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

// Tiberium Project @feature the Tiberium main menu: C&C4's arrangement (a ring with the menu's buttons stacked in it,
// the chosen one's own menu beside it), drawn by the game in clean lines over Zero Hour's main menu, in a GDI or a
// Nod look. Zero Hour's menu stays underneath, hidden, and does the work: a click here presses its button there, in
// the order a player would, so every screen opens as it always did. Only where the mod's data turns the Tiberium
// interface on.

#pragma once

#include "Lib/BaseType.h"

class GameWindow;
class WindowLayout;

class TiberiumMenu
{
public:
	static void init();																							///< Zero Hour's main menu comes onto the screen
	static void update( WindowLayout *layout, GameWindow *parent );	///< each frame Zero Hour's main menu is the screen
	static void shutdown();																					///< Zero Hour's main menu leaves the screen
	static Bool isOn();

	// the test harness's: a click on a part ("main", "side", "difficulty", "style"), and what the menu shows
	static Bool testClick( const char *part, Int index );
	static void describe( char *buffer, Int size );
};

// (Zero Hour's main menu, for the menu drawn over it)
Bool MainMenuCanTakeClick();		///< no transition running and no screen being opened
Bool MainMenuAsksDifficulty();	///< a campaign is chosen and its difficulty is being asked
void MainMenuOpenMods();				///< the Mods screen, as the Mods button opens it
