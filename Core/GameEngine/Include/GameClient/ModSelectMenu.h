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

#include "GameClient/GameWindow.h"

class WindowLayout;

// Abram's Personal Test @feature The Mods screen (Window\Menus\ModSelectMenu.wnd).
// Lists the mods in the game's Mods folder with their picture and description, and restarts the
// game with the chosen mod. These callbacks are registered by name in the FunctionLexicon.

#define ABRAM_MOD_SELECT_MENU_LAYOUT "Menus/ModSelectMenu.wnd"

void ModSelectMenuInit( WindowLayout *layout, void *userData );
void ModSelectMenuUpdate( WindowLayout *layout, void *userData );
void ModSelectMenuShutdown( WindowLayout *layout, void *userData );
WindowMsgHandledType ModSelectMenuSystem( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 );
WindowMsgHandledType ModSelectMenuInput( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 );
