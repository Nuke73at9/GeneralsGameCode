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

// FILE: ReplaceSelfUpdate.cpp ////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's ReplaceSelfSpecialAbility (see ReplaceSelfUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/ReplaceSelfUpdate.h"

#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/ProductionUpdate.h"

//-------------------------------------------------------------------------------------------------
ReplaceSelfUpdate::ReplaceSelfUpdate( Thing *thing, const ModuleData* moduleData ) : SpecialPowerUpdateModule( thing, moduleData )
{
	m_stage = STAGE_NONE;
	m_stageEndFrame = 0;
	m_goal.zero();
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
ReplaceSelfUpdate::~ReplaceSelfUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool ReplaceSelfUpdate::initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions )
{
	const ReplaceSelfUpdateModuleData *data = getReplaceSelfUpdateModuleData();
	if( specialPowerTemplate != data->m_specialPowerTemplate )
		return FALSE;

	Object *me = getObject();
	if( me->isDisabled() || me->isEffectivelyDead() || ( m_stage != STAGE_NONE && m_stage != STAGE_MOVING ) )
		return FALSE;

	// Tiberium Project @feature a deploy at a chosen spot (C&C4's Command_CrawlerUnpack): it drives
	// there first, then deploys (update)
	AIUpdateInterface *ai = me->getAI();
	if( targetPos != nullptr && ai != nullptr )
	{
		m_goal = *targetPos;
		m_stage = STAGE_MOVING;
		ai->aiMoveToPosition( &m_goal, CMD_FROM_AI );
		setWakeFrame( me, UPDATE_SLEEP_NONE );
		return TRUE;
	}
	replace();
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** replace us with the replacement, now */
void ReplaceSelfUpdate::replace()
{
	const ReplaceSelfUpdateModuleData *data = getReplaceSelfUpdateModuleData();
	Object *me = getObject();
	m_stage = STAGE_NONE;
	const ThingTemplate *replacement = TheThingFactory->findTemplate( data->m_replacementName );
	if( replacement == nullptr )
	{
		DEBUG_CRASH( ( "ReplaceSelfUpdate on %s: no object '%s'", me->getTemplate()->getName().str(), data->m_replacementName.str() ) );
		return;
	}

	// what carries over
	Team *team = me->getTeam();
	Player *owner = me->getControllingPlayer();
	const Matrix3D transform = *me->getTransformMatrix();
	const BodyModuleInterface *body = me->getBodyModule();
	const Real healthShare = ( body != nullptr && body->getMaxHealth() > 0.0f ) ? body->getHealth() / body->getMaxHealth() : 1.0f;
	const ExperienceTracker *experience = me->getExperienceTracker();
	const Int experiencePoints = experience != nullptr ? experience->getCurrentExperience() : 0;
	const Drawable *selected = TheInGameUI->getFirstSelectedDrawable();
	const Bool wasSelected = selected != nullptr && me->getDrawable() != nullptr && selected->getID() == me->getDrawable()->getID();
	const Int squad = owner != nullptr ? owner->getSquadNumberForObject( me ) : NO_HOTKEY_SQUAD;
	const Bool local = me->isLocallyControlled();

	// whatever we were building is refunded, as when a factory is sold
	ProductionUpdateInterface *production = me->getProductionUpdateInterface();
	if( production != nullptr )
		production->cancelAndRefundAllProduction();

	// off the map first: cells are marked, not counted (as ReplaceObjectUpgrade)
	TheAI->pathfinder()->removeObjectFromPathfindMap( me );
	TheGameLogic->destroyObject( me );

	Object *made = TheThingFactory->newObject( replacement, team );
	made->setTransformMatrix( &transform );
	TheAI->pathfinder()->addObjectToPathfindMap( made );

	// made in place, not built: complete at once
	for( BehaviorModule **m = made->getBehaviorModules(); *m; ++m )
	{
		CreateModuleInterface *create = (*m)->getCreate();
		if( create != nullptr )
			create->onBuildComplete();
	}
	if( made->getControllingPlayer() != nullptr )
		made->getControllingPlayer()->onStructureConstructionComplete( nullptr, made, FALSE );

	// health as the same share of its own, and the veterancy earned
	BodyModuleInterface *madeBody = made->getBodyModule();
	if( madeBody != nullptr && madeBody->getMaxHealth() > 0.0f )
		madeBody->internalChangeHealth( madeBody->getMaxHealth() * healthShare - madeBody->getHealth() );
	if( made->getExperienceTracker() != nullptr && experiencePoints > 0 )
		made->getExperienceTracker()->setExperienceAndLevel( experiencePoints, FALSE );

	// held while it unfolds, its arrival shown
	if( data->m_newObjectUnpackTime > 0 )
		made->setDisabledUntil( DISABLED_DEFAULT, TheGameLogic->getFrame() + data->m_newObjectUnpackTime );
	static const NameKeyType key_ReplaceSelfUpdate = NAMEKEY( "ReplaceSelfUpdate" );
	ReplaceSelfUpdate *arrival = (ReplaceSelfUpdate *)made->findUpdateModule( key_ReplaceSelfUpdate );
	if( arrival != nullptr )
		arrival->beginArrival();

	// the player's view of it: still selected, still in its hotkey group (as ReplaceObjectUpgrade)
	if( local && made->getDrawable() != nullptr )
	{
		if( wasSelected )
		{
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_CREATE_SELECTED_GROUP_NO_SOUND );
			msg->appendBooleanArgument( TRUE );
			msg->appendObjectIDArgument( made->getID() );
			TheInGameUI->selectDrawable( made->getDrawable() );
		}
		if( squad != NO_HOTKEY_SQUAD )
		{
			GameMessage *msg = TheMessageStream->appendMessage( (GameMessage::Type)( GameMessage::MSG_CREATE_TEAM0 + squad ) );
			msg->appendObjectIDArgument( made->getID() );
		}
	}
}

