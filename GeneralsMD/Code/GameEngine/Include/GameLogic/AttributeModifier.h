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

// FILE: AttributeModifier.h //////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's attribute modifiers (C&C4's buffs and debuffs): a named set
// of multipliers an object carries for a while (or until it is taken off) from a weapon's hit, an
// ability, a toggled state or an aura. Speed multiplies its locomotor's speed, Armor the damage it
// takes, Damage the damage its weapons deal, RateOfFire how fast they fire, Range how far they reach.
// Health its maximum health (keeping its share), Vision and ShroudClearing how far it sees, Recharge
// its own abilities' recharge times, Production how fast it builds, ClipReload, PreAttack and Area its
// weapons' clip reloads, pre-attack delays and blast radii. Dodge, Reflect, Hit and ShieldReact add
// to its chances (summed): to make a shot at it miss, to send a shot that hits it back to the firer,
// for its own shots to hit, for its shield sphere to take a hit. ResistEMP keeps EMP off it (and
// lifts it); Status sets object statuses while it lasts (SAGE's ObjectStatusToSet); with KillOnExpire
// the object dies as it runs out (a decoy army's copies: SAGE's ClonedObjectLifetime).
// Buff classes a modifier for purges (C&C4's BuffInfo); ModelConditions are shown while it lasts.
// The same modifier from the same source refreshes; from another source it stacks up to
// StackingLimit times.
//
//   AttributeModifier C4AM_Modifier_GDIAircraftC_ArmorDebuff
//     Duration        = 2500         ; 0: until taken off (a toggled state's, an aura's own)
//     Armor           = 125%
//     Dodge           = -25%         ; a chance, added to the others in force
//     ResistEMP       = Yes
//     Status          = IMMOBILE
//     Buff            = DEFENSE_NEGATIVE
//     ModelConditions = USER_1
//     StackingLimit   = 1
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/GameCommon.h"
#include "Common/ModelState.h"
#include "Common/ObjectStatusTypes.h"
#include "Common/NameKeyGenerator.h"
#include "Common/STLTypedefs.h"
#include "Common/SubsystemInterface.h"

class INI;
class FXList;
struct FieldParse;

/// what a modifier is, for purges (C&C4's BuffInfo Type)
enum AttributeModifierBuff CPP_11(: Int)
{
	MODIFIER_BUFF_NONE = 0,
	MODIFIER_BUFF_OFFENSE_POSITIVE,
	MODIFIER_BUFF_DEFENSE_POSITIVE,
	MODIFIER_BUFF_OFFENSE_NEGATIVE,
	MODIFIER_BUFF_DEFENSE_NEGATIVE,
	MODIFIER_BUFF_COUNT
};
extern const char *const TheAttributeModifierBuffNames[];	///< for INI: NONE, OFFENSE_POSITIVE, ... (a list as a bit mask: 1 << buff)

/// what a modifier multiplies
enum AttributeModifierAttribute CPP_11(: Int)
{
	MODIFIER_SPEED = 0,				///< its locomotor's speed
	MODIFIER_ARMOR,						///< the damage it takes
	MODIFIER_DAMAGE,					///< the damage its weapons deal
	MODIFIER_RATE_OF_FIRE,		///< how fast its weapons fire
	MODIFIER_RANGE,						///< how far its weapons reach
	MODIFIER_HEALTH,					///< its maximum health (its health keeps its share): SAGE's HEALTH_MULT
	MODIFIER_VISION,					///< how far it sees: VISION
	MODIFIER_SHROUD_CLEARING,	///< how far it clears the shroud: SHROUD_CLEARING
	MODIFIER_RECHARGE,				///< its powers' and abilities' recharge times: RECHARGE_TIME
	MODIFIER_PRODUCTION,			///< how fast it builds: PRODUCTION
	MODIFIER_CLIP_RELOAD,			///< its weapons' clip reload times: CLIP_RELOAD_TIME
	MODIFIER_PRE_ATTACK,			///< its weapons' pre-attack delays: PREATTACK_DELAY
	MODIFIER_AREA,						///< its weapons' blast radii: AREA_OF_EFFECT
	MODIFIER_ATTRIBUTE_COUNT
};

