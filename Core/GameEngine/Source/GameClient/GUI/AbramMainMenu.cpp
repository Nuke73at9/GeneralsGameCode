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
#include "Common/ModManager.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"

namespace
{

GameWindow *buttonModSelect = nullptr;
GameWindow *buttonModApply = nullptr;
GameWindow *buttonSwitchGame = nullptr;

// Mod index: -1 means no mod (vanilla). The active mod's folder is kept as text, because a rescan
// can change the order of the list.
char activeModFolderPath[260] = { 0 };
int activeMod = -1;
int selectedMod = -1;

const Int BUTTON_WIDTH = 260;
const Int BUTTON_HEIGHT = 26;
const Int BUTTON_SPACING = 6;
const Int MARGIN = 20;

GameWindow *createButton( GameWindow *parent, Int x, Int y )
{
	WinInstanceData instData;
	instData.init();
	BitSet( instData.m_style, GWS_PUSH_BUTTON | GWS_MOUSE_TRACK );

	return TheWindowManager->gogoGadgetPushButton( parent, WIN_STATUS_ENABLED,
		x, y, BUTTON_WIDTH, BUTTON_HEIGHT, &instData, nullptr, TRUE );
}

UnicodeString modDisplayName( int index )
{
	if (index < 0)
		return UnicodeString(L"None (original game)");

	UnicodeString name;
	name.translate( AsciiString(ModManager::getModName(index)) );
	return name;
}

void refreshModButtons()
{
	if (buttonModSelect)
	{
		UnicodeString text;
		if (ModManager::getModCount() == 0)
			text = UnicodeString(L"Mods: none installed");
		else
			text.format( L"Mod: %ls", modDisplayName(selectedMod).str() );
		GadgetButtonSetText( buttonModSelect, text );
	}

	if (buttonModApply)
	{
		const bool changed = (selectedMod != activeMod);
		buttonModApply->winHide( !changed );
		if (changed)
		{
			GadgetButtonSetText( buttonModApply,
				UnicodeString(selectedMod < 0 ? L"Restart without mods" : L"Restart with this mod") );
		}
	}
}

} // namespace

void AbramMainMenu::init( GameWindow *parent, const char *activeModFolder )
{
	buttonModSelect = nullptr;
	buttonModApply = nullptr;
	buttonSwitchGame = nullptr;

	if (parent == nullptr)
		return;

	strncpy( activeModFolderPath, activeModFolder ? activeModFolder : "", sizeof(activeModFolderPath) );
	activeModFolderPath[sizeof(activeModFolderPath) - 1] = '\0';

	ModManager::scan();
	activeMod = ModManager::findModByFolder( activeModFolderPath );
	selectedMod = activeMod;

	Int parentWidth = 0;
	Int parentHeight = 0;
	parent->winGetSize( &parentWidth, &parentHeight );

	// Stack the buttons upward from the bottom left corner.
	Int y = parentHeight - MARGIN - BUTTON_HEIGHT;

	if (GameSwitcher::isOtherGameAvailable())
	{
		buttonSwitchGame = createButton( parent, MARGIN, y );
		if (buttonSwitchGame)
			GadgetButtonSetText( buttonSwitchGame, UnicodeString(GameSwitcher::getButtonLabel()) );
	}
	y -= BUTTON_HEIGHT + BUTTON_SPACING;

	buttonModApply = createButton( parent, MARGIN, y );
	y -= BUTTON_HEIGHT + BUTTON_SPACING;

	buttonModSelect = createButton( parent, MARGIN, y );

	refreshModButtons();
}

bool AbramMainMenu::handleSelected( GameWindow *control, bool &quitGame )
{
	quitGame = false;

	if (control == nullptr)
		return false;

	if (control == buttonModSelect)
	{
		// Look again each click, so newly copied mods show up without restarting the game.
		ModManager::scan();
		const int count = ModManager::getModCount();
		if (selectedMod >= count)
			selectedMod = -1;
		if (count > 0)
		{
			// Cycle: None -> first mod -> ... -> last mod -> None.
			selectedMod = (selectedMod + 1 >= count) ? -1 : selectedMod + 1;
		}
		else
		{
			selectedMod = -1;
		}
		activeMod = ModManager::findModByFolder( activeModFolderPath );
		refreshModButtons();
		return true;
	}

	if (control == buttonModApply)
	{
		const char *folder = (selectedMod >= 0) ? ModManager::getModFolder(selectedMod) : "";
		quitGame = GameSwitcher::requestRestartWithMod( folder );
		return true;
	}

	if (control == buttonSwitchGame)
	{
		quitGame = GameSwitcher::requestSwitch();
		return true;
	}

	return false;
}
