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


// FILE: SuppressionUpdate.cpp ////////////////////////////////////////////////////////////////////
// Tiberium Project @feature Kane's Wrath's infantry pinned down by fire (see the header)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SuppressionUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"

//-------------------------------------------------------------------------------------------------
SuppressionUpdateModuleData::SuppressionUpdateModuleData()
{
	m_updateDelay = LOGICFRAMES_PER_SECOND;
	m_suppressability = 0.0f;
	m_suppressionDuration = 3 * LOGICFRAMES_PER_SECOND;
	m_suppressedModifier = NAMEKEY_INVALID;
	m_forceMoveModifier = NAMEKEY_INVALID;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void SuppressionUpdateModuleData::buildFieldParse( MultiIniFieldParse &p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "UpdateDelay",					INI::parseDurationUnsignedInt,				nullptr, offsetof( SuppressionUpdateModuleData, m_updateDelay ) },
		{ "Suppressability",			INI::parseReal,												nullptr, offsetof( SuppressionUpdateModuleData, m_suppressability ) },
		{ "SuppressionDuration",	INI::parseDurationUnsignedInt,				nullptr, offsetof( SuppressionUpdateModuleData, m_suppressionDuration ) },
		{ "SuppressedModifier",		AttributeModifierStore::parseModifierName,	nullptr, offsetof( SuppressionUpdateModuleData, m_suppressedModifier ) },
		{ "ForceMoveModifier",		AttributeModifierStore::parseModifierName,	nullptr, offsetof( SuppressionUpdateModuleData, m_forceMoveModifier ) },
		{ "IgnoreStatus",					ObjectStatusMaskType::parseFromINI,		nullptr, offsetof( SuppressionUpdateModuleData, m_ignoreStatus ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
SuppressionUpdate::SuppressionUpdate( Thing *thing, const ModuleData *moduleData ) :
	UpdateModule( thing, moduleData ),
	m_suppressedUntil( 0 )
{
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
SuppressionUpdate::~SuppressionUpdate()
{
}

//-------------------------------------------------------------------------------------------------
void SuppressionUpdate::addSuppression( Real amount, UnsignedInt frames )
{
	if( amount <= 0.0f || frames == 0 || getObject()->isEffectivelyDead() )
		return;
	Entry entry;
	entry.m_amount = amount;
	entry.m_expireFrame = TheGameLogic->getFrame() + frames;
	m_entries.push_back( entry );
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
Bool SuppressionUpdate::isSuppressed() const
{
	return m_suppressedUntil != 0 && TheGameLogic->getFrame() < m_suppressedUntil;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime SuppressionUpdate::update()
{
	const SuppressionUpdateModuleData *data = getSuppressionUpdateModuleData();
	Object *me = getObject();
	const UnsignedInt now = TheGameLogic->getFrame();
	const UnsignedInt delay = data->m_updateDelay > 0 ? data->m_updateDelay : 1;
	if( me->isEffectivelyDead() )
		return UPDATE_SLEEP_FOREVER;

	// what is on it now
	Real total = 0.0f;
	for( size_t i = 0; i < m_entries.size(); )
	{
		if( m_entries[ i ].m_expireFrame <= now )
		{
			m_entries.erase( m_entries.begin() + i );
			continue;
		}
		total += m_entries[ i ].m_amount;
		++i;
	}
	const Bool ignored = data->m_ignoreStatus.any() && me->getStatusBits().testForAny( data->m_ignoreStatus );
	if( !ignored && data->m_suppressability > 0.0f && total >= data->m_suppressability )
		m_suppressedUntil = now + data->m_suppressionDuration;

	const AttributeModifierTemplate *pinned = TheAttributeModifierStore != nullptr ? TheAttributeModifierStore->find( data->m_suppressedModifier ) : nullptr;
	const AttributeModifierTemplate *running = TheAttributeModifierStore != nullptr ? TheAttributeModifierStore->find( data->m_forceMoveModifier ) : nullptr;
	if( !ignored && isSuppressed() )
	{
		// pinned down, unless its player has it get up and go
		const AIUpdateInterface *ai = me->getAI();
		const Bool goes = ai != nullptr && ai->isMoving() && ai->getLastCommandSource() == CMD_FROM_PLAYER;
		const AttributeModifierTemplate *on = goes ? running : pinned;
		const AttributeModifierTemplate *off = goes ? pinned : running;
		if( off != nullptr && me->hasAttributeModifier( off ) )
			me->removeAttributeModifier( off, me->getID() );
		if( on != nullptr )
			me->applyAttributeModifier( on, me->getID(), 2 * delay );
		return UPDATE_SLEEP( delay );
	}

	m_suppressedUntil = 0;
	if( pinned != nullptr && me->hasAttributeModifier( pinned ) )
		me->removeAttributeModifier( pinned, me->getID() );
	if( running != nullptr && me->hasAttributeModifier( running ) )
		me->removeAttributeModifier( running, me->getID() );
	return m_entries.empty() ? UPDATE_SLEEP_FOREVER : UPDATE_SLEEP( delay );
}

//-------------------------------------------------------------------------------------------------
/*static*/ void SuppressionUpdate::suppressAround( const Object *source, const Coord3D *pos, Real radius, Real amount, UnsignedInt frames )
{
	static const NameKeyType key = NAMEKEY( "SuppressionUpdate" );
	if( pos == nullptr || amount <= 0.0f || frames == 0 )
		return;
	SimpleObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( pos, max( radius, 1.0f ), FROM_CENTER_2D, nullptr, ITER_FASTEST );
	MemoryPoolObjectHolder hold( iter );
	for( Object *other = iter->first(); other != nullptr; other = iter->next() )
	{
		if( other == source || other->isEffectivelyDead() )
			continue;
		// (its firer's side is not pinned down by its fire)
		if( source != nullptr && source->getRelationship( other ) == ALLIES )
			continue;
		SuppressionUpdate *sup = (SuppressionUpdate *)other->findUpdateModule( key );
		if( sup != nullptr )
			sup->addSuppression( amount, frames );
	}
}

// ------------------------------------------------------------------------------------------------
void SuppressionUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void SuppressionUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	UnsignedShort count = (UnsignedShort)m_entries.size();
	xfer->xferUnsignedShort( &count );
	if( xfer->getXferMode() == XFER_LOAD )
		m_entries.resize( count );
	for( UnsignedShort i = 0; i < count; ++i )
	{
		xfer->xferReal( &m_entries[ i ].m_amount );
		xfer->xferUnsignedInt( &m_entries[ i ].m_expireFrame );
	}
	xfer->xferUnsignedInt( &m_suppressedUntil );
}

// ------------------------------------------------------------------------------------------------
void SuppressionUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
