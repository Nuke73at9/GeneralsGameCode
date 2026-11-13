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

// FILE: MultiConnectionDamageUpdate.cpp //////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's Spider Tanks' web (see the header)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/LaserUpdate.h"
#include "GameLogic/Module/LifetimeUpdate.h"
#include "GameLogic/Module/MultiConnectionDamageUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"

//-------------------------------------------------------------------------------------------------
MultiConnectionDamageUpdateModuleData::MultiConnectionDamageUpdateModuleData()
{
	m_maxConnections = 9;
	m_maxRange = 0.0f;
	m_weapon = nullptr;
	m_lineWidth = 0.0f;
	m_damageDelay = LOGICFRAMES_PER_SECOND / 2;
	m_rescanDelay = 3 * LOGICFRAMES_PER_SECOND;
	m_updateDelay = LOGICFRAMES_PER_SECOND / 2;
	m_hitsNeutrals = FALSE;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void MultiConnectionDamageUpdateModuleData::buildFieldParse( MultiIniFieldParse &p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "Partner",					INI::parseAsciiStringVector,				nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_partnerNames ) },
		{ "MaxConnections",		INI::parseInt,											nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_maxConnections ) },
		{ "MaxRange",					INI::parseReal,											nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_maxRange ) },
		{ "Weapon",						INI::parseWeaponTemplate,						nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_weapon ) },
		{ "LineWidth",				INI::parseReal,											nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_lineWidth ) },
		{ "DamageDelay",			INI::parseDurationUnsignedInt,			nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_damageDelay ) },
		{ "RescanDelay",			INI::parseDurationUnsignedInt,			nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_rescanDelay ) },
		{ "UpdateDelay",			INI::parseDurationUnsignedInt,			nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_updateDelay ) },
		{ "VictimKindOf",			KindOfMaskType::parseFromINI,				nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_victimKindOf ) },
		{ "HitsNeutrals",			INI::parseBool,											nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_hitsNeutrals ) },
		{ "ForbiddenStatus",	ObjectStatusMaskType::parseFromINI,	nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_forbiddenStatus ) },
		{ "LaserName",				INI::parseAsciiString,							nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_laserName ) },
		{ "BeamStartBone",		INI::parseAsciiString,							nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_beamStartBone ) },
		{ "BeamEndBone",			INI::parseAsciiString,							nullptr, offsetof( MultiConnectionDamageUpdateModuleData, m_beamEndBone ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
MultiConnectionDamageUpdate::MultiConnectionDamageUpdate( Thing *thing, const ModuleData *moduleData ) :
	UpdateModule( thing, moduleData ),
	m_nextRescanFrame( 0 ),
	m_nextDamageFrame( 0 ),
	m_shotCount( 0 )
{
	// (each on its own frame of the delay, by its id: a column of them made together looks on different frames)
	const UnsignedInt step = max( 1u, getMultiConnectionDamageUpdateModuleData()->m_updateDelay );
	setWakeFrame( getObject(), UPDATE_SLEEP( 1 + (UnsignedInt)getObject()->getID() % step ) );
}

//-------------------------------------------------------------------------------------------------
MultiConnectionDamageUpdate::~MultiConnectionDamageUpdate()
{
}

//-------------------------------------------------------------------------------------------------
// standing in the world, whole, and with none of the statuses that forbid it
//-------------------------------------------------------------------------------------------------
Bool MultiConnectionDamageUpdate::canLink( const Object *obj ) const
{
	const MultiConnectionDamageUpdateModuleData *data = getMultiConnectionDamageUpdateModuleData();
	if( obj == nullptr || obj->isEffectivelyDead() || obj->isDestroyed() || obj->isContained() || obj->isDisabled() )
		return FALSE;
	if( obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		return FALSE;
	return !obj->getStatusBits().testForAny( data->m_forbiddenStatus );
}

//-------------------------------------------------------------------------------------------------
Bool MultiConnectionDamageUpdate::isLinkedTo( ObjectID partnerID ) const
{
	for( size_t i = 0; i < m_connections.size(); ++i )
	{
		if( m_connections[ i ].m_partnerID == partnerID )
			return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void MultiConnectionDamageUpdate::dropConnection( size_t index )
{
	if( index >= m_connections.size() )
		return;
	Object *laser = m_connections[ index ].m_laserID != INVALID_ID ? TheGameLogic->findObjectByID( m_connections[ index ].m_laserID ) : nullptr;
	if( laser != nullptr && !laser->isDestroyed() )
		TheGameLogic->destroyObject( laser );
	m_connections.erase( m_connections.begin() + index );
}

//-------------------------------------------------------------------------------------------------
// new partners, the nearest first: allies of the kinds named, made after this one (each pair is linked once, by
// the one of the two made first)
//-------------------------------------------------------------------------------------------------
void MultiConnectionDamageUpdate::rescan()
{
	const MultiConnectionDamageUpdateModuleData *data = getMultiConnectionDamageUpdateModuleData();
	Object *me = getObject();
	if( (Int)m_connections.size() >= data->m_maxConnections || data->m_maxRange <= 0.0f )
		return;
	SimpleObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( me, data->m_maxRange, FROM_CENTER_2D, nullptr, ITER_SORTED_NEAR_TO_FAR );
	MemoryPoolObjectHolder hold( iter );
	for( Object *other = iter->first(); other != nullptr && (Int)m_connections.size() < data->m_maxConnections; other = iter->next() )
	{
		if( other == me || other->getID() < me->getID() || me->getRelationship( other ) != ALLIES )
			continue;
		Bool named = FALSE;
		for( size_t i = 0; i < data->m_partnerNames.size() && !named; ++i )
			named = other->getTemplate()->getName().compareNoCase( data->m_partnerNames[ i ] ) == 0;
		if( !named || !canLink( other ) || isLinkedTo( other->getID() ) )
			continue;
		Connection connection;
		connection.m_partnerID = other->getID();
		connection.m_laserID = INVALID_ID;
		m_connections.push_back( connection );
	}
}

//-------------------------------------------------------------------------------------------------
// the link's beam: made when there is none, its life started over while the link holds
//-------------------------------------------------------------------------------------------------
void MultiConnectionDamageUpdate::keepBeam( Connection &connection, Object *partner )
{
	const MultiConnectionDamageUpdateModuleData *data = getMultiConnectionDamageUpdateModuleData();
	if( data->m_laserName.isEmpty() )
		return;
	Object *me = getObject();
	Object *laser = connection.m_laserID != INVALID_ID ? TheGameLogic->findObjectByID( connection.m_laserID ) : nullptr;
	if( laser != nullptr && !laser->isDestroyed() )
	{
		static const NameKeyType key_LifetimeUpdate = NAMEKEY( "LifetimeUpdate" );
		LifetimeUpdate *life = (LifetimeUpdate *)laser->findUpdateModule( key_LifetimeUpdate );
		if( life != nullptr )
			life->renewLifetime();
		return;
	}
	connection.m_laserID = INVALID_ID;
	const ThingTemplate *tmpl = TheThingFactory->findTemplate( data->m_laserName );
	Player *player = me->getControllingPlayer();
	if( tmpl == nullptr || player == nullptr )
		return;
	laser = TheThingFactory->newObject( tmpl, player->getDefaultTeam() );
	if( laser == nullptr )
		return;
	connection.m_laserID = laser->getID();
	laser->setPosition( me->getPosition() );
	Drawable *draw = laser->getDrawable();
	if( draw != nullptr )
	{
		static const NameKeyType key_LaserUpdate = NAMEKEY( "LaserUpdate" );
		LaserUpdate *update = (LaserUpdate *)draw->findClientUpdateModule( key_LaserUpdate );
		if( update != nullptr )
		{
			update->initLaser( me, partner, me->getPosition(), nullptr, data->m_beamStartBone );
			if( data->m_beamEndBone.isNotEmpty() )
				update->setTargetBone( data->m_beamEndBone );
		}
	}
}

//-------------------------------------------------------------------------------------------------
// its weapon at each enemy on the line to the partner
//-------------------------------------------------------------------------------------------------
void MultiConnectionDamageUpdate::fireAlong( Object *partner )
{
	const MultiConnectionDamageUpdateModuleData *data = getMultiConnectionDamageUpdateModuleData();
	Object *me = getObject();
	if( data->m_weapon == nullptr || partner == nullptr )
		return;
	const Coord3D *a = me->getPosition();
	const Coord3D *b = partner->getPosition();
	const Real dx = b->x - a->x, dy = b->y - a->y;
	const Real lengthSqr = dx * dx + dy * dy;
	if( lengthSqr < 1.0f )
		return;
	Coord3D middle;
	middle.x = ( a->x + b->x ) * 0.5f;
	middle.y = ( a->y + b->y ) * 0.5f;
	middle.z = ( a->z + b->z ) * 0.5f;
	const Real half = WWMath::Sqrt( lengthSqr ) * 0.5f;

	// (who is on it first, then the shots: a shot may kill, and what it leaves is not looked through meanwhile)
	std::vector<ObjectID> victims;
	{
		SimpleObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( &middle, half + data->m_lineWidth, FROM_BOUNDINGSPHERE_2D, nullptr, ITER_SORTED_NEAR_TO_FAR );
		MemoryPoolObjectHolder hold( iter );
		for( Object *other = iter->first(); other != nullptr; other = iter->next() )
		{
			if( other == me || other == partner || other->isEffectivelyDead() || other->isContained() || other->isAirborneTarget() )
				continue;
			if( other->isKindOf( KINDOF_PROJECTILE ) )
				continue;
			const Relationship r = me->getRelationship( other );
			if( r != ENEMIES && !( r == NEUTRAL && data->m_hitsNeutrals ) )
				continue;
			if( data->m_victimKindOf.any() && !other->isAnyKindOf( data->m_victimKindOf ) )
				continue;
			// how far it stands from the line, between the two
			const Coord3D *at = other->getPosition();
			Real along = ( ( at->x - a->x ) * dx + ( at->y - a->y ) * dy ) / lengthSqr;
			along = WWMath::Clamp( along, 0.0f, 1.0f );
			const Real offX = at->x - ( a->x + along * dx ), offY = at->y - ( a->y + along * dy );
			const Real reach = data->m_lineWidth * 0.5f + other->getGeometryInfo().getBoundingCircleRadius();
			if( offX * offX + offY * offY > reach * reach )
				continue;
			victims.push_back( other->getID() );
		}
	}
	for( size_t i = 0; i < victims.size(); ++i )
	{
		Object *victim = TheGameLogic->findObjectByID( victims[ i ] );
		if( victim == nullptr || victim->isEffectivelyDead() )
			continue;
		TheWeaponStore->createAndFireTempWeapon( data->m_weapon, me, victim );
		++m_shotCount;
	}
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime MultiConnectionDamageUpdate::update()
{
	const MultiConnectionDamageUpdateModuleData *data = getMultiConnectionDamageUpdateModuleData();
	Object *me = getObject();
	const UnsignedInt now = TheGameLogic->getFrame();
	const UnsignedInt step = max( 1u, data->m_updateDelay );
	if( me->isEffectivelyDead() || !canLink( me ) )
	{
		while( !m_connections.empty() )
			dropConnection( m_connections.size() - 1 );
		return me->isEffectivelyDead() ? UPDATE_SLEEP_FOREVER : UPDATE_SLEEP( step );
	}

	// the links it has: the partner gone, out of reach, or not to be linked with now
	for( size_t i = m_connections.size(); i > 0; --i )
	{
		Object *partner = TheGameLogic->findObjectByID( m_connections[ i - 1 ].m_partnerID );
		if( partner == nullptr || !canLink( partner ) || me->getRelationship( partner ) != ALLIES
			|| ThePartitionManager->getDistanceSquared( me, partner, FROM_CENTER_2D ) > sqr( data->m_maxRange ) )
			dropConnection( i - 1 );
	}
	if( now >= m_nextRescanFrame )
	{
		rescan();
		m_nextRescanFrame = now + max( 1u, data->m_rescanDelay );
	}
	const Bool fire = now >= m_nextDamageFrame;
	for( size_t i = 0; i < m_connections.size(); ++i )
	{
		Object *partner = TheGameLogic->findObjectByID( m_connections[ i ].m_partnerID );
		if( partner == nullptr )
			continue;
		keepBeam( m_connections[ i ], partner );
		if( fire )
			fireAlong( partner );
	}
	if( fire )
		m_nextDamageFrame = now + max( 1u, data->m_damageDelay );
	return UPDATE_SLEEP( step );
}

// ------------------------------------------------------------------------------------------------
void MultiConnectionDamageUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void MultiConnectionDamageUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	UnsignedShort count = (UnsignedShort)m_connections.size();
	xfer->xferUnsignedShort( &count );
	if( xfer->getXferMode() == XFER_LOAD )
		m_connections.resize( count );
	for( UnsignedShort i = 0; i < count; ++i )
	{
		xfer->xferObjectID( &m_connections[ i ].m_partnerID );
		xfer->xferObjectID( &m_connections[ i ].m_laserID );
	}
	xfer->xferUnsignedInt( &m_nextRescanFrame );
	xfer->xferUnsignedInt( &m_nextDamageFrame );
	xfer->xferUnsignedInt( &m_shotCount );
}

// ------------------------------------------------------------------------------------------------
void MultiConnectionDamageUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
