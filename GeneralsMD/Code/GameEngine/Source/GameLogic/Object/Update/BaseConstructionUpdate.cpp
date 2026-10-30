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

// FILE: BaseConstructionUpdate.cpp ///////////////////////////////////////////////////////////////
// Tiberium Project @feature Kane's Wrath's build-then-place base (see BaseConstructionUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/BaseConstructionUpdate.h"

#include "Common/BuildAssistant.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/BuildAreaBehavior.h"
#include "GameLogic/Module/CreateModule.h"

//-------------------------------------------------------------------------------------------------
BaseConstructionUpdateModuleData::BaseConstructionUpdateModuleData()
{
	m_unpackTime = LOGICFRAMES_PER_SECOND * 3;
	m_buildRadius = 0.0f;
	m_excludeKindOf.clear();
}

//-------------------------------------------------------------------------------------------------
void BaseConstructionUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	static const FieldParse dataFieldParse[] =
	{
		{ "CommandSet",			INI::parseAsciiString,					nullptr, offsetof( BaseConstructionUpdateModuleData, m_commandSetName ) },
		{ "UnpackTime",			INI::parseDurationUnsignedInt,	nullptr, offsetof( BaseConstructionUpdateModuleData, m_unpackTime ) },
		{ "BuildRadius",		INI::parseReal,									nullptr, offsetof( BaseConstructionUpdateModuleData, m_buildRadius ) },
		{ "ExcludeKindOf",	KindOfMaskType::parseFromINI,		nullptr, offsetof( BaseConstructionUpdateModuleData, m_excludeKindOf ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse( p );
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
BaseConstructionUpdate::BaseConstructionUpdate( Thing *thing, const ModuleData* moduleData ) :
	UpdateModule( thing, moduleData ),
	m_building( nullptr ),
	m_framesLeft( 0 ),
	m_framesTotal( 0 ),
	m_paid( 0 )
{
	setWakeFrame( getObject(), UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
BaseConstructionUpdate::~BaseConstructionUpdate()
{
}

//-------------------------------------------------------------------------------------------------
BaseConstructionUpdate *BaseConstructionUpdate::find( Object *obj )
{
	if( obj == nullptr )
		return nullptr;
	static const NameKeyType key = NAMEKEY( "BaseConstructionUpdate" );
	return (BaseConstructionUpdate *)obj->findUpdateModule( key );
}

//-------------------------------------------------------------------------------------------------
const CommandButton *BaseConstructionUpdate::findButton( const ThingTemplate *what ) const
{
	const BaseConstructionUpdateModuleData *d = getBaseConstructionUpdateModuleData();
	if( what == nullptr || TheControlBar == nullptr || what->isAnyKindOf( d->m_excludeKindOf ) )
		return nullptr;
	const CommandSet *set = TheControlBar->findCommandSet( d->m_commandSetName );
	for( Int i = 0; set != nullptr && i < MAX_COMMANDS_PER_SET; ++i )
	{
		const CommandButton *b = set->getCommandButton( i );
		if( b != nullptr && b->getCommandType() == GUI_COMMAND_DOZER_CONSTRUCT && b->getThingTemplate() != nullptr
			&& b->getThingTemplate()->isEquivalentTo( what ) )
			return b;
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
Bool BaseConstructionUpdate::canBuildNow( const ThingTemplate *what ) const
{
	// a builder's checks (BuildAssistant::canMakeUnit), with the yard's command set in the builder's
	const CommandButton *button = findButton( what );
	Player *player = getObject()->getControllingPlayer();
	if( button == nullptr || player == nullptr || !player->canBuildMoreOfType( what ) || !player->canBuild( what ) )
		return FALSE;
	const UpgradeTemplate *needed = button->getUpgradeTemplate();
	if( BitIsSet( button->getOptions(), NEED_UPGRADE ) && needed != nullptr )
	{
		const Bool have = needed->getUpgradeType() == UPGRADE_TYPE_PLAYER ? player->hasUpgradeComplete( needed ) : getObject()->hasUpgrade( needed );
		if( !have )
			return FALSE;
	}
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Real BaseConstructionUpdate::getPercent() const
{
	if( m_building == nullptr || m_framesTotal == 0 )
		return 0.0f;
	return 100.0f * (Real)( m_framesTotal - m_framesLeft ) / (Real)m_framesTotal;
}

//-------------------------------------------------------------------------------------------------
Bool BaseConstructionUpdate::queue( const ThingTemplate *what )
{
	Object *me = getObject();
	Player *player = me->getControllingPlayer();
	if( m_building != nullptr || player == nullptr || !builds( what ) || me->isEffectivelyDead()
		|| me->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) || me->testStatus( OBJECT_STATUS_SOLD ) )
		return FALSE;
	if( !canBuildNow( what ) )
		return FALSE;
	const Int cost = what->calcCostToBuild( player );
	if( player->getMoney()->countMoney() < (UnsignedInt)cost )
		return FALSE;
	player->getMoney()->withdraw( cost );
	m_paid = cost;
	m_building = what;
	m_framesTotal = max( 1, what->calcTimeToBuild( player ) );
	m_framesLeft = m_framesTotal;
	setWakeFrame( me, UPDATE_SLEEP_NONE );
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void BaseConstructionUpdate::cancel()
{
	Player *player = getObject()->getControllingPlayer();
	if( m_building == nullptr )
		return;
	if( player != nullptr && m_paid > 0 )
		player->getMoney()->deposit( m_paid );
	m_building = nullptr;
	m_framesLeft = m_framesTotal = 0;
	m_paid = 0;
}

//-------------------------------------------------------------------------------------------------
Bool BaseConstructionUpdate::isWithinBuildRadius( const Coord3D *where ) const
{
	// Kane's Wrath's buildable areas: the yard's own, the refineries' and the other structures' that have one
	if( !BuildAreaBehavior::isInBuildArea( getObject()->getControllingPlayer(), where ) )
		return FALSE;
	const Real radius = getBaseConstructionUpdateModuleData()->m_buildRadius;
	if( radius <= 0.0f )
		return TRUE;
	struct Near { const Coord3D *at; Real r2; Bool found;
		static void each( Object *o, void *data )
		{
			Near *n = (Near *)data;
			if( n->found || !o->isKindOf( KINDOF_STRUCTURE ) || o->isEffectivelyDead() )
				return;
			const Real dx = o->getPosition()->x - n->at->x, dy = o->getPosition()->y - n->at->y;
			if( dx * dx + dy * dy <= n->r2 )
				n->found = TRUE;
		} };
	Near n = { where, radius * radius, FALSE };
	const Player *player = getObject()->getControllingPlayer();
	if( player != nullptr )
		player->iterateObjects( Near::each, &n );
	return n.found;
}

//-------------------------------------------------------------------------------------------------
Bool BaseConstructionUpdate::place( const Coord3D *where, Real angle )
{
	Object *me = getObject();
	Player *player = me->getControllingPlayer();
	if( !isReady() || player == nullptr || where == nullptr || me->isEffectivelyDead() )
		return FALSE;
	Coord3D spot = *where;
	if( !isWithinBuildRadius( &spot ) )
		return FALSE;
	if( TheBuildAssistant->isLocationLegalToBuild( &spot, m_building, angle,
		BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP, nullptr, player ) != LBC_OK )
		return FALSE;

	// unbuilt, at the spot, rising (as a dozer starts one, with nobody building it)
	Object *obj = TheThingFactory->newObject( m_building, player->getDefaultTeam(), MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION ) );
	obj->setProducer( me );
	obj->setBuilder( me );
	obj->setPosition( &spot );
	obj->setOrientation( angle );
	TheTerrainLogic->flattenTerrain( obj );
	Coord3D ground = spot;
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
	m_rising.push_back( obj->getID() );

	m_building = nullptr;
	m_framesLeft = m_framesTotal = 0;
	m_paid = 0;
	setWakeFrame( me, UPDATE_SLEEP_NONE );
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime BaseConstructionUpdate::update()
{
	Object *me = getObject();
	const BaseConstructionUpdateModuleData *d = getBaseConstructionUpdateModuleData();

	// the one building: a frame nearer (Zero Hour's production slows without power the same way)
	if( m_building != nullptr && m_framesLeft > 0 && !me->isDisabled() )
	{
		const Player *player = me->getControllingPlayer();
		const Bool lowPower = player != nullptr && !player->getEnergy()->hasSufficientPower();
		if( !lowPower || ( TheGameLogic->getFrame() & 1 ) == 0 )
			--m_framesLeft;
	}

	// the placed ones rise
	const UnsignedInt unpack = max( (UnsignedInt)1, d->m_unpackTime );
	for( size_t i = 0; i < m_rising.size(); )
	{
		Object *obj = TheGameLogic->findObjectByID( m_rising[i] );
		if( obj == nullptr || obj->isEffectivelyDead() || !obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		{
			m_rising.erase( m_rising.begin() + i );
			continue;
		}
		obj->setConstructionPercent( obj->getConstructionPercent() + 100.0f / (Real)unpack );
		BodyModuleInterface *body = obj->getBodyModule();
		if( body != nullptr )
			body->internalChangeHealth( body->getMaxHealth() / (Real)unpack );
		if( obj->getConstructionPercent() < 100.0f )
		{
			++i;
			continue;
		}
		// done (as a dozer's building is)
		obj->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION ) );
		obj->clearModelConditionFlags( MAKE_MODELCONDITION_MASK3( MODELCONDITION_AWAITING_CONSTRUCTION, MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED ) );
		obj->setConstructionPercent( CONSTRUCTION_COMPLETE );
		if( body != nullptr )
			body->evaluateVisualCondition();
		Player *owner = obj->getControllingPlayer();
		if( owner != nullptr )
		{
			owner->onStructureConstructionComplete( me, obj, FALSE );
			for( BehaviorModule **m = obj->getBehaviorModules(); *m; ++m )
			{
				CreateModuleInterface *create = (*m)->getCreate();
				if( create != nullptr )
					create->onBuildComplete();
			}
		}
		obj->handlePartitionCellMaintenance();
		obj->updateUpgradeModules();
		m_rising.erase( m_rising.begin() + i );
	}

	if( ( m_building != nullptr && m_framesLeft > 0 ) || !m_rising.empty() )
		return UPDATE_SLEEP_NONE;
	return UPDATE_SLEEP_FOREVER;
}

//-------------------------------------------------------------------------------------------------
void BaseConstructionUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void BaseConstructionUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );

	AsciiString name = m_building != nullptr ? m_building->getName() : AsciiString::TheEmptyString;
	xfer->xferAsciiString( &name );
	if( xfer->getXferMode() == XFER_LOAD )
		m_building = name.isEmpty() ? nullptr : TheThingFactory->findTemplate( name );
	xfer->xferUnsignedInt( &m_framesLeft );
	xfer->xferUnsignedInt( &m_framesTotal );
	xfer->xferInt( &m_paid );
	UnsignedShort count = (UnsignedShort)m_rising.size();
	xfer->xferUnsignedShort( &count );
	if( xfer->getXferMode() == XFER_LOAD )
		m_rising.resize( count );
	for( UnsignedShort i = 0; i < count; ++i )
		xfer->xferObjectID( &m_rising[i] );
}

//-------------------------------------------------------------------------------------------------
void BaseConstructionUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
