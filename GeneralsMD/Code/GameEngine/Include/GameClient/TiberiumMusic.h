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
// Tiberium Project @feature C&C4's dynamic music: its PathMusic "standard" state set (PM_Standard_Full),
// with its own conditions and the music its multiplayer events pick. Client only; it reads the game and
// never changes it.
//
//   Explore                     the start (C&C4 never comes back to it)
//   NearEnemyUnits              NearEnemyCount enemy units within NearEnemyRange of one of the player's
//   FirstFight                  the first engagement (units only) by or against the player
//   SmallFight, BigFight        a fight; big from BigFightEngagements engagements against the player in the
//                               window, back to small under MediumEngagements
//   Small/BigEnemyCrawlerDestroyed   an enemy Crawler killed by the player or an ally (KillWindow), for
//                               EnemyStingerTime; Small or Big by the fight it breaks into
//   Small/BigPlayerCrawlerDestroyed  one of the player's Crawlers killed by an enemy, for PlayerStingerTime
//   FightFinished               no engagement in the window; a fight again with FewEnemyCount enemy units
//                               within FewEnemyRange, or a big one
//   LikelyToWin                 the player's forces worth LikelyToWinRatio times the strongest enemy's
//                               (after LikelyToWinAfter; C&C4 counts its victory points, Zero Hour has none)
// A fight state changes only after MinimumStateTime in it (LikelyToWinStateTime to LikelyToWin). An
// engagement is a thing of the player's hit by an enemy, or an enemy's hit by the player, in the last
// EngagementWindow (one a thing).
//
// The music, as C&C4's events give it: the calm states' music follows the map's theme (MapTheme: words of
// the map's file name; SnowTheme for a snowy map that matches none; else DefaultTheme), the fights' a fight
// set, the other of two each match. A state's tracks play in turn. A fight that goes on (first fight to small
// fight, a stinger back to its fight) keeps its track playing; a small fight after the fight was over plays
// its From:FightFinished tracks. A track that ends is followed by the state's next.
//
//   TiberiumMusic
//     Enabled = Yes
//     Track = Explore USA Theme:Arctic C4M_Explore_GDI_Arctic        ; <state> <side or Any> [Theme:<theme>]
//     Track = SmallFight China Set:2 From:FightFinished C4M_...      ;   [Set:<1|2>] [From:<state>] <track> ...
//     MapTheme = Arctic Arctic_Feasel_1 C4BonechillBasin Snow Winter  ; <theme> <word> ...
//     SnowTheme = Arctic        DefaultTheme = Junktown
//     CheckInterval = 1000      NearEnemyRange = 600      NearEnemyCount = 6
//     FewEnemyRange = 400       FewEnemyCount = 3         EngagementWindow = 20000
//     BigFightEngagements = 6   MediumEngagements = 3     MinimumStateTime = 20000
//     LikelyToWinStateTime = 5000   KillWindow = 3000     EnemyStingerTime = 20000
//     PlayerStingerTime = 5000  LikelyToWinRatio = 2.5    LikelyToWinAfter = 300000
//   End
//
// The jukebox (the HUD's music panel) lists what the music can play now: the calm tracks of the theme, the
// fight set's tracks, the winning track. A track picked there plays at once; the music follows the game
// again at the next change, or, held, stays with the picked track's group. The theme can be changed from
// the map's.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "Common/AsciiString.h"
#include "Common/UnicodeString.h"

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
		SMALL_ENEMY_CRAWLER_DESTROYED,
		SMALL_PLAYER_CRAWLER_DESTROYED,
		BIG_ENEMY_CRAWLER_DESTROYED,
		BIG_PLAYER_CRAWLER_DESTROYED,
		FIGHT_FINISHED,
		LIKELY_TO_WIN,
		STATE_COUNT
	};

	/// the jukebox's groups
	enum Group
	{
		GROUP_CALM,
		GROUP_FIGHT,
		GROUP_WINNING,
		GROUP_COUNT
	};

	/// a track the jukebox lists
	struct Entry
	{
		AsciiString track;
		UnicodeString label;		///< "Fight over 2", "Small fight 1 (return)"
		Int group;
		Bool playing;
	};

	static void parse( INI *ini );		///< the TiberiumMusic block
	static void update();				///< once a client frame, in a game
	static void reset();				///< the game is over
	static Bool isActive();				///< it has the music of this game (the scripts' music waits)
	static const char *getStateName( Int state );

	// the jukebox
	static Int getState();							///< -1 before the game's first
	static Int getGroupOf( Int state );			///< the jukebox group a state's music is in
	static UnicodeString getStateLabel( Int state );
	static UnicodeString getGroupLabel( Int group );
	static AsciiString getTheme();					///< the theme heard: the one chosen in the jukebox, else the map's
	static Bool isThemeChosen();
	static Int getFightSet();
	static AsciiString getTrack();					///< the track playing (or last played)
	static UnicodeString getTrackLabel();
	static void getEntries( std::vector<Entry> &out );
	static void pick( const AsciiString &track );	///< plays it now
	static void next();								///< the next track of the group playing
	static void nextTheme();							///< the map's theme, then each other in turn
	static void setHeld( Bool held );				///< held: the music stays with the jukebox's choice
	static Bool isHeld();
	static void describe( char *buffer, Int size );	///< (the tests) the state, the theme, the set, the track, the counts
};
