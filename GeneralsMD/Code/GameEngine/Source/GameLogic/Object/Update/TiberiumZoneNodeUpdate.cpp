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

// FILE: TiberiumZoneNodeUpdate.cpp ///////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's zone node (see TiberiumZoneNodeUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/TiberiumZoneNodeUpdate.h"

#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"

namespace
{
	const UnsignedInt CHECK_EVERY = 6;

	Bool isPlayable( const Player *player )
	{
		if( player == nullptr || ThePlayerList == nullptr || player == ThePlayerList->getNeutralPlayer() )
			return FALSE;
		return player->getPlayerType() == PLAYER_HUMAN || player->getPlayerType() == PLAYER_COMPUTER;
	}

	/// one side: the same player or allies
	Bool sameSide( const Player *a, const Player *b )
	{
		if( a == nullptr || b == nullptr )
			return FALSE;
		return a == b || ( a->getRelationship( b->getDefaultTeam() ) == ALLIES && b->getRelationship( a->getDefaultTeam() ) == ALLIES );
	}

	NameKeyType nodeKey()
	{
		static NameKeyType key = NAMEKEY_INVALID;
		if( key == NAMEKEY_INVALID )
			key = NAMEKEY( "TiberiumZoneNodeUpdate" );
		return key;
	}
}

//-------------------------------------------------------------------------------------------------
TiberiumZoneNodeUpdateModuleData::TiberiumZoneNodeUpdateModuleData()
{
	m_captureRadius = 120.0f;
	m_captureTime = LOGICFRAMES_PER_SECOND * 20;
	m_decaptureTime = LOGICFRAMES_PER_SECOND * 30;
	m_capturerKindOf = MAKE_KINDOF_MASK( KINDOF_INFANTRY );
	m_capturerKindOf.set( KINDOF_VEHICLE );
	m_capturerForbiddenKindOf = MAKE_KINDOF_MASK( KINDOF_AIRCRAFT );
	m_captureFX = nullptr;
	m_loseFX = nullptr;
}

