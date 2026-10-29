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

// FILE: ReloadAmmoSpecialPower.cpp /////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's manual reload (see ReloadAmmoSpecialPower.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/ReloadAmmoSpecialPower.h"

//-------------------------------------------------------------------------------------------------
ReloadAmmoSpecialPower::ReloadAmmoSpecialPower( Thing *thing, const ModuleData* moduleData ) : SpecialPowerModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
ReloadAmmoSpecialPower::~ReloadAmmoSpecialPower()
{
}

//-------------------------------------------------------------------------------------------------
void ReloadAmmoSpecialPower::doSpecialPower( UnsignedInt commandOptions )
{
	Object *obj = getObject();
	if( obj->isDisabled() || !isReady() )
		return;
	SpecialPowerModule::doSpecialPower( commandOptions );
	obj->reloadAllAmmo( TRUE );
}

//-------------------------------------------------------------------------------------------------
void ReloadAmmoSpecialPower::crc( Xfer *xfer )
{
	SpecialPowerModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void ReloadAmmoSpecialPower::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	SpecialPowerModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void ReloadAmmoSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
}
