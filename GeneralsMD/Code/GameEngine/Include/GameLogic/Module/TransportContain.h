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

// FILE: TransportContain.h ////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, March 2002
// Desc:   Contain module for transport units.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/OpenContain.h"

//-------------------------------------------------------------------------------------------------
class TransportContainModuleData : public OpenContainModuleData
{
public:
	struct InitialPayload
	{
		AsciiString name;
		Int count;
	};

	Int								m_slotCapacity;								///< max units that can be inside us
	Real							m_exitPitchRate;
	AsciiString				m_exitBone;
	InitialPayload		m_initialPayload;
	Real							m_healthRegen;
	UnsignedInt				m_exitDelay;
	Bool							m_scatterNearbyOnExit;
	Bool							m_orientLikeContainerOnExit;
	Bool							m_keepContainerVelocityOnExit;
	Bool							m_goAggressiveOnExit;
	Bool							m_armedRidersUpgradeWeaponSet;
	Bool							m_resetMoodCheckTimeOnExit;
	Bool							m_destroyRidersWhoAreNotFreeToExit;
	Bool							m_isDelayExitInAir;
	Bool							m_leaveMapWhenEmpty;	///< Tiberium Project @feature once it has carried someone and is empty again, it leaves the map (Kane's Wrath's summoned transports)

	// Tiberium Project @feature the places its riders take (Kane's Wrath's UpgradeContainerOnContain): a rider takes
	// a place of its kind with room, else the first free one, and gives the transport its kind's upgrade for that place
	// until the place is empty again (the MARV: a squad's gun on each of its treads)
	struct PlaceKind
	{
		std::vector<AsciiString>	m_upgradeNames;		///< the kind's upgrade at each place, in order (PlaceUpgrades)
		std::vector<AsciiString>	m_riderNames;			///< the riders of the kind (PlaceRiders)
		Int												m_ridersPerPlace;	///< how many of them share a place (a squad's soldiers)
	};
	std::vector<PlaceKind>	m_placeKinds;

	TransportContainModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);
	static void parseInitialPayload( INI* ini, void *instance, void *store, const void* /*userData*/ );
	static void parsePlaceUpgrades( INI* ini, void *instance, void *store, const void* /*userData*/ );	///< Tiberium Project
	static void parsePlaceRiders( INI* ini, void *instance, void *store, const void* /*userData*/ );	///< Tiberium Project
	Int getPlaceCount() const;	///< Tiberium Project: the places its riders take (0: none)

};

//-------------------------------------------------------------------------------------------------
class TransportContain : public OpenContain
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TransportContain, "TransportContain" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( TransportContain, TransportContainModuleData )

public:

	TransportContain( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual Bool isValidContainerFor( const Object* obj, Bool checkCapacity) const override;

	virtual void onCapture( Player *oldOwner, Player *newOwner ) override; // have to kick everyone out on capture.
	virtual void onContaining( Object *obj, Bool wasSelected ) override;		///< object now contains 'obj'
	virtual void onRemoving( Object *obj ) override;			///< object no longer contains 'obj'
	virtual UpdateSleepTime update() override;							///< called once per frame

	virtual Bool isRiderChangeContain() const override { return FALSE; }
  virtual Bool isSpecialOverlordStyleContainer() const override {return FALSE;}

	virtual Int getContainMax() const override;

	virtual Int getExtraSlotsInUse() override { return m_extraSlotsInUse; }///< Transports have the ability to carry guys how take up more than spot.

	virtual Bool isExitBusy() const override;	///< Contain style exiters are getting the ability to space out exits, so ask this before reserveDoor as a kind of no-commitment check.
	virtual ExitDoorType reserveDoorForExit( const ThingTemplate* objType, Object *specificObject ) override;
	virtual void unreserveDoorForExit( ExitDoorType exitDoor ) override;
	virtual Bool isDisplayedOnControlBar() const override {return TRUE;}///< Does this container display its contents on the ControlBar?
	virtual Bool getContainerPipsToShow( Int& numTotal, Int& numFull ) override;	///< Tiberium Project: its places, when it has them

protected:

	// exists primarily for TransportContain to override
	virtual void killRidersWhoAreNotFreeToExit() override;
	virtual Bool isSpecificRiderFreeToExit(Object* obj);
	virtual Bool isPassengerAllowedToFire( ObjectID id = INVALID_ID ) const override;	///< Hey, can I shoot out of this container?

	virtual void createPayload();
	void letRidersUpgradeWeaponSet();
	void leaveMapWhenEmpty();	///< Tiberium Project (LeaveMapWhenEmpty)

	Bool m_payloadCreated;

private:

	Int m_extraSlotsInUse;
	UnsignedInt m_frameExitNotBusy;
	Bool m_hasCarried;		///< Tiberium Project: someone has been inside (LeaveMapWhenEmpty)
	Bool m_leavingMap;		///< Tiberium Project: on its way off the map

	// Tiberium Project @feature the places its riders take (PlaceUpgrades)
	enum { MAX_RIDER_PLACES = 8 };
	struct PlaceRider
	{
		ObjectID	m_id;
		Int				m_place;
	};
	Int												m_placeKind[MAX_RIDER_PLACES];	///< the kind of rider at each place (-1: free)
	std::vector<PlaceRider>		m_placeRiders;									///< who is at which place
	Int findPlaceKind( const Object* rider ) const;
	Int ridersAtPlace( Int place ) const;
	Int choosePlace( Int kind ) const;
	void takePlace( Object* rider );
	void leavePlace( Object* rider );

};