//-------------------------------------------------------------------------------------------------
void TiberiumZoneNodeUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	static const FieldParse dataFieldParse[] =
	{
		{ "CaptureRadius",						INI::parseReal,									nullptr, offsetof( TiberiumZoneNodeUpdateModuleData, m_captureRadius ) },
		{ "CaptureTime",							INI::parseDurationUnsignedInt,	nullptr, offsetof( TiberiumZoneNodeUpdateModuleData, m_captureTime ) },
		{ "DecaptureTime",						INI::parseDurationUnsignedInt,	nullptr, offsetof( TiberiumZoneNodeUpdateModuleData, m_decaptureTime ) },
		{ "CapturerKindOf",						KindOfMaskType::parseFromINI,		nullptr, offsetof( TiberiumZoneNodeUpdateModuleData, m_capturerKindOf ) },
		{ "CapturerForbiddenKindOf",	KindOfMaskType::parseFromINI,		nullptr, offsetof( TiberiumZoneNodeUpdateModuleData, m_capturerForbiddenKindOf ) },
		{ "CaptureFX",								INI::parseFXList,								nullptr, offsetof( TiberiumZoneNodeUpdateModuleData, m_captureFX ) },
		{ "LoseFX",										INI::parseFXList,								nullptr, offsetof( TiberiumZoneNodeUpdateModuleData, m_loseFX ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse( p );
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
TiberiumZoneNodeUpdate::TiberiumZoneNodeUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_capturer = -1;
	m_progress = 0;
	setWakeFrame( getObject(), UPDATE_SLEEP( CHECK_EVERY ) );
}

//-------------------------------------------------------------------------------------------------
TiberiumZoneNodeUpdate::~TiberiumZoneNodeUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumZoneNodeUpdate::isNode( const Object *obj )
{
	return obj != nullptr && obj->findUpdateModule( nodeKey() ) != nullptr;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumZoneNodeUpdate::getCapture( const Object *obj, Int *playerIndex, Real *fraction, Bool *decapturing )
{
	if( obj == nullptr )
		return FALSE;
	const TiberiumZoneNodeUpdate *node = (const TiberiumZoneNodeUpdate *)obj->findUpdateModule( nodeKey() );
	if( node == nullptr || node->m_capturer < 0 || node->m_progress == 0 )
		return FALSE;
	const TiberiumZoneNodeUpdateModuleData *d = node->getTiberiumZoneNodeUpdateModuleData();
	const Bool held = isPlayable( obj->getControllingPlayer() );
	const UnsignedInt need = max( (UnsignedInt)1, held ? d->m_decaptureTime : d->m_captureTime );
	*playerIndex = node->m_capturer;
	*fraction = min( 1.0f, (Real)node->m_progress / (Real)need );
	*decapturing = held;
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** GDI's light while GDI holds it (Zero Hour's China's sides are Nod's), Nod's while Nod does */
void TiberiumZoneNodeUpdate::showHolder()
{
	Object *me = getObject();
	const Player *holder = me->getControllingPlayer();
	ModelConditionFlags clear;
	clear.set( MODELCONDITION_USER_1 );
	clear.set( MODELCONDITION_USER_2 );
	ModelConditionFlags set;
	if( isPlayable( holder ) )
	{
		const Bool nod = holder->getSide().compareNoCase( "China" ) == 0 || holder->getBaseSide().compareNoCase( "China" ) == 0;
		set.set( nod ? MODELCONDITION_USER_2 : MODELCONDITION_USER_1 );
	}
	me->clearAndSetModelConditionFlags( clear, set );
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime TiberiumZoneNodeUpdate::update()
{
	const TiberiumZoneNodeUpdateModuleData *d = getTiberiumZoneNodeUpdateModuleData();
	Object *me = getObject();
	if( me->isEffectivelyDead() )
		return UPDATE_SLEEP_FOREVER;
	Player *holder = me->getControllingPlayer();
	const Bool held = isPlayable( holder );

	// who stands near: one side, or both (contested), or nobody
	Player *side = nullptr;
	Bool contested = FALSE;
	Bool capturerNear = FALSE;
	{
		PartitionFilterAlive alive;
		PartitionFilter *filters[] = { &alive, nullptr };
		ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( me, d->m_captureRadius, FROM_CENTER_2D, filters, ITER_SORTED_NEAR_TO_FAR );
		MemoryPoolObjectHolder hold( iter );
		for( Object *other = iter->first(); other != nullptr && !contested; other = iter->next() )
		{
			if( other == me || !other->isAnyKindOf( d->m_capturerKindOf ) || other->isAnyKindOf( d->m_capturerForbiddenKindOf ) )
				continue;
			if( other->isAirborneTarget() || other->isContained() || other->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
				continue;
			Player *p = other->getControllingPlayer();
			if( !isPlayable( p ) )
				continue;
			if( side == nullptr )
				side = p;
			else if( !sameSide( side, p ) )
				contested = TRUE;
			if( p->getPlayerIndex() == m_capturer )
				capturerNear = TRUE;
		}
	}

	if( contested )
		return UPDATE_SLEEP( CHECK_EVERY );

	// nobody taking it (nobody near, or its holder's side): what was done fades
	if( side == nullptr || ( held && sameSide( holder, side ) ) )
	{
		m_progress = m_progress > CHECK_EVERY ? m_progress - CHECK_EVERY : 0;
		if( m_progress == 0 )
			m_capturer = -1;
		return UPDATE_SLEEP( CHECK_EVERY );
	}

	// another side's taking fades before this one's starts
	if( m_capturer >= 0 && !capturerNear )
	{
		const Player *was = ThePlayerList->getNthPlayer( m_capturer );
		if( !sameSide( was, side ) )
		{
			m_progress = m_progress > CHECK_EVERY ? m_progress - CHECK_EVERY : 0;
			if( m_progress == 0 )
				m_capturer = -1;
			return UPDATE_SLEEP( CHECK_EVERY );
		}
	}
	if( m_capturer < 0 )
		m_capturer = side->getPlayerIndex();

	m_progress += CHECK_EVERY;
	const UnsignedInt need = held ? d->m_decaptureTime : d->m_captureTime;
	if( m_progress >= need )
	{
		m_progress = 0;
		if( held )
		{
			// back to nobody's; the side here goes on to take it
			if( d->m_loseFX != nullptr )
				FXList::doFXObj( d->m_loseFX, me );
			me->setTeam( ThePlayerList->getNeutralPlayer()->getDefaultTeam() );
		}
		else
		{
			Player *taker = ThePlayerList->getNthPlayer( m_capturer );
			if( taker == nullptr )
				taker = side;
			if( d->m_captureFX != nullptr )
				FXList::doFXObj( d->m_captureFX, me );
			me->setTeam( taker->getDefaultTeam() );
			m_capturer = -1;
		}
		showHolder();
	}
	return UPDATE_SLEEP( CHECK_EVERY );
}

//-------------------------------------------------------------------------------------------------
void TiberiumZoneNodeUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void TiberiumZoneNodeUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	xfer->xferInt( &m_capturer );
	xfer->xferUnsignedInt( &m_progress );
}

//-------------------------------------------------------------------------------------------------
void TiberiumZoneNodeUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
