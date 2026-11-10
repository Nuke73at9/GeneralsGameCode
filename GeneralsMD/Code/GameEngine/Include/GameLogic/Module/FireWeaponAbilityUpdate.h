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
// Tiberium Project @feature a beam (SAGE's BeamSpecialAbilityUpdate): BeamWeapon fired at the target object every
// BeamInterval while the target stands, within AbilityAbortRange (0: any distance), the unit told nothing else, for
// BeamTime (0: for as long). Kane's Wrath's Beam Cannon charges an Obelisk of Light so (its weapon's attribute
// modifier, for a while each time: the weapon's AttributeModifierDuration). With BeamReflector the target must stand
// within BeamReflectorReach of one of the unit's side's of those (Kane's Wrath's Venom): BeamReflectorWeapon is fired
// at the reflector and BeamWeapon from it at the target (the Beam Cannon's reflector attack).
//
// Tiberium Project @feature a shot may end with FX:<FXList>, played at the unit as the shot is fired (SAGE's FXListSelf: C&C4's
// Nod aircraft spraying its purification); ActiveConditions are shown from the unpacking to the end (SAGE's
// ActiveModelCondition).
//
// Tiberium Project @feature a teleport (SAGE's TeleportSpecialAbilityUpdate: Kane's Wrath's Firehawk Stratofighter): with
// TeleportHeight > 0 the unit climbs that high above the ground as it unpacks (held, firing nothing), is gone for
// TeleportTime (not drawn, nothing can see or target it), and comes back at the target spot at that height, its
// locomotor taking it on from there. TeleportMaxDistance (0: any) limits how far.
//
// Tiberium Project @feature aimed at an object, the ObjectShot lines are fired instead of the Shot ones, from
// ObjectStartAbilityRange (0: StartAbilityRange): SAGE's SpecialPowerDispatchSpecialPower handing a power on by what it is
// aimed at (Kane's Wrath's Orca drops its sensor pod on the ground, or fires one that sticks to a unit).
//
// Tiberium Project @feature an approach (C&C4's weapon that hands its attack on to an ability with SpecialPowerNugget: the
// Zone Raider's satchel charge, with the Assault Jump a leap to the building first): with ApproachVictimKindOf, while the
// unit attacks an object of those kinds beyond its weapon's reach and within StartAbilityRange (and no nearer than
// ApproachMinRange: SAGE's MinCastRange), any player's unit, the power is used at it by itself when it is ready (its
// upgrade, its recharge), and the attack goes on after.
//
// Tiberium Project @feature taking a vehicle over (Kane's Wrath's Avatar commandeering a Beam Cannon, a Flame
// Tank, an Attack Bike or a Stealth Tank): with TargetUpgrade = <object> <upgrade> (any number), only the
// player's own objects of those kinds are taken (and not one whose upgrade the unit has already); on reaching it
// the unit holds it still, and as it fires (its shots: the one that destroys it) it gets that object's upgrade.
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

#include "GameLogic/AttributeModifier.h"
#include "GameLogic/Module/SpecialPowerUpdateModule.h"

//-------------------------------------------------------------------------------------------------
struct FireWeaponAbilityShot
{
	AsciiString m_weaponName;	///< found when it fires (the weapons load before the objects)
	UnsignedInt m_delay;		///< frames after the first shot
	Real m_offsetX, m_offsetY;	///< from the target: ahead of the unit, to its left
	AsciiString m_fxName;		///< Tiberium Project: played at the unit as it fires (FX:<FXList>, SAGE's FXListSelf)
};

//-------------------------------------------------------------------------------------------------
struct FireWeaponAbilityTargetUpgrade
{
	AsciiString m_thingName;	///< an object of this kind is taken over...
	AsciiString m_upgradeName;	///< ... and the unit gets this
};

