/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: FireWeaponUpdate.h /////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, August 2002
// Desc:   Update fires a weapon at its own feet as quickly as the weapon allows
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/FireWeaponUpdate.h"
#include "GameLogic/Module/StickyBombUpdate.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/WeaponStatus.h"
#include "GameLogic/GameLogic.h"



//-------------------------------------------------------------------------------------------------
FireWeaponUpdateModuleData::FireWeaponUpdateModuleData()
{
	m_weaponTemplate = nullptr;
  m_initialDelayFrames = 0;
	m_exclusiveWeaponDelay = 0;
	m_requiredObjectRange = 0.0f;	// Tiberium Project
	m_requiredObjectRelationship = 0;
	m_startsInactive = FALSE;	// Tiberium Project
	m_fireOffset.zero();	// Tiberium Project
}

// Tiberium Project: the relationships RequiredObjectRelationship names
static const char *const RequiredObjectRelationshipNames[] = { "ALLIES", "ENEMIES", "NEUTRALS", nullptr };
enum { REQUIRED_ALLIES = 1, REQUIRED_ENEMIES = 2, REQUIRED_NEUTRALS = 4 };

//-------------------------------------------------------------------------------------------------
/*static*/ void FireWeaponUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  UpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "Weapon",								INI::parseWeaponTemplate,	      nullptr, offsetof( FireWeaponUpdateModuleData, m_weaponTemplate ) },
		{ "InitialDelay",					INI::parseDurationUnsignedInt,	nullptr, offsetof( FireWeaponUpdateModuleData, m_initialDelayFrames ) },
		{ "ExclusiveWeaponDelay",	INI::parseDurationUnsignedInt,	nullptr, offsetof( FireWeaponUpdateModuleData, m_exclusiveWeaponDelay ) },
		{ "RequiredStatus",				ObjectStatusMaskType::parseFromINI,	nullptr, offsetof( FireWeaponUpdateModuleData, m_requiredStatus ) },	// Tiberium Project
		{ "ForbiddenStatus",			ObjectStatusMaskType::parseFromINI,	nullptr, offsetof( FireWeaponUpdateModuleData, m_forbiddenStatus ) },	// Tiberium Project
		{ "RequiredObjectRange",	INI::parseReal,									nullptr, offsetof( FireWeaponUpdateModuleData, m_requiredObjectRange ) },	// Tiberium Project
		{ "RequiredObjectRelationship",	INI::parseBitString32,	RequiredObjectRelationshipNames, offsetof( FireWeaponUpdateModuleData, m_requiredObjectRelationship ) },	// Tiberium Project
		{ "RequiredObjectKindOf",	KindOfMaskType::parseFromINI,		nullptr, offsetof( FireWeaponUpdateModuleData, m_requiredObjectKindOf ) },	// Tiberium Project
		{ "RequiredObjectForbiddenKindOf",	KindOfMaskType::parseFromINI,	nullptr, offsetof( FireWeaponUpdateModuleData, m_requiredObjectForbiddenKindOf ) },	// Tiberium Project
		{ "StartsInactive",				INI::parseBool,									nullptr, offsetof( FireWeaponUpdateModuleData, m_startsInactive ) },	// Tiberium Project
		{ "FireOffset",						INI::parseCoord3D,							nullptr, offsetof( FireWeaponUpdateModuleData, m_fireOffset ) },	// Tiberium Project
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FireWeaponUpdate::FireWeaponUpdate( Thing *thing, const ModuleData* moduleData ) :
	UpdateModule( thing, moduleData ),
	m_weapon(nullptr)
{
	const WeaponTemplate *tmpl = getFireWeaponUpdateModuleData()->m_weaponTemplate;
	if (tmpl)
	{
		m_weapon = TheWeaponStore->allocateNewWeapon(tmpl, PRIMARY_WEAPON);
		m_weapon->loadAmmoNow( getObject() );
	}


  m_initialDelayFrame = TheGameLogic->getFrame() + getFireWeaponUpdateModuleData()->m_initialDelayFrames;

	// Tiberium Project @feature StartsInactive: asleep until armed
	m_active = !getFireWeaponUpdateModuleData()->m_startsInactive;
	if( !m_active )
		setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature switched on: its InitialDelay from now, its weapon ready
void FireWeaponUpdate::arm()
{
	m_active = TRUE;
	m_initialDelayFrame = TheGameLogic->getFrame() + getFireWeaponUpdateModuleData()->m_initialDelayFrames;
	if( m_weapon != nullptr )
		m_weapon->loadAmmoNow( getObject() );
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
void FireWeaponUpdate::armAll( Object *obj )
{
	static const NameKeyType key = NAMEKEY( "FireWeaponUpdate" );
	if( obj == nullptr )
		return;
	for( BehaviorModule **m = obj->getBehaviorModules(); *m != nullptr; ++m )
	{
		if( (*m)->getModuleNameKey() != key )
			continue;
		FireWeaponUpdate *update = static_cast<FireWeaponUpdate *>( *m );
		if( update->getFireWeaponUpdateModuleData()->m_startsInactive )
			update->arm();
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FireWeaponUpdate::~FireWeaponUpdate()
{
	deleteInstance(m_weapon);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime FireWeaponUpdate::update()
{
	if( !m_active )
		return UPDATE_SLEEP_FOREVER;	// Tiberium Project: not yet armed (StartsInactive)

  if ( TheGameLogic->getFrame() < m_initialDelayFrame )
    return UPDATE_SLEEP_NONE;


	// If my weapon is ready, shoot it.
	if( isOkayToFire() )
	{
		// Tiberium Project @feature a charge stuck to something (StickyBombUpdate) shoots what it is stuck to
		// (C&C4's damage over time from an attached charge: the Shockwave's sonic charge)
		static NameKeyType key_StickyBombUpdate = NAMEKEY( "StickyBombUpdate" );
		const StickyBombUpdate *sticky = (const StickyBombUpdate *)getObject()->findUpdateModule( key_StickyBombUpdate );
		Object *stuckTo = sticky != nullptr ? sticky->getTargetObject() : nullptr;
		// Tiberium Project @bugfix only what its damage is for: one stuck to its own side's (C&C4's Aftershock's
		// Tiberium cloud, on the Aftershock) shoots where it is, its damage for those around and not for the Aftershock;
		// shot at as the weapon's target, the Aftershock was hurt by its own cloud
		if( stuckTo != nullptr )
		{
			const Int affects = m_weapon->getTemplate()->getAffectsMask();
			const Relationship r = getObject()->getRelationship( stuckTo );
			if( ( r == ALLIES && !( affects & WEAPON_AFFECTS_ALLIES ) ) || ( r == NEUTRAL && !( affects & WEAPON_AFFECTS_NEUTRALS ) )
				|| ( r == ENEMIES && !( affects & WEAPON_AFFECTS_ENEMIES ) ) )
				stuckTo = nullptr;
		}
		const Coord3D &offset = getFireWeaponUpdateModuleData()->m_fireOffset;
		if( stuckTo != nullptr && !stuckTo->isEffectivelyDead() )
			m_weapon->forceFireWeapon( getObject(), stuckTo );
		else if( offset.x != 0.0f || offset.y != 0.0f || offset.z != 0.0f )
		{
			// Tiberium Project @feature at its spot from the object, turned with it (FireOffset), on the ground there
			const Object *me = getObject();
			const Real angle = me->getOrientation();
			const Real c = WWMath::Cos( angle );
			const Real s = WWMath::Sin( angle );
			Coord3D at = *me->getPosition();
			at.x += offset.x * c - offset.y * s;
			at.y += offset.x * s + offset.y * c;
			at.z = TheTerrainLogic->getGroundHeight( at.x, at.y ) + offset.z;
			m_weapon->forceFireWeapon( getObject(), &at );
		}
		else
			m_weapon->forceFireWeapon( getObject(), getObject()->getPosition() );
	}
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Bool FireWeaponUpdate::isOkayToFire()
{
	const Object *me = getObject();
	const FireWeaponUpdateModuleData *data = getFireWeaponUpdateModuleData();

	if( m_weapon == nullptr )
		return FALSE;

	// Weapon is reloading
	if( m_weapon->getStatus() != READY_TO_FIRE )
		return FALSE;

	if( me->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) )
		return FALSE; // no hitting with a 0% building, cheater

	// Tiberium Project @feature only in the states given (a deployed unit's pulse, a toggled one's)
	if( data->m_requiredStatus.any() && !me->getStatusBits().testForAll( data->m_requiredStatus ) )
		return FALSE;
	if( me->getStatusBits().testForAny( data->m_forbiddenStatus ) )
		return FALSE;

	// Firing a real weapon suppresses this module
	if( data->m_exclusiveWeaponDelay > 0  &&  ( TheGameLogic->getFrame() < (me->getLastShotFiredFrame() + data->m_exclusiveWeaponDelay) ) )
		return FALSE;

	// Tiberium Project @feature only with another object near enough
	if( data->m_requiredObjectRange > 0.0f && !hasRequiredObject() )
		return FALSE;

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature another live object within RequiredObjectRange of the relationships and kinds asked
Bool FireWeaponUpdate::hasRequiredObject() const
{
	const Object *me = getObject();
	const FireWeaponUpdateModuleData *data = getFireWeaponUpdateModuleData();
	SimpleObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( me, data->m_requiredObjectRange, FROM_BOUNDINGSPHERE_2D );
	MemoryPoolObjectHolder hold( iter );
	for( Object *other = iter->first(); other != nullptr; other = iter->next() )
	{
		if( other == me || other->isEffectivelyDead() )
			continue;
		if( data->m_requiredObjectRelationship != 0 )
		{
			const Relationship r = me->getRelationship( other );
			const UnsignedInt bit = r == ALLIES ? REQUIRED_ALLIES : ( r == ENEMIES ? REQUIRED_ENEMIES : REQUIRED_NEUTRALS );
			if( ( data->m_requiredObjectRelationship & bit ) == 0 )
				continue;
		}
		if( data->m_requiredObjectKindOf.any() && !other->isAnyKindOf( data->m_requiredObjectKindOf ) )
			continue;
		if( data->m_requiredObjectForbiddenKindOf.any() && other->isAnyKindOf( data->m_requiredObjectForbiddenKindOf ) )
			continue;
		return TRUE;
	}
	return FALSE;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void FireWeaponUpdate::crc( Xfer *xfer )
{
	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: m_initialDelayFrame
	* 3: Tiberium Project: m_active */
// ------------------------------------------------------------------------------------------------
void FireWeaponUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 3;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// weapon
	xfer->xferSnapshot( m_weapon );

  if ( version >= 2 )
    xfer->xferUnsignedInt( &m_initialDelayFrame );

	// Tiberium Project @feature switched on (StartsInactive)
	if( version >= 3 )
		xfer->xferBool( &m_active );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void FireWeaponUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
