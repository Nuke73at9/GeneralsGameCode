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

// FILE: AttributeModifierUpgrade.cpp /////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's AttributeModifierUpgrade (see AttributeModifierUpgrade.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AttributeModifierUpgrade.h"

//-------------------------------------------------------------------------------------------------
AttributeModifierUpgrade::AttributeModifierUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
AttributeModifierUpgrade::~AttributeModifierUpgrade()
{
}

//-------------------------------------------------------------------------------------------------
void AttributeModifierUpgrade::upgradeImplementation()
{
	Object *obj = getObject();
	obj->applyAttributeModifier( TheAttributeModifierStore->find( getAttributeModifierUpgradeModuleData()->m_modifier ), obj->getID() );
}

//-------------------------------------------------------------------------------------------------
void AttributeModifierUpgrade::crc( Xfer *xfer )
{
	UpgradeModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void AttributeModifierUpgrade::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	UpgradeModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void AttributeModifierUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
