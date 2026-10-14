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
#include "GameClient/AbramMainMenu.h"

#include "Common/GameSwitcher.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"

namespace
{

GameWindow *buttonCustomMissionMods = nullptr; ///< Steam's Custom Mission button, used as MODS
GameWindow *buttonMods = nullptr;              ///< bottom left fallback when the Steam button is missing
GameWindow *buttonSwitchGame = nullptr;

const Int BUTTON_WIDTH = 220;
const Int BUTTON_HEIGHT = 26;
const Int BUTTON_SPACING = 6;
const Int MARGIN = 20;

GameWindow *createButton( GameWindow *parent, Int x, Int y, const wchar_t *label )
{
	WinInstanceData instData;
	instData.init();
	BitSet( instData.m_style, GWS_PUSH_BUTTON | GWS_MOUSE_TRACK );

	GameWindow *button = TheWindowManager->gogoGadgetPushButton( parent, WIN_STATUS_ENABLED,
		x, y, BUTTON_WIDTH, BUTTON_HEIGHT, &instData, nullptr, TRUE );
	if (button)
		GadgetButtonSetText( button, UnicodeString( label ) );
	return button;
}

} // namespace

void AbramMainMenu::init( GameWindow *parent )
{
	buttonCustomMissionMods = nullptr;
	buttonMods = nullptr;
	buttonSwitchGame = nullptr;

	if (parent == nullptr)
		return;

	// The Steam release adds this button to the Single Player menu. Our build has no code or text
	// for it, so it becomes the MODS button.
	buttonCustomMissionMods = TheWindowManager->winGetWindowFromId( parent,
		TheNameKeyGenerator->nameToKey( "MainMenu.wnd:ButtonCustomMission" ) );
	if (buttonCustomMissionMods)
		GadgetButtonSetText( buttonCustomMissionMods, UnicodeString( L"MODS" ) );

	Int parentWidth = 0;
	Int parentHeight = 0;
	parent->winGetSize( &parentWidth, &parentHeight );

	// Stack plain buttons upward from the bottom left corner.
	Int y = parentHeight - MARGIN - BUTTON_HEIGHT;

	if (GameSwitcher::isOtherGameAvailable())
	{
		buttonSwitchGame = createButton( parent, MARGIN, y, GameSwitcher::getButtonLabel() );
		y -= BUTTON_HEIGHT + BUTTON_SPACING;
	}

	if (buttonCustomMissionMods == nullptr)
		buttonMods = createButton( parent, MARGIN, y, L"Mods" );
}

AbramMainMenu::Action AbramMainMenu::handleSelected( GameWindow *control )
{
	if (control == nullptr)
		return ACTION_NONE;

	if (buttonCustomMissionMods != nullptr && control == buttonCustomMissionMods)
		return ACTION_OPEN_MODS_FROM_SINGLE_PLAYER;

	if (buttonMods != nullptr && control == buttonMods)
		return ACTION_OPEN_MODS;

	if (buttonSwitchGame != nullptr && control == buttonSwitchGame)
		return GameSwitcher::requestSwitch() ? ACTION_QUIT : ACTION_HANDLED;

	return ACTION_NONE;
}
