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

// FILE: AbramSpawnMenu.cpp ///////////////////////////////////////////////////////////////////////
// Abram's Personal Test @feature the cheats' spawn menu (see AbramSpawnMenu.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/AbramSpawnMenu.h"

#include "Common/GameAudio.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/Recorder.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Color.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Image.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/AbramCheats.h"
#include "GameLogic/GameLogic.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace
{
	enum SpawnCategory
	{
		CAT_INFANTRY,
		CAT_VEHICLES,
		CAT_AIRCRAFT,
		CAT_STRUCTURES,
		CAT_COUNT
	};
	const char *const CATEGORY_NAMES[CAT_COUNT] = { "Infantry", "Vehicles", "Aircraft", "Structures" };

	struct SpawnItem
	{
		const ThingTemplate *thing;
		const CommandButton *button;	///< its build button (its name and description), or none
		const Image *image;
		Int cost;
		UnicodeString name;
	};

	struct SpawnFaction
	{
		AsciiString side;
		UnicodeString label;
		Int order;		///< the faction's place among the player templates (other sides after them)
		std::vector<SpawnItem> items[CAT_COUNT];
	};

	struct SpawnLayout
	{
		Int x, y, w, h, gap;
		Int headerH, closeX, closeY, closeS;
		Int listX, listY, listW, listH, rowH, listRows;
		Int gridX, gridW, tabsY, tabsH, gridY, gridH, cellW, cellH, cols, rows;
		Int infoY, infoH;
	};

	enum
	{
		HIT_NONE = -1,
		HIT_PANEL = 0,
		HIT_CLOSE = 1,
		HIT_FACTION = 100,		///< + the faction
		HIT_CATEGORY = 10000,	///< + the category
		HIT_ITEM = 20000,			///< + the item
	};

	GameWindow *s_window = nullptr;
	std::vector<SpawnFaction> s_factions;
	Int s_faction = 0;
	Int s_category = CAT_INFANTRY;
	Int s_scroll = 0;					///< the grid's first row shown
	Int s_factionScroll = 0;	///< the faction list's first row shown
	Int s_hover = HIT_NONE;
	Int s_mouseX = -1, s_mouseY = -1;
	const ThingTemplate *s_placing = nullptr;
	DisplayString *s_title = nullptr;
	DisplayString *s_label = nullptr;
	DisplayString *s_small = nullptr;
	DisplayString *s_info = nullptr;
	Int s_fontScreenHeight = 0;

	//-----------------------------------------------------------------------------------------------
	/** a skirmish or network game with cheats, played (not a replay) by a player still in it */
	Bool mayShow()
	{
		if( TheGameLogic == nullptr || !TheGameLogic->isInGame() || TheGameLogic->isInShellGame() )
			return FALSE;
		if( TheRecorder != nullptr && TheRecorder->isPlaybackMode() )
			return FALSE;
		if( !AbramCheats::isEnabled() || TheWindowManager == nullptr || TheDisplay == nullptr || TheInGameUI == nullptr )
			return FALSE;
		Player *me = ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
		return me != nullptr && me->isPlayerActive();
	}

	//-----------------------------------------------------------------------------------------------
	SpawnFaction *currentFaction()
	{
		return s_faction >= 0 && s_faction < (Int)s_factions.size() ? &s_factions[s_faction] : nullptr;
	}

	//-----------------------------------------------------------------------------------------------
	std::vector<SpawnItem> *currentItems()
	{
		SpawnFaction *f = currentFaction();
		return f != nullptr && s_category >= 0 && s_category < CAT_COUNT ? &f->items[s_category] : nullptr;
	}

	//-----------------------------------------------------------------------------------------------
	SpawnCategory categoryOf( const ThingTemplate *thing )
	{
		if( thing->isKindOf( KINDOF_STRUCTURE ) )
			return CAT_STRUCTURES;
		if( thing->isKindOf( KINDOF_AIRCRAFT ) )
			return CAT_AIRCRAFT;
		if( thing->isKindOf( KINDOF_INFANTRY ) )
			return CAT_INFANTRY;
		return CAT_VEHICLES;
	}

	//-----------------------------------------------------------------------------------------------
	/** a button's label marks its hotkey with '&' ("&Ranger"; "&&" for an '&' itself): the name without it */
	UnicodeString withoutHotkey( const UnicodeString &marked )
	{
		UnicodeString plain;
		for( const WideChar *c = marked.str(); *c != 0; ++c )
		{
			if( *c == L'&' && c[1] == L'&' )
				++c;
			else if( *c == L'&' )
				continue;
			plain.concat( *c );
		}
		return plain;
	}

	//-----------------------------------------------------------------------------------------------
	UnicodeString nameOf( const ThingTemplate *thing, const CommandButton *button )
	{
		UnicodeString name = thing->getDisplayName();
		if( name.isEmpty() && button != nullptr && button->getTextLabel().isNotEmpty() )
		{
			Bool exists = FALSE;
			const UnicodeString text = TheGameText->fetch( button->getTextLabel(), &exists );
			if( exists )
				name = withoutHotkey( text );
		}
		if( name.isEmpty() )
			name.translate( thing->getName() );
		return name;
	}

	//-----------------------------------------------------------------------------------------------
	Bool itemBefore( const SpawnItem &a, const SpawnItem &b )
	{
		if( a.cost != b.cost )
			return a.cost < b.cost;
		return a.thing->getName().compare( b.thing->getName() ) < 0;
	}

	//-----------------------------------------------------------------------------------------------
	Bool factionBefore( const SpawnFaction &a, const SpawnFaction &b )
	{
		if( a.order != b.order )
			return a.order < b.order;
		return a.side.compare( b.side ) < 0;
	}

	//-----------------------------------------------------------------------------------------------
	/** every unit and structure the cheat may spawn that has a picture, by faction (side) and kind */
	void buildList()
	{
		s_factions.clear();
		if( TheThingFactory == nullptr || TheControlBar == nullptr )
			return;

		// the build buttons, for the names, pictures and descriptions the command bar shows
		std::map<std::string, const CommandButton *> buttons;
		for( const CommandButton *b = TheControlBar->getCommandButtons(); b != nullptr; b = b->getNext() )
		{
			if( b->getCommandType() != GUI_COMMAND_UNIT_BUILD && b->getCommandType() != GUI_COMMAND_DOZER_CONSTRUCT )
				continue;
			const ThingTemplate *thing = b->getThingTemplate();
			if( thing == nullptr )
				continue;
			const std::string key = thing->getName().str();
			if( buttons.find( key ) == buttons.end() )
				buttons[key] = b;
		}

		// each side's place: the player templates' order (the skirmish list's)
		std::map<std::string, Int> sideOrder;
		std::map<std::string, UnicodeString> sideLabel;
		if( ThePlayerTemplateStore != nullptr )
		{
			for( Int i = 0; i < ThePlayerTemplateStore->getPlayerTemplateCount(); ++i )
			{
				const PlayerTemplate *pt = ThePlayerTemplateStore->getNthPlayerTemplate( i );
				if( pt == nullptr || pt->getSide().isEmpty() )
					continue;
				const std::string side = pt->getSide().str();
				if( sideOrder.find( side ) != sideOrder.end() )
					continue;
				sideOrder[side] = pt->isPlayableSide() ? i : 1000 + i;
				sideLabel[side] = pt->getDisplayName();
			}
		}

		const Player *me = ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
		std::map<std::string, Int> factionIndex;
		for( const ThingTemplate *thing = TheThingFactory->firstTemplate(); thing != nullptr; thing = thing->friend_getNextTemplate() )
		{
			if( !AbramCheats::isSpawnable( thing ) )
				continue;
			std::map<std::string, const CommandButton *>::const_iterator found = buttons.find( thing->getName().str() );
			const CommandButton *button = found != buttons.end() ? found->second : nullptr;
			const Image *image = thing->getButtonImage();
			if( image == nullptr && button != nullptr )
				image = button->getButtonImage();
			if( image == nullptr )
				continue;		// (only what has a picture: what players build, and what maps place)

			const std::string side = thing->getDefaultOwningSide().str();
			std::map<std::string, Int>::const_iterator at = factionIndex.find( side );
			if( at == factionIndex.end() )
			{
				SpawnFaction faction;
				faction.side = thing->getDefaultOwningSide();
				std::map<std::string, UnicodeString>::const_iterator label = sideLabel.find( side );
				if( label != sideLabel.end() && !label->second.isEmpty() )
					faction.label = label->second;
				else if( !side.empty() )
					faction.label.translate( faction.side );
				else
					faction.label = L"Other";
				std::map<std::string, Int>::const_iterator order = sideOrder.find( side );
				faction.order = order != sideOrder.end() ? order->second : 2000;
				s_factions.push_back( faction );
				at = factionIndex.insert( std::make_pair( side, (Int)s_factions.size() - 1 ) ).first;
			}
			SpawnItem item;
			item.thing = thing;
			item.button = button;
			item.image = image;
			item.cost = me != nullptr ? thing->calcCostToBuild( me ) : 0;
			item.name = nameOf( thing, button );
			s_factions[at->second].items[categoryOf( thing )].push_back( item );
		}

		std::sort( s_factions.begin(), s_factions.end(), factionBefore );
		for( size_t f = 0; f < s_factions.size(); ++f )
			for( Int c = 0; c < CAT_COUNT; ++c )
				std::sort( s_factions[f].items[c].begin(), s_factions[f].items[c].end(), itemBefore );
	}

	//-----------------------------------------------------------------------------------------------
	/** the faction's first kind that has something (infantry first) */
	void chooseCategory()
	{
		SpawnFaction *f = currentFaction();
		if( f == nullptr || !f->items[s_category].empty() )
			return;
		for( Int c = 0; c < CAT_COUNT; ++c )
		{
			if( !f->items[c].empty() )
			{
				s_category = c;
				return;
			}
		}
	}

	//-----------------------------------------------------------------------------------------------
	void ensureStrings()
	{
		const Int sh = TheDisplay->getHeight();
		if( s_label != nullptr && s_fontScreenHeight == sh )
			return;
		s_fontScreenHeight = sh;
		if( s_label == nullptr )
		{
			s_title = TheDisplayStringManager->newDisplayString();
			s_label = TheDisplayStringManager->newDisplayString();
			s_small = TheDisplayStringManager->newDisplayString();
			s_info = TheDisplayStringManager->newDisplayString();
		}
		s_title->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), max( 13, sh / 48 ), TRUE ) );
		s_label->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), max( 11, sh / 60 ), TRUE ) );
		s_small->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), max( 9, sh / 80 ), TRUE ) );
		s_info->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), max( 9, sh / 80 ), FALSE ) );
	}

	//-----------------------------------------------------------------------------------------------
	Int fontHeight( DisplayString *s )
	{
		return s != nullptr && s->getFont() != nullptr ? s->getFont()->height : 12;
	}

	//-----------------------------------------------------------------------------------------------
	void computeLayout( SpawnLayout *L )
	{
		const Int sw = TheDisplay->getWidth(), sh = TheDisplay->getHeight();
		L->gap = max( 3, sh / 216 );
		L->w = min( max( sw * 40 / 100, 480 ), sw - 4 * L->gap );
		L->x = L->gap * 2;
		L->y = sh * 7 / 100;
		L->h = sh * 64 / 100;

		L->headerH = fontHeight( s_title ) + 2 * L->gap;
		L->closeS = L->headerH - 2 * L->gap;
		L->closeX = L->x + L->w - L->gap - L->closeS;
		L->closeY = L->y + L->gap;
		L->rowH = fontHeight( s_small ) + 2 * L->gap;

		// what the cursor is on: the name, a line for its price and faction, and four of description
		L->infoH = fontHeight( s_label ) + fontHeight( s_small ) + 4 * fontHeight( s_info ) + 5 * L->gap;
		L->infoY = L->y + L->h - L->infoH - L->gap;

		L->listX = L->x + L->gap;
		L->listY = L->y + L->headerH;
		L->listW = L->w * 30 / 100;
		L->listH = L->infoY - L->gap - L->listY;
		L->listRows = max( 1, L->listH / max( 1, L->rowH ) );

		L->gridX = L->listX + L->listW + L->gap;
		L->gridW = L->x + L->w - L->gap - L->gridX;
		L->tabsY = L->listY;
		L->tabsH = L->rowH;
		L->gridY = L->tabsY + L->tabsH + L->gap;
		L->gridH = L->infoY - L->gap - L->gridY;
		const Int want = max( 48, sh / 12 );
		L->cols = max( 3, ( L->gridW + L->gap ) / ( want + L->gap ) );
		L->cellW = ( L->gridW - ( L->cols - 1 ) * L->gap ) / L->cols;
		L->cellH = L->cellW * 4 / 5;
		L->rows = max( 1, ( L->gridH + L->gap ) / max( 1, L->cellH + L->gap ) );
	}

	//-----------------------------------------------------------------------------------------------
	void drawText( DisplayString *s, const UnicodeString &text, Int x, Int y, Color color )
	{
		s->setWordWrap( 0 );
		s->setText( text );
		s->draw( x, y, color, GameMakeColor( 0, 0, 0, 200 ) );
	}

	//-----------------------------------------------------------------------------------------------
	/** the text, shortened with ".." to fit the width */
	UnicodeString fitted( DisplayString *s, const UnicodeString &text, Int width )
	{
		s->setWordWrap( 0 );
		s->setText( text );
		if( s->getWidth() <= width )
			return text;
		UnicodeString shortened = text;
		while( shortened.getLength() > 1 )
		{
			shortened.removeLastChar();
			UnicodeString tried = shortened;
			tried.concat( L".." );
			s->setText( tried );
			if( s->getWidth() <= width )
				return tried;
		}
		return shortened;
	}

	//-----------------------------------------------------------------------------------------------
	void colors( Color *accent, Color *accentDim )
	{
		const Player *me = ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
		UnsignedByte r = 90, g = 170, b = 255, a = 255;
		if( me != nullptr )
			GameGetColorComponents( me->getPlayerColor(), &r, &g, &b, &a );
		*accent = GameMakeColor( r, g, b, 255 );
		*accentDim = GameMakeColor( (UnsignedByte)( r * 45 / 100 ), (UnsignedByte)( g * 45 / 100 ), (UnsignedByte)( b * 45 / 100 ), 235 );
	}

	//-----------------------------------------------------------------------------------------------
	const SpawnItem *hoveredItem()
	{
		std::vector<SpawnItem> *items = currentItems();
		if( items == nullptr || s_hover < HIT_ITEM )
			return nullptr;
		const Int i = s_hover - HIT_ITEM;
		return i >= 0 && i < (Int)items->size() ? &(*items)[i] : nullptr;
	}

	//-----------------------------------------------------------------------------------------------
	const SpawnItem *placingItem()
	{
		for( size_t f = 0; f < s_factions.size() && s_placing != nullptr; ++f )
			for( Int c = 0; c < CAT_COUNT; ++c )
				for( size_t i = 0; i < s_factions[f].items[c].size(); ++i )
					if( s_factions[f].items[c][i].thing == s_placing )
						return &s_factions[f].items[c][i];
		return nullptr;
	}

	//-----------------------------------------------------------------------------------------------
	const SpawnFaction *factionOf( const SpawnItem *item )
	{
		for( size_t f = 0; f < s_factions.size() && item != nullptr; ++f )
			for( Int c = 0; c < CAT_COUNT; ++c )
				for( size_t i = 0; i < s_factions[f].items[c].size(); ++i )
					if( &s_factions[f].items[c][i] == item )
						return &s_factions[f];
		return nullptr;
	}

	//-----------------------------------------------------------------------------------------------
	void drawMenu( GameWindow *, WinInstanceData * )
	{
		if( TheDisplay == nullptr || s_factions.empty() )
			return;
		ensureStrings();
		SpawnLayout L;
		computeLayout( &L );
		Color accent, accentDim;
		colors( &accent, &accentDim );
		const Color panel = GameMakeColor( 12, 16, 20, 230 ), panelEdge = GameMakeColor( 60, 70, 78, 255 );
		const Color text = GameMakeColor( 232, 238, 242, 255 ), textDim = GameMakeColor( 150, 160, 168, 255 );
		const Color cellBack = GameMakeColor( 24, 28, 33, 255 ), rowBack = GameMakeColor( 26, 31, 36, 230 );

		TheDisplay->drawFillRect( L.x, L.y, L.w, L.h, panel );
		TheDisplay->drawFillRect( L.x, L.y, 2, L.h, accent );
		TheDisplay->drawOpenRect( L.x, L.y, L.w, L.h, 1.0f, panelEdge );

		// the header: the title, how it works, and the close box
		UnicodeString s;
		s.translate( AsciiString( "SPAWN" ) );
		drawText( s_title, s, L.x + L.gap * 3, L.y + L.gap, accent );
		s_title->setText( s );
		const Int titleW = s_title->getWidth();
		UnicodeString hint;
		hint.translate( AsciiString( "Left click: spawn    Drag: turn    Right click: exit" ) );
		const Int hintX = L.x + L.gap * 6 + titleW;
		drawText( s_small, fitted( s_small, hint, L.closeX - L.gap - hintX ), hintX,
			L.y + ( L.headerH - fontHeight( s_small ) ) / 2, textDim );
		const Bool overClose = s_hover == HIT_CLOSE;
		TheDisplay->drawFillRect( L.closeX, L.closeY, L.closeS, L.closeS, overClose ? accentDim : rowBack );
		TheDisplay->drawOpenRect( L.closeX, L.closeY, L.closeS, L.closeS, 1.0f, overClose ? accent : panelEdge );
		const Int inset = max( 3, L.closeS / 4 );
		TheDisplay->drawLine( L.closeX + inset, L.closeY + inset, L.closeX + L.closeS - inset, L.closeY + L.closeS - inset, 2.0f, text );
		TheDisplay->drawLine( L.closeX + L.closeS - inset, L.closeY + inset, L.closeX + inset, L.closeY + L.closeS - inset, 2.0f, text );

		// the factions
		const Int factionCount = (Int)s_factions.size();
		s_factionScroll = max( 0, min( s_factionScroll, factionCount - L.listRows ) );
		for( Int row = 0; row < L.listRows && s_factionScroll + row < factionCount; ++row )
		{
			const Int f = s_factionScroll + row;
			const Int ry = L.listY + row * L.rowH;
			const Bool on = f == s_faction, over = s_hover == HIT_FACTION + f;
			TheDisplay->drawFillRect( L.listX, ry + 1, L.listW, L.rowH - 2, on ? accentDim : ( over ? GameMakeColor( 40, 47, 54, 235 ) : rowBack ) );
			if( on )
				TheDisplay->drawFillRect( L.listX, ry + 1, 3, L.rowH - 2, accent );
			drawText( s_small, fitted( s_small, s_factions[f].label, L.listW - 4 * L.gap ), L.listX + 2 * L.gap,
				ry + ( L.rowH - fontHeight( s_small ) ) / 2, on || over ? text : textDim );
		}
		const Int as = max( 3, L.gap );
		if( s_factionScroll > 0 )
			for( Int k = 0; k < as; ++k )
				TheDisplay->drawLine( L.listX + L.listW / 2 - k, L.listY - L.gap / 2 - as + k, L.listX + L.listW / 2 + k, L.listY - L.gap / 2 - as + k, 1.0f, accent );
		if( s_factionScroll + L.listRows < factionCount )
		{
			const Int by = L.listY + L.listRows * L.rowH;
			for( Int k = 0; k < as; ++k )
				TheDisplay->drawLine( L.listX + L.listW / 2 - k, by + as - k, L.listX + L.listW / 2 + k, by + as - k, 1.0f, accent );
		}

		// the kinds
		SpawnFaction *faction = currentFaction();
		const Int tabW = ( L.gridW - ( CAT_COUNT - 1 ) * L.gap ) / CAT_COUNT;
		for( Int c = 0; c < CAT_COUNT; ++c )
		{
			const Int tx = L.gridX + c * ( tabW + L.gap );
			const Bool on = c == s_category, over = s_hover == HIT_CATEGORY + c;
			const Bool empty = faction == nullptr || faction->items[c].empty();
			TheDisplay->drawFillRect( tx, L.tabsY, tabW, L.tabsH, on ? accentDim : ( over && !empty ? GameMakeColor( 40, 47, 54, 235 ) : rowBack ) );
			if( on )
				TheDisplay->drawFillRect( tx, L.tabsY + L.tabsH - 3, tabW, 3, accent );
			UnicodeString name;
			name.translate( AsciiString( CATEGORY_NAMES[c] ) );
			if( !empty )
			{
				UnicodeString counted;
				counted.format( L"%ls (%d)", name.str(), (Int)faction->items[c].size() );
				name = counted;
			}
			const UnicodeString shown = fitted( s_small, name, tabW - 2 * L.gap );
			s_small->setText( shown );
			drawText( s_small, shown, tx + ( tabW - s_small->getWidth() ) / 2, L.tabsY + ( L.tabsH - fontHeight( s_small ) ) / 2,
				empty ? GameMakeColor( 80, 86, 92, 255 ) : ( on || over ? text : textDim ) );
		}

		// the pictures
		std::vector<SpawnItem> *items = currentItems();
		const Int count = items != nullptr ? (Int)items->size() : 0;
		const Int rowsAll = ( count + L.cols - 1 ) / L.cols;
		s_scroll = max( 0, min( s_scroll, rowsAll - L.rows ) );
		for( Int i = s_scroll * L.cols; i < count && i < ( s_scroll + L.rows ) * L.cols; ++i )
		{
			const SpawnItem &item = (*items)[i];
			const Int row = i / L.cols - s_scroll, col = i % L.cols;
			const Int ix = L.gridX + col * ( L.cellW + L.gap ), iy = L.gridY + row * ( L.cellH + L.gap );
			TheDisplay->drawFillRect( ix, iy, L.cellW, L.cellH, cellBack );
			TheDisplay->drawImage( item.image, ix + 1, iy + 1, ix + L.cellW - 1, iy + L.cellH - 1 );
			const Bool over = s_hover == HIT_ITEM + i, inHand = item.thing == s_placing;
			if( inHand )
				TheDisplay->drawOpenRect( ix, iy, L.cellW, L.cellH, 3.0f, accent );
			else
				TheDisplay->drawOpenRect( ix, iy, L.cellW, L.cellH, over ? 2.0f : 1.0f, over ? accent : panelEdge );
		}
		if( count == 0 )
		{
			UnicodeString none;
			none.translate( AsciiString( "Nothing of this kind" ) );
			drawText( s_small, none, L.gridX + L.gap, L.gridY + L.gap, textDim );
		}
		const Int ax = L.gridX + L.gridW - 3 * as;
		if( s_scroll > 0 )
			for( Int k = 0; k < as; ++k )
				TheDisplay->drawLine( ax - k, L.gridY - L.gap / 2 - as + k, ax + k, L.gridY - L.gap / 2 - as + k, 1.0f, accent );
		if( s_scroll + L.rows < rowsAll )
		{
			const Int by = L.gridY + L.rows * ( L.cellH + L.gap ) - L.gap / 2;
			for( Int k = 0; k < as; ++k )
				TheDisplay->drawLine( ax - k, by + as - k, ax + k, by + as - k, 1.0f, accent );
		}

		// what the cursor is on (else what is in hand): its name, price and faction, and description
		const Int ix = L.x + L.gap, iw = L.w - 2 * L.gap;
		TheDisplay->drawFillRect( ix, L.infoY, iw, L.infoH, GameMakeColor( 22, 26, 31, 240 ) );
		TheDisplay->drawFillRect( ix, L.infoY, iw, 2, accentDim );
		const SpawnItem *shown = hoveredItem();
		if( shown == nullptr )
			shown = placingItem();
		Int ty = L.infoY + L.gap;
		if( shown != nullptr )
		{
			drawText( s_label, fitted( s_label, shown->name, iw - 4 * L.gap ), ix + 2 * L.gap, ty, text );
			ty += fontHeight( s_label ) + L.gap;
			const SpawnFaction *of = factionOf( shown );
			UnicodeString line;
			line.format( L"$%d    %ls%ls", shown->cost, of != nullptr ? of->label.str() : L"",
				shown->thing == s_placing ? L"    (in hand: left click on the ground to spawn it)" : L"" );
			drawText( s_small, fitted( s_small, line, iw - 4 * L.gap ), ix + 2 * L.gap, ty, GameMakeColor( 255, 214, 90, 255 ) );
			ty += fontHeight( s_small ) + L.gap;
			if( shown->button != nullptr && shown->button->getDescriptionLabel().isNotEmpty() )
			{
				Bool exists = FALSE;
				const UnicodeString description = TheGameText->fetch( shown->button->getDescriptionLabel(), &exists );
				if( exists )
				{
					IRegion2D clip;
					clip.lo.x = ix;
					clip.lo.y = L.infoY;
					clip.hi.x = ix + iw;
					clip.hi.y = L.infoY + L.infoH;
					s_info->setClipRegion( &clip );
					s_info->setWordWrap( iw - 4 * L.gap );
					s_info->setText( description );
					s_info->draw( ix + 2 * L.gap, ty, textDim, GameMakeColor( 0, 0, 0, 200 ) );
					s_info->setWordWrap( 0 );
					clip.lo.x = clip.lo.y = 0;
					clip.hi.x = TheDisplay->getWidth();
					clip.hi.y = TheDisplay->getHeight();
					s_info->setClipRegion( &clip );
				}
			}
		}
		else
		{
			UnicodeString line;
			line.translate( AsciiString( "Point at a unit or a structure to see what it is; click it to take it in hand." ) );
			drawText( s_small, fitted( s_small, line, iw - 4 * L.gap ), ix + 2 * L.gap, ty, textDim );
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** what is under the point (HIT_*), or HIT_NONE off the panel */
	Int hitTest( Int px, Int py )
	{
		SpawnLayout L;
		computeLayout( &L );
		if( px < L.x || py < L.y || px >= L.x + L.w || py >= L.y + L.h )
			return HIT_NONE;
		if( px >= L.closeX && px < L.closeX + L.closeS && py >= L.closeY && py < L.closeY + L.closeS )
			return HIT_CLOSE;
		if( px >= L.listX && px < L.listX + L.listW && py >= L.listY && py < L.listY + L.listRows * L.rowH )
		{
			const Int f = s_factionScroll + ( py - L.listY ) / max( 1, L.rowH );
			return f >= 0 && f < (Int)s_factions.size() ? HIT_FACTION + f : HIT_PANEL;
		}
		const Int tabW = ( L.gridW - ( CAT_COUNT - 1 ) * L.gap ) / CAT_COUNT;
		if( py >= L.tabsY && py < L.tabsY + L.tabsH && px >= L.gridX && px < L.gridX + L.gridW )
		{
			const Int c = ( px - L.gridX ) / max( 1, tabW + L.gap );
			return c >= 0 && c < CAT_COUNT ? HIT_CATEGORY + c : HIT_PANEL;
		}
		std::vector<SpawnItem> *items = currentItems();
		if( items != nullptr && px >= L.gridX && px < L.gridX + L.gridW && py >= L.gridY && py < L.gridY + L.rows * ( L.cellH + L.gap ) )
		{
			const Int col = ( px - L.gridX ) / max( 1, L.cellW + L.gap ), row = ( py - L.gridY ) / max( 1, L.cellH + L.gap );
			const Int cx = L.gridX + col * ( L.cellW + L.gap ), cy = L.gridY + row * ( L.cellH + L.gap );
			const Int i = ( s_scroll + row ) * L.cols + col;
			if( col < L.cols && px < cx + L.cellW && py < cy + L.cellH && i >= 0 && i < (Int)items->size() )
				return HIT_ITEM + i;
		}
		return HIT_PANEL;
	}

	//-----------------------------------------------------------------------------------------------
	/** puts the item in hand: its model follows the cursor, as a structure's being placed */
	void pick( const SpawnItem &item )
	{
		if( TheInGameUI == nullptr )
			return;
		s_placing = item.thing;
		TheInGameUI->placeBuildAvailable( item.thing, nullptr );
		if( TheInGameUI->getPendingPlaceType() != item.thing )
			s_placing = nullptr;
	}

	//-----------------------------------------------------------------------------------------------
	WindowMsgHandledType inputMenu( GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData )
	{
		const Int px = (Short)( mData1 & 0xFFFF ), py = (Short)( ( mData1 >> 16 ) & 0xFFFF );
		switch( msg )
		{
			case GWM_MOUSE_POS:
				s_mouseX = px;
				s_mouseY = py;
				s_hover = hitTest( px, py );
				return MSG_HANDLED;
			case GWM_MOUSE_LEAVING:
				s_hover = HIT_NONE;
				return MSG_HANDLED;
			case GWM_LEFT_DOWN:
			{
				const Int hit = hitTest( px, py );
				if( hit == HIT_CLOSE )
					AbramSpawnMenu::close();
				else if( hit >= HIT_ITEM )
				{
					std::vector<SpawnItem> *items = currentItems();
					if( items != nullptr && hit - HIT_ITEM < (Int)items->size() )
						pick( (*items)[hit - HIT_ITEM] );
				}
				else if( hit >= HIT_CATEGORY )
				{
					SpawnFaction *faction = currentFaction();
					if( faction != nullptr && !faction->items[hit - HIT_CATEGORY].empty() && s_category != hit - HIT_CATEGORY )
					{
						s_category = hit - HIT_CATEGORY;
						s_scroll = 0;
					}
				}
				else if( hit >= HIT_FACTION && s_faction != hit - HIT_FACTION )
				{
					s_faction = hit - HIT_FACTION;
					s_scroll = 0;
					chooseCategory();
				}
				return MSG_HANDLED;
			}
			case GWM_RIGHT_DOWN:
				// a right click: done
				AbramSpawnMenu::close();
				return MSG_HANDLED;
			case GWM_WHEEL_UP:
			case GWM_WHEEL_DOWN:
			{
				const Int step = msg == GWM_WHEEL_DOWN ? 1 : -1;
				SpawnLayout L;
				computeLayout( &L );
				if( s_mouseX >= L.listX && s_mouseX < L.listX + L.listW )
					s_factionScroll = max( 0, s_factionScroll + step );
				else
					s_scroll = max( 0, s_scroll + step );
				s_hover = HIT_NONE;
				return MSG_HANDLED;
			}
			case GWM_LEFT_UP:
			case GWM_RIGHT_UP:
				return MSG_HANDLED;
			default:
				return MSG_IGNORED;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void AbramSpawnMenu::open()
{
	if( !mayShow() )
		return;
	if( s_window != nullptr )
		return;

	buildList();
	if( s_factions.empty() )
		return;

	// the player's own faction first shown
	s_faction = 0;
	const Player *me = ThePlayerList->getLocalPlayer();
	for( size_t f = 0; f < s_factions.size() && me != nullptr; ++f )
	{
		if( s_factions[f].side.compareNoCase( me->getSide() ) == 0 )
		{
			s_faction = (Int)f;
			break;
		}
	}
	s_category = CAT_INFANTRY;
	chooseCategory();
	s_scroll = 0;
	s_hover = HIT_NONE;
	s_placing = nullptr;

	ensureStrings();
	SpawnLayout L;
	computeLayout( &L );
	s_factionScroll = s_faction >= L.listRows ? s_faction - L.listRows / 2 : 0;
	s_window = TheWindowManager->winCreate( nullptr, WIN_STATUS_ENABLED | WIN_STATUS_ABOVE | WIN_STATUS_RIGHT_CLICK,
		L.x, L.y, L.w, L.h, nullptr, nullptr );
	if( s_window == nullptr )
	{
		s_factions.clear();
		return;
	}
	s_window->winSetDrawFunc( drawMenu );
	s_window->winSetInputFunc( inputMenu );
}

//-------------------------------------------------------------------------------------------------
void AbramSpawnMenu::close()
{
	if( s_placing != nullptr && TheInGameUI != nullptr && TheInGameUI->getPendingPlaceType() == s_placing )
		TheInGameUI->placeBuildAvailable( nullptr, nullptr );
	s_placing = nullptr;
	if( s_window != nullptr && TheWindowManager != nullptr )
		TheWindowManager->winDestroy( s_window );
	s_window = nullptr;
	s_factions.clear();
	s_hover = HIT_NONE;
}

//-------------------------------------------------------------------------------------------------
Bool AbramSpawnMenu::isOpen()
{
	return s_window != nullptr;
}

//-------------------------------------------------------------------------------------------------
void AbramSpawnMenu::update()
{
	if( s_window == nullptr )
		return;
	if( !mayShow() )
	{
		close();
		return;
	}
	// what was in hand was put away (a right click on the ground, another command): the menu goes with it
	if( s_placing != nullptr && TheInGameUI->getPendingPlaceType() != s_placing )
	{
		s_placing = nullptr;
		close();
		return;
	}
	// the screen may have changed size
	ensureStrings();
	SpawnLayout L;
	computeLayout( &L );
	Int x = 0, y = 0, w = 0, h = 0;
	s_window->winGetScreenPosition( &x, &y );
	s_window->winGetSize( &w, &h );
	if( x != L.x || y != L.y || w != L.w || h != L.h )
	{
		s_window->winSetPosition( L.x, L.y );
		s_window->winSetSize( L.w, L.h );
	}
}

//-------------------------------------------------------------------------------------------------
void AbramSpawnMenu::reset()
{
	close();
	if( s_label != nullptr && TheDisplayStringManager != nullptr )
	{
		TheDisplayStringManager->freeDisplayString( s_title );
		TheDisplayStringManager->freeDisplayString( s_label );
		TheDisplayStringManager->freeDisplayString( s_small );
		TheDisplayStringManager->freeDisplayString( s_info );
	}
	s_title = s_label = s_small = s_info = nullptr;
	s_fontScreenHeight = 0;
}

//-------------------------------------------------------------------------------------------------
Bool AbramSpawnMenu::isPlacing()
{
	return s_placing != nullptr && s_window != nullptr && TheInGameUI != nullptr && TheInGameUI->getPendingPlaceType() == s_placing;
}

//-------------------------------------------------------------------------------------------------
LegalBuildCode AbramSpawnMenu::checkPlacement( const Coord3D *where, Real angle )
{
	return AbramCheats::checkSpawnLocation( s_placing, where, angle );
}

//-------------------------------------------------------------------------------------------------
void AbramSpawnMenu::spawnAt( const Coord3D *where, Real angle )
{
	if( !isPlacing() || where == nullptr )
		return;
	const LegalBuildCode lbc = checkPlacement( where, angle );
	if( lbc != LBC_OK )
	{
		// as a builder's refused placement: say why, and keep it in hand
		TheInGameUI->displayCantBuildMessage( lbc );
		static AudioEventRTS noCanDoSound( "NoCanDoSound" );
		TheAudio->addAudioEvent( &noCanDoSound );
		return;
	}
	GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_ABRAM_CHEAT_SPAWN );
	msg->appendIntegerArgument( s_placing->getTemplateID() );
	msg->appendLocationArgument( *where );
	msg->appendRealArgument( angle );
	if( s_placing->isKindOf( KINDOF_STRUCTURE ) )
	{
		AudioEventRTS placed( "PlaceBuilding" );
		placed.setPosition( where );
		TheAudio->addAudioEvent( &placed );
	}
}

//-------------------------------------------------------------------------------------------------
Bool AbramSpawnMenu::testPick( const char *templateName )
{
	if( s_window == nullptr || templateName == nullptr )
		return FALSE;
	for( size_t f = 0; f < s_factions.size(); ++f )
	{
		for( Int c = 0; c < CAT_COUNT; ++c )
		{
			for( size_t i = 0; i < s_factions[f].items[c].size(); ++i )
			{
				if( s_factions[f].items[c][i].thing->getName().compareNoCase( templateName ) != 0 )
					continue;
				s_faction = (Int)f;
				s_category = c;
				s_scroll = 0;
				pick( s_factions[f].items[c][i] );
				return isPlacing();
			}
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void AbramSpawnMenu::describe( char *buffer, Int size )
{
	if( buffer == nullptr || size <= 0 )
		return;
	if( s_window == nullptr )
	{
		snprintf( buffer, size, "closed" );
		return;
	}
	Int total = 0;
	for( size_t f = 0; f < s_factions.size(); ++f )
		for( Int c = 0; c < CAT_COUNT; ++c )
			total += (Int)s_factions[f].items[c].size();
	const SpawnFaction *faction = currentFaction();
	AsciiString label;
	if( faction != nullptr )
		label.translate( faction->label );
	std::vector<SpawnItem> *items = currentItems();
	snprintf( buffer, size, "open: %d factions, %d objects; shown: %s (%s), %s, %d objects; in hand: %s",
		(int)s_factions.size(), (int)total, label.str(), faction != nullptr ? faction->side.str() : "-",
		CATEGORY_NAMES[s_category], items != nullptr ? (int)items->size() : 0,
		s_placing != nullptr ? s_placing->getName().str() : "nothing" );
}
