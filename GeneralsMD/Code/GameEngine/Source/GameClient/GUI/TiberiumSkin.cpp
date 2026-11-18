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

// Tiberium Project @feature the parts the Tiberium menus are drawn with (TiberiumSkin.h).

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "GameClient/TiberiumSkin.h"
#include "GameClient/TiberiumMenu.h"
#include "GameClient/TiberiumSettings.h"
#include "GameClient/TiberiumSkirmish.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"

#include <vector>

namespace TiberiumSkin
{
	struct TextEntry
	{
		UnicodeString text;
		Int size;
		Bool bold;
		DisplayString *string;
	};

	static std::vector<TextEntry> s_texts;

	//-----------------------------------------------------------------------------------------------
	Look lookOf( Int style )
	{
		Look l;
		if( style == TiberiumSettings::STYLE_NOD )
		{
			l.accent = GameMakeColor( 255, 74, 61, 255 );			l.accentDim = GameMakeColor( 138, 42, 34, 255 );
			l.hi = GameMakeColor( 255, 210, 122, 255 );				l.text = GameMakeColor( 255, 230, 223, 255 );
			l.textDim = GameMakeColor( 201, 154, 144, 255 );	l.fill = GameMakeColor( 42, 11, 9, 255 );
			l.fillOn = GameMakeColor( 106, 26, 20, 255 );			l.stripe = GameMakeColor( 63, 18, 15, 255 );
			l.stripeOn = GameMakeColor( 138, 38, 29, 255 );		l.panel = GameMakeColor( 30, 7, 6, 238 );
			l.grid = GameMakeColor( 255, 74, 61, 20 );				l.shade = GameMakeColor( 10, 3, 3, 247 );
		}
		else
		{
			l.accent = GameMakeColor( 63, 210, 255, 255 );		l.accentDim = GameMakeColor( 42, 127, 160, 255 );
			l.hi = GameMakeColor( 242, 193, 78, 255 );				l.text = GameMakeColor( 216, 241, 255, 255 );
			l.textDim = GameMakeColor( 143, 182, 201, 255 );	l.fill = GameMakeColor( 11, 39, 64, 255 );
			l.fillOn = GameMakeColor( 26, 74, 114, 255 );			l.stripe = GameMakeColor( 18, 58, 92, 255 );
			l.stripeOn = GameMakeColor( 42, 100, 148, 255 );	l.panel = GameMakeColor( 6, 22, 40, 238 );
			l.grid = GameMakeColor( 63, 210, 255, 20 );				l.shade = GameMakeColor( 5, 9, 14, 247 );
		}
		return l;
	}

