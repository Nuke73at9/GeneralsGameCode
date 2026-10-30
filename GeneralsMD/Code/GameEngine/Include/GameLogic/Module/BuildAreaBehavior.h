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

// FILE: BuildAreaBehavior.h ////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature Kane's Wrath's buildable area (its ProjectedBuildabilityInfo): a finished
// structure with this module lets its owner place a construction yard's structures within Radius of
// it. A player who has one places them only there; a player who has none places them anywhere (as
// Zero Hour does).
//
// It does nothing over time (a behavior, not an update), so it changes no object's updates.
//
//   Behavior = BuildAreaBehavior ModuleTag_BuildArea
//     Radius = 525
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/BehaviorModule.h"

class Player;

//-------------------------------------------------------------------------------------------------
class BuildAreaBehaviorModuleData : public BehaviorModuleData
{
public:
	Real m_radius;

	BuildAreaBehaviorModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class BuildAreaBehavior : public BehaviorModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( BuildAreaBehavior, "BuildAreaBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( BuildAreaBehavior, BuildAreaBehaviorModuleData )

public:

	BuildAreaBehavior( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	/// whether the player may place a construction yard's structure at this spot (in one of its
	/// finished structures' areas, or anywhere when it has none); *hasAreas: whether it has any
	static Bool isInBuildArea( const Player *player, const Coord3D *where, Bool *hasAreas = nullptr );

	/// the player's finished structures' areas, one by one (for drawing them): their positions and radii
	typedef void (*AreaFunc)( const Coord3D *center, Real radius, void *data );
	static void iterateAreas( const Player *player, AreaFunc func, void *data );

	Real getRadius() const { return getBuildAreaBehaviorModuleData()->m_radius; }

private:

	/// a finished structure of the player's, standing
	Bool counts( const Player *player ) const;

};
