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

// FILE: StealthDetectorUpgrade.cpp ///////////////////////////////////////////////////////////////
// Tiberium Project @feature a stealth detector an upgrade turns on (see StealthDetectorUpgrade.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/StealthDetectorUpdate.h"
#include "GameLogic/Module/StealthDetectorUpgrade.h"

//-------------------------------------------------------------------------------------------------
StealthDetectorUpgrade::StealthDetectorUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
StealthDetectorUpgrade::~StealthDetectorUpgrade()
{
}

//-------------------------------------------------------------------------------------------------
// every StealthDetectorUpdate of the object
//-------------------------------------------------------------------------------------------------
void StealthDetectorUpgrade::setDetecting( Bool on )
{
	static NameKeyType key_StealthDetectorUpdate = NAMEKEY( "StealthDetectorUpdate" );
	for( BehaviorModule **m = getObject()->getBehaviorModules(); *m; ++m )
	{
		if( (*m)->getModuleNameKey() != key_StealthDetectorUpdate )
			continue;
		StealthDetectorUpdate *detector = (StealthDetectorUpdate *)(*m);
		detector->setSDEnabled( on );
	}
}

//-------------------------------------------------------------------------------------------------
void StealthDetectorUpgrade::upgradeImplementation()
{
	setDetecting( TRUE );
}

//-------------------------------------------------------------------------------------------------
void StealthDetectorUpgrade::undoUpgradeImplementation()
{
	setDetecting( FALSE );
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void StealthDetectorUpgrade::crc( Xfer *xfer )
{
	UpgradeModule::crc( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void StealthDetectorUpgrade::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpgradeModule::xfer( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void StealthDetectorUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
