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


// FILE: GrabSpecialAbilityUpdate.cpp /////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's Nod Scorpion's claw (see the header)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/GrabSpecialAbilityUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

//-------------------------------------------------------------------------------------------------
GrabSpecialAbilityUpdateModuleData::GrabSpecialAbilityUpdateModuleData()
{
	m_specialPowerTemplate = nullptr;
	m_stopSpecialPowerTemplate = nullptr;
	m_rechargeOnRelease = nullptr;
	m_startAbilityRange = 0.0f;
	m_allowNeutralVictims = TRUE;
	m_victimModifier = NAMEKEY_INVALID;
	m_ownerModifier = NAMEKEY_INVALID;
	m_grabWeaponInterval = LOGICFRAMES_PER_SECOND / 5;
}

//-------------------------------------------------------------------------------------------------
/** VictimBehavior = KindOf:<kinds> [StatusSelf:<statuses>] [DragToBone:<bones>] [DragRate:<distance a second>]
	[LiftHeight:<height>], the lists comma-separated */
//-------------------------------------------------------------------------------------------------
void GrabSpecialAbilityUpdateModuleData::parseVictimBehavior( INI *ini, void *instance, void * /*store*/, const void * /*userData*/ )
{
	GrabSpecialAbilityUpdateModuleData *self = (GrabSpecialAbilityUpdateModuleData *)instance;
	GrabVictimBehavior behavior;
	behavior.m_dragRate = 0.0f;
	behavior.m_liftHeight = 0.0f;
	for( const char *token = ini->getNextTokenOrNull(); token != nullptr; token = ini->getNextTokenOrNull() )
	{
		AsciiString word( token );
		const char *colon = strchr( token, ':' );
		if( colon == nullptr )
			throw INI_INVALID_DATA;
		AsciiString key, value;
		key.set( token, (Int)( colon - token ) );
		value = colon + 1;
		if( stricmp( key.str(), "DragRate" ) == 0 )
			behavior.m_dragRate = (Real)atof( value.str() ) / LOGICFRAMES_PER_SECOND;
		else if( stricmp( key.str(), "LiftHeight" ) == 0 )
			behavior.m_liftHeight = (Real)atof( value.str() );
		else
		{
			// the lists
			AsciiString rest = value, item;
			while( rest.nextToken( &item, "," ) )
			{
				if( stricmp( key.str(), "KindOf" ) == 0 )
				{
					if( !behavior.m_kindOf.setBitByName( item.str() ) )
						throw INI_INVALID_DATA;
				}
				else if( stricmp( key.str(), "StatusSelf" ) == 0 )
				{
					if( !behavior.m_statusSelf.setBitByName( item.str() ) )
						throw INI_INVALID_DATA;
				}
				else if( stricmp( key.str(), "DragToBone" ) == 0 )
					behavior.m_dragToBones.push_back( item );
				else
					throw INI_INVALID_DATA;
			}
		}
	}
	self->m_victimBehaviors.push_back( behavior );
}

