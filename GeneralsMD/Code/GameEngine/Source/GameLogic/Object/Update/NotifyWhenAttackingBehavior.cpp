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

// FILE: NotifyWhenAttackingBehavior.cpp //////////////////////////////////////////////////////////
// Tiberium Project @feature defences that fire together (see the header)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_WEAPONSLOTTYPE_NAMES	// TheWeaponSlotTypeNamesLookupList

#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/NotifyWhenAttackingBehavior.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"

//-------------------------------------------------------------------------------------------------
NotifyWhenAttackingBehaviorModuleData::NotifyWhenAttackingBehaviorModuleData()
{
	m_notifyRadius = 0.0f;
	m_sleepTime = 0;
	m_maxFriends = 100;
	m_weaponSlot = PRIMARY_WEAPON;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void NotifyWhenAttackingBehaviorModuleData::buildFieldParse( MultiIniFieldParse &p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "NotifyRadius",				INI::parseReal,									nullptr, offsetof( NotifyWhenAttackingBehaviorModuleData, m_notifyRadius ) },
		{ "SleepTime",					INI::parseDurationUnsignedInt,	nullptr, offsetof( NotifyWhenAttackingBehaviorModuleData, m_sleepTime ) },
		{ "MaxFriendsToNotify",	INI::parseInt,									nullptr, offsetof( NotifyWhenAttackingBehaviorModuleData, m_maxFriends ) },
		{ "Friend",							INI::parseAsciiStringVector,		nullptr, offsetof( NotifyWhenAttackingBehaviorModuleData, m_friendNames ) },
		{ "WeaponSlot",					INI::parseLookupList,						TheWeaponSlotTypeNamesLookupList, offsetof( NotifyWhenAttackingBehaviorModuleData, m_weaponSlot ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
NotifyWhenAttackingBehavior::NotifyWhenAttackingBehavior( Thing *thing, const ModuleData *moduleData ) :
	UpdateModule( thing, moduleData ),
	m_lastFriendCount( 0 )
{
	setWakeFrame( getObject(), UPDATE_SLEEP( LOGICFRAMES_PER_SECOND ) );
}

//-------------------------------------------------------------------------------------------------
NotifyWhenAttackingBehavior::~NotifyWhenAttackingBehavior()
{
}

//-------------------------------------------------------------------------------------------------
// attacking with its weapon: winding it up, firing it, aiming it (waiting on it between shots), or just fired
// (a turret's weapon, whose attack sets none of the object's own statuses)
//-------------------------------------------------------------------------------------------------
Bool NotifyWhenAttackingBehavior::isAttacking() const
{
	const NotifyWhenAttackingBehaviorModuleData *data = getNotifyWhenAttackingBehaviorModuleData();
	const Object *me = getObject();
	const Weapon *weapon = me->getWeaponInWeaponSlot( data->m_weaponSlot );
	AIUpdateInterface *ai = const_cast<Object *>( me )->getAI();
	if( weapon == nullptr || ai == nullptr )
		return FALSE;
	const Object *victim = ai->getCurrentVictim();
	for( Int t = 0; victim == nullptr && t < MAX_TURRETS; ++t )
		victim = ai->getTurretTargetObject( (WhichTurretType)t, FALSE );
	if( victim == nullptr || victim->isEffectivelyDead() )
		return FALSE;
	if( weapon->getStatus() == PRE_ATTACK || me->testStatus( OBJECT_STATUS_IS_FIRING_WEAPON ) || me->testStatus( OBJECT_STATUS_IS_AIMING_WEAPON ) )
		return TRUE;
	const UnsignedInt JUST_FIRED = LOGICFRAMES_PER_SECOND / 2;
	const UnsignedInt now = TheGameLogic->getFrame();
	const UnsignedInt last = weapon->getLastShotFrame();
	return last != 0 && now >= last && now - last <= JUST_FIRED;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime NotifyWhenAttackingBehavior::update()
{
	const NotifyWhenAttackingBehaviorModuleData *data = getNotifyWhenAttackingBehaviorModuleData();
	Object *me = getObject();
	const UnsignedInt IDLE_LOOK = 5;	// (frames between its looks while it attacks nothing)
	if( me->isEffectivelyDead() )
		return UPDATE_SLEEP_FOREVER;
	if( me->isDisabled() || me->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) || me->testStatus( OBJECT_STATUS_SOLD )
		|| data->m_notifyRadius <= 0.0f || data->m_maxFriends <= 0 || !isAttacking() )
	{
		m_lastFriendCount = 0;
		return UPDATE_SLEEP( IDLE_LOOK );
	}

	static const NameKeyType key = NAMEKEY( "AttackAssistResponseBehavior" );
	Int called = 0;
	SimpleObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( me, data->m_notifyRadius, FROM_CENTER_2D, nullptr, ITER_SORTED_NEAR_TO_FAR );
	MemoryPoolObjectHolder hold( iter );
	for( Object *other = iter->first(); other != nullptr && called < data->m_maxFriends; other = iter->next() )
	{
		if( other == me || me->getRelationship( other ) != ALLIES )
			continue;
		Bool named = FALSE;
		for( size_t i = 0; i < data->m_friendNames.size() && !named; ++i )
			named = other->getTemplate()->getName().compareNoCase( data->m_friendNames[ i ] ) == 0;
		if( !named )
			continue;
		AttackAssistResponseBehavior *response = (AttackAssistResponseBehavior *)other->findUpdateModule( key );
		if( response == nullptr || !response->canAssist() )
			continue;
		++called;
		response->assist( me );
	}
	m_lastFriendCount = called;
	return UPDATE_SLEEP( data->m_sleepTime > 0 ? data->m_sleepTime : 1 );
}

// ------------------------------------------------------------------------------------------------
void NotifyWhenAttackingBehavior::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void NotifyWhenAttackingBehavior::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
}

// ------------------------------------------------------------------------------------------------
void NotifyWhenAttackingBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
AttackAssistResponseBehaviorModuleData::AttackAssistResponseBehaviorModuleData()
{
	m_weaponSlot = SECONDARY_WEAPON;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void AttackAssistResponseBehaviorModuleData::buildFieldParse( MultiIniFieldParse &p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "WeaponSlot",	INI::parseLookupList,	TheWeaponSlotTypeNamesLookupList, offsetof( AttackAssistResponseBehaviorModuleData, m_weaponSlot ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
AttackAssistResponseBehavior::AttackAssistResponseBehavior( Thing *thing, const ModuleData *moduleData ) :
	UpdateModule( thing, moduleData ),
	m_assistCount( 0 )
{
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
AttackAssistResponseBehavior::~AttackAssistResponseBehavior()
{
}

//-------------------------------------------------------------------------------------------------
Bool AttackAssistResponseBehavior::canAssist() const
{
	const Object *me = getObject();
	return !me->isEffectivelyDead() && !me->isDisabled() && !me->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION )
		&& !me->testStatus( OBJECT_STATUS_SOLD )
		&& me->getWeaponInWeaponSlot( getAttackAssistResponseBehaviorModuleData()->m_weaponSlot ) != nullptr;
}

//-------------------------------------------------------------------------------------------------
Bool AttackAssistResponseBehavior::assist( Object *attacker )
{
	Object *me = getObject();
	if( attacker == nullptr || attacker == me || !canAssist() )
		return FALSE;
	Weapon *weapon = me->getWeaponInWeaponSlot( getAttackAssistResponseBehaviorModuleData()->m_weaponSlot );
	if( weapon == nullptr || weapon->getStatus() != READY_TO_FIRE || !weapon->isWithinAttackRange( me, attacker ) )
		return FALSE;
	weapon->fireWeapon( me, attacker );
	++m_assistCount;
	return TRUE;
}

// ------------------------------------------------------------------------------------------------
void AttackAssistResponseBehavior::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void AttackAssistResponseBehavior::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	xfer->xferUnsignedInt( &m_assistCount );
}

// ------------------------------------------------------------------------------------------------
void AttackAssistResponseBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
