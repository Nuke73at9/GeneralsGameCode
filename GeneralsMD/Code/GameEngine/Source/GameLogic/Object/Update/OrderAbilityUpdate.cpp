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

#include "Common/Xfer.h"
#include "GameLogic/AI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"

static const char *const TheAbilityOrderNames[] =
{
	"REPAIR_TARGET",
	"RETURN_TO_PRODUCER",
	nullptr
};
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
	if( ai == nullptr || me->isEffectivelyDead() )
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
