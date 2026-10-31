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

// FILE: TiberiumFieldUpdate.h ////////////////////////////////////////////////////////////////////
// Tiberium Project @feature Kane's Wrath's Tiberium field: crystals (TiberiumCrystalUpdate, harvestable
// supply sources) around the field's centre, on a grid Spacing apart within Radius, on open ground. It
// starts with StartCrystals and grows one more every SpawnDelay, up to MaxCrystals. Each crystal is one of
// the field's Crystal kinds, by their weights (a green field with some blue in it, say). A field with a
// RareObject (C&C4's promotion crystal, a pickup) grows one of those at a free spot RareDelay after the
// last was taken away, one at a time.
//
//   Behavior = TiberiumFieldUpdate ModuleTag_Field
//     Radius = 200
//     Spacing = 40
//     StartCrystals = 12
//     MaxCrystals = 20
//     SpawnDelay = 30000
//     Crystal = TiberiumCrystalGreen 90
//     Crystal = TiberiumCrystalBlue 10
//     RareObject = TiberiumPromotionCrystal
//     RareDelay = 180000
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpdateModule.h"

#include <vector>

//-------------------------------------------------------------------------------------------------
class TiberiumFieldUpdateModuleData : public UpdateModuleData
{
public:
	struct CrystalKind
	{
		AsciiString name;
		Int weight;
	};

	Real m_radius;
	Real m_spacing;
	Int m_startCrystals;
	Int m_maxCrystals;
	UnsignedInt m_spawnDelay;
	std::vector<CrystalKind> m_kinds;
	AsciiString m_rareObject;
	UnsignedInt m_rareDelay;

	TiberiumFieldUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
	static void parseCrystal( INI *ini, void *instance, void *store, const void *userData );
};

//-------------------------------------------------------------------------------------------------
class TiberiumFieldUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TiberiumFieldUpdate, "TiberiumFieldUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( TiberiumFieldUpdate, TiberiumFieldUpdateModuleData )

public:

	TiberiumFieldUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

	Int getCrystalCount() const { return (Int)m_crystals.size(); }

private:

	Bool findSpot( Coord3D *at ) const;	///< a free spot of the field's grid (jittered); false when there is none
	Bool spawnOne();						///< a crystal at a free spot; false when there is none
	void growRare( UnsignedInt now );		///< the rare object, when it is time

	std::vector<ObjectID> m_crystals;
	UnsignedInt m_nextSpawnFrame;
	Bool m_started;
	ObjectID m_rare;						///< the rare object it grew, while it is about
	UnsignedInt m_nextRareFrame;			///< when it grows the next (0: not yet counting)

};
