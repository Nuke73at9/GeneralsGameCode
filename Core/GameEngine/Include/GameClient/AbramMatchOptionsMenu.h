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

#include "Common/UnicodeString.h"

#include <string>

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
	// Tiberium Project @feature readOnly: a network game's guest sees the host's options and can change none.
	static Bool handleSelected( GameWindow *control, GameInfo *game, Bool readOnly = FALSE );

	// Closes the dialog, for when the screen goes away.
	static void shutdown();

	// Tiberium Project @feature for a screen that shows the options itself (the Tiberium skirmish screen): how many
	// options the dialog lists (without its Done), an option's line as the dialog words it ("Cash Multiplier: x2"),
	// and the change a click on that line makes. The screen's onChanged is called as after a click in the dialog.
	static Int getOptionCount();
	static UnicodeString getOptionText( Int option, const GameInfo *game );
	static void changeOption( Int option, GameInfo *game );
	// Tiberium Project @feature an option set by a number typed in (the custom army cap's command points per player):
	// whether this one is, its number, its most, and setting it (kept between 1 and its most)
	static Bool isNumberOption( Int option );
	static Int getOptionNumber( Int option, const GameInfo *game );
	static Int getOptionNumberMax( Int option );
	static void setOptionNumber( Int option, GameInfo *game, Int value );

	// Tiberium Project @feature typing a number (the dialog's and the Tiberium skirmish screen's): a key's effect on the
	// digits typed so far (the number row and the keypad's digits, Backspace; no leading zero, no more digits than `most`
	// has; `fresh`: the first digit replaces the number shown), Enter to set it, Escape to leave it as it was; and the
	// digits' number, kept between 1 and `most` (0 when nothing is typed)
	enum NumberKey { NUMBER_KEY_IGNORED, NUMBER_KEY_TYPED, NUMBER_KEY_SET, NUMBER_KEY_CANCEL };
	static NumberKey typeNumberKey( std::string &digits, Bool &fresh, UnsignedByte key, UnsignedByte state, Int most );
	static Int numberOf( const std::string &digits, Int most );

	// Tiberium Project @feature the open dialog's lines read again from the game (a guest's, when the host's options
	// arrive)
	static void refresh();
};
