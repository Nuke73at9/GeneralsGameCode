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

#include "PreRTS.h"
#include "GameClient/AbramPopup.h"

#include "Common/NameKeyGenerator.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/WindowLayout.h"
#include "GameLogic/GameLogic.h"

namespace
{

WindowLayout *popupLayout = nullptr;
GameWindow *popupParent = nullptr;
GameWindow *popupPanel = nullptr;
GameWindow *staticTextTitle = nullptr;
GameWindow *staticTextMessage = nullptr;
GameWindow *buttonChoices[AbramPopup::MAX_CHOICES] = { nullptr };

// The buttons' own text colours (from the layout), to put back on each open.
Color choiceTextColor = 0;
Color choiceTextBorderColor = 0;

AbramPopup::ChoiceFunc choiceFunc = nullptr;
void *choiceUserData = nullptr;

void findWindows()
{
	popupParent = TheWindowManager->winGetWindowFromId( nullptr,
		TheNameKeyGenerator->nameToKey( "AbramPopup.wnd:AbramPopupParent" ) );
	popupPanel = TheWindowManager->winGetWindowFromId( popupParent,
		TheNameKeyGenerator->nameToKey( "AbramPopup.wnd:SubParent" ) );
	staticTextTitle = TheWindowManager->winGetWindowFromId( popupParent,
		TheNameKeyGenerator->nameToKey( "AbramPopup.wnd:StaticTextTitle" ) );
	staticTextMessage = TheWindowManager->winGetWindowFromId( popupParent,
		TheNameKeyGenerator->nameToKey( "AbramPopup.wnd:StaticTextMessage" ) );

	for (Int i = 0; i < AbramPopup::MAX_CHOICES; ++i)
	{
		AsciiString name;
		name.format( "AbramPopup.wnd:ButtonChoice%d", i );
		buttonChoices[i] = TheWindowManager->winGetWindowFromId( popupParent, TheNameKeyGenerator->nameToKey( name ) );
	}

	if (buttonChoices[0] != nullptr)
	{
		choiceTextColor = buttonChoices[0]->winGetEnabledTextColor();
		choiceTextBorderColor = buttonChoices[0]->winGetEnabledTextBorderColor();
	}
}

void clearWindows()
{
	popupLayout = nullptr;
	popupParent = nullptr;
	popupPanel = nullptr;
	staticTextTitle = nullptr;
	staticTextMessage = nullptr;
	for (Int i = 0; i < AbramPopup::MAX_CHOICES; ++i)
		buttonChoices[i] = nullptr;
	choiceFunc = nullptr;
	choiceUserData = nullptr;
}

// The panel as tall as the buttons in use need (the space under the last one as above the title), in the
// middle of the screen. Its texts and buttons move with it.
void fitPanel( Int choiceCount )
{
	if (popupPanel == nullptr || popupParent == nullptr)
		return;

	GameWindow *last = staticTextMessage;
	for (Int i = choiceCount - 1; i >= 0; --i)
	{
		if (i < AbramPopup::MAX_CHOICES && buttonChoices[i] != nullptr)
		{
			last = buttonChoices[i];
			break;
		}
	}
	if (last == nullptr)
		return;

	Int panelX = 0, panelY = 0, panelW = 0, panelH = 0;
	popupPanel->winGetScreenPosition( &panelX, &panelY );
	popupPanel->winGetSize( &panelW, &panelH );
	Int lastX = 0, lastY = 0, lastW = 0, lastH = 0;
	last->winGetScreenPosition( &lastX, &lastY );
	last->winGetSize( &lastW, &lastH );

	Int margin = 0;
	if (staticTextTitle != nullptr)
	{
		Int titleX = 0, titleY = 0;
		staticTextTitle->winGetScreenPosition( &titleX, &titleY );
		margin = titleY - panelY;
	}
	if (margin < 4)
		margin = 4;

	const Int height = (lastY + lastH - panelY) + margin;
	if (height <= 0)
		return;

	Int parentX = 0, parentY = 0, parentW = 0, parentH = 0;
	popupParent->winGetScreenPosition( &parentX, &parentY );
	popupParent->winGetSize( &parentW, &parentH );

	popupPanel->winSetSize( panelW, height );
	Int relX = 0, relY = 0;
	popupPanel->winGetPosition( &relX, &relY );
	// in a game, in the middle of the view above the command bar (which draws over the screen's bottom)
	const Bool inGame = TheGameLogic != nullptr && TheGameLogic->isInGame() && !TheGameLogic->isInShellGame();
	const Int space = inGame ? parentH * 78 / 100 : parentH;
	Int top = (space - height) / 2;
	if (top < margin)
		top = (parentH - height) / 2;
	popupPanel->winSetPosition( relX, top > 0 ? top : 0 );
}

// Calls the owner. The owner may close or reopen the dialog from inside, so copy what it needs first.
void sendChoice( Int choice )
{
	AbramPopup::ChoiceFunc func = choiceFunc;
	void *userData = choiceUserData;
	if (func != nullptr)
		func( choice, userData );
	else
		AbramPopup::close();
}

} // namespace

