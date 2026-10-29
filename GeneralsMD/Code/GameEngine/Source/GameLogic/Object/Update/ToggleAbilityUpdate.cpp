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

#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/WeaponSetType.h"
#include "GameLogic/Module/AIUpdate.h"

//-------------------------------------------------------------------------------------------------
ToggleAbilityUpdate::ToggleAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	m_initialized = FALSE;
	m_toggled = FALSE;
	m_switching = FALSE;
	m_switchEndFrame = 0;
	m_revertFrame = 0;
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );	// the first state's looks, once drawn
}

//-------------------------------------------------------------------------------------------------
ToggleAbilityUpdate::~ToggleAbilityUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool ToggleAbilityUpdate::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	if( specialPowerTemplate != getToggleAbilityUpdateModuleData()->m_specialPowerTemplate )
		return FALSE;
	Object *me = getObject();
	// (ours, even when it cannot switch now: busy switching, or disabled)
	if( !m_switching && !me->isDisabled() && !me->isEffectivelyDead() )
		beginSwitch();
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void ToggleAbilityUpdate::beginSwitch()
{
	const ToggleAbilityUpdateModuleData *data = getToggleAbilityUpdateModuleData();
	Object *me = getObject();
	m_switching = TRUE;
	m_revertFrame = 0;
	const UnsignedInt time = ( m_toggled && data->m_hasBackTime ) ? data->m_transitionBackTime : data->m_transitionTime;
	m_switchEndFrame = TheGameLogic->getFrame() + time;
	if( time > 0 )
	{
		// it stands still while it switches
		AIUpdateInterface *ai = me->getAI();
		if( ai != nullptr )
			ai->aiIdle( CMD_FROM_AI );
		me->clearAndSetModelConditionFlags( ModelConditionFlags(), switchingConditions( !m_toggled ) );
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
		me->getAI()->setLocomotorUpgrade( toggled );
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
	if( m_switching )
	{
		if( now < m_switchEndFrame )
			return UPDATE_SLEEP( m_switchEndFrame - now );
		me->clearAndSetModelConditionFlags( switchingConditions( !m_toggled ), ModelConditionFlags() );
		m_switching = FALSE;
		m_toggled = !m_toggled;
		applyState( m_toggled );
		if( m_toggled && data->m_autoRevertTime > 0 )
			m_revertFrame = now + data->m_autoRevertTime;
	}
	if( m_toggled && m_revertFrame != 0 )
	{
		if( now >= m_revertFrame )
		{
			beginSwitch();
			return m_switchEndFrame > now ? UPDATE_SLEEP( m_switchEndFrame - now ) : UPDATE_SLEEP_NONE;
		}
		return UPDATE_SLEEP( m_revertFrame - now );
	}
	return UPDATE_SLEEP_FOREVER;
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
//-------------------------------------------------------------------------------------------------
void ToggleAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	SpecialPowerUpdateModule::xfer( xfer );

	xfer->xferBool( &m_initialized );
	xfer->xferBool( &m_toggled );
	xfer->xferBool( &m_switching );
	xfer->xferUnsignedInt( &m_switchEndFrame );
	xfer->xferUnsignedInt( &m_revertFrame );
}

//-------------------------------------------------------------------------------------------------
void ToggleAbilityUpdate::loadPostProcess()
{
	SpecialPowerUpdateModule::loadPostProcess();
}
