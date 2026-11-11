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

// FILE: BannerCarrierUpdate.h ////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's BannerCarrierUpgrade and HordeContain's banner carrier: a Kane's Wrath squad's
// leader. The soldiers of a squad built together (a unit's BuildQuantity: ProductionUpdate tells each of them the
// first of them) get one BannerCarrier between them, made beside the first of them to look once the module is on:
// its TriggeredBy upgrade (Kane's Wrath's Confessor upgrade puts a Confessor with each Militant squad, the Black
// Disciples a Black Hand), or, with none, from the start (the Sniper Team's Spotter). A squad that stood before the
// upgrade gets one as well as those built after it. One a squad in all: the leader killed, none comes again (the
// upgrade is Permanent). The leader stands Offset from the soldier (ahead, to its left) and goes where it goes.
//
//   Behavior = BannerCarrierUpdate ModuleTag_Leader
//     BannerCarrier = C4_NODConfessor
//     Offset        = X:8 Y:0
//     TriggeredBy   = C4Up_Upgrade_NODConfessorUpgrade
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/INI.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"

class Object;

//-------------------------------------------------------------------------------------------------
class BannerCarrierUpdateModuleData : public UpdateModuleData
{
public:
	UpgradeMuxData m_upgradeMuxData;
	AsciiString m_carrierName;		///< the leader (found when it is made: the objects load in any order)
	Coord2D m_offset;							///< from the soldier: ahead of it, to its left

	BannerCarrierUpdateModuleData()
	{
		m_offset.x = 0.0f;
		m_offset.y = 0.0f;
	}

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		static const FieldParse dataFieldParse[] =
		{
			{ "BannerCarrier",	INI::parseAsciiString,	nullptr, offsetof( BannerCarrierUpdateModuleData, m_carrierName ) },
			{ "Offset",					INI::parseCoord2D,			nullptr, offsetof( BannerCarrierUpdateModuleData, m_offset ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		UpdateModuleData::buildFieldParse( p );
		p.add( dataFieldParse );
		p.add( UpgradeMuxData::getFieldParse(), offsetof( BannerCarrierUpdateModuleData, m_upgradeMuxData ) );
	}
};

//-------------------------------------------------------------------------------------------------
class BannerCarrierUpdate : public UpdateModule, public UpgradeMux
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( BannerCarrierUpdate, "BannerCarrierUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( BannerCarrierUpdate, BannerCarrierUpdateModuleData )

public:

	BannerCarrierUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | MODULEINTERFACE_UPGRADE; }
	virtual UpgradeModuleInterface* getUpgrade() override { return this; }

	virtual UpdateSleepTime update() override;

	/// the squad a soldier is built in: the first of those built together (ProductionUpdate)
	static void joinSquad( Object *obj, ObjectID squadID );

	ObjectID getSquadID() const;
	const AsciiString &getCarrierName() const { return getBannerCarrierUpdateModuleData()->m_carrierName; }
	Bool hasGivenCarrier() const { return m_given; }
	void markGiven() { m_given = TRUE; }

protected:

	virtual void upgradeImplementation() override;
	virtual void getUpgradeActivationMasks( UpgradeMaskType& activation, UpgradeMaskType& conflicting ) const override
	{
		getBannerCarrierUpdateModuleData()->m_upgradeMuxData.getUpgradeActivationMasks( activation, conflicting );
	}
	virtual void performUpgradeFX() override
	{
		getBannerCarrierUpdateModuleData()->m_upgradeMuxData.performUpgradeFX( getObject() );
	}
	virtual void processUpgradeRemoval() override
	{
		getBannerCarrierUpdateModuleData()->m_upgradeMuxData.muxDataProcessUpgradeRemoval( getObject() );
	}
	virtual Bool requiresAllActivationUpgrades() const override
	{
		return getBannerCarrierUpdateModuleData()->m_upgradeMuxData.m_requiresAllTriggers;
	}
	virtual Bool isSubObjectsUpgrade() override { return false; }

private:

	ObjectID m_squadID;		///< the first of the squad (none: a squad of one, itself)
	Bool m_wanted;				///< the module is on: the squad is due its leader
	Bool m_given;					///< the squad's leader has been made (by this one or another of the squad)

};