//-------------------------------------------------------------------------------------------------
class FireWeaponAbilityUpdateModuleData : public UpdateModuleData
{
public:
	SpecialPowerTemplate *m_specialPowerTemplate;
	std::vector<FireWeaponAbilityShot> m_shots;
	std::vector<FireWeaponAbilityShot> m_objectShots;	///< Tiberium Project: aimed at an object, these instead (ObjectShot)
	Real m_objectStartAbilityRange;	///< Tiberium Project: ... from this far (0: StartAbilityRange)
	std::vector<FireWeaponAbilityTargetUpgrade> m_targetUpgrades;	///< Tiberium Project: the objects it takes over
	ModelConditionFlags m_activeConditions;	///< Tiberium Project: shown while it is at work
	Real m_teleportHeight;					///< Tiberium Project: above the ground (0: no teleport)
	UnsignedInt m_teleportTime;			///< frames gone
	Real m_teleportMaxDistance;			///< 0: any
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
	NameKeyType m_selfModifier;		///< Tiberium Project: put on the unit itself as it fires (C&C4's SpecialPower AttributeModifier with AttributeModifierAffectsSelf)
	KindOfMaskType m_approachKindOf;	///< Tiberium Project: used by itself on the way to what the unit attacks of these kinds (ApproachVictimKindOf)
	Real m_approachMinRange;					///< Tiberium Project: ... no nearer to it than this (ApproachMinRange)
	AudioEventRTS m_unpackSound;
	AudioEventRTS m_triggerSound;
	Int m_whichSpecialWeapon;				///< Tiberium Project: SPECIAL_WEAPON_ONE (1) to THREE (3) while it is under way (0: none)
	// Tiberium Project @feature a beam
	AsciiString m_beamWeaponName;
	UnsignedInt m_beamTime;					///< frames (0: until the unit is told otherwise)
	UnsignedInt m_beamInterval;			///< frames between its shots
	Real m_abortRange;							///< 0: none
	std::vector<AsciiString> m_reflectorNames;
	Real m_reflectorReach;
	AsciiString m_reflectorWeaponName;

	FireWeaponAbilityUpdateModuleData()
	{
		m_whichSpecialWeapon = 0;
		m_teleportHeight = 0.0f;
		m_teleportTime = 0;
		m_teleportMaxDistance = 0.0f;
		m_beamTime = 0;
		m_beamInterval = LOGICFRAMES_PER_SECOND / 4;
		m_abortRange = 0.0f;
		m_reflectorReach = 0.0f;
		m_specialPowerTemplate = nullptr;
		m_startAbilityRange = 0.0f;
		m_objectStartAbilityRange = 0.0f;
		m_unpackTime = m_preparationTime = m_packTime = 0;
		m_faceTarget = FALSE;
		m_leapSpeed = 0.0f;
		m_leapFirstHeight = m_leapSecondHeight = 0.0f;
		m_leapFirstIndent = 0.33f;
		m_leapSecondIndent = 0.66f;
		m_leapLandingTime = 0;
		m_leapLandingFX = nullptr;
		m_leapPackInFlight = FALSE;
		m_selfModifier = NAMEKEY_INVALID;
		m_approachMinRange = 0.0f;	// Tiberium Project
	}

