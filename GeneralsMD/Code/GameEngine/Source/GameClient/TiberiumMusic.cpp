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

// FILE: TiberiumMusic.cpp ////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's dynamic music (see TiberiumMusic.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/TiberiumMusic.h"

#include <algorithm>
#include <ctype.h>
#include <string.h>
#include <vector>

#include "Common/AudioAffect.h"
#include "Common/AudioEventRTS.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/FramePacer.h"
#include "Common/GameAudio.h"
#include "Common/GameType.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingTemplate.h"
#include "Common/TiberiumTest.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/BodyModule.h"

namespace
{
	const char *const STATE_NAMES[TiberiumMusic::STATE_COUNT] =
	{
		"Explore", "NearEnemyUnits", "FirstFight", "SmallFight", "BigFight",
		"SmallEnemyCrawlerDestroyed", "SmallPlayerCrawlerDestroyed", "BigEnemyCrawlerDestroyed", "BigPlayerCrawlerDestroyed",
		"FightFinished", "LikelyToWin",
	};
	/// what the jukebox calls them (no ampersands: the game's fonts)
	const wchar_t *const STATE_LABELS[TiberiumMusic::STATE_COUNT] =
	{
		L"Explore", L"Enemies near", L"First fight", L"Small fight", L"Big fight",
		L"Enemy Crawler destroyed", L"Crawler lost", L"Enemy Crawler destroyed", L"Crawler lost",
		L"Fight over", L"Winning",
	};

	/// the music's families (C&C4's map keeps one in its variable 5: a fight that goes on keeps its music)
	enum Family
	{
		FAMILY_NONE = 0,
		FAMILY_SMALL = 1,			///< the first fight and the small fights
		FAMILY_BIG = 2,
		FAMILY_FINISHED = 3,
		FAMILY_WINNING = 5
	};

	struct TrackList
	{
		Int state;
		AsciiString side;					///< a player side ("USA"), or empty for any
		AsciiString theme;				///< or empty for any
		Int set;									///< 1 or 2, or 0 for any
		Int from;									///< the state it is for coming back from, or -1
		std::vector<AsciiString> tracks;
	};

	struct MapTheme
	{
		AsciiString theme;
		std::vector<AsciiString> words;	///< lower case, letters and digits only
	};

	/// the TiberiumMusic block (durations in logic frames, as the INI parser gives them: msOf() makes the
	/// state times the music clock's milliseconds; the windows stay in the game's own frames)
	struct Settings
	{
		Bool enabled;
		UnsignedInt checkInterval;
		Real nearEnemyRange;
		Int nearEnemyCount;
		Real fewEnemyRange;
		Int fewEnemyCount;
		UnsignedInt engagementWindow;
		Int bigFightEngagements;
		Int mediumEngagements;
		UnsignedInt minimumStateTime;
		UnsignedInt likelyToWinStateTime;
		UnsignedInt killWindow;
		UnsignedInt enemyStingerTime;
		UnsignedInt playerStingerTime;
		Real likelyToWinRatio;
		UnsignedInt likelyToWinAfter;
		AsciiString snowTheme;
		AsciiString defaultTheme;
		std::vector<TrackList> lists;
		std::vector<MapTheme> mapThemes;
		/// the first director's fields (this file before C&C4's own rules): read, no longer used
		UnsignedInt superseded;
		Real supersededReal;

		Settings()
		{
			enabled = FALSE;
			checkInterval = LOGICFRAMES_PER_SECOND;
			nearEnemyRange = 600.0f;
			nearEnemyCount = 6;
			fewEnemyRange = 400.0f;
			fewEnemyCount = 3;
			engagementWindow = LOGICFRAMES_PER_SECOND * 20;
			bigFightEngagements = 6;
			mediumEngagements = 3;
			minimumStateTime = LOGICFRAMES_PER_SECOND * 20;
			likelyToWinStateTime = LOGICFRAMES_PER_SECOND * 5;
			killWindow = LOGICFRAMES_PER_SECOND * 3;
			enemyStingerTime = LOGICFRAMES_PER_SECOND * 20;
			playerStingerTime = LOGICFRAMES_PER_SECOND * 5;
			likelyToWinRatio = 2.5f;
			likelyToWinAfter = LOGICFRAMES_PER_SECOND * 300;
			superseded = 0;
			supersededReal = 0.0f;
		}
	};
	Settings s_settings;

	/// Tiberium Project @bugfix the music's own clock: real milliseconds of the game being played (it stands
	/// still while paused). The logic's frames run at the game speed, so a faster game used to rush the
	/// music from state to state, cutting tracks short; the music now keeps time as heard.
	double s_clockMs = 0.0;
	UnsignedInt msOf( UnsignedInt frames ) { return (UnsignedInt)( frames * MSEC_PER_LOGICFRAME_REAL ); }

	/// an engagement: a thing hit, and by whom (the last of its hits, in the game's frames)
	struct Engagement
	{
		ObjectID victim;
		UnsignedInt frame;
		Bool againstMe;				///< the player's, hit by an enemy
		Bool structure;
	};

	/// the game's music as it goes (times on the music clock)
	Int s_state = -1;
	UnsignedInt s_stateSince = 0;
	Int s_returnState = -1;						///< a stinger's fight
	Int s_family = FAMILY_NONE;
	UnsignedInt s_lastCheck = 0;
	UnsignedInt s_lastPlay = 0;
	AsciiString s_track;
	Int s_trackState = -1;						///< the state the track playing is from
	AudioHandle s_handle = 0;					///< the track playing (to know when it ends)
	std::vector<Engagement> s_engagements;
	std::vector<ObjectID> s_crawlers;			///< the players' Crawlers seen alive
	Int s_enemyCrawlerKilled = -1;				///< the game's frame of the last kill of each kind, or -1
	Int s_playerCrawlerKilled = -1;
	std::vector<Int> s_turn;							///< each list's next track
	AsciiString s_mapTheme;
	AsciiString s_chosenTheme;					///< the jukebox's, or empty
	Int s_set = 1;
	UnsignedInt s_matches = 0;					///< the games started (the fight set alternates)
	Bool s_held = FALSE;
	Int s_heldGroup = -1;

