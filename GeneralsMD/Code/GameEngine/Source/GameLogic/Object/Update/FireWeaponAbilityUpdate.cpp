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

// FILE: FireWeaponAbilityUpdate.cpp //////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's weapon-firing unit abilities (see FireWeaponAbilityUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/FireWeaponAbilityUpdate.h"

#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AIUpdate.h"

//-------------------------------------------------------------------------------------------------
// Shot = Weapon:<name> Delay:<ms> OffsetX:<ahead> OffsetY:<left>
//-------------------------------------------------------------------------------------------------
void FireWeaponAbilityUpdateModuleData::parseShot( INI *ini, void *instance, void * /*store*/, const void * /*userData*/ )
{
	FireWeaponAbilityUpdateModuleData *self = (FireWeaponAbilityUpdateModuleData *)instance;
	FireWeaponAbilityShot shot;
	shot.m_weaponName = INI::getNextSubToken( "Weapon" );
	const Real delayMs = INI::scanReal( INI::getNextSubToken( "Delay" ) );
	shot.m_delay = (UnsignedInt)WWMath::Ceil( ConvertDurationFromMsecsToFrames( delayMs ) );
	shot.m_offsetX = INI::scanReal( INI::getNextSubToken( "OffsetX" ) );
	shot.m_offsetY = INI::scanReal( INI::getNextSubToken( "OffsetY" ) );
	self->m_shots.push_back( shot );
}

