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

// FILE: AssembleStructureUpdate.cpp //////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's AssembleStructureSpecialAbilityUpdate (see AssembleStructureUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/AssembleStructureUpdate.h"

#include "Common/BuildAssistant.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CreateModule.h"

//-------------------------------------------------------------------------------------------------
AssembleStructureUpdate::AssembleStructureUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	m_stage = STAGE_NONE;
	m_goal.zero();
	m_angle = 0.0f;
	m_structureID = INVALID_ID;
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
AssembleStructureUpdate::~AssembleStructureUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool AssembleStructureUpdate::canPlace() const
{
	const AssembleStructureUpdateModuleData *data = getAssembleStructureUpdateModuleData();
	const Object *me = getObject();
	Player *player = me->getControllingPlayer();
	const ThingTemplate *what = TheThingFactory->findTemplate( data->m_structureName );
	if( what == nullptr || player == nullptr )
		return FALSE;
	if( player->getMoney()->countMoney() < (UnsignedInt)what->calcCostToBuild( player ) )
		return FALSE;
	Coord3D spot = m_goal;
	return TheBuildAssistant->isLocationLegalToBuild( &spot, what, m_angle,
		BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP, me, player ) == LBC_OK;
}

//-------------------------------------------------------------------------------------------------
Bool AssembleStructureUpdate::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const AssembleStructureUpdateModuleData *data = getAssembleStructureUpdateModuleData();
	if( specialPowerTemplate != data->m_specialPowerTemplate )
		return FALSE;
	Object *me = getObject();
	AIUpdateInterface *ai = me->getAI();
	if( ai == nullptr || me->isEffectivelyDead() || m_stage == STAGE_BUILDING )
		return FALSE;
	if( targetObj == nullptr && targetPos == nullptr )
		return FALSE;
	m_goal = targetObj != nullptr ? *targetObj->getPosition() : *targetPos;
	m_angle = me->getOrientation();
	if( !canPlace() )
		return FALSE;

	const Coord3D *pos = me->getPosition();
	const Real dx = pos->x - m_goal.x, dy = pos->y - m_goal.y;
	if( dx * dx + dy * dy > sqr( data->m_startAbilityRange ) )
	{
		m_stage = STAGE_MOVING;
		ai->aiMoveToPosition( &m_goal, CMD_FROM_AI );
		setWakeFrame( me, UPDATE_SLEEP_NONE );
		return TRUE;
	}
	place();
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** the structure, unbuilt, paid for, at the spot (as a dozer starts one) */
//-------------------------------------------------------------------------------------------------
void AssembleStructureUpdate::place()
{
	const AssembleStructureUpdateModuleData *data = getAssembleStructureUpdateModuleData();
	Object *me = getObject();
	Player *player = me->getControllingPlayer();
	const ThingTemplate *what = TheThingFactory->findTemplate( data->m_structureName );
	m_stage = STAGE_NONE;
	if( what == nullptr || player == nullptr || !canPlace() )
		return;

	Object *obj = TheThingFactory->newObject( what, player->getDefaultTeam(), MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION ) );
	obj->setProducer( me );
	obj->setBuilder( me );
	player->getMoney()->withdraw( what->calcCostToBuild( player ) );
	obj->setPosition( &m_goal );
	obj->setOrientation( m_angle );
	TheTerrainLogic->flattenTerrain( obj );
	Coord3D ground = m_goal;
	ground.z = TheTerrainLogic->getGroundHeight( ground.x, ground.y );
	obj->setPosition( &ground );
	TheAI->pathfinder()->addObjectToPathfindMap( obj );
	player->onStructureCreated( me, obj );
	obj->setConstructionPercent( 0.0f );
	BodyModuleInterface *body = obj->getBodyModule();
	if( body != nullptr )
		body->internalChangeHealth( -body->getHealth() + 1.0f );
	obj->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK( MODELCONDITION_AWAITING_CONSTRUCTION ),
		MAKE_MODELCONDITION_MASK2( MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED ) );

	m_structureID = obj->getID();
	m_stage = STAGE_BUILDING;
	if( me->getAI() != nullptr )
		me->getAI()->aiIdle( CMD_FROM_AI );
	me->setModelConditionState( MODELCONDITION_ACTIVELY_CONSTRUCTING );
	setWakeFrame( me, UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
void AssembleStructureUpdate::stop()
{
	Object *me = getObject();
	if( m_stage == STAGE_BUILDING )
	{
		me->clearModelConditionState( MODELCONDITION_ACTIVELY_CONSTRUCTING );
		Object *obj = TheGameLogic->findObjectByID( m_structureID );
		if( obj != nullptr && obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
			obj->clearModelConditionState( MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED );
	}
	m_stage = STAGE_NONE;
	m_structureID = INVALID_ID;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime AssembleStructureUpdate::update()
{
	const AssembleStructureUpdateModuleData *data = getAssembleStructureUpdateModuleData();
	Object *me = getObject();
	AIUpdateInterface *ai = me->getAI();
	if( m_stage == STAGE_NONE )
		return UPDATE_SLEEP_FOREVER;
	// another order (the player's) calls it off
	if( ai == nullptr || me->isEffectivelyDead() || ai->getLastCommandSource() != CMD_FROM_AI )
	{
		stop();
		return UPDATE_SLEEP_FOREVER;
	}

	if( m_stage == STAGE_MOVING )
	{
		const Coord3D *pos = me->getPosition();
		const Real dx = pos->x - m_goal.x, dy = pos->y - m_goal.y;
		if( dx * dx + dy * dy <= sqr( data->m_startAbilityRange ) )
		{
			ai->aiIdle( CMD_FROM_AI );
			place();
			return m_stage == STAGE_NONE ? UPDATE_SLEEP_FOREVER : UPDATE_SLEEP_NONE;
		}
		if( ai->isIdle() )
		{
			stop();	// stopped short of it
			return UPDATE_SLEEP_FOREVER;
		}
		return UPDATE_SLEEP( LOGICFRAMES_PER_SECOND / 4 );
	}

	// building: as a dozer does, a share of it and of its health each frame
	Object *obj = TheGameLogic->findObjectByID( m_structureID );
	if( obj == nullptr || obj->isEffectivelyDead() || !obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
	{
		stop();
		return UPDATE_SLEEP_FOREVER;
	}
	if( me->isDisabled() )
		return UPDATE_SLEEP_NONE;
	const Int framesToBuild = obj->getTemplate()->calcTimeToBuild( me->getControllingPlayer() );
	obj->setConstructionPercent( obj->getConstructionPercent() + WWMath::Div_FixNaN( 100.0f, (Real)framesToBuild, 100.0f ) );
	BodyModuleInterface *body = obj->getBodyModule();
	if( body != nullptr && framesToBuild > 0 )
		body->internalChangeHealth( body->getMaxHealth() / INT_TO_REAL( framesToBuild ) );
	if( obj->getConstructionPercent() < 100.0f )
		return UPDATE_SLEEP_NONE;

	// done (as a dozer's building is)
	obj->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION ) );
	obj->clearModelConditionFlags( MAKE_MODELCONDITION_MASK3( MODELCONDITION_AWAITING_CONSTRUCTION, MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED ) );
	obj->setConstructionPercent( CONSTRUCTION_COMPLETE );
	if( body != nullptr )
		body->evaluateVisualCondition();
	Player *player = obj->getControllingPlayer();
	if( player != nullptr )
	{
		player->onStructureConstructionComplete( me, obj, FALSE );
		for( BehaviorModule **m = obj->getBehaviorModules(); *m; ++m )
		{
			CreateModuleInterface *create = (*m)->getCreate();
			if( create != nullptr )
				create->onBuildComplete();
		}
	}
	obj->handlePartitionCellMaintenance();
	obj->updateUpgradeModules();
	stop();
	return UPDATE_SLEEP_FOREVER;
}

//-------------------------------------------------------------------------------------------------
void AssembleStructureUpdate::crc( Xfer *xfer )
{
	SpecialPowerUpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void AssembleStructureUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	SpecialPowerUpdateModule::xfer( xfer );

	Int stage = (Int)m_stage;
	xfer->xferInt( &stage );
	m_stage = (Stage)stage;
	xfer->xferCoord3D( &m_goal );
	xfer->xferReal( &m_angle );
	xfer->xferObjectID( &m_structureID );
}

//-------------------------------------------------------------------------------------------------
void AssembleStructureUpdate::loadPostProcess()
{
	SpecialPowerUpdateModule::loadPostProcess();
}
