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

// FILE: OrderAbilityUpdate.cpp ///////////////////////////////////////////////////////////////////
// Tiberium Project @feature special powers that give the unit an order (see OrderAbilityUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/OrderAbilityUpdate.h"

#include "Common/AudioEventRTS.h"
#include "Common/GameAudio.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ResourceGatheringManager.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/AI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"

static const char *const TheAbilityOrderNames[] =
{
	"REPAIR_TARGET",
	"RETURN_TO_PRODUCER",
	"RETURN_TO_DOCK",
	"RECALL_AIRCRAFT",
	nullptr
};

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature RETURN_TO_DOCK: the nearest of a player's refineries (supply centers) that stands
//-------------------------------------------------------------------------------------------------
struct NearestCenterInfo
{
	const Object *m_me;
	Object *m_best;
	Real m_bestDistSqr;
};

static void nearestSupplyCenter( Object *obj, void *userData )
{
	static const NameKeyType key_centerUpdate = NAMEKEY( "SupplyCenterDockUpdate" );
	NearestCenterInfo *info = (NearestCenterInfo *)userData;
	if( obj->isEffectivelyDead() || obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) || obj->testStatus( OBJECT_STATUS_SOLD )
		|| obj->findUpdateModule( key_centerUpdate ) == nullptr )
		return;
	const Real distSqr = ThePartitionManager->getDistanceSquared( info->m_me, obj, FROM_CENTER_2D );
	if( info->m_best == nullptr || distSqr < info->m_bestDistSqr )
	{
		info->m_best = obj;
		info->m_bestDistSqr = distSqr;
	}
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature RECALL_AIRCRAFT: the airfield's aircraft, each sent back to its parking place
//-------------------------------------------------------------------------------------------------
struct RecallAircraftInfo
{
	Object *m_airfield;
	ParkingPlaceBehaviorInterface *m_parking;
	Object *m_firstAway;	///< the first one sent back that is in the air
};

static void recallAircraft( Object *obj, void *userData )
{
	RecallAircraftInfo *info = (RecallAircraftInfo *)userData;
	if( obj == info->m_airfield || obj->isEffectivelyDead() || !obj->isKindOf( KINDOF_AIRCRAFT )
		|| !info->m_parking->hasReservedSpace( obj->getID() ) )
		return;
	AIUpdateInterface *ai = obj->getAI();
	if( ai == nullptr )
		return;
	// (one parked there already takes no notice: JetAIUpdate ignores an order into its own airfield)
	ai->aiEnter( info->m_airfield, CMD_FROM_PLAYER );
	if( info->m_firstAway == nullptr && obj->isAirborneTarget() )
		info->m_firstAway = obj;
}
static_assert(ARRAY_SIZE(TheAbilityOrderNames) == ABILITY_ORDER_COUNT + 1, "Incorrect array size");

