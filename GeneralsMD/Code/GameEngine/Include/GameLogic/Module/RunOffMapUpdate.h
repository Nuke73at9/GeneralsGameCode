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

// FILE: RunOffMapUpdate.h ////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's RunOffMapUpdate with its RunOffMapBehavior (C&C4's Firehawk Strike: its
// bombers and rocket planes): the object leaves the map once all its weapons are out of ammunition
// (TriggerOnOutOfAmmo) or once TriggerDelay has passed since it was made (0: never), whichever comes first,
// looked at every RefreshDelay. It flies to the nearest edge of the map (FLEE_TO_NEAREST_MAP_EDGE) and past
// it, no longer selectable, and is gone once off the map.
//
//   Behavior = RunOffMapUpdate ModuleTag_RunOffMap
//     TriggerOnOutOfAmmo = Yes
//     TriggerDelay       = 30000
//     RefreshDelay       = 250
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpdateModule.h"

//-------------------------------------------------------------------------------------------------
class RunOffMapUpdateModuleData : public UpdateModuleData
{
public:
	Bool m_triggerOnOutOfAmmo;		///< it leaves once its weapons are all empty
	UnsignedInt m_triggerDelay;		///< it leaves this long after it is made (0: never)
	UnsignedInt m_refreshDelay;		///< how often it looks

	RunOffMapUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class RunOffMapUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( RunOffMapUpdate, "RunOffMapUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( RunOffMapUpdate, RunOffMapUpdateModuleData )

public:

	RunOffMapUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

private:

	void leave();

	UnsignedInt m_triggerFrame;	///< when it leaves anyway (0: never)
	Bool m_leaving;
	Coord3D m_exit;							///< past the map's nearest edge, where it flies
};
