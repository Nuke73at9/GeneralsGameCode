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

// FILE: StickyBombUpdate.h ////////////////////////////////////////////////////////////////////////
// Author: Kris Morness, July 2002
// Desc:   Similar to ParachuteContain, this module is used essentially to attach a bomb to an object
//         moving around. The sticky bomb position simply updates to the specified bone until it explodes.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/ObjectStatusTypes.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/UpdateModule.h"

#include <vector>

class WeaponTemplate;
class FXList;

//-------------------------------------------------------------------------------------------------
class StickyBombUpdateModuleData : public UpdateModuleData
{
public:
	AsciiString			m_attachToBone;
	Real						m_offsetZ;
	WeaponTemplate*	m_geometryBasedDamageWeaponTemplate;
	FXList*					m_geometryBasedDamageFX;
	Bool						m_keepProducer;		///< Tiberium Project: the one who made it stays its producer (not the target)
	Bool						m_dieWithTarget;	///< Tiberium Project: dies (its death modules run) when the target dies, not just vanishes
	ObjectStatusMaskType	m_targetStatus;	///< Tiberium Project: what the target is while it is stuck there (C&C4's ParentStatus)
	Bool						m_absorbTargetDamage;	///< Tiberium Project: the target's hits on it instead (C&C4's LEECH_DAMAGE_FROM_PARENT, ABSORB_ALL_DAMAGE)
	DamageTypeFlags	m_absorbDamageTypes;	///< Tiberium Project: the kinds of hit it takes for the target (C&C4's all but DamageTypesToNotLeech)
	Bool						m_targetShieldTint;	///< Tiberium Project: the target drawn shielded while it is there (C&C4's PERSONAL_SHIELDS look)
	ObjectStatusMaskType	m_targetForbiddenStatus;	///< Tiberium Project: it dies once its target has any of these (C&C4's ForbiddenParentStatus)
	ObjectStatusMaskType	m_targetRequiredStatus;	///< Tiberium Project: it dies once its target lacks any of these (a forbidden status of a toggle's first state)
	NameKeyType			m_targetModifier;	///< Tiberium Project: an attribute modifier on its target while it is there (C&C4's ParentModifierToSet)
	Bool						m_onePerTarget;	///< Tiberium Project: one of its kind on a target: those already there die (C&C4's DETACH_WHEN_MULTIPLE_ARE_ATTACHED)

	StickyBombUpdateModuleData()
	{
		m_targetForbiddenStatus.clear();
		m_targetRequiredStatus.clear();
		m_targetModifier = NAMEKEY_INVALID;
		m_onePerTarget = FALSE;
		m_offsetZ = 10.0f;
		m_geometryBasedDamageWeaponTemplate = nullptr;
		m_geometryBasedDamageFX = nullptr;
		m_keepProducer = FALSE;
		m_dieWithTarget = FALSE;
		m_targetStatus.clear();
		m_absorbTargetDamage = FALSE;
		m_absorbDamageTypes = DAMAGE_TYPE_FLAGS_ALL;
		m_targetShieldTint = FALSE;
	}

