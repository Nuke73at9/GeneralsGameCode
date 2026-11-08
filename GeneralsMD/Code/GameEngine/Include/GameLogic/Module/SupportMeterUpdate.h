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

// FILE: SupportMeterUpdate.h /////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's support meter: while this object lives (C&C4's Support Crawler, in
// either of its forms), its player's meter fills PointsPerMinute (UpgradedPointsPerMinute once
// FasterWithUpgrade, an object or a player upgrade, is had), to MaxPoints. The support powers spend
// it (SpecialPowerTemplate SupportPoints). When the last of the player's objects with this module
// dies, the meter empties (as C&C4's does when the Crawler is lost); one replaced by another form
// (deploying, packing up) is not a death.
//
//   Behavior = SupportMeterUpdate ModuleTag_SupportMeter
//     PointsPerMinute         = 5
//     UpgradedPointsPerMinute = 10
//     FasterWithUpgrade       = Upgrade_C4_GDISupportTier3
//     MaxPoints               = 50
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/DieModule.h"
#include "GameLogic/Module/UpdateModule.h"

//-------------------------------------------------------------------------------------------------
class SupportMeterUpdateModuleData : public UpdateModuleData
{
public:
	Int m_pointsPerMinute;
	Int m_upgradedPointsPerMinute;
	AsciiString m_fasterWithUpgrade;
	Int m_maxPoints;

	SupportMeterUpdateModuleData()
	{
		m_pointsPerMinute = 5;
		m_upgradedPointsPerMinute = 10;
		m_maxPoints = 50;
	}

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpdateModuleData::buildFieldParse( p );
		static const FieldParse dataFieldParse[] =
		{
			{ "PointsPerMinute",					INI::parseInt,						nullptr, offsetof( SupportMeterUpdateModuleData, m_pointsPerMinute ) },
			{ "UpgradedPointsPerMinute",	INI::parseInt,						nullptr, offsetof( SupportMeterUpdateModuleData, m_upgradedPointsPerMinute ) },
			{ "FasterWithUpgrade",				INI::parseAsciiString,		nullptr, offsetof( SupportMeterUpdateModuleData, m_fasterWithUpgrade ) },
			{ "MaxPoints",								INI::parseInt,						nullptr, offsetof( SupportMeterUpdateModuleData, m_maxPoints ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add( dataFieldParse );
	}
};

//-------------------------------------------------------------------------------------------------
class SupportMeterUpdate : public UpdateModule, public DieModuleInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( SupportMeterUpdate, "SupportMeterUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( SupportMeterUpdate, SupportMeterUpdateModuleData )

public:
	SupportMeterUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | (MODULEINTERFACE_DIE); }

	virtual UpdateSleepTime update() override;

	virtual DieModuleInterface *getDie() override { return this; }
	virtual void onDie( const DamageInfo *damageInfo ) override;

	/// the rate the meter fills at for this object now (points a minute)
	Int getPointsPerMinute() const;
};
