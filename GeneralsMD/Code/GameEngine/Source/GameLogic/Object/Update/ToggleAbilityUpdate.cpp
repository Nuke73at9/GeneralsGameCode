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

// FILE: ToggleAbilityUpdate.cpp //////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's toggled unit abilities (see ToggleAbilityUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/ToggleAbilityUpdate.h"
#include "GameLogic/Module/FireWeaponUpdate.h"	// Tiberium Project

#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/WeaponSetType.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"

//-------------------------------------------------------------------------------------------------
ToggleAbilityUpdate::ToggleAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	m_initialized = FALSE;
	m_toggled = FALSE;
	m_switching = FALSE;
	m_switchEndFrame = 0;
	m_revertFrame = 0;
	m_heldNoAttack = FALSE;
	m_approaching = FALSE;
	m_moveIssued = FALSE;
	m_approachStartFrame = 0;
	m_offTarget.zero();
	m_landFrom.zero();
	m_switchStartFrame = 0;
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );	// the first state's looks, once drawn
}

//-------------------------------------------------------------------------------------------------
ToggleAbilityUpdate::~ToggleAbilityUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool ToggleAbilityUpdate::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const ToggleAbilityUpdateModuleData *data = getToggleAbilityUpdateModuleData();
	const Bool off = specialPowerTemplate != nullptr && specialPowerTemplate == data->m_offSpecialPowerTemplate;
	if( specialPowerTemplate != data->m_specialPowerTemplate && !off )
		return FALSE;
	Object *me = getObject();
	// (ours, even when it cannot switch now: busy switching, or disabled; Tiberium Project: or that power's state is
	// the one it is in)
	if( m_switching || me->isDisabled() || me->isEffectivelyDead() || refusesSpecialPower( specialPowerTemplate, targetObj, targetPos ) )
		return TRUE;
	m_approaching = FALSE;
	// Tiberium Project @feature switching back at a spot: there first (SAGE's landing at the spot ordered)
	const Coord3D *spot = targetObj != nullptr ? targetObj->getPosition() : targetPos;
	if( off && spot != nullptr && data->m_offStartAbilityRange > 0.0f && me->getAI() != nullptr )
	{
		m_offTarget = *spot;
		const Coord3D *at = me->getPosition();
		const Real dx = spot->x - at->x, dy = spot->y - at->y;
		if( dx * dx + dy * dy > data->m_offStartAbilityRange * data->m_offStartAbilityRange )
		{
			m_approaching = TRUE;
			m_moveIssued = FALSE;
			m_approachStartFrame = TheGameLogic->getFrame();
			setWakeFrame( me, UPDATE_SLEEP_NONE );
			return TRUE;
		}
	}
	beginSwitch();
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature with OffSpecialPowerTemplate, each of its two powers does one thing: refused in the state
// it switches to, and while it switches
Bool ToggleAbilityUpdate::refusesSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object * /*targetObj*/, const Coord3D * /*targetPos*/ ) const
{
	return isSpecialPowerUnavailable( specialPowerTemplate );
}

