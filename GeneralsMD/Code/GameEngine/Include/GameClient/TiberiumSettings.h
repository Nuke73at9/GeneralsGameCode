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

// Tiberium Project @feature the Tiberium interface's settings panel, drawn by the game in the HUD's style (the user's
// layout A), in a GDI or a Nod look: the frame-rate limit, the display sync, smooth motion, real-time game speed, the
// skirmish speed and the look itself, kept in Options.ini. It opens in the place of Zero Hour's options screen where
// the mod's data turns the Tiberium interface on; Zero Hour's own options are one click further.

#pragma once

#include "Lib/BaseType.h"

class TiberiumSettings
{
public:
	enum Style { STYLE_GDI = 0, STYLE_NOD = 1 };

	static Bool wanted();								///< the mod's data turns the Tiberium interface on
	static void open();									///< over whatever is on screen
	static void close( Bool keep );			///< keep: the choices are applied and written to Options.ini
	static Bool isOpen();
	static Int getStyle();							///< the menus' look (InterfaceStyle in Options.ini): STYLE_GDI or STYLE_NOD

	// the test harness's: a click on a part ("tab", "option", "accept", "cancel", "defaults"), and what the panel shows
	static Bool testClick( const char *part, Int row, Int index );
	static void describe( char *buffer, Int size );
};