	static void buildFieldParse(MultiIniFieldParse& p)
	{
    UpdateModuleData::buildFieldParse(p);
		static const FieldParse dataFieldParse[] =
		{
			{ "AttachToTargetBone",				INI::parseAsciiString,		nullptr, offsetof( StickyBombUpdateModuleData, m_attachToBone ) },
			{ "OffsetZ",									INI::parseReal,						nullptr, offsetof( StickyBombUpdateModuleData, m_offsetZ ) },
			{ "GeometryBasedDamageWeapon",INI::parseWeaponTemplate, nullptr, offsetof( StickyBombUpdateModuleData, m_geometryBasedDamageWeaponTemplate ) },
			{ "GeometryBasedDamageFX",		INI::parseFXList,					nullptr, offsetof( StickyBombUpdateModuleData, m_geometryBasedDamageFX ) },
			// Tiberium Project @feature a charge stuck to its target by a weapon (C&C4's AttachToTarget): the
			// one who fired it stays its producer, so its damage spares him and not the target; and it dies
			// with the target (its death weapons and effects play) instead of vanishing
			{ "KeepProducer",							INI::parseBool,						nullptr, offsetof( StickyBombUpdateModuleData, m_keepProducer ) },
			{ "DieWithTarget",						INI::parseBool,						nullptr, offsetof( StickyBombUpdateModuleData, m_dieWithTarget ) },
			// Tiberium Project @feature SAGE's AttachUpdate shield (C&C4's Personal Shields): while it is stuck there the target
			// has TargetStatus, and with AbsorbTargetDamage it takes the target's hits of AbsorbDamageTypes on itself (by its own
			// armour), each whole, the target none of them, until it dies of them; it takes no hit but those
			{ "TargetStatus",							ObjectStatusMaskType::parseFromINI,	nullptr, offsetof( StickyBombUpdateModuleData, m_targetStatus ) },
			{ "AbsorbTargetDamage",				INI::parseBool,						nullptr, offsetof( StickyBombUpdateModuleData, m_absorbTargetDamage ) },
			{ "AbsorbDamageTypes",				INI::parseDamageTypeFlags,	nullptr, offsetof( StickyBombUpdateModuleData, m_absorbDamageTypes ) },
			{ "TargetShieldTint",					INI::parseBool,						nullptr, offsetof( StickyBombUpdateModuleData, m_targetShieldTint ) },
			// Tiberium Project @feature SAGE's AttachUpdate on what it is stuck to (C&C4's Aftershock's Tiberium cloud): it dies
			// once its target has any of TargetForbiddenStatus (DIE_WHEN_PARENT_HAS_FORBIDDEN_STATUS),
			// and its target has TargetModifier while it is there (ParentModifierToSet), taken off as it goes
			{ "TargetForbiddenStatus",		ObjectStatusMaskType::parseFromINI,	nullptr, offsetof( StickyBombUpdateModuleData, m_targetForbiddenStatus ) },
			// (and once it lacks any of TargetRequiredStatus: C&C4's ForbiddenParentStatus of the state its toggle starts in,
			// the Aftershock's cloud gone as the Aftershock packs up, out of DEPLOYED)
			{ "TargetRequiredStatus",			ObjectStatusMaskType::parseFromINI,	nullptr, offsetof( StickyBombUpdateModuleData, m_targetRequiredStatus ) },
			{ "TargetModifier",						AttributeModifierStore::parseModifierName,	nullptr, offsetof( StickyBombUpdateModuleData, m_targetModifier ) },
			// Tiberium Project @feature one of its kind on a target at a time: as it sticks there, those of its kind already there
			// die, a death not of old age (C&C4's Shockwave: a second sonic charge on a target sets the first one's bonus blast off)
			{ "OnePerTarget",							INI::parseBool,						nullptr, offsetof( StickyBombUpdateModuleData, m_onePerTarget ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
    p.add(dataFieldParse);
	}
};

//-------------------------------------------------------------------------------------------------
class StickyBombUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( StickyBombUpdate, "StickyBombUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( StickyBombUpdate, StickyBombUpdateModuleData )

public:

	StickyBombUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual void onObjectCreated() override;
	virtual void onDelete() override;

	virtual UpdateSleepTime update() override;							///< called once per frame

	void initStickyBomb( Object *object, const Object *bomber, const Coord3D *specificPos = nullptr );
	void detonate();
	Bool isTimedBomb() const { return m_dieFrame > 0; }
	UnsignedInt getDetonationFrame() const { return m_dieFrame; }
	Object* getTargetObject() const;
	void setTargetObject( Object *obj );

	/** Tiberium Project @feature whether the hit on the victim is dealt with here: taken by a shield stuck to it
		(AbsorbTargetDamage), or the victim is such a shield, which takes only the hits it takes for its target (from ActiveBody) */
	static Bool absorbDamage( Object *victim, const DamageInfo *damageInfo );

private:

	void markTarget( Bool on );	///< Tiberium Project: the target's TargetStatus and look, set or cleared

	ObjectID			m_targetID;
	UnsignedInt		m_dieFrame;
	UnsignedInt   m_nextPingFrame;
};
