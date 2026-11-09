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


// FILE: RadarHackSpecialPower.cpp ////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's RadarHackSpecialPower (see RadarHackSpecialPower.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/RadarHackSpecialPower.h"

#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

//-------------------------------------------------------------------------------------------------
RadarHackSpecialPowerModuleData::RadarHackSpecialPowerModuleData()
{
	m_numFalseReturns = 0;
	m_radius = 0.0f;
	m_hackDuration = 0;
}

//-------------------------------------------------------------------------------------------------
/* static */ void RadarHackSpecialPowerModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	SpecialPowerModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "NumFalseReturns",	INI::parseUnsignedInt,			nullptr, offsetof( RadarHackSpecialPowerModuleData, m_numFalseReturns ) },
		{ "Radius",				INI::parseReal,					nullptr, offsetof( RadarHackSpecialPowerModuleData, m_radius ) },
		{ "HackDuration",		INI::parseDurationUnsignedInt,	nullptr, offsetof( RadarHackSpecialPowerModuleData, m_hackDuration ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
RadarHackSpecialPower::RadarHackSpecialPower( Thing *thing, const ModuleData *moduleData ) : SpecialPowerModule( thing, moduleData )
{
	m_hackEndFrame = 0;
}

//-------------------------------------------------------------------------------------------------
RadarHackSpecialPower::~RadarHackSpecialPower()
{
}

//-------------------------------------------------------------------------------------------------
void RadarHackSpecialPower::doSpecialPower( UnsignedInt commandOptions )
{
	// (used: its recharge started again; not when paused, disabled or refused)
	const UnsignedInt before = m_availableOnFrame;
	SpecialPowerModule::doSpecialPower( commandOptions );
	if( m_availableOnFrame != before )
		startHack();
}

//-------------------------------------------------------------------------------------------------
void RadarHackSpecialPower::doSpecialPowerAtObject( Object *obj, UnsignedInt commandOptions )
{
	// (used: its recharge started again; not when paused, disabled or refused)
	const UnsignedInt before = m_availableOnFrame;
	SpecialPowerModule::doSpecialPowerAtObject( obj, commandOptions );
	if( m_availableOnFrame != before )
		startHack();
}

//-------------------------------------------------------------------------------------------------
void RadarHackSpecialPower::doSpecialPowerAtLocation( const Coord3D *loc, Real angle, UnsignedInt commandOptions )
{
	// (used: its recharge started again; not when paused, disabled or refused)
	const UnsignedInt before = m_availableOnFrame;
	SpecialPowerModule::doSpecialPowerAtLocation( loc, angle, commandOptions );
	if( m_availableOnFrame != before )
		startHack();
}

//-------------------------------------------------------------------------------------------------
void RadarHackSpecialPower::startHack()
{
	m_hackEndFrame = TheGameLogic->getFrame() + getRadarHackSpecialPowerModuleData()->m_hackDuration;
	showOnRadar();
}

//-------------------------------------------------------------------------------------------------
void RadarHackSpecialPower::showOnRadar() const
{
	const RadarHackSpecialPowerModuleData *data = getRadarHackSpecialPowerModuleData();
	Drawable *draw = getObject()->getDrawable();
	if( draw != nullptr && m_hackEndFrame > TheGameLogic->getFrame() )
		draw->setRadarFalseReturns( (Int)data->m_numFalseReturns, data->m_radius, m_hackEndFrame );
}

//-------------------------------------------------------------------------------------------------
void RadarHackSpecialPower::crc( Xfer *xfer )
{
	SpecialPowerModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void RadarHackSpecialPower::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	SpecialPowerModule::xfer( xfer );
	xfer->xferUnsignedInt( &m_hackEndFrame );
}

//-------------------------------------------------------------------------------------------------
void RadarHackSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
	// (the drawable is made anew: its false returns again)
	showOnRadar();
}