Bool AbramPopup::open( const UnicodeString &title, const UnicodeString &message,
	const UnicodeString *choices, Int choiceCount, ChoiceFunc func, void *userData )
{
	if (popupLayout == nullptr)
	{
		popupLayout = TheWindowManager->winCreateLayout( AsciiString( ABRAM_POPUP_LAYOUT ) );
		if (popupLayout == nullptr)
			return FALSE;
		findWindows();
		if (popupParent == nullptr)
		{
			close();
			return FALSE;
		}
	}

	choiceFunc = func;
	choiceUserData = userData;

	if (staticTextTitle)
		GadgetStaticTextSetText( staticTextTitle, title );
	setMessage( message );

	for (Int i = 0; i < MAX_CHOICES; ++i)
	{
		if (buttonChoices[i] == nullptr)
			continue;
		const Bool used = (choices != nullptr && i < choiceCount);
		buttonChoices[i]->winHide( !used );
		buttonChoices[i]->winEnable( TRUE );
		buttonChoices[i]->winSetEnabledTextColors( choiceTextColor, choiceTextBorderColor );
		if (used)
			GadgetButtonSetText( buttonChoices[i], choices[i] );
	}
	fitPanel( choices != nullptr ? choiceCount : 0 );

	popupLayout->hide( FALSE );
	popupLayout->bringForward();
	TheWindowManager->winSetFocus( popupParent );
	return TRUE;
}

void AbramPopup::setMessage( const UnicodeString &message )
{
	if (staticTextMessage)
		GadgetStaticTextSetText( staticTextMessage, message );
}

void AbramPopup::setChoiceText( Int index, const UnicodeString &text )
{
	if (index >= 0 && index < MAX_CHOICES && buttonChoices[index])
		GadgetButtonSetText( buttonChoices[index], text );
}

void AbramPopup::setChoiceColor( Int index, Color color )
{
	if (index >= 0 && index < MAX_CHOICES && buttonChoices[index])
		buttonChoices[index]->winSetEnabledTextColors( color, choiceTextBorderColor );
}

void AbramPopup::setChoiceEnabled( Int index, Bool enabled )
{
	if (index >= 0 && index < MAX_CHOICES && buttonChoices[index] && buttonChoices[index]->winGetEnabled() != enabled)
		buttonChoices[index]->winEnable( enabled );
}

void AbramPopup::close()
{
	if (popupLayout != nullptr)
	{
		popupLayout->destroyWindows();
		deleteInstance( popupLayout );
	}
	clearWindows();
}

Bool AbramPopup::isOpen()
{
	return popupLayout != nullptr;
}

//-------------------------------------------------------------------------------------------------
void AbramPopupInit( WindowLayout *layout, void *userData )
{
	layout->hide( FALSE );
}

//-------------------------------------------------------------------------------------------------
WindowMsgHandledType AbramPopupInput( GameWindow *window, UnsignedInt msg,
	WindowMsgData mData1, WindowMsgData mData2 )
{
	if (msg == GWM_CHAR)
	{
		UnsignedByte key = mData1;
		UnsignedByte state = mData2;
		if (key == KEY_ESC)
		{
			if (BitIsSet( state, KEY_STATE_UP ))
				sendChoice( AbramPopup::CHOICE_CANCEL );
			return MSG_HANDLED;
		}
	}
	return MSG_IGNORED;
}

//-------------------------------------------------------------------------------------------------
WindowMsgHandledType AbramPopupSystem( GameWindow *window, UnsignedInt msg,
	WindowMsgData mData1, WindowMsgData mData2 )
{
	switch (msg)
	{
		case GWM_CREATE:
		case GWM_DESTROY:
			break;

		case GWM_INPUT_FOCUS:
		{
			// The dialog keeps the keyboard focus so Escape reaches it.
			if (mData1 == TRUE)
				*(Bool *)mData2 = TRUE;
			break;
		}

		case GBM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			for (Int i = 0; i < AbramPopup::MAX_CHOICES; ++i)
			{
				if (control != nullptr && control == buttonChoices[i])
				{
					sendChoice( i );
					break;
				}
			}
			break;
		}

		default:
			return MSG_IGNORED;
	}
	return MSG_HANDLED;
}
