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
// place of another (a Crawler packing up) and those on the map from the start do not. One that C&C4's
// match start or a redeployment makes (TiberiumStart) arrives where it was made, zones or not.
//
//   Behavior = TiberiumArrivalUpdate ModuleTag_Arrival
//     Style = AUTO                           ; DROP, DRILL, or AUTO: Nod's sides drill, the rest drop
//     ArrivalTime = 2000                     ; ms
//     DropHeight = 300
//     DrillDepth = 40
//     StartFX = FX_...                       ; where it will land, as it starts
//     LandFX = FX_...                        ; as it lands (or breaks the surface)
//   End
//
// Tiberium Project @feature C&C4's own arrival chain (its redeployment OCLs), opt-in: a warning flare, then a pod
// with its own model, animation and effects (GDI's drop pod, Nod's burrowing pod), then the unit, which enemies
// at the spot do not survive and which stands busy for a moment. Until it comes out the unit is not seen, not
// selected, not targeted and collides with nothing; with a pod it does not move down or up itself.
//     FlareTime = 2000                       ; ms of the warning flare (C&C4's SPECIAL_POWER_WARNING_FLARE_DURATION)
//     FlareFX = FX_...                       ; at the spot as the flare starts
//     PodObject = ...                        ; made at the spot when the flare is over, the unit's player's
//     PodFX = FX_...                         ; at the spot as the pod is made (its own effects as it is made)
//     PodTime = 1500                         ; ms from the pod to the unit coming out
//     LandWeapon = ...                       ; fired at the spot as the unit comes out (C&C4: enemies within 125)
//     BusyTime = 2000                        ; ms held after it comes out (C&C4's StartingBusyTime)
//     Facing = -45                           ; degrees it faces as it comes out (C&C4's CRAWLER_DISPOSITION_ANGLE)
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpdateModule.h"

class FXList;
class WeaponTemplate;

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
	UnsignedInt m_flareTime;
	const FXList *m_flareFX;
	AsciiString m_podObject;
	const FXList *m_podFX;
	UnsignedInt m_podTime;
	const WeaponTemplate *m_landWeapon;
	UnsignedInt m_busyTime;
	Real m_facing;									///< radians
	Bool m_hasFacing;

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
	/// Tiberium Project @feature C&C4's match start and redeployment (TiberiumStart): made at its spot, it arrives
	/// there (whoever made it, with or without zones)
	void beginPlaced() { m_placed = TRUE; }
	Bool isArriving() const;							///< yet to come out (or to land)
	/// (Tiberium Project: it holds the unit itself while it arrives, and must go on meanwhile)
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return MAKE_DISABLED_MASK( DISABLED_HELD ); }

private:

	enum
	{
		STAGE_START,
		STAGE_ARRIVING,
		STAGE_DONE,
		STAGE_FLARE,						///< C&C4's chain: the flare, then
		STAGE_POD								///< the pod (the unit unseen until it comes out)
	};

	void showOffset() const;
	void setUnseen( Bool unseen );
	void comeOut();
	Bool usesChain() const;

	Int m_stage;
	Int m_style;							///< DROP or DRILL, once it starts
	UnsignedInt m_startFrame;
	UnsignedInt m_endFrame;
	Bool m_placed;						///< made at its spot (TiberiumStart)
};
