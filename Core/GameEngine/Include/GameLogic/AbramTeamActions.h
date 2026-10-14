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

// Abram's Personal Test @feature Team actions: giving and requesting units and buildings between allies,
// and taking control of an AI player.
//
// All changes to the game happen here, in game logic, when the MSG_ABRAM_* commands are processed. Those
// commands are recorded in replays and sent to the other players like any other command, so every
// machine makes the same changes on the same frame. The team menu (GameClient/AbramTeamMenu.h) only
// sends the commands and shows the requests waiting for the local player.

#include "Common/GameType.h"
#include "Lib/BaseType.h"

#include <vector>

class GameMessage;
class Player;

enum AbramTransferType
{
	ABRAM_TRANSFER_GIVE = 0,     ///< the sender gives its objects to the other player
	ABRAM_TRANSFER_REQUEST = 1,  ///< the sender asks the other player for that player's objects
};

struct AbramTransferOffer
{
	Int m_id;
	Int m_senderIndex;     ///< player who sent the give or request
	Int m_deciderIndex;    ///< player who accepts or denies it
	AbramTransferType m_type;
	std::vector<ObjectID> m_objects;
};

class AbramTeamActions
{
public:
	enum { MAX_TRANSFER_OBJECTS = 200 }; ///< keeps a command within the message argument limit

	static void reset();
	static void processMessage( const GameMessage *msg );

	// True when the two players may give and request objects: different, both playing, allied both ways.
	static Bool canTransferBetween( const Player *a, const Player *b );

	// True when player may take control of ai: ai is an active computer player nobody controls yet.
	// Only offered without network play, because a network game identifies a player by its connection.
	static Bool canTakeControl( const Player *player, const Player *ai );

	// Index of the human player controlling the AI player, or -1.
	static Int getControllerOf( Int aiPlayerIndex );

	// Requests waiting for a decision. The pointers stay valid until the next command is processed.
	static Int getPendingOfferCount();
	static const AbramTransferOffer *getPendingOffer( Int index );
	static const AbramTransferOffer *findPendingOffer( Int offerId );
};
