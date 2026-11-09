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


// FILE: FireGroupCenterUpdate.cpp ////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's FireGroupCenterUpdate: fires that come together make a firestorm (see the header)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/FireGroupCenterUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

//-------------------------------------------------------------------------------------------------
FireGroupCenterUpdateModuleData::FireGroupCenterUpdateModuleData()
{
	m_weaponTemplate = nullptr;
	m_range = 100.0f;
	m_requiredCount = 1;
	m_samePlayer = TRUE;
	m_killContributing = TRUE;
	m_killOwner = TRUE;
	m_scanFrames = LOGICFRAMES_PER_SECOND / 4;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void FireGroupCenterUpdateModuleData::buildFieldParse( MultiIniFieldParse &p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "Weapon",						INI::parseWeaponTemplate,				nullptr, offsetof( FireGroupCenterUpdateModuleData, m_weaponTemplate ) },
		{ "Range",						INI::parseReal,									nullptr, offsetof( FireGroupCenterUpdateModuleData, m_range ) },
		{ "RequiredCount",		INI::parseUnsignedInt,					nullptr, offsetof( FireGroupCenterUpdateModuleData, m_requiredCount ) },
		{ "KindOf",						KindOfMaskType::parseFromINI,		nullptr, offsetof( FireGroupCenterUpdateModuleData, m_kindOf ) },
		{ "SamePlayer",				INI::parseBool,									nullptr, offsetof( FireGroupCenterUpdateModuleData, m_samePlayer ) },
		{ "KillContributing",	INI::parseBool,									nullptr, offsetof( FireGroupCenterUpdateModuleData, m_killContributing ) },
		{ "KillOwner",				INI::parseBool,									nullptr, offsetof( FireGroupCenterUpdateModuleData, m_killOwner ) },
		{ "ScanTime",					INI::parseDurationUnsignedInt,	nullptr, offsetof( FireGroupCenterUpdateModuleData, m_scanFrames ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
FireGroupCenterUpdate::FireGroupCenterUpdate( Thing *thing, const ModuleData *moduleData ) :
	UpdateModule( thing, moduleData ),
	m_triggered( FALSE )
{
}

//-------------------------------------------------------------------------------------------------
FireGroupCenterUpdate::~FireGroupCenterUpdate()
{
}

//-------------------------------------------------------------------------------------------------
// the nearest RequiredCount others near enough make the group; this one goes to its centre and fires
//-------------------------------------------------------------------------------------------------
UpdateSleepTime FireGroupCenterUpdate::update()
{
	const FireGroupCenterUpdateModuleData *data = getFireGroupCenterUpdateModuleData();
	Object *me = getObject();
	if( m_triggered || me->isEffectivelyDead() || data->m_weaponTemplate == nullptr )
		return UPDATE_SLEEP_FOREVER;
	const UnsignedInt sleep = data->m_scanFrames > 0 ? data->m_scanFrames : 1;
	const UnsignedInt needed = data->m_requiredCount > 0 ? data->m_requiredCount : 1;

	PartitionFilterAlive alive;
	PartitionFilterSamePlayer samePlayer( me->getControllingPlayer() );
	PartitionFilterRelationship allies( me, PartitionFilterRelationship::ALLOW_ALLIES );
	PartitionFilterAcceptByKindOf kinds( data->m_kindOf, KINDOFMASK_NONE );
	PartitionFilter *filters[ 4 ];
	Int n = 0;
	filters[ n++ ] = &alive;
	filters[ n++ ] = data->m_samePlayer ? (PartitionFilter *)&samePlayer : (PartitionFilter *)&allies;
	if( data->m_kindOf.any() )
		filters[ n++ ] = &kinds;
	filters[ n ] = nullptr;

	Object *group[ 16 ];
	UnsignedInt count = 0;
	{
		SimpleObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( me, data->m_range, FROM_CENTER_2D, filters, ITER_SORTED_NEAR_TO_FAR );
		MemoryPoolObjectHolder hold( iter );
		for( Object *other = iter->first(); other != nullptr && count < needed && count < 16; other = iter->next() )
		{
			if( other != me )
				group[ count++ ] = other;
		}
	}
	if( count < needed )
		return UPDATE_SLEEP( sleep );

	m_triggered = TRUE;
	// the group's centre, itself and the others: where the firestorm comes
	Coord3D centre = *me->getPosition();
	for( UnsignedInt i = 0; i < count; ++i )
	{
		centre.x += group[ i ]->getPosition()->x;
		centre.y += group[ i ]->getPosition()->y;
	}
	centre.x /= (Real)( count + 1 );
	centre.y /= (Real)( count + 1 );
	centre.z = TheTerrainLogic->getGroundHeight( centre.x, centre.y );
	me->setPosition( &centre );

	if( data->m_killContributing )
	{
		for( UnsignedInt i = 0; i < count; ++i )
		{
			if( !group[ i ]->isEffectivelyDead() )
				group[ i ]->kill();
		}
	}
	TheWeaponStore->createAndFireTempWeapon( data->m_weaponTemplate, me, me->getPosition() );
	if( data->m_killOwner && !me->isEffectivelyDead() )
		me->kill();
	return UPDATE_SLEEP_FOREVER;
}

// ------------------------------------------------------------------------------------------------
void FireGroupCenterUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void FireGroupCenterUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	xfer->xferBool( &m_triggered );
}

// ------------------------------------------------------------------------------------------------
void FireGroupCenterUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
