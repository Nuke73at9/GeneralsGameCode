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

// FILE: CreateObjectDie.h /////////////////////////////////////////////////////////////////////////////
// Author: Michael S. Booth, January 2002
// Desc:   Create object at current object's death
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/INI.h"
#include "GameLogic/Module/DieModule.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Thing;
class ObjectCreationList;

//-------------------------------------------------------------------------------------------------
class CreateObjectDieModuleData : public DieModuleData
{

public:

	const ObjectCreationList* m_ocl;			///< object creaton list to make
	Bool m_transferPreviousHealth; ///< Transfers previous health before death to the new object created.
	Bool m_transferSelection;      ///< Transfers selection state before death to the new object created.
	/// Tiberium Project @feature its list made ShareCount times for every ShareOf such deaths of its owner's (a running
	/// tally kept by object and module): Kane's Wrath's squad deaths, its squads being their soldiers here (the Redemption:
	/// 3 of 9 for a Militant, the three Awakened of one squad for each nine)
	Int m_shareCount;
	Int m_shareOf;
	/// Tiberium Project @feature how likely its list is made at all (SAGE's ChanceCreateObjectDie: C&C4's units leave a crate
	/// a quarter to three quarters of the time)
	Real m_chance;

	CreateObjectDieModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);

};

//-------------------------------------------------------------------------------------------------
/** When this object dies, create another object in its place */
//-------------------------------------------------------------------------------------------------
class CreateObjectDie : public DieModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( CreateObjectDie, "CreateObjectDie"  )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( CreateObjectDie, CreateObjectDieModuleData );

public:

	CreateObjectDie( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual void onDie( const DamageInfo *damageInfo ) override;

};
