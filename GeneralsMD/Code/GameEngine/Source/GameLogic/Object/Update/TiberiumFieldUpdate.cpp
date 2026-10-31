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

// FILE: TiberiumFieldUpdate.cpp //////////////////////////////////////////////////////////////////
// Tiberium Project @feature Kane's Wrath's Tiberium field (see TiberiumFieldUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/TiberiumFieldUpdate.h"

#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

//-------------------------------------------------------------------------------------------------
TiberiumFieldUpdateModuleData::TiberiumFieldUpdateModuleData()
{
	m_radius = 200.0f;
	m_spacing = 40.0f;
	m_startCrystals = 12;
	m_maxCrystals = 20;
	m_spawnDelay = LOGICFRAMES_PER_SECOND * 30;
}

//-------------------------------------------------------------------------------------------------
void TiberiumFieldUpdateModuleData::parseCrystal( INI *ini, void *instance, void *store, const void * )
{
	// Crystal = <object> <weight>
	CrystalKind kind;
	kind.name = ini->getNextAsciiString();
	const char *w = ini->getNextTokenOrNull();
	kind.weight = w != nullptr ? INI::scanInt( w ) : 1;
	( (std::vector<CrystalKind> *)store )->push_back( kind );
}

//-------------------------------------------------------------------------------------------------
void TiberiumFieldUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	static const FieldParse dataFieldParse[] =
	{
		{ "Radius",					INI::parseReal,									nullptr, offsetof( TiberiumFieldUpdateModuleData, m_radius ) },
		{ "Spacing",				INI::parseReal,									nullptr, offsetof( TiberiumFieldUpdateModuleData, m_spacing ) },
		{ "StartCrystals",	INI::parseInt,									nullptr, offsetof( TiberiumFieldUpdateModuleData, m_startCrystals ) },
		{ "MaxCrystals",		INI::parseInt,									nullptr, offsetof( TiberiumFieldUpdateModuleData, m_maxCrystals ) },
		{ "SpawnDelay",			INI::parseDurationUnsignedInt,	nullptr, offsetof( TiberiumFieldUpdateModuleData, m_spawnDelay ) },
		{ "Crystal",				parseCrystal,										nullptr, offsetof( TiberiumFieldUpdateModuleData, m_kinds ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse( p );
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
TiberiumFieldUpdate::TiberiumFieldUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_nextSpawnFrame = 0;
	m_started = FALSE;
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
TiberiumFieldUpdate::~TiberiumFieldUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumFieldUpdate::spawnOne()
{
	const TiberiumFieldUpdateModuleData *d = getTiberiumFieldUpdateModuleData();
	if( d->m_kinds.empty() || d->m_spacing <= 1.0f )
		return FALSE;

	// the grid's spots within the radius, the centre's own spot left for the field's centre piece
	const Coord3D *centre = getObject()->getPosition();
	const Int steps = (Int)( d->m_radius / d->m_spacing );
	std::vector<Coord3D> spots;
	for( Int gy = -steps; gy <= steps; ++gy )
	{
		for( Int gx = -steps; gx <= steps; ++gx )
		{
			if( gx == 0 && gy == 0 )
				continue;
			Coord3D p;
			// every other row half a step over: crystals in clumps, not in lines
			p.x = centre->x + ( gx + ( ( gy & 1 ) ? 0.5f : 0.0f ) ) * d->m_spacing;
			p.y = centre->y + gy * d->m_spacing;
			const Real dx = p.x - centre->x, dy = p.y - centre->y;
			if( dx * dx + dy * dy > d->m_radius * d->m_radius )
				continue;
			p.z = TheTerrainLogic->getGroundHeight( p.x, p.y );
			PathfindCell *cell = TheAI->pathfinder()->getCell( LAYER_GROUND, &p );
			if( cell == nullptr || cell->getType() != PathfindCell::CELL_CLEAR )
				continue;
			// not on a crystal of its own
			Bool taken = FALSE;
			for( size_t i = 0; i < m_crystals.size() && !taken; ++i )
			{
				const Object *c = TheGameLogic->findObjectByID( m_crystals[i] );
				if( c == nullptr )
					continue;
				const Real cx = c->getPosition()->x - p.x, cy = c->getPosition()->y - p.y;
				taken = cx * cx + cy * cy < 0.25f * d->m_spacing * d->m_spacing;
			}
			if( !taken )
				spots.push_back( p );
		}
	}
	if( spots.empty() )
		return FALSE;

	// a spot, and a kind by the weights
	Coord3D at = spots[ GameLogicRandomValue( 0, (Int)spots.size() - 1 ) ];
	const Real jitter = d->m_spacing * 0.25f;
	at.x += GameLogicRandomValueReal( -jitter, jitter );
	at.y += GameLogicRandomValueReal( -jitter, jitter );
	at.z = TheTerrainLogic->getGroundHeight( at.x, at.y );
	Int total = 0;
	for( size_t i = 0; i < d->m_kinds.size(); ++i )
		total += max( 0, d->m_kinds[i].weight );
	Int pick = total > 0 ? GameLogicRandomValue( 0, total - 1 ) : 0;
	const ThingTemplate *tmpl = nullptr;
	for( size_t i = 0; i < d->m_kinds.size() && tmpl == nullptr; ++i )
	{
		pick -= max( 0, d->m_kinds[i].weight );
		if( pick < 0 || i + 1 == d->m_kinds.size() )
			tmpl = TheThingFactory->findTemplate( d->m_kinds[i].name );
	}
	if( tmpl == nullptr )
		return FALSE;

	Object *crystal = TheThingFactory->newObject( tmpl, getObject()->getTeam() );
	if( crystal == nullptr )
		return FALSE;
	crystal->setPosition( &at );
	crystal->setOrientation( GameLogicRandomValueReal( 0.0f, 2.0f * PI ) );
	m_crystals.push_back( crystal->getID() );
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime TiberiumFieldUpdate::update()
{
	const TiberiumFieldUpdateModuleData *d = getTiberiumFieldUpdateModuleData();
	const UnsignedInt now = TheGameLogic->getFrame();

	// gone crystals (a script, a weapon) leave their spot
	for( size_t i = 0; i < m_crystals.size(); )
	{
		if( TheGameLogic->findObjectByID( m_crystals[i] ) == nullptr )
			m_crystals.erase( m_crystals.begin() + i );
		else
			++i;
	}

	if( !m_started )
	{
		m_started = TRUE;
		for( Int i = 0; i < d->m_startCrystals && (Int)m_crystals.size() < d->m_maxCrystals; ++i )
		{
			if( !spawnOne() )
				break;
		}
		m_nextSpawnFrame = now + d->m_spawnDelay;
	}
	else if( now >= m_nextSpawnFrame )
	{
		if( (Int)m_crystals.size() < d->m_maxCrystals )
			spawnOne();
		m_nextSpawnFrame = now + d->m_spawnDelay;
	}
	return UPDATE_SLEEP( max( (UnsignedInt)1, m_nextSpawnFrame > now ? m_nextSpawnFrame - now : 1 ) );
}

//-------------------------------------------------------------------------------------------------
void TiberiumFieldUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void TiberiumFieldUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	UnsignedShort count = (UnsignedShort)m_crystals.size();
	xfer->xferUnsignedShort( &count );
	if( xfer->getXferMode() == XFER_LOAD )
		m_crystals.resize( count );
	for( UnsignedShort i = 0; i < count; ++i )
		xfer->xferObjectID( &m_crystals[i] );
	xfer->xferUnsignedInt( &m_nextSpawnFrame );
	xfer->xferBool( &m_started );
}

//-------------------------------------------------------------------------------------------------
void TiberiumFieldUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
