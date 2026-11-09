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

// FILE: DecoyArmyUpdate.cpp //////////////////////////////////////////////////////////////////////
// Tiberium Project @feature the decoy armies (see DecoyArmyUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/DecoyArmyUpdate.h"

#include "Common/GameUtility.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/GameText.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

static const char *const TheDecoyArmyModeNames[] =
{
	"COPY",
	"STORE",
	"SPAWN",
	nullptr
};
static_assert(ARRAY_SIZE(TheDecoyArmyModeNames) == DECOY_ARMY_MODE_COUNT + 1, "Incorrect array size");

//-------------------------------------------------------------------------------------------------
DecoyArmyUpdateModuleData::DecoyArmyUpdateModuleData()
{
	m_specialPowerTemplate = nullptr;
	m_mode = DECOY_ARMY_COPY;
	m_radius = 0.0f;
	m_affects = WEAPON_AFFECTS_ALLIES;
	m_kindOf.clear();
	m_forbiddenKindOf.clear();
	m_minDestinationRadius = 0.0f;
	m_maxDestinationRadius = 0.0f;
	m_lifetime = 0;
	m_modifier = NAMEKEY_INVALID;
	m_triggerFX = nullptr;
}

//-------------------------------------------------------------------------------------------------
void DecoyArmyUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "SpecialPowerTemplate",		INI::parseSpecialPowerTemplate,	nullptr, offsetof( DecoyArmyUpdateModuleData, m_specialPowerTemplate ) },
		{ "Mode",										INI::parseIndexList,						TheDecoyArmyModeNames, offsetof( DecoyArmyUpdateModuleData, m_mode ) },
		{ "Radius",									INI::parseReal,									nullptr, offsetof( DecoyArmyUpdateModuleData, m_radius ) },
		{ "Affects",								INI::parseBitString32,					TheWeaponAffectsMaskNames, offsetof( DecoyArmyUpdateModuleData, m_affects ) },
		{ "KindOf",									KindOfMaskType::parseFromINI,		nullptr, offsetof( DecoyArmyUpdateModuleData, m_kindOf ) },
		{ "ForbiddenKindOf",				KindOfMaskType::parseFromINI,		nullptr, offsetof( DecoyArmyUpdateModuleData, m_forbiddenKindOf ) },
		{ "MinDestinationRadius",		INI::parseReal,									nullptr, offsetof( DecoyArmyUpdateModuleData, m_minDestinationRadius ) },
		{ "MaxDestinationRadius",		INI::parseReal,									nullptr, offsetof( DecoyArmyUpdateModuleData, m_maxDestinationRadius ) },
		{ "ClonedObjectLifetime",		INI::parseDurationUnsignedInt,	nullptr, offsetof( DecoyArmyUpdateModuleData, m_lifetime ) },
		{ "ClonedModifier",					AttributeModifierStore::parseModifierName,	nullptr, offsetof( DecoyArmyUpdateModuleData, m_modifier ) },
		{ "TriggerFX",							INI::parseFXList,								nullptr, offsetof( DecoyArmyUpdateModuleData, m_triggerFX ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
DecoyArmyUpdate::DecoyArmyUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
DecoyArmyUpdate::~DecoyArmyUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool DecoyArmyUpdate::isStoring() const
{
	return getDecoyArmyUpdateModuleData()->m_mode == DECOY_ARMY_STORE;
}

//-------------------------------------------------------------------------------------------------
void DecoyArmyUpdate::setStored( const std::vector<AsciiString> &names, const std::vector<Int> &levels )
{
	m_stored = names;
	m_storedLevels = levels;
}

//-------------------------------------------------------------------------------------------------
void DecoyArmyUpdate::clearStored()
{
	m_stored.clear();
	m_storedLevels.clear();
}

//-------------------------------------------------------------------------------------------------
/** (SPAWN) the STORE module on its own object: its player's STORE modules all remember the same */
//-------------------------------------------------------------------------------------------------
const DecoyArmyUpdate *DecoyArmyUpdate::findStore() const
{
	static const NameKeyType key = NAMEKEY( "DecoyArmyUpdate" );
	for( BehaviorModule **m = getObject()->getBehaviorModules(); *m; ++m )
	{
		if( (*m)->getModuleNameKey() != key )
			continue;
		const DecoyArmyUpdate *other = static_cast<const DecoyArmyUpdate *>( *m );
		if( other->isStoring() )
			return other;
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** what it copies (or remembers): within Radius of the spot, of the kinds and relationships it takes; not
	* what is inside something, nor a copy */
//-------------------------------------------------------------------------------------------------
void DecoyArmyUpdate::gather( const Coord3D *spot, std::vector<Copy> &out ) const
{
	const DecoyArmyUpdateModuleData *d = getDecoyArmyUpdateModuleData();
	const Object *me = getObject();
	if( d->m_radius <= 0.0f )
		return;
	const AttributeModifierTemplate *copied = TheAttributeModifierStore->find( d->m_modifier );
	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( spot, d->m_radius, FROM_CENTER_2D );
	MemoryPoolObjectHolder hold( iter );
	for( Object *other = iter->first(); other != nullptr; other = iter->next() )
	{
		if( other == me || other->isEffectivelyDead() || other->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
			continue;
		if( other->getContainedBy() != nullptr )
			continue;
		if( d->m_kindOf.any() && !other->isAnyKindOf( d->m_kindOf ) )
			continue;
		if( other->isAnyKindOf( d->m_forbiddenKindOf ) )
			continue;
		if( !attributeModifierReaches( me, other, d->m_affects ) )
			continue;
		if( copied != nullptr && other->hasAttributeModifier( copied ) )
			continue;
		Copy c;
		c.tmpl = other->getTemplate();
		c.level = (Int)other->getVeterancyLevel();
		c.beside = *other->getPosition();
		c.angle = other->getOrientation();
		out.push_back( c );
	}
}

//-------------------------------------------------------------------------------------------------
/** the copies, for its player: beside what each copies (COPY), or around the spot (SPAWN), each with the
	* copies' modifier for their lifetime, and faint to its own side */
//-------------------------------------------------------------------------------------------------
void DecoyArmyUpdate::makeCopies( const std::vector<Copy> &copies, const Coord3D *spot ) const
{
	const DecoyArmyUpdateModuleData *d = getDecoyArmyUpdateModuleData();
	const Object *me = getObject();
	Player *player = me->getControllingPlayer();
	if( player == nullptr || player->getDefaultTeam() == nullptr )
		return;
	const AttributeModifierTemplate *modifier = TheAttributeModifierStore->find( d->m_modifier );
	const Player *local = ThePlayerList->getLocalPlayer();
	const Bool faint = local != nullptr && ( local == player ||
		( local->getDefaultTeam() != nullptr && player->getRelationship( local->getDefaultTeam() ) == ALLIES ) );
	for( size_t i = 0; i < copies.size(); ++i )
	{
		const Copy &c = copies[i];
		if( c.tmpl == nullptr )
			continue;
		FindPositionOptions options;
		const Coord3D *center = spot;
		if( d->m_mode == DECOY_ARMY_COPY )
		{
			center = &c.beside;
			options.minRadius = c.tmpl->getTemplateGeometryInfo().getBoundingCircleRadius();
			options.maxRadius = options.minRadius * 4.0f + 20.0f;
		}
		else
		{
			options.minRadius = d->m_minDestinationRadius;
			options.maxRadius = max( d->m_maxDestinationRadius, d->m_minDestinationRadius + 10.0f );
		}
		Coord3D pos;
		if( !ThePartitionManager->findPositionAround( center, &options, &pos ) )
			pos = *center;
		Object *copy = TheThingFactory->newObject( c.tmpl, player->getDefaultTeam() );
		if( copy == nullptr )
			continue;
		copy->setPosition( &pos );
		if( d->m_mode == DECOY_ARMY_COPY )
			copy->setOrientation( c.angle );
		else
			copy->setOrientation( WWMath::Atan2( pos.y - spot->y, pos.x - spot->x ) );
		ExperienceTracker *xp = copy->getExperienceTracker();
		if( xp != nullptr && c.level > LEVEL_REGULAR && c.level <= LEVEL_LAST )
			xp->setVeterancyLevel( (VeterancyLevel)c.level, FALSE );
		if( modifier != nullptr )
			copy->applyAttributeModifier( modifier, me->getID(), d->m_lifetime );
		// (to its own side, a copy shows faint: C&C4's decoy shader for its owner and allies)
		Drawable *draw = copy->getDrawable();
		if( faint && draw != nullptr )
			draw->setDrawableOpacity( 0.6f );
	}
}

//-------------------------------------------------------------------------------------------------
/** a use refused before it is paid for: a SPAWN with nothing remembered, a COPY or a STORE with nothing in
	* reach (its player is told) */
//-------------------------------------------------------------------------------------------------
Bool DecoyArmyUpdate::refusesSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos ) const
{
	const DecoyArmyUpdateModuleData *d = getDecoyArmyUpdateModuleData();
	if( specialPowerTemplate != d->m_specialPowerTemplate )
		return FALSE;
	Bool none;
	if( d->m_mode == DECOY_ARMY_SPAWN )
	{
		const DecoyArmyUpdate *store = findStore();
		none = store == nullptr || !store->hasStored();
	}
	else
	{
		Coord3D spot = targetObj != nullptr ? *targetObj->getPosition() : ( targetPos != nullptr ? *targetPos : *getObject()->getPosition() );
		std::vector<Copy> copies;
		gather( &spot, copies );
		none = copies.empty();
	}
	if( !none )
		return FALSE;
	const Player *player = getObject()->getControllingPlayer();
	if( player != nullptr && player == ThePlayerList->getLocalPlayer() && TheInGameUI != nullptr && TheGameText != nullptr )
	{
		if( d->m_mode == DECOY_ARMY_SPAWN )
			TheInGameUI->messageNoFormat( TheGameText->FETCH_OR_SUBSTITUTE( "GUI:TibDecoyChooseFirst", L"Decoy Army: choose the units to copy first" ) );
		else
			TheInGameUI->messageNoFormat( TheGameText->FETCH_OR_SUBSTITUTE( "GUI:TibDecoyNothing", L"Decoy Army: no units of yours there to copy" ) );
	}
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
namespace
{
	struct StoreArgs
	{
		const std::vector<AsciiString> *names;
		const std::vector<Int> *levels;
	};

	// each of its player's STORE modules remembers the same (or forgets: no names)
	void storeOn( Object *obj, void *userData )
	{
		static const NameKeyType key = NAMEKEY( "DecoyArmyUpdate" );
		const StoreArgs *args = (const StoreArgs *)userData;
		for( BehaviorModule **m = obj->getBehaviorModules(); *m; ++m )
		{
			if( (*m)->getModuleNameKey() != key )
				continue;
			DecoyArmyUpdate *store = static_cast<DecoyArmyUpdate *>( *m );
			if( !store->isStoring() )
				continue;
			if( args->names != nullptr )
				store->setStored( *args->names, *args->levels );
			else
				store->clearStored();
		}
	}
}

//-------------------------------------------------------------------------------------------------
Bool DecoyArmyUpdate::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const DecoyArmyUpdateModuleData *d = getDecoyArmyUpdateModuleData();
	if( specialPowerTemplate != d->m_specialPowerTemplate )
		return FALSE;
	Object *me = getObject();
	Player *player = me->getControllingPlayer();
	if( me->isEffectivelyDead() || player == nullptr )
		return FALSE;
	const Coord3D spot = targetObj != nullptr ? *targetObj->getPosition() : ( targetPos != nullptr ? *targetPos : *me->getPosition() );
	if( d->m_triggerFX != nullptr )
		FXList::doFXPos( d->m_triggerFX, &spot );

	std::vector<Copy> copies;
	switch( d->m_mode )
	{
		case DECOY_ARMY_STORE:
		{
			gather( &spot, copies );
			std::vector<AsciiString> names;
			std::vector<Int> levels;
			for( size_t i = 0; i < copies.size(); ++i )
			{
				names.push_back( copies[i].tmpl->getName() );
				levels.push_back( copies[i].level );
			}
			StoreArgs args = { &names, &levels };
			player->iterateObjects( storeOn, &args );
			break;
		}
		case DECOY_ARMY_SPAWN:
		{
			const DecoyArmyUpdate *store = findStore();
			if( store == nullptr || !store->hasStored() )
				return FALSE;
			for( size_t i = 0; i < store->m_stored.size(); ++i )
			{
				Copy c;
				c.tmpl = TheThingFactory->findTemplate( store->m_stored[i] );
				c.level = i < store->m_storedLevels.size() ? store->m_storedLevels[i] : (Int)LEVEL_REGULAR;
				c.beside = spot;
				c.angle = 0.0f;
				copies.push_back( c );
			}
			makeCopies( copies, &spot );
			// (what was remembered is used up)
			StoreArgs args = { nullptr, nullptr };
			player->iterateObjects( storeOn, &args );
			break;
		}
		default:
			gather( &spot, copies );
			makeCopies( copies, &spot );
			break;
	}
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void DecoyArmyUpdate::crc( Xfer *xfer )
{
	SpecialPowerUpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void DecoyArmyUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	SpecialPowerUpdateModule::xfer( xfer );

	UnsignedShort count = (UnsignedShort)m_stored.size();
	xfer->xferUnsignedShort( &count );
	if( xfer->getXferMode() == XFER_LOAD )
	{
		m_stored.resize( count );
		m_storedLevels.resize( count );
	}
	for( UnsignedShort i = 0; i < count; ++i )
	{
		xfer->xferAsciiString( &m_stored[i] );
		xfer->xferInt( &m_storedLevels[i] );
	}
}

//-------------------------------------------------------------------------------------------------
void DecoyArmyUpdate::loadPostProcess()
{
	SpecialPowerUpdateModule::loadPostProcess();
}
