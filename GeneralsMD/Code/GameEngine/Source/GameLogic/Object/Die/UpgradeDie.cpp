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

// FILE: UpgradeDie.cpp ///////////////////////////////////////////////////////////////////////////
// Author: Kris Morness, August 2002
// Desc:  When object dies, the parent object is freed of the specified object upgrade field.
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"	// Tiberium Project
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"

#include "GameClient/Drawable.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/UpgradeDie.h"
#include "GameLogic/Object.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpgradeDie::UpgradeDie( Thing *thing, const ModuleData* moduleData ) : DieModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpgradeDie::~UpgradeDie()
{
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project: whether the player has another of the dying one's kind, built and alive
//-------------------------------------------------------------------------------------------------
struct UpgradeDieOthers
{
	const Object *dying;
	Bool found;
};

static void findAnotherBuilt( Object *obj, void *userData )
{
	UpgradeDieOthers *others = (UpgradeDieOthers *)userData;
	if( others->found || obj == others->dying || obj->isEffectivelyDead()
			|| obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) || obj->testStatus( OBJECT_STATUS_SOLD ) )
		return;
	if( obj->getTemplate()->isEquivalentTo( others->dying->getTemplate() ) )
		others->found = TRUE;
}

//-------------------------------------------------------------------------------------------------
/** The die callback. */
//-------------------------------------------------------------------------------------------------
void UpgradeDie::onDie( const DamageInfo *damageInfo )
{
	if (!isDieApplicable(damageInfo))
		return;

	// Tiberium Project @feature FromPlayerWhenLast: the player's upgrade its kind gives it, taken away as the last of them
	// it has built dies
	if( getUpgradeDieModuleData()->m_fromPlayerWhenLast )
	{
		Player *player = getObject()->getControllingPlayer();
		const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade( getUpgradeDieModuleData()->m_upgradeName );
		if( player != nullptr && upgrade != nullptr && player->hasUpgradeComplete( upgrade ) )
		{
			UpgradeDieOthers others;
			others.dying = getObject();
			others.found = FALSE;
			player->iterateObjects( findAnotherBuilt, &others );
			if( !others.found )
				player->removeUpgrade( upgrade );
		}
		return;
	}

	//Look for the object that created me.
	Object *producer = TheGameLogic->findObjectByID( getObject()->getProducerID() );
	if( producer )
	{
		//Okay, we found our parent... now look for the upgrade.
		const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade( getUpgradeDieModuleData()->m_upgradeName );

		if( upgrade )
		{
			//We found the upgrade, now see if the parent object has it set...
			if( producer->hasUpgrade( upgrade ) )
			{
				//Cool, now remove it.
				producer->removeUpgrade( upgrade );
			}
			else
			{
				DEBUG_CRASH( ("Object %s just died, but is trying to free upgrade %s in it's producer %s%s",
					getObject()->getTemplate()->getName().str(),
					getUpgradeDieModuleData()->m_upgradeName.str(),
					producer->getTemplate()->getName().str(),
					", which the producer doesn't have. This is used in cases where the producer builds an upgrade that can die... like ranger building scout drones.") );
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void UpgradeDie::crc( Xfer *xfer )
{

	// extend base class
	DieModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void UpgradeDie::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	DieModule::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void UpgradeDie::loadPostProcess()
{

	// extend base class
	DieModule::loadPostProcess();

}
