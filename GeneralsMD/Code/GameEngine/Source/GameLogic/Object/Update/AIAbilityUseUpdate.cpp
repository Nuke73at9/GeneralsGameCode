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

// FILE: AIAbilityUseUpdate.cpp ///////////////////////////////////////////////////////////////////
// Tiberium Project @feature the skirmish AI's use of a unit's abilities (see AIAbilityUseUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/AIAbilityUseUpdate.h"

#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"

static const char *const TheAIAbilityHeuristicNames[] =
{
	"IN_COMBAT",
	"AT_VICTIM",
	"TOGGLE",
	"TRANSFORM",
	nullptr
};
static_assert(ARRAY_SIZE(TheAIAbilityHeuristicNames) == AI_ABILITY_HEURISTIC_COUNT + 1, "Incorrect array size");

//-------------------------------------------------------------------------------------------------
/** what a weapon set's weapons can hit (their anti mask), and the longest reach among them */
static Int weaponSetAntiMask( const WeaponTemplateSet *set, Real &reach )
{
	Int mask = 0;
	if( set == nullptr )
		return 0;
	for( Int slot = PRIMARY_WEAPON; slot < WEAPONSLOT_COUNT; ++slot )
	{
		const WeaponTemplate *weapon = set->getNth( (WeaponSlotType)slot );
		if( weapon == nullptr )
			continue;
		mask |= weapon->getAntiMask();
		if( weapon->getUnmodifiedAttackRange() > reach )
			reach = weapon->getUnmodifiedAttackRange();
	}
	return mask;
}

//-------------------------------------------------------------------------------------------------
/** TRANSFORM: whether the toggle's other state (the ABILITY_TOGGLED weapon set flipped) suits what is
	near: the nearest enemy, within the longer reach of the two states' weapons, that either state can
	hit decides; one the weapons now can hit keeps the unit as it is */