//-------------------------------------------------------------------------------------------------
/*static*/ void GrabSpecialAbilityUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "SpecialPowerTemplate",					INI::parseSpecialPowerTemplate,				nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_specialPowerTemplate ) },
		{ "StopSpecialPowerTemplate",			INI::parseSpecialPowerTemplate,				nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_stopSpecialPowerTemplate ) },
		{ "OnReleaseRechargeSpecialPower",	INI::parseSpecialPowerTemplate,				nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_rechargeOnRelease ) },
		{ "StartAbilityRange",						INI::parseReal,												nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_startAbilityRange ) },
		{ "VictimKindOf",									KindOfMaskType::parseFromINI,					nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_victimKindOf ) },
		{ "VictimForbiddenKindOf",				KindOfMaskType::parseFromINI,					nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_victimForbiddenKindOf ) },
		{ "AllowNeutralVictims",					INI::parseBool,												nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_allowNeutralVictims ) },
		{ "StatusSelf",										ObjectStatusMaskType::parseFromINI,		nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_statusSelf ) },
		{ "StatusVictim",									ObjectStatusMaskType::parseFromINI,		nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_statusVictim ) },
		{ "ModelConditionsSelf",					ModelConditionFlags::parseFromINI,		nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_modelConditionsSelf ) },
		{ "ReleaseConditions",						ModelConditionFlags::parseFromINI,		nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_releaseConditions ) },
		{ "CommandSetGrabbing",						INI::parseAsciiString,								nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_commandSetGrabbing ) },
		{ "VictimAttributeModifier",			AttributeModifierStore::parseModifierName,	nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_victimModifier ) },
		{ "OwnerAttributeModifier",				AttributeModifierStore::parseModifierName,	nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_ownerModifier ) },
		{ "VictimBehavior",								parseVictimBehavior,									nullptr, 0 },
		{ "GrabWeapon",										INI::parseAsciiString,								nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_grabWeaponName ) },
		{ "GrabWeaponInterval",						INI::parseDurationUnsignedInt,				nullptr, offsetof( GrabSpecialAbilityUpdateModuleData, m_grabWeaponInterval ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
GrabSpecialAbilityUpdate::GrabSpecialAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	m_stage = STAGE_IDLE;
	m_victimID = INVALID_ID;
	m_stageStartFrame = 0;
	m_nextShotFrame = 0;
	m_behaviorIndex = -1;
	m_moveIssued = FALSE;
	m_attached = FALSE;
	m_commandSetChanged = FALSE;
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
GrabSpecialAbilityUpdate::~GrabSpecialAbilityUpdate()
{
}

//-------------------------------------------------------------------------------------------------
const GrabVictimBehavior *GrabSpecialAbilityUpdate::behaviorFor( const Object *victim ) const
{
	const GrabSpecialAbilityUpdateModuleData *data = getGrabSpecialAbilityUpdateModuleData();
	for( size_t i = 0; i < data->m_victimBehaviors.size(); ++i )
	{
		const GrabVictimBehavior &b = data->m_victimBehaviors[ i ];
		if( !b.m_kindOf.any() || victim->isAnyKindOf( b.m_kindOf ) )
			return &b;
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
Bool GrabSpecialAbilityUpdate::isValidVictim( const Object *victim, Bool held ) const
{
	const GrabSpecialAbilityUpdateModuleData *data = getGrabSpecialAbilityUpdateModuleData();
	const Object *me = getObject();
	if( victim == nullptr || victim == me || victim->isEffectivelyDead() || victim->isDestroyed() )
		return FALSE;
	const Relationship r = me->getRelationship( victim );
	if( r != ENEMIES && !( r == NEUTRAL && data->m_allowNeutralVictims ) )
		return FALSE;
	if( data->m_victimKindOf.any() && !victim->isAnyKindOf( data->m_victimKindOf ) )
		return FALSE;
	if( data->m_victimForbiddenKindOf.any() && victim->isAnyKindOf( data->m_victimForbiddenKindOf ) )
		return FALSE;
	if( !data->m_victimBehaviors.empty() && behaviorFor( victim ) == nullptr )
		return FALSE;
	// (nor one in the air, or inside something: one it holds is lifted, maybe)
	if( !held && ( victim->isSignificantlyAboveTerrain() || victim->getContainedBy() != nullptr ) )
		return FALSE;
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** it shows a look it may not hold in (C&C4's burrowed Scorpion: the grab's PreventConditions) */
Bool GrabSpecialAbilityUpdate::showsReleaseCondition() const
{
	const GrabSpecialAbilityUpdateModuleData *data = getGrabSpecialAbilityUpdateModuleData();
	const Drawable *draw = getObject()->getDrawable();
	return data->m_releaseConditions.any() && draw != nullptr && draw->getModelConditionFlags().anyIntersectionWith( data->m_releaseConditions );
}

//-------------------------------------------------------------------------------------------------
Bool GrabSpecialAbilityUpdate::refusesSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D * /*targetPos*/ ) const
{
	const GrabSpecialAbilityUpdateModuleData *data = getGrabSpecialAbilityUpdateModuleData();
	if( specialPowerTemplate == data->m_stopSpecialPowerTemplate && data->m_stopSpecialPowerTemplate != nullptr )
		return m_stage != STAGE_HOLD;
	if( specialPowerTemplate != data->m_specialPowerTemplate )
		return FALSE;
	if( m_stage == STAGE_HOLD )
		return TRUE;
	if( showsReleaseCondition() )
		return TRUE;
	// (one held already, by this claw or another: SAGE's GRAB_UPDATE_VICTIM)
	if( targetObj != nullptr && ( !isValidVictim( targetObj ) || targetObj->isDisabledByType( DISABLED_HELD ) ) )
		return TRUE;
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool GrabSpecialAbilityUpdate::isSpecialPowerUnavailable( const SpecialPowerTemplate *specialPowerTemplate ) const
{
	const GrabSpecialAbilityUpdateModuleData *data = getGrabSpecialAbilityUpdateModuleData();
	if( specialPowerTemplate == data->m_stopSpecialPowerTemplate && data->m_stopSpecialPowerTemplate != nullptr )
		return m_stage != STAGE_HOLD;
	if( specialPowerTemplate == data->m_specialPowerTemplate )
		return m_stage == STAGE_HOLD;
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool GrabSpecialAbilityUpdate::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint * /*way*/, UnsignedInt /*commandOptions*/ )
{
	const GrabSpecialAbilityUpdateModuleData *data = getGrabSpecialAbilityUpdateModuleData();
	// the claw opens
	if( specialPowerTemplate == data->m_stopSpecialPowerTemplate && data->m_stopSpecialPowerTemplate != nullptr )
	{
		if( m_stage != STAGE_HOLD )
			return FALSE;
		release( TRUE );
		return TRUE;
	}
	if( specialPowerTemplate != data->m_specialPowerTemplate || targetObj == nullptr )
		return FALSE;
	if( refusesSpecialPower( specialPowerTemplate, targetObj, targetPos ) )
		return FALSE;
	Object *me = getObject();
	if( me->isEffectivelyDead() || me->isDisabled() )
		return FALSE;
	// ordered again on the way to another: this one instead
	m_victimID = targetObj->getID();
	m_moveIssued = FALSE;
	m_stage = STAGE_APPROACH;
	m_stageStartFrame = TheGameLogic->getFrame();
	setWakeFrame( me, UPDATE_SLEEP_NONE );
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void GrabSpecialAbilityUpdate::stop()
{
	m_stage = STAGE_IDLE;
	m_victimID = INVALID_ID;
	m_moveIssued = FALSE;
}

//-------------------------------------------------------------------------------------------------
/** where the victim is carried: the bone of the unit's model (else just ahead of the unit, clear of it), the ground below */
//-------------------------------------------------------------------------------------------------
Bool GrabSpecialAbilityUpdate::holdPoint( const GrabVictimBehavior *behavior, const Object *victim, Coord3D *pos ) const
{
	const Object *me = getObject();
	Bool found = FALSE;
	if( behavior != nullptr )
	{
		for( size_t i = 0; i < behavior->m_dragToBones.size() && !found; ++i )
			found = me->getSingleLogicalBonePosition( behavior->m_dragToBones[ i ].str(), pos, nullptr );
	}
	if( !found )
	{
		const Real reach = me->getGeometryInfo().getBoundingCircleRadius() + victim->getGeometryInfo().getBoundingCircleRadius();
		const Coord3D *at = me->getPosition();
		const Real angle = me->getOrientation();
		pos->x = at->x + reach * WWMath::Cos( angle );
		pos->y = at->y + reach * WWMath::Sin( angle );
		pos->z = at->z;
	}
	pos->z = TheTerrainLogic->getGroundHeight( pos->x, pos->y ) + ( behavior != nullptr ? behavior->m_liftHeight : 0.0f );
	return found;
}

//-------------------------------------------------------------------------------------------------
void GrabSpecialAbilityUpdate::grab( Object *victim )
{
	const GrabSpecialAbilityUpdateModuleData *data = getGrabSpecialAbilityUpdateModuleData();
	Object *me = getObject();
	const GrabVictimBehavior *behavior = behaviorFor( victim );
	m_behaviorIndex = behavior != nullptr ? (Int)( behavior - &data->m_victimBehaviors[ 0 ] ) : -1;
	m_stage = STAGE_HOLD;
	m_stageStartFrame = TheGameLogic->getFrame();
	m_nextShotFrame = m_stageStartFrame;
	m_attached = FALSE;

	// the unit: stopped, its statuses, look, modifier and command set
	AIUpdateInterface *ai = me->getAI();
	if( ai != nullptr )
		ai->aiIdle( CMD_FROM_AI );
	ObjectStatusMaskType self = data->m_statusSelf;
	if( behavior != nullptr )
		self.set( behavior->m_statusSelf );
	m_selfStatusAdded = self;
	m_selfStatusAdded.clear( me->getStatusBits() );
	me->setStatus( m_selfStatusAdded );
	m_selfConditionsAdded = data->m_modelConditionsSelf;
	if( m_selfConditionsAdded.any() )
		me->setModelConditionFlags( m_selfConditionsAdded );
	if( TheAttributeModifierStore != nullptr )
		me->applyAttributeModifier( TheAttributeModifierStore->find( data->m_ownerModifier ), me->getID() );
	if( data->m_commandSetGrabbing.isNotEmpty() )
	{
		m_formerCommandSetOverride = me->getCommandSetStringOverride();
		me->setCommandSetStringOverride( data->m_commandSetGrabbing );
		m_commandSetChanged = TRUE;
		TheControlBar->markUIDirty();	// (in case it is selected)
	}

	// the victim: held still, out of everyone's hands and sights, its modifier
	ObjectStatusMaskType held = data->m_statusVictim;
	held.set( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_NO_COLLISIONS ) );
	m_victimStatusAdded = held;
	m_victimStatusAdded.clear( victim->getStatusBits() );
	victim->setStatus( m_victimStatusAdded );
	AIUpdateInterface *victimAI = victim->getAI();
	if( victimAI != nullptr )
		victimAI->aiIdle( CMD_FROM_AI );
	victim->setDisabled( DISABLED_HELD );
	PhysicsBehavior *physics = victim->getPhysics();
	if( physics != nullptr )
		physics->resetDynamicPhysics();
	if( m_victimStatusAdded.test( OBJECT_STATUS_UNSELECTABLE ) || m_victimStatusAdded.test( OBJECT_STATUS_MASKED ) )
		TheGameLogic->deselectObject( victim, PLAYERMASK_ALL, TRUE );
	if( TheAttributeModifierStore != nullptr )
		victim->applyAttributeModifier( TheAttributeModifierStore->find( data->m_victimModifier ), me->getID() );
}

//-------------------------------------------------------------------------------------------------
void GrabSpecialAbilityUpdate::release( Bool recharge )
{
	const GrabSpecialAbilityUpdateModuleData *data = getGrabSpecialAbilityUpdateModuleData();
	Object *me = getObject();
	if( m_stage != STAGE_HOLD )
	{
		stop();
		return;
	}
	Object *victim = m_victimID != INVALID_ID ? TheGameLogic->findObjectByID( m_victimID ) : nullptr;
	if( victim != nullptr )
	{
		victim->clearStatus( m_victimStatusAdded );
		victim->clearDisabled( DISABLED_HELD );
		if( TheAttributeModifierStore != nullptr )
			victim->removeAttributeModifier( TheAttributeModifierStore->find( data->m_victimModifier ), me->getID() );
		if( !victim->isEffectivelyDead() )
		{
			// set down on the ground where it is, at a stop
			Coord3D pos = *victim->getPosition();
			pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );
			victim->setPosition( &pos );
			PhysicsBehavior *physics = victim->getPhysics();
			if( physics != nullptr )
				physics->resetDynamicPhysics();
			AIUpdateInterface *victimAI = victim->getAI();
			if( victimAI != nullptr )
				victimAI->aiIdle( CMD_FROM_AI );
		}
	}
	me->clearStatus( m_selfStatusAdded );
	if( m_selfConditionsAdded.any() )
		me->clearModelConditionFlags( m_selfConditionsAdded );
	if( TheAttributeModifierStore != nullptr )
		me->removeAttributeModifier( TheAttributeModifierStore->find( data->m_ownerModifier ), me->getID() );
	if( m_commandSetChanged )
	{
		me->setCommandSetStringOverride( m_formerCommandSetOverride );
		m_commandSetChanged = FALSE;
		m_formerCommandSetOverride.clear();
		TheControlBar->markUIDirty();
	}
	m_selfStatusAdded.clear();
	m_victimStatusAdded.clear();
	m_selfConditionsAdded.clear();
	m_behaviorIndex = -1;
	m_attached = FALSE;
	stop();

	// the grab reloads from now (C&C4's OnReleaseRechargeSpecialPower)
	if( recharge && data->m_rechargeOnRelease != nullptr )
	{
		SpecialPowerModuleInterface *power = me->getSpecialPowerModule( data->m_rechargeOnRelease );
		if( power != nullptr )
			power->startPowerRecharge();
	}
}

//-------------------------------------------------------------------------------------------------
void GrabSpecialAbilityUpdate::onDelete()
{
	// (gone without dying first: the victim is let go)
	if( m_stage == STAGE_HOLD )
		release( FALSE );
	SpecialPowerUpdateModule::onDelete();
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime GrabSpecialAbilityUpdate::update()
{
	const GrabSpecialAbilityUpdateModuleData *data = getGrabSpecialAbilityUpdateModuleData();
	Object *me = getObject();
	if( m_stage == STAGE_IDLE )
		return UPDATE_SLEEP_FOREVER;
	Object *victim = m_victimID != INVALID_ID ? TheGameLogic->findObjectByID( m_victimID ) : nullptr;
	const UnsignedInt now = TheGameLogic->getFrame();

	if( m_stage == STAGE_APPROACH )
	{
		if( me->isEffectivelyDead() || me->isDisabled() || victim == nullptr || !isValidVictim( victim ) || victim->isDisabledByType( DISABLED_HELD )
			|| showsReleaseCondition() )
		{
			stop();
			return UPDATE_SLEEP_FOREVER;
		}
		const Real reach = data->m_startAbilityRange;
		if( ThePartitionManager->getDistanceSquared( me, victim, FROM_BOUNDINGSPHERE_2D ) <= reach * reach )
		{
			grab( victim );
			return UPDATE_SLEEP_NONE;
		}
		AIUpdateInterface *ai = me->getAI();
		if( ai == nullptr )
		{
			stop();
			return UPDATE_SLEEP_FOREVER;
		}
		if( !m_moveIssued )
		{
			ai->aiMoveToObject( victim, CMD_FROM_AI );
			m_moveIssued = TRUE;
		}
		else if( ai->getLastCommandSource() == CMD_FROM_PLAYER || ( now - m_stageStartFrame > LOGICFRAMES_PER_SECOND && ai->isIdle() ) )
		{
			// told to do something else, or no way there (SAGE's FAIL_WITH_INVALID_APPROACH): it gives up
			stop();
			return UPDATE_SLEEP_FOREVER;
		}
		return UPDATE_SLEEP( 3 );
	}

	// holding
	Bool over = me->isEffectivelyDead() || victim == nullptr || victim->isEffectivelyDead() || victim->isDestroyed();
	if( !over )
	{
		// (disabled by anything but being held itself: the claw lets go)
		DisabledMaskType disabled = me->getDisabledFlags();
		disabled.clear( MAKE_DISABLED_MASK( DISABLED_HELD ) );
		over = disabled.any() || !isValidVictim( victim, TRUE ) || showsReleaseCondition();
	}
	if( over )
	{
		release( TRUE );
		return UPDATE_SLEEP_FOREVER;
	}

	// dragged in, then carried at the claw
	const GrabVictimBehavior *behavior = m_behaviorIndex >= 0 && m_behaviorIndex < (Int)data->m_victimBehaviors.size() ? &data->m_victimBehaviors[ m_behaviorIndex ] : nullptr;
	Coord3D goal;
	holdPoint( behavior, victim, &goal );
	Coord3D at = *victim->getPosition();
	const Real dx = goal.x - at.x, dy = goal.y - at.y, dz = goal.z - at.z;
	const Real dist = WWMath::Sqrt( dx * dx + dy * dy + dz * dz );
	const Real step = behavior != nullptr && behavior->m_dragRate > 0.0f ? behavior->m_dragRate : dist;
	if( m_attached || dist <= step )
	{
		m_attached = TRUE;
		at = goal;
	}
	else
	{
		const Real f = step / dist;
		at.x += dx * f;
		at.y += dy * f;
		at.z += dz * f;
	}
	victim->setPosition( &at );

	// the weapon at it, every interval
	if( now >= m_nextShotFrame && data->m_grabWeaponName.isNotEmpty() )
	{
		m_nextShotFrame = now + max( (UnsignedInt)1, data->m_grabWeaponInterval );
		const WeaponTemplate *weapon = TheWeaponStore->findWeaponTemplate( data->m_grabWeaponName );
		if( weapon != nullptr )
			TheWeaponStore->createAndFireTempWeapon( weapon, me, victim );
	}
	return UPDATE_SLEEP_NONE;
}

// ------------------------------------------------------------------------------------------------
void GrabSpecialAbilityUpdate::crc( Xfer *xfer )
{
	SpecialPowerUpdateModule::crc( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void GrabSpecialAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	SpecialPowerUpdateModule::xfer( xfer );
	Int stage = (Int)m_stage;
	xfer->xferInt( &stage );
	m_stage = (Stage)stage;
	xfer->xferObjectID( &m_victimID );
	xfer->xferUnsignedInt( &m_stageStartFrame );
	xfer->xferUnsignedInt( &m_nextShotFrame );
	xfer->xferInt( &m_behaviorIndex );
	xfer->xferBool( &m_moveIssued );
	xfer->xferBool( &m_attached );
	m_selfStatusAdded.xfer( xfer );
	m_victimStatusAdded.xfer( xfer );
	m_selfConditionsAdded.xfer( xfer );
	xfer->xferAsciiString( &m_formerCommandSetOverride );
	xfer->xferBool( &m_commandSetChanged );
}

// ------------------------------------------------------------------------------------------------
void GrabSpecialAbilityUpdate::loadPostProcess()
{
	SpecialPowerUpdateModule::loadPostProcess();
}
