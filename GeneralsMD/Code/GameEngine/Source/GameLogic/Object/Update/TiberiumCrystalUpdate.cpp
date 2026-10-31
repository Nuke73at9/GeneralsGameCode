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

// FILE: TiberiumCrystalUpdate.cpp ////////////////////////////////////////////////////////////////
// Tiberium Project @feature Kane's Wrath's Tiberium crystal growing back (see TiberiumCrystalUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_DEATH_NAMES
#include "GameLogic/Module/TiberiumCrystalUpdate.h"

#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/Xfer.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"

namespace
{
	const UnsignedInt LOOK_EVERY = LOGICFRAMES_PER_SECOND;

	SupplyWarehouseDockUpdate *warehouseOf( Object *obj )
	{
		static const NameKeyType key = NAMEKEY( "SupplyWarehouseDockUpdate" );
		return (SupplyWarehouseDockUpdate *)obj->findUpdateModule( key );
	}
}

//-------------------------------------------------------------------------------------------------
TiberiumCrystalUpdateModuleData::TiberiumCrystalUpdateModuleData()
{
	m_regrowDelay = LOGICFRAMES_PER_SECOND * 15;
	m_regrowRate = 0.5f;
	m_damageRadius = 0.0f;
	m_damagePerSecond = 0.0f;
	m_damageKindOf = MAKE_KINDOF_MASK( KINDOF_INFANTRY );
	m_damageType = DAMAGE_RADIATION;
	m_deathType = DEATH_POISONED;
}

//-------------------------------------------------------------------------------------------------
void TiberiumCrystalUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	static const FieldParse dataFieldParse[] =
	{
		{ "RegrowDelay",	INI::parseDurationUnsignedInt,	nullptr, offsetof( TiberiumCrystalUpdateModuleData, m_regrowDelay ) },
		{ "RegrowRate",		INI::parseReal,									nullptr, offsetof( TiberiumCrystalUpdateModuleData, m_regrowRate ) },
		{ "DamageRadius",	INI::parseReal,									nullptr, offsetof( TiberiumCrystalUpdateModuleData, m_damageRadius ) },
		{ "DamagePerSecond",	INI::parseReal,								nullptr, offsetof( TiberiumCrystalUpdateModuleData, m_damagePerSecond ) },
		{ "DamageKindOf",	KindOfMaskType::parseFromINI,		nullptr, offsetof( TiberiumCrystalUpdateModuleData, m_damageKindOf ) },
		{ "DamageType",		DamageTypeFlags::parseSingleBitFromINI,	nullptr, offsetof( TiberiumCrystalUpdateModuleData, m_damageType ) },
		{ "DeathType",		INI::parseIndexList,						TheDeathNames, offsetof( TiberiumCrystalUpdateModuleData, m_deathType ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse( p );
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
TiberiumCrystalUpdate::TiberiumCrystalUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_lastStored = -1;
	m_growFrame = 0;
	m_growth = 0.0f;
	setWakeFrame( getObject(), UPDATE_SLEEP( LOOK_EVERY ) );
}

//-------------------------------------------------------------------------------------------------
TiberiumCrystalUpdate::~TiberiumCrystalUpdate()
{
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime TiberiumCrystalUpdate::update()
{
	const TiberiumCrystalUpdateModuleData *d = getTiberiumCrystalUpdateModuleData();
	SupplyWarehouseDockUpdate *warehouse = warehouseOf( getObject() );
	if( warehouse == nullptr )
		return UPDATE_SLEEP_FOREVER;
	const UnsignedInt now = TheGameLogic->getFrame();
	if( m_lastStored < 0 )
	{
		// Tiberium is nobody's: a crystal a harvester spilled (made on its owner's team) can be
		// harvested by anyone, as a field's can
		Player *neutral = ThePlayerList != nullptr ? ThePlayerList->getNeutralPlayer() : nullptr;
		Team *team = neutral != nullptr ? neutral->getDefaultTeam() : nullptr;
		if( team != nullptr && getObject()->getTeam() != team )
			getObject()->setTeam( team );
	}
	const Int stored = warehouse->getBoxesStored();
	if( m_lastStored >= 0 && stored < m_lastStored )
	{
		// harvested since the last look: it waits before it grows again
		m_growFrame = now + d->m_regrowDelay;
		m_growth = 0.0f;
	}
	if( stored < warehouse->getMaxBoxes() && now >= m_growFrame )
	{
		m_growth += d->m_regrowRate * (Real)LOOK_EVERY / (Real)LOGICFRAMES_PER_SECOND;
		const Int whole = (Int)m_growth;
		if( whole > 0 )
		{
			m_growth -= (Real)whole;
			warehouse->regrowBoxes( whole );
		}
	}
	m_lastStored = warehouse->getBoxesStored();
	if( m_lastStored > 0 )
		hurtWhatStandsInIt();
	return UPDATE_SLEEP( LOOK_EVERY );
}

//-------------------------------------------------------------------------------------------------
void TiberiumCrystalUpdate::hurtWhatStandsInIt()
{
	const TiberiumCrystalUpdateModuleData *d = getTiberiumCrystalUpdateModuleData();
	if( d->m_damageRadius <= 0.0f || d->m_damagePerSecond <= 0.0f )
		return;
	Object *me = getObject();
	PartitionFilterAcceptByKindOf kinds( d->m_damageKindOf, KINDOFMASK_NONE );
	PartitionFilterAlive alive;
	PartitionFilter *filters[] = { &kinds, &alive, nullptr };
	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( me, d->m_damageRadius, FROM_CENTER_2D, filters, ITER_SORTED_NEAR_TO_FAR );
	MemoryPoolObjectHolder hold( iter );
	for( Object *other = iter->first(); other != nullptr; other = iter->next() )
	{
		if( other->isAirborneTarget() )
			continue;
		DamageInfo damage;
		damage.in.m_damageType = d->m_damageType;
		damage.in.m_deathType = d->m_deathType;
		damage.in.m_sourceID = me->getID();
		damage.in.m_amount = d->m_damagePerSecond * (Real)LOOK_EVERY / (Real)LOGICFRAMES_PER_SECOND;
		other->attemptDamage( &damage );
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumCrystalUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void TiberiumCrystalUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	xfer->xferInt( &m_lastStored );
	xfer->xferUnsignedInt( &m_growFrame );
	xfer->xferReal( &m_growth );
}

//-------------------------------------------------------------------------------------------------
void TiberiumCrystalUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
