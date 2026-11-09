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


// FILE: AssignSlavesTargetObjectSpecialPower.cpp /////////////////////////////////////////////////
// Tiberium Project @feature SAGE's AssignSlavesTargetObjectSpecialPower (see AssignSlavesTargetObjectSpecialPower.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/AssignSlavesTargetObjectSpecialPower.h"

#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/SpawnBehavior.h"

//-------------------------------------------------------------------------------------------------
AssignSlavesTargetObjectSpecialPower::AssignSlavesTargetObjectSpecialPower( Thing *thing, const ModuleData *moduleData ) : SpecialPowerModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
AssignSlavesTargetObjectSpecialPower::~AssignSlavesTargetObjectSpecialPower()
{
}

//-------------------------------------------------------------------------------------------------
void AssignSlavesTargetObjectSpecialPower::doSpecialPowerAtObject( Object *obj, UnsignedInt commandOptions )
{
	// (used: its recharge started again; not when paused, disabled or refused)
	const UnsignedInt before = m_availableOnFrame;
	SpecialPowerModule::doSpecialPowerAtObject( obj, commandOptions );
	if( obj == nullptr || m_availableOnFrame == before )
		return;
	SpawnBehaviorInterface *spawn = getObject()->getSpawnBehaviorInterface();
	if( spawn != nullptr )
		spawn->orderSlavesToRepair( obj->getID() );
}

//-------------------------------------------------------------------------------------------------
void AssignSlavesTargetObjectSpecialPower::crc( Xfer *xfer )
{
	SpecialPowerModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void AssignSlavesTargetObjectSpecialPower::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	SpecialPowerModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void AssignSlavesTargetObjectSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
}
