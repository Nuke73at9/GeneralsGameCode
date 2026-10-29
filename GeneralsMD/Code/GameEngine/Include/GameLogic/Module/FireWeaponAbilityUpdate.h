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

// FILE: FireWeaponAbilityUpdate.h ////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's WeaponFireSpecialAbilityUpdate and AttackOffsetSpecialAbilityUpdate:
// a special power that fires weapons of its own (not the unit's), at the target, the target spot or
// where the unit stands. The unit first closes to StartAbilityRange of the target, stops, faces it,
// unpacks (UNPACKING for UnpackTime), waits PreparationTime, fires each Shot after its delay (at an
// offset from the target, turned with the unit), then packs up (PACKING for PackTime).
//
//   Behavior = SpecialAbility ModuleTag_SatchelPower
//     SpecialPowerTemplate = C4SP_SatchelCharge
//   End
//   Behavior = FireWeaponAbilityUpdate ModuleTag_Satchel
//     SpecialPowerTemplate = C4SP_SatchelCharge
//     Shot                 = Weapon:C4W_SatchelCharge Delay:0 Offset:0,0
//     StartAbilityRange    = 30
//     UnpackTime           = 500
//     FaceTarget           = Yes
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "Common/INI.h"
#include "GameLogic/Module/UpdateModule.h"

class CommandButton;
class SpecialPowerTemplate;
class Waypoint;

#include "GameLogic/Module/SpecialPowerUpdateModule.h"

//-------------------------------------------------------------------------------------------------
struct FireWeaponAbilityShot
{
	AsciiString m_weaponName;	///< found when it fires (the weapons load before the objects)
	UnsignedInt m_delay;		///< frames after the first shot
	Real m_offsetX, m_offsetY;	///< from the target: ahead of the unit, to its left
};

//-------------------------------------------------------------------------------------------------
class FireWeaponAbilityUpdateModuleData : public UpdateModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;
	std::vector<FireWeaponAbilityShot> m_shots;
	Real m_startAbilityRange;		///< closes to this distance of the target first (0: fires from where it is)
	UnsignedInt m_unpackTime;		///< frames
	UnsignedInt m_preparationTime;	///< frames
	UnsignedInt m_packTime;			///< frames
	Bool m_faceTarget;

	FireWeaponAbilityUpdateModuleData()
	{
		m_specialPowerTemplate = nullptr;
		m_startAbilityRange = 0.0f;
		m_unpackTime = m_preparationTime = m_packTime = 0;
		m_faceTarget = FALSE;
	}

	static void parseShot( INI *ini, void *instance, void *store, const void *userData );

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpdateModuleData::buildFieldParse( p );
		static const FieldParse dataFieldParse[] =
		{
			{ "SpecialPowerTemplate",	INI::parseSpecialPowerTemplate,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_specialPowerTemplate ) },
			{ "Shot",					parseShot,						nullptr, 0 },
			{ "StartAbilityRange",		INI::parseReal,					nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_startAbilityRange ) },
			{ "UnpackTime",				INI::parseDurationUnsignedInt,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_unpackTime ) },
			{ "PreparationTime",		INI::parseDurationUnsignedInt,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_preparationTime ) },
			{ "PackTime",				INI::parseDurationUnsignedInt,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_packTime ) },
			{ "FaceTarget",				INI::parseBool,					nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_faceTarget ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add( dataFieldParse );
	}
};

//-------------------------------------------------------------------------------------------------
class FireWeaponAbilityUpdate : public SpecialPowerUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( FireWeaponAbilityUpdate, "FireWeaponAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( FireWeaponAbilityUpdate, FireWeaponAbilityUpdateModuleData )

public:

	FireWeaponAbilityUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	// SpecialPowerUpdateInterface
	virtual Bool initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions ) override;
	// (not a SpecialAbilityUpdate: Object::findSpecialAbilityUpdate takes any special ability for one)
	virtual Bool isSpecialAbility() const override { return false; }
	virtual Bool isSpecialPower() const override { return false; }
	virtual Bool isActive() const override { return m_stage != STAGE_IDLE; }
	virtual Bool doesSpecialPowerHaveOverridableDestinationActive() const override { return false; }
	virtual Bool doesSpecialPowerHaveOverridableDestination() const override { return false; }
	virtual void setSpecialPowerOverridableDestination( const Coord3D *loc ) override {}
	virtual SpecialPowerUpdateInterface* getSpecialPowerUpdateInterface() override { return this; }
	virtual CommandOption getCommandOption() const override { return (CommandOption)0; }
	virtual Bool isPowerCurrentlyInUse( const CommandButton *command = nullptr ) const override { return m_stage != STAGE_IDLE; }

	virtual UpdateSleepTime update() override;

private:

	enum Stage CPP_11(: Int)
	{
		STAGE_IDLE = 0,
		STAGE_APPROACH,
		STAGE_UNPACK,
		STAGE_PREPARE,
		STAGE_FIRE,
		STAGE_PACK,
	};

	Bool targetPosition( Coord3D *pos ) const;	///< where the target is now (false: it is gone)
	void enterStage( Stage stage );
	void finish();

	Stage m_stage;
	UnsignedInt m_stageStartFrame;
	ObjectID m_targetID;
	Coord3D m_targetPos;
	Bool m_hasTarget;		///< an object or a spot (else the unit's own position)
	Int m_nextShot;
	Bool m_moveIssued;

};
