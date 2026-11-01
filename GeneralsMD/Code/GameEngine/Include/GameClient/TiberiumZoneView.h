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

// FILE: TiberiumZoneView.h ///////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's zones shown (TiberiumZones.h): on the radar, each zone's border in its
// holder's colour (grey: nobody's); over the world, when asked for (the HUD's Zones button) or while a
// structure is being placed in a game whose zones restrict building, the borders on the ground, each
// zone's holder, and its node's taking. Drawing only: nothing here touches the game.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"

class TiberiumZoneView
{
public:
	static void update();			///< InGameUI::update: puts the radar's drawing in place
	static void draw();				///< InGameUI::preDraw: the borders over the world
	static void reset();

	static Bool isAvailable();	///< a game with zones
	static void toggle();				///< the borders over the world on or off
	static Bool isShown();			///< the borders over the world now
	static void setShown( Bool on );
};
