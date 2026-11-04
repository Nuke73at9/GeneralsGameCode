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
#include "GameClient/AbramTeamMenu.h"

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Recorder.h"
#include "GameClient/AbramPopup.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/AbramTeamActions.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameNetwork/GameInfo.h"
#if RTS_ZEROHOUR
#include "GameClient/AbramSpawnMenu.h"
#include "GameLogic/AbramCheats.h"
#endif

#include <set>
#include <vector>

namespace
{

enum MenuState
{
	STATE_CLOSED,
	STATE_MAIN,
	STATE_PICK_AI,
	STATE_PICK_RECEIVER,
	STATE_CONFIRM_REQUEST,
	STATE_OFFER,
};

enum MainChoice
{
	MAIN_CONTROL_AI,
	MAIN_GIVE_UNITS,
	MAIN_GIVE_BUILDING,
	MAIN_REQUEST,
#if RTS_ZEROHOUR
	// Abram's Personal Test @feature the match's cheats (Enable Cheats in the match options)
	MAIN_FREEZE_AI,
	MAIN_SPAWN,
	MAIN_UNLOCK_POWERS,
	MAIN_UNLIMITED_POWER,
	MAIN_UNLIMITED_MONEY,
#endif
	MAIN_CLOSE,
	MAIN_CHOICE_COUNT
};

MenuState state = STATE_CLOSED;

// The main menu's buttons in order (the cheats only in a match that has them), and their texts shown.
std::vector<Int> mainChoices;
std::vector<UnicodeString> mainTexts;

// What the current step is about.
std::vector<ObjectID> pendingObjects;
std::vector<Int> choicePlayers;      // player index for each player button
Int requestOwnerIndex = -1;
Int shownOfferId = 0;
std::set<Int> answeredOffers;        // offers this machine already asked about

void showMain( const UnicodeString &message );
void onChoice( Int choice, void *userData );

UnicodeString playerName( Player *player )
{
	return player != nullptr ? player->getPlayerDisplayName() : UnicodeString( L"?" );
}

void closeMenu()
{
	state = STATE_CLOSED;
	pendingObjects.clear();
	choicePlayers.clear();
	requestOwnerIndex = -1;
	shownOfferId = 0;
	AbramPopup::close();
}

Bool isControllingAI()
{
	Player *local = ThePlayerList->getLocalPlayer();
	return local != nullptr && AbramTeamActions::getControllerOf( local->getPlayerIndex() ) >= 0;
}

// The selected objects, split into the local player's units, the local player's buildings and other
// players' objects.
void collectSelection( std::vector<Object *> &myUnits, std::vector<Object *> &myBuildings, std::vector<Object *> &others )
{
	myUnits.clear();
	myBuildings.clear();
	others.clear();

	Player *local = ThePlayerList->getLocalPlayer();
	const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
	if (selected == nullptr || local == nullptr)
		return;

	for (DrawableList::const_iterator it = selected->begin(); it != selected->end(); ++it)
	{
		Object *obj = (*it) ? (*it)->getObject() : nullptr;
		if (obj == nullptr || obj->isEffectivelyDead())
			continue;
		if (obj->getControllingPlayer() != local)
			others.push_back( obj );
		else if (obj->isKindOf( KINDOF_STRUCTURE ))
			myBuildings.push_back( obj );
		else
			myUnits.push_back( obj );
	}
}

void sendTransfer( Int otherPlayerIndex, AbramTransferType type, const std::vector<ObjectID> &ids )
{
	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_ABRAM_TRANSFER_OBJECTS );
	msg->appendIntegerArgument( otherPlayerIndex );
	msg->appendIntegerArgument( (Int)type );
	for (size_t i = 0; i < ids.size() && (Int)i < AbramTeamActions::MAX_TRANSFER_OBJECTS; ++i)
		msg->appendObjectIDArgument( ids[i] );
}

void showPlayerChoice( MenuState newState, const UnicodeString &title, const UnicodeString &message,
	const std::vector<Int> &players )
{
	state = newState;
	choicePlayers = players;

	UnicodeString choices[AbramPopup::MAX_CHOICES];
	Int count = 0;
	for (size_t i = 0; i < players.size() && count < AbramPopup::MAX_CHOICES - 1; ++i)
		choices[count++] = playerName( ThePlayerList->getNthPlayer( players[i] ) );
	choices[count++] = UnicodeString( L"Back" );

	AbramPopup::open( title, message, choices, count, onChoice, nullptr );

	// Abram's Personal Test @feature each player in its own colour
	for (Int i = 0; i < count - 1 && i < (Int)players.size(); ++i)
	{
		const Player *player = ThePlayerList->getNthPlayer( players[i] );
		if (player != nullptr)
			AbramPopup::setChoiceColor( i, player->getPlayerColor() );
	}
}

