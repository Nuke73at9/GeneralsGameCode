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

// FILE: TiberiumPickupUpdate.cpp /////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's crystal pickup (see TiberiumPickupUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/TiberiumPickupUpdate.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
	const UnsignedInt LOOK_EVERY = 5;		///< on the ground: how often it looks for a carrier
	const UnsignedInt DROP_OFF_EVERY = 10;	///< carried: how often it looks for a drop-off point

	/// a unit of a real player (not the neutral one, not a civilian's) that is alive and on the ground
	Bool isPlayersUnit( const Object *obj )
	{
		if( obj == nullptr || obj->isEffectivelyDead() || obj->isAirborneTarget() )
			return FALSE;
		const Player *player = obj->getControllingPlayer();
		if( player == nullptr || ThePlayerList == nullptr || player == ThePlayerList->getNeutralPlayer() )
			return FALSE;
		return player->getPlayerType() == PLAYER_HUMAN || player->getPlayerType() == PLAYER_COMPUTER;
	}
}

//-------------------------------------------------------------------------------------------------
TiberiumPickupUpdateModuleData::TiberiumPickupUpdateModuleData()
{
	m_pickupRadius = 15.0f;
	m_carrierKindOf = MAKE_KINDOF_MASK( KINDOF_INFANTRY );
	m_carrierKindOf.set( KINDOF_VEHICLE );
	m_carrierForbiddenKindOf = MAKE_KINDOF_MASK( KINDOF_AIRCRAFT );
	m_carryHeight = 25.0f;
	m_dropOffRadius = 120.0f;
	m_dropOffKindOf = MAKE_KINDOF_MASK( KINDOF_FS_SUPPLY_CENTER );
	m_carrierLevels = 1;
	m_shareRadius = 0.0f;
	m_shareLevels = 1;
	m_pickupFX = nullptr;
	m_deliverFX = nullptr;
}

//-------------------------------------------------------------------------------------------------
void TiberiumPickupUpdateModuleData::parseName( INI *ini, void *, void *store, const void * )
{
	( (std::vector<AsciiString> *)store )->push_back( AsciiString( ini->getNextToken() ) );
}

