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
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"

static const char *const TheAIAbilityHeuristicNames[] =
{
	"IN_COMBAT",
	"AT_VICTIM",
	"TOGGLE",
	nullptr
};
static_assert(ARRAY_SIZE(TheAIAbilityHeuristicNames) == AI_ABILITY_HEURISTIC_COUNT + 1, "Incorrect array size");

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
