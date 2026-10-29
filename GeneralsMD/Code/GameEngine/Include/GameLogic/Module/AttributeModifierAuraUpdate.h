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
//
//   Behavior = AttributeModifierAuraUpdate ModuleTag_ArmorAura
//     AttributeModifier   = C4AM_Modifier_ArmorUp_Aura
//     Radius              = 150
//     Affects             = SELF ALLIES
//     RefreshDelay        = 1000
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/INI.h"
#include "Common/KindOf.h"
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

	AttributeModifierAuraUpdateModuleData()
	{
		m_modifier = NAMEKEY_INVALID;
		m_radius = 0.0f;
		m_affects = WEAPON_AFFECTS_ALLIES;
		m_refreshDelay = LOGICFRAMES_PER_SECOND;
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

};
