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

// FILE: TiberiumZoneView.cpp /////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's zones shown (see TiberiumZoneView.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/TiberiumZoneView.h"

#include "Common/GameUtility.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Radar.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/TiberiumZoneNodeUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/TiberiumZones.h"

#include <vector>

namespace
{
	/// a piece of a zone's border (marching squares over the cells' middles: straight and 45 degree
	/// pieces, not the cells' steps), and the way into the zone (where its line is drawn, a little inside)
	struct Edge
	{
		Real x0, y0, x1, y1;
		Real inX, inY;
		Int zone;
	};

	Bool s_shown = FALSE;					///< the borders over the world, asked for
	std::vector<Edge> s_edges;
	Int s_edgesW = -1, s_edgesH = -1, s_edgesCount = -1;
	std::vector<ObjectID> s_nodes;
	UnsignedInt s_nodesFrame = 0xFFFFFFFF;
	std::vector<DisplayString *> s_labels;
	DisplayString *s_small = nullptr;
	Int s_fontFor = 0;

	GameWindow *s_radar = nullptr;
	GameWinDrawFunc s_radarDraw = nullptr;

	//-----------------------------------------------------------------------------------------------
	void buildEdges()
	{
		const Int w = TiberiumZones::getGridWidth(), h = TiberiumZones::getGridHeight();
		if( w == s_edgesW && h == s_edgesH && TiberiumZones::getCount() == s_edgesCount )
			return;
		s_edgesW = w;
		s_edgesH = h;
		s_edgesCount = TiberiumZones::getCount();
		s_edges.clear();
		Real ox = 0.0f, oy = 0.0f;
		TiberiumZones::getGridOrigin( &ox, &oy );
		const Real c = TiberiumZones::getCellSize();
		// a block's corners (counter-clockwise from its lower left) and its sides' middles
		static const Int CX[4] = { 0, 1, 1, 0 };
		static const Int CY[4] = { 0, 0, 1, 1 };
		static const Real MX[4] = { 0.5f, 1.0f, 0.5f, 0.0f };	// bottom, right, top, left
		static const Real MY[4] = { 0.0f, 0.5f, 1.0f, 0.5f };
		// each case's pieces (side to side) and the corner of the zone each bounds (-1: the zone's corners)
		struct Piece { Int from, to, corner; };
		static const Piece NONE = { -1, -1, -1 };
		static const Piece CASES[16][2] =
		{
			{ NONE, NONE }, { { 3, 0, 0 }, NONE }, { { 0, 1, 1 }, NONE }, { { 3, 1, -1 }, NONE },
			{ { 1, 2, 2 }, NONE }, { { 3, 0, 0 }, { 1, 2, 2 } }, { { 0, 2, -1 }, NONE }, { { 3, 2, -1 }, NONE },
			{ { 2, 3, 3 }, NONE }, { { 0, 2, -1 }, NONE }, { { 0, 1, 1 }, { 2, 3, 3 } }, { { 1, 2, -1 }, NONE },
			{ { 1, 3, -1 }, NONE }, { { 0, 1, -1 }, NONE }, { { 0, 3, -1 }, NONE }, { NONE, NONE }
		};
		for( Int by = -1; by < h; ++by )
			for( Int bx = -1; bx < w; ++bx )
			{
				Int z[4];
				for( Int k = 0; k < 4; ++k )
					z[k] = TiberiumZones::cellZone( bx + CX[k], by + CY[k] );
				for( Int k = 0; k < 4; ++k )
				{
					const Int zone = z[k];
					Bool seen = zone < 0;
					for( Int j = 0; j < k && !seen; ++j )
						seen = z[j] == zone;
					if( seen )
						continue;
					Int index = 0;
					Real sx = 0.0f, sy = 0.0f;
					Int in = 0;
					for( Int j = 0; j < 4; ++j )
						if( z[j] == zone )
						{
							index |= 1 << j;
							sx += (Real)CX[j];
							sy += (Real)CY[j];
							++in;
						}
					for( Int p = 0; p < 2; ++p )
					{
						const Piece &piece = CASES[index][p];
						if( piece.from < 0 )
							continue;
						const Real ix = piece.corner >= 0 ? (Real)CX[piece.corner] : sx / in;
						const Real iy = piece.corner >= 0 ? (Real)CY[piece.corner] : sy / in;
						Edge e;
						// cell middles are at (x + 0.5) * c
						e.x0 = ox + ( bx + 0.5f + MX[piece.from] ) * c;
						e.y0 = oy + ( by + 0.5f + MY[piece.from] ) * c;
						e.x1 = ox + ( bx + 0.5f + MX[piece.to] ) * c;
						e.y1 = oy + ( by + 0.5f + MY[piece.to] ) * c;
						const Real mx = ( MX[piece.from] + MX[piece.to] ) * 0.5f, my = ( MY[piece.from] + MY[piece.to] ) * 0.5f;
						Real dx = ix - mx, dy = iy - my;
						const Real len = sqrtf( dx * dx + dy * dy );
						e.inX = len > 0.0f ? dx / len : 0.0f;
						e.inY = len > 0.0f ? dy / len : 0.0f;
						e.zone = zone;
						s_edges.push_back( e );
					}
				}
			}
	}

