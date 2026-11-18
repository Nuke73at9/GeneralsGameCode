/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

// Tiberium Project @feature the Tiberium main menu (TiberiumMenu.h).

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "GameClient/TiberiumMenu.h"
#include "GameClient/TiberiumSettings.h"
#include "GameClient/TiberiumSkin.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Gadget.h"
#include "GameClient/WindowLayout.h"

#include <cmath>
#include <vector>

using namespace TiberiumSkin;

namespace
{
	//-----------------------------------------------------------------------------------------------
	// a line of a menu: its words, the button of Zero Hour's menu it presses (none: done here), what it is for
	struct Item
	{
		const char *label, *button, *note;
	};

	enum { MAIN_SOLO, MAIN_MULTI, MAIN_LOAD, MAIN_MODS, MAIN_SETTINGS, MAIN_EXIT, MAIN_COUNT };
	enum { SIDE_MENUS = 3 };	///< the first three main lines open a menu of their own

	const Item s_main[MAIN_COUNT] =
	{
		{ "SOLO PLAY", "MainMenu.wnd:ButtonSinglePlayer", "Skirmish, the campaigns and the Generals Challenge." },
		{ "MULTIPLAYER", "MainMenu.wnd:ButtonMultiplayer", "Play online or on your own network." },
		{ "LOAD AND REPLAYS", "MainMenu.wnd:ButtonLoadReplay", "Load a saved game or watch a replay." },
		{ "MODS", nullptr, "Choose the mods the game runs with." },
		{ "SETTINGS", nullptr, "Frame rate, game speed, the menus' look, and Zero Hour's options." },
		{ "EXIT", "MainMenu.wnd:ButtonExit", "Leave the game." },
	};
	// the button that closes each of the three menus again in Zero Hour's
	const char *const s_back[SIDE_MENUS] = { "MainMenu.wnd:ButtonSingleBack", "MainMenu.wnd:ButtonMultiBack", "MainMenu.wnd:ButtonLoadReplayBack" };
	const Item s_solo[] =
	{
		{ "SKIRMISH", "MainMenu.wnd:ButtonSkirmish", "Play against computer armies on any map." },
		{ "USA CAMPAIGN", "MainMenu.wnd:ButtonUSA", "Zero Hour's USA campaign." },
		{ "CHINA CAMPAIGN", "MainMenu.wnd:ButtonChina", "Zero Hour's China campaign." },
		{ "GLA CAMPAIGN", "MainMenu.wnd:ButtonGLA", "Zero Hour's GLA campaign." },
		{ "GENERALS CHALLENGE", "MainMenu.wnd:ButtonChallenge", "Fight the generals one after another." },
	};
	const Item s_multi[] =
	{
		{ "ONLINE", "MainMenu.wnd:ButtonOnline", "Play over the internet." },
		{ "NETWORK", "MainMenu.wnd:ButtonNetwork", "Play on your own network." },
	};
	const Item s_load[] =
	{
		{ "LOAD GAME", "MainMenu.wnd:ButtonLoadGame", "Go on with a saved game." },
		{ "REPLAYS", "MainMenu.wnd:ButtonReplay", "Watch a recorded game." },
	};
	const Item s_difficulty[] =
	{
		{ "EASY", "MainMenu.wnd:ButtonEasy", "" },
		{ "MEDIUM", "MainMenu.wnd:ButtonMedium", "" },
		{ "HARD", "MainMenu.wnd:ButtonHard", "" },
		{ "BACK", "MainMenu.wnd:ButtonDiffBack", "" },
	};
	const Item *const s_side[SIDE_MENUS] = { s_solo, s_multi, s_load };
	const Int s_sideCount[SIDE_MENUS] = { ARRAY_SIZE( s_solo ), ARRAY_SIZE( s_multi ), ARRAY_SIZE( s_load ) };
	const char *const MODS_MARK = "@mods";

	enum HitKind { HIT_MAIN, HIT_SIDE, HIT_DIFFICULTY, HIT_STYLE };
	struct Hit
	{
		HitKind kind;
		Int index;
		Int x, y, w, h;
	};