//-------------------------------------------------------------------------------------------------
void TiberiumPickupUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	static const FieldParse dataFieldParse[] =
	{
		{ "PickupRadius",						INI::parseReal,									nullptr, offsetof( TiberiumPickupUpdateModuleData, m_pickupRadius ) },
		{ "CarrierKindOf",					KindOfMaskType::parseFromINI,		nullptr, offsetof( TiberiumPickupUpdateModuleData, m_carrierKindOf ) },
		{ "CarrierForbiddenKindOf",	KindOfMaskType::parseFromINI,		nullptr, offsetof( TiberiumPickupUpdateModuleData, m_carrierForbiddenKindOf ) },
		{ "CarryHeight",						INI::parseReal,									nullptr, offsetof( TiberiumPickupUpdateModuleData, m_carryHeight ) },
		{ "DropOffRadius",					INI::parseReal,									nullptr, offsetof( TiberiumPickupUpdateModuleData, m_dropOffRadius ) },
		{ "DropOffKindOf",					KindOfMaskType::parseFromINI,		nullptr, offsetof( TiberiumPickupUpdateModuleData, m_dropOffKindOf ) },
		{ "DropOffNameContains",		parseName,											nullptr, offsetof( TiberiumPickupUpdateModuleData, m_dropOffNames ) },
		{ "CarrierLevels",					INI::parseInt,									nullptr, offsetof( TiberiumPickupUpdateModuleData, m_carrierLevels ) },
		{ "ShareRadius",						INI::parseReal,									nullptr, offsetof( TiberiumPickupUpdateModuleData, m_shareRadius ) },
		{ "ShareLevels",						INI::parseInt,									nullptr, offsetof( TiberiumPickupUpdateModuleData, m_shareLevels ) },
		{ "PickupFX",								INI::parseFXList,								nullptr, offsetof( TiberiumPickupUpdateModuleData, m_pickupFX ) },
		{ "DeliverFX",							INI::parseFXList,								nullptr, offsetof( TiberiumPickupUpdateModuleData, m_deliverFX ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse( p );
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
TiberiumPickupUpdate::TiberiumPickupUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_carrier = INVALID_ID;
	setWakeFrame( getObject(), UPDATE_SLEEP( LOOK_EVERY ) );
}

//-------------------------------------------------------------------------------------------------
TiberiumPickupUpdate::~TiberiumPickupUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Object *TiberiumPickupUpdate::findCarrier() const
{
	const TiberiumPickupUpdateModuleData *d = getTiberiumPickupUpdateModuleData();
	PartitionFilterAlive alive;
	PartitionFilter *filters[] = { &alive, nullptr };
	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( getObject(), d->m_pickupRadius, FROM_BOUNDINGSPHERE_2D, filters, ITER_SORTED_NEAR_TO_FAR );
	MemoryPoolObjectHolder hold( iter );
	for( Object *other = iter->first(); other != nullptr; other = iter->next() )
	{
		if( !other->isAnyKindOf( d->m_carrierKindOf ) || other->isAnyKindOf( d->m_carrierForbiddenKindOf ) )
			continue;
		if( isPlayersUnit( other ) )
			return other;
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
Object *TiberiumPickupUpdate::findDropOff( Object *carrier ) const
{
	const TiberiumPickupUpdateModuleData *d = getTiberiumPickupUpdateModuleData();
	PartitionFilterSamePlayer mine( carrier->getControllingPlayer() );
	PartitionFilterAlive alive;
	PartitionFilter *filters[] = { &mine, &alive, nullptr };
	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( carrier, d->m_dropOffRadius, FROM_BOUNDINGSPHERE_2D, filters, ITER_SORTED_NEAR_TO_FAR );
	MemoryPoolObjectHolder hold( iter );
	for( Object *other = iter->first(); other != nullptr; other = iter->next() )
	{
		if( other == carrier || other->isKindOf( KINDOF_PROJECTILE ) || other->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION )
			|| other->testStatus( OBJECT_STATUS_SOLD ) )
			continue;
		if( other->isAnyKindOf( d->m_dropOffKindOf ) )
			return other;
		const AsciiString &name = other->getTemplate()->getName();
		for( size_t i = 0; i < d->m_dropOffNames.size(); ++i )
		{
			if( !d->m_dropOffNames[i].isEmpty() && strstr( name.str(), d->m_dropOffNames[i].str() ) != nullptr )
				return other;
		}
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
void TiberiumPickupUpdate::deliver( Object *carrier )
{
	const TiberiumPickupUpdateModuleData *d = getTiberiumPickupUpdateModuleData();
	if( d->m_carrierLevels > 0 && carrier->getExperienceTracker() != nullptr && carrier->getExperienceTracker()->isTrainable() )
		carrier->getExperienceTracker()->gainExpForLevel( d->m_carrierLevels, FALSE );
	if( d->m_shareRadius > 0.0f && d->m_shareLevels > 0 )
	{
		PartitionFilterSamePlayer mine( carrier->getControllingPlayer() );
		PartitionFilterAlive alive;
		PartitionFilter *filters[] = { &mine, &alive, nullptr };
		ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( carrier, d->m_shareRadius, FROM_CENTER_2D, filters, ITER_SORTED_NEAR_TO_FAR );
		MemoryPoolObjectHolder hold( iter );
		for( Object *other = iter->first(); other != nullptr; other = iter->next() )
		{
			if( other == carrier || !other->isAnyKindOf( d->m_carrierKindOf ) || other->isAnyKindOf( d->m_carrierForbiddenKindOf ) )
				continue;
			ExperienceTracker *xp = other->getExperienceTracker();
			if( xp != nullptr && xp->isTrainable() )
				xp->gainExpForLevel( d->m_shareLevels, FALSE );
		}
	}
	FXList::doFXObj( d->m_deliverFX, carrier );
	m_carrier = INVALID_ID;
	TheGameLogic->destroyObject( getObject() );
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime TiberiumPickupUpdate::update()
{
	const TiberiumPickupUpdateModuleData *d = getTiberiumPickupUpdateModuleData();
	Object *me = getObject();

	if( m_carrier == INVALID_ID )
	{
		Object *carrier = findCarrier();
		if( carrier == nullptr )
			return UPDATE_SLEEP( LOOK_EVERY );
		m_carrier = carrier->getID();
		FXList::doFXObj( d->m_pickupFX, carrier );
	}

	Object *carrier = TheGameLogic->findObjectByID( m_carrier );
	if( carrier == nullptr || carrier->isEffectivelyDead() )
	{
		// the carrier fell: the pickup lies where it stood
		m_carrier = INVALID_ID;
		Coord3D pos = *me->getPosition();
		pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );
		me->setPosition( &pos );
		return UPDATE_SLEEP( LOOK_EVERY );
	}

	// carried, above the carrier
	Coord3D pos = *carrier->getPosition();
	pos.z += d->m_carryHeight;
	me->setPosition( &pos );

	if( TheGameLogic->getFrame() % DROP_OFF_EVERY == 0 )
	{
		if( findDropOff( carrier ) != nullptr )
		{
			deliver( carrier );
			return UPDATE_SLEEP_FOREVER;
		}
	}
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumPickupUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void TiberiumPickupUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	xfer->xferObjectID( &m_carrier );
}

//-------------------------------------------------------------------------------------------------
void TiberiumPickupUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
