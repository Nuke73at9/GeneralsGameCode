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

// FILE: CreateAndEnterObjectSpecialPower.cpp ////////////////////////////////
// Tiberium Project @feature the unit calls up an object and gets into it (see the header)
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/CreateAndEnterObjectSpecialPower.h"

#include <vector>

//-------------------------------------------------------------------------------------------------
CreateAndEnterObjectSpecialPowerModuleData::CreateAndEnterObjectSpecialPowerModuleData()
{
	m_ocl = nullptr;
	m_triggerFX = nullptr;
}

//-------------------------------------------------------------------------------------------------
void CreateAndEnterObjectSpecialPowerModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	SpecialPowerModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "OCL",				INI::parseObjectCreationList,	nullptr, offsetof( CreateAndEnterObjectSpecialPowerModuleData, m_ocl ) },
		{ "TriggerFX",		INI::parseFXList,						nullptr, offsetof( CreateAndEnterObjectSpecialPowerModuleData, m_triggerFX ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
CreateAndEnterObjectSpecialPower::CreateAndEnterObjectSpecialPower( Thing *thing, const ModuleData* moduleData ) : SpecialPowerModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
CreateAndEnterObjectSpecialPower::~CreateAndEnterObjectSpecialPower()
{
}

namespace
{
	/// what the last use made, for squad-mates told in the same frame (a group's members are told in turn)
	struct LastMade
	{
		UnsignedInt frame;
		Int player;
		const SpecialPowerTemplate *power;
		ObjectID made;
		Int slotsTaken;		///< by those sent into it so far (not all inside yet)
	};
	LastMade s_lastMade = { 0xffffffffu, -1, nullptr, INVALID_ID, 0 };

	/// those to send into what was made, once the partition manager has placed it (an object just made is in no
	/// cell yet, so shrouded, and a human player's unit may not enter what it cannot see); sent the same frame
	std::vector<ObjectID> s_pendingRiders;
	std::vector<ObjectID> s_pendingInto;
}

//-------------------------------------------------------------------------------------------------
/** a new game, or a game loaded: no squad-mate's object from before (every machine starts it the same) */
void CreateAndEnterObjectSpecialPower::resetLastMade()
{
	s_lastMade.frame = 0xffffffffu;
	s_lastMade.player = -1;
	s_lastMade.power = nullptr;
	s_lastMade.made = INVALID_ID;
	s_lastMade.slotsTaken = 0;
	s_pendingRiders.clear();
	s_pendingInto.clear();
}

//-------------------------------------------------------------------------------------------------
/** after the partition manager's update (GameLogic): each unit sent into what it (or a squad-mate) made */
void CreateAndEnterObjectSpecialPower::sendPendingRiders()
{
	if( s_pendingRiders.empty() )
		return;
	std::vector<ObjectID> riders, into;
	riders.swap( s_pendingRiders );
	into.swap( s_pendingInto );
	for( size_t i = 0; i < riders.size() && i < into.size(); ++i )
	{
		Object *rider = TheGameLogic->findObjectByID( riders[i] );
		Object *made = TheGameLogic->findObjectByID( into[i] );
		if( rider == nullptr || made == nullptr || rider->isEffectivelyDead() || made->isEffectivelyDead() || rider->getAI() == nullptr )
			continue;
		const Player *owner = rider->getControllingPlayer();
		rider->getAI()->aiEnter( made, owner != nullptr && owner->getPlayerType() == PLAYER_COMPUTER ? CMD_FROM_AI : CMD_FROM_PLAYER );
	}
}

//-------------------------------------------------------------------------------------------------
void CreateAndEnterObjectSpecialPower::doSpecialPower( UnsignedInt commandOptions )
{
	createAndEnter( getObject()->getPosition(), TRUE, commandOptions );
}

//-------------------------------------------------------------------------------------------------
void CreateAndEnterObjectSpecialPower::doSpecialPowerAtLocation( const Coord3D *loc, Real angle, UnsignedInt commandOptions )
{
	if( loc == nullptr )
		return;
	Coord3D at = *loc;
	at.z = TheTerrainLogic->getGroundHeight( at.x, at.y );
	createAndEnter( &at, FALSE, commandOptions );
}

//-------------------------------------------------------------------------------------------------
/** the object made at `at` (the unit's place, or the spot it was aimed at) and the unit sent into it; or into
	the one a squad-mate told with it just made, while that has room */
//-------------------------------------------------------------------------------------------------
void CreateAndEnterObjectSpecialPower::createAndEnter( const Coord3D *at, Bool atUnit, UnsignedInt commandOptions )
{
	Object *me = getObject();
	const CreateAndEnterObjectSpecialPowerModuleData *d = getCreateAndEnterObjectSpecialPowerModuleData();
	AIUpdateInterface *ai = me->getAI();
	Player *player = me->getControllingPlayer();
	if( d->m_ocl == nullptr || ai == nullptr || player == nullptr || me->isDisabled() || me->isContained() || !isReady() )
		return;

	// a squad-mate's, told in the same frame
	const UnsignedInt now = TheGameLogic->getFrame();
	const Int slots = max( 1, me->getTransportSlotCount() );
	if( s_lastMade.frame == now && s_lastMade.player == player->getPlayerIndex() && s_lastMade.power == getSpecialPowerTemplate() )
	{
		Object *made = TheGameLogic->findObjectByID( s_lastMade.made );
		ContainModuleInterface *contain = made != nullptr && !made->isEffectivelyDead() ? made->getContain() : nullptr;
		if( contain != nullptr && contain->isValidContainerFor( me, false ) && s_lastMade.slotsTaken + slots <= contain->getContainMax() )
		{
			// records the use and starts the reload time, as the first's
			SpecialPowerModule::doSpecialPower( commandOptions );
			s_lastMade.slotsTaken += slots;
			s_pendingRiders.push_back( me->getID() );
			s_pendingInto.push_back( made->getID() );
			return;
		}
	}

	// records the use and starts the reload time
	SpecialPowerModule::doSpecialPower( commandOptions );

	const Coord3D pos = *at;
	if( d->m_triggerFX != nullptr )
		FXList::doFXPos( d->m_triggerFX, me->getPosition() );
	Object *made = ObjectCreationList::create( d->m_ocl, me, &pos, &pos, atUnit ? me->getOrientation() : 0.0f );
	if( made == nullptr || made->isEffectivelyDead() )
		return;
	s_lastMade.frame = now;
	s_lastMade.player = player->getPlayerIndex();
	s_lastMade.power = getSpecialPowerTemplate();
	s_lastMade.made = made->getID();
	s_lastMade.slotsTaken = slots;
	s_pendingRiders.push_back( me->getID() );
	s_pendingInto.push_back( made->getID() );
}

//-------------------------------------------------------------------------------------------------
void CreateAndEnterObjectSpecialPower::crc( Xfer *xfer )
{
	SpecialPowerModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void CreateAndEnterObjectSpecialPower::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	SpecialPowerModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void CreateAndEnterObjectSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
	resetLastMade();
}
