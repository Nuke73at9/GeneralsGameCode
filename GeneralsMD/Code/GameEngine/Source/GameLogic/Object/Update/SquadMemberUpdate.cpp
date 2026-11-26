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

// FILE: SquadMemberUpdate.cpp ////////////////////////////////////////////////////////////////////
// Tiberium Project @feature a soldier of a C&C3 squad (see SquadMemberUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/SquadMemberUpdate.h"

#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SquadUpdate.h"
#include "GameLogic/Object.h"

//-------------------------------------------------------------------------------------------------
SquadMemberUpdate::SquadMemberUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_squad = INVALID_ID;
	setWakeFrame( getObject(), UPDATE_SLEEP( LOGICFRAMES_PER_SECOND ) );
}

//-------------------------------------------------------------------------------------------------
SquadMemberUpdate::~SquadMemberUpdate()
{
}

//-------------------------------------------------------------------------------------------------
void SquadMemberUpdate::onEnslave( const Object *slaver )
{
	m_squad = slaver != nullptr ? slaver->getID() : INVALID_ID;
	SquadUpdate *squad = SquadUpdate::getSquadUpdate( slaver );
	if( squad != nullptr )
		squad->addMember( getObject() );
}

//-------------------------------------------------------------------------------------------------
void SquadMemberUpdate::onSlaverDie( const DamageInfo * /*info*/ )
{
	m_squad = INVALID_ID;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime SquadMemberUpdate::update()
{
	// (its squad gone without it, by a script or a sale: a soldier of its own)
	if( m_squad != INVALID_ID && TheGameLogic->findObjectByID( m_squad ) == nullptr )
		m_squad = INVALID_ID;
	return m_squad != INVALID_ID ? UPDATE_SLEEP( LOGICFRAMES_PER_SECOND ) : UPDATE_SLEEP_FOREVER;
}

//-------------------------------------------------------------------------------------------------
const Object *SquadMemberUpdate::getSelectionStandIn( const Object *obj )
{
	const Object *squad = SquadUpdate::getSquadOf( obj );
	return squad != nullptr ? squad : obj;
}

//-------------------------------------------------------------------------------------------------
void SquadMemberUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void SquadMemberUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	xfer->xferObjectID( &m_squad );
}

//-------------------------------------------------------------------------------------------------
void SquadMemberUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
