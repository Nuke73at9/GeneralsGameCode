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

// FILE: StealthFieldUpdate.h /////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's stealth fields (C&C4's InvisibilityBroadcastUpdate): every
// UpdatePeriod, the friends within Radius are hidden until the next period ends (Zero Hour's stealth,
// granted for a while: those with a StealthUpdate that is not stealthy by itself). Walking out of the
// field, they show again.
//
//   Behavior = StealthFieldUpdate ModuleTag_Field
//     Radius = 200.0
//     UpdatePeriod = 500              ; msec
//     KindOf = INFANTRY VEHICLE STRUCTURE    ; (default: all)
//     ForbiddenKindOf = AIRCRAFT
//     AffectsSelf = Yes
//     StartsActive = No               ; else on from the start
//     TriggeredBy = Upgrade_X         ; (upgrade mux) on once this is had
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"

//-------------------------------------------------------------------------------------------------
class StealthFieldUpdateModuleData : public UpdateModuleData
{
public:
	UpgradeMuxData m_upgradeMuxData;
	Bool m_initiallyActive;
	Real m_radius;
	UnsignedInt m_updatePeriod;
	KindOfMaskType m_kindOf;
	KindOfMaskType m_forbiddenKindOf;
	Bool m_affectsSelf;

	StealthFieldUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class StealthFieldUpdate : public UpdateModule, public UpgradeMux
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( StealthFieldUpdate, "StealthFieldUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( StealthFieldUpdate, StealthFieldUpdateModuleData )

public:

	StealthFieldUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | MODULEINTERFACE_UPGRADE; }
	virtual UpgradeModuleInterface* getUpgrade() override { return this; }

	virtual UpdateSleepTime update() override;

protected:

	virtual void upgradeImplementation() override { setWakeFrame( getObject(), UPDATE_SLEEP_NONE ); }
	virtual void getUpgradeActivationMasks( UpgradeMaskType& activation, UpgradeMaskType& conflicting ) const override
	{
		getStealthFieldUpdateModuleData()->m_upgradeMuxData.getUpgradeActivationMasks( activation, conflicting );
	}
	virtual void performUpgradeFX() override
	{
		getStealthFieldUpdateModuleData()->m_upgradeMuxData.performUpgradeFX( getObject() );
	}
	virtual void processUpgradeRemoval() override
	{
		getStealthFieldUpdateModuleData()->m_upgradeMuxData.muxDataProcessUpgradeRemoval( getObject() );
	}
	virtual Bool requiresAllActivationUpgrades() const override
	{
		return getStealthFieldUpdateModuleData()->m_upgradeMuxData.m_requiresAllTriggers;
	}
	virtual Bool isSubObjectsUpgrade() override { return false; }

};
