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

// FILE: FireWeaponUpdate.h /////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, August 2002
// Desc:   Update fires a weapon at its own feet as quickly as the weapon allows
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Weapon.h"
#include "Common/ObjectStatusTypes.h"

//-------------------------------------------------------------------------------------------------
class FireWeaponUpdateModuleData : public UpdateModuleData
{
public:
	const WeaponTemplate* m_weaponTemplate;
  UnsignedInt m_initialDelayFrames;
	UnsignedInt m_exclusiveWeaponDelay;	///< If non-zero, any other weapon having fired this recently will keep us from doing anything
	ObjectStatusMaskType m_requiredStatus;	///< Tiberium Project @feature fires only with all of these (SAGE 2's ObjectStatusValidation)
	ObjectStatusMaskType m_forbiddenStatus;	///< Tiberium Project: and none of these
	/// Tiberium Project @feature fires only with another object this near (SAGE 2's RequiredObject: a fire field
	/// burns while something stands in it, a barrel mine goes off as an enemy comes): of these relationships to
	/// it (RequiredObjectRelationship: ALLIES ENEMIES NEUTRALS; none named: any), of any of these kinds and none
	/// of those (RequiredObjectKindOf, RequiredObjectForbiddenKindOf)
	Real m_requiredObjectRange;
	UnsignedInt m_requiredObjectRelationship;
	KindOfMaskType m_requiredObjectKindOf;
	KindOfMaskType m_requiredObjectForbiddenKindOf;
	/// Tiberium Project @feature SAGE's INITIALLY_INACTIVE: it fires only once something switches it on (arm: a toggle's
	/// ArmsTimedWeapons, C&C4's ACTIVATE_FIRE_WEAPON_UPDATE_MODULES), its InitialDelay counted from then
	Bool m_startsInactive;

	FireWeaponUpdateModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);

private:

};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class FireWeaponUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( FireWeaponUpdate, "FireWeaponUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( FireWeaponUpdate, FireWeaponUpdateModuleData )

public:

	FireWeaponUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

	/** Tiberium Project @feature switched on (StartsInactive): it fires its InitialDelay from now, its weapon ready again
		(C&C4's Thunderhead's slam as it lands) */
	void arm();
	/// Tiberium Project: every FireWeaponUpdate of the object that starts inactive, switched on
	static void armAll( Object *obj );

protected:

	Bool isOkayToFire();
	Bool hasRequiredObject() const;	///< Tiberium Project: another object near enough (RequiredObjectRange)

	Weapon* m_weapon;
  UnsignedInt m_initialDelayFrame;
	Bool m_active;	///< Tiberium Project: switched on (StartsInactive: not until armed)

};