static Bool otherStateSuits( const Object *me )
{
	WeaponSetFlags other = me->getWeaponSetFlags();
	other.set( WEAPONSET_ABILITY_TOGGLED, me->testWeaponSetFlag( WEAPONSET_ABILITY_TOGGLED ) ? 0 : 1 );
	Real reach = 0.0f;
	const Int now = weaponSetAntiMask( me->getTemplate()->findWeaponTemplateSet( me->getWeaponSetFlags() ), reach );
	const Int then = weaponSetAntiMask( me->getTemplate()->findWeaponTemplateSet( other ), reach );
	if( then == 0 || then == now || reach <= 0.0f )
		return FALSE;
	PartitionFilterRelationship enemies( me, PartitionFilterRelationship::ALLOW_ENEMIES );
	PartitionFilterAlive alive;
	PartitionFilterSameMapStatus sameMap( me );
	PartitionFilterStealthedAndUndetected seen( me, FALSE );
	PartitionFilter *filters[] = { &enemies, &alive, &sameMap, &seen, nullptr };
	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( me, reach, FROM_BOUNDINGSPHERE_2D, filters, ITER_SORTED_NEAR_TO_FAR );
	MemoryPoolObjectHolder hold( iter );
	for( Object *o = iter->first(); o != nullptr; o = iter->next() )
	{
		const Int need = WeaponSet::getVictimAntiMaskFor( o );
		if( ( now & need ) != 0 )
			return FALSE;
		if( ( then & need ) != 0 )
			return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** Power = SpecialPowerTemplate Heuristic */
void AIAbilityUseUpdateModuleData::parsePower( INI *ini, void *instance, void * /*store*/, const void * /*userData*/ )
{
	AIAbilityUseUpdateModuleData *self = (AIAbilityUseUpdateModuleData *)instance;
	Power power;
	power.power = nullptr;
	INI::parseSpecialPowerTemplate( ini, nullptr, &power.power, nullptr );
	power.heuristic = (AIAbilityHeuristic)INI::scanIndexList( ini->getNextToken(), TheAIAbilityHeuristicNames );
	if( power.power != nullptr && self->m_powers.size() < 32 )
		self->m_powers.push_back( power );
}

//-------------------------------------------------------------------------------------------------
void AIAbilityUseUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "Power",			parsePower,											nullptr, 0 },
		{ "ScanDelay",	INI::parseDurationUnsignedInt,	nullptr, offsetof( AIAbilityUseUpdateModuleData, m_scanDelay ) },
		{ "CalmTime",		INI::parseDurationUnsignedInt,	nullptr, offsetof( AIAbilityUseUpdateModuleData, m_calmTime ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
AIAbilityUseUpdate::AIAbilityUseUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_lastFightFrame = 0;
	m_toggledMask = 0;
	setWakeFrame( getObject(), UPDATE_SLEEP( getAIAbilityUseUpdateModuleData()->m_scanDelay ) );
}

//-------------------------------------------------------------------------------------------------
AIAbilityUseUpdate::~AIAbilityUseUpdate()
{
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime AIAbilityUseUpdate::update()
{
	const AIAbilityUseUpdateModuleData *data = getAIAbilityUseUpdateModuleData();
	const UnsignedInt delay = data->m_scanDelay > 0 ? data->m_scanDelay : 1;
	Object *me = getObject();
	const Player *owner = me->getControllingPlayer();
	AIUpdateInterface *ai = me->getAI();
	if( owner == nullptr || owner->getPlayerType() != PLAYER_COMPUTER || ai == nullptr || me->isEffectivelyDead() )
		return UPDATE_SLEEP( delay * 5 );	// (it may change hands)
	if( me->isDisabled() || me->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		return UPDATE_SLEEP( delay );

	// fighting: attacking something alive within reach of a weapon
	const UnsignedInt now = TheGameLogic->getFrame();
	Object *victim = ai->isAttacking() ? ai->getCurrentVictim() : nullptr;
	if( victim != nullptr && victim->isEffectivelyDead() )
		victim = nullptr;
	if( victim != nullptr )
	{
		const Weapon *weapon = me->getCurrentWeapon();
		const Real reach = weapon != nullptr ? weapon->getAttackRange( me ) + me->getGeometryInfo().getBoundingCircleRadius() : 0.0f;
		const Coord3D *a = me->getPosition();
		const Coord3D *b = victim->getPosition();
		const Real dx = a->x - b->x, dy = a->y - b->y;
		if( weapon == nullptr || dx * dx + dy * dy > sqr( reach + victim->getGeometryInfo().getBoundingCircleRadius() ) )
			victim = nullptr;
	}
	if( victim != nullptr )
		m_lastFightFrame = now;
	const Bool calm = victim == nullptr && now - m_lastFightFrame > data->m_calmTime;

	for( size_t i = 0; i < data->m_powers.size(); ++i )
	{
		const AIAbilityUseUpdateModuleData::Power &power = data->m_powers[i];
		SpecialPowerModuleInterface *module = me->getSpecialPowerModule( power.power );
		if( module == nullptr || !module->isReady() )
			continue;
		const UnsignedInt bit = 1u << i;
		switch( power.heuristic )
		{
			case AI_ABILITY_IN_COMBAT:
				if( victim != nullptr )
					module->doSpecialPower( 0 );
				break;
			case AI_ABILITY_AT_VICTIM:
				if( victim != nullptr )
				{
					const SpecialPowerType type = power.power->getSpecialPowerType();
					if( type == SPECIAL_ABILITY_AT_LOCATION )
						module->doSpecialPowerAtLocation( victim->getPosition(), INVALID_ANGLE, 0 );
					else if( type == SPECIAL_ABILITY_AT_OBJECT )
						module->doSpecialPowerAtObject( victim, 0 );
					else
						module->doSpecialPower( 0 );
				}
				break;
			case AI_ABILITY_TOGGLE:
				if( victim != nullptr && ( m_toggledMask & bit ) == 0 )
				{
					module->doSpecialPower( 0 );
					m_toggledMask |= bit;
				}
				else if( calm && ( m_toggledMask & bit ) != 0 )
				{
					module->doSpecialPower( 0 );
					m_toggledMask &= ~bit;
				}
				break;
			case AI_ABILITY_TRANSFORM:
				// (in a fight its weapons already hit what it fights)
				if( victim == nullptr && otherStateSuits( me ) )
					module->doSpecialPower( 0 );
				break;
			default:
				break;
		}
	}
	return UPDATE_SLEEP( delay );
}

//-------------------------------------------------------------------------------------------------
void AIAbilityUseUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void AIAbilityUseUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	UpdateModule::xfer( xfer );

	xfer->xferUnsignedInt( &m_lastFightFrame );
	xfer->xferUnsignedInt( &m_toggledMask );
}

//-------------------------------------------------------------------------------------------------
void AIAbilityUseUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
