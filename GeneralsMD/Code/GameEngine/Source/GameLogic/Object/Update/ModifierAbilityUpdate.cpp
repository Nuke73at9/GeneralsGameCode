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
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/StealthUpdate.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/Upgrade.h"

//-------------------------------------------------------------------------------------------------
/** Tiberium Project @feature UpgradeToGrant: names, one or more a line */
//-------------------------------------------------------------------------------------------------
void ModifierAbilityUpdateModuleData::parseUpgradeNames( INI *ini, void * /*instance*/, void *store, const void * /*userData*/ )
{
	std::vector<AsciiString> *names = (std::vector<AsciiString> *)store;
	for( const char *token = ini->getNextTokenOrNull(); token != nullptr; token = ini->getNextTokenOrNull() )
		names->push_back( AsciiString( token ) );
}

//-------------------------------------------------------------------------------------------------
ModifierAbilityUpdate::ModifierAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	m_approaching = FALSE;
	m_hasTarget = FALSE;
	m_targetID = INVALID_ID;
	m_targetPos.zero();
	m_actFrame = 0;
	m_revealUntil = 0;
	m_revealMask = 0;
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
	// Tiberium Project @feature Delay: it acts a while after (Kane's Wrath's unpacking)
	if( data->m_delayFrames > 0 )
	{
		m_actFrame = TheGameLogic->getFrame() + data->m_delayFrames;
		setWakeFrame( me, nextWake() );
		return TRUE;
	}
	act();
	setWakeFrame( me, nextWake() );
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Tiberium Project @feature an object it takes in: of the relationship Affects names (its own player's only,
	* with SamePlayerOnly), any of KindOf and none of ForbiddenKindOf */
//-------------------------------------------------------------------------------------------------
Bool ModifierAbilityUpdate::reaches( const Object *other ) const
{
	const ModifierAbilityUpdateModuleData *data = getModifierAbilityUpdateModuleData();
	const Object *me = getObject();
	if( other == nullptr || other->isEffectivelyDead() )
		return FALSE;
	if( data->m_kindOf.any() && !other->isAnyKindOf( data->m_kindOf ) )
		return FALSE;
	if( other->isAnyKindOf( data->m_forbiddenKindOf ) )
		return FALSE;
	if( data->m_samePlayerOnly && other->getControllingPlayer() != me->getControllingPlayer() )
		return FALSE;
	return attributeModifierReaches( me, other, data->m_affects );
}