	GameWindow *s_window = nullptr;			///< the menu's own window, over everything of Zero Hour's
	Bool s_closed = TRUE;								///< Zero Hour's main menu is not the screen, or is on its way out
	GameWindow *s_parent = nullptr;			///< Zero Hour's main menu: its buttons are pressed through it
	Int s_open = -1;										///< the main line whose menu is open, -1: none
	Int s_hover = -1;										///< index into s_hits
	std::vector<Hit> s_hits;						///< as last drawn
	std::vector<const char *> s_presses;	///< the buttons of Zero Hour's menu still to press, in order

	//-----------------------------------------------------------------------------------------------
	/** a button of Zero Hour's main menu pressed, as its own click does it */
	void press( const char *name )
	{
		if( strcmp( name, MODS_MARK ) == 0 )
		{
			MainMenuOpenMods();
			return;
		}
		GameWindow *button = TheWindowManager->winGetWindowFromId( nullptr, TheNameKeyGenerator->nameToKey( name ) );
		if( button != nullptr && s_parent != nullptr )
			TheWindowManager->winSendSystemMsg( s_parent, GBM_SELECTED, (WindowMsgData)button, button->winGetWindowId() );
	}

	//-----------------------------------------------------------------------------------------------
	void act( const Hit &hit )
	{
		const Bool asking = MainMenuAsksDifficulty();
		switch( hit.kind )
		{
			case HIT_STYLE:
				TiberiumSettings::setStyle( hit.index );
				break;
			case HIT_DIFFICULTY:
				if( asking )
					s_presses.push_back( s_difficulty[hit.index].button );
				break;
			case HIT_SIDE:
				// (while a difficulty is asked only its own lines answer, as in Zero Hour's menu)
				if( !asking && s_open >= 0 && s_open < SIDE_MENUS && hit.index < s_sideCount[s_open] )
					s_presses.push_back( s_side[s_open][hit.index].button );
				break;
			case HIT_MAIN:
				if( asking )
					break;
				if( hit.index < SIDE_MENUS )
				{
					// a line with a menu of its own: the open one shut first, then this one opened; its own shut again
					if( s_open >= 0 )
						s_presses.push_back( s_back[s_open] );
					if( s_open == hit.index )
						s_open = -1;
					else
					{
						s_presses.push_back( s_main[hit.index].button );
						s_open = hit.index;
					}
				}
				else
				{
					if( s_open >= 0 )
						s_presses.push_back( s_back[s_open] );
					s_open = -1;
					if( hit.index == MAIN_SETTINGS )
						TiberiumSettings::open();
					else if( hit.index == MAIN_MODS )
						s_presses.push_back( MODS_MARK );
					else
						s_presses.push_back( s_main[hit.index].button );
				}
				break;
		}
	}

	//-----------------------------------------------------------------------------------------------
	void addHit( HitKind kind, Int index, Int x, Int y, Int w, Int h )
	{
		Hit hit;
		hit.kind = kind;
		hit.index = index;
		hit.x = x;
		hit.y = y;
		hit.w = w;
		hit.h = h;
		s_hits.push_back( hit );
	}

