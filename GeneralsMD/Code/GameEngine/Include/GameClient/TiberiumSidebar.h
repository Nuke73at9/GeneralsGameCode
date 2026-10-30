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

// FILE: TiberiumSidebar.h ////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature a sidebar in Red Alert 3's way (in C&C4's colours): always there on the
// right, whatever is selected, with tabs (Structures, Defenses, Infantry, Vehicles, Aircraft,
// Upgrades), a queue for each producer that builds that tab's things (each factory, each deployed
// Crawler), and the queue's items with their progress. A click on an item queues it at that
// producer (no need to select it); a right click cancels the last of it. A structure is placed by a
// builder (the selected one, else the nearest).
//
// On when the game's data asks for it (TiberiumGameData: TiberiumSidebar = Yes); Zero Hour's own bar
// stays for the selected objects' commands. While a construction yard's structure is placed, the base's
// build areas (BuildAreaBehavior) are drawn on the ground (TiberiumGameData: BuildAreaCursor).
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class INI;

class TiberiumSidebar
{
public:
	/// once a frame in a game: makes the sidebar when the data asks for it, and keeps it up to date
	static void update();
	/// the end of a game: takes it down
	static void reset();
	/// the screen width the sidebar takes on the right (0 when off)
	static Int getWidth();
	/// test runs: a click at (fx, fy), fractions of the screen; whether the sidebar took it
	static Bool testClick( Real fx, Real fy, Bool right );
	/// test runs: a click on a part of the sidebar by what it is: "tab" (0 Base .. 5 Tech), "queue" (the
	/// current tab's, in order) or "item" (the current queue's); whether the sidebar took it
	static Bool testClickPart( const char *part, Int index, Bool right );
	/// test runs: the tab shown and its queues' and items' counts, for the log
	static void describe( char *buffer, Int size );
	/// TiberiumGameData's BuildAreaCursor: how the base's build areas are drawn on the ground while a
	/// construction yard's structure is placed (a radius cursor's fields: Texture, Style, Color, ...)
	static void parseBuildAreaCursor( INI *ini, void *instance, void *store, const void *userData );
	/// test runs: how many build areas are drawn
	static Int getBuildAreasDrawn();
};
