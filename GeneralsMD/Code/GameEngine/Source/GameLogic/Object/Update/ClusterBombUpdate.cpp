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

// FILE: ClusterBombUpdate.cpp ////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's ClusterBombUpdate (see ClusterBombUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/ClusterBombUpdate.h"

#include <algorithm>

#include "Common/RandomValue.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

//-------------------------------------------------------------------------------------------------
ClusterBombUpdateModuleData::ClusterBombUpdateModuleData()
{
	m_weapon = nullptr;
	m_initialWeapon = nullptr;
	m_radius = 0.0f;
	m_numBomblets = 0;
	m_minDelay = 0;
	m_maxDelay = 0;
	m_bombletFX = nullptr;
}

//-------------------------------------------------------------------------------------------------
void ClusterBombUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	static const FieldParse dataFieldParse[] =
	{
		{ "Weapon",					INI::parseWeaponTemplate,				nullptr, offsetof( ClusterBombUpdateModuleData, m_weapon ) },
		{ "InitialWeapon",	INI::parseWeaponTemplate,				nullptr, offsetof( ClusterBombUpdateModuleData, m_initialWeapon ) },
		{ "Radius",					INI::parseReal,									nullptr, offsetof( ClusterBombUpdateModuleData, m_radius ) },
		{ "NumBomblets",		INI::parseInt,									nullptr, offsetof( ClusterBombUpdateModuleData, m_numBomblets ) },
		{ "MinDelay",				INI::parseDurationUnsignedInt,	nullptr, offsetof( ClusterBombUpdateModuleData, m_minDelay ) },
		{ "MaxDelay",				INI::parseDurationUnsignedInt,	nullptr, offsetof( ClusterBombUpdateModuleData, m_maxDelay ) },
		{ "BombletFX",			INI::parseFXList,								nullptr, offsetof( ClusterBombUpdateModuleData, m_bombletFX ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse( p );
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
ClusterBombUpdate::ClusterBombUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_started = FALSE;
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
ClusterBombUpdate::~ClusterBombUpdate()
{
}

//-------------------------------------------------------------------------------------------------
/** One bomblet: the weapon fired at a random spot within the radius, its effect played there */
void ClusterBombUpdate::fireBomblet()
{
	const ClusterBombUpdateModuleData *d = getClusterBombUpdateModuleData();
	Object *me = getObject();
	Coord3D spot = *me->getPosition();
	if( d->m_radius > 0.0f )
	{
		// evenly over the disc (the square root of a share of the radius squared)
		const Real angle = GameLogicRandomValueReal( 0.0f, 2.0f * PI );
		const Real dist = d->m_radius * WWMath::Sqrt( GameLogicRandomValueReal( 0.0f, 1.0f ) );
		spot.x += dist * WWMath::Cos( angle );
		spot.y += dist * WWMath::Sin( angle );
	}
	spot.z = TheTerrainLogic->getGroundHeight( spot.x, spot.y );
	if( d->m_weapon != nullptr )
		TheWeaponStore->createAndFireTempWeapon( d->m_weapon, me, &spot );
	if( d->m_bombletFX != nullptr )
		FXList::doFXPos( d->m_bombletFX, &spot );
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime ClusterBombUpdate::update()
{
	const ClusterBombUpdateModuleData *d = getClusterBombUpdateModuleData();
	Object *me = getObject();
	const UnsignedInt now = TheGameLogic->getFrame();
	if( !m_started )
	{
		// each bomblet's time, from now
		m_started = TRUE;
		const UnsignedInt lo = min( d->m_minDelay, d->m_maxDelay ), hi = max( d->m_minDelay, d->m_maxDelay );
		for( Int i = 0; i < d->m_numBomblets; ++i )
			m_frames.push_back( now + (UnsignedInt)GameLogicRandomValue( (Int)lo, (Int)hi ) );
		std::sort( m_frames.begin(), m_frames.end() );
		if( d->m_initialWeapon != nullptr )
			TheWeaponStore->createAndFireTempWeapon( d->m_initialWeapon, me, me->getPosition() );
	}
	while( !m_frames.empty() && m_frames.front() <= now )
	{
		m_frames.erase( m_frames.begin() );
		fireBomblet();
	}
	if( m_frames.empty() )
		return UPDATE_SLEEP_FOREVER;
	return UPDATE_SLEEP( m_frames.front() > now ? m_frames.front() - now : 1 );
}

//-------------------------------------------------------------------------------------------------
void ClusterBombUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void ClusterBombUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	xfer->xferBool( &m_started );
	UnsignedShort count = (UnsignedShort)m_frames.size();
	xfer->xferUnsignedShort( &count );
	if( xfer->getXferMode() == XFER_LOAD )
		m_frames.resize( count );
	for( UnsignedShort i = 0; i < count; ++i )
		xfer->xferUnsignedInt( &m_frames[i] );
}

//-------------------------------------------------------------------------------------------------
void ClusterBombUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
