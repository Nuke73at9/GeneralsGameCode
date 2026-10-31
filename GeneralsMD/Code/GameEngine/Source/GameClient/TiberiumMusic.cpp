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
#include <string.h>
#include <vector>

#include "Common/AudioAffect.h"
#include "Common/AudioEventRTS.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/GameAudio.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingTemplate.h"
#include "Common/TiberiumTest.h"
#include "GameClient/ClientRandomValue.h"
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
		"EnemyCrawlerDestroyed", "PlayerCrawlerDestroyed", "FightFinished", "LikelyToWin",
	};

	struct TrackList
	{
		Int state;
		AsciiString side;					///< a player side ("America"), or empty for any
		std::vector<AsciiString> tracks;
	};

	/// the TiberiumMusic block (durations in logic frames)
	struct Settings
	{
		Bool enabled;
		UnsignedInt checkInterval;
		Real nearEnemyDistance;
		UnsignedInt engagedWindow;
		Int bigFightUnits;
		UnsignedInt fightOverDelay;
		UnsignedInt fightFinishedTime;
		UnsignedInt stingerTime;
		Real likelyToWinRatio;
		UnsignedInt likelyToWinAfter;
		UnsignedInt minStateTime;
		std::vector<TrackList> lists;

		Settings()
		{
			enabled = FALSE;
			checkInterval = LOGICFRAMES_PER_SECOND;
			nearEnemyDistance = 500.0f;
			engagedWindow = LOGICFRAMES_PER_SECOND * 3;
			bigFightUnits = 12;
			fightOverDelay = LOGICFRAMES_PER_SECOND * 10;
			fightFinishedTime = LOGICFRAMES_PER_SECOND * 45;
			stingerTime = LOGICFRAMES_PER_SECOND * 20;
			likelyToWinRatio = 2.5f;
			likelyToWinAfter = LOGICFRAMES_PER_SECOND * 300;
			minStateTime = LOGICFRAMES_PER_SECOND * 8;
		}
	};
	Settings s_settings;

	/// the game's music as it goes
	Int s_state = -1;
	UnsignedInt s_stateSince = 0;
	UnsignedInt s_lastCheck = 0;
	UnsignedInt s_lastFight = 0;
	UnsignedInt s_holdUntil = 0;			///< a stinger, the first fight, the fight's end: held until then
	Bool s_hadFight = FALSE;
	UnsignedInt s_lastPlay = 0;
	AsciiString s_track;
	AudioHandle s_handle = 0;					///< the track playing (to know when it ends)
	std::vector<ObjectID> s_crawlersCounted;	///< Crawlers already seen dead

	void parseTrack( INI *ini, void *, void *, const void * )
	{
		// Track = <state> <side or Any> <track> <track> ...
		const char *stateName = ini->getNextToken();
		TrackList list;
		list.state = -1;
		for( Int i = 0; i < TiberiumMusic::STATE_COUNT; ++i )
		{
			if( stricmp( stateName, STATE_NAMES[i] ) == 0 )
				list.state = i;
		}
		if( list.state < 0 )
			throw INI_INVALID_DATA;
		const char *side = ini->getNextToken();
		if( stricmp( side, "Any" ) != 0 )
			list.side = side;
		for( const char *track = ini->getNextTokenOrNull(); track != nullptr; track = ini->getNextTokenOrNull() )
			list.tracks.push_back( AsciiString( track ) );
		s_settings.lists.push_back( list );
	}

	/// the tracks the local player hears in a state: its side's (its side or base side: "America" or
	/// "USA", which takes in the generals), else any side's
	const std::vector<AsciiString> *tracksFor( Int state )
	{
		const Player *me = ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
		const AsciiString side = me != nullptr ? me->getSide() : AsciiString::TheEmptyString;
		const AsciiString baseSide = me != nullptr ? me->getBaseSide() : AsciiString::TheEmptyString;
		const std::vector<AsciiString> *any = nullptr;
		for( size_t i = 0; i < s_settings.lists.size(); ++i )
		{
			const TrackList &list = s_settings.lists[i];
			if( list.state != state || list.tracks.empty() )
				continue;
			if( !list.side.isEmpty() && ( list.side.compareNoCase( side ) == 0 || list.side.compareNoCase( baseSide ) == 0 ) )
				return &list.tracks;
			if( list.side.isEmpty() && any == nullptr )
				any = &list.tracks;
		}
		return any;
	}

	void play( Int state )
	{
		const std::vector<AsciiString> *tracks = tracksFor( state );
		if( tracks == nullptr || TheAudio == nullptr )
			return;	// a state without tracks of its own: the music goes on
		// one of them (another than the one playing, when there is a choice), that the game knows
		std::vector<AsciiString> known;
		for( size_t i = 0; i < tracks->size(); ++i )
		{
			if( TheAudio->findAudioEventInfo( (*tracks)[i] ) != nullptr && ( tracks->size() == 1 || (*tracks)[i] != s_track ) )
				known.push_back( (*tracks)[i] );
		}
		if( known.empty() )
			return;
		const AsciiString name = known[ GameClientRandomValue( 0, (Int)known.size() - 1 ) ];
		TheAudio->removeAudioEvent( AHSV_StopTheMusicFade );
		AudioEventRTS event( name );
		event.setShouldFade( TRUE );
		if( ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
			event.setPlayerIndex( ThePlayerList->getLocalPlayer()->getPlayerIndex() );
		s_handle = TheAudio->addAudioEvent( &event );
		s_track = name;
		s_lastPlay = TheGameLogic->getFrame();
		if( TiberiumTest::isActive() )
			TiberiumTest::log( "frame %u: music: %s plays %s", TheGameLogic->getFrame(), STATE_NAMES[state], name.str() );
	}

	void enter( Int state, UnsignedInt now )
	{
		if( TiberiumTest::isActive() )
			TiberiumTest::log( "frame %u: music: %s -> %s", now, s_state >= 0 ? STATE_NAMES[s_state] : "(start)", STATE_NAMES[state] );
		s_state = state;
		s_stateSince = now;
		play( state );
	}

	Bool isFightState( Int state )
	{
		return state == TiberiumMusic::FIRST_FIGHT || state == TiberiumMusic::SMALL_FIGHT || state == TiberiumMusic::BIG_FIGHT
			|| state == TiberiumMusic::ENEMY_CRAWLER_DESTROYED || state == TiberiumMusic::PLAYER_CRAWLER_DESTROYED;
	}

	/// a real player's (not the neutral one's, not a civilian's)
	Bool isPlayers( const Player *player )
	{
		return player != nullptr && player != ThePlayerList->getNeutralPlayer()
			&& ( player->getPlayerType() == PLAYER_HUMAN || player->getPlayerType() == PLAYER_COMPUTER );
	}

	/// the game as the local player sees it
	struct Reading
	{
		Int mineEngaged = 0;
		Int enemiesEngaged = 0;
		Bool enemyNear = FALSE;
		Bool enemyCrawlerLost = FALSE;
		Bool myCrawlerLost = FALSE;
		Real myWorth = 0.0f;
		Real strongestEnemy = 0.0f;
	};

	Reading read( Player *me, UnsignedInt now )
	{
		Reading r;
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
			const Bool isEnemy = !isMine && me->getRelationship( owner->getDefaultTeam() ) == ENEMIES;
			if( !isMine && !isEnemy )
				continue;

			// a Crawler's death: a stinger
			const Bool crawler = strstr( obj->getTemplate()->getName().str(), "Crawler" ) != nullptr;
			if( obj->isEffectivelyDead() )
			{
				if( crawler && std::find( s_crawlersCounted.begin(), s_crawlersCounted.end(), obj->getID() ) == s_crawlersCounted.end() )
				{
					s_crawlersCounted.push_back( obj->getID() );
					( isMine ? r.myCrawlerLost : r.enemyCrawlerLost ) = TRUE;
				}
				continue;
			}

			if( owner->getPlayerIndex() >= 0 && owner->getPlayerIndex() < MAX_PLAYER_COUNT )
				worth[ owner->getPlayerIndex() ] += (Real)obj->getTemplate()->calcCostToBuild( owner );

			// in a fight: it fired, or was hit, a moment ago
			const UnsignedInt shot = obj->getLastShotFiredFrame();
			const BodyModuleInterface *body = obj->getBodyModule();
			const UnsignedInt *hit = body != nullptr ? body->getLastDamageTimestamp() : nullptr;
			const Bool shotRecently = shot != 0 && now - shot <= s_settings.engagedWindow;
			const Bool hitRecently = hit != nullptr && *hit != 0 && now - *hit <= s_settings.engagedWindow;
			if( isMine )
			{
				if( shotRecently || hitRecently )
					++r.mineEngaged;
				if( !obj->isKindOf( KINDOF_STRUCTURE ) && mine.size() < 64 )
					mine.push_back( obj );
			}
			else if( hitRecently )
			{
				// an enemy the player's forces hit
				const DamageInfo *info = body->getLastDamageInfo();
				const Object *source = info != nullptr ? TheGameLogic->findObjectByID( info->in.m_sourceID ) : nullptr;
				if( source != nullptr && source->getControllingPlayer() == me )
					++r.enemiesEngaged;
			}
		}

		// enemy units close to the player's
		for( size_t i = 0; i < mine.size() && !r.enemyNear; ++i )
		{
			PartitionFilterRelationship enemies( mine[i], PartitionFilterRelationship::ALLOW_ENEMIES );
			PartitionFilterAlive alive;
			PartitionFilter *filters[] = { &enemies, &alive, nullptr };
			const Object *near1 = ThePartitionManager->getClosestObject( mine[i], s_settings.nearEnemyDistance, FROM_CENTER_2D, filters );
			r.enemyNear = near1 != nullptr && isPlayers( near1->getControllingPlayer() ) && !near1->isKindOf( KINDOF_STRUCTURE );
		}

		r.myWorth = worth[ me->getPlayerIndex() ];
		for( Int n = 0; n < ThePlayerList->getPlayerCount(); ++n )
		{
			Player *p = ThePlayerList->getNthPlayer( n );
			if( p != nullptr && p != me && isPlayers( p ) && p->getPlayerIndex() < MAX_PLAYER_COUNT
				&& me->getRelationship( p->getDefaultTeam() ) == ENEMIES )
				r.strongestEnemy = max( r.strongestEnemy, worth[ p->getPlayerIndex() ] );
		}
		return r;
	}
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
		{ "Enabled",						INI::parseBool,									nullptr, offsetof( Settings, enabled ) },
		{ "CheckInterval",			INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, checkInterval ) },
		{ "NearEnemyDistance",	INI::parseReal,									nullptr, offsetof( Settings, nearEnemyDistance ) },
		{ "EngagedWindow",			INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, engagedWindow ) },
		{ "BigFightUnits",			INI::parseInt,									nullptr, offsetof( Settings, bigFightUnits ) },
		{ "FightOverDelay",			INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, fightOverDelay ) },
		{ "FightFinishedTime",	INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, fightFinishedTime ) },
		{ "StingerTime",				INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, stingerTime ) },
		{ "LikelyToWinRatio",		INI::parseReal,									nullptr, offsetof( Settings, likelyToWinRatio ) },
		{ "LikelyToWinAfter",		INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, likelyToWinAfter ) },
		{ "MinStateTime",				INI::parseDurationUnsignedInt,	nullptr, offsetof( Settings, minStateTime ) },
		{ "Track",							parseTrack,											nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	s_settings.lists.clear();
	ini->initFromINI( &s_settings, fields );
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
	s_lastCheck = 0;
	s_lastFight = 0;
	s_holdUntil = 0;
	s_hadFight = FALSE;
	s_lastPlay = 0;
	s_track.clear();
	s_handle = 0;
	s_crawlersCounted.clear();
}