void startGive( Bool buildings )
{
	std::vector<Object *> myUnits, myBuildings, others;
	collectSelection( myUnits, myBuildings, others );

	pendingObjects.clear();
	if (buildings)
	{
		if (myBuildings.size() != 1)
		{
			showMain( UnicodeString( L"Select one of your buildings first, then press Ctrl+G." ) );
			return;
		}
		pendingObjects.push_back( myBuildings[0]->getID() );
	}
	else
	{
		if (myUnits.empty())
		{
			showMain( UnicodeString( L"Select your units first (click, or drag a box), then press Ctrl+G." ) );
			return;
		}
		for (size_t i = 0; i < myUnits.size(); ++i)
			pendingObjects.push_back( myUnits[i]->getID() );
	}

	Player *local = ThePlayerList->getLocalPlayer();
	std::vector<Int> allies;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer( i );
		if (AbramTeamActions::canTransferBetween( local, player ))
			allies.push_back( i );
	}
	if (allies.empty())
	{
		showMain( UnicodeString( L"You have no allies to give to." ) );
		return;
	}

	UnicodeString message;
	if (buildings)
		message = UnicodeString( L"Who gets the building?" );
	else
		message.format( L"Who gets the %d unit(s)?", (Int)pendingObjects.size() );
	showPlayerChoice( STATE_PICK_RECEIVER, UnicodeString( buildings ? L"GIVE BUILDING" : L"GIVE UNIT(S)" ), message, allies );
}

void startRequest()
{
	std::vector<Object *> myUnits, myBuildings, others;
	collectSelection( myUnits, myBuildings, others );

	if (others.empty())
	{
		showMain( UnicodeString( L"Select your ally's unit or building first, then press Ctrl+G." ) );
		return;
	}

	Player *owner = others[0]->getControllingPlayer();
	pendingObjects.clear();
	for (size_t i = 0; i < others.size(); ++i)
	{
		if (others[i]->getControllingPlayer() == owner)
			pendingObjects.push_back( others[i]->getID() );
	}

	if (!AbramTeamActions::canTransferBetween( ThePlayerList->getLocalPlayer(), owner ))
	{
		showMain( UnicodeString( L"You can only request from an ally." ) );
		return;
	}

	state = STATE_CONFIRM_REQUEST;
	requestOwnerIndex = owner->getPlayerIndex();

	UnicodeString message;
	message.format( L"Ask %ls for %d selected object(s)?", playerName( owner ).str(), (Int)pendingObjects.size() );
	UnicodeString choices[2] = { UnicodeString( L"Send Request" ), UnicodeString( L"Back" ) };
	AbramPopup::open( UnicodeString( L"REQUEST UNIT(S)/BUILDING" ), message, choices, 2, onChoice, nullptr );
}

void startControl()
{
	Player *local = ThePlayerList->getLocalPlayer();

	if (isControllingAI())
	{
		TheMessageStream->appendMessage( GameMessage::MSG_ABRAM_RELEASE_AI_CONTROL );
		closeMenu();
		return;
	}

	std::vector<Int> ais;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		if (AbramTeamActions::canTakeControl( local, ThePlayerList->getNthPlayer( i ) ))
			ais.push_back( i );
	}
	if (ais.empty())
	{
		showMain( UnicodeString( L"There is no AI player you can control here." ) );
		return;
	}

	showPlayerChoice( STATE_PICK_AI, UnicodeString( L"TAKE CONTROL OF AI" ),
		UnicodeString( L"Which AI do you want to control? Press Ctrl+G again to release it." ), ais );
}

#if RTS_ZEROHOUR
// Abram's Personal Test @feature the match's cheats: whether the local player's (or everyone's) is on
Bool isCheatOn( Int what )
{
	const Player *local = ThePlayerList->getLocalPlayer();
	const Int index = local != nullptr ? local->getPlayerIndex() : -1;
	switch (what)
	{
	case MAIN_FREEZE_AI: return AbramCheats::isAIFrozen();
	case MAIN_UNLOCK_POWERS: return AbramCheats::hasUnlockedPowers( index );
	case MAIN_UNLIMITED_POWER: return AbramCheats::hasUnlimitedPower( index );
	case MAIN_UNLIMITED_MONEY: return AbramCheats::hasUnlimitedMoney( index );
	default: return FALSE;
	}
}

