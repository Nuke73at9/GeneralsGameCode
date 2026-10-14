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

// Abram's Personal Test @feature A small dialog (Window\Menus\AbramPopup.wnd) with a title, a message and up
// to MAX_CHOICES buttons, used by the Match Options screen and the in-game team menu.
//
// Clicking a button calls the owner's function with the button's index; Escape calls it with
// CHOICE_CANCEL. The dialog stays open until the owner closes it or opens it again with new texts, so
// an owner can update a button in place (for options that change on each click).

#include "Common/UnicodeString.h"
#include "GameClient/GameWindow.h"

class WindowLayout;

#define ABRAM_POPUP_LAYOUT "Menus/AbramPopup.wnd"

class AbramPopup
{
public:
	enum
	{
		MAX_CHOICES = 8,
		CHOICE_CANCEL = -1,
	};

	typedef void (*ChoiceFunc)( Int choice, void *userData );

	// Shows the dialog (or replaces the texts of the open one). Returns FALSE if it could not be created.
	static Bool open( const UnicodeString &title, const UnicodeString &message,
		const UnicodeString *choices, Int choiceCount, ChoiceFunc func, void *userData );

	static void setMessage( const UnicodeString &message );
	static void setChoiceText( Int index, const UnicodeString &text );

	static void close();
	static Bool isOpen();
};

void AbramPopupInit( WindowLayout *layout, void *userData );
WindowMsgHandledType AbramPopupSystem( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 );
WindowMsgHandledType AbramPopupInput( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 );
