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

// FILE: TiberiumArrivalUpdate.h //////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's Crawler arrival: in a game with zones (TiberiumZones.h), a unit with
// this that a factory makes does not drive out of its door; it arrives in one of its player's zones (the
// one nearest the factory's rally point, at the rally point when it is in it): GDI's dropped from the
// sky (DROP), Nod's drilling up from under the ground (DRILL). Held while it arrives. Units made in
// place of another (a Crawler packing up) and those on the map from the start do not.
//
//   Behavior = TiberiumArrivalUpdate ModuleTag_Arrival
//     Style = AUTO                           ; DROP, DRILL, or AUTO: Nod's sides drill, the rest drop
//     ArrivalTime = 2000                     ; ms
//     DropHeight = 300
//     DrillDepth = 40
//     StartFX = FX_...                       ; where it will land, as it starts
//     LandFX = FX_...                        ; as it lands (or breaks the surface)
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpdateModule.h"

class FXList;

enum TiberiumArrivalStyle CPP_11(: Int)
{
	TIBERIUM_ARRIVAL_AUTO,
	TIBERIUM_ARRIVAL_DROP,
	TIBERIUM_ARRIVAL_DRILL
};

//-------------------------------------------------------------------------------------------------
class TiberiumArrivalUpdateModuleData : public UpdateModuleData
{
public:
	Int m_style;
	UnsignedInt m_arrivalTime;
	Real m_dropHeight;
	Real m_drillDepth;
	const FXList *m_startFX;
	const FXList *m_landFX;

	TiberiumArrivalUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class TiberiumArrivalUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TiberiumArrivalUpdate, "TiberiumArrivalUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( TiberiumArrivalUpdate, TiberiumArrivalUpdateModuleData )

public:

	TiberiumArrivalUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

private:

	enum
	{
		STAGE_START,
		STAGE_ARRIVING,
		STAGE_DONE
	};

	void showOffset() const;

	Int m_stage;
	Int m_style;							///< DROP or DRILL, once it starts
	UnsignedInt m_startFrame;
	UnsignedInt m_endFrame;
};
