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

// FILE: BannerCarrierUpdate.cpp //////////////////////////////////////////////////////////////////
// Tiberium Project @feature a Kane's Wrath squad's leader (see BannerCarrierUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/BannerCarrierUpdate.h"

#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"

//-------------------------------------------------------------------------------------------------
static BannerCarrierUpdate *findBannerCarrierUpdate( Object *obj )
{
	static const NameKeyType key = NAMEKEY( "BannerCarrierUpdate" );
	return obj != nullptr ? (BannerCarrierUpdate *)obj->findUpdateModule( key ) : nullptr;
}

//-------------------------------------------------------------------------------------------------
BannerCarrierUpdate::BannerCarrierUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_squadID = INVALID_ID;
	m_given = FALSE;
	// (no upgrade: the squad comes with its leader. From the masks: the names are let go once the first of its kind has
	// made them masks)
	UpgradeMaskType activation, conflicting;
	getBannerCarrierUpdateModuleData()->m_upgradeMuxData.getUpgradeActivationMasks( activation, conflicting );
	m_wanted = !activation.any();
	setWakeFrame( getObject(), m_wanted ? UPDATE_SLEEP_NONE : UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
BannerCarrierUpdate::~BannerCarrierUpdate()
{
}

//-------------------------------------------------------------------------------------------------
void BannerCarrierUpdate::joinSquad( Object *obj, ObjectID squadID )
{
	BannerCarrierUpdate *update = findBannerCarrierUpdate( obj );
	if( update != nullptr )
		update->m_squadID = squadID;
}

//-------------------------------------------------------------------------------------------------
ObjectID BannerCarrierUpdate::getSquadID() const
{
	return m_squadID != INVALID_ID ? m_squadID : getObject()->getID();
}

//-------------------------------------------------------------------------------------------------
void BannerCarrierUpdate::upgradeImplementation()
{
	m_wanted = TRUE;
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
// the squad's other soldiers: whether one of them has given the squad its leader (Mark: tell them all it is given)
//-------------------------------------------------------------------------------------------------
struct BannerCarrierSquadInfo
{
	Object *m_me;
	ObjectID m_squadID;
	AsciiString m_carrierName;
	Bool m_mark;
	Bool m_given;
};

static void checkSquadMate( Object *obj, void *userData )
{
	BannerCarrierSquadInfo *info = (BannerCarrierSquadInfo *)userData;
	if( obj == info->m_me )
		return;
	BannerCarrierUpdate *mate = findBannerCarrierUpdate( obj );
	if( mate == nullptr || mate->getSquadID() != info->m_squadID || mate->getCarrierName() != info->m_carrierName )
		return;
	if( info->m_mark )
		mate->markGiven();
	else if( mate->hasGivenCarrier() )
		info->m_given = TRUE;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime BannerCarrierUpdate::update()
{
	if( !m_wanted || m_given )
		return UPDATE_SLEEP_FOREVER;
	Object *me = getObject();
	Player *owner = me->getControllingPlayer();
	if( me->isEffectivelyDead() || owner == nullptr )
		return UPDATE_SLEEP_FOREVER;
	// (in a transport or a building: the leader comes once the soldier is out)
	if( me->getContainedBy() != nullptr || me->isDisabled() )
		return UPDATE_SLEEP( LOGICFRAMES_PER_SECOND );

	const BannerCarrierUpdateModuleData *data = getBannerCarrierUpdateModuleData();
	BannerCarrierSquadInfo info;
	info.m_me = me;
	info.m_squadID = getSquadID();
	info.m_carrierName = data->m_carrierName;
	info.m_mark = FALSE;
	info.m_given = FALSE;
	owner->iterateObjects( checkSquadMate, &info );
	m_given = TRUE;
	if( info.m_given )
		return UPDATE_SLEEP_FOREVER;

	const ThingTemplate *tmpl = data->m_carrierName.isEmpty() ? nullptr : TheThingFactory->findTemplate( data->m_carrierName );
	if( tmpl == nullptr )
	{
		DEBUG_CRASH(( "BannerCarrierUpdate: no object %s", data->m_carrierName.str() ));
		return UPDATE_SLEEP_FOREVER;
	}
	Object *carrier = TheThingFactory->newObject( tmpl, me->getTeam() );
	if( carrier == nullptr )
		return UPDATE_SLEEP_FOREVER;
	// beside the soldier: ahead of it, to its left
	const Real angle = me->getOrientation();
	const Real c = Cos( angle ), s = Sin( angle );
	Coord3D pos = *me->getPosition();
	pos.x += data->m_offset.x * c - data->m_offset.y * s;
	pos.y += data->m_offset.x * s + data->m_offset.y * c;
	carrier->setPosition( &pos );
	carrier->setOrientation( angle );
	carrier->setProducer( me );
	// and where the squad goes
	AIUpdateInterface *myAI = me->getAI();
	AIUpdateInterface *ai = carrier->getAI();
	if( myAI != nullptr && ai != nullptr && !myAI->isIdle() && myAI->getGoalPosition() != nullptr )
	{
		Coord3D goal = *myAI->getGoalPosition();
		goal.x += data->m_offset.x * c - data->m_offset.y * s;
		goal.y += data->m_offset.x * s + data->m_offset.y * c;
		ai->aiMoveToPosition( &goal, CMD_FROM_AI );
	}

	info.m_mark = TRUE;
	owner->iterateObjects( checkSquadMate, &info );
	return UPDATE_SLEEP_FOREVER;
}

//-------------------------------------------------------------------------------------------------
void BannerCarrierUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
	UpgradeMux::upgradeMuxCRC( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void BannerCarrierUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	UpdateModule::xfer( xfer );
	UpgradeMux::upgradeMuxXfer( xfer );
	xfer->xferObjectID( &m_squadID );
	xfer->xferBool( &m_wanted );
	xfer->xferBool( &m_given );
}

//-------------------------------------------------------------------------------------------------
void BannerCarrierUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	UpgradeMux::upgradeMuxLoadPostProcess();
}
