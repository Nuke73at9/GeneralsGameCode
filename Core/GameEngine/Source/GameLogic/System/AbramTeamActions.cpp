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
#include "GameLogic/AbramTeamActions.h"

#include "Common/GameUtility.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Recorder.h"
#include "Common/Team.h"
#include "GameLogic/AI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameNetwork/GameInfo.h"
#include "GameNetwork/NetworkInterface.h"

namespace
{

std::vector<AbramTransferOffer> PendingOffers;
Int NextOfferId = 1;
// For each AI player, the index of the human player controlling it, or -1.
struct ControllerTable
{
	Int index[MAX_PLAYER_COUNT];
	ControllerTable() { clear(); }
	void clear() { for (Int i = 0; i < MAX_PLAYER_COUNT; ++i) index[i] = -1; }
};
ControllerTable AIControllers;

Bool isValidPlayerIndex( Int index )
{
	return index >= 0 && index < ThePlayerList->getPlayerCount() && index < MAX_PLAYER_COUNT;
}

Player *playerAt( Int index )
{
	return isValidPlayerIndex( index ) ? ThePlayerList->getNthPlayer( index ) : nullptr;
}

// An object that may change owner: alive, owned by owner and not a part of something else.
Bool isTransferable( const Object *obj, const Player *owner )
{
	if (obj == nullptr || owner == nullptr)
		return false;
	if (obj->isEffectivelyDead() || obj->isDestroyed())
		return false;
	if (obj->getControllingPlayer() != owner)
		return false;
	// Contained units change owner together with their transport or building.
	if (obj->getContainedBy() != nullptr)
		return false;
	return true;
}

void giveObjectTo( Object *obj, Player *newOwner );

void giveContainedObjects( Object *container, Player *newOwner, const Player *oldOwner )
{
	ContainModuleInterface *contain = container->getContain();
	if (contain == nullptr)
		return;

	// Copy first: changing owners must not disturb the list being walked.
	std::vector<Object *> passengers;
	const ContainedItemsList *items = contain->getContainedItemsList();
	if (items != nullptr)
	{
		for (ContainedItemsList::const_iterator it = items->begin(); it != items->end(); ++it)
			passengers.push_back( *it );
	}

	for (size_t i = 0; i < passengers.size(); ++i)
	{
		if (passengers[i] != nullptr && passengers[i]->getControllingPlayer() == oldOwner)
			giveObjectTo( passengers[i], newOwner );
	}
}

void giveObjectTo( Object *obj, Player *newOwner )
{
	Team *team = newOwner->getDefaultTeam();
	if (team == nullptr)
		return;

	const Player *oldOwner = obj->getControllingPlayer();

	// Forget the old owner's orders.
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	if (ai != nullptr)
		ai->aiIdle( CMD_FROM_AI );

	obj->setTeam( team );
	obj->updateUpgradeModules();

	giveContainedObjects( obj, newOwner, oldOwner );
}

void transferObjects( const std::vector<ObjectID> &ids, Player *from, Player *to )
{
	for (size_t i = 0; i < ids.size(); ++i)
	{
		Object *obj = TheGameLogic->findObjectByID( ids[i] );
		if (isTransferable( obj, from ))
			giveObjectTo( obj, to );
	}
}

Bool isLocalPlayer( const Player *player )
{
	return player != nullptr && player == ThePlayerList->getLocalPlayer();
}

// Only a live game changes which player this machine controls. A replay keeps watching as observer.
Bool mayChangeLocalPlayer()
{
	return TheRecorder == nullptr || !TheRecorder->isPlaybackMode();
}

void onTransferObjects( const GameMessage *msg, Player *sender )
{
	if (msg->getArgumentCount() < 2)
		return;

	Player *other = playerAt( msg->getArgument(0)->integer );
	const Int typeValue = msg->getArgument(1)->integer;
	if (other == nullptr || !AbramTeamActions::canTransferBetween( sender, other ))
		return;
	if (typeValue != ABRAM_TRANSFER_GIVE && typeValue != ABRAM_TRANSFER_REQUEST)
		return;

	const AbramTransferType type = (AbramTransferType)typeValue;
	Player *owner = (type == ABRAM_TRANSFER_GIVE) ? sender : other;
	Player *receiver = (type == ABRAM_TRANSFER_GIVE) ? other : sender;

	std::vector<ObjectID> ids;
	for (Int i = 2; i < msg->getArgumentCount() && (Int)ids.size() < AbramTeamActions::MAX_TRANSFER_OBJECTS; ++i)
	{
		const ObjectID id = msg->getArgument(i)->objectID;
		if (isTransferable( TheGameLogic->findObjectByID( id ), owner ))
			ids.push_back( id );
	}
	if (ids.empty())
		return;

	// The computer always agrees. A human player decides in the team menu.
	if (other->getPlayerType() == PLAYER_COMPUTER)
	{
		transferObjects( ids, owner, receiver );
		return;
	}

	AbramTransferOffer offer;
	offer.m_id = NextOfferId++;
	offer.m_senderIndex = sender->getPlayerIndex();
	offer.m_deciderIndex = other->getPlayerIndex();
	offer.m_type = type;
	offer.m_objects = ids;
	PendingOffers.push_back( offer );
}

void onOfferResponse( const GameMessage *msg, Player *decider )
{
	if (msg->getArgumentCount() < 2)
		return;

	const Int offerId = msg->getArgument(0)->integer;
	const Bool accept = msg->getArgument(1)->boolean;

	for (std::vector<AbramTransferOffer>::iterator it = PendingOffers.begin(); it != PendingOffers.end(); ++it)
	{
		if (it->m_id != offerId)
			continue;
		if (it->m_deciderIndex != decider->getPlayerIndex())
			return; // only the asked player decides

		const AbramTransferOffer offer = *it;
		PendingOffers.erase( it );

		Player *sender = playerAt( offer.m_senderIndex );
		if (!accept || sender == nullptr || !AbramTeamActions::canTransferBetween( sender, decider ))
			return;

		if (offer.m_type == ABRAM_TRANSFER_GIVE)
			transferObjects( offer.m_objects, sender, decider );
		else
			transferObjects( offer.m_objects, decider, sender );
		return;
	}
}

void onTakeAIControl( const GameMessage *msg, Player *player )
{
	if (msg->getArgumentCount() < 1)
		return;

	Player *ai = playerAt( msg->getArgument(0)->integer );
	if (!AbramTeamActions::canTakeControl( player, ai ))
		return;

	AIControllers.index[ai->getPlayerIndex()] = player->getPlayerIndex();

	// The AI stops while the player controls it. Commands for it now come in its own name.
	ai->setPlayerType( PLAYER_HUMAN, FALSE );

	if (isLocalPlayer( player ) && mayChangeLocalPlayer())
		rts::changeLocalPlayer( ai );
}

void onReleaseAIControl( Player *ai )
{
	const Int aiIndex = ai->getPlayerIndex();
	if (!isValidPlayerIndex( aiIndex ) || AIControllers.index[aiIndex] < 0)
		return;

	Player *controller = playerAt( AIControllers.index[aiIndex] );
	AIControllers.index[aiIndex] = -1;

	// A new AI takes over again, with the player's difficulty.
	ai->setPlayerType( PLAYER_COMPUTER, TRUE );

	if (controller != nullptr && isLocalPlayer( ai ) && mayChangeLocalPlayer())
		rts::changeLocalPlayer( controller );
}

} // namespace

