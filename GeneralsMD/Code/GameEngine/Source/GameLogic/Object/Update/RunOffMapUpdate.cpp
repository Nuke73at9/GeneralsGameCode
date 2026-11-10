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

// FILE: RunOffMapUpdate.cpp //////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's RunOffMapUpdate and RunOffMapBehavior (see RunOffMapUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/RunOffMapUpdate.h"

#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"

namespace
{
	const Real PAST_THE_EDGE = 200.0f;	///< how far past the map's edge it flies before it is gone
}

//-------------------------------------------------------------------------------------------------
RunOffMapUpdateModuleData::RunOffMapUpdateModuleData()
{
	m_triggerOnOutOfAmmo = FALSE;
	m_triggerDelay = 0;
	m_refreshDelay = LOGICFRAMES_PER_SECOND / 4;
}

//-------------------------------------------------------------------------------------------------
void RunOffMapUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	static const FieldParse dataFieldParse[] =
	{
		{ "TriggerOnOutOfAmmo",	INI::parseBool,									nullptr, offsetof( RunOffMapUpdateModuleData, m_triggerOnOutOfAmmo ) },
		{ "TriggerDelay",				INI::parseDurationUnsignedInt,	nullptr, offsetof( RunOffMapUpdateModuleData, m_triggerDelay ) },
		{ "RefreshDelay",				INI::parseDurationUnsignedInt,	nullptr, offsetof( RunOffMapUpdateModuleData, m_refreshDelay ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse( p );
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
RunOffMapUpdate::RunOffMapUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	const RunOffMapUpdateModuleData *d = getRunOffMapUpdateModuleData();
	m_leaving = FALSE;
	m_exit.zero();
	m_triggerFrame = d->m_triggerDelay > 0 ? TheGameLogic->getFrame() + d->m_triggerDelay : 0;
	setWakeFrame( getObject(), UPDATE_SLEEP( max( (UnsignedInt)1, d->m_refreshDelay ) ) );
}

//-------------------------------------------------------------------------------------------------
RunOffMapUpdate::~RunOffMapUpdate()
{
}

//-------------------------------------------------------------------------------------------------
/** to the map's nearest edge and past it (SAGE's FLEE_TO_NEAREST_MAP_EDGE) */
void RunOffMapUpdate::leave()
{
	Object *me = getObject();
	m_leaving = TRUE;
	me->setSelectable( FALSE );
	AIUpdateInterface *ai = me->getAI();
	if( ai == nullptr )
	{
		TheGameLogic->destroyObject( me );	// (nothing to fly it off: it is simply gone)
		return;
	}
	Region3D map;
	TheTerrainLogic->getExtentIncludingBorder( &map );
	const Coord3D *pos = me->getPosition();
	m_exit = *pos;
	Real best = pos->x - map.lo.x;
	m_exit.x = map.lo.x - PAST_THE_EDGE;
	if( map.hi.x - pos->x < best )
	{
		best = map.hi.x - pos->x;
		m_exit = *pos;
		m_exit.x = map.hi.x + PAST_THE_EDGE;
	}
	if( pos->y - map.lo.y < best )
	{
		best = pos->y - map.lo.y;
		m_exit = *pos;
		m_exit.y = map.lo.y - PAST_THE_EDGE;
	}
	if( map.hi.y - pos->y < best )
	{
		m_exit = *pos;
		m_exit.y = map.hi.y + PAST_THE_EDGE;
	}
	if( ai->getCurLocomotor() != nullptr )
	{
		ai->getCurLocomotor()->setAllowInvalidPosition( true );
		ai->getCurLocomotor()->setUltraAccurate( true );
	}
	ai->aiMoveToPosition( &m_exit, CMD_FROM_AI );
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime RunOffMapUpdate::update()
{
	Object *me = getObject();
	if( me->isEffectivelyDead() )
		return UPDATE_SLEEP_FOREVER;
	const RunOffMapUpdateModuleData *d = getRunOffMapUpdateModuleData();
	if( m_leaving )
	{
		Region3D map;
		TheTerrainLogic->getExtentIncludingBorder( &map );
		if( !map.isInRegion( me->getPosition()->asCoord2D() ) )
		{
			TheGameLogic->destroyObject( me );
			return UPDATE_SLEEP_FOREVER;
		}
		// (an order from elsewhere that stopped it: on to the edge again)
		AIUpdateInterface *ai = me->getAI();
		if( ai != nullptr && ai->isIdle() )
			ai->aiMoveToPosition( &m_exit, CMD_FROM_AI );
		return UPDATE_SLEEP( LOGICFRAMES_PER_SECOND / 3 );
	}
	const UnsignedInt now = TheGameLogic->getFrame();
	if( ( m_triggerFrame != 0 && now >= m_triggerFrame )
		|| ( d->m_triggerOnOutOfAmmo && me->hasAnyWeapon() && me->isOutOfAmmo() ) )
	{
		leave();
		return m_leaving && !me->isEffectivelyDead() ? UPDATE_SLEEP( LOGICFRAMES_PER_SECOND / 3 ) : UPDATE_SLEEP_FOREVER;
	}
	return UPDATE_SLEEP( max( (UnsignedInt)1, d->m_refreshDelay ) );
}

//-------------------------------------------------------------------------------------------------
void RunOffMapUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void RunOffMapUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	xfer->xferUnsignedInt( &m_triggerFrame );
	xfer->xferBool( &m_leaving );
	xfer->xferCoord3D( &m_exit );
}

//-------------------------------------------------------------------------------------------------
void RunOffMapUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
