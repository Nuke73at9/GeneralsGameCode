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
#include "GameClient/KeyDefs.h"
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
#if RTS_ZEROHOUR
	CHOICE_ZONES,		// Tiberium Project @feature C&C4's zones
	CHOICE_ARMY_CAP,	// Tiberium Project @feature the army cap: Classic, Custom, Unlimited
	CHOICE_ARMY_POINTS,	// Tiberium Project @feature a custom army cap's command points per player
	CHOICE_CHEATS,	// Abram's Personal Test @feature the team menu's cheats (GameLogic/AbramCheats.h)
#endif
	CHOICE_DONE,
	CHOICE_COUNT
};

GameWindow *buttonMatchOptions = nullptr;
AbramMatchOptionsMenu::ChangedFunc changedFunc = nullptr;
GameInfo *editedGame = nullptr;
Bool editedReadOnly = FALSE;	// Tiberium Project @feature a network game's guest sees the host's options, and changes none

// Tiberium Project @feature the option a number is being typed into in the dialog (-1: none), what is typed so far, and
// whether the first digit replaces the number shown
Int typingChoice = -1;
std::string typedDigits;
Bool typedFresh = FALSE;

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
#if RTS_ZEROHOUR
	case CHOICE_ZONES:
	{
		static const wchar_t *const MODES[ABRAM_ZONE_MODE_COUNT] = { L"Off", L"Pre-zones", L"Expansion zones", L"Free zones" };
		text.format( L"Tiberium Zones: %ls", MODES[AbramGameRules::getZoneModeOf( rules )] );
		break;
	}
	case CHOICE_ARMY_CAP:
	{
		static const wchar_t *const CAPS[ABRAM_ARMY_CAP_MODE_COUNT] = { L"Classic", L"Custom", L"Unlimited" };
		text.format( L"Army Cap: %ls", CAPS[AbramGameRules::getArmyCapModeOf( rules )] );
		break;
	}
	case CHOICE_ARMY_POINTS:
		// (Classic shares C&C4's team totals out by team size; Unlimited has none)
		if (AbramGameRules::getArmyCapModeOf( rules ) == ABRAM_ARMY_CAP_CUSTOM)
			text.format( L"Command Points per Player: %d", AbramGameRules::getArmyCapPointsOf( rules ) );
		else if (AbramGameRules::getArmyCapModeOf( rules ) == ABRAM_ARMY_CAP_CLASSIC)
			text = L"Command Points per Player: by team size";
		else
			text = L"Command Points per Player: no cap";
		break;
	case CHOICE_CHEATS:
		text.format( L"Enable Cheats: %ls", onOff( (rules & ABRAM_RULE_CHEATS) != 0 ) );
		break;
#endif
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

/// the change a click on an option's line makes
void applyChoice( GameInfo *game, Int choice )
{
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
#if RTS_ZEROHOUR
	case CHOICE_ZONES:
		game->setAbramRules( AbramGameRules::setZoneMode( rules,
			(AbramZoneMode)((AbramGameRules::getZoneModeOf( rules ) + 1) % ABRAM_ZONE_MODE_COUNT) ) );
		break;
	case CHOICE_ARMY_CAP:
		game->setAbramRules( AbramGameRules::setArmyCapMode( rules,
			(AbramArmyCapMode)((AbramGameRules::getArmyCapModeOf( rules ) + 1) % ABRAM_ARMY_CAP_MODE_COUNT) ) );
		break;
	case CHOICE_ARMY_POINTS:
		// the cap made Custom at the points it has (the number itself is typed in: setOptionNumber)
		game->setAbramRules( AbramGameRules::setArmyCapMode( rules, ABRAM_ARMY_CAP_CUSTOM ) );
		break;
	case CHOICE_CHEATS:
		toggleRule( game, ABRAM_RULE_CHEATS );
		break;
#endif
	default:
		break;
	}
}

// Tiberium Project @feature a line as the dialog shows it: the one being typed into with what is typed and a caret
UnicodeString lineText( Int choice, const GameInfo *game )
{
#if RTS_ZEROHOUR
	if (choice == typingChoice && choice == CHOICE_ARMY_POINTS)
	{
		UnicodeString digits;
		digits.translate( AsciiString( typedDigits.c_str() ) );
		UnicodeString text;
		text.format( L"Command Points per Player: %ls_", digits.str() );
		return text;
	}
#endif
	return choiceText( choice, game );
}

UnicodeString dialogMessage()
{
	if (editedReadOnly)
		return UnicodeString( L"The host's options for this match. Only the host can change them." );
	if (typingChoice >= 0)
	{
		UnicodeString text;
		text.format( L"Type the command points each player gets (1 to %d), then press Enter.",
			AbramMatchOptionsMenu::getOptionNumberMax( typingChoice ) );
		return text;
	}
	return UnicodeString( L"Click an option to change it. They apply to everyone in this match." );
}