void sendCheat( AbramCheat cheat, Bool on )
{
	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_ABRAM_CHEAT );
	msg->appendIntegerArgument( (Int)cheat );
	msg->appendIntegerArgument( on ? 1 : 0 );
}
#endif

UnicodeString mainChoiceText( Int what )
{
	switch (what)
	{
	case MAIN_CONTROL_AI: return UnicodeString( isControllingAI() ? L"Release Control of AI" : L"Take Control of AI" );
	case MAIN_GIVE_UNITS: return UnicodeString( L"Give Unit(s)" );
	case MAIN_GIVE_BUILDING: return UnicodeString( L"Give Building" );
	case MAIN_REQUEST: return UnicodeString( L"Request Unit(s)/Building" );
#if RTS_ZEROHOUR
	case MAIN_FREEZE_AI: return UnicodeString( isCheatOn( what ) ? L"Freeze AI: On" : L"Freeze AI: Off" );
	case MAIN_SPAWN: return UnicodeString( L"Spawn Menu" );
	case MAIN_UNLOCK_POWERS: return UnicodeString( isCheatOn( what ) ? L"Generals Powers: Unlocked" : L"Unlock Generals Powers" );
	case MAIN_UNLIMITED_POWER: return UnicodeString( isCheatOn( what ) ? L"Unlimited Power: On" : L"Unlimited Power: Off" );
	case MAIN_UNLIMITED_MONEY: return UnicodeString( isCheatOn( what ) ? L"Unlimited Money: On" : L"Unlimited Money: Off" );
#endif
	default: return UnicodeString( L"Close" );
	}
}

// The texts that follow the game (a cheat turned on comes back from the logic a frame or more later).
void refreshMain()
{
	for (size_t i = 0; i < mainChoices.size() && i < mainTexts.size(); ++i)
	{
		const UnicodeString text = mainChoiceText( mainChoices[i] );
		if (text.compare( mainTexts[i] ) != 0)
		{
			mainTexts[i] = text;
			AbramPopup::setChoiceText( (Int)i, text );
		}
#if RTS_ZEROHOUR
		if (mainChoices[i] == MAIN_UNLOCK_POWERS)
			AbramPopup::setChoiceEnabled( (Int)i, !isCheatOn( MAIN_UNLOCK_POWERS ) );
#endif
	}
}

void showMain( const UnicodeString &message )
{
	state = STATE_MAIN;
	mainChoices.clear();
	for (Int what = 0; what < MAIN_CHOICE_COUNT; ++what)
	{
#if RTS_ZEROHOUR
		if (what >= MAIN_FREEZE_AI && what <= MAIN_UNLIMITED_MONEY && !AbramCheats::isEnabled())
			continue;
#endif
		mainChoices.push_back( what );
	}

	UnicodeString choices[AbramPopup::MAX_CHOICES];
	mainTexts.clear();
	Int count = 0;
	for (size_t i = 0; i < mainChoices.size() && count < AbramPopup::MAX_CHOICES; ++i)
	{
		choices[count++] = mainChoiceText( mainChoices[i] );
		mainTexts.push_back( choices[count - 1] );
	}
	AbramPopup::open( UnicodeString( L"TEAM" ), message, choices, count, onChoice, nullptr );
	refreshMain();
}

void showOffer( const AbramTransferOffer *offer )
{
	state = STATE_OFFER;
	shownOfferId = offer->m_id;
	answeredOffers.insert( offer->m_id );

	Player *sender = ThePlayerList->getNthPlayer( offer->m_senderIndex );
	UnicodeString message;
	if (offer->m_type == ABRAM_TRANSFER_GIVE)
		message.format( L"%ls wants to give you %d object(s).", playerName( sender ).str(), (Int)offer->m_objects.size() );
	else
		message.format( L"%ls asks for %d of your object(s).", playerName( sender ).str(), (Int)offer->m_objects.size() );

	UnicodeString choices[2] = { UnicodeString( L"Accept" ), UnicodeString( L"Deny" ) };
	AbramPopup::open( UnicodeString( offer->m_type == ABRAM_TRANSFER_GIVE ? L"GIFT FROM ALLY" : L"REQUEST FROM ALLY" ),
		message, choices, 2, onChoice, nullptr );
}

void answerOffer( Bool accept )
{
	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_ABRAM_OFFER_RESPONSE );
	msg->appendIntegerArgument( shownOfferId );
	msg->appendBooleanArgument( accept );
	closeMenu();
}

