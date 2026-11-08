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

// FILE: MoveAndEvacuateSpecialPower.cpp /////////////////////////////////////
// Tiberium Project @feature a transport goes to the spot and lets everyone out there (see the header)
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/MoveAndEvacuateSpecialPower.h"

//-------------------------------------------------------------------------------------------------
MoveAndEvacuateSpecialPower::MoveAndEvacuateSpecialPower( Thing *thing, const ModuleData* moduleData ) : SpecialPowerModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
MoveAndEvacuateSpecialPower::~MoveAndEvacuateSpecialPower()
{
}

//-------------------------------------------------------------------------------------------------
void MoveAndEvacuateSpecialPower::doSpecialPowerAtLocation( const Coord3D *loc, Real angle, UnsignedInt commandOptions )
{
	Object *obj = getObject();
	if( loc == nullptr || obj->isDisabled() || !isReady() || !canPayForPower() )	// Tiberium Project @feature its price
		return;
	AIUpdateInterface *ai = obj->getAI();
	if( ai == nullptr )
		return;

	// records the use and starts the reload time
	SpecialPowerModule::doSpecialPowerAtLocation( loc, angle, commandOptions );

	ai->aiMoveToAndEvacuate( loc, CMD_FROM_PLAYER );
}

//-------------------------------------------------------------------------------------------------
void MoveAndEvacuateSpecialPower::crc( Xfer *xfer )
{
	SpecialPowerModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void MoveAndEvacuateSpecialPower::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	SpecialPowerModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void MoveAndEvacuateSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
}
