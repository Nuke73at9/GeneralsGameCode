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

// Tiberium Project @feature the Tiberium interface's settings panel (TiberiumSettings.h). Everything on it is drawn
// here in lines and flat fills: clean at any resolution, in the shapes of C&C4's menus (a frame with a raised tab and
// a grid behind it, striped buttons standing on two feet), in a GDI or a Nod look.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "GameClient/TiberiumSettings.h"
#include "GameClient/TiberiumSkin.h"
#include "Common/FramePacer.h"
#include "Common/GlobalData.h"
#include "Common/OptionPreferences.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"

#include <vector>

using namespace TiberiumSkin;

namespace
{
	//-----------------------------------------------------------------------------------------------
	// the settings: each a row of choices on one of the pages
	enum { ROW_FPS, ROW_VSYNC, ROW_SMOOTH, ROW_REALTIME, ROW_SKIRMISH, ROW_STYLE, ROW_COUNT };
	enum { PAGE_PACE, PAGE_INTERFACE, PAGE_COUNT, TAB_ZERO_HOUR = PAGE_COUNT, TAB_COUNT };
	enum { MAX_CHOICES = 7 };

	struct RowDef
	{
		Int page;
		const char *section;				///< a heading over the row, or null
		const char *label, *note;
		Int count;
		const char *choices[MAX_CHOICES];
	};

	const RowDef s_rows[ROW_COUNT] =
	{
		{ PAGE_PACE, "FRAME RATE", "Frame-rate limit", "How many frames are drawn a second", 7, { "30", "60", "120", "144", "165", "240", "No limit" } },
		{ PAGE_PACE, nullptr, "Sync to the display", "Stops tearing. Applies at the next start", 3, { "As the game was", "Off", "On" } },
		{ PAGE_PACE, nullptr, "Smooth motion", "Units move at every drawn frame", 2, { "Off", "On" } },
		{ PAGE_PACE, "GAME SPEED", "Real-time game speed", "The same speed at any frame rate", 2, { "Off", "On" } },
		{ PAGE_PACE, nullptr, "Skirmish speed", "Logic frames a second in skirmish games", 4, { "Slow 20", "Normal 30", "Fast 45", "Very fast 60" } },
		{ PAGE_INTERFACE, "LOOK", "Style", "The menus' look", 2, { "GDI", "Nod" } },
	};
	const char *const s_tabNames[TAB_COUNT] = { "FRAME RATE AND SPEED", "INTERFACE", "ZERO HOUR'S OPTIONS" };
	const Int s_fpsValues[7] = { 30, 60, 120, 144, 165, 240, 0 };
	const Int s_tickValues[4] = { 20, 30, 45, 60 };

	// what a click lands on
	enum HitKind { HIT_NONE, HIT_TAB, HIT_CHOICE, HIT_BACK, HIT_DEFAULTS, HIT_ACCEPT };
	struct Hit
	{
		HitKind kind;
		Int a, b;
		Int x, y, w, h;
	};

	GameWindow *s_window = nullptr;
	Int s_page = PAGE_PACE;
	Int s_choice[ROW_COUNT];						///< the choice lit in each row, -1: none (the key is not in Options.ini)
	Int s_style = -1;										///< the look in use (-1: not read yet)
	Int s_hover = -1;										///< index into s_hits
	std::vector<Hit> s_hits;						///< as last drawn