void AbramTeamActions::reset()
{
	PendingOffers.clear();
	NextOfferId = 1;
	AIControllers.clear();
}

void AbramTeamActions::processMessage( const GameMessage *msg )
{
	Player *player = playerAt( msg->getPlayerIndex() );
	if (player == nullptr)
		return;

	switch (msg->getType())
	{
	case GameMessage::MSG_ABRAM_TRANSFER_OBJECTS:
		onTransferObjects( msg, player );
		break;
	case GameMessage::MSG_ABRAM_OFFER_RESPONSE:
		onOfferResponse( msg, player );
		break;
	case GameMessage::MSG_ABRAM_TAKE_AI_CONTROL:
		onTakeAIControl( msg, player );
		break;
	case GameMessage::MSG_ABRAM_RELEASE_AI_CONTROL:
		onReleaseAIControl( player );
		break;
	default:
		break;
	}
}

Bool AbramTeamActions::canTransferBetween( const Player *a, const Player *b )
{
	if (a == nullptr || b == nullptr || a == b)
		return false;
	if (!a->isPlayerActive() || !b->isPlayerActive())
		return false;
	if (a == ThePlayerList->getNeutralPlayer() || b == ThePlayerList->getNeutralPlayer())
		return false;
	const Team *aTeam = a->getDefaultTeam();
	const Team *bTeam = b->getDefaultTeam();
	if (aTeam == nullptr || bTeam == nullptr)
		return false;
	return a->getRelationship( bTeam ) == ALLIES && b->getRelationship( aTeam ) == ALLIES;
}

Bool AbramTeamActions::canTakeControl( const Player *player, const Player *ai )
{
	if (player == nullptr || ai == nullptr || player == ai)
		return false;
	if (TheNetwork != nullptr)
		return false;
	if (player->getPlayerType() != PLAYER_HUMAN || ai->getPlayerType() != PLAYER_COMPUTER)
		return false;
	if (!ai->isPlayerActive() || ai == ThePlayerList->getNeutralPlayer())
		return false;
	const Int aiIndex = ai->getPlayerIndex();
	return isValidPlayerIndex( aiIndex ) && AIControllers.index[aiIndex] < 0;
}

Int AbramTeamActions::getControllerOf( Int aiPlayerIndex )
{
	return isValidPlayerIndex( aiPlayerIndex ) ? AIControllers.index[aiPlayerIndex] : -1;
}

Int AbramTeamActions::getPendingOfferCount()
{
	return (Int)PendingOffers.size();
}

const AbramTransferOffer *AbramTeamActions::getPendingOffer( Int index )
{
	return (index >= 0 && index < (Int)PendingOffers.size()) ? &PendingOffers[index] : nullptr;
}

const AbramTransferOffer *AbramTeamActions::findPendingOffer( Int offerId )
{
	for (size_t i = 0; i < PendingOffers.size(); ++i)
	{
		if (PendingOffers[i].m_id == offerId)
			return &PendingOffers[i];
	}
	return nullptr;
}
