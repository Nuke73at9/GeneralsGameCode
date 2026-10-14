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

// Abram's Personal Test @feature The in-game team menu, opened with Ctrl+G:
//   Take Control of AI / Release Control of AI
//   Give Unit(s)        - the selected units go to an ally
//   Give Building       - the one selected building goes to an ally
//   Request Unit(s)/Building - asks the ally who owns the selected unit(s) or building for them
// It also asks the local player to accept or deny what other players give or request.
//
// The menu only sends commands; GameLogic/AbramTeamActions does the actual changes.

class AbramTeamMenu
{
public:
	static void open();
	// Shows a waiting give or request for the local player. Call once per frame in a game.
	static void update();
	// Closes the menu, for the end of a game.
	static void reset();
};
