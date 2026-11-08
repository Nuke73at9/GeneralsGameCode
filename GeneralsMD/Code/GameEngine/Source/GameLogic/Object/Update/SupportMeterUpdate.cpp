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

// FILE: SupportMeterUpdate.cpp ///////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's support meter (see SupportMeterUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/SupportMeterUpdate.h"

#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameLogic/Object.h"

//-------------------------------------------------------------------------------------------------
SupportMeterUpdate::SupportMeterUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
SupportMeterUpdate::~SupportMeterUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Int SupportMeterUpdate::getPointsPerMinute() const
{
	const SupportMeterUpdateModuleData *data = getSupportMeterUpdateModuleData();
	if( data->m_fasterWithUpgrade.isNotEmpty() && TheUpgradeCenter != nullptr )
	{
		const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade( data->m_fasterWithUpgrade );
		const Object *me = getObject();
		const Player *player = me->getControllingPlayer();
		if( upgrade != nullptr && ( me->hasUpgrade( upgrade ) || ( player != nullptr && player->hasUpgradeComplete( upgrade ) ) ) )
			return data->m_upgradedPointsPerMinute;
	}
	return data->m_pointsPerMinute;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime SupportMeterUpdate::update()
{
	Object *me = getObject();
	Player *player = me->getControllingPlayer();
	if( player == nullptr || me->isEffectivelyDead() || me->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		return UPDATE_SLEEP_NONE;
	player->feedSupportMeter( getPointsPerMinute(), getSupportMeterUpdateModuleData()->m_maxPoints );
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
namespace
{
	struct OtherSource
	{
		const Object *me;
		Bool found;
	};

	void findOtherSource( Object *obj, void *userData )
	{
		OtherSource *info = (OtherSource *)userData;
		if( info->found || obj == info->me || obj->isEffectivelyDead() )
			return;
		static const NameKeyType key = NAMEKEY( "SupportMeterUpdate" );
		if( obj->findUpdateModule( key ) != nullptr )
			info->found = TRUE;
	}
}

//-------------------------------------------------------------------------------------------------
/** the last of the player's sources lost: the meter empties */
void SupportMeterUpdate::onDie( const DamageInfo *damageInfo )
{
	Object *me = getObject();
	Player *player = me->getControllingPlayer();
	if( player == nullptr )
		return;
	OtherSource info;
	info.me = me;
	info.found = FALSE;
	player->iterateObjects( findOtherSource, &info );
	if( !info.found )
		player->emptySupportMeter();
}

//-------------------------------------------------------------------------------------------------
void SupportMeterUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void SupportMeterUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	UpdateModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void SupportMeterUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
