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

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// FILE: ConvertToHijackedVehicleCrateCollide.h
// Author: Mark Lorenzen, July 2002
// Desc:   A crate (actually a terrorist - mobile crate) that makes the target vehicle switch
//				 sides, and kills its driver
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/GameCommon.h"
#include "Common/INI.h"
#include "Common/Module.h"
#include "GameLogic/Module/CrateCollide.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Thing;

//-------------------------------------------------------------------------------------------------
class ConvertToHijackedVehicleCrateCollideModuleData : public CrateCollideModuleData
{
public:
	UnsignedInt m_rangeOfEffect;
	/// Tiberium Project @feature RideAlly: it boards one of its own player's vehicles instead of an enemy's, hidden inside
	/// it, the vehicle staying its player's, at least RideVeterancy while it rides, and gets out where the vehicle dies
	/// (C&C4's Commando: SAGE's AttachUpdate with UpgradeToAddToParent, the vehicle made heroic)
	Bool m_rideAlly;
	Int m_rideVeterancy;	///< VeterancyLevel

	ConvertToHijackedVehicleCrateCollideModuleData()
	{
		m_rangeOfEffect = 0;
		m_rideAlly = FALSE;
		m_rideVeterancy = LEVEL_REGULAR;
	}

	static void buildFieldParse(MultiIniFieldParse& p)
	{
    CrateCollideModuleData::buildFieldParse(p);
		static const FieldParse dataFieldParse[] =
		{
			{ "RideAlly",				INI::parseBool,				nullptr,						offsetof( ConvertToHijackedVehicleCrateCollideModuleData, m_rideAlly ) },	// Tiberium Project
			{ "RideVeterancy",	INI::parseIndexList,	TheVeterancyNames,	offsetof( ConvertToHijackedVehicleCrateCollideModuleData, m_rideVeterancy ) },	// Tiberium Project
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add( dataFieldParse );
	}

};

//-------------------------------------------------------------------------------------------------
class ConvertToHijackedVehicleCrateCollide : public CrateCollide
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ConvertToHijackedVehicleCrateCollide, "ConvertToHijackedVehicleCrateCollide" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ConvertToHijackedVehicleCrateCollide, ConvertToHijackedVehicleCrateCollideModuleData );

public:

	ConvertToHijackedVehicleCrateCollide( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

protected:

	/// This allows specific vetoes to certain types of crates and their data
	virtual Bool isValidToExecute( const Object *other ) const override;

	/// This is the game logic execution function that all real CrateCollides will implement
	virtual Bool executeCrateBehavior( Object *other ) override;

	virtual Bool isHijackedVehicleCrateCollide() const override { return TRUE; }
};