//-------------------------------------------------------------------------------------------------
/** Tiberium Project @feature the shroud it lifted (RevealTime) falls again */
//-------------------------------------------------------------------------------------------------
void ModifierAbilityUpdate::lowerReveals()
{
	for( size_t i = 0; i < m_reveals.size(); ++i )
		ThePartitionManager->undoShroudReveal( m_reveals[i].x, m_reveals[i].y, m_reveals[i].z, (PlayerMaskType)m_revealMask );
	m_reveals.clear();
	m_revealUntil = 0;
	m_revealMask = 0;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime ModifierAbilityUpdate::nextWake() const
{
	UnsignedInt next = m_actFrame;
	if( m_revealUntil != 0 && ( next == 0 || m_revealUntil < next ) )
		next = m_revealUntil;
	if( next == 0 )
		return UPDATE_SLEEP_FOREVER;
	const UnsignedInt now = TheGameLogic->getFrame();
	return next > now ? UPDATE_SLEEP( next - now ) : UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
void ModifierAbilityUpdate::onDelete()
{
	// (the shroud it lifted does not stay lifted for good)
	if( m_revealUntil != 0 )
		lowerReveals();
	SpecialPowerUpdateModule::onDelete();
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
	if( data->m_global )
	{
		// Tiberium Project @feature anywhere on the map (C&C4's Voice of Kane, Kane's Wrath's scans)
		for( Object *other = TheGameLogic->getFirstObject(); other != nullptr; other = other->getNextObject() )
			if( reaches( other ) )
				reached.push_back( other );
	}
	else if( data->m_radius > 0.0f )
	{
		ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( &spot, data->m_radius, FROM_CENTER_2D );
		MemoryPoolObjectHolder hold( iter );
		for( Object *other = iter->first(); other; other = iter->next() )
		{
			// (a player power's filter: the kinds it takes and those it leaves)
			if( reaches( other ) )
				reached.push_back( other );
		}
	}
	else
	{
		Object *target = m_targetID != INVALID_ID ? TheGameLogic->findObjectByID( m_targetID ) : me;
		if( attributeModifierReaches( me, target, data->m_affects ) &&
				( !data->m_samePlayerOnly || target->getControllingPlayer() == me->getControllingPlayer() ) )
			reached.push_back( target );
	}

	// Tiberium Project @feature the upgrades it gives (resolved once a use)
	std::vector<const UpgradeTemplate *> upgrades;
	for( size_t i = 0; i < data->m_upgradeNames.size(); ++i )
	{
		const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade( data->m_upgradeNames[i] );
		if( upgrade != nullptr && upgrade->getUpgradeType() == UPGRADE_TYPE_OBJECT )
			upgrades.push_back( upgrade );
	}
	Player *player = me->getControllingPlayer();
	const Bool reveal = data->m_revealFrames > 0 && player != nullptr;
	if( reveal && m_revealUntil != 0 )
		lowerReveals();

	for( size_t i = 0; i < reached.size(); ++i )
	{
		Object *other = reached[i];
		if( modifier != nullptr )
			other->applyAttributeModifier( modifier, me->getID() );
		const Bool enemy = other != me && me->getRelationship( other ) == ENEMIES;
		const UnsignedInt purge = enemy ? data->m_purgeEnemyBuffs : data->m_purgeAlliedBuffs;
		if( purge != 0 )
			other->purgeAttributeModifiers( purge, data->m_maxPurgePerUnit );
		// Tiberium Project @feature what the SAGE 2 area powers do besides
		for( size_t u = 0; u < upgrades.size(); ++u )
			if( !other->hasUpgrade( upgrades[u] ) )
				other->giveUpgrade( upgrades[u] );
		if( data->m_grantStealth )
		{
			StealthUpdate *stealth = other->getStealth();
			if( stealth != nullptr )
				stealth->receiveGrant( TRUE, data->m_stealthFrames );
		}
		if( data->m_refreshRecharge )
		{
			for( BehaviorModule **m = other->getBehaviorModules(); *m; ++m )
			{
				SpecialPowerModuleInterface *power = (*m)->getSpecialPower();
				if( power != nullptr )
					power->refreshRecharge();
			}
		}
		if( reveal )
		{
			Coord3D lifted = *other->getPosition();
			lifted.z = other->getGeometryInfo().getBoundingCircleRadius() + data->m_revealRadius;
			ThePartitionManager->doShroudReveal( lifted.x, lifted.y, lifted.z, player->getPlayerMask() );
			m_reveals.push_back( lifted );
		}
		if( data->m_targetFX != nullptr )
			FXList::doFXObj( data->m_targetFX, other );
	}
	if( reveal && !m_reveals.empty() )
	{
		m_revealUntil = TheGameLogic->getFrame() + data->m_revealFrames;
		m_revealMask = (UnsignedInt)player->getPlayerMask();
	}

	// Tiberium Project @feature the radars of its player's enemies dark (Kane's Wrath's Radar Jamming Missile)
	if( data->m_jamRadarFrames > 0 && player != nullptr )
	{
		for( Int i = 0; i < ThePlayerList->getPlayerCount(); ++i )
		{
			Player *other = ThePlayerList->getNthPlayer( i );
			if( other != nullptr && other != player && other->getDefaultTeam() != nullptr &&
					player->getRelationship( other->getDefaultTeam() ) == ENEMIES )
				other->jamRadar( data->m_jamRadarFrames );
		}
	}

	// Tiberium Project @feature a weapon fired at the spot (Kane's Wrath's Cloaking Field kills the infantry in it)
	if( data->m_weaponName.isNotEmpty() )
	{
		const WeaponTemplate *weapon = TheWeaponStore->findWeaponTemplate( data->m_weaponName );
		if( weapon != nullptr )
			TheWeaponStore->createAndFireTempWeapon( weapon, me, &spot );
	}
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime ModifierAbilityUpdate::update()
{
	// Tiberium Project @feature the shroud it lifted falls again, a delayed use acts
	const UnsignedInt now = TheGameLogic->getFrame();
	if( m_revealUntil != 0 && now >= m_revealUntil )
		lowerReveals();
	if( m_actFrame != 0 && now >= m_actFrame )
	{
		m_actFrame = 0;
		if( !getObject()->isEffectivelyDead() )
			act();
	}
	if( !m_approaching )
		return nextWake();
	const ModifierAbilityUpdateModuleData *data = getModifierAbilityUpdateModuleData();
	Object *me = getObject();
	AIUpdateInterface *ai = me->getAI();
	Coord3D spot;
	// another order (the player's) calls it off, as does its target's going
	if( ai == nullptr || me->isEffectivelyDead() || ai->getLastCommandSource() != CMD_FROM_AI || !targetSpot( &spot ) )
	{
		m_approaching = FALSE;
		return nextWake();
	}
	const Coord3D *pos = me->getPosition();
	const Real dx = pos->x - spot.x, dy = pos->y - spot.y;
	if( dx * dx + dy * dy <= sqr( data->m_startAbilityRange ) )
	{
		ai->aiIdle( CMD_FROM_AI );
		act();
		return nextWake();
	}
	if( ai->isIdle() )
	{
		// it stopped short (the target moved, or the way was blocked): on after it again
		if( m_targetID != INVALID_ID )
			ai->aiMoveToPosition( &spot, CMD_FROM_AI );
		else
		{
			m_approaching = FALSE;
			return nextWake();
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
// 2: Tiberium Project: a delayed use, the shroud it has lifted
//-------------------------------------------------------------------------------------------------
void ModifierAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	SpecialPowerUpdateModule::xfer( xfer );

	xfer->xferBool( &m_approaching );
	xfer->xferBool( &m_hasTarget );
	xfer->xferObjectID( &m_targetID );
	xfer->xferCoord3D( &m_targetPos );

	if( version >= 2 )
	{
		xfer->xferUnsignedInt( &m_actFrame );
		xfer->xferUnsignedInt( &m_revealUntil );
		xfer->xferUnsignedInt( &m_revealMask );
		UnsignedShort count = (UnsignedShort)m_reveals.size();
		xfer->xferUnsignedShort( &count );
		if( xfer->getXferMode() == XFER_LOAD )
			m_reveals.resize( count );
		for( UnsignedShort i = 0; i < count; ++i )
			xfer->xferCoord3D( &m_reveals[i] );
	}
}

//-------------------------------------------------------------------------------------------------
void ModifierAbilityUpdate::loadPostProcess()
{
	SpecialPowerUpdateModule::loadPostProcess();
}