	//-----------------------------------------------------------------------------------------------
	/** the choices of Options.ini, as the rows show them */
	void loadChoices()
	{
		OptionPreferences prefs;
		s_choice[ROW_FPS] = -1;
		if( prefs.find( "MaxRenderFPS" ) != prefs.end() )
		{
			const Int fps = prefs.getInt( "MaxRenderFPS", -1 );
			for( Int i = 0; i < 7; ++i )
				if( s_fpsValues[i] == fps )
					s_choice[ROW_FPS] = i;
		}
		s_choice[ROW_VSYNC] = 0;
		if( prefs.getAsciiString( "VSync", AsciiString::TheEmptyString ).isNotEmpty() )
			s_choice[ROW_VSYNC] = prefs.getBool( "VSync", TRUE ) ? 2 : 1;
		s_choice[ROW_SMOOTH] = prefs.getBool( "InterpolateMotion", FALSE ) ? 1 : 0;
		s_choice[ROW_REALTIME] = prefs.getBool( "RealTimeLogic", FALSE ) ? 1 : 0;
		s_choice[ROW_SKIRMISH] = -1;
		const Int tick = prefs.getSkirmishTickRate();
		for( Int i = 0; i < 4; ++i )
			if( s_tickValues[i] == tick )
				s_choice[ROW_SKIRMISH] = i;
		s_choice[ROW_STYLE] = TiberiumSettings::getStyle();
	}

	//-----------------------------------------------------------------------------------------------
	/** the rows' choices into Options.ini, and into the running game where that can be done at once */
	void keepChoices()
	{
		OptionPreferences prefs;
		if( s_choice[ROW_FPS] >= 0 )
			prefs.setInt( "MaxRenderFPS", s_fpsValues[s_choice[ROW_FPS]] );
		else
			prefs.erase( "MaxRenderFPS" );
		if( s_choice[ROW_VSYNC] == 0 )
			prefs.erase( "VSync" );
		else
			prefs.setAsciiString( "VSync", s_choice[ROW_VSYNC] == 2 ? "yes" : "no" );
		prefs.setAsciiString( "InterpolateMotion", s_choice[ROW_SMOOTH] == 1 ? "yes" : "no" );
		prefs.setAsciiString( "RealTimeLogic", s_choice[ROW_REALTIME] == 1 ? "yes" : "no" );
		if( s_choice[ROW_SKIRMISH] >= 0 )
			prefs.setInt( "TickRate", s_tickValues[s_choice[ROW_SKIRMISH]] );
		prefs.setAsciiString( "InterfaceStyle", s_choice[ROW_STYLE] == TiberiumSettings::STYLE_NOD ? "Nod" : "GDI" );
		prefs.write();

		s_style = s_choice[ROW_STYLE] == TiberiumSettings::STYLE_NOD ? TiberiumSettings::STYLE_NOD : TiberiumSettings::STYLE_GDI;
		if( TheFramePacer != nullptr )
		{
			TheFramePacer->setRenderFpsOverride( s_choice[ROW_FPS] >= 0 ? s_fpsValues[s_choice[ROW_FPS]] : -1 );
			TheFramePacer->setInterpolateDrawables( s_choice[ROW_SMOOTH] == 1 );
			TheFramePacer->setRealTimeLogic( s_choice[ROW_REALTIME] == 1, prefs.getInt( "MaxLogicTicksPerUpdate", 4 ) );
			if( s_choice[ROW_REALTIME] == 1 )
				TheFramePacer->enableLogicTimeScale( TRUE );
		}
		if( s_choice[ROW_SKIRMISH] >= 0 && TheWritableGlobalData != nullptr )
			TheWritableGlobalData->m_skirmishTickRate = s_tickValues[s_choice[ROW_SKIRMISH]];
	}

	//-----------------------------------------------------------------------------------------------
	void defaultChoices()
	{
		// as Zero Hour was: no limit of ours, the sync as the game had it, no smoothing, the logic at the frame rate's pace
		s_choice[ROW_FPS] = -1;
		s_choice[ROW_VSYNC] = 0;
		s_choice[ROW_SMOOTH] = 0;
		s_choice[ROW_REALTIME] = 0;
		s_choice[ROW_SKIRMISH] = 1;
		s_choice[ROW_STYLE] = TiberiumSettings::STYLE_GDI;
	}

