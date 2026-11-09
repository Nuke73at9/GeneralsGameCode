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

// FILE: TiberiumHUD.h ////////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature the HUD in C&C4's layout: Zero Hour's bar along the bottom gives way to
//   - at the bottom left, C&C4's radar panel (its TacticalHUDCommandModeBar, drawn from its own shapes
//     and textures): the radar in its square, the menu, idle worker, beacon and general's buttons in
//     the slots of its column (the zones' in its top square)
//   - at the bottom right, C&C4's build panel (TacticalHUDMainCommandBar): the sidebar (TiberiumSidebar),
//     with the money, power and support points in its top boxes
//   - at the bottom in the middle, C&C4's selection panel, in its art (GDI's blue, Nod's red): the
//     selected unit's portrait, name and kind, its health, shield and rank, what it is; three of its
//     abilities in the slots at the panel's right and its other commands in a row above the panel
//     (Zero Hour's own command buttons, moved there, so they and their hotkeys work as they did);
//     several kinds selected: a picture of each kind and how many (a click keeps only that kind)
//   - under it, C&C4's experience bar as the tier bar (the player's tier: the Tech Up upgrades, and the
//     one being researched); the support powers' buttons (Zero Hour's shortcut bar) up the left edge
// Everything is sized by C&C4's units (its screen is 768 of them tall), as C&C4 places them.
//
// On with the sidebar, while the C&C4 art for it is there (TiberiumHUD.big: the TibHUD_* images,
// made from the player's own C&C4 by make_hudart.py). Interface only: the game's logic sees the same
// clicks and commands as with Zero Hour's bar.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class TiberiumHUD
{
public:
	/// once a frame in a game, after the control bar's: puts the HUD up (or takes it down) and keeps it up to date
	static void update();
	/// the end of a game: takes it down, giving Zero Hour's bar its windows back
	static void reset();
	/// whether the HUD is up (Zero Hour's bar is out of sight)
	static Bool isOn();
	/// whether the game hides its interface now (a cinematic hides the control bar): the HUD and sidebar hide with it
	static Bool isHidden();
	/// the screen rectangle the sidebar has: C&C4's build panel, bottom right; FALSE when off
	static Bool getSidebarArea( Int *x, Int *y, Int *w, Int *h );
	/// C&C4's build panel's top left corner on the screen, and its scale (C&C4's units to the screen's); FALSE when off
	static Bool getCommandBar( Int *x, Int *y, Real *scale );
	/// test runs: a click on a part of the HUD by what it is: "button" (0 menu, 1 idle worker, 2 beacon,
	/// 3 general), "row" or "slot" (the commands above the panel and the abilities in it, in order), "group"
	/// (several kinds selected: the kinds, in order) or "portrait"; whether something took it
	static Bool testClickPart( const char *part, Int index, Bool right );
	static Bool testPressPower( const char *buttonName );	///< (the test) a click on the Powers bar's power with that button
	/// test runs: what the HUD shows, for the log
	static void describe( char *buffer, Int size );
};
