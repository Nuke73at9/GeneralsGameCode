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

// FILE: AIAbilityUseUpdate.h /////////////////////////////////////////////////////////////////////
// Tiberium Project @feature the skirmish AI's use of a unit's abilities (C&C4's AISpecialPower
// heuristics): for a computer player's unit, every ScanDelay each Power that is ready is used as its
// heuristic says: IN_COMBAT (a power on itself: a berserk, an armour-up) while it fights; AT_VICTIM (a
// power aimed at what it fights: an attack, a debuff, a purge); TOGGLE (a deploy) on while it
// fights, back off after CalmTime without a fight. A human player's units are left alone.
//
//   Behavior = AIAbilityUseUpdate ModuleTag_AIAbilities
//     Power = C4SP_SpecialPower_GDIAircraftQBezerkAbility IN_COMBAT
//     Power = C4SP_SpecialPower_NODVehicleMToggleDeploy TOGGLE
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/INI.h"
#include "GameLogic/Module/UpdateModule.h"

class SpecialPowerTemplate;

enum AIAbilityHeuristic CPP_11(: Int)
{
	AI_ABILITY_IN_COMBAT = 0,
	AI_ABILITY_AT_VICTIM,
	AI_ABILITY_TOGGLE,
	AI_ABILITY_HEURISTIC_COUNT
};

//-------------------------------------------------------------------------------------------------
class AIAbilityUseUpdateModuleData : public UpdateModuleData
{
public:
	struct Power
	{
		const SpecialPowerTemplate *power;
		AIAbilityHeuristic heuristic;
	};
	std::vector<Power> m_powers;
	UnsignedInt m_scanDelay;		///< frames between looks
	UnsignedInt m_calmTime;			///< frames without a fight before a toggle goes back

	AIAbilityUseUpdateModuleData()
	{
		m_scanDelay = LOGICFRAMES_PER_SECOND;
		m_calmTime = 5 * LOGICFRAMES_PER_SECOND;
	}

	static void parsePower( INI *ini, void *instance, void *store, const void *userData );
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class AIAbilityUseUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AIAbilityUseUpdate, "AIAbilityUseUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AIAbilityUseUpdate, AIAbilityUseUpdateModuleData )

public:

	AIAbilityUseUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

private:

	UnsignedInt m_lastFightFrame;
	UnsignedInt m_toggledMask;		///< the TOGGLE powers it has switched on (bit per Power)

};
