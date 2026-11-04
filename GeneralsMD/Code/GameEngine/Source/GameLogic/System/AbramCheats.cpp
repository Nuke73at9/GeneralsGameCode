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

// FILE: AbramCheats.cpp //////////////////////////////////////////////////////////////////////////
// Abram's Personal Test @feature the match's cheats (see AbramCheats.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/AbramCheats.h"

#include "Common/AbramGameRules.h"
#include "Common/BuildAssistant.h"
#include "Common/GameAudio.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Science.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/CreateModule.h"

#include <map>
#include <vector>

namespace
{
	enum
	{
		FLAG_UNLIMITED_MONEY = 0x0001,
		FLAG_UNLIMITED_POWER = 0x0002,
		FLAG_UNLOCKED_POWERS = 0x0004,
	};

	/// a frozen object, and whether the freeze holds it where it is (DISABLED_HELD: it was in the air)
	typedef std::map<ObjectID, Bool> FrozenObjects;

	Bool s_aiFrozen = FALSE;
	UnsignedInt s_playerFlags[MAX_PLAYER_COUNT];
	FrozenObjects s_frozen;
	AbramCheats s_snapshot;

	//-----------------------------------------------------------------------------------------------
	Bool isValidPlayerIndex( Int index )
	{
		return index >= 0 && index < MAX_PLAYER_COUNT;
	}

	//-----------------------------------------------------------------------------------------------
	Bool hasFlag( Int playerIndex, UnsignedInt flag )
	{
		return isValidPlayerIndex( playerIndex ) && ( s_playerFlags[playerIndex] & flag ) != 0;
	}

	//-----------------------------------------------------------------------------------------------
	void setFlag( Int playerIndex, UnsignedInt flag, Bool on )
	{
		if( !isValidPlayerIndex( playerIndex ) )
			return;
		if( on )
			s_playerFlags[playerIndex] |= flag;
		else
			s_playerFlags[playerIndex] &= ~flag;
	}

	//-----------------------------------------------------------------------------------------------
	/** a disable that darkens the object, as the engine's own do (the freeze's two do not) */
	Bool hasTintingDisable( const Object *obj )
	{
		DisabledMaskType flags = obj->getDisabledFlags();
		flags.set( DISABLED_HELD, 0 );
		flags.set( DISABLED_SCRIPT_DISABLED, 0 );
		flags.set( DISABLED_UNMANNED, 0 );
		return flags.any();
	}

	//-----------------------------------------------------------------------------------------------
	/** a frozen object looks it: the engine's dark tint of a disabled object */
	void showFrozen( Object *obj )
	{
		Drawable *draw = obj->getDrawable();
		if( draw != nullptr && !draw->testTintStatus( TINT_STATUS_DISABLED ) )
			draw->setTintStatus( TINT_STATUS_DISABLED );
	}

	//-----------------------------------------------------------------------------------------------
	/** stops an object: no thinking, moving, shooting, building or powers (a script's disable), and one in
		* the air hangs there (held, so it does not fall). Whether it was held for that. */
	Bool freeze( Object *obj )
	{
		obj->setDisabled( DISABLED_SCRIPT_DISABLED );
		Bool held = FALSE;
		if( !obj->isContained() && !obj->isDisabledByType( DISABLED_HELD ) && obj->getHeightAboveTerrain() > 1.0f )
		{
			obj->setDisabled( DISABLED_HELD );
			held = TRUE;
		}
		showFrozen( obj );
		return held;
	}

	//-----------------------------------------------------------------------------------------------
	void thaw( Object *obj, Bool held )
	{
		if( held )
			obj->clearDisabled( DISABLED_HELD );
		// (a script's own disable stays)
		if( !obj->testScriptStatusBit( OBJECT_STATUS_SCRIPT_DISABLED ) )
			obj->clearDisabled( DISABLED_SCRIPT_DISABLED );
		Drawable *draw = obj->getDrawable();
		if( draw != nullptr && !hasTintingDisable( obj ) )
			draw->clearTintStatus( TINT_STATUS_DISABLED );
	}

