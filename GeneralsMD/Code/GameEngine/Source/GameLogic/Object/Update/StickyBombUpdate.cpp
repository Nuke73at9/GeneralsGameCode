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

// FILE: StickyBombUpdate.cpp //////////////////////////////////////////////////////////////////////
// Author: Kris Morness, July 2002
// Desc:   Updates stickybomb to stay on target while it counts down to detonation (lifetime update)
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/StickyBombUpdate.h"

#include <algorithm>

#include "Common/ThingTemplate.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/LifetimeUpdate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/GameLogic.h"

// PRIVATE ////////////////////////////////////////////////////////////////////////////////////////

// Tiberium Project @feature the stuck objects that mark or shield their targets (TargetStatus, AbsorbTargetDamage,
// TargetShieldTint), in the order they were made
static std::vector<StickyBombUpdate *> s_markers;
static Bool s_absorbing = FALSE;	///< a shield is taking a hit for its target (the hit is its own)

static Bool marksTarget( const StickyBombUpdateModuleData *d )
{
	return d->m_targetStatus.any() || d->m_absorbTargetDamage || d->m_targetShieldTint || d->m_onePerTarget;
}

// PUBLIC /////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
StickyBombUpdate::StickyBombUpdate( Thing *thing, const ModuleData *moduleData ) : UpdateModule( thing, moduleData )
{
	m_targetID		= INVALID_ID;
	m_dieFrame		= 0;
	m_nextPingFrame = 0;
	m_triggerFrame = 0;	// Tiberium Project
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
	if( marksTarget( getStickyBombUpdateModuleData() ) )
		s_markers.push_back( this );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
StickyBombUpdate::~StickyBombUpdate()
{
	std::vector<StickyBombUpdate *>::iterator it = std::find( s_markers.begin(), s_markers.end(), this );
	if( it != s_markers.end() )
		s_markers.erase( it );
}

//-------------------------------------------------------------------------------------------------
/** Tiberium Project @feature the target's TargetStatus and shielded look while this is stuck to it; cleared, what
	another stuck to it still gives it stays */
void StickyBombUpdate::markTarget( Bool on )
{
	const StickyBombUpdateModuleData *d = getStickyBombUpdateModuleData();
	Object *target = getTargetObject();
	if( target == nullptr )
		return;
	// Tiberium Project @feature its target's TargetModifier while it is there, put on and taken off as from this one
	// (another stuck to it keeps its own)
	if( d->m_targetModifier != NAMEKEY_INVALID && TheAttributeModifierStore != nullptr )
	{
		const AttributeModifierTemplate *tmpl = TheAttributeModifierStore->find( d->m_targetModifier );
		if( tmpl != nullptr && on )
			target->applyAttributeModifier( tmpl, getObject()->getID() );
		else if( tmpl != nullptr )
			target->removeAttributeModifier( tmpl, getObject()->getID() );
	}
	if( !d->m_targetStatus.any() && !d->m_targetShieldTint )
		return;
	Drawable *draw = target->getDrawable();
	if( on )
	{
		if( d->m_targetStatus.any() )
			target->setStatus( d->m_targetStatus );
		if( d->m_targetShieldTint && draw != nullptr )
			draw->setTintStatus( TINT_STATUS_SHIELDED );
		return;
	}
	ObjectStatusMaskType clear = d->m_targetStatus;
	Bool tint = d->m_targetShieldTint;
	for( size_t i = 0; i < s_markers.size(); ++i )
	{
		const StickyBombUpdate *other = s_markers[i];
		if( other == this || other->m_targetID != m_targetID )
			continue;
		const StickyBombUpdateModuleData *od = other->getStickyBombUpdateModuleData();
		clear.clear( od->m_targetStatus );
		if( od->m_targetShieldTint )
			tint = FALSE;
	}
	if( clear.any() )
		target->clearStatus( clear );
	if( tint && draw != nullptr )
		draw->clearTintStatus( TINT_STATUS_SHIELDED );
}

//-------------------------------------------------------------------------------------------------
/** Tiberium Project @feature a shield stuck to the victim takes the hit, whole, by its own armour (C&C4's
	ABSORB_ALL_DAMAGE LEECH_DAMAGE_FROM_PARENT); a shield takes no hit but those it takes for its target */
Bool StickyBombUpdate::absorbDamage( Object *victim, const DamageInfo *damageInfo )
{
	if( s_markers.empty() || victim == nullptr || s_absorbing )
		return FALSE;
	const ObjectID id = victim->getID();
	for( size_t i = 0; i < s_markers.size(); ++i )
	{
		StickyBombUpdate *sticky = s_markers[i];
		const StickyBombUpdateModuleData *d = sticky->getStickyBombUpdateModuleData();
		if( !d->m_absorbTargetDamage )
			continue;
		Object *shield = sticky->getObject();
		if( shield == victim )
			return TRUE;
		if( sticky->m_targetID != id || shield->isEffectivelyDead() || !getDamageTypeFlag( d->m_absorbDamageTypes, damageInfo->in.m_damageType ) )
			continue;
		DamageInfo taken;
		taken.in = damageInfo->in;
		s_absorbing = TRUE;
		shield->attemptDamage( &taken );
		s_absorbing = FALSE;
		return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void StickyBombUpdate::onObjectCreated()
{
	//This first step is an initialization step. Immediately after the stickybomb is created,
	//the producerID is stored in the object to signify the shooter of this weapon. From there,
	//we can get the target. The target information will be used to determine where the sticky
	//bomb will go.
	ObjectID shooterID = getObject()->getProducerID();
	Object* shooter = TheGameLogic->findObjectByID( shooterID );
	if( shooter )
	{
		//Find the shooters target!
		AIUpdateInterface *ai = shooter->getAIUpdateInterface();
		if( ai )
		{
			Object *target = ai->getGoalObject();
			if( target )
			{
				initStickyBomb( target, nullptr);
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void StickyBombUpdate::initStickyBomb( Object *target, const Object *bomber, const Coord3D *specificPos )
{
	// Tiberium Project @feature what it gave the target it was on before is taken back (an OCL's object, stuck first to
	// its maker's goal, then to the weapon's victim)
	if( m_targetID != INVALID_ID && ( target == nullptr || target->getID() != m_targetID ) )
		markTarget( FALSE );

	//Store the target.
	m_targetID = target ? target->getID() : INVALID_ID;
	markTarget( TRUE );

	// Tiberium Project @feature one of its kind on a target (OnePerTarget): those of its kind already stuck there die, a death
	// not of old age (C&C4's DETACH_WHEN_MULTIPLE_ARE_ATTACHED: a second sonic charge sets the first one's bonus blast off)
	if( getStickyBombUpdateModuleData()->m_onePerTarget && m_targetID != INVALID_ID )
	{
		std::vector<Object *> older;
		for( size_t i = 0; i < s_markers.size(); ++i )
		{
			StickyBombUpdate *other = s_markers[i];
			Object *o = other->getObject();
			if( other != this && other->m_targetID == m_targetID && !o->isEffectivelyDead()
				&& o->getTemplate()->isEquivalentTo( getObject()->getTemplate() ) )
				older.push_back( o );
		}
		for( size_t i = 0; i < older.size(); ++i )
			older[i]->kill();
	}

	//Set the target as the producer ID -- so we can access it from other areas.
	// Tiberium Project @feature unless it keeps the one who made it (KeepProducer)
	if( !getStickyBombUpdateModuleData()->m_keepProducer )
		getObject()->setProducer( target );
	UnsignedInt now = TheGameLogic->getFrame();

	//Also determine our lifetime.... for countdown purposes.
	static NameKeyType key_LifetimeUpdate = NAMEKEY( "LifetimeUpdate" );
	LifetimeUpdate *update = (LifetimeUpdate*)getObject()->findUpdateModule( key_LifetimeUpdate );
	if( update )
	{
		//we are a timer bomb
		m_dieFrame = update->getDieFrame();

		//Calculate the number of seconds (rounded down)
		UnsignedInt pings = (m_dieFrame - now) / LOGICFRAMES_PER_SECOND;
		//Now determine the next frame we will make a "ping" sound.
		m_nextPingFrame = m_dieFrame - (pings * LOGICFRAMES_PER_SECOND);
	}
	else
	{
		//we are a remotely triggered bomb.
		m_dieFrame = 0;

		//Because we don't die -- make our ping in a second.
		m_nextPingFrame = now + LOGICFRAMES_PER_SECOND;
	}
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );

	if( target )
	{
		const StickyBombUpdateModuleData* d = getStickyBombUpdateModuleData();
		Coord3D pos = *target->getPosition();

		if( specificPos )
		{
			pos = *specificPos;
			pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
		}
		else if(target->isKindOf( KINDOF_IMMOBILE ) && bomber )
		{
			// make this exception, if bomber has placed bomb on a structure
			// let the bomb just stay where it was first put, so a mine clearing unit can get to it later
			pos = *bomber->getPosition();
			pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
			//keep it at ground height for mine clearing units to reach
		}
		else
			pos.z += d->m_offsetZ; // ride on the roof of the truck/tank

		getObject()->setPosition( &pos );

		if( getObject()->isKindOf(KINDOF_BOOBY_TRAP) )
		{
			// This kind of sticky bomb needs to set a status, so the poor victim can trigger us from assorted places
			target->setStatus( MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_BOOBY_TRAPPED) );
		}

		AudioEventRTS soundCreateBomb = *(getObject()->getTemplate()->getPerUnitSound("StickyBombCreated"));
		soundCreateBomb.setPosition( getObject()->getPosition() );
		TheAudio->addAudioEvent(&soundCreateBomb);

	}
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime StickyBombUpdate::update()
{
	// Continually reset position of stickybomb to match the position of the target.
	const Object *target = getTargetObject();
	Object *self = getObject();
	// Tiberium Project @feature set off a while ago (TriggerDelay): it goes off now
	if( m_triggerFrame != 0 && TheGameLogic->getFrame() >= m_triggerFrame && !self->isEffectivelyDead() )
	{
		m_triggerFrame = 0;
		detonate();
		return UPDATE_SLEEP_FOREVER;
	}
	// Tiberium Project @feature a shield whose target is gone goes too
	if( target == nullptr && m_targetID != INVALID_ID && getStickyBombUpdateModuleData()->m_absorbTargetDamage )
	{
		TheGameLogic->destroyObject( self );
		return UPDATE_SLEEP_FOREVER;
	}
	if( target )
	{
		if( target->isEffectivelyDead() )
		{
			// Tiberium Project @feature a charge that dies with its target (DieWithTarget): its death weapons and
			// effects play where it was
			if( getStickyBombUpdateModuleData()->m_dieWithTarget )
			{
				if( !self->isEffectivelyDead() )
					self->kill();
				return UPDATE_SLEEP_FOREVER;
			}
			//If the target is dead, then
			TheGameLogic->destroyObject( self );
			return UPDATE_SLEEP_NONE;
		}

		// Tiberium Project @feature it dies once its target has any of TargetForbiddenStatus (C&C4's
		// DIE_WHEN_PARENT_HAS_FORBIDDEN_STATUS), or lacks any of TargetRequiredStatus (the Aftershock's Tiberium cloud is
		// gone as the Aftershock packs up, out of DEPLOYED)
		const ObjectStatusMaskType &forbidden = getStickyBombUpdateModuleData()->m_targetForbiddenStatus;
		const ObjectStatusMaskType &required = getStickyBombUpdateModuleData()->m_targetRequiredStatus;
		if( ( forbidden.any() && target->getStatusBits().testForAny( forbidden ) )
			|| ( required.any() && !target->getStatusBits().testForAll( required ) ) )
		{
			if( !self->isEffectivelyDead() )
				self->kill();
			return UPDATE_SLEEP_FOREVER;
		}

		if ( target->isKindOf( KINDOF_IMMOBILE) )
		{
			const Coord3D *pos = self->getPosition();
			Coord3D newPos;
			newPos.x = pos->x;
			newPos.y = pos->y;
			newPos.z = TheTerrainLogic->getGroundHeight(newPos.x, newPos.y);
			//keep it at ground height for mine clearing units to reach

			self->setPosition( &newPos );
		}
		else // make the bomb follow the target around
		{
			const StickyBombUpdateModuleData* d = getStickyBombUpdateModuleData();
			const Coord3D *pos = target->getPosition();
			Coord3D newPos;
			newPos.x = pos->x;
			newPos.y = pos->y;
			newPos.z = pos->z + d->m_offsetZ;

			self->setPosition( &newPos );
		}
	}

	UnsignedInt now = TheGameLogic->getFrame();
	if( now >= m_nextPingFrame )
	{
		m_nextPingFrame += LOGICFRAMES_PER_SECOND;

		//Play the "ping" sound.
		AudioEventRTS sound = *self->getTemplate()->getPerUnitSound( "UnitBombPing" );
		sound.setObjectID( self->getID() );
		TheAudio->addAudioEvent( &sound );
	}

	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
Object* StickyBombUpdate::getTargetObject() const
{
	return TheGameLogic->findObjectByID( m_targetID );
}

//-------------------------------------------------------------------------------------------------
void StickyBombUpdate::setTargetObject( Object *obj )
{
	m_targetID = obj ? obj->getID() : INVALID_ID;
}

//-------------------------------------------------------------------------------------------------
void StickyBombUpdate::trigger()
{
	// Tiberium Project @feature (the trap's beep: SAGE's weapon's PreAttackDelay) goes off TriggerDelay after it is set off
	const UnsignedInt delay = getStickyBombUpdateModuleData()->m_triggerDelay;
	if( delay == 0 )
	{
		detonate();
		return;
	}
	if( m_triggerFrame == 0 )
	{
		m_triggerFrame = TheGameLogic->getFrame() + delay;
		setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
	}
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature the trap on a container set off by one not allied to it now inside (TriggerAfterEntry: SAGE's
// BoobyTrapUpdate on a garrison, its GARRISONED); the ones that go off as they try to get in are Object's
// checkAndDetonateBoobyTrap's
//-------------------------------------------------------------------------------------------------
void StickyBombUpdate::setOffAfterEntry( Object *container, const Object *rider )
{
	if( container == nullptr || rider == nullptr || !container->testStatus( OBJECT_STATUS_BOOBY_TRAPPED ) )
		return;
	static const NameKeyType key_StickyBombUpdate = NAMEKEY( "StickyBombUpdate" );
	PartitionFilterAcceptByKindOf kindFilter( MAKE_KINDOF_MASK( KINDOF_BOOBY_TRAP ), KINDOFMASK_NONE );
	PartitionFilterSameMapStatus filterMapStatus( container );
	PartitionFilter *filters[] = { &kindFilter, &filterMapStatus, nullptr };
	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( container->getPosition(),
		25.0f + container->getGeometryInfo().getBoundingCircleRadius(), FROM_CENTER_2D, filters, ITER_SORTED_NEAR_TO_FAR );
	MemoryPoolObjectHolder hold( iter );
	for( Object *other = iter->first(); other != nullptr; other = iter->next() )
	{
		if( other->getProducerID() != container->getID() )
			continue;
		StickyBombUpdate *update = (StickyBombUpdate *)other->findUpdateModule( key_StickyBombUpdate );
		if( update == nullptr || !update->triggersAfterEntry() )
			return;
		if( other->getControllingPlayer() != nullptr && rider->getTeam() != nullptr
			&& other->getControllingPlayer()->getRelationship( rider->getTeam() ) == ALLIES )
			return;
		update->trigger();
		return;
	}
}

//-------------------------------------------------------------------------------------------------
void StickyBombUpdate::detonate()
{
	const StickyBombUpdateModuleData *data = getStickyBombUpdateModuleData();

	Object* boobyTrappedObject = getTargetObject();
	// Tiberium Project @feature its weapon at what it is on (TargetWeapon: SAGE's BoobyTrapUpdate's SpecialWeapon at its parent)
	if( data->m_targetWeaponTemplate != nullptr && boobyTrappedObject != nullptr && !boobyTrappedObject->isEffectivelyDead() )
		TheWeaponStore->createAndFireTempWeapon( data->m_targetWeaponTemplate, getObject(), boobyTrappedObject );
	if( data->m_geometryBasedDamageWeaponTemplate )
	{
		// We need to hurt people based on the size of the thing we are on.  The radius in our weapon
		// is the radius beyond our borders
		if( boobyTrappedObject )
		{
			WeaponBonus nullBonus;
			Real boundingCircle = boobyTrappedObject->getGeometryInfo().getBoundingCircleRadius();
			Real primaryDamage = data->m_geometryBasedDamageWeaponTemplate->getPrimaryDamage(nullBonus);
			Real secondaryDamage = data->m_geometryBasedDamageWeaponTemplate->getSecondaryDamage(nullBonus);
			Real primaryDamageRange = data->m_geometryBasedDamageWeaponTemplate->getPrimaryDamageRadius(nullBonus);
			Real secondaryDamageRange = data->m_geometryBasedDamageWeaponTemplate->getSecondaryDamageRadius(nullBonus);
			primaryDamageRange += boundingCircle;
			secondaryDamageRange += boundingCircle;
			Real primaryDamageRangeSqr = sqr(primaryDamageRange);
			Real radius = max(primaryDamageRange, secondaryDamageRange);

			SimpleObjectIterator *iter;
			iter = ThePartitionManager->iterateObjectsInRange(boobyTrappedObject->getPosition(), radius, FROM_BOUNDINGSPHERE_3D);
			MemoryPoolObjectHolder hold(iter);
			Real curVictimDistSqr;
			Object *curVictim = iter->firstWithNumeric(&curVictimDistSqr);
			DamageInfo damageInfo;
			damageInfo.in.m_damageType = data->m_geometryBasedDamageWeaponTemplate->getDamageType();
			damageInfo.in.m_deathType = data->m_geometryBasedDamageWeaponTemplate->getDeathType();
			damageInfo.in.m_sourceID = getObject()->getID();
			damageInfo.in.m_sourcePlayerMask = getObject()->getControllingPlayer()->getPlayerMask();
			damageInfo.in.m_damageStatusType = data->m_geometryBasedDamageWeaponTemplate->getDamageStatusType();

			for (; curVictim != nullptr; curVictim = iter ? iter->nextWithNumeric(&curVictimDistSqr) : nullptr)
			{
				damageInfo.in.m_amount = (curVictimDistSqr <= primaryDamageRangeSqr) ? primaryDamage : secondaryDamage;
				curVictim->attemptDamage(&damageInfo);
			}

			if( data->m_geometryBasedDamageFX )
			{
				// And we make FX based on that size too.
				FXList::doFXPos(data->m_geometryBasedDamageFX, boobyTrappedObject->getPosition(), nullptr, 0, nullptr, secondaryDamageRange);
			}
		}
	}

#if RETAIL_COMPATIBLE_CRC
	if( getObject()->isKindOf(KINDOF_BOOBY_TRAP) && boobyTrappedObject )
	{
		// This kind of sticky bomb needs to set a status, so the poor victim can trigger us from assorted places
		boobyTrappedObject->clearStatus( MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_BOOBY_TRAPPED) );
	}
#endif

	getObject()->kill();// Most things just fire weapons in their death modules
}

void StickyBombUpdate::onDelete()
{
#if !RETAIL_COMPATIBLE_CRC
	// TheSuperHackers @bugfix Stubbjax 05/08/2026 Clear booby trap status when destroyed, not just when detonated.
	if (getObject()->isKindOf(KINDOF_BOOBY_TRAP))
	{
		Object* boobyTrappedObject = getTargetObject();

		// This kind of sticky bomb needs to set a status, so the poor victim can trigger us from assorted places
		if (boobyTrappedObject)
			boobyTrappedObject->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_BOOBY_TRAPPED));
	}
#endif
	// Tiberium Project @feature what it gave its target goes with it
	markTarget( FALSE );
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void StickyBombUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void StickyBombUpdate::xfer( Xfer *xfer )
{

	// version (Tiberium Project: 2: when it goes off, once set off)
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// target id
	xfer->xferObjectID( &m_targetID );

	// die frame
	xfer->xferUnsignedInt( &m_dieFrame );

	//Next frame that a ping sound will play.
	xfer->xferUnsignedInt( &m_nextPingFrame );

	if( version >= 2 )
		xfer->xferUnsignedInt( &m_triggerFrame );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void StickyBombUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
