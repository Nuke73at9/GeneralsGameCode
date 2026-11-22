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

// FILE: TiberiumStartView.h //////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's match start and redeployment on screen (TiberiumStart.h), for a player
// whose faction starts that way:
//   - the intro: the camera held on the map's middle, then its pan to the player's start
//   - the "Match Countdown" box (00:SS:cc) at the top, and C&C4's SELECT YOUR CLASS box: a card for each
//     of its classes (the object's portrait and name), a line on the one under the mouse, CANCEL
//   - the landing marker: C&C4's ring (four inward marks, a pin, the player's name) following the mouse
//     until a click on the ground puts it down (green where it may land, red where not, and why); a click
//     anywhere else moves it until the countdown ends. Allies' markers are shown as well. The zones are
//     drawn meanwhile.
//   - DEPLOY! across the screen as the countdown ends
//   - after the start, with redeployment: DEPLOY MCV at the top once the Crawler is lost (its wait counted
//     down), which opens the box and the marker again; with a Crawler power (Tier IV), CALL CRAWLER in the same
//     way; DECOMMISSION when one of the player's Crawlers alone is selected (a second click within 3 s confirms)
// Drawing and the player's messages only: the logic decides (MSG_TIBERIUM_CRAWLER_PICK, ..._DECOMMISSION).
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"

class GameMessageTranslator;

class TiberiumStartView
{
public:
	static void update();				///< InGameUI's update (after the HUD's)
	static void draw();					///< InGameUI::preDraw: the markers over the world, the countdown, the banner
	static void reset();
	static GameMessageTranslator *makeTranslator();	///< the clicks that put the marker down (before the selection's)
	static Bool showsZones();		///< the start, or a landing being placed: the zones are drawn
	static Bool isPlacing();		///< the marker follows the mouse

	// test hooks (TiberiumTest)
	/// the box's card, then a click at the start + dx,dy (list 1: the Crawler power's)
	static void testPick( Int choice, Real dx, Real dy, Bool hasSpot, Int list = 0 );
	static void testDecommission();
	static void describe( char *buffer, Int size );
};
