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

// FILE: ReplaceSelfUpdate.h //////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's ReplaceSelfSpecialAbility: a special power that replaces the
// object with another (a C&C4 Crawler deploys into its structure form and packs up into its vehicle
// form). The new object keeps the old one's health (as a share of its own), veterancy, owner,
// place, facing, selection and hotkey group; whatever the old one was building is refunded. Given a
// spot (a location power: C&C4's deploy where you click), it drives there first. The new
// object is held for NewObjectUnpackTime while it unfolds, showing CRAWLER_TRANSITION_1 for its own
// Transition1Time and then CRAWLER_TRANSITION_2 for its Transition2Time (its arrival, if it has this
// module too).
//
//   Behavior = SpecialAbility ModuleTag_DeployPower
//     SpecialPowerTemplate = C4SP_CrawlerDeploy
//   End
//   Behavior = ReplaceSelfUpdate ModuleTag_Deploy
//     SpecialPowerTemplate = C4SP_CrawlerDeploy
//     ReplacementTemplate  = C4_GDICrawlerBStructure
//     NewObjectUnpackTime  = 1930
//     Transition1Time      = 670    ; this object's own arrival, when something turns into it
//     Transition2Time      = 1330
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
class ReplaceSelfUpdateModuleData : public UpdateModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;	///< the power that replaces us
	AsciiString m_replacementName;					///< what we turn into
	UnsignedInt m_newObjectUnpackTime;				///< frames the new object is held while it unfolds
	UnsignedInt m_transition1Time;					///< frames of CRAWLER_TRANSITION_1 when we arrive
	UnsignedInt m_transition2Time;					///< frames of CRAWLER_TRANSITION_2 after it

	ReplaceSelfUpdateModuleData()
	{
		m_specialPowerTemplate = nullptr;
		m_newObjectUnpackTime = 0;
		m_transition1Time = 0;
		m_transition2Time = 0;
	}

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpdateModuleData::buildFieldParse( p );
		static const FieldParse dataFieldParse[] =
		{
			{ "SpecialPowerTemplate",	INI::parseSpecialPowerTemplate,	nullptr, offsetof( ReplaceSelfUpdateModuleData, m_specialPowerTemplate ) },
			{ "ReplacementTemplate",	INI::parseAsciiString,			nullptr, offsetof( ReplaceSelfUpdateModuleData, m_replacementName ) },
			{ "NewObjectUnpackTime",	INI::parseDurationUnsignedInt,	nullptr, offsetof( ReplaceSelfUpdateModuleData, m_newObjectUnpackTime ) },
			{ "Transition1Time",		INI::parseDurationUnsignedInt,	nullptr, offsetof( ReplaceSelfUpdateModuleData, m_transition1Time ) },
			{ "Transition2Time",		INI::parseDurationUnsignedInt,	nullptr, offsetof( ReplaceSelfUpdateModuleData, m_transition2Time ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add( dataFieldParse );
	}
};

//-------------------------------------------------------------------------------------------------
class ReplaceSelfUpdate : public SpecialPowerUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ReplaceSelfUpdate, "ReplaceSelfUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ReplaceSelfUpdate, ReplaceSelfUpdateModuleData )

public:

	ReplaceSelfUpdate( Thing *thing, const ModuleData* moduleData );
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

	/// we were just made in place of another object: show our arrival (the transition conditions)
	void beginArrival();

private:

	enum Stage CPP_11(: Int)
	{
		STAGE_NONE = 0,
		STAGE_TRANSITION_1,
		STAGE_TRANSITION_2,
		STAGE_MOVING,		///< driving to the spot it was told to deploy at
	};

	void enterStage( Stage stage );
	void replace();

	Stage m_stage;
	UnsignedInt m_stageEndFrame;
	Coord3D m_goal;			///< where it deploys (STAGE_MOVING)

};