//-------------------------------------------------------------------------------------------------
FireWeaponAbilityUpdate::FireWeaponAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	m_stage = STAGE_IDLE;
	m_stageStartFrame = 0;
	m_targetID = INVALID_ID;
	m_targetPos.zero();
	m_hasTarget = FALSE;
	m_nextShot = 0;
	m_moveIssued = FALSE;
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
FireWeaponAbilityUpdate::~FireWeaponAbilityUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool FireWeaponAbilityUpdate::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const FireWeaponAbilityUpdateModuleData *data = getFireWeaponAbilityUpdateModuleData();
	if( specialPowerTemplate != data->m_specialPowerTemplate || data->m_shots.empty() )
		return FALSE;
	Object *me = getObject();
	if( me->isDisabled() || me->isEffectivelyDead() )
		return FALSE;

	m_targetID = targetObj != nullptr ? targetObj->getID() : INVALID_ID;
	m_hasTarget = targetObj != nullptr || targetPos != nullptr;
	if( targetObj != nullptr )
		m_targetPos = *targetObj->getPosition();
	else if( targetPos != nullptr )
		m_targetPos = *targetPos;
	else
		m_targetPos = *me->getPosition();
	m_nextShot = 0;
	m_moveIssued = FALSE;
	enterStage( data->m_startAbilityRange > 0.0f && m_hasTarget ? STAGE_APPROACH : STAGE_UNPACK );
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool FireWeaponAbilityUpdate::targetPosition( Coord3D *pos ) const
{
	if( m_targetID != INVALID_ID )
	{
		const Object *target = TheGameLogic->findObjectByID( m_targetID );
		if( target == nullptr || target->isEffectivelyDead() )
			return FALSE;
		*pos = *target->getPosition();
		return TRUE;
	}
	*pos = m_hasTarget ? m_targetPos : *getObject()->getPosition();
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void FireWeaponAbilityUpdate::enterStage( Stage stage )
{
	const FireWeaponAbilityUpdateModuleData *data = getFireWeaponAbilityUpdateModuleData();
	Object *me = getObject();
	m_stage = stage;
	m_stageStartFrame = TheGameLogic->getFrame();
	me->clearModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_UNPACKING, MODELCONDITION_PACKING ) );
	AIUpdateInterface *ai = me->getAI();
	switch( stage )
	{
		case STAGE_UNPACK:
		{
			// stand still and face the target
			if( ai != nullptr && data->m_startAbilityRange > 0.0f )
				ai->aiIdle( CMD_FROM_AI );
			Coord3D pos;
			if( data->m_faceTarget && m_hasTarget && targetPosition( &pos ) )
			{
				const Coord3D *at = me->getPosition();
				if( pos.x != at->x || pos.y != at->y )
					me->setOrientation( WWMath::Atan2Origin( pos.y - at->y, pos.x - at->x ) );
			}
			if( data->m_unpackTime > 0 )
				me->setModelConditionState( MODELCONDITION_UNPACKING );
			break;
		}
		case STAGE_PACK:
			if( data->m_packTime > 0 )
				me->setModelConditionState( MODELCONDITION_PACKING );
			break;
		default:
			break;
	}
	setWakeFrame( me, UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
void FireWeaponAbilityUpdate::finish()
{
	getObject()->clearModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_UNPACKING, MODELCONDITION_PACKING ) );
	m_stage = STAGE_IDLE;
	m_targetID = INVALID_ID;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime FireWeaponAbilityUpdate::update()
{
	const FireWeaponAbilityUpdateModuleData *data = getFireWeaponAbilityUpdateModuleData();
	Object *me = getObject();
	if( m_stage == STAGE_IDLE )
		return UPDATE_SLEEP_FOREVER;
	if( me->isEffectivelyDead() )
	{
		finish();
		return UPDATE_SLEEP_FOREVER;
	}
	const UnsignedInt now = TheGameLogic->getFrame();
	const UnsignedInt since = now - m_stageStartFrame;
	Coord3D pos;
	switch( m_stage )
	{
		case STAGE_APPROACH:
		{
			if( !targetPosition( &pos ) )
			{
				finish();
				return UPDATE_SLEEP_FOREVER;
			}
			const Coord3D *at = me->getPosition();
			const Real dx = pos.x - at->x, dy = pos.y - at->y;
			if( dx * dx + dy * dy <= data->m_startAbilityRange * data->m_startAbilityRange )
			{
				enterStage( STAGE_UNPACK );
				return UPDATE_SLEEP_NONE;
			}
			AIUpdateInterface *ai = me->getAI();
			if( ai == nullptr )
			{
				finish();
				return UPDATE_SLEEP_FOREVER;
			}
			if( !m_moveIssued )
			{
				ai->aiMoveToPosition( &pos, CMD_FROM_AI );
				m_moveIssued = TRUE;
			}
			else if( since > LOGICFRAMES_PER_SECOND && ai->isIdle() )
			{
				// stopped short (told to go elsewhere, or no way there): the ability is off
				finish();
				return UPDATE_SLEEP_FOREVER;
			}
			return UPDATE_SLEEP( 3 );
		}
		case STAGE_UNPACK:
			if( since >= data->m_unpackTime )
				enterStage( data->m_preparationTime > 0 ? STAGE_PREPARE : STAGE_FIRE );
			return UPDATE_SLEEP_NONE;
		case STAGE_PREPARE:
			if( since >= data->m_preparationTime )
				enterStage( STAGE_FIRE );
			return UPDATE_SLEEP_NONE;
		case STAGE_FIRE:
		{
			while( m_nextShot < (Int)data->m_shots.size() && since >= data->m_shots[m_nextShot].m_delay )
			{
				const FireWeaponAbilityShot &shot = data->m_shots[m_nextShot++];
				const WeaponTemplate *weapon = TheWeaponStore->findWeaponTemplate( shot.m_weaponName );
				if( weapon == nullptr || !targetPosition( &pos ) )
					continue;
				Object *target = m_targetID != INVALID_ID ? TheGameLogic->findObjectByID( m_targetID ) : nullptr;
				if( target != nullptr && shot.m_offsetX == 0.0f && shot.m_offsetY == 0.0f )
				{
					TheWeaponStore->createAndFireTempWeapon( weapon, me, target );
					continue;
				}
				// the offset turns with the unit: ahead of it, then to its left
				const Real angle = me->getOrientation();
				const Real c = Cos( angle ), s = Sin( angle );
				pos.x += shot.m_offsetX * c - shot.m_offsetY * s;
				pos.y += shot.m_offsetX * s + shot.m_offsetY * c;
				TheWeaponStore->createAndFireTempWeapon( weapon, me, &pos );
			}
			if( m_nextShot >= (Int)data->m_shots.size() )
			{
				if( data->m_packTime > 0 )
					enterStage( STAGE_PACK );
				else
				{
					finish();
					return UPDATE_SLEEP_FOREVER;
				}
			}
			return UPDATE_SLEEP_NONE;
		}
		case STAGE_PACK:
			if( since >= data->m_packTime )
			{
				finish();
				return UPDATE_SLEEP_FOREVER;
			}
			return UPDATE_SLEEP_NONE;
		default:
			break;
	}
	return UPDATE_SLEEP_FOREVER;
}

//-------------------------------------------------------------------------------------------------
void FireWeaponAbilityUpdate::crc( Xfer *xfer )
{
	SpecialPowerUpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void FireWeaponAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	SpecialPowerUpdateModule::xfer( xfer );

	Int stage = (Int)m_stage;
	xfer->xferInt( &stage );
	m_stage = (Stage)stage;
	xfer->xferUnsignedInt( &m_stageStartFrame );
	xfer->xferObjectID( &m_targetID );
	xfer->xferCoord3D( &m_targetPos );
	xfer->xferBool( &m_hasTarget );
	xfer->xferInt( &m_nextShot );
	xfer->xferBool( &m_moveIssued );
}

//-------------------------------------------------------------------------------------------------
void FireWeaponAbilityUpdate::loadPostProcess()
{
	SpecialPowerUpdateModule::loadPostProcess();
}