void onChoice( Int choice, void *userData )
{
	switch (state)
	{
	case STATE_MAIN:
		switch (choice >= 0 && choice < (Int)mainChoices.size() ? mainChoices[choice] : (Int)MAIN_CLOSE)
		{
		case MAIN_CONTROL_AI: startControl(); break;
		case MAIN_GIVE_UNITS: startGive( FALSE ); break;
		case MAIN_GIVE_BUILDING: startGive( TRUE ); break;
		case MAIN_REQUEST: startRequest(); break;
#if RTS_ZEROHOUR
		// Abram's Personal Test @feature the cheats: a toggle keeps the menu open (its text follows the game)
		case MAIN_FREEZE_AI: sendCheat( ABRAM_CHEAT_FREEZE_AI, !isCheatOn( MAIN_FREEZE_AI ) ); break;
		case MAIN_UNLIMITED_POWER: sendCheat( ABRAM_CHEAT_UNLIMITED_POWER, !isCheatOn( MAIN_UNLIMITED_POWER ) ); break;
		case MAIN_UNLIMITED_MONEY: sendCheat( ABRAM_CHEAT_UNLIMITED_MONEY, !isCheatOn( MAIN_UNLIMITED_MONEY ) ); break;
		case MAIN_UNLOCK_POWERS:
			if (!isCheatOn( MAIN_UNLOCK_POWERS ))
				sendCheat( ABRAM_CHEAT_UNLOCK_POWERS, TRUE );
			break;
		case MAIN_SPAWN:
			closeMenu();
			AbramSpawnMenu::open();
			if (!AbramSpawnMenu::isOpen())
				showMain( UnicodeString( L"There is nothing to spawn." ) );
			break;
#endif
		default: closeMenu(); break;
		}
		break;

	case STATE_PICK_AI:
		if (choice >= 0 && choice < (Int)choicePlayers.size())
		{
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_ABRAM_TAKE_AI_CONTROL );
			msg->appendIntegerArgument( choicePlayers[choice] );
			closeMenu();
		}
		else
			showMain( UnicodeString::TheEmptyString );
		break;

	case STATE_PICK_RECEIVER:
		if (choice >= 0 && choice < (Int)choicePlayers.size())
		{
			sendTransfer( choicePlayers[choice], ABRAM_TRANSFER_GIVE, pendingObjects );
			closeMenu();
		}
		else
			showMain( UnicodeString::TheEmptyString );
		break;

	case STATE_CONFIRM_REQUEST:
		if (choice == 0 && requestOwnerIndex >= 0)
		{
			sendTransfer( requestOwnerIndex, ABRAM_TRANSFER_REQUEST, pendingObjects );
			closeMenu();
		}
		else
			showMain( UnicodeString::TheEmptyString );
		break;

	case STATE_OFFER:
		// Escape counts as Deny.
		answerOffer( choice == 0 );
		break;

	default:
		closeMenu();
		break;
	}
}

// The team menu is for players in a skirmish or network game, not for the campaign or replays.
Bool isMenuAvailable()
{
	if (TheGameLogic == nullptr || !TheGameLogic->isInGame() || TheGameLogic->isInShellGame())
		return false;
	if (TheRecorder != nullptr && TheRecorder->isPlaybackMode())
		return false;
	Player *local = ThePlayerList->getLocalPlayer();
	return local != nullptr && local->isPlayerActive() && TheGameInfo != nullptr;
}

} // namespace

void AbramTeamMenu::open()
{
	if (!isMenuAvailable())
		return;
	if (state == STATE_OFFER)
		return; // answer the waiting question first
	showMain( UnicodeString::TheEmptyString );
}

void AbramTeamMenu::update()
{
	if (state != STATE_CLOSED)
	{
		// The dialog was closed some other way (for example by leaving the game).
		if (!AbramPopup::isOpen())
			closeMenu();
		// A request we are showing may have been taken back by the game, for example after its objects died.
		else if (state == STATE_OFFER && AbramTeamActions::findPendingOffer( shownOfferId ) == nullptr)
			closeMenu();
		else if (state == STATE_MAIN)
			refreshMain();
		return;
	}

	if (!isMenuAvailable() || AbramPopup::isOpen())
		return;

	const Int localIndex = ThePlayerList->getLocalPlayer()->getPlayerIndex();
	for (Int i = 0; i < AbramTeamActions::getPendingOfferCount(); ++i)
	{
		const AbramTransferOffer *offer = AbramTeamActions::getPendingOffer( i );
		if (offer != nullptr && offer->m_deciderIndex == localIndex && answeredOffers.count( offer->m_id ) == 0)
		{
			showOffer( offer );
			return;
		}
	}
}

void AbramTeamMenu::reset()
{
	if (state != STATE_CLOSED)
		closeMenu();
	answeredOffers.clear();
}
