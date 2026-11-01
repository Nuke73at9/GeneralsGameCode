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

// FILE: TiberiumZones.cpp ////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's zones in a skirmish (see TiberiumZones.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/TiberiumZones.h"

#include "Common/GameUtility.h"
#include "Common/GlobalData.h"
#include "Common/MapObject.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/TiberiumZoneNodeUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameNetwork/GameInfo.h"

#include <functional>
#include <queue>
#include <vector>

namespace
{
	const Real CELL = 20.0f;										///< the grid's cell, in world units
	const Int STEP = 10;												///< a cell's step in the fill's costs (a diagonal: 14)
	const Int STEP_DIAGONAL = 14;
	const UnsignedInt OWNERS_EVERY = LOGICFRAMES_PER_SECOND;
	const Real CLUSTER = 320.0f;								///< supplies and tech buildings nearer than this make one zone

	struct Zone
	{
		Coord3D center;
		Bool home;
		PlayerMaskType homeOwners;	///< a start zone: its players
		PlayerMaskType owners;			///< who holds it now
		Int cells;
	};

	struct Seed
	{
		Int cell;
		Int zone;
		Int limit;		///< how far its fill reaches, in costs
	};

	struct Step
	{
		Int cost;
		Int cell;
		Int zone;
		Int limit;
		/// the order the fill takes them in: cheapest first, then by cell and zone (the same on every machine)
		Bool operator>( const Step &o ) const
		{
			if( cost != o.cost )
				return cost > o.cost;
			if( cell != o.cell )
				return cell > o.cell;
			return zone > o.zone;
		}
	};

	AbramZoneMode s_mode = ABRAM_ZONES_OFF;
	Int s_w = 0;
	Int s_h = 0;
	Real s_ox = 0.0f;
	Real s_oy = 0.0f;
	std::vector<UnsignedByte> s_grid;		///< each cell's zone (NO_ZONE: none)
	std::vector<UnsignedByte> s_land;		///< each cell: ground a base could stand on
	std::vector<Zone> s_zones;
	Bool s_ownersStale = FALSE;					///< a loaded game: who holds what, once its objects are in

	//-----------------------------------------------------------------------------------------------
	Int cellOf( Real x, Real y )
	{
		const Int cx = (Int)floorf( ( x - s_ox ) / CELL ), cy = (Int)floorf( ( y - s_oy ) / CELL );
		if( cx < 0 || cy < 0 || cx >= s_w || cy >= s_h )
			return -1;
		return cy * s_w + cx;
	}

	//-----------------------------------------------------------------------------------------------
	void cellCenter( Int cell, Coord3D *pos )
	{
		pos->x = s_ox + ( cell % s_w ) * CELL + CELL * 0.5f;
		pos->y = s_oy + ( cell / s_w ) * CELL + CELL * 0.5f;
		pos->z = TheTerrainLogic->getGroundHeight( pos->x, pos->y );
	}

	//-----------------------------------------------------------------------------------------------
	/** land: the pathfinder's ground (clear, rubble, or a structure on it), not under water */
	Bool isLandAt( const Coord3D *pos )
	{
		PathfindCell *pc = TheAI->pathfinder()->getCell( LAYER_GROUND, pos );
		if( pc == nullptr )
			return FALSE;
		const PathfindCell::CellType type = pc->getType();
		if( type != PathfindCell::CELL_CLEAR && type != PathfindCell::CELL_RUBBLE && type != PathfindCell::CELL_OBSTACLE )
			return FALSE;
		return !TheTerrainLogic->isUnderwater( pos->x, pos->y );
	}

