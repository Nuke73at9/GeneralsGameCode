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

// Abram's Personal Test @feature Optional gameplay rules, chosen per game in the skirmish setup.
//
// Every rule is off unless the player ticks it, so the original game, its replays and mods behave
// exactly as designed. The chosen rules are part of the game options (GameInfo), which are stored in
// replays and skirmish save games, so a replay or a loaded game plays with the same rules.

#include "Lib/BaseType.h"

enum AbramGameRule
{
	ABRAM_RULE_LARGE_BUILD_QUEUE = 0x0001, ///< production buildings queue up to ABRAM_LARGE_BUILD_QUEUE_SIZE entries
	ABRAM_RULE_REVEAL_MAP        = 0x0002, ///< no shroud or fog of war for anyone, from the start of the game
	ABRAM_RULE_UNLOCK_ALL        = 0x0004, ///< units, buildings and generals powers ignore their prerequisites
};

enum
{
	ABRAM_LARGE_BUILD_QUEUE_SIZE = 99,

	// The rules value also holds two small settings above the on/off rules.
	ABRAM_RULE_FLAG_MASK          = 0x000000FF,
	ABRAM_CASH_MULTIPLIER_SHIFT   = 8,
	ABRAM_CASH_MULTIPLIER_MASK    = 0x00000F00,
	ABRAM_BUILD_SPEED_SHIFT       = 12,
	ABRAM_BUILD_SPEED_MASK        = 0x0000F000,
};

class AbramGameRules
{
public:
	// True when the rule is on for the game being played. Always false outside skirmish and replays.
	static Bool isEnabled( AbramGameRule rule );

	// Income (supplies, oil derricks, hackers and other automatic income) is multiplied by this.
	static UnsignedInt getCashMultiplier();
	// Units, buildings and upgrades are built this many times faster.
	static UnsignedInt getBuildSpeed();

	static UnsignedInt scaleIncome( UnsignedInt amount );
	static Int scaleBuildTime( Int buildFrames );

	// The choices offered for the two settings, and helpers to read and change them in a rules value.
	static Int getCashMultiplierChoiceCount();
	static UnsignedInt getCashMultiplierChoice( Int index );
	static Int getBuildSpeedChoiceCount();
	static UnsignedInt getBuildSpeedChoice( Int index );

	static Int getCashMultiplierIndex( UnsignedInt rules );
	static Int getBuildSpeedIndex( UnsignedInt rules );
	static UnsignedInt setCashMultiplierIndex( UnsignedInt rules, Int index );
	static UnsignedInt setBuildSpeedIndex( UnsignedInt rules, Int index );
};
