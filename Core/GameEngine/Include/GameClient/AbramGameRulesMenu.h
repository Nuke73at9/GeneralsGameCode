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

// Abram's Personal Test @feature Check boxes for the optional gameplay rules (Common/AbramGameRules.h)
// in a game setup screen.
//
// The boxes are created in code, styled like the screen's "Limit Superweapons" check box and placed
// in the free row under the player list, so the original window layouts (and mods' versions of them)
// need no changes.

class GameInfo;
class GameWindow;

class AbramGameRulesMenu
{
public:
	// Creates the check boxes. anchorCheckBox is the screen's "Limit Superweapons" check box.
	static void init( GameWindow *anchorCheckBox );

	// Sets the check boxes from the game's rules.
	static void refresh( const GameInfo *game );

	// Call for GBM_SELECTED. Returns TRUE when the control was one of the rule check boxes; the
	// game's rules are then updated.
	static Bool handleSelected( GameWindow *control, GameInfo *game );

	// Enables or disables the check boxes, for screens where only the host may change options.
	static void enable( Bool enable );
};
