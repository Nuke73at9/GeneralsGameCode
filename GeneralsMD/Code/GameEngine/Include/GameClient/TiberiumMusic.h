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

// FILE: TiberiumMusic.h //////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's dynamic music (its PathMusic "standard" state set): the music follows
// the game as the local player sees it. Client only; it reads the game and never changes it.
//
//   Explore                 nothing has happened yet
//   NearEnemyUnits          enemy units close to the player's
//   FirstFight              the first time shots are fired
//   SmallFight, BigFight    a fight, by how many units are in it (BigFightUnits)
//   EnemyCrawlerDestroyed   an enemy Crawler destroyed (for StingerTime)
//   PlayerCrawlerDestroyed  one of the player's Crawlers destroyed (for StingerTime)
//   FightFinished           no fighting for FightOverDelay (for FightFinishedTime)
//   LikelyToWin             the player's forces worth LikelyToWinRatio times the strongest enemy's,
//                           after LikelyToWinAfter, while not fighting
//
// Each state plays its own tracks (Zero Hour music tracks, Data\INI\Music), the local player's side's
// (its side or base side: China, or USA for all of America's generals) when it has any, else any side's; a track that ends is followed by another of the state's. A new
// state fades the music over, after MinStateTime in the last (fights come in at once). On in
// skirmish and multiplayer games when the data turns it on; the maps' scripted music then waits.
//
//   TiberiumMusic
//     Enabled = Yes
//     Track = Explore Any C4M_Explore1 C4M_Explore2      ; <state> <side or Any> <track> ...
//     Track = BigFight China C4M_NodBigFight1   ; or USA, America, ...
//     CheckInterval = 1000      NearEnemyDistance = 500   EngagedWindow = 3000
//     BigFightUnits = 12        FightOverDelay = 10000    FightFinishedTime = 45000
//     StingerTime = 20000       LikelyToWinRatio = 2.5    LikelyToWinAfter = 300000
//     MinStateTime = 8000
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class INI;

class TiberiumMusic
{
public:
	enum State
	{
		EXPLORE,
		NEAR_ENEMY_UNITS,
		FIRST_FIGHT,
		SMALL_FIGHT,
		BIG_FIGHT,
		ENEMY_CRAWLER_DESTROYED,
		PLAYER_CRAWLER_DESTROYED,
		FIGHT_FINISHED,
		LIKELY_TO_WIN,
		STATE_COUNT
	};

	static void parse( INI *ini );		///< the TiberiumMusic block
	static void update();				///< once a client frame, in a game
	static void reset();				///< the game is over
	static Bool isActive();				///< it has the music of this game (the scripts' music waits)
	static const char *getStateName( Int state );
};
