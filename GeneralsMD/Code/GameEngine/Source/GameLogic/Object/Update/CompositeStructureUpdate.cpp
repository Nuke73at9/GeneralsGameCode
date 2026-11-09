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

// FILE: CompositeStructureUpdate.cpp /////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's CompositeStructureInfoModule (see CompositeStructureUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/CompositeStructureUpdate.h"

#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/BaseConstructionUpdate.h"

//-------------------------------------------------------------------------------------------------
CompositeStructureUpdateModuleData::CompositeStructureUpdateModuleData()
{
	m_count = 3;
	m_buildableDistance = 150.0f;
	m_radius = 0.0f;
	m_spread = 60.0f * PI / 180.0f;
	m_unpackTime = LOGICFRAMES_PER_SECOND * 3 / 2;
	m_repairDelay = LOGICFRAMES_PER_SECOND * 20;
}

//-------------------------------------------------------------------------------------------------
void CompositeStructureUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "ThingTemplate",			INI::parseAsciiString,					nullptr, offsetof( CompositeStructureUpdateModuleData, m_thingTemplateName ) },
		{ "Count",							INI::parseInt,									nullptr, offsetof( CompositeStructureUpdateModuleData, m_count ) },
		{ "BuildableDistance",	INI::parseReal,									nullptr, offsetof( CompositeStructureUpdateModuleData, m_buildableDistance ) },
		{ "Radius",							INI::parseReal,									nullptr, offsetof( CompositeStructureUpdateModuleData, m_radius ) },
		{ "Spread",							INI::parseAngleReal,						nullptr, offsetof( CompositeStructureUpdateModuleData, m_spread ) },
		{ "UnpackTime",					INI::parseDurationUnsignedInt,	nullptr, offsetof( CompositeStructureUpdateModuleData, m_unpackTime ) },
		{ "RepairDelay",				INI::parseDurationUnsignedInt,	nullptr, offsetof( CompositeStructureUpdateModuleData, m_repairDelay ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
CompositeStructureUpdate::CompositeStructureUpdate( Thing *thing, const ModuleData* moduleData ) :
	UpdateModule( thing, moduleData ),
	m_released( FALSE )
{
	setWakeFrame( getObject(), UPDATE_SLEEP( LOGICFRAMES_PER_SECOND / 2 ) );
}

//-------------------------------------------------------------------------------------------------
CompositeStructureUpdate::~CompositeStructureUpdate()
{
}

//-------------------------------------------------------------------------------------------------
// whether a turret may stand there now (the ground, and nothing in the way that cannot move)
//-------------------------------------------------------------------------------------------------
Bool CompositeStructureUpdate::isFree( const Coord3D *pos, Real angle, const ThingTemplate *tmpl ) const
{
	Player *player = getObject()->getControllingPlayer();
	return TheBuildAssistant->isLocationLegalToBuild( pos, tmpl, angle,
		BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP, nullptr, player ) == LBC_OK;
}

//-------------------------------------------------------------------------------------------------
// the turret's spot: in an arc before the hub, at Radius; where that is taken, a little nearer or
// farther or to one side (never beyond BuildableDistance)
//-------------------------------------------------------------------------------------------------
Bool CompositeStructureUpdate::choosePlace( Slot &slot, Int index, const ThingTemplate *tmpl )
{
	const CompositeStructureUpdateModuleData *d = getCompositeStructureUpdateModuleData();
	const Object *me = getObject();
	const Coord3D *center = me->getPosition();
	const Real facing = me->getOrientation();
	const Real reach = max( 1.0f, d->m_buildableDistance );
	const Real radius = min( reach, d->m_radius > 0.0f ? d->m_radius : reach * 0.5f );
	const Real angle = facing + ( (Real)index - (Real)( d->m_count - 1 ) * 0.5f ) * d->m_spread;
	static const Real RADII[] = { 1.0f, 0.75f, 1.3f, 0.5f, 1.6f };
	static const Real TURNS[] = { 0.0f, 0.26f, -0.26f, 0.52f, -0.52f };
	for( Int r = 0; r < (Int)ARRAY_SIZE( RADII ); ++r )
	{
		const Real dist = min( reach, radius * RADII[r] );
		for( Int a = 0; a < (Int)ARRAY_SIZE( TURNS ); ++a )
		{
			const Real at = angle + TURNS[a];
			Coord3D pos;
			pos.x = center->x + dist * WWMath::Cos( at );
			pos.y = center->y + dist * WWMath::Sin( at );
			pos.z = center->z;
			if( !isFree( &pos, facing, tmpl ) )
				continue;
			slot.m_pos = pos;
			slot.m_angle = facing;
			slot.m_placed = TRUE;
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void CompositeStructureUpdate::releaseTurrets()
{
	if( m_released )
		return;
	m_released = TRUE;
	for( size_t i = 0; i < m_slots.size(); ++i )
	{
		Object *turret = m_slots[i].m_id != INVALID_ID ? TheGameLogic->findObjectByID( m_slots[i].m_id ) : nullptr;
		m_slots[i].m_id = INVALID_ID;
		m_slots[i].m_rising = FALSE;
		if( turret != nullptr && !turret->isEffectivelyDead() )
			turret->kill( DAMAGE_UNRESISTABLE, DEATH_SUICIDED );
	}
}

//-------------------------------------------------------------------------------------------------
void CompositeStructureUpdate::onDelete()
{
	// (gone without dying: taken off the map)
	releaseTurrets();
	UpdateModule::onDelete();
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime CompositeStructureUpdate::update()
{
	const CompositeStructureUpdateModuleData *d = getCompositeStructureUpdateModuleData();
	Object *me = getObject();
	if( m_released )
		return UPDATE_SLEEP_FOREVER;

	// the hub gone, destroyed or sold: its turrets with it
	if( me->isEffectivelyDead() || me->testStatus( OBJECT_STATUS_SOLD ) )
	{
		releaseTurrets();
		return UPDATE_SLEEP_FOREVER;
	}
	// (nothing before it is built)
	if( me->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		return UPDATE_SLEEP( LOGICFRAMES_PER_SECOND / 2 );

	const ThingTemplate *tmpl = TheThingFactory->findTemplate( d->m_thingTemplateName );
	if( tmpl == nullptr || d->m_count <= 0 )
		return UPDATE_SLEEP_FOREVER;

	const UnsignedInt now = TheGameLogic->getFrame();
	if( m_slots.empty() )
	{
		Slot blank;
		blank.m_id = INVALID_ID;
		blank.m_readyFrame = now;
		blank.m_pos.zero();
		blank.m_angle = 0.0f;
		blank.m_placed = FALSE;
		blank.m_rising = FALSE;
		m_slots.assign( (size_t)d->m_count, blank );
	}

	Bool rising = FALSE;
	for( size_t i = 0; i < m_slots.size(); ++i )
	{
		Slot &slot = m_slots[i];
		Object *turret = slot.m_id != INVALID_ID ? TheGameLogic->findObjectByID( slot.m_id ) : nullptr;
		if( turret != nullptr && !turret->isEffectivelyDead() )
		{
			// (the hub's side: a hub taken over takes its turrets with it)
			if( turret->getTeam() != me->getTeam() )
				turret->setTeam( me->getTeam() );
			if( slot.m_rising )
			{
				if( BaseConstructionUpdate::riseStep( turret, d->m_unpackTime, me ) )
					slot.m_rising = FALSE;
				else
					rising = TRUE;
			}
			continue;
		}
		if( slot.m_id != INVALID_ID )
		{
			// destroyed: put back where it stood a while later
			slot.m_id = INVALID_ID;
			slot.m_rising = FALSE;
			slot.m_readyFrame = now + d->m_repairDelay;
		}
		if( now < slot.m_readyFrame )
			continue;
		if( !slot.m_placed ? !choosePlace( slot, (Int)i, tmpl ) : !isFree( &slot.m_pos, slot.m_angle, tmpl ) )
			continue;		// (no room now: tried again in a while)
		Object *made = BaseConstructionUpdate::startRising( tmpl, me->getTeam(), me, &slot.m_pos, slot.m_angle );
		if( made == nullptr )
			continue;
		slot.m_id = made->getID();
		slot.m_rising = TRUE;
		rising = TRUE;
	}
	return rising ? UPDATE_SLEEP_NONE : UPDATE_SLEEP( LOGICFRAMES_PER_SECOND / 2 );
}

//-------------------------------------------------------------------------------------------------
void CompositeStructureUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void CompositeStructureUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );

	xfer->xferBool( &m_released );
	UnsignedShort count = (UnsignedShort)m_slots.size();
	xfer->xferUnsignedShort( &count );
	if( xfer->getXferMode() == XFER_LOAD )
		m_slots.resize( count );
	for( UnsignedShort i = 0; i < count; ++i )
	{
		Slot &slot = m_slots[i];
		xfer->xferObjectID( &slot.m_id );
		xfer->xferUnsignedInt( &slot.m_readyFrame );
		xfer->xferCoord3D( &slot.m_pos );
		xfer->xferReal( &slot.m_angle );
		xfer->xferBool( &slot.m_placed );
		xfer->xferBool( &slot.m_rising );
	}
}

//-------------------------------------------------------------------------------------------------
void CompositeStructureUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