//-------------------------------------------------------------------------------------------------
void ReplaceSelfUpdate::beginArrival()
{
	const ReplaceSelfUpdateModuleData *data = getReplaceSelfUpdateModuleData();
	if( data->m_transition1Time > 0 )
		enterStage( STAGE_TRANSITION_1 );
	else if( data->m_transition2Time > 0 )
		enterStage( STAGE_TRANSITION_2 );
}

//-------------------------------------------------------------------------------------------------
void ReplaceSelfUpdate::enterStage( Stage stage )
{
	const ReplaceSelfUpdateModuleData *data = getReplaceSelfUpdateModuleData();
	Object *me = getObject();
	me->clearModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_CRAWLER_TRANSITION_1, MODELCONDITION_CRAWLER_TRANSITION_2 ) );
	m_stage = stage;
	if( stage == STAGE_NONE )
	{
		setWakeFrame( me, UPDATE_SLEEP_FOREVER );
		return;
	}
	const UnsignedInt length = stage == STAGE_TRANSITION_1 ? data->m_transition1Time : data->m_transition2Time;
	me->setModelConditionState( stage == STAGE_TRANSITION_1 ? MODELCONDITION_CRAWLER_TRANSITION_1 : MODELCONDITION_CRAWLER_TRANSITION_2 );
	m_stageEndFrame = TheGameLogic->getFrame() + length;
	setWakeFrame( me, UPDATE_SLEEP( length > 0 ? length : 1 ) );
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime ReplaceSelfUpdate::update()
{
	if( m_stage == STAGE_NONE )
		return UPDATE_SLEEP_FOREVER;
	if( m_stage == STAGE_MOVING )
	{
		// on its way to the spot it was told to deploy at: deploys there, or where it stopped near it;
		// another order (the player's) calls the deploy off
		Object *me = getObject();
		AIUpdateInterface *ai = me->getAI();
		if( ai == nullptr || me->isEffectivelyDead() || ai->getLastCommandSource() != CMD_FROM_AI )
		{
			m_stage = STAGE_NONE;
			return UPDATE_SLEEP_FOREVER;
		}
		const Coord3D *pos = me->getPosition();
		const Real dx = pos->x - m_goal.x, dy = pos->y - m_goal.y;
		const Real dist2 = dx * dx + dy * dy;
		const Real nearDist = me->getGeometryInfo().getMajorRadius() + 2.0f * PATHFIND_CELL_SIZE_F;
		if( !me->isDisabled() && ( dist2 <= sqr( 2.0f * PATHFIND_CELL_SIZE_F ) || ( ai->isIdle() && dist2 <= sqr( nearDist ) ) ) )
		{
			replace();
			return UPDATE_SLEEP_FOREVER;
		}
		if( ai->isIdle() && dist2 > sqr( nearDist ) )
		{
			m_stage = STAGE_NONE;	// stopped short of it: no deploy
			return UPDATE_SLEEP_FOREVER;
		}
		return UPDATE_SLEEP( LOGICFRAMES_PER_SECOND / 4 );
	}
	const UnsignedInt now = TheGameLogic->getFrame();
	if( now < m_stageEndFrame )
		return UPDATE_SLEEP( m_stageEndFrame - now );
	const ReplaceSelfUpdateModuleData *data = getReplaceSelfUpdateModuleData();
	if( m_stage == STAGE_TRANSITION_1 && data->m_transition2Time > 0 )
		enterStage( STAGE_TRANSITION_2 );
	else
		enterStage( STAGE_NONE );
	return m_stage == STAGE_NONE ? UPDATE_SLEEP_FOREVER : UPDATE_SLEEP( m_stageEndFrame - now );
}

//-------------------------------------------------------------------------------------------------
void ReplaceSelfUpdate::crc( Xfer *xfer )
{
	SpecialPowerUpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
// 2: the spot it drives to before it deploys
//-------------------------------------------------------------------------------------------------
void ReplaceSelfUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	SpecialPowerUpdateModule::xfer( xfer );

	Int stage = (Int)m_stage;
	xfer->xferInt( &stage );
	m_stage = (Stage)stage;
	xfer->xferUnsignedInt( &m_stageEndFrame );
	if( version >= 2 )
		xfer->xferCoord3D( &m_goal );
}

//-------------------------------------------------------------------------------------------------
void ReplaceSelfUpdate::loadPostProcess()
{
	SpecialPowerUpdateModule::loadPostProcess();
}
