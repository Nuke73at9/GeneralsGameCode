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

// FILE: DeployToggleSpecialPower.h ///////////////////////////////////////////
// Tiberium Project @feature A command button that deploys or packs up a unit whose
// DeployStyleAIUpdate has ManualDeploy = Yes (C&C4 Crawlers).
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/SpecialPowerModule.h"

//-------------------------------------------------------------------------------------------------
class DeployToggleSpecialPower : public SpecialPowerModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( DeployToggleSpecialPower, "DeployToggleSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( DeployToggleSpecialPower, SpecialPowerModuleData )

public:

	DeployToggleSpecialPower( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype defined by MemoryPoolObject

	virtual void doSpecialPower( UnsignedInt commandOptions ) override;
};
