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

// FILE: TiberiumZoneNodeUpdate.h /////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's node (the TCN's goal structure) that takes an expansion zone
// (TiberiumZones.h): taken by standing next to it. While one side's units (a player's and its allies')
// are within CaptureRadius and none of their enemies are, the node turns to that player in CaptureTime;
// a node an enemy holds is first brought back to nobody's in DecaptureTime. Nobody near: the progress
// fades. Its holder holds the zone it stands in. The model shows GDI's light (USER_1) or Nod's (USER_2).
//
//   Behavior = TiberiumZoneNodeUpdate ModuleTag_Node
//     CaptureRadius = 120
//     CaptureTime = 20000                    ; ms
//     DecaptureTime = 30000                  ; ms
//     CapturerKindOf = INFANTRY VEHICLE
//     CapturerForbiddenKindOf = AIRCRAFT
//     CaptureFX = FX_...
//     LoseFX = FX_...
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/KindOf.h"
#include "GameLogic/Module/UpdateModule.h"

class FXList;
class Player;

//-------------------------------------------------------------------------------------------------
class TiberiumZoneNodeUpdateModuleData : public UpdateModuleData
{
public:
	Real m_captureRadius;
	UnsignedInt m_captureTime;
	UnsignedInt m_decaptureTime;
	KindOfMaskType m_capturerKindOf;
	KindOfMaskType m_capturerForbiddenKindOf;
	const FXList *m_captureFX;
	const FXList *m_loseFX;

	TiberiumZoneNodeUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class TiberiumZoneNodeUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TiberiumZoneNodeUpdate, "TiberiumZoneNodeUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( TiberiumZoneNodeUpdate, TiberiumZoneNodeUpdateModuleData )

public:

	TiberiumZoneNodeUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

	static Bool isNode( const Object *obj );
	/// a node's taking, for drawing: who is taking it (or bringing it back to nobody's) and how far (0 to 1)
	static Bool getCapture( const Object *obj, Int *playerIndex, Real *fraction, Bool *decapturing );

private:

	void showHolder();

	Int m_capturer;					///< the player taking it (-1: nobody)
	UnsignedInt m_progress;	///< frames of taking done
};
