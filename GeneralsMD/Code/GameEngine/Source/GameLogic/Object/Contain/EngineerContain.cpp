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

// FILE: EngineerContain.cpp //////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's engineer's way in (Kane's Wrath's EngineerContain, C&C4's CaptureContain):
// see EngineerContain.h
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Eva.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/EngineerContain.h"

//-------------------------------------------------------------------------------------------------
EngineerContainModuleData::EngineerContainModuleData()
{
	m_replaceWith = nullptr;
	m_destroyContainer = TRUE;
	m_destroyRider = TRUE;
	m_captureEnemies = TRUE;
	m_repairAllies = TRUE;
	m_repairFX = nullptr;
	m_captureFX = nullptr;
	m_replaceFX = nullptr;
	m_captureAndReplaceFX = nullptr;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void EngineerContainModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	OpenContainModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "AllowedRiders",				INI::parseAsciiStringVectorAppend,	nullptr, offsetof( EngineerContainModuleData, m_allowedRiders ) },
		{ "ReplaceWith",					INI::parseObjectCreationList,				nullptr, offsetof( EngineerContainModuleData, m_replaceWith ) },
		{ "DestroyContainer",			INI::parseBool,											nullptr, offsetof( EngineerContainModuleData, m_destroyContainer ) },
		{ "DestroyRider",					INI::parseBool,											nullptr, offsetof( EngineerContainModuleData, m_destroyRider ) },
		{ "CaptureEnemies",				INI::parseBool,											nullptr, offsetof( EngineerContainModuleData, m_captureEnemies ) },
		{ "RepairAllies",					INI::parseBool,											nullptr, offsetof( EngineerContainModuleData, m_repairAllies ) },
		{ "RepairFX",							INI::parseFXList,										nullptr, offsetof( EngineerContainModuleData, m_repairFX ) },
		{ "CaptureFX",						INI::parseFXList,										nullptr, offsetof( EngineerContainModuleData, m_captureFX ) },
		{ "ReplaceFX",						INI::parseFXList,										nullptr, offsetof( EngineerContainModuleData, m_replaceFX ) },
		{ "CaptureAndReplaceFX",	INI::parseFXList,										nullptr, offsetof( EngineerContainModuleData, m_captureAndReplaceFX ) },
		{ "FriendlyEnterVoice",		INI::parseAsciiString,							nullptr, offsetof( EngineerContainModuleData, m_friendlyEnterVoice ) },
		{ "HostileEnterVoice",		INI::parseAsciiString,							nullptr, offsetof( EngineerContainModuleData, m_hostileEnterVoice ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
EngineerContain::EngineerContain( Thing *thing, const ModuleData *moduleData ) : OpenContain( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
EngineerContain::~EngineerContain()
{
}

//-------------------------------------------------------------------------------------------------
/** the per-unit voice of a rider ordered in, a friend or a foe (none: nullptr) */
//-------------------------------------------------------------------------------------------------
const char *EngineerContain::getEnterVoiceName( Bool hostile ) const
{
	const AsciiString& name = hostile ? getEngineerContainModuleData()->m_hostileEnterVoice : getEngineerContainModuleData()->m_friendlyEnterVoice;
	return name.isEmpty() ? nullptr : name.str();
}

//-------------------------------------------------------------------------------------------------
/** one of those it names (by name, or one of their build variations); anyone when it names none */
//-------------------------------------------------------------------------------------------------
Bool EngineerContain::isAllowedRider( const Object *rider ) const
{
	const std::vector<AsciiString>& names = getEngineerContainModuleData()->m_allowedRiders;
	if( names.empty() )
		return TRUE;

	const ThingTemplate *riderTemplate = rider->getTemplate();
	for( std::vector<AsciiString>::const_iterator it = names.begin(); it != names.end(); ++it )
	{
		const ThingTemplate *allowed = TheThingFactory->findTemplate( *it );
		if( allowed != nullptr && riderTemplate->isEquivalentTo( allowed ) )
			return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** a rider it names, whose side the Allow*Inside let in, with something to do: raise it anew, mend it
	(a friend's, not whole) or take it (an enemy's or no one's); one at a time */
//-------------------------------------------------------------------------------------------------
Bool EngineerContain::isValidContainerFor( const Object* rider, Bool checkCapacity ) const
{
	if( rider == nullptr || !OpenContain::isValidContainerFor( rider, checkCapacity ) )
		return FALSE;

	if( !isAllowedRider( rider ) )
		return FALSE;

	if( checkCapacity && getContainCount() > 0 )
		return FALSE;

	const Object *me = getObject();
	const EngineerContainModuleData *d = getEngineerContainModuleData();
	if( d->m_replaceWith != nullptr && d->m_destroyContainer )
		return TRUE;

	if( rider->getRelationship( me ) == ALLIES )
	{
		const BodyModuleInterface *body = me->getBodyModule();
		return d->m_repairAllies && body != nullptr && body->getHealth() < body->getMaxHealth();
	}

	return d->m_captureEnemies && !me->isKindOf( KINDOF_IMMUNE_TO_CAPTURE );
}

//-------------------------------------------------------------------------------------------------
/** the rider is in: used up doing what it came for */
//-------------------------------------------------------------------------------------------------
void EngineerContain::onContaining( Object *rider, Bool wasSelected )
{
	OpenContain::onContaining( rider, wasSelected );	// (the enter sound)

	Object *me = getObject();
	const EngineerContainModuleData *d = getEngineerContainModuleData();
	Player *riderPlayer = rider->getControllingPlayer();
	const Bool friendly = rider->getRelationship( me ) == ALLIES;

	if( d->m_destroyRider )
		TheGameLogic->destroyObject( rider );

	if( riderPlayer == nullptr || me->isEffectivelyDead() || me->isDestroyed() )
		return;

	// raised anew for the rider's side: a husk becomes the walker or the vehicle again
	if( d->m_replaceWith != nullptr )
	{
		ObjectCreationList::setOwnerOverride( riderPlayer->getDefaultTeam() );
		ObjectCreationList::create( d->m_replaceWith, me, rider );
		ObjectCreationList::setOwnerOverride( nullptr );

		const FXList *fx = friendly ? d->m_replaceFX : d->m_captureAndReplaceFX;
		if( fx != nullptr )
			FXList::doFXObj( fx, me );

		if( d->m_destroyContainer )
		{
			TheGameLogic->destroyObject( me );
			return;
		}
	}

	// a friend's mended whole
	if( friendly )
	{
		BodyModuleInterface *body = me->getBodyModule();
		if( d->m_repairAllies && body != nullptr && body->getHealth() < body->getMaxHealth() )
		{
			me->attemptHealing( body->getMaxHealth() - body->getHealth(), rider );
			if( d->m_repairFX != nullptr )
				FXList::doFXObj( d->m_repairFX, me );
		}
		return;
	}

	// an enemy's or no one's taken for the rider's side
	if( !d->m_captureEnemies || me->isKindOf( KINDOF_IMMUNE_TO_CAPTURE ) )
		return;

	if( me->isLocallyViewed() )
		TheEva->setShouldPlay( EVA_BuildingStolen );
	if( riderPlayer == ThePlayerList->getLocalPlayer() )
		TheEva->setShouldPlay( EVA_BuildingCaptured );

	me->defect( riderPlayer->getDefaultTeam(), 1 );	// (a frame of flash)

	if( d->m_captureFX != nullptr )
		FXList::doFXObj( d->m_captureFX, me );

	riderPlayer->getAcademyStats()->recordBuildingCapture();
}

//-------------------------------------------------------------------------------------------------
void EngineerContain::crc( Xfer *xfer )
{
	OpenContain::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
//-------------------------------------------------------------------------------------------------
void EngineerContain::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	OpenContain::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void EngineerContain::loadPostProcess()
{
	OpenContain::loadPostProcess();
}