	//-----------------------------------------------------------------------------------------------
	void act( const Hit &hit )
	{
		switch( hit.kind )
		{
			case HIT_TAB:
				if( hit.a == TAB_ZERO_HOUR )
				{
					// Zero Hour's own options screen, as its button opened it; ours closes without keeping
					TiberiumSettings::close( FALSE );
					WindowLayout *layout = TheShell != nullptr ? TheShell->getOptionsLayout( TRUE ) : nullptr;
					if( layout != nullptr )
					{
						layout->runInit();
						layout->hide( FALSE );
						layout->bringForward();
					}
				}
				else
					s_page = hit.a;
				break;
			case HIT_CHOICE:
				s_choice[hit.a] = hit.b;
				break;
			case HIT_DEFAULTS:
				defaultChoices();
				break;
			case HIT_ACCEPT:
				TiberiumSettings::close( TRUE );
				break;
			case HIT_BACK:
				TiberiumSettings::close( FALSE );
				break;
			default:
				break;
		}
	}

	//-----------------------------------------------------------------------------------------------
	void addHit( HitKind kind, Int a, Int b, Int x, Int y, Int w, Int h )
	{
		Hit hit;
		hit.kind = kind;
		hit.a = a;
		hit.b = b;
		hit.x = x;
		hit.y = y;
		hit.w = w;
		hit.h = h;
		s_hits.push_back( hit );
	}