	//-----------------------------------------------------------------------------------------------
	/** the land cell nearest 'cell' within 'reach' cells (-1: none) */
	Int nearestLand( Int cell, Int reach )
	{
		if( cell < 0 )
			return -1;
		if( s_land[cell] )
			return cell;
		const Int cx = cell % s_w, cy = cell / s_w;
		for( Int r = 1; r <= reach; ++r )
		{
			Int best = -1, bestD = 0x7FFFFFFF;
			for( Int dy = -r; dy <= r; ++dy )
				for( Int dx = -r; dx <= r; ++dx )
				{
					if( abs( dx ) != r && abs( dy ) != r )
						continue;
					const Int x = cx + dx, y = cy + dy;
					if( x < 0 || y < 0 || x >= s_w || y >= s_h || !s_land[y * s_w + x] )
						continue;
					const Int d = dx * dx + dy * dy;
					if( d < bestD )
					{
						bestD = d;
						best = y * s_w + x;
					}
				}
			if( best >= 0 )
				return best;
		}
		return -1;
	}

	//-----------------------------------------------------------------------------------------------
	/** each land cell to the zone of the seed nearest it over land, as far as that seed reaches */
	void fill( const std::vector<Seed> &seeds )
	{
		const Int n = s_w * s_h;
		std::vector<Int> best( n, 0x7FFFFFFF );
		s_grid.assign( n, (UnsignedByte)TiberiumZones::NO_ZONE );
		std::priority_queue< Step, std::vector<Step>, std::greater<Step> > open;
		for( size_t i = 0; i < seeds.size(); ++i )
		{
			const Seed &s = seeds[i];
			if( s.cell < 0 || !s_land[s.cell] || best[s.cell] == 0 )
				continue;
			best[s.cell] = 0;
			s_grid[s.cell] = (UnsignedByte)s.zone;
			Step st = { 0, s.cell, s.zone, s.limit };
			open.push( st );
		}
		static const Int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		static const Int DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
		while( !open.empty() )
		{
			const Step st = open.top();
			open.pop();
			if( st.cost > best[st.cell] || s_grid[st.cell] != st.zone )
				continue;
			const Int cx = st.cell % s_w, cy = st.cell / s_w;
			for( Int k = 0; k < 8; ++k )
			{
				const Int x = cx + DX[k], y = cy + DY[k];
				if( x < 0 || y < 0 || x >= s_w || y >= s_h )
					continue;
				const Int c = y * s_w + x;
				if( !s_land[c] )
					continue;
				// a diagonal step only between land on both sides (no slipping through a gap)
				if( k >= 4 && ( !s_land[cy * s_w + x] || !s_land[y * s_w + cx] ) )
					continue;
				const Int cost = st.cost + ( k >= 4 ? STEP_DIAGONAL : STEP );
				if( cost > st.limit || cost >= best[c] )
					continue;
				best[c] = cost;
				s_grid[c] = (UnsignedByte)st.zone;
				Step next = { cost, c, st.zone, st.limit };
				open.push( next );
			}
		}
	}

	//-----------------------------------------------------------------------------------------------
	Bool isPlayablePlayer( const Player *player )
	{
		if( player == nullptr || ThePlayerList == nullptr || player == ThePlayerList->getNeutralPlayer() )
			return FALSE;
		return player->getPlayerType() == PLAYER_HUMAN || player->getPlayerType() == PLAYER_COMPUTER;
	}

	//-----------------------------------------------------------------------------------------------
	/// whether the template has a module whose name contains 'part' (BaseConstructionUpdate, or one of the
	/// production exits: Default, Queue, SupplyCenter, SpawnPoint ProductionExitUpdate)
	Bool hasModule( const ThingTemplate *t, const char *part )
	{
		const ModuleInfo &info = t->getBehaviorModuleInfo();
		for( Int i = 0; i < info.getCount(); ++i )
			if( strstr( info.getNthName( i ).str(), part ) != nullptr )
				return TRUE;
		return FALSE;
	}

