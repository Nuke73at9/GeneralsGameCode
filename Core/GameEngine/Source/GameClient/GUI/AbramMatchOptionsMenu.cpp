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
#include "GameClient/AbramMatchOptionsMenu.h"

#include "Common/AbramGameRules.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/AbramPopup.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameNetwork/GameInfo.h"

namespace
{

enum Choice
{
	CHOICE_CASH_MULTIPLIER,
	CHOICE_BUILD_SPEED,
	CHOICE_LARGE_BUILD_QUEUE,
	CHOICE_LIMIT_SUPERWEAPONS,
	CHOICE_REVEAL_MAP,
	CHOICE_UNLOCK_ALL,
	CHOICE_DONE,
	CHOICE_COUNT
};

GameWindow *buttonMatchOptions = nullptr;
AbramMatchOptionsMenu::ChangedFunc changedFunc = nullptr;
GameInfo *editedGame = nullptr;

// Layout of the original skirmish setup screen (800x600 design units): SELECT MAP and the free space
// under it, above the Limit Superweapons check box.
const Real SELECT_MAP_DESIGN_HEIGHT = 24.0f;
const Real BUTTON_DESIGN_GAP = 3.0f;

const wchar_t *onOff( Bool on ) { return on ? L"On" : L"Off"; }

UnicodeString choiceText( Int choice, const GameInfo *game )
{
	const UnsignedInt rules = game->getAbramRules();
	UnicodeString text;
	switch (choice)
	{
	case CHOICE_CASH_MULTIPLIER:
		text.format( L"Cash Multiplier: x%u",
			AbramGameRules::getCashMultiplierChoice( AbramGameRules::getCashMultiplierIndex( rules ) ) );
		break;
	case CHOICE_BUILD_SPEED:
		text.format( L"Build Speed: x%u",
			AbramGameRules::getBuildSpeedChoice( AbramGameRules::getBuildSpeedIndex( rules ) ) );
		break;
	case CHOICE_LARGE_BUILD_QUEUE:
		text.format( L"99 Unit Queue: %ls", onOff( (rules & ABRAM_RULE_LARGE_BUILD_QUEUE) != 0 ) );
		break;
	case CHOICE_LIMIT_SUPERWEAPONS:
		text.format( L"Limit Superweapons: %ls", onOff( game->getSuperweaponRestriction() != 0 ) );
		break;
	case CHOICE_REVEAL_MAP:
		text.format( L"Reveal Map (No Fog): %ls", onOff( (rules & ABRAM_RULE_REVEAL_MAP) != 0 ) );
		break;
	case CHOICE_UNLOCK_ALL:
		text.format( L"Unlock Everything: %ls", onOff( (rules & ABRAM_RULE_UNLOCK_ALL) != 0 ) );
		break;
	default:
		text = L"Done";
		break;
	}
	return text;
}

void toggleRule( GameInfo *game, AbramGameRule rule )
{
	game->setAbramRules( game->getAbramRules() ^ (UnsignedInt)rule );
}

void onChoice( Int choice, void *userData )
{
	GameInfo *game = editedGame;
	if (game == nullptr || choice == CHOICE_DONE || choice == AbramPopup::CHOICE_CANCEL)
	{
		AbramPopup::close();
		editedGame = nullptr;
		return;
	}

	UnsignedInt rules = game->getAbramRules();
	switch (choice)
	{
	case CHOICE_CASH_MULTIPLIER:
		rules = AbramGameRules::setCashMultiplierIndex( rules,
			(AbramGameRules::getCashMultiplierIndex( rules ) + 1) % AbramGameRules::getCashMultiplierChoiceCount() );
		game->setAbramRules( rules );
		break;
	case CHOICE_BUILD_SPEED:
		rules = AbramGameRules::setBuildSpeedIndex( rules,
			(AbramGameRules::getBuildSpeedIndex( rules ) + 1) % AbramGameRules::getBuildSpeedChoiceCount() );
		game->setAbramRules( rules );
		break;
	case CHOICE_LARGE_BUILD_QUEUE:
		toggleRule( game, ABRAM_RULE_LARGE_BUILD_QUEUE );
		break;
	case CHOICE_LIMIT_SUPERWEAPONS:
		game->setSuperweaponRestriction( game->getSuperweaponRestriction() != 0 ? 0 : 1 );
		break;
	case CHOICE_REVEAL_MAP:
		toggleRule( game, ABRAM_RULE_REVEAL_MAP );
		break;
	case CHOICE_UNLOCK_ALL:
		toggleRule( game, ABRAM_RULE_UNLOCK_ALL );
		break;
	default:
		break;
	}

	AbramPopup::setChoiceText( choice, choiceText( choice, game ) );
	if (changedFunc != nullptr)
		changedFunc();
}

void openDialog( GameInfo *game )
{
	editedGame = game;

	UnicodeString choices[CHOICE_COUNT];
	for (Int i = 0; i < CHOICE_COUNT; ++i)
		choices[i] = choiceText( i, game );

	AbramPopup::open( UnicodeString( L"MATCH OPTIONS" ),
		UnicodeString( L"Click an option to change it. They apply to everyone in this match." ),
		choices, CHOICE_COUNT, onChoice, nullptr );
}

} // namespace