// Tiberium Project @feature every line read again (one option can change another's: the army cap and its points)
void refreshTexts()
{
	if (editedGame == nullptr)
		return;
	for (Int i = 0; i < CHOICE_COUNT; ++i)
		AbramPopup::setChoiceText( i, lineText( i, editedGame ) );
}

// Tiberium Project @feature the number typed set (`set`, and something typed), or left as it was
void stopTyping( Bool set )
{
	if (typingChoice < 0)
		return;
	const Int choice = typingChoice;
	typingChoice = -1;
	const Int value = AbramMatchOptionsMenu::numberOf( typedDigits, AbramMatchOptionsMenu::getOptionNumberMax( choice ) );
	if (set && value > 0 && editedGame != nullptr && !editedReadOnly)
		AbramMatchOptionsMenu::setOptionNumber( choice, editedGame, value );
	AbramPopup::setMessage( dialogMessage() );
	refreshTexts();
}

// Tiberium Project @feature while a number is typed, every key is the number's (Escape leaves it as it was and the
// dialog open)
Bool onKey( UnsignedByte key, UnsignedByte state )
{
	if (typingChoice < 0)
		return FALSE;
	switch (AbramMatchOptionsMenu::typeNumberKey( typedDigits, typedFresh, key, state,
		AbramMatchOptionsMenu::getOptionNumberMax( typingChoice ) ))
	{
	case AbramMatchOptionsMenu::NUMBER_KEY_SET:
		stopTyping( TRUE );
		break;
	case AbramMatchOptionsMenu::NUMBER_KEY_CANCEL:
		stopTyping( FALSE );
		break;
	case AbramMatchOptionsMenu::NUMBER_KEY_TYPED:
		if (editedGame != nullptr)
			AbramPopup::setChoiceText( typingChoice, lineText( typingChoice, editedGame ) );
		break;
	default:
		break;
	}
	return TRUE;
}

void onChoice( Int choice, void *userData )
{
	GameInfo *game = editedGame;
	// (a click anywhere but the line being typed into sets what was typed)
	if (typingChoice >= 0 && choice != typingChoice)
		stopTyping( TRUE );
	if (game == nullptr || choice == CHOICE_DONE || choice == AbramPopup::CHOICE_CANCEL)
	{
		AbramPopup::close();
		editedGame = nullptr;
		return;
	}
	// (a guest of a network game only looks: the host's options are what the match plays with)
	if (editedReadOnly)
		return;

	// Tiberium Project @feature an option set by a number: a click starts typing it (the option made to apply: the
	// cap Custom), a second click sets it
	if (AbramMatchOptionsMenu::isNumberOption( choice ))
	{
		if (typingChoice == choice)
		{
			stopTyping( TRUE );
			return;
		}
		applyChoice( game, choice );
		char digits[16];
		snprintf( digits, sizeof( digits ), "%d", (int)AbramMatchOptionsMenu::getOptionNumber( choice, game ) );
		typedDigits = digits;
		typedFresh = TRUE;
		typingChoice = choice;
		AbramPopup::setMessage( dialogMessage() );
		refreshTexts();
		if (changedFunc != nullptr)
			changedFunc();
		return;
	}

	applyChoice( game, choice );

	refreshTexts();
	if (changedFunc != nullptr)
		changedFunc();
}

