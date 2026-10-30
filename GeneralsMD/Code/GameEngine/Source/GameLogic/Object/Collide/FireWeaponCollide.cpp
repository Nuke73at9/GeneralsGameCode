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

// FILE: FireWeaponCollide.cpp ///////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood  April 2002
// Desc:   Shoot something that collides with me every frame with my weapon
///////////////////////////////////////////////////////////////////////////////////////////////////


// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#define DEFINE_OBJECT_STATUS_NAMES
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/FireWeaponCollide.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void FireWeaponCollideModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  CollideModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "CollideWeapon",		INI::parseWeaponTemplate,						nullptr, offsetof( FireWeaponCollideModuleData, m_collideWeaponTemplate ) },
		{ "FireOnce",					INI::parseBool,											nullptr, offsetof( FireWeaponCollideModuleData, m_fireOnce ) },
		{ "RequiredStatus",		ObjectStatusMaskType::parseFromINI,	nullptr, offsetof( FireWeaponCollideModuleData, m_requiredStatus ) },
		{ "ForbiddenStatus",	ObjectStatusMaskType::parseFromINI,	nullptr, offsetof( FireWeaponCollideModuleData, m_forbiddenStatus ) },
		// Tiberium Project @feature
		{ "EnemiesOnly",			INI::parseBool,											nullptr, offsetof( FireWeaponCollideModuleData, m_enemiesOnly ) },
		{ "TargetMustBeMoving",	INI::parseBool,										nullptr, offsetof( FireWeaponCollideModuleData, m_targetMustBeMoving ) },
		{ "DelayBetweenHits",	INI::parseDurationUnsignedInt,			nullptr, offsetof( FireWeaponCollideModuleData, m_delayBetweenHits ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FireWeaponCollide::FireWeaponCollide( Thing *thing, const ModuleData* moduleData ) :
	CollideModule( thing, moduleData ),
	m_collideWeapon(nullptr)
{
	m_collideWeapon = TheWeaponStore->allocateNewWeapon(getFireWeaponCollideModuleData()->m_collideWeaponTemplate, PRIMARY_WEAPON);
	m_everFired = FALSE;
	m_nextFireFrame = 0;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
FireWeaponCollide::~FireWeaponCollide()
{
	deleteInstance(m_collideWeapon);
}

//-------------------------------------------------------------------------------------------------
/** The die callback. */
//-------------------------------------------------------------------------------------------------
void FireWeaponCollide::onCollide( Object *other, const Coord3D *loc, const Coord3D *normal )
{
	if( other == nullptr )
		return; //Don't shoot the ground

	Object *me = getObject();

	// Tiberium Project @feature what may set it off: an enemy's touch, something moving; and not again
	// before its delay
	const FireWeaponCollideModuleData *d = getFireWeaponCollideModuleData();
	if( d->m_enemiesOnly && me->getRelationship( other ) != ENEMIES )
		return;
	if( d->m_targetMustBeMoving && ( other->getAIUpdateInterface() == nullptr || !other->getAIUpdateInterface()->isMoving() ) )
		return;
	if( d->m_delayBetweenHits > 0 && TheGameLogic->getFrame() < m_nextFireFrame )
		return;

	// This will fire at you every frame, because multiple people could be colliding and we want
	// to hurt them all.  Another solution would be to keep a Map of other->objetIDs and
	// delays for each individually.  However, this solution here is so quick and simple that it
	// warrants the "do it eventually if we need it" clause.
	if( shouldFireWeapon() )
	{
		m_collideWeapon->loadAmmoNow( me );
		m_collideWeapon->fireWeapon( me, other );
		if( d->m_fireOnce )
			m_everFired = TRUE;	// Tiberium Project @bugfix FireOnce: nothing set it before
		if( d->m_delayBetweenHits > 0 )
			m_nextFireFrame = TheGameLogic->getFrame() + d->m_delayBetweenHits;
	}
}

//-------------------------------------------------------------------------------------------------
Bool FireWeaponCollide::shouldFireWeapon()
{
	const FireWeaponCollideModuleData *d = getFireWeaponCollideModuleData();

	ObjectStatusMaskType status = getObject()->getStatusBits();

	//We need all required status or else we fail
	if( !status.testForAll( d->m_requiredStatus ) )
		return FALSE;

	//If we have any forbidden statii, then fail
	if( status.testForAny( d->m_forbiddenStatus ) )
		return FALSE;

	if( m_everFired && d->m_fireOnce )
		return FALSE;// can only fire once ever

	return TRUE;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void FireWeaponCollide::crc( Xfer *xfer )
{

	// extend base class
	CollideModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void FireWeaponCollide::xfer( Xfer *xfer )
{

	// version
#if RETAIL_COMPATIBLE_XFER_SAVE
	XferVersion currentVersion = 1;
#else
	XferVersion currentVersion = 2;	// Tiberium Project: 2, the delay between hits
#endif
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	CollideModule::xfer( xfer );

	// weapon
	Bool collideWeaponPresent = m_collideWeapon ? TRUE : FALSE;
	xfer->xferBool( &collideWeaponPresent );
	if( collideWeaponPresent )
	{

		DEBUG_ASSERTCRASH( m_collideWeapon != nullptr,
											 ("FireWeaponCollide::xfer - m_collideWeapon present mismatch") );
		xfer->xferSnapshot( m_collideWeapon );

	}
	else
	{

		DEBUG_ASSERTCRASH( m_collideWeapon == nullptr,
											 ("FireWeaponCollide::Xfer - m_collideWeapon missing mismatch" ));

	}

	// ever fired
	xfer->xferBool( &m_everFired );

	// Tiberium Project: not before this frame
	if( version >= 2 )
		xfer->xferUnsignedInt( &m_nextFireFrame );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void FireWeaponCollide::loadPostProcess()
{

	// extend base class
	CollideModule::loadPostProcess();

}
