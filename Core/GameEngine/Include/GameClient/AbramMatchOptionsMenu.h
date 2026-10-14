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

// Abram's Personal Test @feature The MATCH OPTIONS button of a game setup screen and the dialog it opens:
// cash multiplier, build speed, 99 unit queue, limit superweapons, reveal map and unlock everything
// (see Common/AbramGameRules.h).
//
// The button is created in code under the screen's SELECT MAP button and styled like it, so the
// original window layouts (and mods' versions of them) need no changes.

class GameInfo;
class GameWindow;

class AbramMatchOptionsMenu
{
public:
	typedef void (*ChangedFunc)();

	// Creates the button under selectMapButton. onChanged is called after the player changes an
	// option, so the screen can update what it shows (the Limit Superweapons check box).
	static void init( GameWindow *selectMapButton, ChangedFunc onChanged );

	// Call for GBM_SELECTED. Returns TRUE when the control was the MATCH OPTIONS button; the dialog is
	// then open and changes the game's options directly.
	static Bool handleSelected( GameWindow *control, GameInfo *game );

	// Closes the dialog, for when the screen goes away.
	static void shutdown();
};
