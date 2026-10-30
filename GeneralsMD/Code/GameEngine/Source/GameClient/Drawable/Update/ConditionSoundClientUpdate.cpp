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

// FILE: ConditionSoundClientUpdate.cpp ///////////////////////////////////////////////////////////
// Tiberium Project @feature sounds by model state and by animation frame (see the header)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/Module/ConditionSoundClientUpdate.h"

#include "Common/AudioEventInfo.h"
#include "Common/AudioEventRTS.h"
#include "Common/DrawModule.h"
#include "Common/GameAudio.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"

//-------------------------------------------------------------------------------------------------
/** the model conditions named from here on, up to EXCLUDE (into required) and after it (into
	* excluded); unknown names are counted, so an entry that needs one Zero Hour lacks never plays */
static Int parseConditions( INI *ini, ModelConditionFlags *required, ModelConditionFlags *excluded, std::vector<Int> *numbers )
{
	Int unknownRequired = 0;
	Bool exclude = FALSE;
	for( const char *token = ini->getNextTokenOrNull(); token != nullptr; token = ini->getNextTokenOrNull() )
	{
		if( stricmp( token, "EXCLUDE" ) == 0 )
		{
			exclude = TRUE;
			continue;
		}
		if( numbers != nullptr && !exclude && token[0] >= '0' && token[0] <= '9' )
		{
			numbers->push_back( atoi( token ) );
			continue;
		}
		const Int bit = ModelConditionFlags::getSingleBitFromName( token );
		if( bit < 0 )
		{
			if( !exclude )
				++unknownRequired;
			continue;
		}
		if( exclude )
			excluded->set( bit );
		else if( required != nullptr )
			required->set( bit );
	}
	return unknownRequired;
}

//-------------------------------------------------------------------------------------------------
void ConditionSoundClientUpdateModuleData::parseLoop( INI *ini, void *instance, void * /*store*/, const void * /*userData*/ )
{
	ConditionSoundClientUpdateModuleData *self = (ConditionSoundClientUpdateModuleData *)instance;
	Loop loop;
	loop.sound = ini->getNextToken();
	if( parseConditions( ini, &loop.required, &loop.excluded, nullptr ) == 0 )
		self->m_loops.push_back( loop );
}

//-------------------------------------------------------------------------------------------------
void ConditionSoundClientUpdateModuleData::parseAnimationSound( INI *ini, void *instance, void * /*store*/, const void * /*userData*/ )
{
	ConditionSoundClientUpdateModuleData *self = (ConditionSoundClientUpdateModuleData *)instance;
	AnimationSound entry;
	entry.sound = ini->getNextToken();
	entry.animation = ini->getNextToken();
	entry.animation.toUpper();
	parseConditions( ini, nullptr, &entry.excluded, &entry.frames );
	if( !entry.frames.empty() )
		self->m_animationSounds.push_back( entry );
}

//-------------------------------------------------------------------------------------------------
void ConditionSoundClientUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	ClientUpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "Loop",						parseLoop,						nullptr, 0 },
		{ "AnimationSound",	parseAnimationSound,	nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
ConditionSoundClientUpdate::ConditionSoundClientUpdate( Thing *thing, const ModuleData* moduleData ) :
	ClientUpdateModule( thing, moduleData ),
	m_lastFrame( -1.0f )
{
	const size_t count = getConditionSoundClientUpdateModuleData()->m_loops.size();
	m_loopHandles.assign( count, (AudioHandle)AHSV_NoSound );
	m_loopActive.assign( count, FALSE );
}

//-------------------------------------------------------------------------------------------------
ConditionSoundClientUpdate::~ConditionSoundClientUpdate()
{
	stopAll();
}

//-------------------------------------------------------------------------------------------------
void ConditionSoundClientUpdate::stopAll()
{
	for( size_t i = 0; i < m_loopHandles.size(); ++i )
	{
		if( m_loopHandles[i] != (AudioHandle)AHSV_NoSound && TheAudio != nullptr )
			TheAudio->removeAudioEvent( m_loopHandles[i] );
		m_loopHandles[i] = (AudioHandle)AHSV_NoSound;
		m_loopActive[i] = FALSE;
	}
}

//-------------------------------------------------------------------------------------------------
/** a sound at the drawable; its handle */
static AudioHandle playAt( Drawable *draw, const AsciiString &sound )
{
	if( TheAudio == nullptr || sound.isEmpty() )
		return (AudioHandle)AHSV_NoSound;
	AudioEventRTS event( sound );
	event.setDrawableID( draw->getID() );
	return TheAudio->addAudioEvent( &event );
}

//-------------------------------------------------------------------------------------------------
void ConditionSoundClientUpdate::clientUpdate()
{
	const ConditionSoundClientUpdateModuleData *data = getConditionSoundClientUpdateModuleData();
	Drawable *draw = getDrawable();
	if( draw == nullptr )
		return;
	const ModelConditionFlags &flags = draw->getModelConditionFlags();

	// the states' sounds: started as a state begins, a looping one stopped as it ends
	for( size_t i = 0; i < data->m_loops.size(); ++i )
	{
		const ConditionSoundClientUpdateModuleData::Loop &loop = data->m_loops[i];
		const Bool active = flags.testForAll( loop.required ) && !flags.testForAny( loop.excluded );
		if( active && !m_loopActive[i] )
			m_loopHandles[i] = playAt( draw, loop.sound );
		else if( !active && m_loopActive[i] )
		{
			if( m_loopHandles[i] != (AudioHandle)AHSV_NoSound && TheAudio != nullptr )
				TheAudio->removeAudioEvent( m_loopHandles[i] );
			m_loopHandles[i] = (AudioHandle)AHSV_NoSound;
		}
		m_loopActive[i] = active;
	}

	// the animations' sounds: each frame given that the animation has passed since the last update
	if( data->m_animationSounds.empty() )
		return;
	AsciiString animation;
	Real frame = 0.0f;
	const ObjectDrawInterface *odi = nullptr;
	for( DrawModule **dm = draw->getDrawModules(); *dm && odi == nullptr; ++dm )
		odi = (*dm)->getObjectDrawInterface();
	if( odi == nullptr || !odi->clientOnly_getAnimationFrame( &animation, &frame ) )
	{
		m_lastAnimation.clear();
		return;
	}
	const Bool sameAnimation = animation == m_lastAnimation;
	const Real from = sameAnimation ? m_lastFrame : -1.0f;
	const Bool wrapped = sameAnimation && frame < m_lastFrame;	// it looped
	for( size_t i = 0; i < data->m_animationSounds.size(); ++i )
	{
		const ConditionSoundClientUpdateModuleData::AnimationSound &entry = data->m_animationSounds[i];
		if( entry.animation != animation || flags.testForAny( entry.excluded ) )
			continue;
		for( size_t k = 0; k < entry.frames.size(); ++k )
		{
			const Real f = (Real)entry.frames[k];
			const Bool passed = wrapped ? ( f > from || f <= frame ) : ( f > from && f <= frame );
			if( passed )
			{
				playAt( draw, entry.sound );
				break;	// one a frame is enough
			}
		}
	}
	m_lastAnimation = animation;
	m_lastFrame = frame;
}

//-------------------------------------------------------------------------------------------------
void ConditionSoundClientUpdate::crc( Xfer *xfer )
{
	ClientUpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version (nothing of its own is saved: the sounds start again from the state loaded)
//-------------------------------------------------------------------------------------------------
void ConditionSoundClientUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	ClientUpdateModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void ConditionSoundClientUpdate::loadPostProcess()
{
	ClientUpdateModule::loadPostProcess();
	stopAll();
}