	//-----------------------------------------------------------------------------------------------
	Int stateByName( const char *name )
	{
		for( Int i = 0; i < TiberiumMusic::STATE_COUNT; ++i )
			if( stricmp( name, STATE_NAMES[i] ) == 0 )
				return i;
		// the first director's names: its stingers had no small and big
		if( stricmp( name, "EnemyCrawlerDestroyed" ) == 0 )
			return TiberiumMusic::SMALL_ENEMY_CRAWLER_DESTROYED;
		if( stricmp( name, "PlayerCrawlerDestroyed" ) == 0 )
			return TiberiumMusic::SMALL_PLAYER_CRAWLER_DESTROYED;
		return -1;
	}

	//-----------------------------------------------------------------------------------------------
	/** a word as the maps' names are matched: lower case, letters and digits only */
	AsciiString plainWord( const char *text )
	{
		AsciiString out;
		for( const char *c = text; *c != '\0'; ++c )
			if( isalnum( (unsigned char)*c ) )
				out.concat( (char)tolower( (unsigned char)*c ) );
		return out;
	}

	//-----------------------------------------------------------------------------------------------
	void parseTrack( INI *ini, void *, void *, const void * )
	{
		// Track = <state> <side or Any> [Theme:<theme>] [Set:<n>] [From:<state>] <track> <track> ...
		TrackList list;
		list.state = stateByName( ini->getNextToken() );
		if( list.state < 0 )
			throw INI_INVALID_DATA;
		const char *side = ini->getNextToken();
		if( stricmp( side, "Any" ) != 0 )
			list.side = side;
		list.set = 0;
		list.from = -1;
		for( const char *token = ini->getNextTokenOrNull(); token != nullptr; token = ini->getNextTokenOrNull() )
		{
			if( _strnicmp( token, "Theme:", 6 ) == 0 )
				list.theme = token + 6;
			else if( _strnicmp( token, "Set:", 4 ) == 0 )
				list.set = atoi( token + 4 );
			else if( _strnicmp( token, "From:", 5 ) == 0 )
			{
				list.from = stateByName( token + 5 );
				if( list.from < 0 )
					throw INI_INVALID_DATA;
			}
			else
				list.tracks.push_back( AsciiString( token ) );
		}
		s_settings.lists.push_back( list );
	}

	//-----------------------------------------------------------------------------------------------
	void parseMapTheme( INI *ini, void *, void *, const void * )
	{
		// MapTheme = <theme> <word> <word> ...
		MapTheme t;
		t.theme = ini->getNextToken();
		for( const char *token = ini->getNextTokenOrNull(); token != nullptr; token = ini->getNextTokenOrNull() )
		{
			AsciiString word = plainWord( token );
			if( word.isNotEmpty() )
				t.words.push_back( word );
		}
		s_settings.mapThemes.push_back( t );
	}

	//-----------------------------------------------------------------------------------------------
	/** the map's theme: the first MapTheme with a word that is one or more of the words of the map's file
		name run together; else SnowTheme on a snowy map; else DefaultTheme */
	AsciiString themeOfMap()
	{
		const char *path = TheGlobalData != nullptr ? TheGlobalData->m_mapName.str() : "";
		const char *base = path;
		for( const char *c = path; *c != '\0'; ++c )
			if( *c == '\\' || *c == '/' )
				base = c + 1;
		std::vector<AsciiString> words;
		AsciiString word;
		for( const char *c = base; ; ++c )
		{
			if( *c == '.' && strchr( c + 1, '.' ) == nullptr )
				break;		// the extension
			if( *c != '\0' && isalnum( (unsigned char)*c ) )
				word.concat( (char)tolower( (unsigned char)*c ) );
			else if( word.isNotEmpty() )
			{
				words.push_back( word );
				word.clear();
			}
			if( *c == '\0' )
				break;
		}
		if( word.isNotEmpty() )
			words.push_back( word );
		for( size_t t = 0; t < s_settings.mapThemes.size(); ++t )
		{
			const MapTheme &mt = s_settings.mapThemes[t];
			for( size_t w = 0; w < mt.words.size(); ++w )
			{
				for( size_t i = 0; i < words.size(); ++i )
				{
					AsciiString run;
					for( size_t j = i; j < words.size() && run.getLength() < mt.words[w].getLength(); ++j )
					{
						run.concat( words[j] );
						if( run == mt.words[w] )
							return mt.theme;
					}
				}
			}
		}
		if( TheGlobalData != nullptr && TheGlobalData->m_weather == WEATHER_SNOWY && s_settings.snowTheme.isNotEmpty() )
			return s_settings.snowTheme;
		return s_settings.defaultTheme;
	}

	AsciiString theme()
	{
		return s_chosenTheme.isNotEmpty() ? s_chosenTheme : s_mapTheme;
	}

	//-----------------------------------------------------------------------------------------------
	/** the list the local player hears in a state: its side's before any side's, the theme's before the
		themeless, the match's fight set's before the setless; for coming back from FightFinished, a list
		for that first. -1 when the state has none */
	Int listFor( Int state, Bool fromFinished )
	{
		const Player *me = ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
		const AsciiString side = me != nullptr ? me->getSide() : AsciiString::TheEmptyString;
		const AsciiString baseSide = me != nullptr ? me->getBaseSide() : AsciiString::TheEmptyString;
		const AsciiString heard = theme();
		Int best = -1, bestScore = -1;
		for( size_t i = 0; i < s_settings.lists.size(); ++i )
		{
			const TrackList &list = s_settings.lists[i];
			if( list.state != state || list.tracks.empty() )
				continue;
			const Bool sideMatch = !list.side.isEmpty() && ( list.side.compareNoCase( side ) == 0 || list.side.compareNoCase( baseSide ) == 0 );
			if( !list.side.isEmpty() && !sideMatch )
				continue;
			if( !list.theme.isEmpty() && list.theme.compareNoCase( heard ) != 0 )
				continue;
			if( list.set != 0 && list.set != s_set )
				continue;
			if( list.from >= 0 && !( fromFinished && list.from == TiberiumMusic::FIGHT_FINISHED ) )
				continue;
			const Int score = ( list.from >= 0 ? 8 : 0 ) + ( sideMatch ? 4 : 0 ) + ( list.theme.isEmpty() ? 0 : 2 ) + ( list.set != 0 ? 1 : 0 );
			if( score > bestScore )
			{
				best = (Int)i;
				bestScore = score;
			}
		}
		return best;
	}

