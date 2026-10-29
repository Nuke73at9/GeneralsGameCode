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

// FILE: AttributeModifier.cpp ////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's attribute modifiers (see AttributeModifier.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/INI.h"
#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

AttributeModifierStore *TheAttributeModifierStore = nullptr;

const char *const TheAttributeModifierBuffNames[] =
{
	"NONE",
	"OFFENSE_POSITIVE",
	"DEFENSE_POSITIVE",
	"OFFENSE_NEGATIVE",
	"DEFENSE_NEGATIVE",
	nullptr
};
static_assert(ARRAY_SIZE(TheAttributeModifierBuffNames) == MODIFIER_BUFF_COUNT + 1, "Incorrect array size");

//-------------------------------------------------------------------------------------------------
AttributeModifierTemplate::AttributeModifierTemplate() :
	m_nameKey( NAMEKEY_INVALID ),
	m_duration( 0 ),
	m_buff( MODIFIER_BUFF_NONE ),
	m_stackingLimit( 1 )
{
	for( Int i = 0; i < MODIFIER_ATTRIBUTE_COUNT; ++i )
		m_scalar[i] = 1.0f;
}

//-------------------------------------------------------------------------------------------------
const FXList *AttributeModifierTemplate::getStartFX() const
{
	return m_startFXName.isEmpty() ? nullptr : TheFXListStore->findFXList( m_startFXName.str() );
}

//-------------------------------------------------------------------------------------------------
const FieldParse *AttributeModifierTemplate::getFieldParse()
{
	static const FieldParse dataFieldParse[] =
	{
		{ "Duration",					INI::parseDurationUnsignedInt,	nullptr,	offsetof( AttributeModifierTemplate, m_duration ) },
		{ "Speed",						INI::parsePercentToReal,				nullptr,	offsetof( AttributeModifierTemplate, m_scalar[MODIFIER_SPEED] ) },
		{ "Armor",						INI::parsePercentToReal,				nullptr,	offsetof( AttributeModifierTemplate, m_scalar[MODIFIER_ARMOR] ) },
		{ "Damage",						INI::parsePercentToReal,				nullptr,	offsetof( AttributeModifierTemplate, m_scalar[MODIFIER_DAMAGE] ) },
		{ "RateOfFire",				INI::parsePercentToReal,				nullptr,	offsetof( AttributeModifierTemplate, m_scalar[MODIFIER_RATE_OF_FIRE] ) },
		{ "Range",						INI::parsePercentToReal,				nullptr,	offsetof( AttributeModifierTemplate, m_scalar[MODIFIER_RANGE] ) },
		{ "Buff",							INI::parseIndexList,						TheAttributeModifierBuffNames,	offsetof( AttributeModifierTemplate, m_buff ) },
		{ "ModelConditions",	ModelConditionFlags::parseFromINI,	nullptr,	offsetof( AttributeModifierTemplate, m_modelConditions ) },
		{ "StackingLimit",		INI::parseInt,									nullptr,	offsetof( AttributeModifierTemplate, m_stackingLimit ) },
		{ "StartFX",					INI::parseAsciiString,					nullptr,	offsetof( AttributeModifierTemplate, m_startFXName ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return dataFieldParse;
}

//-------------------------------------------------------------------------------------------------
AttributeModifierStore::AttributeModifierStore()
{
}

//-------------------------------------------------------------------------------------------------
AttributeModifierStore::~AttributeModifierStore()
{
	m_templates.clear();
}

//-------------------------------------------------------------------------------------------------
const AttributeModifierTemplate *AttributeModifierStore::find( NameKeyType key ) const
{
	TemplateMap::const_iterator it = m_templates.find( key );
	return it != m_templates.end() ? &it->second : nullptr;
}

//-------------------------------------------------------------------------------------------------
const AttributeModifierTemplate *AttributeModifierStore::find( const AsciiString &name ) const
{
	return name.isEmpty() ? nullptr : find( TheNameKeyGenerator->nameToKey( name ) );
}

//-------------------------------------------------------------------------------------------------
/*static*/ void AttributeModifierStore::parseAttributeModifierDefinition( INI *ini )
{
	const char *name = ini->getNextToken();
	const NameKeyType key = TheNameKeyGenerator->nameToKey( name );
	// a later definition of the same name replaces the earlier one (a mod's over the game's); read
	// and dropped where there is no store (the tools)
	AttributeModifierTemplate unused;
	AttributeModifierTemplate &tmpl = TheAttributeModifierStore != nullptr ? TheAttributeModifierStore->m_templates[key] : unused;
	tmpl = AttributeModifierTemplate();
	tmpl.m_name = name;
	tmpl.m_nameKey = key;
	ini->initFromINI( &tmpl, AttributeModifierTemplate::getFieldParse() );
	if( tmpl.m_stackingLimit < 1 )
		tmpl.m_stackingLimit = 1;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void AttributeModifierStore::parseModifierName( INI *ini, void * /*instance*/, void *store, const void * /*userData*/ )
{
	const char *name = ini->getNextToken();
	*(NameKeyType *)store = ( name == nullptr || stricmp( name, "None" ) == 0 ) ? NAMEKEY_INVALID : TheNameKeyGenerator->nameToKey( name );
}

//-------------------------------------------------------------------------------------------------
Bool attributeModifierReaches( const Object *source, const Object *obj, UnsignedInt affects )
{
	if( obj == nullptr || source == nullptr || obj->isEffectivelyDead() )
		return FALSE;
	if( obj == source )
		return ( affects & WEAPON_AFFECTS_SELF ) != 0;
	switch( source->getRelationship( obj ) )
	{
		case ALLIES:	return ( affects & WEAPON_AFFECTS_ALLIES ) != 0;
		case ENEMIES:	return ( affects & WEAPON_AFFECTS_ENEMIES ) != 0;
		default:			return ( affects & WEAPON_AFFECTS_NEUTRALS ) != 0;
	}
}

//-------------------------------------------------------------------------------------------------
/*static*/ void INI::parseAttributeModifierDefinition( INI *ini )
{
	AttributeModifierStore::parseAttributeModifierDefinition( ini );
}