	static void parseShot( INI *ini, void *instance, void *store, const void *userData );
	static void parseTargetUpgrade( INI *ini, void *instance, void *store, const void *userData );	///< Tiberium Project
	static void parseObjectShot( INI *ini, void *instance, void *store, const void *userData );	///< Tiberium Project

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpdateModuleData::buildFieldParse( p );
		static const FieldParse dataFieldParse[] =
		{
			{ "SpecialPowerTemplate",	INI::parseSpecialPowerTemplate,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_specialPowerTemplate ) },
			{ "Shot",					parseShot,						nullptr, 0 },
			{ "ObjectShot",				parseObjectShot,				nullptr, 0 },	// Tiberium Project
			{ "ObjectStartAbilityRange",	INI::parseReal,				nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_objectStartAbilityRange ) },	// Tiberium Project
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
			{ "BeamWeapon",				INI::parseAsciiString,			nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_beamWeaponName ) },	// Tiberium Project
			{ "BeamTime",				INI::parseDurationUnsignedInt,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_beamTime ) },
			{ "BeamInterval",			INI::parseDurationUnsignedInt,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_beamInterval ) },
			{ "AbilityAbortRange",		INI::parseReal,					nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_abortRange ) },
			{ "BeamReflector",			INI::parseAsciiStringVectorAppend,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_reflectorNames ) },
			{ "BeamReflectorReach",		INI::parseReal,					nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_reflectorReach ) },
			{ "BeamReflectorWeapon",		INI::parseAsciiString,			nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_reflectorWeaponName ) },
			{ "TargetUpgrade",			parseTargetUpgrade,				nullptr, 0 },	// Tiberium Project
			{ "ActiveConditions",		ModelConditionFlags::parseFromINI,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_activeConditions ) },
			{ "TeleportHeight",			INI::parseReal,					nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_teleportHeight ) },
			{ "TeleportTime",				INI::parseDurationUnsignedInt,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_teleportTime ) },
			{ "TeleportMaxDistance",		INI::parseReal,					nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_teleportMaxDistance ) },
			// Tiberium Project @feature a modifier on the unit itself as it fires (C&C4's Flame Tank's Trail of Flame: faster, its trail lit)
			{ "SelfModifier",				AttributeModifierStore::parseModifierName,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_selfModifier ) },
			// Tiberium Project @feature used by itself on the way to what the unit attacks (C&C4's Zone Raider's jump to a building)
			{ "ApproachVictimKindOf",	KindOfMaskType::parseFromINI,	nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_approachKindOf ) },
			{ "ApproachMinRange",			INI::parseReal,					nullptr, offsetof( FireWeaponAbilityUpdateModuleData, m_approachMinRange ) },
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
	// Tiberium Project @feature an object it cannot take over (TargetUpgrade)
	virtual Bool refusesSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos ) const override;

	virtual UpdateSleepTime update() override;
	UpdateSleepTime updateStages();	///< Tiberium Project: the ability under way (update: and the approach's look while idle)
	void tryApproach();						///< Tiberium Project: ApproachVictimKindOf, the power used on the way to what the unit attacks
	Object *findReflector( const Object *target ) const;	///< Tiberium Project: the beam's reflector nearest the target
	const FireWeaponAbilityTargetUpgrade *targetUpgradeFor( const Object *target ) const;	///< Tiberium Project: or nullptr
	// Tiberium Project: in the air the unit is held (and may be disabled on the way), and still flies on
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return m_stage == STAGE_LEAP || m_stage == STAGE_TELEPORT || isTeleportClimb() ? DISABLEDMASK_ALL : DISABLEDMASK_NONE; }

private:
	const std::vector<FireWeaponAbilityShot>& currentShots() const;	///< Tiberium Project: ObjectShot at an object, else Shot
	Real currentStartRange() const;	///< Tiberium Project: its reach for them

	enum Stage CPP_11(: Int)
	{
		STAGE_IDLE = 0,
		STAGE_APPROACH,
		STAGE_UNPACK,
		STAGE_PREPARE,
		STAGE_FIRE,
		STAGE_PACK,
		STAGE_LEAP,		///< Tiberium Project: in the air, to the target spot
		STAGE_TELEPORT,	///< Tiberium Project: gone, to come back at the target spot
		STAGE_TAKEOFF,	///< Tiberium Project: an aircraft off its pad first (a teleport climbs from the air)
	};

	Bool targetPosition( Coord3D *pos ) const;	///< where the target is now (false: it is gone)
	void enterStage( Stage stage );
	void finish();
	void startLeap();						///< Tiberium Project: the arc to the target spot, and off
	void endLeap();							///< Tiberium Project: not held, nor in the air, any more
	void playSound( const AudioEventRTS &sound ) const;	///< Tiberium Project
	void setLeapLook( Int look );			///< Tiberium Project: 0 none, 1 in flight, 2 landing, 3 packing in flight
	Bool isTeleportClimb() const;		///< Tiberium Project: climbing to go (a teleport's unpacking)
	void setGone( Bool gone );			///< Tiberium Project: not drawn, seen or targeted while teleporting
	void endTeleport();							///< Tiberium Project: back, at the target spot
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
	ObjectID m_resumeVictimID;	///< Tiberium Project: what the unit attacked when the approach took it (its attack goes on after)

};
