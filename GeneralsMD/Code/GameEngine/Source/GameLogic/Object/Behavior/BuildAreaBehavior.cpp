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

// FILE: BuildAreaBehavior.cpp //////////////////////////////////////////////////////////////////////
// Tiberium Project @feature Kane's Wrath's buildable area (see BuildAreaBehavior.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/BuildAreaBehavior.h"

#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameLogic/Object.h"

#include <vector>

namespace
{
	/// every area in the game (each module adds itself while it lives); the questions asked of them
	/// (whether a spot is in one) do not depend on their order
	std::vector<BuildAreaBehavior *> s_areas;
}

//-------------------------------------------------------------------------------------------------
BuildAreaBehaviorModuleData::BuildAreaBehaviorModuleData()
{
	m_radius = 0.0f;
}

//-------------------------------------------------------------------------------------------------
void BuildAreaBehaviorModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	static const FieldParse dataFieldParse[] =
	{
		{ "Radius",	INI::parseReal,	nullptr, offsetof( BuildAreaBehaviorModuleData, m_radius ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	BehaviorModuleData::buildFieldParse( p );
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
BuildAreaBehavior::BuildAreaBehavior( Thing *thing, const ModuleData* moduleData ) : BehaviorModule( thing, moduleData )
{
	s_areas.push_back( this );
}

//-------------------------------------------------------------------------------------------------
BuildAreaBehavior::~BuildAreaBehavior()
{
	for( size_t i = 0; i < s_areas.size(); ++i )
	{
		if( s_areas[i] == this )
		{
			s_areas.erase( s_areas.begin() + i );
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
Bool BuildAreaBehavior::counts( const Player *player ) const
{
	const Object *obj = getObject();
	return getRadius() > 0.0f && obj->getControllingPlayer() == player && !obj->isEffectivelyDead()
		&& !obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) && !obj->testStatus( OBJECT_STATUS_SOLD );
}

//-------------------------------------------------------------------------------------------------
Bool BuildAreaBehavior::isInBuildArea( const Player *player, const Coord3D *where, Bool *hasAreas )
{
	Bool any = FALSE;
	Bool inside = FALSE;
	for( size_t i = 0; i < s_areas.size() && !inside; ++i )
	{
		const BuildAreaBehavior *area = s_areas[i];
		if( !area->counts( player ) )
			continue;
		any = TRUE;
		const Coord3D *at = area->getObject()->getPosition();
		const Real dx = at->x - where->x, dy = at->y - where->y, r = area->getRadius();
		if( dx * dx + dy * dy <= r * r )
			inside = TRUE;
	}
	if( hasAreas != nullptr )
		*hasAreas = any;
	return inside || !any;
}

//-------------------------------------------------------------------------------------------------
void BuildAreaBehavior::iterateAreas( const Player *player, AreaFunc func, void *data )
{
	for( size_t i = 0; i < s_areas.size(); ++i )
	{
		if( s_areas[i]->counts( player ) )
			func( s_areas[i]->getObject()->getPosition(), s_areas[i]->getRadius(), data );
	}
}

//-------------------------------------------------------------------------------------------------
void BuildAreaBehavior::crc( Xfer *xfer )
{
	BehaviorModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void BuildAreaBehavior::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	BehaviorModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void BuildAreaBehavior::loadPostProcess()
{
	BehaviorModule::loadPostProcess();
}