void AbramMatchOptionsMenu::init( GameWindow *selectMapButton, ChangedFunc onChanged )
{
	buttonMatchOptions = nullptr;
	changedFunc = onChanged;
	editedGame = nullptr;

	if (selectMapButton == nullptr)
		return;

	GameWindow *parent = selectMapButton->winGetParent();
	if (parent == nullptr)
		return;

	Int x = 0, y = 0, width = 0, height = 0;
	selectMapButton->winGetPosition( &x, &y );
	selectMapButton->winGetSize( &width, &height );

	// The layout is scaled to the screen resolution, so measure the scale on SELECT MAP.
	const Real scale = height > 0 ? (Real)height / SELECT_MAP_DESIGN_HEIGHT : 1.0f;
	const Int gap = REAL_TO_INT( BUTTON_DESIGN_GAP * scale );

	// Copy the look of SELECT MAP (images, colors, font), but none of its identity.
	WinInstanceData *selectMapData = selectMapButton->winGetInstanceData();
	WinInstanceData instData;
	instData.m_style = selectMapData->m_style;
	for (Int d = 0; d < MAX_DRAW_DATA; ++d)
	{
		instData.m_enabledDrawData[d] = selectMapData->m_enabledDrawData[d];
		instData.m_disabledDrawData[d] = selectMapData->m_disabledDrawData[d];
		instData.m_hiliteDrawData[d] = selectMapData->m_hiliteDrawData[d];
	}
	instData.m_enabledText = selectMapData->m_enabledText;
	instData.m_disabledText = selectMapData->m_disabledText;
	instData.m_hiliteText = selectMapData->m_hiliteText;
	instData.m_imageOffset = selectMapData->m_imageOffset;
	instData.m_font = selectMapData->m_font;
	instData.m_id = TheNameKeyGenerator->nameToKey( "AbramMatchOptions:ButtonMatchOptions" );
	instData.m_decoratedNameString = "AbramMatchOptions:ButtonMatchOptions";

	const UnsignedInt status = (selectMapButton->winGetStatus() & ~WIN_STATUS_HIDDEN) | WIN_STATUS_ENABLED;
	buttonMatchOptions = TheWindowManager->gogoGadgetPushButton( parent, status,
		x, y + height + gap, width, height, &instData, selectMapButton->winGetFont(), FALSE );
	if (buttonMatchOptions == nullptr)
		return;

	// Send clicks to the same window as the screen's own buttons.
	buttonMatchOptions->winSetOwner( selectMapButton->winGetOwner() );
	GadgetButtonSetText( buttonMatchOptions, UnicodeString( L"MATCH OPTIONS" ) );
}

Bool AbramMatchOptionsMenu::handleSelected( GameWindow *control, GameInfo *game )
{
	if (control == nullptr || buttonMatchOptions == nullptr || control != buttonMatchOptions)
		return FALSE;

	if (game != nullptr)
		openDialog( game );
	return TRUE;
}

void AbramMatchOptionsMenu::shutdown()
{
	if (editedGame != nullptr)
		AbramPopup::close();
	editedGame = nullptr;
	buttonMatchOptions = nullptr;
	changedFunc = nullptr;
}
