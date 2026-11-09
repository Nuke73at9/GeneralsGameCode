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
// Tiberium Project @feature a leap: C&C4's jump jets, a weapon that fires the unit itself (ProjectileSelf).
// With LeapSpeed > 0 the unit flies to the target spot instead of firing its shots, on an arc whose two
// inner points stand LeapFirstHeight and LeapSecondHeight above the highest ground on the way, at
// LeapFirstPercentIndent and LeapSecondPercentIndent along it (as a DumbProjectileBehavior's), with
// LeapFlightConditions while it is in the air and LeapLandingConditions for its last LeapLandingTime;
// UNPACKING (UnpackTime) is its takeoff and PACKING (PackTime) its landing, LeapLandingFX where it lands.
// With LeapPackInFlight it packs as it takes off instead (Kane's Wrath's jump jets: PACKING is the jump),
// for PackTime of its flight. UnpackSound plays as the unit unpacks, TriggerSound as it fires (or takes off).
// WhichSpecialWeapon (1 to 3): SAGE's SPECIAL_WEAPON_ONE to THREE while the ability is under way (Kane's
// Wrath's Temple of Nod launches its missile in SPECIAL_WEAPON_ONE and UNPACKING).
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

#include "Common/AudioEventRTS.h"
#include "Common/INI.h"
#include "Common/ModelState.h"
#include "GameLogic/Module/UpdateModule.h"

class CommandButton;
class FXList;
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
	// Tiberium Project @feature the leap
	Real m_leapSpeed;					///< distance per frame (0: no leap, the shots are fired)
	Real m_leapFirstHeight;
	Real m_leapSecondHeight;
	Real m_leapFirstIndent;
	Real m_leapSecondIndent;
	UnsignedInt m_leapLandingTime;		///< frames before it lands
	ModelConditionFlags m_leapFlightConditions;
	ModelConditionFlags m_leapLandingConditions;
	const FXList *m_leapLandingFX;
	Bool m_leapPackInFlight;
	AudioEventRTS m_unpackSound;
	AudioEventRTS m_triggerSound;
	Int m_whichSpecialWeapon;				///< Tiberium Project: SPECIAL_WEAPON_ONE (1) to THREE (3) while it is under way (0: none)

	FireWeaponAbilityUpdateModuleData()
	{
		m_whichSpecialWeapon = 0;
		m_specialPowerTemplate = nullptr;
		m_startAbilityRange = 0.0f;
		m_unpackTime = m_preparationTime = m_packTime = 0;
		m_faceTarget = FALSE;
		m_leapSpeed = 0.0f;
		m_leapFirstHeight = m_leapSecondHeight = 0.0f;
		m_leapFirstIndent = 0.33f;
		m_leapSecondIndent = 0.66f;
		m_leapLandingTime = 0;
		m_leapLandingFX = nullptr;
		m_leapPackInFlight = FALSE;
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
			{ "LeapSpeed",				INI::parseVelocityReal,			nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_leapSpeed ) },
			{ "LeapFirstHeight",			INI::parseReal,					nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_leapFirstHeight ) },
			{ "LeapSecondHeight",			INI::parseReal,					nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_leapSecondHeight ) },
			{ "LeapFirstPercentIndent",	INI::parsePercentToReal,		nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_leapFirstIndent ) },
			{ "LeapSecondPercentIndent",	INI::parsePercentToReal,		nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_leapSecondIndent ) },
			{ "LeapLandingTime",			INI::parseDurationUnsignedInt,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_leapLandingTime ) },
			{ "LeapFlightConditions",		ModelConditionFlags::parseFromINI,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_leapFlightConditions ) },
			{ "LeapLandingConditions",	ModelConditionFlags::parseFromINI,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_leapLandingConditions ) },
			{ "LeapLandingFX",			INI::parseFXList,				nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_leapLandingFX ) },
			{ "LeapPackInFlight",			INI::parseBool,					nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_leapPackInFlight ) },
			{ "UnpackSound",				INI::parseAudioEventRTS,		nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_unpackSound ) },
			{ "TriggerSound",			INI::parseAudioEventRTS,		nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_triggerSound ) },
			{ "WhichSpecialWeapon",		INI::parseInt,					nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_whichSpecialWeapon ) },	// Tiberium Project
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
	// Tiberium Project: in the air the unit is held (and may be disabled on the way), and still flies on
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return m_stage == STAGE_LEAP ? DISABLEDMASK_ALL : DISABLEDMASK_NONE; }

private:

	enum Stage CPP_11(: Int)
	{
		STAGE_IDLE = 0,
		STAGE_APPROACH,
		STAGE_UNPACK,
		STAGE_PREPARE,
		STAGE_FIRE,
		STAGE_PACK,
		STAGE_LEAP,		///< Tiberium Project: in the air, to the target spot
	};

	Bool targetPosition( Coord3D *pos ) const;	///< where the target is now (false: it is gone)
	void enterStage( Stage stage );
	void finish();
	void startLeap();						///< Tiberium Project: the arc to the target spot, and off
	void endLeap();							///< Tiberium Project: not held, nor in the air, any more
	void playSound( const AudioEventRTS &sound ) const;	///< Tiberium Project
	void setLeapLook( Int look );			///< Tiberium Project: 0 none, 1 in flight, 2 landing, 3 packing in flight
	ModelConditionFlags leapLook( Int look ) const;

	Stage m_stage;
	UnsignedInt m_stageStartFrame;
	ObjectID m_targetID;
	Coord3D m_targetPos;
	Bool m_hasTarget;		///< an object or a spot (else the unit's own position)
	Int m_nextShot;
	Bool m_moveIssued;
	// Tiberium Project @feature the leap: its arc, how far along it the unit is, and how it looks
	Coord3D m_leapPoints[4];
	Int m_leapSegments;
	Int m_leapSegment;
	Int m_leapLook;
	Bool m_leapNoAttack;	///< it set NO_ATTACK for the flight (so clears it on landing)

};
