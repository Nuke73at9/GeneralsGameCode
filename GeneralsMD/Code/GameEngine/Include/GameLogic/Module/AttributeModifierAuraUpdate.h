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

// FILE: AttributeModifierAuraUpdate.h ////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's AttributeModifierAuraUpdate (C&C4's auras: a support unit's
// armour or dodge for the units around it, a structure's): every RefreshDelay the object puts its
// AttributeModifier on what is within Radius and Affects takes in (of the KindOf given, none of
// ForbiddenKindOf), for the modifier's Duration (twice RefreshDelay when it has none), so it wears
// off soon after they leave. Not while the object is being built or sold, or is disabled.
// Tiberium Project @feature AffectContainedOnly: only on what rides inside it (C&C4's Armadillo and
// Reckoner: their passengers fire further); Radius 0: only on itself (SELF in Affects). RequiredStatus,
// RequiredModifier: only while the object has all of those status bits, or carries that modifier itself
// (a toggled state's: C&C4's Kodiak shields its allies only while armoured up); ForbiddenStatus: none of these.
// Tiberium Project @feature AffectsNamed: only objects of these kinds (C&C4's Crawlers reload their side's bombers faster,
// SAGE's IncludeThing); RequiredUpgrade: only once the object, or its player, has that upgrade (SAGE's aura switched on
// by an upgrade: C&C4's Spectre's dodge for its neighbours, with its improved stealth).
//
//   Behavior = AttributeModifierAuraUpdate ModuleTag_ArmorAura
//     AttributeModifier   = C4AM_Modifier_ArmorUp_Aura
//     Radius              = 150
//     Affects             = SELF ALLIES
//     RefreshDelay        = 1000
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "Common/INI.h"
#include "Common/KindOf.h"
#include "Common/ObjectStatusTypes.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/UpdateModule.h"

//-------------------------------------------------------------------------------------------------
class AttributeModifierAuraUpdateModuleData : public UpdateModuleData
{
public:
	NameKeyType m_modifier;
	Real m_radius;
	UnsignedInt m_affects;							///< WeaponAffectsMaskType: SELF ALLIES ENEMIES NEUTRALS
	UnsignedInt m_refreshDelay;					///< frames
	KindOfMaskType m_kindOf;						///< any of these (none: all)
	KindOfMaskType m_forbiddenKindOf;
	Bool m_affectContainedOnly;					///< Tiberium Project @feature only its passengers
	ObjectStatusMaskType m_requiredStatus;	///< Tiberium Project @feature only while it has all of these
	ObjectStatusMaskType m_forbiddenStatus;	///< Tiberium Project @feature and none of these (C&C4's Nod structures: its plain aura
																					///< stops as its upgraded one takes over)
	NameKeyType m_requiredModifier;			///< Tiberium Project @feature only while it carries this modifier
	std::vector<AsciiString> m_affectsNamed;	///< Tiberium Project @feature only these kinds of object (none named: any)
	AsciiString m_requiredUpgrade;			///< Tiberium Project @feature only once it (or its player) has this

	AttributeModifierAuraUpdateModuleData()
	{
		m_modifier = NAMEKEY_INVALID;
		m_radius = 0.0f;
		m_affects = WEAPON_AFFECTS_ALLIES;
		m_refreshDelay = LOGICFRAMES_PER_SECOND;
		m_affectContainedOnly = FALSE;
		m_requiredModifier = NAMEKEY_INVALID;
	}

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpdateModuleData::buildFieldParse( p );
		static const FieldParse dataFieldParse[] =
		{
			{ "AttributeModifier",		AttributeModifierStore::parseModifierName,	nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_modifier ) },
			{ "Radius",								INI::parseReal,									nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_radius ) },
			{ "Affects",							INI::parseBitString32,					TheWeaponAffectsMaskNames, offsetof( AttributeModifierAuraUpdateModuleData, m_affects ) },
			{ "RefreshDelay",					INI::parseDurationUnsignedInt,	nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_refreshDelay ) },
			{ "KindOf",								KindOfMaskType::parseFromINI,		nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_kindOf ) },
			{ "ForbiddenKindOf",			KindOfMaskType::parseFromINI,		nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_forbiddenKindOf ) },
			{ "AffectContainedOnly",	INI::parseBool,									nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_affectContainedOnly ) },
			{ "RequiredStatus",				ObjectStatusMaskType::parseFromINI,	nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_requiredStatus ) },
			{ "ForbiddenStatus",			ObjectStatusMaskType::parseFromINI,	nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_forbiddenStatus ) },
			{ "RequiredModifier",			AttributeModifierStore::parseModifierName,	nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_requiredModifier ) },
			{ "AffectsNamed",					INI::parseAsciiStringVectorAppend,	nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_affectsNamed ) },
			{ "RequiredUpgrade",			INI::parseAsciiString,					nullptr, offsetof( AttributeModifierAuraUpdateModuleData, m_requiredUpgrade ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add( dataFieldParse );
	}
};

//-------------------------------------------------------------------------------------------------
class AttributeModifierAuraUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AttributeModifierAuraUpdate, "AttributeModifierAuraUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AttributeModifierAuraUpdate, AttributeModifierAuraUpdateModuleData )

public:

	AttributeModifierAuraUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

private:

	void affect( Object *other, const AttributeModifierTemplate *modifier, UnsignedInt duration );

};
