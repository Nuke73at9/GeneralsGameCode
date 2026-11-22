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

// FILE: FXListUpgrade.cpp ////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's FXListBehavior onUpgraded (see FXListUpgrade.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ThingTemplate.h"
#include "Common/TiberiumTest.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/FXListUpgrade.h"

//-------------------------------------------------------------------------------------------------
FXListUpgrade::FXListUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
FXListUpgrade::~FXListUpgrade()
{
}

//-------------------------------------------------------------------------------------------------
void FXListUpgrade::upgradeImplementation()
{
	FXList::doFXObj( getFXListUpgradeModuleData()->m_fx, getObject() );
	if( TiberiumTest::isActive() )
		TiberiumTest::log( "upgrade fx: %s's %s played", getObject()->getTemplate()->getName().str(),
			getModuleTagNameKey() != NAMEKEY_INVALID ? TheNameKeyGenerator->keyToName( getModuleTagNameKey() ).str() : "?" );
}

//-------------------------------------------------------------------------------------------------
void FXListUpgrade::crc( Xfer *xfer )
{
	UpgradeModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void FXListUpgrade::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	UpgradeModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void FXListUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