	/// a state's list, or the one it stands in for (a big stinger: the small one's)
	Int listOrStandIn( Int state, Bool fromFinished )
	{
		Int list = listFor( state, fromFinished );
		if( list < 0 && state == TiberiumMusic::BIG_ENEMY_CRAWLER_DESTROYED )
			list = listFor( TiberiumMusic::SMALL_ENEMY_CRAWLER_DESTROYED, fromFinished );
		if( list < 0 && state == TiberiumMusic::BIG_PLAYER_CRAWLER_DESTROYED )
			list = listFor( TiberiumMusic::SMALL_PLAYER_CRAWLER_DESTROYED, fromFinished );
		return list;
	}

	Bool known( const AsciiString &track )
	{
		return TheAudio != nullptr && TheAudio->findAudioEventInfo( track ) != nullptr;
	}

	Bool trackPlaying()
	{
		return TheAudio != nullptr && s_handle != 0 && TheAudio->isCurrentlyPlaying( s_handle );
	}

	//-----------------------------------------------------------------------------------------------
	void playTrack( const AsciiString &name, Int fromState )
	{
		if( TheAudio == nullptr )
			return;
		TheAudio->removeAudioEvent( AHSV_StopTheMusicFade );
		AudioEventRTS event( name );
		event.setShouldFade( TRUE );
		if( ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
			event.setPlayerIndex( ThePlayerList->getLocalPlayer()->getPlayerIndex() );
		s_handle = TheAudio->addAudioEvent( &event );
		s_track = name;
		s_trackState = fromState;
		s_lastPlay = (UnsignedInt)s_clockMs;
		if( TiberiumTest::isActive() )
			TiberiumTest::log( "frame %u: music: %s plays %s (theme %s, set %d)", TheGameLogic->getFrame(),
				fromState >= 0 ? STATE_NAMES[fromState] : "jukebox", name.str(), theme().str(), (int)s_set );
	}

	/// the next track of a list, in turn (C&C4's events count through their pieces), one the game knows
	Bool playNextOf( Int list, Int fromState )
	{
		if( list < 0 )
			return FALSE;
		if( s_turn.size() < s_settings.lists.size() )
			s_turn.resize( s_settings.lists.size(), 0 );
		const std::vector<AsciiString> &tracks = s_settings.lists[list].tracks;
		for( size_t tries = 0; tries < tracks.size(); ++tries )
		{
			const AsciiString &name = tracks[ s_turn[list] % tracks.size() ];
			s_turn[list] = (Int)( ( s_turn[list] + 1 ) % tracks.size() );
			if( known( name ) )
			{
				playTrack( name, fromState );
				return TRUE;
			}
		}
		return FALSE;
	}

	//-----------------------------------------------------------------------------------------------
	/** a state's music as C&C4's multiplayer event for it gives it */
	void musicFor( Int state )
	{
		if( s_held )
			return;
		switch( state )
		{
			case TiberiumMusic::SMALL_FIGHT:
				// (a first or small fight goes on: its music too)
				if( s_family != FAMILY_SMALL )
					playNextOf( listFor( state, s_family == FAMILY_FINISHED ), state );
				s_family = FAMILY_SMALL;
				return;
			case TiberiumMusic::BIG_FIGHT:
				if( s_family != FAMILY_BIG )
					playNextOf( listFor( state, FALSE ), state );
				s_family = FAMILY_BIG;
				return;
			case TiberiumMusic::FIRST_FIGHT:
				playNextOf( listFor( state, FALSE ), state );
				s_family = FAMILY_SMALL;
				return;
			case TiberiumMusic::FIGHT_FINISHED:
				playNextOf( listFor( state, FALSE ), state );
				s_family = FAMILY_FINISHED;
				return;
			case TiberiumMusic::LIKELY_TO_WIN:
				if( playNextOf( listFor( state, FALSE ), state ) )
					s_family = FAMILY_WINNING;
				return;
			case TiberiumMusic::EXPLORE:
				playNextOf( listFor( state, FALSE ), state );
				s_family = FAMILY_NONE;
				return;
			default:
				// the stingers (they go on into their fight's music: the family stays), the enemies near (C&C4's
				// multiplayer event has no music: the exploring goes on, unless the data gives it some)
				playNextOf( listOrStandIn( state, FALSE ), state );
				return;
		}
	}

	void enter( Int state, UnsignedInt now )
	{
		if( TiberiumTest::isActive() )
			TiberiumTest::log( "frame %u: music: %s -> %s", TheGameLogic->getFrame(), s_state >= 0 ? STATE_NAMES[s_state] : "(start)",
				STATE_NAMES[state] );
		s_state = state;
		s_stateSince = now;
		musicFor( state );
	}

	Bool isStinger( Int state )
	{
		return state >= TiberiumMusic::SMALL_ENEMY_CRAWLER_DESTROYED && state <= TiberiumMusic::BIG_PLAYER_CRAWLER_DESTROYED;
	}

	/// a real player's (not the neutral one's, not a civilian's)
	Bool isPlayers( const Player *player )
	{
		return player != nullptr && player != ThePlayerList->getNeutralPlayer()
			&& ( player->getPlayerType() == PLAYER_HUMAN || player->getPlayerType() == PLAYER_COMPUTER );
	}

	/// one of C&C4's Crawlers (its CRAWLER kind is a name here: GDICrawlerA, NODCrawlerBStructure, ...)
	Bool isCrawler( const Object *obj )
	{
		const ThingTemplate *t = obj->getTemplate();
		return strstr( t->getName().str(), "Crawler" ) != nullptr && t->isKindOf( KINDOF_SELECTABLE ) && !t->isKindOf( KINDOF_PROJECTILE );
	}

	/// who hit it last: its source's player, or the source's player mask's when the source is gone
	Player *lastHitBy( const Object *obj )
	{
		const BodyModuleInterface *body = obj->getBodyModule();
		const DamageInfo *info = body != nullptr ? body->getLastDamageInfo() : nullptr;
		if( info == nullptr )
			return nullptr;
		const Object *source = TheGameLogic->findObjectByID( info->in.m_sourceID );
		if( source != nullptr )
			return source->getControllingPlayer();
		return info->in.m_sourcePlayerMask != 0 ? ThePlayerList->getPlayerFromMask( info->in.m_sourcePlayerMask ) : nullptr;
	}

	Bool isEnemy( const Player *me, const Player *other )
	{
		return other != nullptr && other != me && me->getRelationship( other->getDefaultTeam() ) == ENEMIES;
	}

	//-----------------------------------------------------------------------------------------------
	/** a Crawler killed: an enemy's by the player or an ally (C&C4's COUNT_ONLY_KILLS_BY_LOCALPLAYER_AND_ALLIES),
		one of the player's by an enemy (COUNT_ONLY_KILLS_AGAINST_LOCALPLAYER). Watched every frame: a dead
		Crawler may be gone by the next check */
	void watchCrawlers( Player *me )
	{
		const Int frame = (Int)TheGameLogic->getFrame();
		for( size_t i = 0; i < s_crawlers.size(); )
		{
			const Object *obj = TheGameLogic->findObjectByID( s_crawlers[i] );
			if( obj == nullptr )
			{
				// gone without being seen dead: packed up, redeployed, sold
				s_crawlers.erase( s_crawlers.begin() + i );
				continue;
			}
			if( obj->isEffectivelyDead() )
			{
				Player *owner = obj->getControllingPlayer();
				Player *killer = lastHitBy( obj );
				if( owner == me && isEnemy( me, killer ) )
					s_playerCrawlerKilled = frame;
				else if( isEnemy( me, owner ) && killer != nullptr && ( killer == me || me->getRelationship( killer->getDefaultTeam() ) == ALLIES ) )
					s_enemyCrawlerKilled = frame;
				if( TiberiumTest::isActive() )
					TiberiumTest::log( "frame %u: music: Crawler %s of player %d killed by player %d", TheGameLogic->getFrame(),
						obj->getTemplate()->getName().str(), owner != nullptr ? (int)owner->getPlayerIndex() : -1,
						killer != nullptr ? (int)killer->getPlayerIndex() : -1 );
				s_crawlers.erase( s_crawlers.begin() + i );
				continue;
			}
			++i;
		}
	}

	/// the game as the local player sees it
	struct Reading
	{
		Int engagedAny = 0;				///< engagements by or against the player
		Int engagedUnits = 0;			///< ... units only (no structures)
		Int againstMe = 0;				///< engagements against the player
		Bool manyEnemiesNear = FALSE;
		Bool fewEnemiesNear = FALSE;
		Real myWorth = 0.0f;
		Real strongestEnemy = 0.0f;
	};

	/// counts the enemy units around one of the player's: whether NearEnemyCount are within NearEnemyRange,
	/// FewEnemyCount within FewEnemyRange
	void countAround( Object *mine, Reading *r )
	{
		PartitionFilterRelationship enemies( mine, PartitionFilterRelationship::ALLOW_ENEMIES );
		PartitionFilterAlive alive;
		PartitionFilter *filters[] = { &enemies, &alive, nullptr };
		const Real range = max( s_settings.nearEnemyRange, s_settings.fewEnemyRange );
		ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( mine, range, FROM_CENTER_2D, filters );
		MemoryPoolObjectHolder hold( iter );
		Int near1 = 0, few = 0;
		const Real fewSq = s_settings.fewEnemyRange * s_settings.fewEnemyRange;
		const Real nearSq = s_settings.nearEnemyRange * s_settings.nearEnemyRange;
		for( Object *o = iter->first(); o != nullptr; o = iter->next() )
		{
			if( o->isKindOf( KINDOF_STRUCTURE ) || o->isKindOf( KINDOF_PROJECTILE ) || o->isKindOf( KINDOF_UNATTACKABLE )
				|| o->isKindOf( KINDOF_INERT ) || !isPlayers( o->getControllingPlayer() ) )
				continue;
			const Real dx = o->getPosition()->x - mine->getPosition()->x, dy = o->getPosition()->y - mine->getPosition()->y;
			const Real d = dx * dx + dy * dy;
			if( d <= nearSq )
				++near1;
			if( d <= fewSq )
				++few;
		}
		r->manyEnemiesNear = r->manyEnemiesNear || near1 >= s_settings.nearEnemyCount;
		r->fewEnemiesNear = r->fewEnemiesNear || few >= s_settings.fewEnemyCount;
	}

	Reading read( Player *me )
	{
		Reading r;
		const UnsignedInt frame = TheGameLogic->getFrame();
		Real worth[MAX_PLAYER_COUNT] = { 0.0f };
		std::vector<Object *> mine;
		for( Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject() )
		{
			if( obj->isKindOf( KINDOF_PROJECTILE ) || obj->isKindOf( KINDOF_INERT ) )
				continue;
			Player *owner = obj->getControllingPlayer();
			if( !isPlayers( owner ) )
				continue;
			const Bool isMine = owner == me;
			if( !isMine && !isEnemy( me, owner ) )
				continue;
			if( obj->isEffectivelyDead() )
				continue;
			if( isCrawler( obj ) && std::find( s_crawlers.begin(), s_crawlers.end(), obj->getID() ) == s_crawlers.end() )
				s_crawlers.push_back( obj->getID() );
			if( owner->getPlayerIndex() >= 0 && owner->getPlayerIndex() < MAX_PLAYER_COUNT )
				worth[ owner->getPlayerIndex() ] += (Real)obj->getTemplate()->calcCostToBuild( owner );

			// hit in the window, by the other side: an engagement (its last hit's frame)
			const BodyModuleInterface *body = obj->getBodyModule();
			const UnsignedInt *hit = body != nullptr ? body->getLastDamageTimestamp() : nullptr;
			if( hit != nullptr && *hit != 0 && frame - *hit <= s_settings.engagementWindow )
			{
				Player *by = lastHitBy( obj );
				if( ( isMine && isEnemy( me, by ) ) || ( !isMine && by == me ) )
				{
					std::vector<Engagement>::iterator it = s_engagements.begin();
					while( it != s_engagements.end() && it->victim != obj->getID() )
						++it;
					if( it == s_engagements.end() )
					{
						Engagement e;
						e.victim = obj->getID();
						s_engagements.push_back( e );
						it = s_engagements.end() - 1;
					}
					it->frame = *hit;
					it->againstMe = isMine;
					it->structure = obj->isKindOf( KINDOF_STRUCTURE );
				}
			}
			if( isMine && !obj->isKindOf( KINDOF_STRUCTURE ) && !obj->isKindOf( KINDOF_UNATTACKABLE ) && !obj->isKindOf( KINDOF_PROP ) )
				mine.push_back( obj );
		}

		// the window's engagements (a thing killed in it still counts: C&C4 counts engagements, not the living)
		for( size_t i = 0; i < s_engagements.size(); )
		{
			const Engagement &e = s_engagements[i];
			if( frame - e.frame > s_settings.engagementWindow )
			{
				s_engagements.erase( s_engagements.begin() + i );
				continue;
			}
			++r.engagedAny;
			if( !e.structure )
				++r.engagedUnits;
			if( e.againstMe )
				++r.againstMe;
			++i;
		}

		// enemy units close to the player's (C&C4 asks of each of the player's units)
		for( size_t i = 0; i < mine.size() && !( r.manyEnemiesNear && r.fewEnemiesNear ); ++i )
			countAround( mine[i], &r );

		r.myWorth = worth[ me->getPlayerIndex() ];
		for( Int n = 0; n < ThePlayerList->getPlayerCount(); ++n )
		{
			Player *p = ThePlayerList->getNthPlayer( n );
			if( p != nullptr && isPlayers( p ) && p->getPlayerIndex() < MAX_PLAYER_COUNT && isEnemy( me, p ) )
				r.strongestEnemy = max( r.strongestEnemy, worth[ p->getPlayerIndex() ] );
		}
		return r;
	}

	//-----------------------------------------------------------------------------------------------
	/** C&C4's transitions out of a state, in its order (the first that holds) */
	Int nextState( const Reading &r, UnsignedInt now )
	{
		const UnsignedInt inState = now - s_stateSince;
		const Int frame = (Int)TheGameLogic->getFrame();
		const Bool enemyKill = s_enemyCrawlerKilled >= 0 && frame - s_enemyCrawlerKilled <= (Int)s_settings.killWindow;
		const Bool playerKill = s_playerCrawlerKilled >= 0 && frame - s_playerCrawlerKilled <= (Int)s_settings.killWindow;
		const Bool settled = inState >= msOf( s_settings.minimumStateTime );
		const Bool big = r.againstMe >= s_settings.bigFightEngagements;
		const Bool noEngagement = r.engagedAny == 0;
		const Bool likely = r.myWorth > 0.0f && r.strongestEnemy > 0.0f && TheGameLogic->getFrame() >= s_settings.likelyToWinAfter
			&& r.myWorth >= r.strongestEnemy * s_settings.likelyToWinRatio;
		const Bool likelyNow = likely && inState >= msOf( s_settings.likelyToWinStateTime );
		switch( s_state )
		{
			case TiberiumMusic::EXPLORE:
				if( r.manyEnemiesNear )
					return TiberiumMusic::NEAR_ENEMY_UNITS;
				return r.engagedUnits > 0 ? TiberiumMusic::FIRST_FIGHT : s_state;
			case TiberiumMusic::NEAR_ENEMY_UNITS:
				return r.engagedUnits > 0 ? TiberiumMusic::FIRST_FIGHT : s_state;
			case TiberiumMusic::FIRST_FIGHT:
				if( enemyKill )
					return TiberiumMusic::SMALL_ENEMY_CRAWLER_DESTROYED;
				if( playerKill )
					return TiberiumMusic::SMALL_PLAYER_CRAWLER_DESTROYED;
				if( settled && big )
					return TiberiumMusic::BIG_FIGHT;
				if( settled && noEngagement )
					return TiberiumMusic::FIGHT_FINISHED;
				return s_state;
			case TiberiumMusic::SMALL_FIGHT:
				if( enemyKill )
					return TiberiumMusic::SMALL_ENEMY_CRAWLER_DESTROYED;
				if( playerKill )
					return TiberiumMusic::SMALL_PLAYER_CRAWLER_DESTROYED;
				if( settled && big )
					return TiberiumMusic::BIG_FIGHT;
				if( settled && noEngagement )
					return TiberiumMusic::FIGHT_FINISHED;
				return likelyNow ? TiberiumMusic::LIKELY_TO_WIN : s_state;
			case TiberiumMusic::BIG_FIGHT:
				if( enemyKill )
					return TiberiumMusic::BIG_ENEMY_CRAWLER_DESTROYED;
				if( playerKill )
					return TiberiumMusic::BIG_PLAYER_CRAWLER_DESTROYED;
				if( settled && noEngagement )
					return TiberiumMusic::FIGHT_FINISHED;
				if( settled && r.againstMe < s_settings.mediumEngagements )
					return TiberiumMusic::SMALL_FIGHT;
				return likelyNow ? TiberiumMusic::LIKELY_TO_WIN : s_state;
			case TiberiumMusic::SMALL_ENEMY_CRAWLER_DESTROYED:
			case TiberiumMusic::BIG_ENEMY_CRAWLER_DESTROYED:
				return inState >= msOf( s_settings.enemyStingerTime ) ? s_returnState : s_state;
			case TiberiumMusic::SMALL_PLAYER_CRAWLER_DESTROYED:
			case TiberiumMusic::BIG_PLAYER_CRAWLER_DESTROYED:
				return inState >= msOf( s_settings.playerStingerTime ) ? s_returnState : s_state;
			case TiberiumMusic::FIGHT_FINISHED:
				if( settled && r.fewEnemiesNear )
					return TiberiumMusic::SMALL_FIGHT;
				if( settled && big )
					return TiberiumMusic::BIG_FIGHT;
				return likelyNow ? TiberiumMusic::LIKELY_TO_WIN : s_state;
			case TiberiumMusic::LIKELY_TO_WIN:
				return settled && !likely ? TiberiumMusic::SMALL_FIGHT : s_state;
			default:
				return s_state;
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** the themes the data knows: the maps', the tracks' */
	std::vector<AsciiString> knownThemes()
	{
		std::vector<AsciiString> themes;
		const AsciiString *const fixed[] = { &s_settings.defaultTheme, &s_settings.snowTheme };
		for( size_t i = 0; i < 2; ++i )
			if( fixed[i]->isNotEmpty() )
				themes.push_back( *fixed[i] );
		for( size_t i = 0; i < s_settings.mapThemes.size(); ++i )
			themes.push_back( s_settings.mapThemes[i].theme );
		for( size_t i = 0; i < s_settings.lists.size(); ++i )
			if( s_settings.lists[i].theme.isNotEmpty() )
				themes.push_back( s_settings.lists[i].theme );
		std::vector<AsciiString> out;
		for( size_t i = 0; i < themes.size(); ++i )
		{
			Bool seen = FALSE;
			for( size_t j = 0; j < out.size() && !seen; ++j )
				seen = out[j].compareNoCase( themes[i] ) == 0;
			if( !seen )
				out.push_back( themes[i] );
		}
		return out;
	}

	/// the states whose music each jukebox group lists, in its order
	const Int GROUP_STATES[TiberiumMusic::GROUP_COUNT][4] =
	{
		{ TiberiumMusic::EXPLORE, TiberiumMusic::NEAR_ENEMY_UNITS, TiberiumMusic::FIGHT_FINISHED, -1 },
		{ TiberiumMusic::FIRST_FIGHT, TiberiumMusic::SMALL_FIGHT, TiberiumMusic::BIG_FIGHT, -1 },
		{ TiberiumMusic::LIKELY_TO_WIN, -1, -1, -1 },
	};
}

//-------------------------------------------------------------------------------------------------
const char *TiberiumMusic::getStateName( Int state )
{
	return state >= 0 && state < STATE_COUNT ? STATE_NAMES[state] : "";
}

//-------------------------------------------------------------------------------------------------
void TiberiumMusic::parse( INI *ini )
{
	static const FieldParse fields[] =
	{
		{ "Enabled",							INI::parseBool,									nullptr, offsetof( Settings, enabled ) },
		{ "CheckInterval",				INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, checkInterval ) },
		{ "NearEnemyRange",				INI::parseReal,									nullptr, offsetof( Settings, nearEnemyRange ) },
		{ "NearEnemyCount",				INI::parseInt,									nullptr, offsetof( Settings, nearEnemyCount ) },
		{ "FewEnemyRange",				INI::parseReal,									nullptr, offsetof( Settings, fewEnemyRange ) },
		{ "FewEnemyCount",				INI::parseInt,									nullptr, offsetof( Settings, fewEnemyCount ) },
		{ "EngagementWindow",			INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, engagementWindow ) },
		{ "BigFightEngagements",	INI::parseInt,									nullptr, offsetof( Settings, bigFightEngagements ) },
		{ "MediumEngagements",		INI::parseInt,									nullptr, offsetof( Settings, mediumEngagements ) },
		{ "MinimumStateTime",			INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, minimumStateTime ) },
		{ "LikelyToWinStateTime",	INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, likelyToWinStateTime ) },
		{ "KillWindow",						INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, killWindow ) },
		{ "EnemyStingerTime",			INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, enemyStingerTime ) },
		{ "PlayerStingerTime",		INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, playerStingerTime ) },
		{ "LikelyToWinRatio",			INI::parseReal,									nullptr, offsetof( Settings, likelyToWinRatio ) },
		{ "LikelyToWinAfter",			INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, likelyToWinAfter ) },
		{ "SnowTheme",						INI::parseAsciiString,					nullptr, offsetof( Settings, snowTheme ) },
		{ "DefaultTheme",					INI::parseAsciiString,					nullptr, offsetof( Settings, defaultTheme ) },
		{ "Track",								parseTrack,											nullptr, 0 },
		{ "MapTheme",							parseMapTheme,									nullptr, 0 },
		// the first director's fields, from data made before C&C4's own rules: read, no longer used
		{ "NearEnemyDistance",		INI::parseReal,									nullptr, offsetof( Settings, supersededReal ) },
		{ "EngagedWindow",				INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, superseded ) },
		{ "BigFightUnits",				INI::parseInt,									nullptr, offsetof( Settings, superseded ) },
		{ "FightOverDelay",				INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, superseded ) },
		{ "FightFinishedTime",		INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, superseded ) },
		{ "StingerTime",					INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, superseded ) },
		{ "MinStateTime",					INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, superseded ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	s_settings.lists.clear();
	s_settings.mapThemes.clear();
	ini->initFromINI( &s_settings, fields );
	s_turn.clear();
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumMusic::isActive()
{
	if( !s_settings.enabled || s_settings.lists.empty() || TheGameLogic == nullptr || ThePlayerList == nullptr
		|| ThePlayerList->getLocalPlayer() == nullptr )
		return FALSE;
	return TheGameLogic->isInSkirmishGame() || TheGameLogic->isInMultiplayerGame() || TheGameLogic->isInReplayGame();
}

//-------------------------------------------------------------------------------------------------
void TiberiumMusic::reset()
{
	s_state = -1;
	s_stateSince = 0;
	s_returnState = -1;
	s_family = FAMILY_NONE;
	s_lastCheck = 0;
	s_lastPlay = 0;
	s_track.clear();
	s_trackState = -1;
	s_handle = 0;
	s_engagements.clear();
	s_crawlers.clear();
	s_enemyCrawlerKilled = s_playerCrawlerKilled = -1;
	s_turn.clear();
	s_mapTheme.clear();
	s_chosenTheme.clear();
	s_held = FALSE;
	s_heldGroup = -1;
	s_clockMs = 0.0;
}

//-------------------------------------------------------------------------------------------------
void TiberiumMusic::update()
{
	if( !isActive() )
		return;
	// the music clock: the real time of this frame, unless the game is paused (a hitch counts a moment)
	if( TheFramePacer != nullptr && !TheGameLogic->isGamePaused() )
		s_clockMs += min( (double)TheFramePacer->getUpdateTime() * 1000.0, 250.0 );
	const UnsignedInt now = (UnsignedInt)s_clockMs;
	Player *me = ThePlayerList->getLocalPlayer();
	if( s_state < 0 )
	{
		// the local player's side picks the tracks: wait until the game has given it one
		if( me->getSide().isEmpty() && me->getBaseSide().isEmpty() )
			return;
		// C&C4 toggles its fight set when the player's side is set, once a match (the first match: the second set)
		++s_matches;
		s_set = ( s_matches % 2 ) == 1 ? 2 : 1;
		s_mapTheme = themeOfMap();
		if( TiberiumTest::isActive() )
			TiberiumTest::log( "frame %u: music: map %s, theme %s, fight set %d", TheGameLogic->getFrame(),
				TheGlobalData->m_mapName.str(), s_mapTheme.str(), (int)s_set );
		enter( EXPLORE, now );
		s_lastCheck = now;
		return;
	}
	watchCrawlers( me );
	if( now - s_lastCheck >= max( (UnsignedInt)1, msOf( s_settings.checkInterval ) ) )
	{
		s_lastCheck = now;
		const Reading r = read( me );
		const Int next = nextState( r, now );
		if( next != s_state && next >= 0 )
		{
			if( isStinger( next ) )
				s_returnState = s_state;
			if( isStinger( next ) )
			{
				// (a stinger's kill is used up)
				if( next == SMALL_ENEMY_CRAWLER_DESTROYED || next == BIG_ENEMY_CRAWLER_DESTROYED )
					s_enemyCrawlerKilled = -1;
				else
					s_playerCrawlerKilled = -1;
			}
			// a stinger back to its fight: the fight state, on its stinger's music (C&C4's fight event changes
			// nothing then: its music family is the same); FirstFight comes back as SmallFight
			if( isStinger( s_state ) && next == s_returnState && next == FIRST_FIGHT )
				enter( SMALL_FIGHT, now );
			else
				enter( next, now );
		}
	}
	// the track ended: the next of the state's, or of the held group's (asked by its handle: the music group
	// plays on between tracks on some audio devices)
	if( TheAudio != nullptr && TheGlobalData->m_audioOn && TheAudio->isOn( AudioAffect_Music ) && !trackPlaying()
		&& now - s_lastPlay > 2000 )
	{
		if( s_held && s_heldGroup >= 0 )
		{
			std::vector<Entry> entries;
			getEntries( entries );
			std::vector<AsciiString> group;
			for( size_t i = 0; i < entries.size(); ++i )
				if( entries[i].group == s_heldGroup && std::find( group.begin(), group.end(), entries[i].track ) == group.end() )
					group.push_back( entries[i].track );
			if( !group.empty() )
			{
				size_t at = std::find( group.begin(), group.end(), s_track ) - group.begin();
				playTrack( group[ at < group.size() ? ( at + 1 ) % group.size() : 0 ], -1 );
			}
		}
		else
		{
			// a stinger's track ran out: its fight's music
			const Int state = isStinger( s_state ) && s_returnState >= 0 ? s_returnState : s_state;
			const Int list = listOrStandIn( state, FALSE );
			if( !playNextOf( list, state ) && list < 0 )
				playNextOf( listFor( EXPLORE, FALSE ), EXPLORE );
		}
	}
}

//-------------------------------------------------------------------------------------------------
Int TiberiumMusic::getState()
{
	return s_state;
}

//-------------------------------------------------------------------------------------------------
Int TiberiumMusic::getGroupOf( Int state )
{
	switch( state )
	{
		case EXPLORE:
		case NEAR_ENEMY_UNITS:
		case FIGHT_FINISHED:
			return GROUP_CALM;
		case LIKELY_TO_WIN:
			return GROUP_WINNING;
		default:
			return state >= 0 ? GROUP_FIGHT : -1;
	}
}

//-------------------------------------------------------------------------------------------------
UnicodeString TiberiumMusic::getStateLabel( Int state )
{
	return UnicodeString( state >= 0 && state < STATE_COUNT ? STATE_LABELS[state] : L"" );
}

//-------------------------------------------------------------------------------------------------
UnicodeString TiberiumMusic::getGroupLabel( Int group )
{
	static const wchar_t *const labels[GROUP_COUNT] = { L"Calm", L"Fight", L"Winning" };
	return UnicodeString( group >= 0 && group < GROUP_COUNT ? labels[group] : L"" );
}

//-------------------------------------------------------------------------------------------------
AsciiString TiberiumMusic::getTheme()
{
	return theme();
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumMusic::isThemeChosen()
{
	return s_chosenTheme.isNotEmpty();
}

//-------------------------------------------------------------------------------------------------
Int TiberiumMusic::getFightSet()
{
	return s_set;
}

//-------------------------------------------------------------------------------------------------
AsciiString TiberiumMusic::getTrack()
{
	return s_track;
}

//-------------------------------------------------------------------------------------------------
UnicodeString TiberiumMusic::getTrackLabel()
{
	std::vector<Entry> entries;
	getEntries( entries );
	for( size_t i = 0; i < entries.size(); ++i )
		if( entries[i].track == s_track )
			return entries[i].label;
	// a track the jukebox does not list (a stinger): its state's name
	for( size_t i = 0; i < s_settings.lists.size(); ++i )
		if( std::find( s_settings.lists[i].tracks.begin(), s_settings.lists[i].tracks.end(), s_track ) != s_settings.lists[i].tracks.end() )
			return UnicodeString( STATE_LABELS[ s_settings.lists[i].state ] );
	UnicodeString label;
	if( s_track.isNotEmpty() )
		label.translate( s_track );
	return label;
}

//-------------------------------------------------------------------------------------------------
void TiberiumMusic::getEntries( std::vector<Entry> &out )
{
	out.clear();
	if( TheAudio == nullptr )
		return;
	for( Int group = 0; group < GROUP_COUNT; ++group )
	{
		for( Int k = 0; k < 4 && GROUP_STATES[group][k] >= 0; ++k )
		{
			const Int state = GROUP_STATES[group][k];
			// a small fight's two lists: going on, and coming back after the fight was over
			for( Int pass = 0; pass < ( state == SMALL_FIGHT ? 2 : 1 ); ++pass )
			{
				const Int list = listFor( state, pass == 1 );
				if( list < 0 || ( pass == 1 && s_settings.lists[list].from < 0 ) )
					continue;
				std::vector<AsciiString> distinct;
				const std::vector<AsciiString> &tracks = s_settings.lists[list].tracks;
				for( size_t i = 0; i < tracks.size(); ++i )
					if( known( tracks[i] ) && std::find( distinct.begin(), distinct.end(), tracks[i] ) == distinct.end() )
						distinct.push_back( tracks[i] );
				for( size_t i = 0; i < distinct.size(); ++i )
				{
					Bool listed = FALSE;
					for( size_t j = 0; j < out.size() && !listed; ++j )
						listed = out[j].track == distinct[i];
					if( listed )
						continue;
					Entry e;
					e.track = distinct[i];
					e.group = group;
					e.playing = distinct[i] == s_track && trackPlaying();
					e.label = STATE_LABELS[state];
					if( distinct.size() > 1 )
					{
						UnicodeString n;
						n.format( L" %d", (int)( i + 1 ) );
						e.label.concat( n );
					}
					if( pass == 1 )
						e.label.concat( L" (return)" );
					out.push_back( e );
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumMusic::pick( const AsciiString &track )
{
	if( !isActive() || !known( track ) )
		return;
	std::vector<Entry> entries;
	getEntries( entries );
	for( size_t i = 0; i < entries.size(); ++i )
		if( entries[i].track == track )
			s_heldGroup = entries[i].group;
	playTrack( track, -1 );
}

//-------------------------------------------------------------------------------------------------
void TiberiumMusic::next()
{
	if( !isActive() )
		return;
	std::vector<Entry> entries;
	getEntries( entries );
	if( entries.empty() )
		return;
	// the group playing: the track's, else the state's
	Int group = s_held && s_heldGroup >= 0 ? s_heldGroup : getGroupOf( s_state );
	size_t at = entries.size();
	for( size_t i = 0; i < entries.size(); ++i )
		if( entries[i].track == s_track )
		{
			at = i;
			group = entries[i].group;
		}
	for( size_t step = 1; step <= entries.size(); ++step )
	{
		const size_t i = ( ( at < entries.size() ? at : 0 ) + step ) % entries.size();
		if( entries[i].group == group )
		{
			pick( entries[i].track );
			return;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumMusic::nextTheme()
{
	if( !isActive() )
		return;
	const std::vector<AsciiString> themes = knownThemes();
	if( themes.empty() )
		return;
	// the map's own, then each other in turn, then the map's again
	if( s_chosenTheme.isEmpty() )
	{
		size_t at = 0;
		while( at < themes.size() && themes[at].compareNoCase( s_mapTheme ) != 0 )
			++at;
		s_chosenTheme = themes[ at < themes.size() ? ( at + 1 ) % themes.size() : 0 ];
	}
	else
	{
		size_t at = 0;
		while( at < themes.size() && themes[at].compareNoCase( s_chosenTheme ) != 0 )
			++at;
		const AsciiString next1 = themes[ at < themes.size() ? ( at + 1 ) % themes.size() : 0 ];
		s_chosenTheme = next1.compareNoCase( s_mapTheme ) == 0 ? AsciiString::TheEmptyString : next1;
	}
	if( TiberiumTest::isActive() )
		TiberiumTest::log( "frame %u: music: theme %s%s", TheGameLogic->getFrame(), theme().str(), s_chosenTheme.isEmpty() ? " (the map's)" : "" );
	// the calm music follows the theme at once
	if( s_state >= 0 && getGroupOf( s_state ) == GROUP_CALM && !s_held )
		playNextOf( listFor( s_state == NEAR_ENEMY_UNITS && listFor( s_state, FALSE ) < 0 ? EXPLORE : s_state, FALSE ), s_state );
	else if( s_held && s_heldGroup == GROUP_CALM )
	{
		std::vector<Entry> entries;
		getEntries( entries );
		for( size_t i = 0; i < entries.size(); ++i )
			if( entries[i].group == GROUP_CALM )
			{
				playTrack( entries[i].track, -1 );
				break;
			}
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumMusic::setHeld( Bool held )
{
	s_held = held;
	if( held )
	{
		if( s_heldGroup < 0 )
			s_heldGroup = getGroupOf( s_trackState >= 0 ? s_trackState : s_state );
	}
	else
	{
		s_heldGroup = -1;
		// following the game again: the state's music
		if( isActive() && s_state >= 0 )
		{
			s_family = FAMILY_NONE;
			musicFor( isStinger( s_state ) && s_returnState >= 0 ? s_returnState : s_state );
		}
	}
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumMusic::isHeld()
{
	return s_held;
}

//-------------------------------------------------------------------------------------------------
void TiberiumMusic::describe( char *buffer, Int size )
{
	if( buffer == nullptr || size <= 0 )
		return;
	if( !isActive() )
	{
		snprintf( buffer, size, "off" );
		return;
	}
	std::vector<Entry> entries;
	getEntries( entries );
	Int counts[GROUP_COUNT] = { 0, 0, 0 };
	for( size_t i = 0; i < entries.size(); ++i )
		++counts[ entries[i].group ];
	snprintf( buffer, size, "state %s for %u ms, theme %s%s, set %d, track %s (%s), %s; jukebox %d calm, %d fight, %d winning; engagements %d",
		getStateName( s_state ), (unsigned)( (UnsignedInt)s_clockMs - s_stateSince ), theme().str(), s_chosenTheme.isEmpty() ? " (map)" : " (chosen)",
		(int)s_set, s_track.str(), trackPlaying() ? "playing" : "stopped", s_held ? "held" : "following", (int)counts[0], (int)counts[1],
		(int)counts[2], (int)s_engagements.size() );
}

//-------------------------------------------------------------------------------------------------
void INI::parseTiberiumMusicDefinition( INI *ini )
{
	TiberiumMusic::parse( ini );
}
