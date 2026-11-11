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
// Tiberium Project @feature ReadyStatus: statuses it has while the power is ready (SAGE's
// ObjectStatusWhenReady: C&C4's Temple of Nod charges its missile only while it is ready, USER_1).
// ReadyEvaEvent: the announcer tells the owner as it comes ready (SAGE's EvaAnnounceSpecialPowerReady-
// ClientUpdate: the Temple of Nod's nuclear missile ready).
//
//   Behavior = MonitorSpecialPowerTimerUpdate ModuleTag_NukeReady
//     SpecialPowerTemplate = KWSP_SpecialPowerNuclearMissile
//     ReadyCondition       = SPECIALPOWER1_READY
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/INI.h"
#include "Common/ModelState.h"
#include "Common/ObjectStatusTypes.h"
#include "GameClient/Eva.h"
#include "GameLogic/Module/UpdateModule.h"

class SpecialPowerTemplate;

//-------------------------------------------------------------------------------------------------
class MonitorSpecialPowerTimerUpdateModuleData : public UpdateModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;
	ModelConditionFlagType m_readyCondition;
	ObjectStatusMaskType m_readyStatus;	///< Tiberium Project
	EvaMessage m_readyEva;							///< Tiberium Project

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
	// (also while the building is unpowered or otherwise disabled: what it shows follows the power's timer)
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }

private:

	Bool m_ready;	///< the condition is shown

};