	//-----------------------------------------------------------------------------------------------
	Player *localPlayer()
	{
		return ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
	}

	/// the first player of a mask (the local player when it is one of them)
	Player *ownerOf( Int zone )
	{
		const PlayerMaskType owners = TiberiumZones::getOwners( zone );
		if( owners == 0 || ThePlayerList == nullptr )
			return nullptr;
		Player *me = localPlayer();
		if( me != nullptr && ( owners & me->getPlayerMask() ) != 0 )
			return me;
		for( Int i = 0; i < ThePlayerList->getPlayerCount(); ++i )
		{
			Player *p = ThePlayerList->getNthPlayer( i );
			if( p != nullptr && ( owners & p->getPlayerMask() ) != 0 )
				return p;
		}
		return nullptr;
	}

	Color colorOf( Int zone, Int alpha )
	{
		const Player *owner = ownerOf( zone );
		if( owner == nullptr )
			return GameMakeColor( 175, 175, 175, alpha );
		UnsignedByte r, g, b, a;
		GameGetColorComponents( owner->getPlayerColor(), &r, &g, &b, &a );
		return GameMakeColor( r, g, b, alpha );
	}

	/// which side of a border shows on the radar (one line): the local player's, then anyone's, then nobody's
	Int rank( Int zone )
	{
		if( zone < 0 )
			return 0;
		const Player *owner = ownerOf( zone );
		if( owner == nullptr )
			return 1;
		return owner == localPlayer() ? 3 : 2;
	}

