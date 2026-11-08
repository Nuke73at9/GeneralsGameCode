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

// FILE: StrafeAreaUpdate.cpp /////////////////////////////////////////////////////////////////////
// Tiberium Project @feature Kane's Wrath's strafing runs of called-in aircraft (see StrafeAreaUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/StrafeAreaUpdate.h"

#include "Common/NameKeyGenerator.h"
#include "Common/RandomValue.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AIUpdate.h"

namespace
{
	const Real PAST_THE_EDGE = 200.0f;
}

//-------------------------------------------------------------------------------------------------
StrafeAreaUpdate::StrafeAreaUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_state = STRAFE_IDLE;
	m_target.zero();
	m_dir.zero();
	m_nextShotFrame = 0;
	m_startFrame = 0;
	m_shotsFired = 0;
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
StrafeAreaUpdate::~StrafeAreaUpdate()
{
}

//-------------------------------------------------------------------------------------------------
StrafeAreaUpdate *StrafeAreaUpdate::find( Object *obj )
{
	static const NameKeyType key = NAMEKEY( "StrafeAreaUpdate" );
	return obj != nullptr ? (StrafeAreaUpdate *)obj->findUpdateModule( key ) : nullptr;
}

//-------------------------------------------------------------------------------------------------
void StrafeAreaUpdate::strafe( const Coord3D *target )
{
	Object *me = getObject();
	AIUpdateInterface *ai = me->getAI();
	if( ai == nullptr || target == nullptr )
		return;
	m_target = *target;
	const Coord3D *pos = me->getPosition();
	Real dx = m_target.x - pos->x, dy = m_target.y - pos->y;
	const Real len = WWMath::Sqrt( dx * dx + dy * dy );
	if( len > 0.1f )
	{
		dx /= len;
		dy /= len;
	}
	else
	{
		dx = 1.0f;
		dy = 0.0f;
	}
	m_dir.set( dx, dy, 0.0f );
	m_state = STRAFE_RUN;
	m_startFrame = TheGameLogic->getFrame();
	m_nextShotFrame = 0;
	m_shotsFired = 0;
	// over the spot and on: it turns away only once past (it may start beyond the playable area)
	const StrafeAreaUpdateModuleData *data = getStrafeAreaUpdateModuleData();
	Coord3D through = m_target;
	through.x += dx * ( data->m_radius + data->m_preattackDistance );
	through.y += dy * ( data->m_radius + data->m_preattackDistance );
	if( ai->getCurLocomotor() != nullptr )
		ai->getCurLocomotor()->setAllowInvalidPosition( true );
	ai->aiMoveToPosition( &through, CMD_FROM_AI );
	me->setSelectable( FALSE );
	setWakeFrame( me, UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
/** on along its run, off the map */
void StrafeAreaUpdate::leave()
{
	Object *me = getObject();
	AIUpdateInterface *ai = me->getAI();
	m_state = STRAFE_LEAVING;
	if( ai == nullptr )
		return;
	Region3D map;
	TheTerrainLogic->getExtentIncludingBorder( &map );
	// as far along its heading as the map's far side, and past the border
	const Coord3D *pos = me->getPosition();
	const Real span = ( map.hi.x - map.lo.x ) + ( map.hi.y - map.lo.y ) + PAST_THE_EDGE * 2.0f;
	Coord3D to = *pos;
	to.x += m_dir.x * span;
	to.y += m_dir.y * span;
	to.x = max( map.lo.x - PAST_THE_EDGE, min( map.hi.x + PAST_THE_EDGE, to.x ) );
	to.y = max( map.lo.y - PAST_THE_EDGE, min( map.hi.y + PAST_THE_EDGE, to.y ) );
	if( ai->getCurLocomotor() != nullptr )
	{
		ai->getCurLocomotor()->setAllowInvalidPosition( true );
		ai->getCurLocomotor()->setUltraAccurate( true );
	}
	ai->aiMoveToPosition( &to, CMD_FROM_AI );
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime StrafeAreaUpdate::update()
{
	Object *me = getObject();
	if( m_state == STRAFE_IDLE || me->isEffectivelyDead() )
		return UPDATE_SLEEP_FOREVER;
	const StrafeAreaUpdateModuleData *data = getStrafeAreaUpdateModuleData();
	const UnsignedInt now = TheGameLogic->getFrame();
	const Coord3D *pos = me->getPosition();
	AIUpdateInterface *ai = me->getAI();

	if( m_state == STRAFE_LEAVING )
	{
		Region3D map;
		TheTerrainLogic->getExtentIncludingBorder( &map );
		if( !map.isInRegion( pos->asCoord2D() ) )
		{
			TheGameLogic->destroyObject( me );
			return UPDATE_SLEEP_FOREVER;
		}
		if( ai != nullptr && ai->isIdle() )
			leave();
		return UPDATE_SLEEP( LOGICFRAMES_PER_SECOND / 3 );
	}

	// how far along its run it is, the spot at 0
	const Real along = ( pos->x - m_target.x ) * m_dir.x + ( pos->y - m_target.y ) * m_dir.y;
	// (a weapon with a clip fires one clip a run: the bombing run's Orcas their two bombs, not a stream of them)
	const Bool ammo = data->m_weapon != nullptr && ( data->m_weapon->getClipSize() <= 0 || m_shotsFired < data->m_weapon->getClipSize() );
	if( along >= -data->m_preattackDistance && along <= data->m_radius && ammo && now >= m_nextShotFrame )
	{
		// a point around the spot (the logic's own random numbers: the same on every machine)
		const Real reach = data->m_radius + data->m_targetJitter;
		Coord3D at = m_target;
		const Real angle = GameLogicRandomValueReal( 0.0f, 2.0f * PI );
		const Real r = GameLogicRandomValueReal( 0.0f, reach );
		at.x += r * WWMath::Cos( angle );
		at.y += r * WWMath::Sin( angle );
		at.z = TheTerrainLogic->getGroundHeight( at.x, at.y );
		TheWeaponStore->createAndFireTempWeapon( data->m_weapon, me, &at );
		++m_shotsFired;
		WeaponBonus bonus;
		const UnsignedInt delay = (UnsignedInt)max( 1, data->m_weapon->getDelayBetweenShots( bonus ) );
		m_nextShotFrame = now + max( delay, data->m_sweepFrequency );
	}
	// past the spot (or stopped short of it, or too long about it): away
	const UnsignedInt tooLong = LOGICFRAMES_PER_SECOND * 60;
	if( along > data->m_radius || ( ai != nullptr && ai->isIdle() && now > m_startFrame + LOGICFRAMES_PER_SECOND ) || now > m_startFrame + tooLong )
		leave();
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
void StrafeAreaUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
// 2: the run's shots
//-------------------------------------------------------------------------------------------------
void StrafeAreaUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	UpdateModule::xfer( xfer );
	xfer->xferInt( &m_state );
	xfer->xferCoord3D( &m_target );
	xfer->xferCoord3D( &m_dir );
	xfer->xferUnsignedInt( &m_nextShotFrame );
	xfer->xferUnsignedInt( &m_startFrame );
	if( version >= 2 )
		xfer->xferInt( &m_shotsFired );
}

//-------------------------------------------------------------------------------------------------
void StrafeAreaUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
