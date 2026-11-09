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

// FILE: MonitorSpecialPowerTimerUpdate.h /////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's MonitorSpecialPowerTimerUpdate: while its special power is ready to
// use, the object shows ReadyCondition (Kane's Wrath's Temple of Nod raises its missile, the Ion Cannon
// Control Center its emitter, as their superweapons come ready). Not while it is being built or sold.
//
//   Behavior = MonitorSpecialPowerTimerUpdate ModuleTag_NukeReady
//     SpecialPowerTemplate = KWSP_SpecialPowerNuclearMissile
//     ReadyCondition       = SPECIALPOWER1_READY
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/INI.h"
#include "Common/ModelState.h"
#include "GameLogic/Module/UpdateModule.h"

class SpecialPowerTemplate;

//-------------------------------------------------------------------------------------------------
class MonitorSpecialPowerTimerUpdateModuleData : public UpdateModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;
	ModelConditionFlagType m_readyCondition;

	MonitorSpecialPowerTimerUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class MonitorSpecialPowerTimerUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( MonitorSpecialPowerTimerUpdate, "MonitorSpecialPowerTimerUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( MonitorSpecialPowerTimerUpdate, MonitorSpecialPowerTimerUpdateModuleData )

public:

	MonitorSpecialPowerTimerUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

private:

	Bool m_ready;	///< the condition is shown

};