	//-----------------------------------------------------------------------------------------------
	/** a text's string in a size, made once and kept until freeTexts() */
	DisplayString *textOf( const UnicodeString &text, Int size, Bool bold )
	{
		for( size_t i = 0; i < s_texts.size(); ++i )
			if( s_texts[i].size == size && s_texts[i].bold == bold && s_texts[i].text.compare( text ) == 0 )
				return s_texts[i].string;
		TextEntry e;
		e.text = text;
		e.size = size;
		e.bold = bold;
		e.string = TheDisplayStringManager->newDisplayString();
		if( e.string != nullptr )
		{
			e.string->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), size, bold ) );
			e.string->setText( text );
		}
		s_texts.push_back( e );
		return e.string;
	}

	DisplayString *textOf( const char *text, Int size, Bool bold )
	{
		UnicodeString u;
		u.translate( AsciiString( text ) );
		return textOf( u, size, bold );
	}

	void freeTexts()
	{
		for( size_t i = 0; i < s_texts.size(); ++i )
			if( s_texts[i].string != nullptr && TheDisplayStringManager != nullptr )
				TheDisplayStringManager->freeDisplayString( s_texts[i].string );
		s_texts.clear();
	}

	Int widthOf( const char *text, Int size, Bool bold )
	{
		DisplayString *s = textOf( text, size, bold );
		return s != nullptr ? s->getWidth() : 0;
	}

	Int widthOf( const UnicodeString &text, Int size, Bool bold )
	{
		DisplayString *s = textOf( text, size, bold );
		return s != nullptr ? s->getWidth() : 0;
	}

	void drawText( const char *text, Int size, Bool bold, Int x, Int y, Color color )
	{
		DisplayString *s = textOf( text, size, bold );
		if( s != nullptr )
			s->draw( x, y, color, GameMakeColor( 0, 0, 0, 160 ) );
	}

	void drawText( const UnicodeString &text, Int size, Bool bold, Int x, Int y, Color color )
	{
		DisplayString *s = textOf( text, size, bold );
		if( s != nullptr )
			s->draw( x, y, color, GameMakeColor( 0, 0, 0, 160 ) );
	}

	//-----------------------------------------------------------------------------------------------
	/** the text on one line, cut short with dots where it is wider than the room it has */
	void drawTextIn( const UnicodeString &text, Int size, Bool bold, Int x, Int y, Int width, Color color )
	{
		if( widthOf( text, size, bold ) <= width )
		{
			drawText( text, size, bold, x, y, color );
			return;
		}
		UnicodeString cut = text;
		while( cut.getLength() > 1 )
		{
			cut.removeLastChar();
			UnicodeString shown = cut;
			shown.concat( L"..." );
			if( widthOf( shown, size, bold ) <= width )
			{
				drawText( shown, size, bold, x, y, color );
				return;
			}
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** a field of a form: a flat box with its text; one that opens a list has a mark at its right end */
	void drawField( const Look &l, Int x, Int y, Int w, Int h, Real u, const UnicodeString &text, Int size, Bool hover, Bool dim, Bool list )
	{
		TheDisplay->drawFillRect( x, y, w, h, hover && !dim ? l.fillOn : l.fill );
		TheDisplay->drawOpenRect( x, y, w, h, 1.0f, hover && !dim ? l.hi : l.accentDim );
		const Int pad = max( 3, (Int)( 6.0f * u ) ), mark = list && !dim ? max( 5, (Int)( 9.0f * u ) ) : 0;
		DisplayString *s = textOf( text, size, FALSE );
		const Int ty = s != nullptr ? y + ( h - s->getFont()->height ) / 2 : y;
		drawTextIn( text, size, FALSE, x + pad, ty, w - 2 * pad - mark, dim ? l.textDim : l.text );
		if( mark > 0 )
		{
			// a small arrow head pointing down
			const Int mx = x + w - pad - mark / 2, my = y + h / 2 - mark / 4;
			TheDisplay->drawLine( mx - mark / 2, my, mx, my + mark / 2, 1.0f, hover ? l.hi : l.accent );
			TheDisplay->drawLine( mx, my + mark / 2, mx + mark / 2, my, 1.0f, hover ? l.hi : l.accent );
		}
	}

	//-----------------------------------------------------------------------------------------------
	Int drawWrapped( const char *text, Int size, Bool bold, Int x, Int y, Int width, Color color )
	{
		// (a font's letters are not as wide on every machine: what fits one line here need not there)
		AsciiString line, word;
		Int used = 0, lineHeight = 0;
		const char *c = text;
		for( ;; )
		{
			if( *c != 0 && *c != ' ' )
			{
				word.concat( *c++ );
				continue;
			}
			AsciiString longer = line;
			if( longer.isNotEmpty() && word.isNotEmpty() )
				longer.concat( ' ' );
			longer.concat( word );
			if( line.isNotEmpty() && widthOf( longer.str(), size, bold ) > width )
			{
				DisplayString *s = textOf( line.str(), size, bold );
				if( s != nullptr )
				{
					s->draw( x, y + used, color, GameMakeColor( 0, 0, 0, 160 ) );
					lineHeight = s->getFont()->height + 2;
				}
				used += lineHeight;
				line = word;
			}
			else
				line = longer;
			word.clear();
			if( *c == 0 )
				break;
			++c;
		}
		if( line.isNotEmpty() )
		{
			DisplayString *s = textOf( line.str(), size, bold );
			if( s != nullptr )
			{
				s->draw( x, y + used, color, GameMakeColor( 0, 0, 0, 160 ) );
				used += s->getFont()->height + 2;
			}
		}
		return used;
	}

	//-----------------------------------------------------------------------------------------------
	/** C&C4's menu button, clean: a striped bar standing on a foot under each end */
	void drawButton( const Look &l, Int x, Int y, Int w, Int h, Real u, const char *text, Int size, Bool on, Bool hover, Bool dim )
	{
		const Int foot = max( 3, (Int)( 6.0f * u ) ), footW = max( 10, (Int)( 26.0f * u ) ), cut = max( 4, (Int)( 8.0f * u ) );
		const Int body = h - foot;
		TheDisplay->drawFillRect( x, y, w, body, on ? l.fillOn : l.fill );
		const Int pitch = max( 3, (Int)( 4.0f * u ) );
		for( Int sy = y + pitch - 1; sy < y + body; sy += pitch )
			TheDisplay->drawLine( x + 1, sy, x + w - 1, sy, 1.0f, on ? l.stripeOn : l.stripe );
		TheDisplay->drawFillRect( x, y + body, footW, foot, on ? l.fillOn : l.fill );
		TheDisplay->drawFillRect( x + w - footW, y + body, footW, foot, on ? l.fillOn : l.fill );
		const Color edge = on || hover ? l.hi : ( dim ? l.accentDim : l.accent );
		const Real lw = on ? 2.0f : 1.0f;
		TheDisplay->drawLine( x, y, x + w, y, lw, edge );
		TheDisplay->drawLine( x + w, y, x + w, y + h, lw, edge );
		TheDisplay->drawLine( x + w, y + h, x + w - footW, y + h, lw, edge );
		TheDisplay->drawLine( x + w - footW, y + h, x + w - footW - cut, y + body, lw, edge );
		TheDisplay->drawLine( x + w - footW - cut, y + body, x + footW + cut, y + body, lw, edge );
		TheDisplay->drawLine( x + footW + cut, y + body, x + footW, y + h, lw, edge );
		TheDisplay->drawLine( x + footW, y + h, x, y + h, lw, edge );
		TheDisplay->drawLine( x, y + h, x, y, lw, edge );
		DisplayString *s = textOf( text, size, TRUE );
		if( s != nullptr )
			s->draw( x + ( w - s->getWidth() ) / 2, y + ( body - s->getFont()->height ) / 2, dim && !on && !hover ? l.textDim : l.text, GameMakeColor( 0, 0, 0, 160 ) );
	}

	//-----------------------------------------------------------------------------------------------
	/** C&C4's menu frame, clean: two outlines, a raised tab on its top edge, a nick in each side, a grid inside */
	void drawFrame( const Look &l, Int x, Int y, Int w, Int h, Real u )
	{
		TheDisplay->drawFillRect( x, y, w, h, l.panel );
		const Int cell = max( 12, (Int)( 40.0f * u ) );
		for( Int gx = x + cell; gx < x + w; gx += cell )
			TheDisplay->drawLine( gx, y + 1, gx, y + h - 1, 1.0f, l.grid );
		for( Int gy = y + cell; gy < y + h; gy += cell )
			TheDisplay->drawLine( x + 1, gy, x + w - 1, gy, 1.0f, l.grid );
		const Int in = max( 3, (Int)( 5.0f * u ) );
		TheDisplay->drawOpenRect( x + in, y + in, w - 2 * in, h - 2 * in, 1.0f, l.accentDim );
		// the outline, its top edge stepping up over a tab in the middle
		const Int tabW = w / 3, tabH = max( 4, (Int)( 7.0f * u ) ), slope = tabH + max( 2, (Int)( 3.0f * u ) );
		const Int t0 = x + ( w - tabW ) / 2, t1 = t0 + tabW;
		TheDisplay->drawLine( x, y, t0 - slope, y, 2.0f, l.accent );
		TheDisplay->drawLine( t0 - slope, y, t0, y - tabH, 2.0f, l.accent );
		TheDisplay->drawLine( t0, y - tabH, t1, y - tabH, 2.0f, l.accent );
		TheDisplay->drawLine( t1, y - tabH, t1 + slope, y, 2.0f, l.accent );
		TheDisplay->drawLine( t1 + slope, y, x + w, y, 2.0f, l.accent );
		TheDisplay->drawFillRect( t0, y - tabH + 2, tabW, max( 2, tabH - 3 ), l.accent );
		TheDisplay->drawLine( x + w, y, x + w, y + h, 2.0f, l.accent );
		TheDisplay->drawLine( x + w, y + h, x, y + h, 2.0f, l.accent );
		TheDisplay->drawLine( x, y + h, x, y, 2.0f, l.accent );
		const Int nick = max( 3, (Int)( 6.0f * u ) );
		TheDisplay->drawLine( x - nick, y + h / 3, x - nick, y + 2 * h / 3, 2.0f, l.accentDim );
		TheDisplay->drawLine( x + w + nick, y + h / 3, x + w + nick, y + 2 * h / 3, 2.0f, l.accentDim );
	}

	//-----------------------------------------------------------------------------------------------
	/** a whole screen's ground: Zero Hour's menu map darkened, a line all round, a bright bar in its top edge, ticks
		down both sides, the screen's name top left */
	void drawBackdrop( const Look &l, Int ox, Real u, Int sw, Int sh, const char *title, Int alpha )
	{
		#define BX( v ) ( ox + (Int)( ( v ) * u ) )
		#define BS( v ) ( (Int)( ( v ) * u ) )
		TheDisplay->drawFillRect( 0, 0, sw, sh, ( l.shade & 0x00FFFFFF ) | ( (UnsignedInt)alpha << 24 ) );
		TheDisplay->drawOpenRect( BX( 14 ), BS( 12 ), BS( 1252 ), BS( 696 ), 1.0f, l.accentDim );
		TheDisplay->drawFillRect( BX( 340 ), BS( 10 ), BS( 600 ), max( 2, BS( 4 ) ), l.accent );
		for( Int ty = BS( 60 ); ty < BS( 660 ); ty += max( 4, BS( 9 ) ) )
		{
			TheDisplay->drawLine( BX( 20 ), ty, BX( 30 ), ty, 1.0f, l.accentDim );
			TheDisplay->drawLine( BX( 1250 ), ty, BX( 1260 ), ty, 1.0f, l.accentDim );
		}
		drawText( title, max( 12, BS( 19.0f ) ), TRUE, BX( 34 ), BS( 26 ), l.text );
		#undef BX
		#undef BS
	}

	//-----------------------------------------------------------------------------------------------
	Bool overStockScreen()
	{
		return TiberiumMenu::isOn() || TiberiumSkirmish::isOn();
	}
}