	//-----------------------------------------------------------------------------------------------
	/** who holds each expansion zone: the player of its oldest claim (a captured node, or a finished
		outpost, factory or deployed Crawler in it). The oldest claim keeps it, so a zone changes hands
		only when its holder loses all of its claims there (or its node). */
	void updateOwners()
	{
		const Int count = (Int)s_zones.size();
		std::vector<ObjectID> oldest( count, INVALID_ID );
		std::vector<PlayerMaskType> holder( count, 0 );
		for( Int z = 0; z < count; ++z )
			s_zones[z].owners = s_zones[z].home ? s_zones[z].homeOwners : 0;
		if( s_mode == ABRAM_ZONES_PRE )
			return;
		for( Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject() )
		{
			if( obj->isEffectivelyDead() )
				continue;
			const Player *player = obj->getControllingPlayer();
			if( !isPlayablePlayer( player ) )
				continue;
			if( TiberiumZoneNodeUpdate::isNode( obj ) == FALSE )
			{
				if( !obj->isKindOf( KINDOF_STRUCTURE ) )
					continue;
				if( obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) || obj->testStatus( OBJECT_STATUS_SOLD ) )
					continue;
				if( !TiberiumZones::isClaimer( obj->getTemplate() ) )
					continue;
			}
			const Int z = TiberiumZones::zoneAt( obj->getPosition() );
			if( z < 0 || s_zones[z].home )
				continue;
			if( oldest[z] == INVALID_ID || obj->getID() < oldest[z] )
			{
				oldest[z] = obj->getID();
				holder[z] = player->getPlayerMask();
			}
		}
		for( Int z = 0; z < count; ++z )
			if( !s_zones[z].home )
				s_zones[z].owners = holder[z];
	}

	//-----------------------------------------------------------------------------------------------
	/** C&C4's node (the TCN) in each expansion zone, on a clear spot near its middle */
	void placeNodes()
	{
		if( TheGlobalData->m_zoneNode.isEmpty() || TheThingFactory == nullptr )
			return;
		const ThingTemplate *node = TheThingFactory->findTemplate( TheGlobalData->m_zoneNode );
		if( node == nullptr )
		{
			DEBUG_LOG(( "TiberiumZones: no node object %s; the zones are taken by outposts and Crawlers only",
				TheGlobalData->m_zoneNode.str() ));
			return;
		}
		Team *neutral = ThePlayerList->getNeutralPlayer()->getDefaultTeam();
		for( size_t z = 0; z < s_zones.size(); ++z )
		{
			if( s_zones[z].home )
				continue;
			Coord3D at = s_zones[z].center;
			FindPositionOptions fpo;
			fpo.minRadius = 0.0f;
			fpo.maxRadius = 150.0f;
			fpo.startAngle = 0.0f;
			Coord3D found;
			if( ThePartitionManager->findPositionAround( &at, &fpo, &found ) && TiberiumZones::zoneAt( &found ) == (Int)z )
				at = found;
			at.z = TheTerrainLogic->getGroundHeight( at.x, at.y );
			Object *obj = TheThingFactory->newObject( node, neutral );
			if( obj == nullptr )
				continue;
			obj->setPosition( &at );
			obj->setOrientation( 0.0f );
		}
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumZones::reset()
{
	s_mode = ABRAM_ZONES_OFF;
	s_ownersStale = FALSE;
	s_w = s_h = 0;
	s_grid.clear();
	s_land.clear();
	s_zones.clear();
}

//-------------------------------------------------------------------------------------------------
void TiberiumZones::build( Bool loadingSaveGame )
{
	reset();
	const AbramZoneMode mode = AbramGameRules::getZoneMode();
	if( mode == ABRAM_ZONES_OFF || TheGameInfo == nullptr || TheTerrainLogic == nullptr || TheAI == nullptr || ThePlayerList == nullptr )
		return;

	// the grid over the playable map, and its land
	Region3D extent;
	TheTerrainLogic->getMaximumPathfindExtent( &extent );
	s_ox = extent.lo.x;
	s_oy = extent.lo.y;
	s_w = (Int)ceilf( ( extent.hi.x - extent.lo.x ) / CELL );
	s_h = (Int)ceilf( ( extent.hi.y - extent.lo.y ) / CELL );
	if( s_w <= 0 || s_h <= 0 )
	{
		reset();
		return;
	}
	s_land.assign( s_w * s_h, 0 );
	for( Int c = 0; c < s_w * s_h; ++c )
	{
		Coord3D pos;
		cellCenter( c, &pos );
		s_land[c] = isLandAt( &pos ) ? 1 : 0;
	}

	const Int homeLimit = (Int)( TheGlobalData->m_zoneHomeRadius / CELL * STEP );
	const Int expansionLimit = (Int)( TheGlobalData->m_zoneExpansionRadius / CELL * STEP );
	std::vector<Seed> seeds;

	// the starts: one zone each, or one for allies' starts near each other (bridged along the way)
	struct Start { Coord3D pos; Int team; PlayerMaskType mask; Int group; };
	std::vector<Start> starts;
	for( Int i = 0; i < MAX_SLOTS; ++i )
	{
		const GameSlot *slot = TheGameInfo->getConstSlot( i );
		if( slot == nullptr || !slot->isOccupied() || slot->getStartPos() < 0 )
			continue;
		Player *player = ThePlayerList->getPlayerFromSlotIndex( i );
		if( player == nullptr || player->isPlayerObserver() )
			continue;
		AsciiString name;
		name.format( "Player_%d_Start", slot->getStartPos() + 1 );
		Waypoint *wp = TheTerrainLogic->getWaypointByName( name );
		if( wp == nullptr )
			continue;
		Start s;
		s.pos = *wp->getLocation();
		s.team = slot->getTeamNumber();
		s.mask = player->getPlayerMask();
		s.group = (Int)starts.size();
		starts.push_back( s );
	}
	if( starts.empty() )
	{
		reset();
		return;
	}
	const Real link2 = TheGlobalData->m_zoneLinkDistance * TheGlobalData->m_zoneLinkDistance;
	std::vector< std::pair<Int, Int> > links;
	for( size_t i = 0; i < starts.size(); ++i )
		for( size_t j = i + 1; j < starts.size(); ++j )
		{
			if( starts[i].team < 0 || starts[i].team != starts[j].team )
				continue;
			const Real dx = starts[i].pos.x - starts[j].pos.x, dy = starts[i].pos.y - starts[j].pos.y;
			if( dx * dx + dy * dy > link2 )
				continue;
			links.push_back( std::make_pair( (Int)i, (Int)j ) );
			const Int from = starts[j].group, to = starts[i].group;
			for( size_t k = 0; k < starts.size(); ++k )
				if( starts[k].group == from )
					starts[k].group = to;
		}
	for( size_t i = 0; i < starts.size(); ++i )
	{
		Int zone = -1;
		for( size_t z = 0; z < s_zones.size(); ++z )
			if( s_zones[z].home && (Int)s_zones[z].cells == starts[i].group )	// (cells holds the group while building)
				zone = (Int)z;
		if( zone < 0 )
		{
			Zone nz;
			nz.center = starts[i].pos;
			nz.home = TRUE;
			nz.homeOwners = 0;
			nz.owners = 0;
			nz.cells = starts[i].group;
			s_zones.push_back( nz );
			zone = (Int)s_zones.size() - 1;
		}
		s_zones[zone].homeOwners |= starts[i].mask;
		Seed s = { nearestLand( cellOf( starts[i].pos.x, starts[i].pos.y ), 12 ), zone, homeLimit };
		seeds.push_back( s );
	}
	for( size_t l = 0; l < links.size(); ++l )
	{
		const Start &a = starts[links[l].first], &b = starts[links[l].second];
		Int zone = -1;
		for( size_t z = 0; z < s_zones.size(); ++z )
			if( s_zones[z].home && s_zones[z].cells == a.group )
				zone = (Int)z;
		const Real dx = b.pos.x - a.pos.x, dy = b.pos.y - a.pos.y;
		const Int steps = (Int)( sqrtf( dx * dx + dy * dy ) / ( CELL * 4.0f ) );
		for( Int k = 1; k < steps; ++k )
		{
			const Real t = (Real)k / (Real)steps;
			const Int c = cellOf( a.pos.x + dx * t, a.pos.y + dy * t );
			if( c >= 0 && s_land[c] )
			{
				Seed s = { c, zone, homeLimit * 3 / 5 };
				seeds.push_back( s );
			}
		}
	}

	// the expansion zones: around the supplies and the tech buildings away from the starts
	struct Cluster { Real x, y; Int n; };
	std::vector<Cluster> clusters;
	const Real home2 = TheGlobalData->m_zoneHomeRadius * TheGlobalData->m_zoneHomeRadius;
	for( MapObject *mo = MapObject::getFirstMapObject(); mo != nullptr; mo = mo->getNext() )
	{
		const ThingTemplate *t = mo->getThingTemplate();
		if( t == nullptr || !( t->isKindOf( KINDOF_SUPPLY_SOURCE ) || t->isKindOf( KINDOF_TECH_BUILDING ) ) )
			continue;
		const Coord3D *p = mo->getLocation();
		Bool nearStart = FALSE;
		for( size_t i = 0; i < starts.size() && !nearStart; ++i )
		{
			const Real dx = starts[i].pos.x - p->x, dy = starts[i].pos.y - p->y;
			nearStart = dx * dx + dy * dy < home2;
		}
		if( nearStart )
			continue;
		Bool merged = FALSE;
		for( size_t c = 0; c < clusters.size() && !merged; ++c )
		{
			const Real cx = clusters[c].x / clusters[c].n, cy = clusters[c].y / clusters[c].n;
			if( ( cx - p->x ) * ( cx - p->x ) + ( cy - p->y ) * ( cy - p->y ) < CLUSTER * CLUSTER )
			{
				clusters[c].x += p->x;
				clusters[c].y += p->y;
				++clusters[c].n;
				merged = TRUE;
			}
		}
		if( !merged )
		{
			Cluster c = { p->x, p->y, 1 };
			clusters.push_back( c );
		}
	}
	for( size_t c = 0; c < clusters.size() && s_zones.size() < MAX_ZONES; ++c )
	{
		const Int cell = nearestLand( cellOf( clusters[c].x / clusters[c].n, clusters[c].y / clusters[c].n ), 10 );
		if( cell < 0 )
			continue;
		Zone nz;
		cellCenter( cell, &nz.center );
		nz.home = FALSE;
		nz.homeOwners = nz.owners = 0;
		nz.cells = -1;
		s_zones.push_back( nz );
		Seed s = { cell, (Int)s_zones.size() - 1, expansionLimit };
		seeds.push_back( s );
	}
	fill( seeds );

	// the land left: zones of its own where there is room for one
	{
		const Int r = max( 2, (Int)( TheGlobalData->m_zoneExpansionRadius / CELL ) );
		const Int spacing = max( 4, r * 17 / 10 );
		const Int probe = max( 1, r * 6 / 10 );
		Bool added = FALSE;
		for( Int y = spacing / 2; y < s_h && s_zones.size() < MAX_ZONES; y += spacing )
			for( Int x = spacing / 2; x < s_w && s_zones.size() < MAX_ZONES; x += spacing )
			{
				const Int c = y * s_w + x;
				if( !s_land[c] || s_grid[c] != NO_ZONE )
					continue;
				Int free = 0, all = 0;
				for( Int dy = -probe; dy <= probe; ++dy )
					for( Int dx = -probe; dx <= probe; ++dx )
					{
						if( dx * dx + dy * dy > probe * probe )
							continue;
						++all;
						const Int px = x + dx, py = y + dy;
						if( px >= 0 && py >= 0 && px < s_w && py < s_h && s_land[py * s_w + px] && s_grid[py * s_w + px] == NO_ZONE )
							++free;
					}
				if( free * 2 < all )
					continue;
				Zone nz;
				cellCenter( c, &nz.center );
				nz.home = FALSE;
				nz.homeOwners = nz.owners = 0;
				nz.cells = -1;
				s_zones.push_back( nz );
				Seed s = { c, (Int)s_zones.size() - 1, expansionLimit };
				seeds.push_back( s );
				added = TRUE;
			}
		if( added )
			fill( seeds );
	}

	// the zones too small to hold a base go; the rest numbered again, each with its middle in it
	{
		const Int n = s_w * s_h;
		std::vector<Int> cells( s_zones.size(), 0 );
		std::vector<Real> sx( s_zones.size(), 0.0f ), sy( s_zones.size(), 0.0f );
		for( Int c = 0; c < n; ++c )
			if( s_grid[c] != NO_ZONE )
			{
				++cells[s_grid[c]];
				sx[s_grid[c]] += (Real)( c % s_w );
				sy[s_grid[c]] += (Real)( c / s_w );
			}
		const Real r = TheGlobalData->m_zoneExpansionRadius / CELL;
		const Int least = max( 8, (Int)( 3.14159f * r * r * 0.15f ) );
		std::vector<Int> renumber( s_zones.size(), -1 );
		std::vector<Zone> kept;
		for( size_t z = 0; z < s_zones.size(); ++z )
		{
			if( cells[z] == 0 || ( !s_zones[z].home && cells[z] < least ) )
				continue;
			renumber[z] = (Int)kept.size();
			Zone k = s_zones[z];
			k.cells = cells[z];
			// the zone's cell nearest its middle
			const Real mx = sx[z] / cells[z], my = sy[z] / cells[z];
			Int bestC = -1;
			Real bestD = 1e30f;
			for( Int c = 0; c < n; ++c )
			{
				if( s_grid[c] != z )
					continue;
				const Real dx = (Real)( c % s_w ) - mx, dy = (Real)( c / s_w ) - my;
				const Real d = dx * dx + dy * dy;
				if( d < bestD )
				{
					bestD = d;
					bestC = c;
				}
			}
			if( !k.home && bestC >= 0 )
				cellCenter( bestC, &k.center );
			kept.push_back( k );
		}
		for( Int c = 0; c < n; ++c )
			if( s_grid[c] != NO_ZONE )
				s_grid[c] = renumber[s_grid[c]] < 0 ? (UnsignedByte)NO_ZONE : (UnsignedByte)renumber[s_grid[c]];
		s_zones = kept;
	}

	s_mode = mode;
	if( !loadingSaveGame && ( mode == ABRAM_ZONES_EXPANSION || mode == ABRAM_ZONES_FREE ) )
		placeNodes();
	updateOwners();
	s_ownersStale = loadingSaveGame;
	DEBUG_LOG(( "TiberiumZones: mode %d, %d zones on a %dx%d grid", (Int)mode, (Int)s_zones.size(), s_w, s_h ));
}

//-------------------------------------------------------------------------------------------------
void TiberiumZones::update()
{
	if( s_mode == ABRAM_ZONES_OFF || TheGameLogic == nullptr )
		return;
	if( s_ownersStale || TheGameLogic->getFrame() % OWNERS_EVERY == 0 )
		updateOwners();
	s_ownersStale = FALSE;
}

//-------------------------------------------------------------------------------------------------
AbramZoneMode TiberiumZones::getMode()
{
	return s_mode;
}

//-------------------------------------------------------------------------------------------------
Int TiberiumZones::getCount()
{
	return (Int)s_zones.size();
}

//-------------------------------------------------------------------------------------------------
Int TiberiumZones::zoneAt( const Coord3D *pos )
{
	if( s_mode == ABRAM_ZONES_OFF || pos == nullptr )
		return -1;
	const Int c = cellOf( pos->x, pos->y );
	if( c < 0 || s_grid[c] == NO_ZONE )
		return -1;
	return s_grid[c];
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumZones::isHome( Int zone )
{
	return zone >= 0 && zone < (Int)s_zones.size() && s_zones[zone].home;
}

//-------------------------------------------------------------------------------------------------
PlayerMaskType TiberiumZones::getOwners( Int zone )
{
	return ( zone >= 0 && zone < (Int)s_zones.size() ) ? s_zones[zone].owners : 0;
}

//-------------------------------------------------------------------------------------------------
const Coord3D *TiberiumZones::getCenter( Int zone )
{
	return ( zone >= 0 && zone < (Int)s_zones.size() ) ? &s_zones[zone].center : nullptr;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumZones::isOwnedBy( Int zone, const Player *player )
{
	return player != nullptr && ( getOwners( zone ) & player->getPlayerMask() ) != 0;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumZones::isClaimer( const ThingTemplate *build )
{
	if( build == nullptr || !build->isKindOf( KINDOF_STRUCTURE ) )
		return FALSE;
	if( build->isKindOf( KINDOF_COMMANDCENTER ) || build->isKindOf( KINDOF_FS_FACTORY ) )
		return TRUE;
	// what units come out of (a factory, a refinery, a deployed Crawler) or a base is built from; not
	// what only researches (a power plant's Control Rods)
	return hasModule( build, "ProductionExitUpdate" ) || hasModule( build, "BaseConstructionUpdate" );
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumZones::mayBuild( const Player *player, const ThingTemplate *build, const Coord3D *pos )
{
	if( s_mode != ABRAM_ZONES_PRE && s_mode != ABRAM_ZONES_EXPANSION )
		return TRUE;
	if( build == nullptr || !build->isKindOf( KINDOF_STRUCTURE ) || !isPlayablePlayer( player ) )
		return TRUE;
	const Int zone = zoneAt( pos );
	if( zone < 0 )
		return FALSE;
	if( isOwnedBy( zone, player ) )
		return TRUE;
	// an expansion zone that is nobody's: taken by building an outpost (or a factory) in it
	return s_mode == ABRAM_ZONES_EXPANSION && !s_zones[zone].home && s_zones[zone].owners == 0 && isClaimer( build );
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumZones::findArrival( const Player *player, const Coord3D *near, Coord3D *spot )
{
	if( s_mode == ABRAM_ZONES_OFF || player == nullptr || spot == nullptr )
		return FALSE;
	Int best = -1;
	Real bestD = 1e30f;
	for( size_t z = 0; z < s_zones.size(); ++z )
	{
		if( !isOwnedBy( (Int)z, player ) )
			continue;
		Real d = 0.0f;
		if( near != nullptr )
		{
			const Real dx = s_zones[z].center.x - near->x, dy = s_zones[z].center.y - near->y;
			d = dx * dx + dy * dy;
		}
		// without a place asked for, the player's start zone
		else if( !s_zones[z].home )
			d = 1.0f;
		if( d < bestD )
		{
			bestD = d;
			best = (Int)z;
		}
	}
	if( best < 0 )
		return FALSE;
	Coord3D at = s_zones[best].center;
	if( near != nullptr && zoneAt( near ) == best )
		at = *near;
	FindPositionOptions fpo;
	fpo.minRadius = 0.0f;
	fpo.maxRadius = 200.0f;
	fpo.startAngle = 0.0f;
	Coord3D found;
	if( ThePartitionManager->findPositionAround( &at, &fpo, &found ) && zoneAt( &found ) == best )
		at = found;
	at.z = TheTerrainLogic->getGroundHeight( at.x, at.y );
	*spot = at;
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Int TiberiumZones::getGridWidth() { return s_mode == ABRAM_ZONES_OFF ? 0 : s_w; }
Int TiberiumZones::getGridHeight() { return s_mode == ABRAM_ZONES_OFF ? 0 : s_h; }
Real TiberiumZones::getCellSize() { return CELL; }

void TiberiumZones::getGridOrigin( Real *x, Real *y )
{
	*x = s_ox;
	*y = s_oy;
}

Int TiberiumZones::cellZone( Int x, Int y )
{
	if( s_mode == ABRAM_ZONES_OFF || x < 0 || y < 0 || x >= s_w || y >= s_h )
		return -1;
	const UnsignedByte z = s_grid[y * s_w + x];
	return z == NO_ZONE ? -1 : (Int)z;
}