//-------------------------------------------------------------------------------------------------
Bool ToggleAbilityUpdate::isSpecialPowerUnavailable( const SpecialPowerTemplate *specialPowerTemplate ) const
{
	const ToggleAbilityUpdateModuleData *data = getToggleAbilityUpdateModuleData();
	// Tiberium Project @feature SAGE's ToggleState ForbiddenStatus: not into the second state while it has one of them (C&C4's
	// burrowing units in a Seismic Disturbance's quake; update() brings out those in it); back out of it is always allowed
	if( specialPowerTemplate != nullptr && specialPowerTemplate == data->m_specialPowerTemplate && !m_toggled && !m_switching
		&& data->m_forbiddenStatus.any() && getObject()->getStatusBits().testForAny( data->m_forbiddenStatus ) )
		return TRUE;
	if( data->m_offSpecialPowerTemplate == nullptr || specialPowerTemplate == nullptr )
		return FALSE;
	if( specialPowerTemplate == data->m_specialPowerTemplate )
		return m_switching || m_toggled;
	if( specialPowerTemplate == data->m_offSpecialPowerTemplate )
		return m_switching || !m_toggled;
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void ToggleAbilityUpdate::beginSwitch()
{
	const ToggleAbilityUpdateModuleData *data = getToggleAbilityUpdateModuleData();
	Object *me = getObject();
	m_switching = TRUE;
	m_revertFrame = 0;
	m_approaching = FALSE;
	const UnsignedInt time = ( m_toggled && data->m_hasBackTime ) ? data->m_transitionBackTime : data->m_transitionTime;
	m_switchStartFrame = TheGameLogic->getFrame();
	m_switchEndFrame = TheGameLogic->getFrame() + time;
	m_landFrom = *me->getPosition();	// (Tiberium Project: where it comes down from, LandOnSwitchBack)
	// Tiberium Project @feature the timed weapons the state it goes into switches on (its InitialDelay from now)
	if( m_toggled ? data->m_normalArmsTimedWeapons : data->m_toggledArmsTimedWeapons )
		FireWeaponUpdate::armAll( me );
	if( time > 0 )
	{
		// it stands still while it switches
		AIUpdateInterface *ai = me->getAI();
		if( ai != nullptr )
			ai->aiIdle( CMD_FROM_AI );
		me->clearAndSetModelConditionFlags( ModelConditionFlags(), switchingConditions( !m_toggled ) );
		// Tiberium Project @bugfix and is held: no orders, no shots (C&C4's Zone Lancer fired its rifle all through
		// its two seconds of deploying)
		me->setDisabled( DISABLED_HELD );
		// (its weapons' wind-up, cut short, shows no longer)
		ModelConditionFlags windUp;
		windUp.set( MODELCONDITION_PREATTACK_A );
		windUp.set( MODELCONDITION_PREATTACK_B );
		windUp.set( MODELCONDITION_PREATTACK_C );
		windUp.set( MODELCONDITION_PREATTACK_D );
		windUp.set( MODELCONDITION_PREATTACK_E );
		windUp.set( MODELCONDITION_PREATTACK_F );
		windUp.set( MODELCONDITION_PREATTACK_G );
		windUp.set( MODELCONDITION_PREATTACK_H );
		me->clearModelConditionFlags( windUp );
		m_heldNoAttack = !me->testStatus( OBJECT_STATUS_NO_ATTACK );
		if( m_heldNoAttack )
			me->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_NO_ATTACK ) );
	}
	setWakeFrame( me, UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
/** what it shows while switching into the second state (intoToggled) or back */
ModelConditionFlags ToggleAbilityUpdate::switchingConditions( Bool intoToggled ) const
{
	const ToggleAbilityUpdateModuleData *data = getToggleAbilityUpdateModuleData();
	const ModelConditionFlags &given = intoToggled ? data->m_toggleInConditions : data->m_toggleOutConditions;
	if( given.any() )
		return given;
	return MAKE_MODELCONDITION_MASK( intoToggled ? MODELCONDITION_UNPACKING : MODELCONDITION_PACKING );
}

//-------------------------------------------------------------------------------------------------
void ToggleAbilityUpdate::applyState( Bool toggled )
{
	const ToggleAbilityUpdateModuleData *data = getToggleAbilityUpdateModuleData();
	Object *me = getObject();
	me->clearAndSetModelConditionFlags( toggled ? data->m_normalConditions : data->m_toggledConditions,
		toggled ? data->m_toggledConditions : data->m_normalConditions );
	if( data->m_toggledWeaponSet )
	{
		if( toggled )
			me->setWeaponSetFlag( WEAPONSET_ABILITY_TOGGLED );
		else
			me->clearWeaponSetFlag( WEAPONSET_ABILITY_TOGGLED );
	}
	if( data->m_toggledLocomotor && me->getAI() != nullptr )
	{
		me->getAI()->setLocomotorUpgrade( toggled );
		// Tiberium Project @bugfix its new locomotor at work at once (an idle unit's AI sleeps: Kane's Wrath's Shadow,
		// on its glider, rose only a second later)
		me->getAI()->friend_wakeUp();
	}
	if( data->m_toggledStatus.any() )
		me->setStatus( data->m_toggledStatus, toggled );
	if( data->m_toggledModifier != NAMEKEY_INVALID )
	{
		const AttributeModifierTemplate *modifier = TheAttributeModifierStore->find( data->m_toggledModifier );
		if( toggled )
			me->applyAttributeModifier( modifier, me->getID() );
		else
			me->removeAttributeModifier( modifier, me->getID() );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature forced back to the first state (see the header): only from the second, once it is in it
//-------------------------------------------------------------------------------------------------
Bool ToggleAbilityUpdate::forceToDefault( const ModelConditionFlags &ifShows )
{
	const ToggleAbilityUpdateModuleData *data = getToggleAbilityUpdateModuleData();
	if( !m_toggled || m_switching )
		return FALSE;
	if( ifShows.any() && !data->m_toggledConditions.anyIntersectionWith( ifShows ) )
		return FALSE;
	beginSwitch();
	return TRUE;
}

void ToggleAbilityUpdate::forceAllToDefault( Object *obj, const ModelConditionFlags &ifShows )
{
	static const NameKeyType key = NAMEKEY( "ToggleAbilityUpdate" );
	if( obj == nullptr || obj->isEffectivelyDead() )
		return;
	for( BehaviorModule **m = obj->getBehaviorModules(); *m != nullptr; ++m )
	{
		if( (*m)->getModuleNameKey() == key )
			static_cast<ToggleAbilityUpdate *>( *m )->forceToDefault( ifShows );
	}
}

UpdateSleepTime ToggleAbilityUpdate::update()
{
	const ToggleAbilityUpdateModuleData *data = getToggleAbilityUpdateModuleData();
	Object *me = getObject();
	const UnsignedInt now = TheGameLogic->getFrame();
	if( !m_initialized )
	{
		m_initialized = TRUE;
		applyState( m_toggled );
	}
	// Tiberium Project @feature going to the spot where it switches back
	if( m_approaching )
	{
		AIUpdateInterface *ai = me->getAI();
		const Coord3D *at = me->getPosition();
		const Real dx = m_offTarget.x - at->x, dy = m_offTarget.y - at->y;
		const Real range = data->m_offStartAbilityRange;
		if( ai == nullptr || me->isEffectivelyDead() || !m_toggled )
			m_approaching = FALSE;
		else if( dx * dx + dy * dy <= range * range )
		{
			if( !me->isDisabled() )
				beginSwitch();
			else
				return UPDATE_SLEEP( 3 );
		}
		else if( !m_moveIssued )
		{
			ai->aiMoveToPosition( &m_offTarget, CMD_FROM_AI );
			m_moveIssued = TRUE;
			return UPDATE_SLEEP( 3 );
		}
		else
		{
			// told to go elsewhere: it does not switch back; stopped short (no way nearer): it does so there
			const Coord3D *goal = ai->getGoalPosition();
			const Real gx = goal != nullptr ? goal->x - m_offTarget.x : 0.0f, gy = goal != nullptr ? goal->y - m_offTarget.y : 0.0f;
			if( !ai->isIdle() && goal != nullptr && gx * gx + gy * gy > range * range )
				m_approaching = FALSE;
			else if( ai->isIdle() && now - m_approachStartFrame > LOGICFRAMES_PER_SECOND )
				beginSwitch();
			else
				return UPDATE_SLEEP( 3 );
		}
	}
	if( m_switching )
	{
		// Tiberium Project @feature coming down to the ground as it switches back (held: no physics meanwhile)
		if( m_toggled && data->m_landOnSwitchBack )
		{
			Coord3D pos = *me->getPosition();
			const Real ground = TheTerrainLogic->getGroundHeight( pos.x, pos.y );
			if( m_landFrom.z > ground )
			{
				const UnsignedInt span = m_switchEndFrame > m_switchStartFrame ? m_switchEndFrame - m_switchStartFrame : 1;
				const UnsignedInt done = now > m_switchStartFrame ? now - m_switchStartFrame : 0;
				const Real t = done >= span ? 1.0f : (Real)done / (Real)span;
				pos.z = m_landFrom.z + ( ground - m_landFrom.z ) * t;
				me->setPosition( &pos );
			}
			if( now < m_switchEndFrame )
				return UPDATE_SLEEP_NONE;
			PhysicsBehavior *physics = me->getPhysics();
			if( physics != nullptr )
				physics->resetDynamicPhysics();
		}
		if( now < m_switchEndFrame )
			return UPDATE_SLEEP( m_switchEndFrame - now );
		me->clearAndSetModelConditionFlags( switchingConditions( !m_toggled ), ModelConditionFlags() );
		m_switching = FALSE;
		// (free again: Tiberium Project)
		if( me->isDisabledByType( DISABLED_HELD ) )
			me->clearDisabled( DISABLED_HELD );
		if( m_heldNoAttack )
			me->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_NO_ATTACK ) );
		m_heldNoAttack = FALSE;
		m_toggled = !m_toggled;
		applyState( m_toggled );
		if( m_toggled && data->m_autoRevertTime > 0 )
			m_revertFrame = now + data->m_autoRevertTime;
	}
	// Tiberium Project @feature SAGE's ToggleState ObjectStatusValidation: out of the second state as soon as it gains one of
	// its forbidden statuses (C&C4's burrowed units brought up by a Seismic Disturbance's quake); looked at every few frames
	// while it is in that state, as nothing tells it of a status set
	const UnsignedInt forbiddenPoll = 5;
	const Bool watchForbidden = m_toggled && !m_switching && data->m_forbiddenStatus.any();
	if( watchForbidden && !me->isEffectivelyDead() && !me->isDisabled() && me->getStatusBits().testForAny( data->m_forbiddenStatus ) )
	{
		beginSwitch();
		return m_switchEndFrame > now ? UPDATE_SLEEP( m_switchEndFrame - now ) : UPDATE_SLEEP_NONE;
	}
	if( m_toggled && m_revertFrame != 0 )
	{
		if( now >= m_revertFrame )
		{
			beginSwitch();
			return m_switchEndFrame > now ? UPDATE_SLEEP( m_switchEndFrame - now ) : UPDATE_SLEEP_NONE;
		}
		const UnsignedInt wait = m_revertFrame - now;
		return UPDATE_SLEEP( watchForbidden && wait > forbiddenPoll ? forbiddenPoll : wait );
	}
	if( watchForbidden )
		return UPDATE_SLEEP( forbiddenPoll );
	return m_approaching ? UPDATE_SLEEP( 3 ) : UPDATE_SLEEP_FOREVER;
}

//-------------------------------------------------------------------------------------------------
void ToggleAbilityUpdate::crc( Xfer *xfer )
{
	SpecialPowerUpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
// 2: Tiberium Project: whether it set NO_ATTACK for the switch
// 3: Tiberium Project: the spot it goes to before switching back, and its landing
//-------------------------------------------------------------------------------------------------
void ToggleAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 3;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	SpecialPowerUpdateModule::xfer( xfer );

	xfer->xferBool( &m_initialized );
	xfer->xferBool( &m_toggled );
	xfer->xferBool( &m_switching );
	xfer->xferUnsignedInt( &m_switchEndFrame );
	xfer->xferUnsignedInt( &m_revertFrame );
	if( version >= 2 )
		xfer->xferBool( &m_heldNoAttack );
	if( version >= 3 )
	{
		xfer->xferBool( &m_approaching );
		xfer->xferBool( &m_moveIssued );
		xfer->xferUnsignedInt( &m_approachStartFrame );
		xfer->xferCoord3D( &m_offTarget );
		xfer->xferCoord3D( &m_landFrom );
		xfer->xferUnsignedInt( &m_switchStartFrame );
	}
}

//-------------------------------------------------------------------------------------------------
void ToggleAbilityUpdate::loadPostProcess()
{
	SpecialPowerUpdateModule::loadPostProcess();
}
