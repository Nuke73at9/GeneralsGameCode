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

// FILE: StealthFieldUpdate.cpp ///////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's stealth fields (see StealthFieldUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/StealthFieldUpdate.h"

#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/StealthUpdate.h"

//-------------------------------------------------------------------------------------------------
StealthFieldUpdateModuleData::StealthFieldUpdateModuleData()
{
	m_initiallyActive = FALSE;
	m_radius = 0.0f;
	m_updatePeriod = LOGICFRAMES_PER_SECOND / 2;
	SET_ALL_KINDOFMASK_BITS( m_kindOf );
	m_forbiddenKindOf.clear();
	m_affectsSelf = FALSE;
}

//-------------------------------------------------------------------------------------------------
void StealthFieldUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	static const FieldParse dataFieldParse[] =
	{
		{ "StartsActive",			INI::parseBool,									nullptr, offsetof( StealthFieldUpdateModuleData, m_initiallyActive ) },
		{ "Radius",						INI::parseReal,									nullptr, offsetof( StealthFieldUpdateModuleData, m_radius ) },
		{ "UpdatePeriod",			INI::parseDurationUnsignedInt,	nullptr, offsetof( StealthFieldUpdateModuleData, m_updatePeriod ) },
		{ "KindOf",						KindOfMaskType::parseFromINI,		nullptr, offsetof( StealthFieldUpdateModuleData, m_kindOf ) },
		{ "ForbiddenKindOf",	KindOfMaskType::parseFromINI,		nullptr, offsetof( StealthFieldUpdateModuleData, m_forbiddenKindOf ) },
		{ "AffectsSelf",			INI::parseBool,									nullptr, offsetof( StealthFieldUpdateModuleData, m_affectsSelf ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse( p );
	p.add( dataFieldParse );
	p.add( UpgradeMuxData::getFieldParse(), offsetof( StealthFieldUpdateModuleData, m_upgradeMuxData ) );
}

//-------------------------------------------------------------------------------------------------
StealthFieldUpdate::StealthFieldUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	if( getStealthFieldUpdateModuleData()->m_initiallyActive )
	{
		giveSelfUpgrade();
		setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
	}
	else
	{
		setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
	}
}

//-------------------------------------------------------------------------------------------------
StealthFieldUpdate::~StealthFieldUpdate()
{
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime StealthFieldUpdate::update()
{
	const StealthFieldUpdateModuleData *d = getStealthFieldUpdateModuleData();
	Object *me = getObject();
	const UnsignedInt period = d->m_updatePeriod > 0 ? d->m_updatePeriod : 1;
	if( !isAlreadyUpgraded() || me->isEffectivelyDead() )
		return UPDATE_SLEEP_FOREVER;
	if( me->isDisabled() || me->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) || me->testStatus( OBJECT_STATUS_SOLD ) )
		return UPDATE_SLEEP( period );

	// hidden until the next pulse is due, and a little after (so the field holds without a gap)
	const UnsignedInt frames = period + LOGICFRAMES_PER_SECOND / 3;
	PartitionFilterRelationship filterTeam( me, PartitionFilterRelationship::ALLOW_ALLIES );
	PartitionFilter *filters[] = { &filterTeam, nullptr };
	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( me->getPosition(), d->m_radius, FROM_CENTER_2D, filters );
	MemoryPoolObjectHolder hold( iter );
	for( Object *other = iter->first(); other != nullptr; other = iter->next() )
	{
		if( other == me && !d->m_affectsSelf )
			continue;
		if( other->isEffectivelyDead() || !other->isAnyKindOf( d->m_kindOf ) || other->isAnyKindOf( d->m_forbiddenKindOf ) )
			continue;
		StealthUpdate *stealth = other->getStealth();
		if( stealth == nullptr || stealth->isInnateStealth() )
			continue;
		stealth->receiveGrant( TRUE, frames );
	}
	return UPDATE_SLEEP( period );
}

//-------------------------------------------------------------------------------------------------
void StealthFieldUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void StealthFieldUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	UpgradeMux::upgradeMuxXfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void StealthFieldUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	UpgradeMux::upgradeMuxLoadPostProcess();
}