	//-----------------------------------------------------------------------------------------------
	/** a ring in short straight lines; dashed: every other one left out */
	void drawRing( Int cx, Int cy, Real radius, Real width, Color color, Bool dashed )
	{
		const Int sides = 96;
		for( Int i = 0; i < sides; ++i )
		{
			if( dashed && ( i & 1 ) != 0 )
				continue;
			const Real a0 = 6.2831853f * i / sides, a1 = 6.2831853f * ( i + 1 ) / sides;
			TheDisplay->drawLine( cx + (Int)( cosf( a0 ) * radius ), cy + (Int)( sinf( a0 ) * radius ), cx + (Int)( cosf( a1 ) * radius ), cy + (Int)( sinf( a1 ) * radius ),
				width, color );
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** the menu, on a 1280 by 720 sheet scaled to the screen's height and centred across it. Zero Hour's menu map
		goes on behind it, darkened. */
	void draw( GameWindow *, WinInstanceData * )
	{
		if( TheDisplay == nullptr || TheDisplayStringManager == nullptr || TheFontLibrary == nullptr )
			return;
		const Int sw = TheDisplay->getWidth(), sh = TheDisplay->getHeight();
		const Real u = sh / 720.0f;
		const Int ox = ( sw - (Int)( 1280.0f * u ) ) / 2;
		const Look l = lookOf( TiberiumSettings::getStyle() );
		#define PX( v ) ( ox + (Int)( ( v ) * u ) )
		#define PY( v ) ( (Int)( ( v ) * u ) )
		#define PS( v ) ( (Int)( ( v ) * u ) )
		const Int titleSize = max( 12, PS( 19.0f ) ), mainSize = max( 9, PS( 15.0f ) ), sideSize = max( 8, PS( 12.0f ) ), noteSize = max( 8, PS( 13.0f ) );
		const Int oldHover = s_hover;
		const Bool asking = MainMenuAsksDifficulty();
		s_hits.clear();

		// the menu map darkened
		TheDisplay->drawFillRect( 0, 0, sw, sh, ( l.shade & 0x00FFFFFF ) | ( 175u << 24 ) );
		// the screen's frame, as the settings': a line all round, a bright bar in its top edge, ticks down both sides
		TheDisplay->drawOpenRect( PX( 14 ), PY( 12 ), PS( 1252 ), PS( 696 ), 1.0f, l.accentDim );
		TheDisplay->drawFillRect( PX( 340 ), PY( 10 ), PS( 600 ), max( 2, PS( 4 ) ), l.accent );
		for( Int ty = PY( 60 ); ty < PY( 660 ); ty += max( 4, PS( 9 ) ) )
		{
			TheDisplay->drawLine( PX( 20 ), ty, PX( 30 ), ty, 1.0f, l.accentDim );
			TheDisplay->drawLine( PX( 1250 ), ty, PX( 1260 ), ty, 1.0f, l.accentDim );
		}
		drawText( "MAIN MENU", titleSize, TRUE, PX( 34 ), PY( 26 ), l.text );

		// the look's switch, top right
		for( Int i = 0; i < 2; ++i )
		{
			const Int x = PX( 1110 + i * 68 ), y = PY( 24 ), w = PS( 62 ), h = PS( 28 );
			const Bool on = TiberiumSettings::getStyle() == i, hover = oldHover == (Int)s_hits.size();
			TheDisplay->drawFillRect( x, y, w, h, on ? l.fillOn : l.fill );
			TheDisplay->drawOpenRect( x, y, w, h, on ? 2.0f : 1.0f, on || hover ? l.hi : l.accentDim );
			const char *name = i == TiberiumSettings::STYLE_NOD ? "NOD" : "GDI";
			drawText( name, sideSize, TRUE, x + ( w - widthOf( name, sideSize, TRUE ) ) / 2, y + PS( 7 ), on || hover ? l.text : l.textDim );
			addHit( HIT_STYLE, i, x, y, w, h );
		}

		// C&C4's ring: circles about the stack of buttons, the widest faint, ticks round the middle one
		const Int cx = PX( 330 ), cy = PY( 372 );
		drawRing( cx, cy, 285.0f * u, 1.0f, l.accentDim, FALSE );
		drawRing( cx, cy, 240.0f * u, max( 2.0f, 6.0f * u ), ( l.accentDim & 0x00FFFFFF ) | ( 70u << 24 ), FALSE );
		drawRing( cx, cy, 240.0f * u, 1.0f, l.accentDim, TRUE );
		drawRing( cx, cy, 196.0f * u, 1.0f, l.accent, FALSE );
		for( Int i = 0; i < 72; ++i )
		{
			const Real a = 6.2831853f * i / 72, r0 = 203.0f * u, r1 = ( i % 6 == 0 ? 216.0f : 209.0f ) * u;
			TheDisplay->drawLine( cx + (Int)( cosf( a ) * r0 ), cy + (Int)( sinf( a ) * r0 ), cx + (Int)( cosf( a ) * r1 ), cy + (Int)( sinf( a ) * r1 ), 1.0f, l.accentDim );
		}

		// the menu's own lines, stacked in the ring
		const char *note = s_open >= 0 ? s_main[s_open].note : "";
		const char *noteTitle = s_open >= 0 ? s_main[s_open].label : "";
		for( Int i = 0; i < MAIN_COUNT; ++i )
		{
			const Int x = PX( 210 ), y = PY( 214 + i * 54 ), w = PS( 240 ), h = PS( 44 );
			const Bool hover = oldHover == (Int)s_hits.size() && !asking;
			drawButton( l, x, y, w, h, u, s_main[i].label, mainSize, i == s_open, hover, asking );
			if( hover )
			{
				note = s_main[i].note;
				noteTitle = s_main[i].label;
			}
			addHit( HIT_MAIN, i, x, y, w, h );
		}

		// the open line's menu beside it, and the difficulty beside that when a campaign asks for it
		if( s_open >= 0 && s_open < SIDE_MENUS )
		{
			const Int top = 214 + s_open * 54;
			TheDisplay->drawLine( PX( 450 ), PY( top + 20 ), PX( 482 ), PY( top + 20 ), 1.0f, l.hi );
			for( Int i = 0; i < s_sideCount[s_open]; ++i )
			{
				const Int x = PX( 482 ), y = PY( top + 2 + i * 42 ), w = PS( 210 ), h = PS( 36 );
				const Bool hover = oldHover == (Int)s_hits.size() && !asking;
				drawButton( l, x, y, w, h, u, s_side[s_open][i].label, sideSize, FALSE, hover, asking );
				if( hover )
				{
					note = s_side[s_open][i].note;
					noteTitle = s_side[s_open][i].label;
				}
				addHit( HIT_SIDE, i, x, y, w, h );
			}
			if( asking )
			{
				for( Int i = 0; i < (Int)ARRAY_SIZE( s_difficulty ); ++i )
				{
					const Int x = PX( 706 ), y = PY( top + 2 + i * 42 ), w = PS( 150 ), h = PS( 36 );
					const Bool hover = oldHover == (Int)s_hits.size();
					drawButton( l, x, y, w, h, u, s_difficulty[i].label, sideSize, FALSE, hover, FALSE );
					addHit( HIT_DIFFICULTY, i, x, y, w, h );
				}
				note = "Choose how hard the campaign is to be.";
				noteTitle = "DIFFICULTY";
			}
		}

		// what the line under the mouse, or the open one, is for
		if( note[0] != 0 )
		{
			const Int x = PX( 880 ), y = PY( 96 ), w = PS( 360 ), h = PS( 108 );
			drawFrame( l, x, y, w, h, u );
			drawText( noteTitle, sideSize, TRUE, x + PS( 14 ), y + PS( 12 ), l.accent );
			TheDisplay->drawLine( x + PS( 10 ), y + PS( 34 ), x + w - PS( 10 ), y + PS( 34 ), 1.0f, l.accentDim );
			drawWrapped( note, noteSize, FALSE, x + PS( 14 ), y + PS( 44 ), w - PS( 28 ), l.text );
		}
		#undef PX
		#undef PY
		#undef PS
	}

	//-----------------------------------------------------------------------------------------------
	Int hitAt( Int mx, Int my )
	{
		for( size_t i = 0; i < s_hits.size(); ++i )
			if( mx >= s_hits[i].x && mx < s_hits[i].x + s_hits[i].w && my >= s_hits[i].y && my < s_hits[i].y + s_hits[i].h )
				return (Int)i;
		return -1;
	}

	//-----------------------------------------------------------------------------------------------
	WindowMsgHandledType input( GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData )
	{
		const Int mx = (Short)( mData1 & 0xFFFF ), my = (Short)( ( mData1 >> 16 ) & 0xFFFF );
		switch( msg )
		{
			case GWM_MOUSE_POS:
				s_hover = hitAt( mx, my );
				return MSG_HANDLED;
			case GWM_MOUSE_LEAVING:
				s_hover = -1;
				return MSG_HANDLED;
			case GWM_LEFT_UP:
			{
				const Int index = hitAt( mx, my );
				if( index >= 0 )
				{
					const Hit hit = s_hits[index];
					act( hit );
				}
				return MSG_HANDLED;
			}
			default:
				break;
		}
		return MSG_HANDLED;
	}
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumMenu::isOn()
{
	return s_window != nullptr;
}

//-------------------------------------------------------------------------------------------------
void TiberiumMenu::init()
{
	s_closed = FALSE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumMenu::update( WindowLayout *layout, GameWindow *parent )
{
	// (Zero Hour's menu goes on updating while it slides out for the next screen: not to be drawn over again then)
	if( s_closed )
		return;
	if( !TiberiumSettings::wanted() || layout == nullptr || parent == nullptr )
	{
		shutdown();
		s_closed = FALSE;
		return;
	}
	s_parent = parent;
	if( s_window == nullptr )
	{
		s_window = TheWindowManager->winCreate( nullptr, WIN_STATUS_ENABLED | WIN_STATUS_ABOVE, 0, 0, TheDisplay->getWidth(), TheDisplay->getHeight(),
			nullptr, nullptr );
		if( s_window == nullptr )
			return;
		s_window->winSetDrawFunc( draw );
		s_window->winSetInputFunc( input );
		s_open = -1;
		s_hover = -1;
		s_hits.clear();
		s_presses.clear();
	}
	// Zero Hour's menu stays under this one, out of sight; its buttons are pressed from here. Its windows are hidden,
	// not its screen: the shell goes on taking it for the screen that is up, and shuts it down for the next one
	for( GameWindow *win = layout->getFirstWindow(); win != nullptr; win = win->winGetNextInLayout() )
		if( !win->winIsHidden() )
			win->winHide( TRUE );
	// the next press once Zero Hour's menu can take it (its own menus slide in and out between them)
	if( !s_presses.empty() && MainMenuCanTakeClick() )
	{
		const char *name = s_presses.front();
		s_presses.erase( s_presses.begin() );
		press( name );
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumMenu::shutdown()
{
	if( s_window != nullptr && TheWindowManager != nullptr )
		TheWindowManager->winDestroy( s_window );
	s_window = nullptr;
	s_parent = nullptr;
	s_open = -1;
	s_hits.clear();
	s_presses.clear();
	s_closed = TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumMenu::testClick( const char *part, Int index )
{
	if( s_window == nullptr || part == nullptr )
		return FALSE;
	HitKind kind = HIT_MAIN;
	if( strcmp( part, "side" ) == 0 )
		kind = HIT_SIDE;
	else if( strcmp( part, "difficulty" ) == 0 )
		kind = HIT_DIFFICULTY;
	else if( strcmp( part, "style" ) == 0 )
		kind = HIT_STYLE;
	else if( strcmp( part, "main" ) != 0 )
		return FALSE;
	for( size_t i = 0; i < s_hits.size(); ++i )
	{
		if( s_hits[i].kind != kind || s_hits[i].index != index )
			continue;
		const Hit hit = s_hits[i];
		act( hit );
		return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumMenu::describe( char *buffer, Int size )
{
	if( buffer == nullptr || size <= 0 )
		return;
	if( s_window == nullptr )
	{
		snprintf( buffer, size, "off" );
		return;
	}
	snprintf( buffer, size, "on; %d parts; open %d; difficulty asked %d; %d presses waiting; can take a click %d; style %s", (int)s_hits.size(), (int)s_open,
		MainMenuAsksDifficulty() ? 1 : 0, (int)s_presses.size(), MainMenuCanTakeClick() ? 1 : 0, TiberiumSettings::getStyle() == TiberiumSettings::STYLE_NOD ? "Nod" : "GDI" );
}
