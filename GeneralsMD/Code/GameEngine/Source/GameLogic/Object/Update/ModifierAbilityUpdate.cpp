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

// FILE: ModifierAbilityUpdate.cpp ////////////////////////////////////////////////////////////////
// Tiberium Project @feature special powers that work by attribute modifiers (see ModifierAbilityUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/ModifierAbilityUpdate.h"

#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/AI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/AIUpdate.h"

//-------------------------------------------------------------------------------------------------
ModifierAbilityUpdate::ModifierAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	m_approaching = FALSE;
	m_hasTarget = FALSE;
	m_targetID = INVALID_ID;
	m_targetPos.zero();
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
ModifierAbilityUpdate::~ModifierAbilityUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool ModifierAbilityUpdate::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const ModifierAbilityUpdateModuleData *data = getModifierAbilityUpdateModuleData();
	if( specialPowerTemplate != data->m_specialPowerTemplate )
		return FALSE;
	Object *me = getObject();
	if( me->isEffectivelyDead() )
		return FALSE;

	m_hasTarget = targetObj != nullptr || targetPos != nullptr;
	m_targetID = targetObj != nullptr ? targetObj->getID() : INVALID_ID;
	if( targetObj != nullptr )
		m_targetPos = *targetObj->getPosition();
	else if( targetPos != nullptr )
		m_targetPos = *targetPos;
	else
		m_targetPos = *me->getPosition();

	// out of reach: it goes within StartAbilityRange first (update)
	AIUpdateInterface *ai = me->getAI();
	if( m_hasTarget && data->m_startAbilityRange > 0.0f && ai != nullptr )
	{
		const Coord3D *pos = me->getPosition();
		const Real dx = pos->x - m_targetPos.x, dy = pos->y - m_targetPos.y;
		if( dx * dx + dy * dy > sqr( data->m_startAbilityRange ) )
		{
			m_approaching = TRUE;
			ai->aiMoveToPosition( &m_targetPos, CMD_FROM_AI );
			setWakeFrame( me, UPDATE_SLEEP_NONE );
			return TRUE;
		}
	}
	act();
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool ModifierAbilityUpdate::targetSpot( Coord3D *spot ) const
{
	if( m_targetID != INVALID_ID )
	{
		const Object *target = TheGameLogic->findObjectByID( m_targetID );
		if( target == nullptr || target->isEffectivelyDead() )
			return FALSE;
		*spot = *target->getPosition();
		return TRUE;
	}
	*spot = m_hasTarget ? m_targetPos : *getObject()->getPosition();
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** its modifier on, and the purges, on what it reaches */
//-------------------------------------------------------------------------------------------------
void ModifierAbilityUpdate::act()
{
	const ModifierAbilityUpdateModuleData *data = getModifierAbilityUpdateModuleData();
	Object *me = getObject();
	m_approaching = FALSE;
	Coord3D spot;
	if( !targetSpot( &spot ) )
		return;
	const AttributeModifierTemplate *modifier = TheAttributeModifierStore->find( data->m_modifier );
	if( data->m_initiateFX != nullptr )
		FXList::doFXObj( data->m_initiateFX, me );

	std::vector<Object *> reached;
	if( data->m_radius > 0.0f )
	{
		ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( &spot, data->m_radius, FROM_CENTER_2D );
		MemoryPoolObjectHolder hold( iter );
		for( Object *other = iter->first(); other; other = iter->next() )
			if( attributeModifierReaches( me, other, data->m_affects ) )
				reached.push_back( other );
	}
	else
	{
		Object *target = m_targetID != INVALID_ID ? TheGameLogic->findObjectByID( m_targetID ) : me;
		if( attributeModifierReaches( me, target, data->m_affects ) )
			reached.push_back( target );
	}

	for( size_t i = 0; i < reached.size(); ++i )
	{
		Object *other = reached[i];
		if( modifier != nullptr )
			other->applyAttributeModifier( modifier, me->getID() );
		const Bool enemy = other != me && me->getRelationship( other ) == ENEMIES;
		const UnsignedInt purge = enemy ? data->m_purgeEnemyBuffs : data->m_purgeAlliedBuffs;
		if( purge != 0 )
			other->purgeAttributeModifiers( purge, data->m_maxPurgePerUnit );
		if( data->m_targetFX != nullptr )
			FXList::doFXObj( data->m_targetFX, other );
	}
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime ModifierAbilityUpdate::update()
{
	if( !m_approaching )
		return UPDATE_SLEEP_FOREVER;
	const ModifierAbilityUpdateModuleData *data = getModifierAbilityUpdateModuleData();
	Object *me = getObject();
	AIUpdateInterface *ai = me->getAI();
	Coord3D spot;
	// another order (the player's) calls it off, as does its target's going
	if( ai == nullptr || me->isEffectivelyDead() || ai->getLastCommandSource() != CMD_FROM_AI || !targetSpot( &spot ) )
	{
		m_approaching = FALSE;
		return UPDATE_SLEEP_FOREVER;
	}
	const Coord3D *pos = me->getPosition();
	const Real dx = pos->x - spot.x, dy = pos->y - spot.y;
	if( dx * dx + dy * dy <= sqr( data->m_startAbilityRange ) )
	{
		ai->aiIdle( CMD_FROM_AI );
		act();
		return UPDATE_SLEEP_FOREVER;
	}
	if( ai->isIdle() )
	{
		// it stopped short (the target moved, or the way was blocked): on after it again
		if( m_targetID != INVALID_ID )
			ai->aiMoveToPosition( &spot, CMD_FROM_AI );
		else
		{
			m_approaching = FALSE;
			return UPDATE_SLEEP_FOREVER;
		}
	}
	return UPDATE_SLEEP( LOGICFRAMES_PER_SECOND / 4 );
}

//-------------------------------------------------------------------------------------------------
void ModifierAbilityUpdate::crc( Xfer *xfer )
{
	SpecialPowerUpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void ModifierAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	SpecialPowerUpdateModule::xfer( xfer );

	xfer->xferBool( &m_approaching );
	xfer->xferBool( &m_hasTarget );
	xfer->xferObjectID( &m_targetID );
	xfer->xferCoord3D( &m_targetPos );
}

//-------------------------------------------------------------------------------------------------
void ModifierAbilityUpdate::loadPostProcess()
{
	SpecialPowerUpdateModule::loadPostProcess();
}
