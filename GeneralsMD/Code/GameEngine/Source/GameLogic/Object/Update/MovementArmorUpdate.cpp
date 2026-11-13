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

// FILE: MovementArmorUpdate.cpp //////////////////////////////////////////////////////////////////
// Tiberium Project @feature a modifier carried while the object moves (see the header)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/MovementArmorUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/ShieldSphereUpdate.h"
#include "GameLogic/Object.h"

//-------------------------------------------------------------------------------------------------
MovementArmorUpdateModuleData::MovementArmorUpdateModuleData()
{
	m_modifier = NAMEKEY_INVALID;
	m_minimumSpeed = 0.0f;
	m_updateRefresh = 0;
	m_removeDelay = 0;
	m_deactivateShieldSphere = FALSE;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void MovementArmorUpdateModuleData::buildFieldParse( MultiIniFieldParse &p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "AttributeModifier",			AttributeModifierStore::parseModifierName,	nullptr, offsetof( MovementArmorUpdateModuleData, m_modifier ) },
		{ "MinimumSpeed",						INI::parseVelocityReal,											nullptr, offsetof( MovementArmorUpdateModuleData, m_minimumSpeed ) },
		{ "UpdateRefresh",					INI::parseDurationUnsignedInt,							nullptr, offsetof( MovementArmorUpdateModuleData, m_updateRefresh ) },
		{ "RemoveDelayTime",				INI::parseDurationUnsignedInt,							nullptr, offsetof( MovementArmorUpdateModuleData, m_removeDelay ) },
		{ "DeactivateShieldSphere",	INI::parseBool,															nullptr, offsetof( MovementArmorUpdateModuleData, m_deactivateShieldSphere ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
MovementArmorUpdate::MovementArmorUpdate( Thing *thing, const ModuleData *moduleData ) :
	UpdateModule( thing, moduleData ),
	m_active( FALSE ),
	m_lastMovingFrame( 0 )
{
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
MovementArmorUpdate::~MovementArmorUpdate()
{
}

//-------------------------------------------------------------------------------------------------
void MovementArmorUpdate::setActive( Bool on )
{
	if( on == m_active )
		return;
	m_active = on;
	const MovementArmorUpdateModuleData *data = getMovementArmorUpdateModuleData();
	Object *me = getObject();
	const AttributeModifierTemplate *modifier = data->m_modifier != NAMEKEY_INVALID ? TheAttributeModifierStore->find( data->m_modifier ) : nullptr;
	if( modifier != nullptr )
	{
		if( on )
			me->applyAttributeModifier( modifier, me->getID() );
		else
			me->removeAttributeModifier( modifier, me->getID() );
	}
	if( data->m_deactivateShieldSphere )
		ShieldSphereUpdate::suppress( me, on );
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime MovementArmorUpdate::update()
{
	const MovementArmorUpdateModuleData *data = getMovementArmorUpdateModuleData();
	Object *me = getObject();
	if( me->isEffectivelyDead() )
		return UPDATE_SLEEP_FOREVER;
	const UnsignedInt now = TheGameLogic->getFrame();
	// (a thing at rest still has a breath of speed left as it settles: under a hundredth of a unit a frame is standing)
	const Real STANDING = 0.01f;
	const PhysicsBehavior *physics = me->getPhysics();
	const Real speed = physics != nullptr ? physics->getVelocityMagnitude() : 0.0f;
	if( speed > STANDING && speed > data->m_minimumSpeed )
	{
		m_lastMovingFrame = now;
		setActive( TRUE );
	}
	else if( m_active && now - m_lastMovingFrame >= data->m_removeDelay )
		setActive( FALSE );
	return data->m_updateRefresh > 1 ? UPDATE_SLEEP( data->m_updateRefresh ) : UPDATE_SLEEP_NONE;
}

// ------------------------------------------------------------------------------------------------
void MovementArmorUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void MovementArmorUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	xfer->xferBool( &m_active );
	xfer->xferUnsignedInt( &m_lastMovingFrame );
}

// ------------------------------------------------------------------------------------------------
void MovementArmorUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
