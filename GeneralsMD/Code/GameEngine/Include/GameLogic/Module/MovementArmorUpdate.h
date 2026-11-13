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

// FILE: MovementArmorUpdate.h ////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's MovementArmorUpdate: an attribute modifier the object carries while it moves
// faster than MinimumSpeed, taken off RemoveDelayTime after it stops. With DeactivateShieldSphere its shield
// spheres are held down meanwhile (C&C4's Centurion: its deflecting shield is up only while it stands).
//
//   Behavior = MovementArmorUpdate ModuleTag_ShieldSuppress
//     AttributeModifier      = C4AM_Modifier_NODCenturionVehVShieldOff
//     MinimumSpeed           = 0        ; distance a second: moving is faster than this
//     UpdateRefresh          = 0        ; between its looks (0: every frame)
//     RemoveDelayTime        = 300
//     DeactivateShieldSphere = Yes
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpdateModule.h"

//-------------------------------------------------------------------------------------------------
class MovementArmorUpdateModuleData : public UpdateModuleData
{
public:
	NameKeyType m_modifier;						///< carried while it moves
	Real m_minimumSpeed;							///< distance a frame: moving is faster than this
	UnsignedInt m_updateRefresh;			///< frames between its looks
	UnsignedInt m_removeDelay;				///< frames after it stops
	Bool m_deactivateShieldSphere;		///< its shield spheres are down meanwhile

	MovementArmorUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse &p );
};

//-------------------------------------------------------------------------------------------------
class MovementArmorUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( MovementArmorUpdate, "MovementArmorUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( MovementArmorUpdate, MovementArmorUpdateModuleData )

public:
	MovementArmorUpdate( Thing *thing, const ModuleData *moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

private:
	void setActive( Bool on );

	Bool m_active;										///< carrying the modifier
	UnsignedInt m_lastMovingFrame;		///< the frame it last moved on
};
