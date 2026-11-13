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

// FILE: TransportContain.cpp //////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, March 2002
// Desc:   Contain module for transport units.
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/StealthUpdate.h"
#include "GameLogic/Module/TransportContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"
#include "Common/Upgrade.h"	// Tiberium Project (PlaceUpgrades)


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
TransportContainModuleData::TransportContainModuleData()
{

	m_slotCapacity = 0;
	m_scatterNearbyOnExit = true;
	m_orientLikeContainerOnExit = false;
	m_keepContainerVelocityOnExit = false;
	m_goAggressiveOnExit = FALSE;
  m_armedRidersUpgradeWeaponSet = FALSE;
	m_resetMoodCheckTimeOnExit = true;
	m_destroyRidersWhoAreNotFreeToExit = false;
	m_exitPitchRate = 0.0f;
	m_initialPayload.count = 0;
	m_healthRegen = 0.0f;
	m_exitDelay = 0;
	m_isDelayExitInAir = FALSE;
	m_leaveMapWhenEmpty = FALSE;

	//
	// by default we say that transports can have infantry inside them, this will be totally
	// overwritten by any data provided from the INI entry tho
	//
	m_allowInsideKindOf = MAKE_KINDOF_MASK( KINDOF_INFANTRY );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TransportContainModuleData::parseInitialPayload( INI* ini, void *instance, void *store, const void* /*userData*/ )
{
	TransportContainModuleData* self = (TransportContainModuleData*)instance;
	const char* name = ini->getNextToken();
	const char* countStr = ini->getNextTokenOrNull();
	Int count = countStr ? INI::scanInt(countStr) : 1;

	self->m_initialPayload.name.set(name);
	self->m_initialPayload.count = count;
}

// ------------------------------------------------------------------------------------------------
// Tiberium Project @feature PlaceUpgrades = <upgrade at the first place> <at the second> ...: a kind of rider's upgrades,
// one for each place; its riders follow (PlaceRiders)
// ------------------------------------------------------------------------------------------------
void TransportContainModuleData::parsePlaceUpgrades( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	TransportContainModuleData* self = (TransportContainModuleData*)instance;
	PlaceKind kind;
	kind.m_ridersPerPlace = 1;
	for (const char* token = ini->getNextTokenOrNull(); token != nullptr; token = ini->getNextTokenOrNull())
		kind.m_upgradeNames.push_back(AsciiString(token));
	self->m_placeKinds.push_back(kind);
}

// ------------------------------------------------------------------------------------------------
// Tiberium Project @feature PlaceRiders = <how many share a place> <rider> <rider> ...: the riders of the kind the
// PlaceUpgrades before it gave
// ------------------------------------------------------------------------------------------------
void TransportContainModuleData::parsePlaceRiders( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	TransportContainModuleData* self = (TransportContainModuleData*)instance;
	if (self->m_placeKinds.empty())
		throw INI_INVALID_DATA;
	PlaceKind& kind = self->m_placeKinds.back();
	kind.m_ridersPerPlace = INI::scanInt(ini->getNextToken());
	if (kind.m_ridersPerPlace < 1)
		kind.m_ridersPerPlace = 1;
	for (const char* token = ini->getNextTokenOrNull(); token != nullptr; token = ini->getNextTokenOrNull())
		kind.m_riderNames.push_back(AsciiString(token));
}

// ------------------------------------------------------------------------------------------------
// Tiberium Project: the places its riders take (the most upgrades a kind lists)
// ------------------------------------------------------------------------------------------------
Int TransportContainModuleData::getPlaceCount() const
{
	Int count = 0;
	for (std::vector<PlaceKind>::const_iterator it = m_placeKinds.begin(); it != m_placeKinds.end(); ++it)
	{
		if ((Int)it->m_upgradeNames.size() > count)
			count = (Int)it->m_upgradeNames.size();
	}
	return count;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TransportContainModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  OpenContainModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "Slots",	INI::parseInt,		nullptr, offsetof( TransportContainModuleData, m_slotCapacity ) },
		{ "ScatterNearbyOnExit",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_scatterNearbyOnExit ) },
		{ "OrientLikeContainerOnExit",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_orientLikeContainerOnExit ) },
		{ "KeepContainerVelocityOnExit",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_keepContainerVelocityOnExit ) },
		{ "GoAggressiveOnExit",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_goAggressiveOnExit ) },
		{ "ResetMoodCheckTimeOnExit",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_resetMoodCheckTimeOnExit ) },
		{ "DestroyRidersWhoAreNotFreeToExit",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_destroyRidersWhoAreNotFreeToExit ) },
		{ "ExitBone",	INI::parseAsciiString,		nullptr, offsetof( TransportContainModuleData, m_exitBone ) },
		{ "ExitPitchRate",	INI::parseAngularVelocityReal,		nullptr, offsetof( TransportContainModuleData, m_exitPitchRate ) },
		{ "InitialPayload", parseInitialPayload, nullptr, 0 },
		{ "HealthRegen%PerSec", INI::parseReal, nullptr, offsetof( TransportContainModuleData, m_healthRegen ) },
		{ "ExitDelay",	INI::parseDurationUnsignedInt,		nullptr, offsetof( TransportContainModuleData, m_exitDelay ) },
		{ "ArmedRidersUpgradeMyWeaponSet",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_armedRidersUpgradeWeaponSet ) },
		{ "DelayExitInAir",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_isDelayExitInAir ) },
		{ "LeaveMapWhenEmpty",	INI::parseBool,		nullptr, offsetof( TransportContainModuleData, m_leaveMapWhenEmpty ) },	// Tiberium Project
		{ "PlaceUpgrades",	parsePlaceUpgrades,		nullptr, 0 },	// Tiberium Project
		{ "PlaceRiders",	parsePlaceRiders,		nullptr, 0 },	// Tiberium Project

    { nullptr, nullptr, nullptr, 0 }
  };
  p.add(dataFieldParse);
}