//-------------------------------------------------------------------------------------------------
void TiberiumMusic::update()
{
	if( !isActive() )
		return;
	const UnsignedInt now = TheGameLogic->getFrame();
	if( s_state < 0 )
	{
		// the local player's side picks the tracks: wait until the game has given it one
		const Player *me = ThePlayerList->getLocalPlayer();
		if( me->getSide().isEmpty() && me->getBaseSide().isEmpty() )
			return;
		enter( EXPLORE, now );
		s_lastCheck = now;
		return;
	}
	if( now - s_lastCheck < max( (UnsignedInt)1, s_settings.checkInterval ) )
		return;
	s_lastCheck = now;

	Player *me = ThePlayerList->getLocalPlayer();
	const Reading r = read( me, now );
	const Int engaged = r.mineEngaged + r.enemiesEngaged;
	if( engaged > 0 )
		s_lastFight = now;

	Int next = s_state;
	if( r.enemyCrawlerLost || r.myCrawlerLost )
	{
		next = r.myCrawlerLost ? PLAYER_CRAWLER_DESTROYED : ENEMY_CRAWLER_DESTROYED;
		s_holdUntil = now + s_settings.stingerTime;
		s_hadFight = TRUE;
	}
	else if( now < s_holdUntil && ( isFightState( s_state ) || s_state == FIGHT_FINISHED ) && !( engaged >= s_settings.bigFightUnits && s_state != BIG_FIGHT ) )
	{
		// a stinger, the first fight, the fight's end: they play out (a big fight breaks in)
	}
	else if( engaged > 0 )
	{
		if( !s_hadFight )
		{
			next = FIRST_FIGHT;
			s_hadFight = TRUE;
			s_holdUntil = now + s_settings.minStateTime * 3;
		}
		else if( engaged >= s_settings.bigFightUnits || ( s_state == BIG_FIGHT && engaged * 2 >= s_settings.bigFightUnits ) )
			next = BIG_FIGHT;
		else
			next = SMALL_FIGHT;
	}
	else if( isFightState( s_state ) )
	{
		// the fighting stops: the fight is over after a while
		if( now - s_lastFight >= s_settings.fightOverDelay )
		{
			next = FIGHT_FINISHED;
			s_holdUntil = now + s_settings.fightFinishedTime;
		}
	}
	else if( r.myWorth > 0.0f && r.strongestEnemy > 0.0f && now >= s_settings.likelyToWinAfter
		&& r.myWorth >= r.strongestEnemy * s_settings.likelyToWinRatio )
		next = LIKELY_TO_WIN;
	else if( r.enemyNear )
		next = NEAR_ENEMY_UNITS;
	else
		next = EXPLORE;

	if( next != s_state )
	{
		// a fight comes in at once; the quiet states wait their turn
		if( isFightState( next ) || now - s_stateSince >= s_settings.minStateTime )
			enter( next, now );
	}
	else if( TheAudio != nullptr && TheGlobalData->m_audioOn && TheAudio->isOn( AudioAffect_Music ) && !TheAudio->isCurrentlyPlaying( s_handle )
		&& now - s_lastPlay > LOGICFRAMES_PER_SECOND * 2 )
	{
		// the track ended: another of the state's (asked by its handle: the music group plays on
		// between tracks on some audio devices)
		play( s_state );
	}
}

//-------------------------------------------------------------------------------------------------
void INI::parseTiberiumMusicDefinition( INI *ini )
{
	TiberiumMusic::parse( ini );
}
