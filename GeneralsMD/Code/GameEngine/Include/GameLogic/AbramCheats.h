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

// FILE: AbramCheats.h ////////////////////////////////////////////////////////////////////////////
// Abram's Personal Test @feature The match's cheats, in the team menu (Ctrl+G) when the match options'
// Enable Cheats is on:
//   Freeze AI              every computer player stops thinking, and its units and buildings stop doing
//                          anything (they hang where they are, aircraft too) until it is turned off
//   Unlimited Money        the player's money reads full and spending takes nothing from it; turned off,
//                          the player's own money is there again
//   Unlimited Power        the player's base never lacks power
//   Unlock Generals Powers every faction's generals powers for the player, usable from the command
//                          structure that has their buttons
//   Spawn Menu             a unit or a structure of any faction, for the player, where it is clicked
//                          (GameClient/AbramSpawnMenu.h)
//
// The menus only send the MSG_ABRAM_CHEAT commands. The changes are made here, in game logic, when the
// commands are processed, so every machine and a replay make them on the same frame; a command is
// ignored when the match has no cheats. The cheats' state is part of save games (CHUNK_AbramCheats).
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/BuildAssistant.h"
#include "Common/GameType.h"
#include "Common/Snapshot.h"
#include "Lib/BaseType.h"

class GameMessage;
class Player;
class ThingTemplate;

enum AbramCheat
{
	ABRAM_CHEAT_FREEZE_AI = 0,			///< (on) every computer player and its objects stop
	ABRAM_CHEAT_UNLIMITED_MONEY,		///< (on) the sender's money never runs out
	ABRAM_CHEAT_UNLIMITED_POWER,		///< (on) the sender's base never lacks power
	ABRAM_CHEAT_UNLOCK_POWERS,			///< every faction's generals powers for the sender
	ABRAM_CHEAT_COUNT
};

class AbramCheats : public Snapshot
{
public:
	enum { UNLIMITED_MONEY_AMOUNT = 999999 };	///< what an unlimited purse reads (when it has less)

	static Bool isEnabled();		///< the match has cheats (its Enable Cheats option)

	static void reset();				///< a new game: no cheat is on
	static void update();				///< once a logic frame, after the commands: keeps the frozen AI frozen
	static void processMessage( const GameMessage *msg );

	static Bool isAIFrozen();
	static Bool isFrozenPlayer( Player *player );		///< a computer player, while the AI is frozen
	static Bool hasUnlimitedMoney( Int playerIndex );
	static Bool hasUnlimitedPower( Int playerIndex );
	static Bool hasUnlockedPowers( Int playerIndex );

	/// a unit or a structure the spawn menu offers (the logic spawns nothing else)
	static Bool isSpawnable( const ThingTemplate *tmpl );
	/// whether tmpl may be spawned at pos: a structure where it may stand (not over others, on ground it can
	/// be built on), a unit anywhere on the map
	static LegalBuildCode checkSpawnLocation( const ThingTemplate *tmpl, const Coord3D *pos, Real angle );

	static Snapshot *getSnapshot();	///< for the save games

protected:
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;
};