// PRIVATE ////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Int TransportContain::getContainMax() const
{
	// Tiberium Project @feature and the slots its upgrades added (ContainerCapacityUpgrade)
	if (getTransportContainModuleData())
		return max( 0, getTransportContainModuleData()->m_slotCapacity + getCapacityBonus() );

	return 0;
}

// PUBLIC /////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TransportContain::TransportContain( Thing *thing, const ModuleData *moduleData ) :
								 OpenContain( thing, moduleData )
{
	m_extraSlotsInUse = 0;
	m_frameExitNotBusy = 0;
	m_hasCarried = FALSE;
	m_leavingMap = FALSE;
	for (Int i = 0; i < MAX_RIDER_PLACES; ++i)
		m_placeKind[i] = -1;	// Tiberium Project: every place free
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
TransportContain::~TransportContain()
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/**
	can this container contain this kind of object?
	and, if checkCapacity is TRUE, does this container have enough space left to hold the given unit?
*/
Bool TransportContain::isValidContainerFor(const Object* rider, Bool checkCapacity) const
{

	// sanity
	if (!rider)
		return false;

	// The point of this new code is to determine when something is a "fake" container, to
	// look at the object inside of it to use that as the valid check. There is a case, when a
	// paratrooper (an infantry contained in a parachute). In this case, when we pass this object
	// to contain in a transport plane, we want to check the infantry, not the parachute.
	if (rider->getContain() && rider->getContain()->isSpecialZeroSlotContainer())
	{
		// Report the first thing inside it!
		const ContainedItemsList *items = rider->getContain()->getContainedItemsList();
		if (items && !items->empty())
		{
			if (items->front())
			{
				// Replace the object we are checking with the *first* object contained within it.
				rider = items->front();
			}
		}
	}


	// extend functionality
	if( OpenContain::isValidContainerFor( rider, checkCapacity ) == false )
		return false;

//	// only allied objects can be transported.
//	// order matters: we want to know if I consider it to be an ally, not vice versa
//	if (getObject()->getRelationship(rider) != ALLIES)
//		return false;

// no... actually, only OUR OWN units can be transported.
	if (rider->getControllingPlayer() != getObject()->getControllingPlayer())
		return false;

	Int transportSlotCount = rider->getTransportSlotCount();

	// if 0, this object isn't transportable.
	if (transportSlotCount == 0)
		return false;

	// Tiberium Project @feature with places for its riders (PlaceUpgrades), only riders of a kind it has, with a place of
	// their kind with room or a free one
	const Bool hasPlaces = !getTransportContainModuleData()->m_placeKinds.empty();
	const Int kind = hasPlaces ? findPlaceKind(rider) : -1;
	if (hasPlaces && kind < 0)
		return false;

	if (checkCapacity)
	{
    Int containMax = getContainMax();
    Int containCount = getContainCount();

		if (hasPlaces && choosePlace(kind) < 0)
			return false;

		return (m_extraSlotsInUse + containCount + transportSlotCount <= containMax);

	}
	else
	{
		return true;
	}
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature the places its riders take (PlaceUpgrades)
//-------------------------------------------------------------------------------------------------
Int TransportContain::findPlaceKind( const Object* rider ) const
{
	const TransportContainModuleData* d = getTransportContainModuleData();
	if (rider == nullptr || d->m_placeKinds.empty())
		return -1;
	const ThingTemplate* tmpl = rider->getTemplate();
	for (Int k = 0; k < (Int)d->m_placeKinds.size(); ++k)
	{
		const std::vector<AsciiString>& names = d->m_placeKinds[k].m_riderNames;
		for (std::vector<AsciiString>::const_iterator it = names.begin(); it != names.end(); ++it)
		{
			const ThingTemplate* t = TheThingFactory->findTemplate(*it);
			if (t != nullptr && tmpl->isEquivalentTo(t))
				return k;
		}
	}
	return -1;
}

//-------------------------------------------------------------------------------------------------
Int TransportContain::ridersAtPlace( Int place ) const
{
	Int count = 0;
	for (std::vector<PlaceRider>::const_iterator it = m_placeRiders.begin(); it != m_placeRiders.end(); ++it)
	{
		if (it->m_place == place)
			++count;
	}
	return count;
}

//-------------------------------------------------------------------------------------------------
// the place a rider of the kind takes: one of its kind with room, else the first free one (-1: none)
Int TransportContain::choosePlace( Int kind ) const
{
	const TransportContainModuleData* d = getTransportContainModuleData();
	if (kind < 0 || kind >= (Int)d->m_placeKinds.size())
		return -1;
	Int places = d->getPlaceCount();
	if (places > MAX_RIDER_PLACES)
		places = MAX_RIDER_PLACES;
	const Int share = d->m_placeKinds[kind].m_ridersPerPlace;
	for (Int p = 0; p < places; ++p)
	{
		if (m_placeKind[p] == kind && ridersAtPlace(p) < share)
			return p;
	}
	for (Int p = 0; p < places && p < (Int)d->m_placeKinds[kind].m_upgradeNames.size(); ++p)
	{
		if (m_placeKind[p] < 0)
			return p;
	}
	return -1;
}

//-------------------------------------------------------------------------------------------------
// the rider takes its place; the first of its kind there gives the transport the kind's upgrade for it
void TransportContain::takePlace( Object* rider )
{
	const TransportContainModuleData* d = getTransportContainModuleData();
	const Int kind = findPlaceKind(rider);
	const Int place = choosePlace(kind);
	if (place < 0)
		return;
	PlaceRider entry;
	entry.m_id = rider->getID();
	entry.m_place = place;
	m_placeRiders.push_back(entry);
	if (m_placeKind[place] < 0)
	{
		m_placeKind[place] = kind;
		const UpgradeTemplate* up = TheUpgradeCenter->findUpgrade(d->m_placeKinds[kind].m_upgradeNames[place]);
		if (up != nullptr)
			getObject()->giveUpgrade(up);
	}
}

//-------------------------------------------------------------------------------------------------
// the rider leaves its place; the last of them there takes the place's upgrade off the transport again
void TransportContain::leavePlace( Object* rider )
{
	const TransportContainModuleData* d = getTransportContainModuleData();
	for (std::vector<PlaceRider>::iterator it = m_placeRiders.begin(); it != m_placeRiders.end(); ++it)
	{
		if (it->m_id != rider->getID())
			continue;
		const Int place = it->m_place;
		m_placeRiders.erase(it);
		if (place >= 0 && place < MAX_RIDER_PLACES && ridersAtPlace(place) == 0 && m_placeKind[place] >= 0)
		{
			const Int kind = m_placeKind[place];
			m_placeKind[place] = -1;
			if (kind < (Int)d->m_placeKinds.size() && place < (Int)d->m_placeKinds[kind].m_upgradeNames.size())
			{
				const UpgradeTemplate* up = TheUpgradeCenter->findUpgrade(d->m_placeKinds[kind].m_upgradeNames[place]);
				if (up != nullptr && getObject()->hasUpgrade(up))
					getObject()->removeUpgrade(up);
			}
		}
		return;
	}
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature with places for its riders, a pip for each place
Bool TransportContain::getContainerPipsToShow( Int& numTotal, Int& numFull )
{
	const TransportContainModuleData* d = getTransportContainModuleData();
	if (d->m_placeKinds.empty())
		return OpenContain::getContainerPipsToShow(numTotal, numFull);
	numTotal = d->getPlaceCount();
	if (numTotal > MAX_RIDER_PLACES)
		numTotal = MAX_RIDER_PLACES;
	numFull = 0;
	for (Int p = 0; p < numTotal; ++p)
	{
		if (m_placeKind[p] >= 0)
			++numFull;
	}
	return true;
}



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void TransportContain::letRidersUpgradeWeaponSet()
{

  const TransportContainModuleData * d = getTransportContainModuleData();

  if ( ! d->m_armedRidersUpgradeWeaponSet )
    return;

  Object *self = getObject();
  if ( self == nullptr )
    return;

  Bool anyRiderHasViableWeapon = FALSE;

	const ContainedItemsList* riderList = getContainedItemsList();
	if( riderList )
	{
		ContainedItemsList::const_iterator it;
		it = riderList->begin();

		while( it != riderList->end() )
		{
			Object *rider = *it;

			//Advance to the next iterator
			it++;

      if ( rider )
      {
		if(rider->isKindOf(KINDOF_INFANTRY) == false)
			continue;
        Weapon *weapon = nullptr;
        for ( Int w = PRIMARY_WEAPON; w < WEAPONSLOT_COUNT; ++ w )
        {
          weapon = rider->getWeaponInWeaponSlot( (WeaponSlotType)w );
          if ( weapon )
          {
            if ( weapon->getTemplate()->isContactWeapon() == FALSE && weapon->isDamageWeapon() == TRUE ) // THIS MAY NEED TO CHECK MORE WEAPON ATTRIBUTES TO WORK BEST
            {
              anyRiderHasViableWeapon = TRUE;
              break;
            }
          }
        }
      }

    }
  }

  if ( anyRiderHasViableWeapon )
  	self->setWeaponSetFlag( WEAPONSET_PLAYER_UPGRADE );
  else
  	self->clearWeaponSetFlag( WEAPONSET_PLAYER_UPGRADE );

}



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void TransportContain::onContaining( Object *rider, Bool wasSelected )
{
	OpenContain::onContaining( rider, wasSelected );

	// objects inside a transport are held
	rider->setDisabled( DISABLED_HELD );
	m_hasCarried = TRUE;	// Tiberium Project (LeaveMapWhenEmpty)

	Int transportSlotCount = rider->getTransportSlotCount();

	DEBUG_ASSERTCRASH(transportSlotCount > 0, ("Hmm, this object isnt transportable"));
	m_extraSlotsInUse += transportSlotCount - 1;
	DEBUG_ASSERTCRASH(m_extraSlotsInUse >= 0 && m_extraSlotsInUse + getContainCount() <= getContainMax(), ("Hmm, bad slot count"));

	//
	// when we go from holding nothing to holding something we have a model condition
	// to visually show the change
	//
	if( getContainCount() == 1 )
	{
		Drawable *draw = getObject()->getDrawable();

		if( draw )
			draw->setModelConditionState( MODELCONDITION_LOADED );

	}

  if ( getTransportContainModuleData()->m_armedRidersUpgradeWeaponSet )
    letRidersUpgradeWeaponSet();

	// Tiberium Project @feature the rider takes its place (PlaceUpgrades)
	if( !getTransportContainModuleData()->m_placeKinds.empty() )
		takePlace( rider );

	//Kris: October 20, 2003 - Patch 1.01
	//Force Jarmen Kell to transfer weapon timer for snipe to and from the combat bike.
	if( getObject()->isKindOf( KINDOF_CLIFF_JUMPER ) && rider->isKindOf( KINDOF_HERO ) && rider->isKindOf( KINDOF_SALVAGER ) )
	{
		//Admittedly brutal hack, but considering the state of the game (post-ship), this is a particularly surgical fix.
		Weapon *bikeWeapon = getObject()->getWeaponInWeaponSlot( SECONDARY_WEAPON );
		Weapon *riderWeapon = rider->getWeaponInWeaponSlot( SECONDARY_WEAPON );
		if( bikeWeapon && riderWeapon )
		{
			//Transfer the reload time from the rider to the bike
			bikeWeapon->transferNextShotStatsFrom( *riderWeapon );
		}
	}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void TransportContain::onRemoving( Object *rider )
{
	OpenContain::onRemoving(rider);

	// object is no longer held inside a transport
	rider->clearDisabled( DISABLED_HELD );

	const TransportContainModuleData* d = getTransportContainModuleData();

	// Tiberium Project @feature the rider leaves its place (PlaceUpgrades)
	if( !d->m_placeKinds.empty() )
		leavePlace( rider );

	if (!d->m_exitBone.isEmpty())
	{
		Drawable* draw = getObject()->getDrawable();
		if (draw)
		{
			Coord3D bonePos, worldPos;
			if (draw->getPristineBonePositions(d->m_exitBone.str(), 0, &bonePos, nullptr, 1) == 1)
			{
				getObject()->convertBonePosToWorldPos(&bonePos, nullptr, &worldPos, nullptr);
				rider->setPosition(&worldPos);
			}
		}
	}

	if (d->m_orientLikeContainerOnExit)
	{
		rider->setOrientation(getObject()->getOrientation());
	}

	if (d->m_keepContainerVelocityOnExit)
	{
		PhysicsBehavior* parent = getObject()->getPhysics();
		PhysicsBehavior* child = rider->getPhysics();
		if (parent && child)
		{
			Coord3D startingForce = *parent->getVelocity();
			Real mass = child->getMass();
			startingForce.x *= mass;
			startingForce.y *= mass;
			startingForce.z *= mass;
			child->applyMotiveForce( &startingForce );

			Real pitchRate = child->getCenterOfMassOffset() * d->m_exitPitchRate;
			child->setPitchRate( pitchRate );
		}
	}

	Int transportSlotCount = rider->getTransportSlotCount();
	DEBUG_ASSERTCRASH(transportSlotCount > 0, ("Hmm, this object isnt transportable"));
	m_extraSlotsInUse -= transportSlotCount - 1;
	DEBUG_ASSERTCRASH(m_extraSlotsInUse >= 0 && m_extraSlotsInUse + getContainCount() <= getContainMax(), ("Hmm, bad slot count"));

	// when we are empty again, clear the model condition for loaded
	if( getContainCount() == 0 )
	{
		Drawable *draw = getObject()->getDrawable();

		if( draw )
			draw->clearModelConditionState( MODELCONDITION_LOADED );

	}

	if (getObject()->isAboveTerrain())
	{
		// temporarily mark the guy as being allowed to fall
		// (overriding his locomotor's stick-to-ground attribute).
		// this will be reset (by PhysicsBehavior) when he touches the ground.
		PhysicsBehavior* physics = rider->getPhysics();
		if (physics)
			physics->setAllowToFall(true);
	}

	// AI might need help using this transport in a good way.  Make the passengers aggressive.
	//There is no computer player check since Aggressive only means something for computer players anyway
	if( d->m_goAggressiveOnExit && rider->getAI() )
	{
		rider->getAI()->setAttitude( ATTITUDE_AGGRESSIVE );
	}
	if (getObject()->isEffectivelyDead()) {
		scatterToNearbyPosition(rider);
	}
	if( d->m_resetMoodCheckTimeOnExit && rider->getAI() )
	{
		rider->getAI()->wakeUpAndAttemptToTarget();
	}

	m_frameExitNotBusy = TheGameLogic->getFrame() + d->m_exitDelay;


  if ( d->m_armedRidersUpgradeWeaponSet )
    letRidersUpgradeWeaponSet();

	//Kris: October 20, 2003 - Patch 1.01
	//Force Jarmen Kell to transfer weapon timer for snipe to and from the combat bike.
	if( getObject()->isKindOf( KINDOF_CLIFF_JUMPER ) && rider->isKindOf( KINDOF_HERO ) && rider->isKindOf( KINDOF_SALVAGER ) )
	{
		//Admittedly brutal hack, but considering the state of the game (post-ship), this is a particularly surgical fix.
		Weapon *bikeWeapon = getObject()->getWeaponInWeaponSlot( SECONDARY_WEAPON );
		Weapon *riderWeapon = rider->getWeaponInWeaponSlot( SECONDARY_WEAPON );
		if( bikeWeapon && riderWeapon )
		{
			//Transfer the reload time from the bike to the rider
			riderWeapon->transferNextShotStatsFrom( *bikeWeapon );
		}
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TransportContain::createPayload()
{
	TransportContainModuleData* self = (TransportContainModuleData*)getTransportContainModuleData();

	Int count = self->m_initialPayload.count;
	const ThingTemplate* payloadTemplate = TheThingFactory->findTemplate( self->m_initialPayload.name );
	Object* object = getObject();
	ContainModuleInterface *contain = object->getContain();

	if( contain )
	{
		contain->enableLoadSounds( FALSE );
		for( int i = 0; i < count; i++ )
		{
			//We are creating a transport that comes with a initial payload, so add it now!
			Object* payload = TheThingFactory->newObject( payloadTemplate, object->getControllingPlayer()->getDefaultTeam() );
			if( contain->isValidContainerFor( payload, true ) )
			{
				contain->addToContain( payload );
			}
			else
			{
				DEBUG_CRASH( ( "DeliverPayload: PutInContainer %s is full, or not valid for the payload %s!", object->getName().str(), self->m_initialPayload.name.str() ) );
			}
		}
		contain->enableLoadSounds( TRUE );
	}

	m_payloadCreated = TRUE;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
UpdateSleepTime TransportContain::update()
{
	const TransportContainModuleData *moduleData = getTransportContainModuleData();

	if (m_payloadCreated == FALSE)
	{
#if RETAIL_COMPATIBLE_CRC
		createPayload();
#else
		// TheSuperHackers @bugfix Caball009 25/05/2026 Don't create payload
		// for destroyed object to avoid leaving the payload in an invalid state.
		if (!getObject()->isDestroyed())
		{
			createPayload();
		}
#endif
	}

	if( moduleData && moduleData->m_healthRegen )
	{
		ContainModuleInterface *contain = getObject()->getContain();
		if( contain )
		{
			//This transport has a health regeneration value, so go through and heal all inside.
			const ContainedItemsList* items = contain->getContainedItemsList();
			if( items )
			{
				ContainedItemsList::const_iterator it;
				it = items->begin();

				while( it != items->end() )
				{
					Object *object = *it;

					//Advance to the next iterator
					it++;

					//Determine if we need healing or not.
					BodyModuleInterface *body = object->getBodyModule();
					if( body->getHealth() < body->getMaxHealth() )
					{
						//Calculate the health to be regenerated on each unit.
						Real regen = body->getMaxHealth() * moduleData->m_healthRegen / 100.0f * SECONDS_PER_LOGICFRAME_REAL;

						//Perform the actual healing for this frame.
//						DamageInfo damageInfo;
//						damageInfo.in.m_damageType = DAMAGE_HEALING;
//						damageInfo.in.m_deathType = DEATH_NONE;
//						damageInfo.in.m_sourceID = getObject()->getID();
//						damageInfo.in.m_amount = regen;
//						object->attemptDamage( &damageInfo );
						object->attemptHealing( regen, getObject() );
					}
				}
			}
		}
	}
	// Tiberium Project @feature empty again after carrying someone: off the map
	if( moduleData && moduleData->m_leaveMapWhenEmpty )
		leaveMapWhenEmpty();

	return OpenContain::update(); //extend
}

//-------------------------------------------------------------------------------------------------
/** Tiberium Project @feature LeaveMapWhenEmpty: once it has carried someone and is empty, with no one on
	the way in or out, it heads for the nearest edge of the map and past it (as a cargo plane leaves), no
	longer selectable, and is gone once it is off the map (Kane's Wrath's summoned transports, its
	FlyOffMapOnEmpty). */
//-------------------------------------------------------------------------------------------------
void TransportContain::leaveMapWhenEmpty()
{
	Object *me = getObject();
	AIUpdateInterface *ai = me->getAI();
	if( ai == nullptr || me->isEffectivelyDead() || me->isDestroyed() )
		return;
	Bool start = FALSE;
	if( !m_leavingMap )
	{
		if( !m_hasCarried || getContainCount() > 0 || hasObjectsWantingToEnterOrExit() )
			return;
		m_leavingMap = TRUE;
		start = TRUE;
		// (no one picks it to order it about: it is on its way)
		me->setSelectable( FALSE );
	}

	// gone, once past the map's edge
	Region3D map;
	TheTerrainLogic->getExtentIncludingBorder( &map );
	if( !map.isInRegion( me->getPosition()->asCoord2D() ) )
	{
		TheGameLogic->destroyObject( me );
		return;
	}

	// (again each frame: a Chinook takes off on a locomotor of its own)
	if( ai->getCurLocomotor() != nullptr )
	{
		ai->getCurLocomotor()->setAllowInvalidPosition( true );
		ai->getCurLocomotor()->setUltraAccurate( true );
	}
	if( start || ai->isIdle() )
	{
		// to the nearest edge, and a little past the border
		const Coord3D *pos = me->getPosition();
		Coord3D to = *pos;
		const Real PAST = 200.0f;
		const Real toLoX = pos->x - map.lo.x, toHiX = map.hi.x - pos->x, toLoY = pos->y - map.lo.y, toHiY = map.hi.y - pos->y;
		const Real nearest = min( min( toLoX, toHiX ), min( toLoY, toHiY ) );
		if( nearest == toLoX )
			to.x = map.lo.x - PAST;
		else if( nearest == toHiX )
			to.x = map.hi.x + PAST;
		else if( nearest == toLoY )
			to.y = map.lo.y - PAST;
		else
			to.y = map.hi.y + PAST;
		ai->aiMoveToPosition( &to, CMD_FROM_AI );
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TransportContain::unreserveDoorForExit( ExitDoorType exitDoor )
{
	/* nothing */
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TransportContain::killRidersWhoAreNotFreeToExit()
{
	const TransportContainModuleData* d = getTransportContainModuleData();
	ContainedItemsList::const_iterator it = getContainList().begin();
	while( it != getContainList().end() )
	{
		// get the object
		Object *obj = *it;
		// increment the iterator, since death will pull this guy from the list somewhere else
		++it;

		if (!isSpecificRiderFreeToExit(obj)) // only TransportContain has a meaningful failure for isFreeToExit
		{
			// If we cannot exit this guy legally, kill the bastard before removeAllContained forces him out.
			if (d->m_destroyRidersWhoAreNotFreeToExit)
				TheGameLogic->destroyObject(obj);
			else
#if RETAIL_COMPATIBLE_CRC
				obj->kill();
#else
				// TheSuperHackers @info Burned death prevents infantry corpses dropping out of the container.
				obj->kill(DAMAGE_UNRESISTABLE, d->m_isBurnedDeathToUnits ? DEATH_BURNED : DEATH_NORMAL);
#endif
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool TransportContain::isSpecificRiderFreeToExit(Object* specificObject)
{
	if( specificObject == nullptr )
		return TRUE;	// I can, in general, exit people.

	// This is a override, not an extend.  I will check for game legality for
	// okaying the call to exitObjectViaDoor.
  const Object* me = getObject();

	// this is present solely for some transports to override, so that they can land before
	// allowing people to exit...
	const AIUpdateInterface* ai = me->getAIUpdateInterface();
	if (ai && ai->getAiFreeToExit(specificObject) != FREE_TO_EXIT)
		return FALSE;

#if !RETAIL_COMPATIBLE_CRC
	// TheSuperHackers @bugfix Stubbjax/bobtista 01/08/2026 If our container is itself contained,
	// then we are not free to exit.
	// GeneralsX @bugfix UnicodeApocalypse 12/09/2026 Except for special Overlord-style containers (e.g. Battle Bunker).
	if (me->isContained())
	{
		const ContainModuleInterface* outerContain = me->getContainedBy()->getContain();
		if (outerContain == nullptr || !outerContain->isSpecialOverlordStyleContainer())
			return FALSE;
	}
#endif

  // I can always kick people out if I am in the air, I know what I'm doing
  if (me->isUsingAirborneLocomotor())
   	return TRUE;

  const Coord3D *myPosition = me->getPosition();
 	if (!specificObject->getAIUpdateInterface())
		return FALSE;

	const Locomotor *hisLocomotor = specificObject->getAIUpdateInterface()->getCurLocomotor();
	if( hisLocomotor == nullptr )
   	return FALSE;

  // He can't get to this spot naturally, so I can't force him there.  (amphib transport)
  if (!TheAI->pathfinder()->validMovementTerrain(me->getLayer(), hisLocomotor, myPosition))
   	return FALSE;

  return TRUE;
}

Bool TransportContain::isPassengerAllowedToFire( ObjectID id ) const
{
	Object *passenger = TheGameLogic->findObjectByID(id);

	if( passenger != nullptr )
	{
		//only allow infantry, and turrets and such.  no vehicles.
		if( passenger->isKindOf(KINDOF_INFANTRY) == FALSE )
			return FALSE;
	}

  if ( ! getObject() )
    return FALSE;

  // but wait! I may be riding on an Overlord
  // This code detects the case of whether the contained passenger is in a bunker riding on an overlord, inside a helix!
  // Oh  my  God.
  const Object *heWhoContainsMe = getObject()->getContainedBy();
  if ( heWhoContainsMe)
  {
    ContainModuleInterface *hisContain = heWhoContainsMe->getContain();
    DEBUG_ASSERTCRASH( hisContain,("TransportContain::isPassengerAllowedToFire()... CONTAINER WITHOUT A CONTAIN! AARRGH!") );
    if ( hisContain && hisContain->isSpecialOverlordStyleContainer() )
      return hisContain->isPassengerAllowedToFire( id );
  }



	return OpenContain::isPassengerAllowedToFire();
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
ExitDoorType TransportContain::reserveDoorForExit( const ThingTemplate* objType, Object *specificObject )
{
	return isSpecificRiderFreeToExit(specificObject) ? DOOR_1 : DOOR_NONE_AVAILABLE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Bool TransportContain::isExitBusy() const	///< Contain style exiters are getting the ability to space out exits, so ask this before reserveDoor as a kind of no-commitment check.
{
	const TransportContainModuleData *data = getTransportContainModuleData();
	if( data->m_isDelayExitInAir && getObject()->isAboveTerrain() )
		return TRUE;

	return TheGameLogic->getFrame() < m_frameExitNotBusy;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void TransportContain::onCapture( Player *oldOwner, Player *newOwner )
{
	if( oldOwner != newOwner )
	{
		if( getObject()->isDisabledByType( DISABLED_UNMANNED ) )
		{
			//If this vehicle was sniped, then instantly eject everyone (otherwise, the next guy to eject will recapture it).
			removeAllContained();
		}
		else
		{
			//Use standard
#if RETAIL_COMPATIBLE_CRC
			orderAllPassengersToExit( CMD_FROM_AI, FALSE );
#else
      // TheSuperHackers @bugfix Stubbjax 20/11/2025 Only eject passengers if the new owner is not allied with the old owner.
			if (oldOwner->getRelationship(newOwner->getDefaultTeam()) != ALLIES)
				orderAllPassengersToExit(CMD_FROM_AI, FALSE);
#endif
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void TransportContain::crc( Xfer *xfer )
{

	// extend base class
	OpenContain::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void TransportContain::xfer( Xfer *xfer )
{

	// version
	// Tiberium Project: 2: carried someone, leaving the map (LeaveMapWhenEmpty)
	// Tiberium Project: 3: the places its riders take (PlaceUpgrades)
	XferVersion currentVersion = 3;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	OpenContain::xfer( xfer );

	// payload created
	xfer->xferBool( &m_payloadCreated );

	// extra slots in use
	xfer->xferInt( &m_extraSlotsInUse );

	// frame exit not busy
	xfer->xferUnsignedInt( &m_frameExitNotBusy );

	// Tiberium Project: LeaveMapWhenEmpty
	if( version >= 2 )
	{
		xfer->xferBool( &m_hasCarried );
		xfer->xferBool( &m_leavingMap );
	}

	// Tiberium Project: the places its riders take (PlaceUpgrades)
	if( version >= 3 )
	{
		for( Int p = 0; p < MAX_RIDER_PLACES; ++p )
			xfer->xferInt( &m_placeKind[ p ] );
		UnsignedShort count = (UnsignedShort)m_placeRiders.size();
		xfer->xferUnsignedShort( &count );
		if( xfer->getXferMode() == XFER_LOAD )
		{
			m_placeRiders.clear();
			for( UnsignedShort i = 0; i < count; ++i )
			{
				PlaceRider entry;
				xfer->xferObjectID( &entry.m_id );
				xfer->xferInt( &entry.m_place );
				m_placeRiders.push_back( entry );
			}
		}
		else
		{
			for( std::vector<PlaceRider>::iterator it = m_placeRiders.begin(); it != m_placeRiders.end(); ++it )
			{
				xfer->xferObjectID( &it->m_id );
				xfer->xferInt( &it->m_place );
			}
		}
	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void TransportContain::loadPostProcess()
{

	// extend base class
	OpenContain::loadPostProcess();

}