	//-----------------------------------------------------------------------------------------------
	void radarDraw( GameWindow *window, WinInstanceData *instData )
	{
		if( s_radarDraw != nullptr )
			s_radarDraw( window, instData );
		if( !TiberiumZones::isOn() || TheRadar == nullptr || TheDisplay == nullptr || TheTerrainLogic == nullptr )
			return;
		if( TheInGameUI->videoBuffer() != nullptr || !rts::localPlayerHasRadar() )
			return;
		buildEdges();
		ICoord2D pos, size, ul, lr;
		window->winGetScreenPosition( &pos.x, &pos.y );
		window->winGetSize( &size.x, &size.y );
		TheRadar->findDrawPositions( pos.x + 1, pos.y + 1, size.x - 2, size.y - 2, &ul, &lr );
		Region3D extent;
		TheTerrainLogic->getExtent( &extent );
		const Real ew = max( 1.0f, extent.hi.x - extent.lo.x ), eh = max( 1.0f, extent.hi.y - extent.lo.y );
		const Real sx = ( lr.x - ul.x ) / ew, sy = ( lr.y - ul.y ) / eh;
		const Int count = TiberiumZones::getCount();
		std::vector<Color> colors( count );
		std::vector<Int> ranks( count );
		for( Int z = 0; z < count; ++z )
		{
			colors[z] = colorOf( z, 235 );
			ranks[z] = rank( z );
		}
		// the local player's zones last (on top), then anyone's, nobody's first
		for( Int pass = 1; pass <= 3; ++pass )
			for( size_t i = 0; i < s_edges.size(); ++i )
			{
				const Edge &e = s_edges[i];
				if( ranks[e.zone] != pass )
					continue;
				const Real x0 = ul.x + ( e.x0 - extent.lo.x ) * sx, y0 = lr.y - ( e.y0 - extent.lo.y ) * sy;
				const Real x1 = ul.x + ( e.x1 - extent.lo.x ) * sx, y1 = lr.y - ( e.y1 - extent.lo.y ) * sy;
				TheDisplay->drawLine( (Int)x0, (Int)y0, (Int)x1, (Int)y1, 1.0f, colors[e.zone] );
			}
		// the nodes, as a dot in their holder's colour
		for( size_t i = 0; i < s_nodes.size(); ++i )
		{
			const Object *node = TheGameLogic->findObjectByID( s_nodes[i] );
			if( node == nullptr )
				continue;
			const Int z = TiberiumZones::zoneAt( node->getPosition() );
			const Int px = (Int)( ul.x + ( node->getPosition()->x - extent.lo.x ) * sx ), py = (Int)( lr.y - ( node->getPosition()->y - extent.lo.y ) * sy );
			TheDisplay->drawFillRect( px - 2, py - 2, 5, 5, GameMakeColor( 0, 0, 0, 230 ) );
			TheDisplay->drawFillRect( px - 1, py - 1, 3, 3, colorOf( z, 255 ) );
		}
	}

	//-----------------------------------------------------------------------------------------------
	void findNodes()
	{
		const UnsignedInt frame = TheGameLogic->getFrame();
		if( s_nodesFrame != 0xFFFFFFFF && frame >= s_nodesFrame && frame - s_nodesFrame < LOGICFRAMES_PER_SECOND )
			return;
		s_nodesFrame = frame;
		s_nodes.clear();
		for( Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject() )
			if( TiberiumZoneNodeUpdate::isNode( obj ) )
				s_nodes.push_back( obj->getID() );
	}

	//-----------------------------------------------------------------------------------------------
	Bool project( Real x, Real y, Real lift, ICoord2D *out )
	{
		Coord3D w;
		w.x = x;
		w.y = y;
		w.z = TheTerrainLogic->getGroundHeight( x, y ) + lift;
		return TheTacticalView->worldToScreenTriReturn( &w, out ) != View::WTS_INVALID;
	}

	void ensureFonts()
	{
		const Int sh = (Int)TheDisplay->getHeight();
		if( s_small != nullptr && s_fontFor == sh )
			return;
		s_fontFor = sh;
		if( s_small == nullptr )
			s_small = TheDisplayStringManager->newDisplayString();
		s_small->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), max( 10, sh / 72 ), TRUE ) );
		for( size_t i = 0; i < s_labels.size(); ++i )
			if( s_labels[i] != nullptr )
				s_labels[i]->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), max( 11, sh / 60 ), TRUE ) );
	}

	DisplayString *label( Int zone )
	{
		while( (Int)s_labels.size() <= zone )
		{
			DisplayString *s = TheDisplayStringManager->newDisplayString();
			s->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), max( 11, (Int)TheDisplay->getHeight() / 60 ), TRUE ) );
			s_labels.push_back( s );
		}
		return s_labels[zone];
	}

	UnicodeString zoneName( Int zone )
	{
		Player *owner = ownerOf( zone );
		const Bool home = TiberiumZones::isHome( zone );
		UnicodeString text;
		if( owner == nullptr )
			text = L"Unclaimed zone";
		else if( owner == localPlayer() )
			text = home ? L"Your start zone" : L"Your zone";
		else
		{
			text = owner->getPlayerDisplayName();
			text.concat( home ? L" - start zone" : L" - zone" );
		}
		return text;
	}
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumZoneView::isAvailable()
{
	return TiberiumZones::isOn();
}

//-------------------------------------------------------------------------------------------------
void TiberiumZoneView::toggle()
{
	s_shown = !s_shown;
}

