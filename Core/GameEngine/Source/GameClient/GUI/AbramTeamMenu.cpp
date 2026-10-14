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
	MAIN_CLOSE,
	MAIN_CHOICE_COUNT
};

MenuState state = STATE_CLOSED;

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

void showMain( const UnicodeString &message )
{
	state = STATE_MAIN;
	UnicodeString choices[MAIN_CHOICE_COUNT];
	choices[MAIN_CONTROL_AI] = UnicodeString( isControllingAI() ? L"Release Control of AI" : L"Take Control of AI" );
	choices[MAIN_GIVE_UNITS] = UnicodeString( L"Give Unit(s)" );
	choices[MAIN_GIVE_BUILDING] = UnicodeString( L"Give Building" );
	choices[MAIN_REQUEST] = UnicodeString( L"Request Unit(s)/Building" );
	choices[MAIN_CLOSE] = UnicodeString( L"Close" );
	AbramPopup::open( UnicodeString( L"TEAM" ), message, choices, MAIN_CHOICE_COUNT, onChoice, nullptr );
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
		switch (choice)
		{
		case MAIN_CONTROL_AI: startControl(); break;
		case MAIN_GIVE_UNITS: startGive( FALSE ); break;
		case MAIN_GIVE_BUILDING: startGive( TRUE ); break;
		case MAIN_REQUEST: startRequest(); break;
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
