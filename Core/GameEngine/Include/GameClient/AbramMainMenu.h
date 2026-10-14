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
//   - MODS: the Steam version's "Custom Mission" button in the Single Player menu
//     (MainMenu.wnd:ButtonCustomMission) is renamed and opens the Mods screen. Without the Steam
//     menu data, a plain "Mods" button in the bottom left corner does the same.
//   - Switch to <other game>: quits and starts the other game. Shown only when it is installed.

namespace AbramMainMenu
{
	enum Action
	{
		ACTION_NONE,                 ///< The click was not for one of these buttons.
		ACTION_HANDLED,              ///< Handled, nothing more to do.
		ACTION_QUIT,                 ///< A switch is pending; the game should quit now.
		ACTION_OPEN_MODS,            ///< Open the Mods screen (from the bottom left button).
		ACTION_OPEN_MODS_FROM_SINGLE_PLAYER ///< Open the Mods screen (from the Single Player menu).
	};

	// Sets up the buttons. Call after the main menu layout is created.
	void init( GameWindow *parent );

	// Handles a button click and says what the main menu should do next.
	Action handleSelected( GameWindow *control );
}
