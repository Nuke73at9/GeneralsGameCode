/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: LifetimeUpdate.cpp ///////////////////////////////////////////////////////////////////////
// Author: Colin Day, December 2001
// Desc:   Update that will count down a lifetime and destroy object when it reaches zero
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_DEATH_NAMES	// Tiberium Project: for the DeathType field
#include "Common/RandomValue.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/LifetimeUpdate.h"
#include "GameLogic/Object.h"


//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature the death type by name
void LifetimeUpdateModuleData::parseDeathType( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	INI::parseIndexList( ini, instance, store, TheDeathNames );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
LifetimeUpdate::LifetimeUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	const LifetimeUpdateModuleData* d = getLifetimeUpdateModuleData();
	m_dieFrame = 0;
	UnsignedInt delay;
	if( getObject()->isKindOf( KINDOF_HULK ) && TheGameLogic->getHulkMaxLifetimeOverride() != -1 )
	{
		delay = calcSleepDelay( TheGameLogic->getHulkMaxLifetimeOverride(), TheGameLogic->getHulkMaxLifetimeOverride() );
	}
	else
	{
		delay = calcSleepDelay(d->m_minFrames, d->m_maxFrames);
	}

	setWakeFrame(getObject(), UPDATE_SLEEP(delay));
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
LifetimeUpdate::~LifetimeUpdate()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void LifetimeUpdate::setLifetimeRange( UnsignedInt minFrames, UnsignedInt maxFrames )
{
	UnsignedInt delay = calcSleepDelay(minFrames, maxFrames);
	setWakeFrame(getObject(), UPDATE_SLEEP(delay));
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature the object's life starts over from now, as long as its module gives (a renewed
// laser: RenewLaser)
//-------------------------------------------------------------------------------------------------
void LifetimeUpdate::renewLifetime()
{
	const LifetimeUpdateModuleData* d = getLifetimeUpdateModuleData();
	setLifetimeRange( d->m_minFrames, d->m_maxFrames );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UnsignedInt LifetimeUpdate::calcSleepDelay(UnsignedInt minFrames, UnsignedInt maxFrames)
{
	UnsignedInt delay = GameLogicRandomValue( minFrames, maxFrames );
	if (delay < 1) delay = 1;
	m_dieFrame = TheGameLogic->getFrame() + delay;
	return delay;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime LifetimeUpdate::update()
{
	// Tiberium Project @feature not while the object or its player has one of its ForbiddenUpgrade (C&C4's husk is kept
	// by its player's Backup Systems, a neutral one is not): it looks again in a second
	const std::vector<AsciiString>& forbidden = getLifetimeUpdateModuleData()->m_forbiddenUpgrades;
	for( std::vector<AsciiString>::const_iterator it = forbidden.begin(); it != forbidden.end(); ++it )
	{
		if( getObject()->hasUpgradeNamed( *it ) )
			return UPDATE_SLEEP( LOGICFRAMES_PER_SECOND );
	}

	// Kill (NOT destroy) if time is up
	// Tiberium Project @feature with the module's death type (DEATH_NORMAL unless given)
	getObject()->kill( DAMAGE_UNRESISTABLE, getLifetimeUpdateModuleData()->m_deathType );
	return UPDATE_SLEEP_FOREVER;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void LifetimeUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void LifetimeUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// die frame
	xfer->xferUnsignedInt( &m_dieFrame );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void LifetimeUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
