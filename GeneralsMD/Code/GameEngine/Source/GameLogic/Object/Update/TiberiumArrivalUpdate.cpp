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

// FILE: TiberiumArrivalUpdate.cpp ////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's Crawler arrival (see TiberiumArrivalUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/TiberiumArrivalUpdate.h"

#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameLogic/AI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TiberiumZones.h"

namespace
{
	const char *const STYLE_NAMES[] = { "AUTO", "DROP", "DRILL", nullptr };
}

//-------------------------------------------------------------------------------------------------
TiberiumArrivalUpdateModuleData::TiberiumArrivalUpdateModuleData()
{
	m_style = TIBERIUM_ARRIVAL_AUTO;
	m_arrivalTime = LOGICFRAMES_PER_SECOND * 2;
	m_dropHeight = 300.0f;
	m_drillDepth = 40.0f;
	m_startFX = nullptr;
	m_landFX = nullptr;
}

//-------------------------------------------------------------------------------------------------
void TiberiumArrivalUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	static const FieldParse dataFieldParse[] =
	{
		{ "Style",				INI::parseIndexList,						STYLE_NAMES,	offsetof( TiberiumArrivalUpdateModuleData, m_style ) },
		{ "ArrivalTime",	INI::parseDurationUnsignedInt,	nullptr,			offsetof( TiberiumArrivalUpdateModuleData, m_arrivalTime ) },
		{ "DropHeight",		INI::parseReal,									nullptr,			offsetof( TiberiumArrivalUpdateModuleData, m_dropHeight ) },
		{ "DrillDepth",		INI::parseReal,									nullptr,			offsetof( TiberiumArrivalUpdateModuleData, m_drillDepth ) },
		{ "StartFX",			INI::parseFXList,								nullptr,			offsetof( TiberiumArrivalUpdateModuleData, m_startFX ) },
		{ "LandFX",				INI::parseFXList,								nullptr,			offsetof( TiberiumArrivalUpdateModuleData, m_landFX ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse( p );
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
TiberiumArrivalUpdate::TiberiumArrivalUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_stage = STAGE_START;
	m_style = TIBERIUM_ARRIVAL_DROP;
	m_startFrame = m_endFrame = 0;
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
TiberiumArrivalUpdate::~TiberiumArrivalUpdate()
{
}

//-------------------------------------------------------------------------------------------------
/** where the model is drawn while it arrives: above its spot, falling faster as it comes (a drop),
	or below it, rising (a drill); the object itself is on its spot all along */
void TiberiumArrivalUpdate::showOffset() const
{
	Drawable *draw = getObject()->getDrawable();
	if( draw == nullptr )
		return;
	if( m_stage != STAGE_ARRIVING || m_endFrame <= m_startFrame )
	{
		draw->setInstanceMatrix( nullptr );
		return;
	}
	const TiberiumArrivalUpdateModuleData *d = getTiberiumArrivalUpdateModuleData();
	const UnsignedInt now = TheGameLogic->getFrame();
	Real t = (Real)( now - m_startFrame ) / (Real)( m_endFrame - m_startFrame );
	t = t < 0.0f ? 0.0f : ( t > 1.0f ? 1.0f : t );
	Matrix3D offset( true );
	if( m_style == TIBERIUM_ARRIVAL_DRILL )
	{
		// up through the ground, shaking as it breaks out
		const Real shake = ( ( now & 1 ) ? 1.0f : -1.0f ) * 1.5f * ( 1.0f - t );
		offset.Set_Translation( Vector3( shake, -shake, -d->m_drillDepth * ( 1.0f - t ) ) );
	}
	else
		offset.Set_Translation( Vector3( 0.0f, 0.0f, d->m_dropHeight * ( 1.0f - t * t ) ) );
	draw->setInstanceMatrix( &offset );
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime TiberiumArrivalUpdate::update()
{
	const TiberiumArrivalUpdateModuleData *d = getTiberiumArrivalUpdateModuleData();
	Object *me = getObject();
	const UnsignedInt now = TheGameLogic->getFrame();

	if( m_stage == STAGE_START )
	{
		m_stage = STAGE_DONE;
		// only what a factory made, in a game with zones
		if( !TiberiumZones::isOn() || me->getProducerID() == INVALID_ID || me->isEffectivelyDead() )
			return UPDATE_SLEEP_FOREVER;
		const Player *player = me->getControllingPlayer();
		const Object *factory = TheGameLogic->findObjectByID( me->getProducerID() );
		const Coord3D *near = me->getPosition();
		if( factory != nullptr )
		{
			near = factory->getPosition();
			const ExitInterface *exit = factory->getObjectExitInterface();
			if( exit != nullptr && exit->getRallyPoint() != nullptr )
				near = exit->getRallyPoint();
		}
		Coord3D spot;
		if( !TiberiumZones::findArrival( player, near, &spot ) )
			return UPDATE_SLEEP_FOREVER;
		me->setPosition( &spot );
		if( me->getAI() != nullptr )
			me->getAI()->aiIdle( CMD_FROM_AI );
		m_style = d->m_style;
		if( m_style == TIBERIUM_ARRIVAL_AUTO )
		{
			const Bool nod = player != nullptr && ( player->getSide().compareNoCase( "China" ) == 0 || player->getBaseSide().compareNoCase( "China" ) == 0 );
			m_style = nod ? TIBERIUM_ARRIVAL_DRILL : TIBERIUM_ARRIVAL_DROP;
		}
		const UnsignedInt time = max( (UnsignedInt)1, d->m_arrivalTime );
		m_startFrame = now;
		m_endFrame = now + time;
		m_stage = STAGE_ARRIVING;
		me->setDisabledUntil( DISABLED_HELD, m_endFrame );
		if( d->m_startFX != nullptr )
			FXList::doFXPos( d->m_startFX, &spot );
		showOffset();
		return UPDATE_SLEEP_NONE;
	}

	if( m_stage == STAGE_ARRIVING )
	{
		if( now >= m_endFrame )
		{
			m_stage = STAGE_DONE;
			showOffset();
			if( d->m_landFX != nullptr )
				FXList::doFXObj( d->m_landFX, me );
			return UPDATE_SLEEP_FOREVER;
		}
		showOffset();
		return UPDATE_SLEEP_NONE;
	}
	return UPDATE_SLEEP_FOREVER;
}

//-------------------------------------------------------------------------------------------------
void TiberiumArrivalUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void TiberiumArrivalUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	xfer->xferInt( &m_stage );
	xfer->xferInt( &m_style );
	xfer->xferUnsignedInt( &m_startFrame );
	xfer->xferUnsignedInt( &m_endFrame );
}

//-------------------------------------------------------------------------------------------------
void TiberiumArrivalUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	showOffset();
}
