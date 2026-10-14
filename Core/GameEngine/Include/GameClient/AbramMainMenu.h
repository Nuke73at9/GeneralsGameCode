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

#pragma once

class GameWindow;

// Abram's Personal Test @feature Extra main menu buttons shared by Generals and Zero Hour:
//   - Mod: <name>             cycles through the mods in the Mods folder (and "None").
//   - Restart with this mod   restarts the game with the chosen mod. Shown only when the choice changed.
//   - Switch to <other game>  quits and starts the other game. Shown only when it is installed.
// The buttons are stacked in the bottom left corner of the main menu.

namespace AbramMainMenu
{
	// Creates the buttons. activeModFolder is the mod this session was started with (empty for none).
	void init( GameWindow *parent, const char *activeModFolder );

	// Handles a button click. Returns true when the click belonged to one of these buttons.
	// Sets quitGame to true when the game should now quit (a restart or switch is pending).
	bool handleSelected( GameWindow *control, bool &quitGame );
}
