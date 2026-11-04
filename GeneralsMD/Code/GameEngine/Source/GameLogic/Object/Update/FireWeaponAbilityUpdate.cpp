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

#include "Common/BezierSegment.h"
#include "Common/GameAudio.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"

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
	for( Int i = 0; i < 4; ++i )
		m_leapPoints[i].zero();
	m_leapSegments = 0;
	m_leapSegment = 0;
	m_leapLook = 0;
	m_leapNoAttack = FALSE;
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
	// (a leap needs no shots: Tiberium Project)
	if( specialPowerTemplate != data->m_specialPowerTemplate || ( data->m_shots.empty() && data->m_leapSpeed <= 0.0f ) )
		return FALSE;
	Object *me = getObject();
	if( me->isDisabled() || me->isEffectivelyDead() || m_stage == STAGE_LEAP )
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
	m_leapSegments = 0;
	m_leapSegment = 0;
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
			playSound( data->m_unpackSound );	// Tiberium Project
			break;
		}
		case STAGE_FIRE:
			// Tiberium Project: as it fires, or takes off (not again as it fires on landing)
			if( m_leapSegments == 0 )
				playSound( data->m_triggerSound );
			break;
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
	Object *me = getObject();
	if( m_stage == STAGE_LEAP )
	{
		// stopped in the air (it died there): down on the ground, as it was
		Coord3D pos = *me->getPosition();
		pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );
		me->setPosition( &pos );
		endLeap();
	}
	me->clearModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_UNPACKING, MODELCONDITION_PACKING ) );
	m_stage = STAGE_IDLE;
	m_targetID = INVALID_ID;
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature
void FireWeaponAbilityUpdate::playSound( const AudioEventRTS &sound ) const
{
	if( sound.getEventName().isEmpty() )
		return;
	AudioEventRTS play = sound;
	play.setObjectID( getObject()->getID() );
	TheAudio->addAudioEvent( &play );
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature the leap: an arc from the unit to the target spot (as a DumbProjectileBehavior's,
// its inner points above the highest ground on the way), and up it goes, held (no physics, no orders, no
// shots) until it is down again
void FireWeaponAbilityUpdate::startLeap()
{
	const FireWeaponAbilityUpdateModuleData *data = getFireWeaponAbilityUpdateModuleData();
	Object *me = getObject();
	Coord3D dest;
	if( !targetPosition( &dest ) )
		dest = m_targetPos;		// (the target died: where it was)
	AIUpdateInterface *ai = me->getAI();
	if( ai != nullptr )
	{
		ai->aiIdle( CMD_FROM_AI );
		// where it can stand, clear of the others
		TheAI->pathfinder()->adjustDestination( me, ai->getLocomotorSet(), &dest );
	}
	dest.z = TheTerrainLogic->getGroundHeight( dest.x, dest.y );

	m_leapPoints[0] = *me->getPosition();
	m_leapPoints[3] = dest;
	Real highest = max( m_leapPoints[0].z, dest.z );
	Real terrain;
	if( ThePartitionManager->estimateTerrainExtremesAlongLine( m_leapPoints[0], m_leapPoints[3], nullptr, &terrain, nullptr, nullptr ) )
		highest = max( highest, terrain );
	const Real dx = dest.x - m_leapPoints[0].x;
	const Real dy = dest.y - m_leapPoints[0].y;
	m_leapPoints[1].x = m_leapPoints[0].x + dx * data->m_leapFirstIndent;
	m_leapPoints[1].y = m_leapPoints[0].y + dy * data->m_leapFirstIndent;
	m_leapPoints[1].z = highest + data->m_leapFirstHeight;
	m_leapPoints[2].x = m_leapPoints[0].x + dx * data->m_leapSecondIndent;
	m_leapPoints[2].y = m_leapPoints[0].y + dy * data->m_leapSecondIndent;
	m_leapPoints[2].z = highest + data->m_leapSecondHeight;
	BezierSegment arc( m_leapPoints );
	m_leapSegments = max( 1, (Int)WWMath::Ceil( WWMath::Div_FixNaN( arc.getApproximateLength(), data->m_leapSpeed, 1.0f ) ) );
	m_leapSegment = 0;
	if( dx != 0.0f || dy != 0.0f )
		me->setOrientation( WWMath::Atan2Origin( dy, dx ) );

	me->setDisabled( DISABLED_HELD );
	m_leapNoAttack = !me->testStatus( OBJECT_STATUS_NO_ATTACK );
	if( m_leapNoAttack )
		me->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_NO_ATTACK ) );
	m_stage = STAGE_LEAP;
	m_stageStartFrame = TheGameLogic->getFrame();
	setLeapLook( data->m_leapPackInFlight && data->m_packTime > 0 ? 3 : 1 );
	setWakeFrame( me, UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature down on the ground: not held, nor still in the air
void FireWeaponAbilityUpdate::endLeap()
{
	Object *me = getObject();
	setLeapLook( 0 );
	me->clearDisabled( DISABLED_HELD );
	if( m_leapNoAttack )
		me->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_NO_ATTACK ) );
	m_leapNoAttack = FALSE;
	PhysicsBehavior *physics = me->getPhysics();
	if( physics != nullptr )
		physics->resetDynamicPhysics();
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature the leap's looks: 0 none, 1 in the air, 2 in the air and about to land, 3 packing
// as it takes off (LeapPackInFlight)
ModelConditionFlags FireWeaponAbilityUpdate::leapLook( Int look ) const
{
	const FireWeaponAbilityUpdateModuleData *data = getFireWeaponAbilityUpdateModuleData();
	ModelConditionFlags flags;
	if( look == 1 || look == 2 )
		flags.set( data->m_leapFlightConditions );
	if( look == 2 )
		flags.set( data->m_leapLandingConditions );
	if( look == 3 )
		flags.set( MODELCONDITION_PACKING );
	return flags;
}

//-------------------------------------------------------------------------------------------------
void FireWeaponAbilityUpdate::setLeapLook( Int look )
{
	if( look == m_leapLook )
		return;
	const ModelConditionFlags clr = leapLook( m_leapLook );
	const ModelConditionFlags set = leapLook( look );
	m_leapLook = look;
	if( clr.any() || set.any() )
		getObject()->clearAndSetModelConditionFlags( clr, set );
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
			// Tiberium Project: a leap first (its shots, if it has any, as it lands)
			if( data->m_leapSpeed > 0.0f && m_leapSegments == 0 )
			{
				startLeap();
				return UPDATE_SLEEP_NONE;
			}
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
				// (a leap that packed as it took off is done once it is down: Tiberium Project)
				if( data->m_packTime > 0 && !( data->m_leapPackInFlight && m_leapSegments > 0 ) )
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
		case STAGE_LEAP:
		{
			// Tiberium Project @feature along the arc, a step a frame
			++m_leapSegment;
			if( m_leapSegment >= m_leapSegments )
			{
				Coord3D down = m_leapPoints[3];
				down.z = TheTerrainLogic->getGroundHeight( down.x, down.y );
				me->setPosition( &down );
				endLeap();
				FXList::doFXPos( data->m_leapLandingFX, &down );
				// then its shots, if any, and its landing (PACKING)
				enterStage( STAGE_FIRE );
				return UPDATE_SLEEP_NONE;
			}
			BezierSegment arc( m_leapPoints );
			Coord3D at;
			arc.evaluateBezSegmentAtT( (Real)m_leapSegment / (Real)m_leapSegments, &at );
			me->setPosition( &at );
			if( data->m_leapPackInFlight && (UnsignedInt)m_leapSegment < data->m_packTime )
				setLeapLook( 3 );
			else if( data->m_leapLandingTime > 0 && (UnsignedInt)( m_leapSegments - m_leapSegment ) <= data->m_leapLandingTime )
				setLeapLook( 2 );
			else
				setLeapLook( 1 );
			return UPDATE_SLEEP_NONE;
		}
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
// 2: Tiberium Project: the leap
//-------------------------------------------------------------------------------------------------
void FireWeaponAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 2;
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
	if( version >= 2 )
	{
		for( Int i = 0; i < 4; ++i )
			xfer->xferCoord3D( &m_leapPoints[i] );
		xfer->xferInt( &m_leapSegments );
		xfer->xferInt( &m_leapSegment );
		xfer->xferInt( &m_leapLook );
		xfer->xferBool( &m_leapNoAttack );
	}
}

//-------------------------------------------------------------------------------------------------
void FireWeaponAbilityUpdate::loadPostProcess()
{
	SpecialPowerUpdateModule::loadPostProcess();
}
