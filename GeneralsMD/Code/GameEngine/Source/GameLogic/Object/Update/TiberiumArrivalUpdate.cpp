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
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/TiberiumTest.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameLogic/AI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TiberiumZones.h"
#include "GameLogic/Weapon.h"

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
	m_flareTime = 0;
	m_flareFX = nullptr;
	m_podTime = 0;
	m_podFX = nullptr;
	m_landWeapon = nullptr;
	m_busyTime = 0;
	m_facing = 0.0f;
	m_hasFacing = FALSE;
}

//-------------------------------------------------------------------------------------------------
static void parseFacing( INI *ini, void *instance, void *, const void * )
{
	TiberiumArrivalUpdateModuleData *d = (TiberiumArrivalUpdateModuleData *)instance;
	d->m_facing = INI::scanReal( ini->getNextToken() ) * ( PI / 180.0f );
	d->m_hasFacing = TRUE;
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
		{ "FlareTime",		INI::parseDurationUnsignedInt,	nullptr,			offsetof( TiberiumArrivalUpdateModuleData, m_flareTime ) },
		{ "FlareFX",			INI::parseFXList,								nullptr,			offsetof( TiberiumArrivalUpdateModuleData, m_flareFX ) },
		{ "PodObject",		INI::parseAsciiString,					nullptr,			offsetof( TiberiumArrivalUpdateModuleData, m_podObject ) },
		{ "PodTime",			INI::parseDurationUnsignedInt,	nullptr,			offsetof( TiberiumArrivalUpdateModuleData, m_podTime ) },
		{ "PodFX",				INI::parseFXList,								nullptr,			offsetof( TiberiumArrivalUpdateModuleData, m_podFX ) },
		{ "LandWeapon",		INI::parseWeaponTemplate,				nullptr,			offsetof( TiberiumArrivalUpdateModuleData, m_landWeapon ) },
		{ "BusyTime",			INI::parseDurationUnsignedInt,	nullptr,			offsetof( TiberiumArrivalUpdateModuleData, m_busyTime ) },
		{ "Facing",				parseFacing,										nullptr,			0 },
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
	m_placed = FALSE;
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumArrivalUpdate::isArriving() const
{
	if( m_stage == STAGE_START )
		return m_placed;
	return m_stage == STAGE_ARRIVING || m_stage == STAGE_FLARE || m_stage == STAGE_POD;
}

//-------------------------------------------------------------------------------------------------
TiberiumArrivalUpdate::~TiberiumArrivalUpdate()
{
}

//-------------------------------------------------------------------------------------------------
/** C&C4's chain (a flare or a pod) rather than the unit's own drop or drill */
Bool TiberiumArrivalUpdate::usesChain() const
{
	const TiberiumArrivalUpdateModuleData *d = getTiberiumArrivalUpdateModuleData();
	return d->m_flareTime > 0 || d->m_podObject.isNotEmpty();
}

//-------------------------------------------------------------------------------------------------
/** not seen, selected, targeted or run into while it is yet to come out */
void TiberiumArrivalUpdate::setUnseen( Bool unseen )
{
	Object *me = getObject();
	const ObjectStatusMaskType mask = MAKE_OBJECT_STATUS_MASK3( OBJECT_STATUS_NO_COLLISIONS, OBJECT_STATUS_MASKED, OBJECT_STATUS_UNSELECTABLE );
	if( unseen )
		me->setStatus( mask );
	else
		me->clearStatus( mask );
	Drawable *draw = me->getDrawable();
	if( draw != nullptr )
		draw->setDrawableHidden( unseen );
}

//-------------------------------------------------------------------------------------------------
/** the unit comes out of its pod: seen again, facing the way C&C4 sets it, the spot's enemies killed (its
	weapon), held for the busy time */
void TiberiumArrivalUpdate::comeOut()
{
	const TiberiumArrivalUpdateModuleData *d = getTiberiumArrivalUpdateModuleData();
	Object *me = getObject();
	const UnsignedInt now = TheGameLogic->getFrame();
	setUnseen( FALSE );
	if( d->m_hasFacing )
		me->setOrientation( d->m_facing );
	if( d->m_landWeapon != nullptr )
		TheWeaponStore->createAndFireTempWeapon( d->m_landWeapon, me, me->getPosition() );
	if( d->m_landFX != nullptr )
		FXList::doFXObj( d->m_landFX, me );
	if( d->m_busyTime > 0 )
		me->setDisabledUntil( DISABLED_HELD, now + d->m_busyTime );
	else
		me->clearDisabled( DISABLED_HELD );
	m_stage = STAGE_DONE;
	if( TiberiumTest::isActive() )
		TiberiumTest::log( "frame %u: arrival: %s comes out at %.0f %.0f facing %.0f, busy until %u", now, me->getTemplate()->getName().str(),
			me->getPosition()->x, me->getPosition()->y, me->getOrientation() * 180.0f / PI, now + d->m_busyTime );
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
		const Player *player = me->getControllingPlayer();
		Coord3D spot = *me->getPosition();
		if( m_placed )
		{
			// Tiberium Project @feature made at its spot (C&C4's match start, a redeployment): it arrives there
			m_placed = FALSE;
			if( me->isEffectivelyDead() )
				return UPDATE_SLEEP_FOREVER;
		}
		else
		{
			// only what a factory made, in a game with zones
			if( !TiberiumZones::isOn() || me->getProducerID() == INVALID_ID || me->isEffectivelyDead() )
				return UPDATE_SLEEP_FOREVER;
			const Object *factory = TheGameLogic->findObjectByID( me->getProducerID() );
			const Coord3D *towards = me->getPosition();
			if( factory != nullptr )
			{
				towards = factory->getPosition();
				const ExitInterface *exit = factory->getObjectExitInterface();
				if( exit != nullptr && exit->getRallyPoint() != nullptr )
					towards = exit->getRallyPoint();
			}
			if( !TiberiumZones::findArrival( player, towards, &spot ) )
				return UPDATE_SLEEP_FOREVER;
		}
		me->setPosition( &spot );
		if( me->getAI() != nullptr )
			me->getAI()->aiIdle( CMD_FROM_AI );
		m_style = d->m_style;
		if( m_style == TIBERIUM_ARRIVAL_AUTO )
		{
			const Bool nod = player != nullptr && ( player->getSide().compareNoCase( "China" ) == 0 || player->getBaseSide().compareNoCase( "China" ) == 0 );
			m_style = nod ? TIBERIUM_ARRIVAL_DRILL : TIBERIUM_ARRIVAL_DROP;
		}
		if( usesChain() )
		{
			// Tiberium Project @feature C&C4's chain: unseen at its spot through the flare and the pod (held until it
			// comes out, and the busy time after)
			setUnseen( TRUE );
			m_startFrame = now;
			m_endFrame = now + d->m_flareTime;
			m_stage = STAGE_FLARE;
			if( TiberiumTest::isActive() )
				TiberiumTest::log( "frame %u: arrival: %s flare at %.0f %.0f until %u", now, me->getTemplate()->getName().str(), spot.x, spot.y, m_endFrame );
			me->setDisabledUntil( DISABLED_HELD, FOREVER );
			if( d->m_startFX != nullptr )
				FXList::doFXPos( d->m_startFX, &spot );
			if( d->m_flareFX != nullptr )
				FXList::doFXPos( d->m_flareFX, &spot );
			return UPDATE_SLEEP_NONE;
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

	if( m_stage == STAGE_FLARE )
	{
		if( now < m_endFrame )
			return UPDATE_SLEEP_NONE;
		// the flare is over: the pod, the player's, at the spot facing as the unit will
		const ThingTemplate *pod = d->m_podObject.isNotEmpty() ? TheThingFactory->findTemplate( d->m_podObject ) : nullptr;
		Player *player = me->getControllingPlayer();
		if( pod != nullptr && player != nullptr && player->getDefaultTeam() != nullptr )
		{
			Object *made = TheThingFactory->newObject( pod, player->getDefaultTeam() );
			if( made != nullptr )
			{
				made->setPosition( me->getPosition() );
				made->setOrientation( d->m_hasFacing ? d->m_facing : me->getOrientation() );
				if( TiberiumTest::isActive() )
					TiberiumTest::log( "frame %u: arrival: pod %s made for %s", now, pod->getName().str(), me->getTemplate()->getName().str() );
			}
		}
		if( d->m_podFX != nullptr )
			FXList::doFXPos( d->m_podFX, me->getPosition() );
		m_startFrame = now;
		m_endFrame = now + d->m_podTime;
		m_stage = STAGE_POD;
		if( d->m_podTime == 0 )
		{
			comeOut();
			return UPDATE_SLEEP_FOREVER;
		}
		return UPDATE_SLEEP_NONE;
	}

	if( m_stage == STAGE_POD )
	{
		if( now < m_endFrame )
			return UPDATE_SLEEP_NONE;
		comeOut();
		return UPDATE_SLEEP_FOREVER;
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
// 2: Tiberium Project: m_placed (TiberiumStart)
//-------------------------------------------------------------------------------------------------
void TiberiumArrivalUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	xfer->xferInt( &m_stage );
	xfer->xferInt( &m_style );
	xfer->xferUnsignedInt( &m_startFrame );
	xfer->xferUnsignedInt( &m_endFrame );
	if( version >= 2 )
		xfer->xferBool( &m_placed );
}

//-------------------------------------------------------------------------------------------------
void TiberiumArrivalUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	showOffset();
	// (Tiberium Project: a unit saved before it came out is unseen again: its drawable is made anew)
	if( m_stage == STAGE_FLARE || m_stage == STAGE_POD )
	{
		Drawable *draw = getObject()->getDrawable();
		if( draw != nullptr )
			draw->setDrawableHidden( TRUE );
	}
}
