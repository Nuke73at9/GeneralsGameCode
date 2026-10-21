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

// FILE: DeployToggleSpecialPower.cpp /////////////////////////////////////////
// Tiberium Project @feature Deploys or packs up a unit whose DeployStyleAIUpdate has
// ManualDeploy = Yes. Use with a SpecialPower of Type SPECIAL_TOGGLE_DEPLOY and a
// SPECIAL_POWER command button without a target, e.g.
//
//   Behavior = DeployToggleSpecialPower ModuleTag_Deploy
//     SpecialPowerTemplate = SpecialAbilityToggleDeploy
//   End
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DeployStyleAIUpdate.h"
#include "GameLogic/Module/DeployToggleSpecialPower.h"

//-------------------------------------------------------------------------------------------------
DeployToggleSpecialPower::DeployToggleSpecialPower( Thing *thing, const ModuleData* moduleData ) : SpecialPowerModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
DeployToggleSpecialPower::~DeployToggleSpecialPower()
{
}

//-------------------------------------------------------------------------------------------------
void DeployToggleSpecialPower::doSpecialPower( UnsignedInt commandOptions )
{
	Object *obj = getObject();
	if( obj->isDisabled() || !isReady() )
		return;

	AIUpdateInterface *ai = obj->getAI();
	DeployStyleAIUpdate *deploy = ai ? ai->getDeployStyleAIUpdate() : nullptr;
	if( deploy == nullptr || !deploy->isManualDeploy() )
	{
		DEBUG_CRASH( ("%s uses DeployToggleSpecialPower but has no DeployStyleAIUpdate with ManualDeploy = Yes",
			obj->getTemplate()->getName().str()) );
		return;
	}

	// records the use and starts the (usually zero) reload time
	SpecialPowerModule::doSpecialPower( commandOptions );

	deploy->toggleManualDeploy( CMD_FROM_PLAYER );
}

//-------------------------------------------------------------------------------------------------
void DeployToggleSpecialPower::crc( Xfer *xfer )
{
	// extend base class
	SpecialPowerModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void DeployToggleSpecialPower::xfer( Xfer *xfer )
{
	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	SpecialPowerModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void DeployToggleSpecialPower::loadPostProcess()
{
	// extend base class
	SpecialPowerModule::loadPostProcess();
}
