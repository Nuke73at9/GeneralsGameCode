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
	ABRAM_RULE_CHEATS            = 0x0008, ///< the team menu (Ctrl+G) has the cheats (Zero Hour: GameLogic/AbramCheats.h)
};

enum
{
	ABRAM_LARGE_BUILD_QUEUE_SIZE = 99,

	// The rules value also holds two small settings above the on/off rules.
	ABRAM_RULE_FLAG_MASK          = 0x0000000F,
	// Tiberium Project @feature C&C4's Lethality (Zero Hour): its armour percents by the match's lethality
	ABRAM_LETHALITY_SHIFT         = 4,
	ABRAM_LETHALITY_MASK          = 0x00000030,
	ABRAM_CASH_MULTIPLIER_SHIFT   = 8,
	ABRAM_CASH_MULTIPLIER_MASK    = 0x00000F00,
	ABRAM_BUILD_SPEED_SHIFT       = 12,
	ABRAM_BUILD_SPEED_MASK        = 0x0000F000,
	// Tiberium Project @feature C&C4's zones (Zero Hour): off, pre-zones, expansion zones, free zones
	ABRAM_ZONES_SHIFT             = 16,
	ABRAM_ZONES_MASK              = 0x00030000,
	// Tiberium Project @feature the army cap (C&C4's command points): its mode, and a custom cap's points per player
	ABRAM_ARMY_CAP_SHIFT          = 18,
	ABRAM_ARMY_CAP_MASK           = 0x000C0000,
	ABRAM_ARMY_POINTS_SHIFT       = 20,
	ABRAM_ARMY_POINTS_MAX         = 4095,	///< the most a custom cap may be (its 12 bits)
	ABRAM_ARMY_POINTS_DEFAULT     = 250,	///< a custom cap's points until the host sets them
};

// Tiberium Project @feature a custom army cap's points, the rules' top 12 bits (outside the enum: above an Int's range)
static const UnsignedInt ABRAM_ARMY_POINTS_MASK = 0xFFF00000u;

// Tiberium Project @feature how a skirmish's bases grow (C&C4's zones; TiberiumZones in Zero Hour)
enum AbramZoneMode
{
	ABRAM_ZONES_OFF,          ///< Zero Hour's way: structures anywhere, no zones
	ABRAM_ZONES_PRE,          ///< structures only in the player's own (start) zone; Crawlers make units anywhere
	ABRAM_ZONES_EXPANSION,    ///< the start zones, and more taken (their node, an outpost, a Crawler deployed)
	ABRAM_ZONES_FREE,         ///< structures anywhere (as Zero Hour), the zones' taking as expansion; Crawlers arrive in zones
	ABRAM_ZONE_MODE_COUNT
};

// Tiberium Project @feature C&C4's Lethality, as the rules hold it (Medium, C&C4's default, is 0)
enum AbramLethality
{
	ABRAM_LETHALITY_MEDIUM,
	ABRAM_LETHALITY_LOW,
	ABRAM_LETHALITY_HIGH,
	ABRAM_LETHALITY_VERY_HIGH,
	ABRAM_LETHALITY_COUNT
};

// Tiberium Project @feature the army cap: how each player's command point cap is set at the match start
enum AbramArmyCapMode
{
	ABRAM_ARMY_CAP_CLASSIC,     ///< C&C4's: a team's total by its size, shared out (the players' PlayerTemplate TeamCommandPoints)
	ABRAM_ARMY_CAP_CUSTOM,      ///< the host's points for every player, whatever its faction or team
	ABRAM_ARMY_CAP_UNLIMITED,   ///< no cap; every other build rule as before
	ABRAM_ARMY_CAP_MODE_COUNT
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

	// Tiberium Project @feature the zones' mode of the game being played (off outside skirmish and replays)
	static AbramZoneMode getZoneMode();
	static AbramZoneMode getZoneModeOf( UnsignedInt rules );
	static UnsignedInt setZoneMode( UnsignedInt rules, AbramZoneMode mode );

	// Tiberium Project @feature the army cap of the game being played (Classic outside skirmish, network games and
	// replays), and a custom cap's points per player (1 to ABRAM_ARMY_POINTS_MAX)
	static AbramArmyCapMode getArmyCapMode();
	static AbramArmyCapMode getArmyCapModeOf( UnsignedInt rules );
	static UnsignedInt setArmyCapMode( UnsignedInt rules, AbramArmyCapMode mode );
	static Int getArmyCapPoints();
	// Tiberium Project @feature C&C4's Lethality of the game being played (Medium outside skirmish, network games and
	// replays), and its level from the lowest: 0 Low, 1 Medium, 2 High, 3 Very High (the armours' LethalityArmor order)
	static AbramLethality getLethality();
	static AbramLethality getLethalityOf( UnsignedInt rules );
	static UnsignedInt setLethality( UnsignedInt rules, AbramLethality lethality );
	static Int getLethalityLevel();
	static Int levelOfLethality( AbramLethality lethality );
	static AbramLethality lethalityOfLevel( Int level );
	static Int getArmyCapPointsOf( UnsignedInt rules );
	static UnsignedInt setArmyCapPoints( UnsignedInt rules, Int points );

	static Int getCashMultiplierIndex( UnsignedInt rules );
	static Int getBuildSpeedIndex( UnsignedInt rules );
	static UnsignedInt setCashMultiplierIndex( UnsignedInt rules, Int index );
	static UnsignedInt setBuildSpeedIndex( UnsignedInt rules, Int index );
};