//-------------------------------------------------------------------------------------------------
void TiberiumZoneView::setShown( Bool on )
{
	s_shown = on;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumZoneView::isShown()
{
	if( !TiberiumZones::isOn() )
		return FALSE;
	if( s_shown )
		return TRUE;
	// placing a structure where the zones decide
	const AbramZoneMode mode = TiberiumZones::getMode();
	return TheInGameUI != nullptr && TheInGameUI->getPendingPlaceType() != nullptr
		&& ( mode == ABRAM_ZONES_PRE || mode == ABRAM_ZONES_EXPANSION );
}

//-------------------------------------------------------------------------------------------------
void TiberiumZoneView::update()
{
	if( !TiberiumZones::isOn() || TheWindowManager == nullptr )
	{
		if( s_radar != nullptr )
			reset();
		return;
	}
	findNodes();
	static const NameKeyType radarID = NAMEKEY( "ControlBar.wnd:LeftHUD" );
	GameWindow *radar = TheWindowManager->winGetWindowFromId( nullptr, radarID );
	if( radar != s_radar )
	{
		// a new radar window (the control bar made again): its own drawing, then the zones over it
		s_radar = radar;
		s_radarDraw = nullptr;
		if( radar != nullptr )
		{
			GameWinDrawFunc draw = radar->winGetDrawFunc();
			if( draw != radarDraw )
			{
				s_radarDraw = draw;
				radar->winSetDrawFunc( radarDraw );
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumZoneView::reset()
{
	if( s_radar != nullptr && TheWindowManager != nullptr )
	{
		static const NameKeyType radarID = NAMEKEY( "ControlBar.wnd:LeftHUD" );
		GameWindow *radar = TheWindowManager->winGetWindowFromId( nullptr, radarID );
		if( radar == s_radar && radar->winGetDrawFunc() == radarDraw )
			radar->winSetDrawFunc( s_radarDraw );
	}
	s_radar = nullptr;
	s_radarDraw = nullptr;
	s_shown = FALSE;
	s_edges.clear();
	s_edgesW = s_edgesH = s_edgesCount = -1;
	s_nodes.clear();
	s_nodesFrame = 0xFFFFFFFF;
	if( TheDisplayStringManager != nullptr )
	{
		for( size_t i = 0; i < s_labels.size(); ++i )
			TheDisplayStringManager->freeDisplayString( s_labels[i] );
		if( s_small != nullptr )
			TheDisplayStringManager->freeDisplayString( s_small );
	}
	s_labels.clear();
	s_small = nullptr;
	s_fontFor = 0;
}

//-------------------------------------------------------------------------------------------------
void TiberiumZoneView::draw()
{
	if( !isShown() || TheTacticalView == nullptr || TheDisplay == nullptr || TheTerrainLogic == nullptr )
		return;
	buildEdges();
	ensureFonts();

	// what the camera sees, on the ground
	const Coord3D at = TheTacticalView->getPosition();
	const Real groundZ = TheTerrainLogic->getGroundHeight( at.x, at.y );
	Coord3D corner[4];
	Real lox = at.x - 2500.0f, hix = at.x + 2500.0f, loy = at.y - 2500.0f, hiy = at.y + 2500.0f;
	if( TheTacticalView->getScreenCornerWorldPointsAtZ( &corner[0], &corner[1], &corner[2], &corner[3], groundZ ) == PlaneClass::INSIDE_SEGMENT )
	{
		lox = hix = corner[0].x;
		loy = hiy = corner[0].y;
		for( Int i = 1; i < 4; ++i )
		{
			lox = min( lox, corner[i].x );
			hix = max( hix, corner[i].x );
			loy = min( loy, corner[i].y );
			hiy = max( hiy, corner[i].y );
		}
		lox -= 150.0f;
		loy -= 150.0f;
		hix += 150.0f;
		hiy += 150.0f;
	}

	const Int count = TiberiumZones::getCount();
	std::vector<Color> colors( count );
	for( Int z = 0; z < count; ++z )
		colors[z] = colorOf( z, 210 );
	const Real width = (Real)max( 2, (Int)TheDisplay->getHeight() / 480 );
	const Real inset = 3.0f;
	const Int sw = (Int)TheDisplay->getWidth(), sh = (Int)TheDisplay->getHeight();

	// each zone's side of a border, a little inside it (two zones meeting: two lines)
	for( size_t i = 0; i < s_edges.size(); ++i )
	{
		const Edge &e = s_edges[i];
		if( max( e.x0, e.x1 ) < lox || min( e.x0, e.x1 ) > hix || max( e.y0, e.y1 ) < loy || min( e.y0, e.y1 ) > hiy )
			continue;
		const Real dx = e.inX * inset, dy = e.inY * inset;
		ICoord2D p0, p1;
		if( !project( e.x0 + dx, e.y0 + dy, 2.0f, &p0 ) || !project( e.x1 + dx, e.y1 + dy, 2.0f, &p1 ) )
			continue;
		if( ( p0.x < 0 && p1.x < 0 ) || ( p0.y < 0 && p1.y < 0 ) || ( p0.x > sw && p1.x > sw ) || ( p0.y > sh && p1.y > sh ) )
			continue;
		TheDisplay->drawLine( p0.x, p0.y, p1.x, p1.y, width, colors[e.zone] );
	}

	// each zone's holder, at its middle
	for( Int z = 0; z < count; ++z )
	{
		const Coord3D *c = TiberiumZones::getCenter( z );
		if( c == nullptr || c->x < lox || c->x > hix || c->y < loy || c->y > hiy )
			continue;
		ICoord2D p;
		Coord3D w = *c;
		// a start zone's: below its start, off the base's first structure
		if( TiberiumZones::isHome( z ) )
			w.y -= 160.0f;
		w.z = TheTerrainLogic->getGroundHeight( w.x, w.y ) + 10.0f;
		if( TheTacticalView->worldToScreenTriReturn( &w, &p ) != View::WTS_INSIDE_FRUSTUM )
			continue;
		DisplayString *s = label( z );
		s->setText( zoneName( z ) );
		s->draw( p.x - s->getWidth() / 2, p.y - s->getFont()->height / 2, colorOf( z, 255 ), GameMakeColor( 0, 0, 0, 220 ) );
	}

	// the nodes being taken: who takes them, and how far
	for( size_t i = 0; i < s_nodes.size(); ++i )
	{
		const Object *node = TheGameLogic->findObjectByID( s_nodes[i] );
		Int who = -1;
		Real done = 0.0f;
		Bool back = FALSE;
		if( node == nullptr || !TiberiumZoneNodeUpdate::getCapture( node, &who, &done, &back ) )
			continue;
		Coord3D w = *node->getPosition();
		w.z += 45.0f;
		ICoord2D p;
		if( TheTacticalView->worldToScreenTriReturn( &w, &p ) != View::WTS_INSIDE_FRUSTUM )
			continue;
		const Player *taker = ThePlayerList->getNthPlayer( who );
		UnsignedByte r = 255, g = 255, b = 255, a = 255;
		if( taker != nullptr )
			GameGetColorComponents( taker->getPlayerColor(), &r, &g, &b, &a );
		const Int bw = max( 60, sh / 12 ), bh = max( 5, sh / 160 );
		TheDisplay->drawFillRect( p.x - bw / 2 - 1, p.y - 1, bw + 2, bh + 2, GameMakeColor( 0, 0, 0, 220 ) );
		TheDisplay->drawFillRect( p.x - bw / 2, p.y, (Int)( bw * done ), bh, back ? GameMakeColor( 200, 200, 200, 255 ) : GameMakeColor( r, g, b, 255 ) );
		s_small->setText( back ? UnicodeString( L"Neutralizing" ) : UnicodeString( L"Capturing" ) );
		s_small->draw( p.x - s_small->getWidth() / 2, p.y - s_small->getFont()->height - 2, GameMakeColor( 255, 255, 255, 255 ), GameMakeColor( 0, 0, 0, 220 ) );
	}
}
