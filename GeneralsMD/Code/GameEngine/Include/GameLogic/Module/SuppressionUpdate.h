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


// FILE: SuppressionUpdate.h //////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's SuppressionUpdate (Kane's Wrath's infantry pinned down by fire): a weapon's
// suppression (Weapon SuppressionAmount, SuppressionRadius, SuppressionDuration: SAGE's SuppressionNugget) adds to that of
// each enemy within its radius that has this module, for its duration. Every UpdateDelay the object adds up what is on
// it; at Suppressability or more it is suppressed for SuppressionDuration: it carries SuppressedModifier (Kane's Wrath's:
// it hugs the ground, its PRONE look, crawls at a quarter of its speed and takes three quarters of the damage). Told by
// its player to move while suppressed it gets up and goes, carrying ForceMoveModifier instead (more damage taken). Not
// while it has any of IgnoreStatus.
//
//   Behavior = SuppressionUpdate ModuleTag_Suppression
//     UpdateDelay         = 1000
//     Suppressability     = 500
//     SuppressionDuration = 5000
//     SuppressedModifier  = KWAM_Modifier_Test_Suppression
//     ForceMoveModifier   = KWAM_Modifier_Test_Suppression_ForceMove
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "Common/ObjectStatusTypes.h"
#include "GameLogic/Module/UpdateModule.h"

//-------------------------------------------------------------------------------------------------
class SuppressionUpdateModuleData : public UpdateModuleData
{
public:
	UnsignedInt m_updateDelay;					///< frames between its sums
	Real m_suppressability;							///< what it takes to suppress it (0: never)
	UnsignedInt m_suppressionDuration;	///< frames it stays suppressed
	NameKeyType m_suppressedModifier;		///< carried while suppressed
	NameKeyType m_forceMoveModifier;		///< carried instead while its player has it move
	ObjectStatusMaskType m_ignoreStatus;	///< none of this while it has any of these

	SuppressionUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse &p );
};

//-------------------------------------------------------------------------------------------------
class SuppressionUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( SuppressionUpdate, "SuppressionUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( SuppressionUpdate, SuppressionUpdateModuleData )

public:
	SuppressionUpdate( Thing *thing, const ModuleData *moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

	/// a weapon's suppression on it, for this many frames
	void addSuppression( Real amount, UnsignedInt frames );
	Bool isSuppressed() const;

	/// the weapon's suppression on each object with this module within the radius of the spot, but its firer's side
	static void suppressAround( const Object *source, const Coord3D *pos, Real radius, Real amount, UnsignedInt frames );

private:
	struct Entry
	{
		Real m_amount;
		UnsignedInt m_expireFrame;
	};
	std::vector<Entry> m_entries;
	UnsignedInt m_suppressedUntil;	///< frame its suppression ends (0: not suppressed)
};
