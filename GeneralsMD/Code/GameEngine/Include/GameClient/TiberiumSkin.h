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

// Tiberium Project @feature the parts the Tiberium menus are drawn with (TiberiumSettings, TiberiumMenu,
// TiberiumSkirmish): lines and flat fills in the shapes of C&C4's menus, in a GDI or a Nod look.

#pragma once

#include "Lib/BaseType.h"
#include "Common/UnicodeString.h"
#include "GameClient/Color.h"

class DisplayString;

namespace TiberiumSkin
{
	struct Look
	{
		Color accent, accentDim, hi, text, textDim, fill, fillOn, stripe, stripeOn, panel, grid, shade;
	};

	Look lookOf( Int style );																			///< TiberiumSettings::STYLE_GDI or STYLE_NOD
	DisplayString *textOf( const char *text, Int size, Bool bold );	///< made once, kept until freeTexts()
	DisplayString *textOf( const UnicodeString &text, Int size, Bool bold );
	void freeTexts();
	Int widthOf( const char *text, Int size, Bool bold );
	Int widthOf( const UnicodeString &text, Int size, Bool bold );
	void drawText( const char *text, Int size, Bool bold, Int x, Int y, Color color );
	void drawText( const UnicodeString &text, Int size, Bool bold, Int x, Int y, Color color );
	/// the text on one line, cut short with dots where it is wider than the room it has
	void drawTextIn( const UnicodeString &text, Int size, Bool bold, Int x, Int y, Int width, Color color );
	/// a field of a form: a flat box with its text; one that opens a list has a mark at its right end
	void drawField( const Look &l, Int x, Int y, Int w, Int h, Real u, const UnicodeString &text, Int size, Bool hover, Bool dim, Bool list );
	/// the text broken at its spaces to fit a width, a line under the other; returns the height it took
	Int drawWrapped( const char *text, Int size, Bool bold, Int x, Int y, Int width, Color color );
	/// C&C4's menu button, clean: a striped bar standing on a foot under each end (u: the screen's scale)
	void drawButton( const Look &l, Int x, Int y, Int w, Int h, Real u, const char *text, Int size, Bool on, Bool hover, Bool dim );
	/// C&C4's menu frame, clean: two outlines, a raised tab on its top edge, a nick in each side, a grid inside
	void drawFrame( const Look &l, Int x, Int y, Int w, Int h, Real u );
	/// a whole screen's ground on the 1280 by 720 sheet (ox: its left edge, u: its scale): Zero Hour's menu map darkened
	/// (alpha: how much), a line all round, a bright bar in its top edge, ticks down both sides, the screen's name
	void drawBackdrop( const Look &l, Int ox, Real u, Int sw, Int sh, const char *title, Int alpha );
	/// one of the Tiberium screens is up in the place of Zero Hour's own, which stays under it out of sight
	Bool overStockScreen();
}