	//-----------------------------------------------------------------------------------------------
	/** the panel: laid out on a 1280 by 720 sheet scaled to the screen's height and centred across it; the parts a
		click can land on are noted as they are drawn */
	void draw( GameWindow *, WinInstanceData * )
	{
		if( TheDisplay == nullptr || TheDisplayStringManager == nullptr || TheFontLibrary == nullptr )
			return;
		const Int sw = TheDisplay->getWidth(), sh = TheDisplay->getHeight();
		const Real u = sh / 720.0f;
		const Int ox = ( sw - (Int)( 1280.0f * u ) ) / 2;
		// (the look the rows choose shows at once, kept or not)
		const Look l = lookOf( s_choice[ROW_STYLE] == TiberiumSettings::STYLE_NOD ? TiberiumSettings::STYLE_NOD : TiberiumSettings::STYLE_GDI );
		#define PX( v ) ( ox + (Int)( ( v ) * u ) )
		#define PY( v ) ( (Int)( ( v ) * u ) )
		#define PS( v ) ( (Int)( ( v ) * u ) )
		const Int titleSize = max( 12, PS( 19.0f ) ), labelSize = max( 9, PS( 14.0f ) ), noteSize = max( 8, PS( 11.0f ) ), tabSize = max( 8, PS( 12.0f ) );
		const Int oldHover = s_hover;
		s_hits.clear();

		TheDisplay->drawFillRect( 0, 0, sw, sh, l.shade );
		// the screen's own frame: a line all round, a bright bar in its top edge, ticks down both sides
		TheDisplay->drawOpenRect( PX( 14 ), PY( 12 ), PS( 1252 ), PS( 696 ), 1.0f, l.accentDim );
		TheDisplay->drawFillRect( PX( 340 ), PY( 10 ), PS( 600 ), max( 2, PS( 4 ) ), l.accent );
		for( Int ty = PY( 60 ); ty < PY( 660 ); ty += max( 4, PS( 9 ) ) )
		{
			TheDisplay->drawLine( PX( 20 ), ty, PX( 30 ), ty, 1.0f, l.accentDim );
			TheDisplay->drawLine( PX( 1250 ), ty, PX( 1260 ), ty, 1.0f, l.accentDim );
		}
		drawText( "SETTINGS", titleSize, TRUE, PX( 34 ), PY( 26 ), l.text );

		// the pages' tabs down the left
		for( Int i = 0; i < TAB_COUNT; ++i )
		{
			const Int x = PX( 40 ), y = PY( 90 + i * 52 ), w = PS( 230 ), h = PS( 42 );
			drawButton( l, x, y, w, h, u, s_tabNames[i], tabSize, i == s_page, oldHover == (Int)s_hits.size(), i != s_page );
			addHit( HIT_TAB, i, 0, x, y, w, h );
		}

		// the page
		const Int px = PX( 290 ), py = PY( 84 ), pw = PS( 950 ), ph = PS( 496 );
		drawFrame( l, px, py, pw, ph, u );
		drawText( s_tabNames[s_page], max( 10, PS( 15.0f ) ), TRUE, px + PS( 18 ), py + PS( 14 ), l.text );
		drawText( "saved to Options.ini", noteSize, FALSE, px + pw - PS( 18 ) - widthOf( "saved to Options.ini", noteSize, FALSE ), py + PS( 17 ), l.textDim );
		TheDisplay->drawLine( px + PS( 12 ), py + PS( 42 ), px + pw - PS( 12 ), py + PS( 42 ), 1.0f, l.accentDim );
		Int y = py + PS( 52 );
		for( Int r = 0; r < ROW_COUNT; ++r )
		{
			const RowDef &row = s_rows[r];
			if( row.page != s_page )
				continue;
			if( row.section != nullptr )
			{
				drawText( row.section, noteSize, TRUE, px + PS( 18 ), y + PS( 4 ), l.accent );
				y += PS( 26 );
			}
			drawText( row.label, labelSize, FALSE, px + PS( 18 ), y + PS( 2 ), l.text );
			drawText( row.note, noteSize, FALSE, px + PS( 18 ), y + PS( 22 ), l.textDim );
			Int x = px + PS( 330 );
			const Int bh = PS( 30 );
			for( Int c = 0; c < row.count; ++c )
			{
				const Int bw = widthOf( row.choices[c], labelSize, FALSE ) + PS( 22 );
				const Bool on = s_choice[r] == c;
				const Bool hover = oldHover == (Int)s_hits.size();
				TheDisplay->drawFillRect( x, y + PS( 4 ), bw, bh, on ? l.fillOn : l.fill );
				TheDisplay->drawOpenRect( x, y + PS( 4 ), bw, bh, on ? 2.0f : 1.0f, on || hover ? l.hi : l.accentDim );
				DisplayString *s = textOf( row.choices[c], labelSize, FALSE );
				if( s != nullptr )
					s->draw( x + PS( 11 ), y + PS( 4 ) + ( bh - s->getFont()->height ) / 2, on || hover ? l.text : l.textDim, GameMakeColor( 0, 0, 0, 160 ) );
				addHit( HIT_CHOICE, r, c, x, y + PS( 4 ), bw, bh );
				x += bw + PS( 4 );
			}
			y += PS( 50 );
		}

		// back, defaults, accept
		static const HitKind kinds[3] = { HIT_BACK, HIT_DEFAULTS, HIT_ACCEPT };
		static const char *const names[3] = { "BACK", "DEFAULTS", "ACCEPT" };
		static const Real lefts[3] = { 40.0f, 800.0f, 1030.0f };
		for( Int i = 0; i < 3; ++i )
		{
			const Int x = PX( lefts[i] ), by = PY( 598 ), w = PS( 210 ), h = PS( 46 );
			drawButton( l, x, by, w, h, u, names[i], labelSize, i == 2, oldHover == (Int)s_hits.size(), i != 2 );
			addHit( kinds[i], 0, 0, x, by, w, h );
		}
		drawText( s_page == PAGE_INTERFACE ? "In a match the HUD follows the faction you play."
			: "The frame-rate limit, smooth motion and the game speed take effect when accepted.", noteSize, FALSE, px + PS( 18 ), py + ph - PS( 28 ), l.textDim );
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
	WindowMsgHandledType input( GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 )
	{
		const Int mx = (Short)( mData1 & 0xFFFF ), my = (Short)( ( mData1 >> 16 ) & 0xFFFF );
		switch( msg )
		{
			case GWM_MOUSE_POS:
				s_hover = hitAt( mx, my );
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
			case GWM_CHAR:
				if( (UnsignedByte)mData1 == KEY_ESC && BitIsSet( mData2, KEY_STATE_UP ) )
					TiberiumSettings::close( FALSE );
				return MSG_HANDLED;
			default:
				break;
		}
		// (everything under the panel waits)
		return MSG_HANDLED;
	}
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumSettings::wanted()
{
	return TheGlobalData != nullptr && TheGlobalData->m_tiberiumSidebar && TheWindowManager != nullptr && TheDisplay != nullptr
		&& TheDisplayStringManager != nullptr && TheFontLibrary != nullptr;
}

//-------------------------------------------------------------------------------------------------
Int TiberiumSettings::getStyle()
{
	if( s_style < 0 )
	{
		OptionPreferences prefs;
		AsciiString style = prefs.getAsciiString( "InterfaceStyle", "GDI" );
		s_style = style.compareNoCase( "Nod" ) == 0 ? STYLE_NOD : STYLE_GDI;
	}
	return s_style;
}

//-------------------------------------------------------------------------------------------------
void TiberiumSettings::setStyle( Int style )
{
	s_style = style == STYLE_NOD ? STYLE_NOD : STYLE_GDI;
	OptionPreferences prefs;
	prefs.setAsciiString( "InterfaceStyle", s_style == STYLE_NOD ? "Nod" : "GDI" );
	prefs.write();
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumSettings::isOpen()
{
	return s_window != nullptr;
}

//-------------------------------------------------------------------------------------------------
void TiberiumSettings::open()
{
	if( s_window != nullptr || !wanted() )
		return;
	s_window = TheWindowManager->winCreate( nullptr, WIN_STATUS_ENABLED | WIN_STATUS_ABOVE, 0, 0, TheDisplay->getWidth(), TheDisplay->getHeight(),
		nullptr, nullptr );
	if( s_window == nullptr )
		return;
	s_window->winSetDrawFunc( draw );
	s_window->winSetInputFunc( input );
	TheWindowManager->winSetModal( s_window );
	TheWindowManager->winSetFocus( s_window );
	s_page = PAGE_PACE;
	s_hover = -1;
	s_hits.clear();
	loadChoices();
}

//-------------------------------------------------------------------------------------------------
void TiberiumSettings::close( Bool keep )
{
	if( s_window == nullptr )
		return;
	if( keep )
		keepChoices();
	if( TheWindowManager != nullptr )
	{
		TheWindowManager->winUnsetModal( s_window );
		TheWindowManager->winDestroy( s_window );
	}
	s_window = nullptr;
	s_hits.clear();
	freeTexts();
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumSettings::testClick( const char *part, Int row, Int index )
{
	if( s_window == nullptr || part == nullptr )
		return FALSE;
	HitKind kind = HIT_NONE;
	if( strcmp( part, "tab" ) == 0 )
		kind = HIT_TAB;
	else if( strcmp( part, "option" ) == 0 )
		kind = HIT_CHOICE;
	else if( strcmp( part, "accept" ) == 0 )
		kind = HIT_ACCEPT;
	else if( strcmp( part, "cancel" ) == 0 )
		kind = HIT_BACK;
	else if( strcmp( part, "defaults" ) == 0 )
		kind = HIT_DEFAULTS;
	// (through the parts as last drawn: a part that is not on the page cannot be clicked)
	for( size_t i = 0; i < s_hits.size(); ++i )
	{
		if( s_hits[i].kind != kind || ( kind == HIT_TAB && s_hits[i].a != row ) || ( kind == HIT_CHOICE && ( s_hits[i].a != row || s_hits[i].b != index ) ) )
			continue;
		const Hit hit = s_hits[i];
		act( hit );
		return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumSettings::describe( char *buffer, Int size )
{
	if( buffer == nullptr || size <= 0 )
		return;
	if( s_window == nullptr )
	{
		snprintf( buffer, size, "closed; style %s", getStyle() == STYLE_NOD ? "Nod" : "GDI" );
		return;
	}
	snprintf( buffer, size, "open; page %d; %d parts; limit %d, sync %d, smooth %d, real time %d, skirmish %d, style %d", (int)s_page, (int)s_hits.size(),
		(int)s_choice[ROW_FPS], (int)s_choice[ROW_VSYNC], (int)s_choice[ROW_SMOOTH], (int)s_choice[ROW_REALTIME], (int)s_choice[ROW_SKIRMISH], (int)s_choice[ROW_STYLE] );
}