	//-----------------------------------------------------------------------------------------------
	/** every generals power of every faction: the sciences that are bought with generals points (not the
		* ranks, not a faction's own), in the store's order, so every machine grants the same */
	void unlockPowers( Player *player )
	{
		const std::vector<AsciiString> names = TheScienceStore->friend_getScienceNames();
		for( size_t i = 0; i < names.size(); ++i )
		{
			const ScienceType science = TheScienceStore->getScienceFromInternalName( names[i] );
			if( science == SCIENCE_INVALID || TheScienceStore->getSciencePurchaseCost( science ) <= 0 )
				continue;
			if( !TheScienceStore->isScienceGrantable( science ) || player->hasScience( science ) )
				continue;
			player->grantScience( science );
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** where the n-th of a squad's count stands: around the click, a step apart */
	Coord3D squadSpot( const ThingTemplate *tmpl, const Coord3D &where, Int n, Int count )
	{
		Coord3D at = where;
		if( count <= 1 || n == 0 )
			return at;
		const Real radius = tmpl->getTemplateGeometryInfo().getBoundingCircleRadius();
		const Real step = radius > 6.0f ? radius * 2.5f : 15.0f;
		const Real angle = 2.0f * PI * (Real)( n - 1 ) / (Real)( count - 1 );
		at.x += step * Cos( angle );
		at.y += step * Sin( angle );
		return at;
	}

	//-----------------------------------------------------------------------------------------------
	/** a unit, as a factory makes one (its player told, its create modules finished), at at or the nearest
		* clear ground around it */
	void spawnUnit( Player *player, const ThingTemplate *tmpl, const Coord3D &at, Real angle, Bool voice )
	{
		Coord3D pos = at;
		if( !tmpl->isKindOf( KINDOF_AIRCRAFT ) )
		{
			FindPositionOptions options;
			options.minRadius = 0.0f;
			options.maxRadius = 200.0f;
			options.startAngle = angle;		// (not the logic's random numbers)
			Coord3D found;
			if( ThePartitionManager->findPositionAround( &at, &options, &found ) )
				pos = found;
		}
		pos.z = TheTerrainLogic->getGroundHeight( pos.x, pos.y );

		Object *obj = TheThingFactory->newObject( tmpl, player->getDefaultTeam() );
		if( obj == nullptr )
			return;
		obj->setPosition( &pos );
		obj->setOrientation( angle );

		if( voice )
		{
			AudioEventRTS sound = *obj->getTemplate()->getVoiceCreated();
			sound.setObjectID( obj->getID() );
			TheAudio->addAudioEvent( &sound );
		}

		player->onUnitCreated( nullptr, obj );
		for( BehaviorModule **m = obj->getBehaviorModules(); *m != nullptr; ++m )
		{
			CreateModuleInterface *create = (*m)->getCreate();
			if( create != nullptr )
				create->onBuildComplete();
		}
	}

	//-----------------------------------------------------------------------------------------------
	void spawn( Player *player, Int templateID, const Coord3D &where, Real angle )
	{
		const ThingTemplate *tmpl = templateID >= 0 && templateID <= 0xFFFF
			? TheThingFactory->findByTemplateID( (UnsignedShort)templateID ) : nullptr;
		if( !AbramCheats::isSpawnable( tmpl ) || AbramCheats::checkSpawnLocation( tmpl, &where, angle ) != LBC_OK )
			return;

		if( tmpl->isKindOf( KINDOF_STRUCTURE ) )
		{
			// built at once, as the AI's own instant structures
			Object *made = TheBuildAssistant->buildObjectNow( nullptr, tmpl, &where, angle, player );
			if( made != nullptr )
			{
				made->clearStatus( MAKE_OBJECT_STATUS_MASK2( OBJECT_STATUS_UNDER_CONSTRUCTION, OBJECT_STATUS_RECONSTRUCTING ) );
				made->updateUpgradeModules();
			}
			return;
		}

		// a squad's soldiers all (Kane's Wrath's: BuildQuantity), as one purchase makes them
		const Int count = tmpl->getBuildQuantity() > 1 ? tmpl->getBuildQuantity() : 1;
		for( Int n = 0; n < count; ++n )
			spawnUnit( player, tmpl, squadSpot( tmpl, where, n, count ), angle, n == 0 );
	}

	//-----------------------------------------------------------------------------------------------
	void applyCheat( Player *player, Int cheat, Bool on )
	{
		const Int index = player->getPlayerIndex();
		switch( cheat )
		{
			case ABRAM_CHEAT_FREEZE_AI:
				// (update() freezes and thaws, this same frame)
				s_aiFrozen = on;
				break;
			case ABRAM_CHEAT_UNLIMITED_MONEY:
				setFlag( index, FLAG_UNLIMITED_MONEY, on );
				break;
			case ABRAM_CHEAT_UNLIMITED_POWER:
				setFlag( index, FLAG_UNLIMITED_POWER, on );
				// the base's buildings go on or off with it now
				player->onPowerBrownOutChange( !player->getEnergy()->hasSufficientPower() );
				break;
			case ABRAM_CHEAT_UNLOCK_POWERS:
				unlockPowers( player );
				setFlag( index, FLAG_UNLOCKED_POWERS, TRUE );
				break;
			default:
				break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
Bool AbramCheats::isEnabled()
{
	return AbramGameRules::isEnabled( ABRAM_RULE_CHEATS );
}

//-------------------------------------------------------------------------------------------------
void AbramCheats::reset()
{
	s_aiFrozen = FALSE;
	for( Int i = 0; i < MAX_PLAYER_COUNT; ++i )
		s_playerFlags[i] = 0;
	s_frozen.clear();
}

//-------------------------------------------------------------------------------------------------
void AbramCheats::update()
{
	if( !s_aiFrozen && s_frozen.empty() )
		return;
	if( TheGameLogic == nullptr || ThePlayerList == nullptr )
		return;

	// the ones that go on: the freeze is off, or the object changed hands, died or is gone
	for( FrozenObjects::iterator it = s_frozen.begin(); it != s_frozen.end(); )
	{
		Object *obj = TheGameLogic->findObjectByID( it->first );
		if( obj == nullptr )
		{
			s_frozen.erase( it++ );
			continue;
		}
		if( !s_aiFrozen || obj->isEffectivelyDead() || !isFrozenPlayer( obj->getControllingPlayer() ) )
		{
			thaw( obj, it->second );
			s_frozen.erase( it++ );
			continue;
		}
		// (another disable that ended may have taken the tint)
		showFrozen( obj );
		++it;
	}

	if( !s_aiFrozen )
		return;

	// the computer players' objects not frozen yet (new ones too: made, spawned, a wreck's crew)
	for( Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject() )
	{
		if( obj->isEffectivelyDead() || !isFrozenPlayer( obj->getControllingPlayer() ) )
			continue;
		if( s_frozen.find( obj->getID() ) != s_frozen.end() )
			continue;
		s_frozen[ obj->getID() ] = freeze( obj );
	}
}

//-------------------------------------------------------------------------------------------------
void AbramCheats::processMessage( const GameMessage *msg )
{
	if( !isEnabled() || msg == nullptr || ThePlayerList == nullptr )
		return;
	const Int index = msg->getPlayerIndex();
	Player *player = index >= 0 && index < ThePlayerList->getPlayerCount() ? ThePlayerList->getNthPlayer( index ) : nullptr;
	if( player == nullptr || player == ThePlayerList->getNeutralPlayer() || !player->isPlayerActive() )
		return;

	switch( msg->getType() )
	{
		case GameMessage::MSG_ABRAM_CHEAT:
			if( msg->getArgumentCount() >= 2 )
				applyCheat( player, msg->getArgument( 0 )->integer, msg->getArgument( 1 )->integer != 0 );
			break;
		case GameMessage::MSG_ABRAM_CHEAT_SPAWN:
			if( msg->getArgumentCount() >= 3 )
				spawn( player, msg->getArgument( 0 )->integer, msg->getArgument( 1 )->location, msg->getArgument( 2 )->real );
			break;
		default:
			break;
	}
}

//-------------------------------------------------------------------------------------------------
Bool AbramCheats::isAIFrozen()
{
	return s_aiFrozen;
}

//-------------------------------------------------------------------------------------------------
Bool AbramCheats::isFrozenPlayer( Player *player )
{
	// (a skirmish AI: not the civilians, not an AI a player has taken control of)
	return s_aiFrozen && player != nullptr && ThePlayerList != nullptr && player != ThePlayerList->getNeutralPlayer()
		&& player->getPlayerType() == PLAYER_COMPUTER && player->isSkirmishAIPlayer();
}

//-------------------------------------------------------------------------------------------------
Bool AbramCheats::hasUnlimitedMoney( Int playerIndex )
{
	return hasFlag( playerIndex, FLAG_UNLIMITED_MONEY );
}

//-------------------------------------------------------------------------------------------------
Bool AbramCheats::hasUnlimitedPower( Int playerIndex )
{
	return hasFlag( playerIndex, FLAG_UNLIMITED_POWER );
}

//-------------------------------------------------------------------------------------------------
Bool AbramCheats::hasUnlockedPowers( Int playerIndex )
{
	return hasFlag( playerIndex, FLAG_UNLOCKED_POWERS );
}

//-------------------------------------------------------------------------------------------------
Bool AbramCheats::isSpawnable( const ThingTemplate *tmpl )
{
	if( tmpl == nullptr )
		return FALSE;
	if( tmpl->isKindOf( KINDOF_PROJECTILE ) || tmpl->isKindOf( KINDOF_INERT ) )
		return FALSE;
	return tmpl->isKindOf( KINDOF_STRUCTURE ) || tmpl->isKindOf( KINDOF_INFANTRY )
		|| tmpl->isKindOf( KINDOF_VEHICLE ) || tmpl->isKindOf( KINDOF_AIRCRAFT );
}

//-------------------------------------------------------------------------------------------------
LegalBuildCode AbramCheats::checkSpawnLocation( const ThingTemplate *tmpl, const Coord3D *pos, Real angle )
{
	if( tmpl == nullptr || pos == nullptr || TheTerrainLogic == nullptr )
		return LBC_GENERIC_FAILURE;
	Region3D extent;
	TheTerrainLogic->getMaximumPathfindExtent( &extent );
	if( !extent.isInRegion( pos->asCoord2D() ) )
		return LBC_RESTRICTED_TERRAIN;
	if( !tmpl->isKindOf( KINDOF_STRUCTURE ) )
		return LBC_OK;
	// no builder and no player: no path to walk, no shroud, no zones (the cheat places it anywhere it fits)
	return TheBuildAssistant->isLocationLegalToBuild( pos, tmpl, angle,
		BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP, nullptr, nullptr );
}

//-------------------------------------------------------------------------------------------------
Snapshot *AbramCheats::getSnapshot()
{
	return &s_snapshot;
}

//-------------------------------------------------------------------------------------------------
void AbramCheats::crc( Xfer *xfer )
{
}

//-------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
//-------------------------------------------------------------------------------------------------
void AbramCheats::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	xfer->xferBool( &s_aiFrozen );

	Int playerCount = MAX_PLAYER_COUNT;
	xfer->xferInt( &playerCount );
	for( Int i = 0; i < playerCount; ++i )
	{
		UnsignedInt flags = isValidPlayerIndex( i ) ? s_playerFlags[i] : 0;
		xfer->xferUnsignedInt( &flags );
		if( isValidPlayerIndex( i ) )
			s_playerFlags[i] = flags;
	}

	Int frozenCount = (Int)s_frozen.size();
	xfer->xferInt( &frozenCount );
	if( xfer->getXferMode() == XFER_LOAD )
	{
		s_frozen.clear();
		for( Int i = 0; i < frozenCount; ++i )
		{
			ObjectID id = INVALID_ID;
			Bool held = FALSE;
			xfer->xferObjectID( &id );
			xfer->xferBool( &held );
			s_frozen[ id ] = held;
		}
	}
	else
	{
		for( FrozenObjects::iterator it = s_frozen.begin(); it != s_frozen.end(); ++it )
		{
			ObjectID id = it->first;
			Bool held = it->second;
			xfer->xferObjectID( &id );
			xfer->xferBool( &held );
		}
	}
}

//-------------------------------------------------------------------------------------------------
void AbramCheats::loadPostProcess()
{
}
