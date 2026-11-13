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

// FILE: ContainerCapacityUpgrade.cpp //////////////////////////////////////////////////////////////
// Tiberium Project @feature an upgrade that changes how many a container holds (see the header)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/ContainerCapacityUpgrade.h"
#include "GameLogic/Module/OpenContain.h"

//-------------------------------------------------------------------------------------------------
ContainerCapacityUpgradeModuleData::ContainerCapacityUpgradeModuleData()
{
	m_slotModifier = 0;
	m_killExtra = FALSE;
}

//-------------------------------------------------------------------------------------------------
static void parseOnRemoving( INI *ini, void * /*instance*/, void *store, const void * /*userData*/ )
{
	const char *token = ini->getNextToken();
	if( stricmp( token, "KILL_EXTRA_PASSENGERS" ) == 0 )
		*(Bool *)store = TRUE;
	else if( stricmp( token, "EJECT_EXTRA_PASSENGERS" ) == 0 )
		*(Bool *)store = FALSE;
	else
	{
		DEBUG_CRASH(( "ContainerCapacityUpgrade: OnRemoving is EJECT_EXTRA_PASSENGERS or KILL_EXTRA_PASSENGERS, not %s", token ));
		throw INI_INVALID_DATA;
	}
}

//-------------------------------------------------------------------------------------------------
void ContainerCapacityUpgradeModuleData::buildFieldParse( MultiIniFieldParse &p )
{
	UpgradeModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "SlotModifier",	INI::parseInt,		nullptr, offsetof( ContainerCapacityUpgradeModuleData, m_slotModifier ) },
		{ "OnRemoving",		parseOnRemoving,	nullptr, offsetof( ContainerCapacityUpgradeModuleData, m_killExtra ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
ContainerCapacityUpgrade::ContainerCapacityUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
ContainerCapacityUpgrade::~ContainerCapacityUpgrade()
{
}

//-------------------------------------------------------------------------------------------------
void ContainerCapacityUpgrade::upgradeImplementation()
{
	ContainModuleInterface *contain = getObject()->getContain();
	OpenContain *open = contain != nullptr ? contain->asOpenContain() : nullptr;
	if( open != nullptr )
		open->addCapacityBonus( getContainerCapacityUpgradeModuleData()->m_slotModifier );
}

//-------------------------------------------------------------------------------------------------
// the slots go again, and those left without a place get out, or die: the last in first
//-------------------------------------------------------------------------------------------------
void ContainerCapacityUpgrade::undoUpgradeImplementation()
{
	const ContainerCapacityUpgradeModuleData *data = getContainerCapacityUpgradeModuleData();
	ContainModuleInterface *contain = getObject()->getContain();
	OpenContain *open = contain != nullptr ? contain->asOpenContain() : nullptr;
	if( open == nullptr )
		return;
	open->addCapacityBonus( -data->m_slotModifier );
	const Int most = contain->getContainMax();
	if( most < 0 )
		return;
	while( (Int)contain->getContainCount() + contain->getExtraSlotsInUse() > most )
	{
		const ContainedItemsList *riders = contain->getContainedItemsList();
		if( riders == nullptr || riders->empty() )
			break;
		Object *rider = riders->back();
		contain->removeFromContain( rider, TRUE );
		if( data->m_killExtra )
			rider->kill();
		else
			open->scatterRemoved( rider );
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ContainerCapacityUpgrade::crc( Xfer *xfer )
{
	UpgradeModule::crc( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void ContainerCapacityUpgrade::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpgradeModule::xfer( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void ContainerCapacityUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
