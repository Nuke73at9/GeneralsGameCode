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

// FILE: MonitorSpecialPowerTimerUpdate.cpp ///////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's MonitorSpecialPowerTimerUpdate (see MonitorSpecialPowerTimerUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/MonitorSpecialPowerTimerUpdate.h"

#include "Common/BitFlagsIO.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/SpecialPowerModule.h"

//-------------------------------------------------------------------------------------------------
MonitorSpecialPowerTimerUpdateModuleData::MonitorSpecialPowerTimerUpdateModuleData()
{
	m_specialPowerTemplate = nullptr;
	m_readyCondition = MODELCONDITION_SPECIALPOWER1_READY;
	m_readyEva = EVA_Invalid;	// Tiberium Project
}

//-------------------------------------------------------------------------------------------------
void MonitorSpecialPowerTimerUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "SpecialPowerTemplate",	INI::parseSpecialPowerTemplate,				nullptr, offsetof( MonitorSpecialPowerTimerUpdateModuleData, m_specialPowerTemplate ) },
		{ "ReadyCondition",			ModelConditionFlags::parseSingleBitFromINI,	nullptr, offsetof( MonitorSpecialPowerTimerUpdateModuleData, m_readyCondition ) },
		{ "ReadyStatus",				ObjectStatusMaskType::parseFromINI,			nullptr, offsetof( MonitorSpecialPowerTimerUpdateModuleData, m_readyStatus ) },	// Tiberium Project
		{ "ReadyEvaEvent",			Eva::parseEvaMessageFromIni,					nullptr, offsetof( MonitorSpecialPowerTimerUpdateModuleData, m_readyEva ) },	// Tiberium Project
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
MonitorSpecialPowerTimerUpdate::MonitorSpecialPowerTimerUpdate( Thing *thing, const ModuleData* moduleData ) :
	UpdateModule( thing, moduleData ),
	m_ready( FALSE )
{
}

//-------------------------------------------------------------------------------------------------
MonitorSpecialPowerTimerUpdate::~MonitorSpecialPowerTimerUpdate()
{
}

//-------------------------------------------------------------------------------------------------
// twice a second: is the power ready (and the object whole, not being built or sold)?
//-------------------------------------------------------------------------------------------------
UpdateSleepTime MonitorSpecialPowerTimerUpdate::update()
{
	const MonitorSpecialPowerTimerUpdateModuleData *data = getMonitorSpecialPowerTimerUpdateModuleData();
	Object *me = getObject();
	const SpecialPowerModuleInterface *power = data->m_specialPowerTemplate != nullptr ? me->getSpecialPowerModule( data->m_specialPowerTemplate ) : nullptr;
	const Bool ready = power != nullptr && power->isReady()
		&& !me->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) && !me->testStatus( OBJECT_STATUS_SOLD );
	if( ready != m_ready && data->m_readyCondition != MODELCONDITION_INVALID )
	{
		if( ready )
			me->setModelConditionState( data->m_readyCondition );
		else
			me->clearModelConditionState( data->m_readyCondition );
	}
	// Tiberium Project @feature and its statuses (ReadyStatus)
	if( ready != m_ready && data->m_readyStatus.any() )
		me->setStatus( data->m_readyStatus, ready );
	// Tiberium Project @feature the announcer tells its owner (ReadyEvaEvent)
	if( ready && !m_ready && data->m_readyEva != EVA_Invalid && TheEva != nullptr && TheGameLogic->getFrame() > 0 &&
			ThePlayerList != nullptr && me->getControllingPlayer() == ThePlayerList->getLocalPlayer() )
		TheEva->setShouldPlay( data->m_readyEva );
	m_ready = ready;
	return UPDATE_SLEEP( LOGICFRAMES_PER_SECOND / 2 );
}

//-------------------------------------------------------------------------------------------------
void MonitorSpecialPowerTimerUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void MonitorSpecialPowerTimerUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	UpdateModule::xfer( xfer );
	xfer->xferBool( &m_ready );
}

//-------------------------------------------------------------------------------------------------
void MonitorSpecialPowerTimerUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
