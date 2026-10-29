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

// FILE: AssembleStructureUpdate.h ////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's AssembleStructureSpecialAbilityUpdate (C&C4's outposts: a unit
// that is no dozer builds one structure where it is told): the unit goes within StartAbilityRange of
// the spot, pays the structure's cost, and builds it over its build time as a dozer would (the
// structure rising, its health with it), standing by it; another order calls the building off (the
// structure stays as far as it got, for a dozer to finish).
//
//   Behavior = SpecialAbility ModuleTag_OutpostPower
//     SpecialPowerTemplate = C4SP_SpecialPower_GDIBuildStructureA
//   End
//   Behavior = AssembleStructureUpdate ModuleTag_Outpost
//     SpecialPowerTemplate = C4SP_SpecialPower_GDIBuildStructureA
//     StructureTemplate    = C4_GDIStructureA
//     StartAbilityRange    = 78
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/INI.h"
#include "GameLogic/Module/UpdateModule.h"

class CommandButton;
class SpecialPowerTemplate;
class Waypoint;

#include "GameLogic/Module/SpecialPowerUpdateModule.h"

//-------------------------------------------------------------------------------------------------
class AssembleStructureUpdateModuleData : public UpdateModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;
	AsciiString m_structureName;
	Real m_startAbilityRange;

	AssembleStructureUpdateModuleData()
	{
		m_specialPowerTemplate = nullptr;
		m_startAbilityRange = 60.0f;
	}

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpdateModuleData::buildFieldParse( p );
		static const FieldParse dataFieldParse[] =
		{
			{ "SpecialPowerTemplate",	INI::parseSpecialPowerTemplate,	nullptr, offsetof( AssembleStructureUpdateModuleData, m_specialPowerTemplate ) },
			{ "StructureTemplate",		INI::parseAsciiString,			nullptr, offsetof( AssembleStructureUpdateModuleData, m_structureName ) },
			{ "StartAbilityRange",		INI::parseReal,					nullptr, offsetof( AssembleStructureUpdateModuleData, m_startAbilityRange ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add( dataFieldParse );
	}
};

//-------------------------------------------------------------------------------------------------
class AssembleStructureUpdate : public SpecialPowerUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AssembleStructureUpdate, "AssembleStructureUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AssembleStructureUpdate, AssembleStructureUpdateModuleData )

public:

	AssembleStructureUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	// SpecialPowerUpdateInterface
	virtual Bool initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions ) override;
	// (not a SpecialAbilityUpdate: Object::findSpecialAbilityUpdate takes any special ability for one)
	virtual Bool isSpecialAbility() const override { return false; }
	virtual Bool isSpecialPower() const override { return false; }
	virtual Bool isActive() const override { return m_stage != STAGE_NONE; }
	virtual Bool doesSpecialPowerHaveOverridableDestinationActive() const override { return false; }
	virtual Bool doesSpecialPowerHaveOverridableDestination() const override { return false; }
	virtual void setSpecialPowerOverridableDestination( const Coord3D *loc ) override {}
	virtual SpecialPowerUpdateInterface* getSpecialPowerUpdateInterface() override { return this; }
	virtual CommandOption getCommandOption() const override { return (CommandOption)0; }
	virtual Bool isPowerCurrentlyInUse( const CommandButton *command = nullptr ) const override { return m_stage != STAGE_NONE; }

	virtual UpdateSleepTime update() override;

private:

	enum Stage CPP_11(: Int)
	{
		STAGE_NONE = 0,
		STAGE_MOVING,		///< on its way to the spot
		STAGE_BUILDING,	///< building it
	};

	Bool canPlace() const;	///< the spot is free and the player can pay
	void place();
	void stop();

	Stage m_stage;
	Coord3D m_goal;
	Real m_angle;
	ObjectID m_structureID;

};