void openDialog( GameInfo *game )
{
	editedGame = game;
	typingChoice = -1;

	UnicodeString choices[CHOICE_COUNT];
	for (Int i = 0; i < CHOICE_COUNT; ++i)
		choices[i] = choiceText( i, game );

	AbramPopup::open( UnicodeString( L"MATCH OPTIONS" ), dialogMessage(),
		choices, CHOICE_COUNT, onChoice, nullptr );
	AbramPopup::setKeyFunc( onKey );	// Tiberium Project: a number typed in
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

Bool AbramMatchOptionsMenu::handleSelected( GameWindow *control, GameInfo *game, Bool readOnly )
{
	if (control == nullptr || buttonMatchOptions == nullptr || control != buttonMatchOptions)
		return FALSE;

	editedReadOnly = readOnly;
	if (game != nullptr)
		openDialog( game );
	return TRUE;
}

void AbramMatchOptionsMenu::shutdown()
{
	typingChoice = -1;	// Tiberium Project
	if (editedGame != nullptr)
		AbramPopup::close();
	editedGame = nullptr;
	buttonMatchOptions = nullptr;
	changedFunc = nullptr;
}

// Tiberium Project @feature for a screen that shows the options itself (the Tiberium skirmish screen): the options
// the dialog lists, without its Done; an option's line as the dialog words it; the change a click on it makes there.
Int AbramMatchOptionsMenu::getOptionCount()
{
	return CHOICE_DONE;
}

UnicodeString AbramMatchOptionsMenu::getOptionText( Int option, const GameInfo *game )
{
	if (game == nullptr || option < 0 || option >= CHOICE_DONE)
		return UnicodeString::TheEmptyString;
	return choiceText( option, game );
}

void AbramMatchOptionsMenu::changeOption( Int option, GameInfo *game )
{
	if (game == nullptr || option < 0 || option >= CHOICE_DONE)
		return;
	applyChoice( game, option );
	if (changedFunc != nullptr)
		changedFunc();
}

// Tiberium Project @feature an option set by a number typed in (the custom army cap's points): which, its number, and
// setting it (kept within its range; the cap made Custom)
Bool AbramMatchOptionsMenu::isNumberOption( Int option )
{
#if RTS_ZEROHOUR
	return option == CHOICE_ARMY_POINTS;
#else
	return FALSE;
#endif
}

Int AbramMatchOptionsMenu::getOptionNumber( Int option, const GameInfo *game )
{
#if RTS_ZEROHOUR
	if (game != nullptr && option == CHOICE_ARMY_POINTS)
		return AbramGameRules::getArmyCapPointsOf( game->getAbramRules() );
#endif
	return 0;
}

Int AbramMatchOptionsMenu::getOptionNumberMax( Int option )
{
#if RTS_ZEROHOUR
	if (option == CHOICE_ARMY_POINTS)
		return ABRAM_ARMY_POINTS_MAX;
#endif
	return 0;
}

void AbramMatchOptionsMenu::setOptionNumber( Int option, GameInfo *game, Int value )
{
#if RTS_ZEROHOUR
	if (game == nullptr || option != CHOICE_ARMY_POINTS)
		return;
	UnsignedInt rules = AbramGameRules::setArmyCapMode( game->getAbramRules(), ABRAM_ARMY_CAP_CUSTOM );
	game->setAbramRules( AbramGameRules::setArmyCapPoints( rules, value ) );
	if (changedFunc != nullptr)
		changedFunc();
#endif
}

// Tiberium Project @feature typing a number: a key's effect on the digits typed so far
AbramMatchOptionsMenu::NumberKey AbramMatchOptionsMenu::typeNumberKey( std::string &digits, Bool &fresh, UnsignedByte key,
	UnsignedByte state, Int most )
{
	if (key == KEY_ESC)
		return BitIsSet( state, KEY_STATE_UP ) ? NUMBER_KEY_CANCEL : NUMBER_KEY_IGNORED;
	if (!BitIsSet( state, KEY_STATE_DOWN ))
		return NUMBER_KEY_IGNORED;

	// (no more digits than the most has: the number is kept within it when set)
	size_t mostDigits = 1;
	for (Int m = most; m >= 10; m /= 10)
		++mostDigits;

	static const UnsignedByte ROW[10] = { KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };
	static const UnsignedByte PAD[10] = { KEY_KP0, KEY_KP1, KEY_KP2, KEY_KP3, KEY_KP4, KEY_KP5, KEY_KP6, KEY_KP7, KEY_KP8, KEY_KP9 };
	for (Int d = 0; d < 10; ++d)
	{
		if (key != ROW[d] && key != PAD[d])
			continue;
		if (fresh)
			digits.clear();
		fresh = FALSE;
		if (digits.size() < mostDigits && !(digits.empty() && d == 0))
			digits += (char)('0' + d);
		return NUMBER_KEY_TYPED;
	}
	if (key == KEY_BACKSPACE)
	{
		fresh = FALSE;
		if (!digits.empty())
			digits.erase( digits.size() - 1 );
		return NUMBER_KEY_TYPED;
	}
	if (key == KEY_ENTER || key == KEY_KPENTER)
		return NUMBER_KEY_SET;
	return NUMBER_KEY_IGNORED;
}

// Tiberium Project @feature the digits' number, kept between 1 and the most (0 when nothing is typed); it stops adding
// digits once past the most, so it never overflows
Int AbramMatchOptionsMenu::numberOf( const std::string &digits, Int most )
{
	if (digits.empty())
		return 0;
	Int value = 0;
	for (size_t i = 0; i < digits.size() && value <= most; ++i)
	{
		if (digits[i] < '0' || digits[i] > '9')
			break;
		value = value * 10 + (digits[i] - '0');
	}
	if (value < 1)
		value = 1;
	if (value > most)
		value = most;
	return value;
}

// Tiberium Project @feature the open dialog's lines read again (a guest's, when the host's options arrive)
void AbramMatchOptionsMenu::refresh()
{
	if (editedGame != nullptr && AbramPopup::isOpen())
		refreshTexts();
}
