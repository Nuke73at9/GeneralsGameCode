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

#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/CreateAndEnterObjectSpecialPower.h"

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

//-------------------------------------------------------------------------------------------------
void CreateAndEnterObjectSpecialPower::doSpecialPower( UnsignedInt commandOptions )
{
	Object *me = getObject();
	const CreateAndEnterObjectSpecialPowerModuleData *d = getCreateAndEnterObjectSpecialPowerModuleData();
	AIUpdateInterface *ai = me->getAI();
	if( d->m_ocl == nullptr || ai == nullptr || me->isDisabled() || me->isContained() || !isReady() )
		return;

	// records the use and starts the reload time
	SpecialPowerModule::doSpecialPower( commandOptions );

	const Coord3D pos = *me->getPosition();
	if( d->m_triggerFX != nullptr )
		FXList::doFXPos( d->m_triggerFX, &pos );
	Object *made = ObjectCreationList::create( d->m_ocl, me, &pos, &pos, me->getOrientation() );
	if( made != nullptr && !made->isEffectivelyDead() )
		ai->aiEnter( made, CMD_FROM_PLAYER );
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
}
