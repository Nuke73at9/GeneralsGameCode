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

// FILE: TiberiumPickupUpdate.h ///////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's crystal pickup: an object lying on the map (a rare crystal a Tiberium
// field grows, say) that a unit picks up by driving or walking over it and carries to one of its own
// drop-off points: a refinery, a Crawler. There the delivery promotes the carrier CarrierLevels ranks,
// and every unit of its owner within ShareRadius ShareLevels ranks. Carried, the pickup is drawn
// CarryHeight above its carrier; if the carrier dies, it falls where it stood, for anyone to take.
//
//   Behavior = TiberiumPickupUpdate ModuleTag_Pickup
//     PickupRadius = 15
//     CarrierKindOf = INFANTRY VEHICLE       ; any of these may carry it
//     CarrierForbiddenKindOf = AIRCRAFT      ; none of these
//     CarryHeight = 25
//     DropOffRadius = 120
//     DropOffKindOf = FS_SUPPLY_CENTER       ; any of these is a drop-off point
//     DropOffNameContains = Crawler          ; and objects with this in their name (repeatable)
//     CarrierLevels = 1
//     ShareRadius = 200
//     ShareLevels = 1
//     PickupFX = FX_...
//     DeliverFX = FX_...
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/KindOf.h"
#include "GameLogic/Module/UpdateModule.h"

#include <vector>

class FXList;

//-------------------------------------------------------------------------------------------------
class TiberiumPickupUpdateModuleData : public UpdateModuleData
{
public:
	Real m_pickupRadius;
	KindOfMaskType m_carrierKindOf;
	KindOfMaskType m_carrierForbiddenKindOf;
	Real m_carryHeight;
	Real m_dropOffRadius;
	KindOfMaskType m_dropOffKindOf;
	std::vector<AsciiString> m_dropOffNames;
	Int m_carrierLevels;
	Real m_shareRadius;
	Int m_shareLevels;
	const FXList *m_pickupFX;
	const FXList *m_deliverFX;

	TiberiumPickupUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
	static void parseName( INI *ini, void *instance, void *store, const void *userData );
};

//-------------------------------------------------------------------------------------------------
class TiberiumPickupUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TiberiumPickupUpdate, "TiberiumPickupUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( TiberiumPickupUpdate, TiberiumPickupUpdateModuleData )

public:

	TiberiumPickupUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

	ObjectID getCarrier() const { return m_carrier; }

private:

	Object *findCarrier() const;					///< a unit standing on it that may take it
	Object *findDropOff( Object *carrier ) const;	///< one of the carrier's drop-off points within reach
	void deliver( Object *carrier );

	ObjectID m_carrier;		///< who carries it (INVALID_ID: it lies on the ground)
};