//-------------------------------------------------------------------------------------------------
void OrderAbilityUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "SpecialPowerTemplate",	INI::parseSpecialPowerTemplate,	nullptr, offsetof( OrderAbilityUpdateModuleData, m_specialPowerTemplate ) },
		{ "Order",								INI::parseIndexList,						TheAbilityOrderNames, offsetof( OrderAbilityUpdateModuleData, m_order ) },
		{ "AttributeModifier",		AttributeModifierStore::parseModifierName,	nullptr, offsetof( OrderAbilityUpdateModuleData, m_modifier ) },
		{ "RecalledVoice",				INI::parseBool,									nullptr, offsetof( OrderAbilityUpdateModuleData, m_recalledVoice ) },	// Tiberium Project
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
OrderAbilityUpdate::OrderAbilityUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
OrderAbilityUpdate::~OrderAbilityUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool OrderAbilityUpdate::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const OrderAbilityUpdateModuleData *data = getOrderAbilityUpdateModuleData();
	if( specialPowerTemplate != data->m_specialPowerTemplate )
		return FALSE;
	Object *me = getObject();
	AIUpdateInterface *ai = me->getAI();
	// (an airfield has no AI of its own: it only orders others)
	if( me->isEffectivelyDead() || ( ai == nullptr && data->m_order != ABILITY_ORDER_RECALL_AIRCRAFT ) )
		return FALSE;

	switch( data->m_order )
	{
		case ABILITY_ORDER_REPAIR_TARGET:
		{
			// an ally's damaged structure (a Zero Hour dozer's repair)
			Object *target = targetObj != nullptr ? TheGameLogic->findObjectByID( targetObj->getID() ) : nullptr;
			const BodyModuleInterface *body = target != nullptr ? target->getBodyModule() : nullptr;
			if( ai->getDozerAIInterface() == nullptr
				|| target == nullptr || !target->isKindOf( KINDOF_STRUCTURE ) || me->getRelationship( target ) != ALLIES
				|| body == nullptr || body->getHealth() >= body->getMaxHealth() )
				return FALSE;
			ai->aiRepair( target, CMD_FROM_PLAYER );
			break;
		}
		case ABILITY_ORDER_RETURN_TO_PRODUCER:
		{
			// back to what made it: into its parking place there (a jet's, as on a right click on its
			// airfield), or to it
			Object *producer = TheGameLogic->findObjectByID( me->getProducerID() );
			if( producer == nullptr || producer->isEffectivelyDead() || me->getRelationship( producer ) == ENEMIES )
				return FALSE;
			Bool parks = FALSE;
			for( BehaviorModule **b = producer->getBehaviorModules(); *b != nullptr && !parks; ++b )
				parks = (*b)->getParkingPlaceBehaviorInterface() != nullptr;
			if( parks )
				ai->aiEnter( producer, CMD_FROM_PLAYER );
			else
				ai->aiMoveToObject( producer, CMD_FROM_PLAYER );
			break;
		}
		case ABILITY_ORDER_RETURN_TO_DOCK:
		{
			// Tiberium Project @feature a harvester to its refinery with what it carries (the one it would take its load
			// to), then back to its field as after any trip. Given as the AI's order: the refinery the player chose for it
			// with a right click stays its own
			SupplyTruckAIInterface *truck = ai->getSupplyTruckAIInterface();
			Player *owner = me->getControllingPlayer();
			ResourceGatheringManager *manager = owner != nullptr ? owner->getResourceGatheringManager() : nullptr;
			if( truck == nullptr || manager == nullptr )
				return FALSE;
			Object *center = manager->findBestSupplyCenter( me );
			if( center == nullptr )
			{
				// (empty, it has nothing to give one: the nearest of its player's refineries all the same)
				NearestCenterInfo info;
				info.m_me = me;
				info.m_best = nullptr;
				info.m_bestDistSqr = 0.0f;
				owner->iterateObjects( nearestSupplyCenter, &info );
				center = info.m_best;
			}
			if( center == nullptr )
				return FALSE;
			ai->aiDock( center, CMD_FROM_AI );
			break;
		}
		case ABILITY_ORDER_RECALL_AIRCRAFT:
		{
			// Tiberium Project @feature the airfield's aircraft (those with a parking place here) back to it
			ParkingPlaceBehaviorInterface *parking = nullptr;
			for( BehaviorModule **b = me->getBehaviorModules(); *b != nullptr && parking == nullptr; ++b )
				parking = (*b)->getParkingPlaceBehaviorInterface();
			Player *owner = me->getControllingPlayer();
			if( parking == nullptr || owner == nullptr )
				return FALSE;
			RecallAircraftInfo info;
			info.m_airfield = me;
			info.m_parking = parking;
			info.m_firstAway = nullptr;
			owner->iterateObjects( recallAircraft, &info );
			// (Kane's Wrath's PlayReturnVoiceFromRecalledUnit: heard by its player alone)
			if( data->m_recalledVoice && info.m_firstAway != nullptr && owner == ThePlayerList->getLocalPlayer() )
			{
				const ThingTemplate *tmpl = info.m_firstAway->getTemplate();
				const AudioEventRTS *voice = tmpl->getPerUnitSound( "VoiceEnter" );
				if( voice == nullptr || voice->getEventName().isEmpty() )
					voice = tmpl->getVoiceMove();
				if( voice != nullptr && voice->getEventName().isNotEmpty() )
				{
					AudioEventRTS sound = *voice;
					sound.setObjectID( info.m_firstAway->getID() );
					TheAudio->addAudioEvent( &sound );
				}
			}
			break;
		}
		default:
			return FALSE;
	}
	if( data->m_modifier != NAMEKEY_INVALID )
		me->applyAttributeModifier( TheAttributeModifierStore->find( data->m_modifier ), me->getID() );
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void OrderAbilityUpdate::crc( Xfer *xfer )
{
	SpecialPowerUpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void OrderAbilityUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	SpecialPowerUpdateModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void OrderAbilityUpdate::loadPostProcess()
{
	SpecialPowerUpdateModule::loadPostProcess();
}