/// what a modifier adds to (chances, summed over the modifiers in force)
enum AttributeModifierChance CPP_11(: Int)
{
	MODIFIER_CHANCE_DODGE = 0,		///< a shot at it misses: SAGE's DODGE_PERCENTAGE_ADD (less than none: a debuff on a dodger)
	MODIFIER_CHANCE_REFLECT,			///< a shot that hits it goes back to its firer: REFLECTION_CHANCE (C&C4's Mirror Coat)
	MODIFIER_CHANCE_HIT,					///< its own shots hit (less than none: they miss): HIT_PERCENTAGE_ADD
	MODIFIER_CHANCE_SHIELD_REACT,	///< its shield sphere takes a hit: SPHERE_MODULE_REACT_CHANCE (C&C4's Mirror Plating)
	MODIFIER_CHANCE_COUNT
};

//-------------------------------------------------------------------------------------------------
class AttributeModifierTemplate
{
public:
	AttributeModifierTemplate();

	const AsciiString &getName() const { return m_name; }
	NameKeyType getNameKey() const { return m_nameKey; }
	UnsignedInt getDuration() const { return m_duration; }
	Real getScalar( AttributeModifierAttribute a ) const { return m_scalar[a]; }
	Real getChance( AttributeModifierChance c ) const { return m_chance[c]; }
	Bool isResistingEMP() const { return m_resistEMP; }
	Bool isKillOnExpire() const { return m_killOnExpire; }
	const ObjectStatusMaskType &getStatus() const { return m_status; }
	AttributeModifierBuff getBuff() const { return m_buff; }
	const ModelConditionFlags &getModelConditions() const { return m_modelConditions; }
	Int getStackingLimit() const { return m_stackingLimit; }
	const FXList *getStartFX() const;

	static const FieldParse *getFieldParse();

	AsciiString m_name;
	NameKeyType m_nameKey;
	UnsignedInt m_duration;									///< frames; 0: until taken off
	Real m_scalar[MODIFIER_ATTRIBUTE_COUNT];
	Real m_chance[MODIFIER_CHANCE_COUNT];		///< added to the object's chances
	Bool m_resistEMP;												///< it is not disabled by EMP meanwhile: RESIST_EMP
	Bool m_killOnExpire;										///< the object dies as it runs out (a decoy's lifetime)
	ObjectStatusMaskType m_status;					///< set on the object while it lasts: SAGE's ObjectStatusToSet
	AttributeModifierBuff m_buff;
	ModelConditionFlags m_modelConditions;	///< shown while it lasts
	Int m_stackingLimit;										///< from how many sources at once
	AsciiString m_startFXName;							///< played on the object when it is put on
};

//-------------------------------------------------------------------------------------------------
class AttributeModifierStore : public SubsystemInterface
{
public:
	AttributeModifierStore();
	virtual ~AttributeModifierStore() override;

	virtual void init() override { }
	virtual void reset() override { }
	virtual void update() override { }

	const AttributeModifierTemplate *find( NameKeyType key ) const;
	const AttributeModifierTemplate *find( const AsciiString &name ) const;

	static void parseAttributeModifierDefinition( INI *ini );
	/// INI field: a modifier's name, stored as its NameKeyType (looked up when used: the modifiers
	/// may be read after what names them)
	static void parseModifierName( INI *ini, void *instance, void *store, const void *userData );

private:
	typedef std::map< NameKeyType, AttributeModifierTemplate > TemplateMap;	// ordered: the same on every machine
	TemplateMap m_templates;
};

extern AttributeModifierStore *TheAttributeModifierStore;

class Object;
/// whether what an object does to others (a power's, an aura's modifier) reaches obj: affects is
/// WeaponAffectsMaskType's SELF, ALLIES, ENEMIES and NEUTRALS
Bool attributeModifierReaches( const Object *source, const Object *obj, UnsignedInt affects );
