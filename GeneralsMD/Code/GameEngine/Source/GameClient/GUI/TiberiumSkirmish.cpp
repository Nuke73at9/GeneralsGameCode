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

// Tiberium Project @feature the Tiberium skirmish screen (TiberiumSkirmish.h).

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "GameClient/TiberiumSkirmish.h"
#include "GameClient/TiberiumSettings.h"
#include "GameClient/TiberiumSkin.h"
#include "GameClient/AbramMatchOptionsMenu.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetCheckBox.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetRadioButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/Image.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/MapUtil.h"
#include "GameClient/Mouse.h"
#include "GameClient/WindowLayout.h"
#include "GameNetwork/GameInfo.h"

#include <string>
#include <vector>

using namespace TiberiumSkin;

extern WindowLayout *skirmishMapSelectLayout;	///< Zero Hour's map list, while it is up (SkirmishGameOptionsMenu.cpp)

namespace
{
	//-----------------------------------------------------------------------------------------------
	enum Field { FIELD_PLAYER, FIELD_FACTION, FIELD_TEAM, FIELD_COLOR, FIELD_COUNT };
	enum { ROW_CASH = -1 };	///< the row of the starting cash's list
	enum { LIST_ROWS = 14, STAT_COUNT = 4 };
	enum ActKind
	{
		ACT_LIST,			///< a: the row (ROW_CASH: the starting cash), b: the field; its list opened, or shut again
		ACT_CHOOSE,		///< a: the open list's entry
		ACT_START,		///< a: the start position on the map: taken, or handed to the next player
		ACT_UNSTART,	///< a: the start position left
		ACT_LIMIT, ACT_MAPS, ACT_RESET, ACT_PLAY, ACT_BACK,
		ACT_OPTIONS,	///< the match options' menu opened, or shut again
		ACT_OPTION,		///< a: the match option changed, as a click on its line in the mod's dialog changes it
		ACT_OPTIONS_DONE,
		ACT_MAP_ROW,	///< a: the map list's row, b: chosen at once (a double click)
		ACT_MAP_TAB,	///< a: 0 the game's maps, 1 the player's
		ACT_MAP_USE, ACT_MAP_BACK
	};
	struct Act
	{
		ActKind kind;
		Int a, b;
	};
	struct Hit
	{
		Act act;
		Int x, y, w, h;
	};

	GameWindow *s_window = nullptr;			///< the screen's own window, over Zero Hour's
	Bool s_closed = TRUE;								///< Zero Hour's skirmish screen is not the screen, or is on its way out
	GameWindow *s_parent = nullptr;			///< Zero Hour's skirmish screen: its gadgets are set and pressed through it
	GameWindow *s_field[MAX_SLOTS][FIELD_COUNT];	///< its combo boxes, a row a slot (the first has no player's)
	Int s_rowCount = 0;									///< the slots its layout has rows for
	GameWindow *s_start[MAX_SLOTS];			///< its start position buttons on the map
	GameWindow *s_name = nullptr, *s_map = nullptr, *s_cash = nullptr, *s_limit = nullptr;
	GameWindow *s_buttonBack = nullptr, *s_buttonPlay = nullptr, *s_buttonMaps = nullptr, *s_buttonReset = nullptr;
	GameWindow *s_stat[STAT_COUNT];

	WindowLayout *s_mapLayout = nullptr;	///< Zero Hour's map list while it is up: shown here in its place
	GameWindow *s_mapParent = nullptr, *s_mapList = nullptr, *s_mapPreview = nullptr, *s_mapBack = nullptr, *s_mapUse = nullptr;
	GameWindow *s_mapTab[2];
	Int s_mapTop = 0;										///< the first row of the map list shown
	Bool s_mapCentre = FALSE;						///< the chosen map is to be brought in sight when the list is next drawn
	Int s_mapRows = 1;									///< the rows of it that fit, as last drawn

	GameWindow *s_list = nullptr;				///< the combo box whose list is open
	Bool s_options = FALSE;							///< the match options' menu is open over the screen
	Int s_editOption = -1;							///< Tiberium Project @feature the match option a number is being typed into (-1: none)
	std::string s_editDigits;						///< what is typed so far
	Bool s_editFresh = FALSE;						///< the first digit typed replaces the number shown
	Bool s_under = FALSE;								///< what is being drawn lies under an open list or menu: it answers no mouse
	Int s_listRow = 0, s_listField = 0;
	Int s_listX = 0, s_listY = 0, s_listW = 0, s_listH = 0;	///< under its field, as last drawn
	Int s_listTop = 0;

	std::vector<Hit> s_hits;						///< as last drawn; a later one lies over an earlier
	std::vector<Act> s_acts;						///< clicked, to be done by the next update
	Int s_mx = -1, s_my = -1;

	const char *const s_statName[STAT_COUNT] = { "StaticTextWinsValue", "StaticTextLossesValue", "StaticTextStreakValue", "StaticTextBestStreakValue" };
	const char *const s_statLabel[STAT_COUNT] = { "WINS", "LOSSES", "STREAK", "BEST" };

	//-----------------------------------------------------------------------------------------------
	GameWindow *find( GameWindow *parent, const char *layout, const char *name )
	{
		AsciiString full;
		full.format( "%s:%s", layout, name );
		return TheWindowManager->winGetWindowFromId( parent, TheNameKeyGenerator->nameToKey( full ) );
	}

	GameWindow *findNumbered( const char *name, Int number )
	{
		AsciiString numbered;
		numbered.format( "%s%d", name, (int)number );
		return find( s_parent, "SkirmishGameOptionsMenu.wnd", numbered.str() );
	}

	//-----------------------------------------------------------------------------------------------
	/** Zero Hour's skirmish screen's gadgets, by their names in its layout */
	void findGadgets()
	{
		static const char *const fieldName[FIELD_COUNT] = { "ComboBoxPlayer", "ComboBoxPlayerTemplate", "ComboBoxTeam", "ComboBoxColor" };
		s_rowCount = 0;
		for( Int i = 0; i < MAX_SLOTS; ++i )
		{
			for( Int f = 0; f < FIELD_COUNT; ++f )
				s_field[i][f] = i == 0 && f == FIELD_PLAYER ? nullptr : findNumbered( fieldName[f], i );
			s_start[i] = findNumbered( "ButtonMapStartPosition", i );
			// (the rows are counted as Zero Hour's screen counts them: the original layout has 8, the mod's 16)
			if( s_rowCount == i && s_field[i][FIELD_FACTION] != nullptr && s_field[i][FIELD_TEAM] != nullptr && s_field[i][FIELD_COLOR] != nullptr
					&& ( i == 0 || s_field[i][FIELD_PLAYER] != nullptr ) )
				s_rowCount = i + 1;
		}
		s_name = find( nullptr, "SkirmishGameOptionsMenu.wnd", "TextEntryPlayerName" );
		s_map = find( s_parent, "SkirmishGameOptionsMenu.wnd", "MapWindow" );
		s_cash = find( s_parent, "SkirmishGameOptionsMenu.wnd", "ComboBoxStartingCash" );
		s_limit = find( s_parent, "SkirmishGameOptionsMenu.wnd", "CheckboxLimitSuperweapons" );
		s_buttonBack = find( s_parent, "SkirmishGameOptionsMenu.wnd", "ButtonBack" );
		s_buttonPlay = find( s_parent, "SkirmishGameOptionsMenu.wnd", "ButtonStart" );
		s_buttonMaps = find( s_parent, "SkirmishGameOptionsMenu.wnd", "ButtonSelectMap" );
		s_buttonReset = find( s_parent, "SkirmishGameOptionsMenu.wnd", "ButtonReset" );
		for( Int i = 0; i < STAT_COUNT; ++i )
			s_stat[i] = find( nullptr, "SkirmishGameOptionsMenu.wnd", s_statName[i] );
	}

	//-----------------------------------------------------------------------------------------------
	/** Zero Hour's map list's gadgets, while it is up */
	void findMapGadgets()
	{
		// (its top window, which takes its gadgets' messages. Zero Hour's own code looks it up under a misspelt name,
		// finds nothing and so searches every window for its gadgets: not to be copied)
		s_mapParent = find( nullptr, "SkirmishMapSelectMenu.wnd", "SkirmishMapSelectMenuParent" );
		if( s_mapParent == nullptr && s_mapLayout != nullptr )
			s_mapParent = s_mapLayout->getFirstWindow();
		s_mapList = find( s_mapParent, "SkirmishMapSelectMenu.wnd", "ListboxMap" );
		s_mapPreview = find( s_mapParent, "SkirmishMapSelectMenu.wnd", "WinMapPreview" );
		s_mapBack = find( s_mapParent, "SkirmishMapSelectMenu.wnd", "ButtonBack" );
		s_mapUse = find( s_mapParent, "SkirmishMapSelectMenu.wnd", "ButtonOK" );
		s_mapTab[0] = find( s_mapParent, "SkirmishMapSelectMenu.wnd", "RadioButtonSystemMaps" );
		s_mapTab[1] = find( s_mapParent, "SkirmishMapSelectMenu.wnd", "RadioButtonUserMaps" );
	}

	void forgetMapGadgets()
	{
		s_mapLayout = nullptr;
		s_mapParent = s_mapList = s_mapPreview = s_mapBack = s_mapUse = nullptr;
		s_mapTab[0] = s_mapTab[1] = nullptr;
		s_mapTop = 0;
		s_mapCentre = FALSE;
	}

	//-----------------------------------------------------------------------------------------------
	/** Zero Hour's map list is up and its gadgets here are its own. It makes and destroys that list itself, on its own
		buttons and in the middle of a frame: nothing kept of it is touched unless this says so (its windows, once
		destroyed, are freed memory: the first PC run crashed in the frame after its Back, drawing from them) */
	Bool mapsUp()
	{
		return s_mapLayout != nullptr && s_mapLayout == skirmishMapSelectLayout;
	}

	/** what is kept of Zero Hour's map list brought in line with it: forgotten when it has gone, found when it came */
	void followMapList()
	{
		if( skirmishMapSelectLayout == s_mapLayout )
			return;
		forgetMapGadgets();
		s_mapLayout = skirmishMapSelectLayout;
		s_list = nullptr;
		if( s_mapLayout != nullptr )
		{
			findMapGadgets();
			s_mapCentre = TRUE;
		}
	}

	//-----------------------------------------------------------------------------------------------
	GameWindow *comboOf( Int row, Int field )
	{
		if( row == ROW_CASH )
			return s_cash;
		if( row < 0 || row >= s_rowCount || field < 0 || field >= FIELD_COUNT )
			return nullptr;
		return s_field[row][field];
	}

	Int selectedOf( GameWindow *combo )
	{
		Int selected = -1;
		if( combo != nullptr )
			GadgetComboBoxGetSelectedPos( combo, &selected );
		return selected;
	}

	/** a combo box's entry: its words, and the colour Zero Hour's list writes them in (a colour's own, in the colours' list) */
	UnicodeString entryOf( GameWindow *combo, Int index, Color *color )
	{
		GameWindow *list = combo != nullptr ? GadgetComboBoxGetListBox( combo ) : nullptr;
		Color c = 0;
		UnicodeString text;
		if( list != nullptr && index >= 0 && index < GadgetComboBoxGetLength( combo ) )
			text = GadgetListBoxGetTextAndColor( list, &c, index, 0 );
		if( color != nullptr )
			*color = c;
		return text;
	}

	/** a button of Zero Hour's pressed, as its own click does it */
	void press( GameWindow *parent, GameWindow *button, UnsignedInt message )
	{
		if( parent != nullptr && button != nullptr )
			TheWindowManager->winSendSystemMsg( parent, message, (WindowMsgData)button, button->winGetWindowId() );
	}

	Int mapColumn()
	{
		return s_mapList != nullptr ? max( 0, GadgetListBoxGetNumColumns( s_mapList ) - 1 ) : 0;
	}

	//-----------------------------------------------------------------------------------------------
	/** Tiberium Project @feature a number typed into a match option (the custom army cap's points): set (kept within
		its range), or left as it was when nothing was typed */
	void commitEdit()
	{
		if( s_editOption < 0 )
			return;
		const Int option = s_editOption;
		s_editOption = -1;
		if( s_editDigits.empty() || TheSkirmishGameInfo == nullptr )
			return;
		AbramMatchOptionsMenu::setOptionNumber( option, TheSkirmishGameInfo,
			AbramMatchOptionsMenu::numberOf( s_editDigits, AbramMatchOptionsMenu::getOptionNumberMax( option ) ) );
	}

	//-----------------------------------------------------------------------------------------------
	void run( const Act &act )
	{
		followMapList();
		const Bool maps = mapsUp();
		// (a click anywhere but the line being typed into sets what was typed)
		if( s_editOption >= 0 && !( act.kind == ACT_OPTION && act.a == s_editOption ) )
			commitEdit();
		switch( act.kind )
		{
			case ACT_LIST:
			{
				GameWindow *combo = comboOf( act.a, act.b );
				if( maps || s_options || combo == nullptr || !combo->winGetEnabled() || combo == s_list )
				{
					s_list = nullptr;
					break;
				}
				s_list = combo;
				s_listRow = act.a;
				s_listField = act.b;
				// (the chosen entry in sight where the list is longer than it shows)
				s_listTop = max( 0, min( selectedOf( combo ) - LIST_ROWS / 2, GadgetComboBoxGetLength( combo ) - LIST_ROWS ) );
				s_listW = 0;
				break;
			}
			case ACT_CHOOSE:
				// the entry set in Zero Hour's combo box: its screen takes it as a choice made there
				if( s_list != nullptr && act.a >= 0 && act.a < GadgetComboBoxGetLength( s_list ) )
					GadgetComboBoxSetSelectedPos( s_list, act.a );
				s_list = nullptr;
				break;
			case ACT_START:
			case ACT_UNSTART:
				if( !maps && act.a >= 0 && act.a < MAX_SLOTS )
					press( s_parent, s_start[act.a], act.kind == ACT_START ? GBM_SELECTED : GBM_SELECTED_RIGHT );
				break;
			case ACT_LIMIT:
				// (the check box tells Zero Hour's screen itself)
				if( !maps && s_limit != nullptr )
					GadgetCheckBoxSetChecked( s_limit, !GadgetCheckBoxIsChecked( s_limit ) );
				break;
			case ACT_MAPS:
				if( !maps )
					press( s_parent, s_buttonMaps, GBM_SELECTED );
				break;
			case ACT_OPTIONS:
				// the mod's match options, as a menu of this screen (its own dialog, in Zero Hour's look, is not opened)
				s_options = !maps && !s_options;
				s_list = nullptr;
				break;
			case ACT_OPTION:
				// (the mod's own code changes the option, as a click on its line in its dialog does; a line that takes a
				// number, the custom army cap's points, is typed into instead: a click starts it (and makes the cap Custom),
				// Enter, a second click or a click elsewhere sets it, Escape leaves it as it was)
				if( s_options && !maps && AbramMatchOptionsMenu::isNumberOption( act.a ) )
				{
					if( s_editOption == act.a )
						commitEdit();
					else
					{
						AbramMatchOptionsMenu::changeOption( act.a, TheSkirmishGameInfo );
						char digits[16];
						snprintf( digits, sizeof( digits ), "%d", (int)AbramMatchOptionsMenu::getOptionNumber( act.a, TheSkirmishGameInfo ) );
						s_editDigits = digits;
						s_editOption = act.a;
						s_editFresh = TRUE;
					}
				}
				else if( s_options && !maps )
					AbramMatchOptionsMenu::changeOption( act.a, TheSkirmishGameInfo );
				break;
			case ACT_OPTIONS_DONE:
				s_options = FALSE;
				s_editOption = -1;	// Tiberium Project
				break;
			case ACT_RESET:
				if( !maps )
					press( s_parent, s_buttonReset, GBM_SELECTED );
				break;
			case ACT_PLAY:
				if( !maps )
					press( s_parent, s_buttonPlay, GBM_SELECTED );
				break;
			case ACT_BACK:
				if( !maps )
					press( s_parent, s_buttonBack, GBM_SELECTED );
				break;
			case ACT_MAP_ROW:
				if( maps && s_mapList != nullptr && act.a >= 0 && act.a < GadgetListBoxGetNumEntries( s_mapList ) )
				{
					GadgetListBoxSetSelected( s_mapList, act.a );
					if( act.b != 0 )
						press( s_mapParent, s_mapUse, GBM_SELECTED );
				}
				break;
			case ACT_MAP_TAB:
				if( maps && act.a >= 0 && act.a < 2 && s_mapTab[act.a] != nullptr )
				{
					GadgetRadioSetSelection( s_mapTab[act.a], TRUE );
					s_mapTop = 0;
				}
				break;
			case ACT_MAP_USE:
				if( maps )
					press( s_mapParent, s_mapUse, GBM_SELECTED );
				break;
			case ACT_MAP_BACK:
				if( maps )
					press( s_mapParent, s_mapBack, GBM_SELECTED );
				break;
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** a part that answers a click; says whether the mouse is on it (never while a list or the match options' menu
		is open over it) */
	Bool addHit( ActKind kind, Int a, Int b, Int x, Int y, Int w, Int h )
	{
		Hit hit;
		hit.act.kind = kind;
		hit.act.a = a;
		hit.act.b = b;
		hit.x = x;
		hit.y = y;
		hit.w = w;
		hit.h = h;
		s_hits.push_back( hit );
		return !s_under && s_mx >= x && s_mx < x + w && s_my >= y && s_my < y + h;
	}

	UnicodeString wide( const char *text )
	{
		UnicodeString u;
		u.translate( AsciiString( text ) );
		return u;
	}

	//-----------------------------------------------------------------------------------------------
	/** a map's picture with what is on it: supplies, tech buildings, and its start positions with the number of the
		player on each. Where the screen is live a start position answers a click. */
	void drawMap( const Look &l, Real u, const MapMetaData *md, const Image *image, Int x, Int y, Int w, Int h, Bool live, Int size )
	{
		if( md == nullptr || w <= 0 || h <= 0 || md->m_extent.width() <= 0.0f || md->m_extent.height() <= 0.0f )
		{
			const char *none = "No map chosen";
			drawText( none, size, FALSE, x + ( w - widthOf( none, size, FALSE ) ) / 2, y + h / 2, l.textDim );
			return;
		}
		ICoord2D ul, lr;
		findDrawPositions( x, y, w, h, md->m_extent, &ul, &lr );
		if( image != nullptr )
			TheDisplay->drawImage( image, ul.x, ul.y, lr.x, lr.y );
		else
		{
			TheDisplay->drawFillRect( ul.x, ul.y, lr.x - ul.x, lr.y - ul.y, l.fill );
			const char *none = "No picture of this map";
			drawText( none, size, FALSE, x + ( w - widthOf( none, size, FALSE ) ) / 2, y + h / 2, l.textDim );
		}
		TheDisplay->drawOpenRect( ul.x, ul.y, lr.x - ul.x, lr.y - ul.y, 1.0f, l.accentDim );
		const Real mw = md->m_extent.width(), mh = md->m_extent.height();
		const Int pw = lr.x - ul.x, ph = lr.y - ul.y;
		#define MAPX( wx ) ( ul.x + (Int)( ( ( wx ) - md->m_extent.lo.x ) / mw * pw ) )
		#define MAPY( wy ) ( ul.y + (Int)( ( 1.0f - ( ( wy ) - md->m_extent.lo.y ) / mh ) * ph ) )
		// supplies: a small gold square; tech buildings: a small diamond
		const Int dot = max( 2, (Int)( 3.0f * u ) );
		for( Coord3DList::const_iterator it = md->m_supplyPositions.begin(); it != md->m_supplyPositions.end(); ++it )
		{
			TheDisplay->drawFillRect( MAPX( it->x ) - dot, MAPY( it->y ) - dot, 2 * dot, 2 * dot, GameMakeColor( 242, 193, 78, 255 ) );
			TheDisplay->drawOpenRect( MAPX( it->x ) - dot, MAPY( it->y ) - dot, 2 * dot, 2 * dot, 1.0f, GameMakeColor( 0, 0, 0, 255 ) );
		}
		for( Coord3DList::const_iterator it = md->m_techPositions.begin(); it != md->m_techPositions.end(); ++it )
		{
			const Int cx = MAPX( it->x ), cy = MAPY( it->y ), d = dot + 1;
			TheDisplay->drawLine( cx - d, cy, cx, cy - d, 2.0f, GameMakeColor( 255, 255, 255, 255 ) );
			TheDisplay->drawLine( cx, cy - d, cx + d, cy, 2.0f, GameMakeColor( 255, 255, 255, 255 ) );
			TheDisplay->drawLine( cx + d, cy, cx, cy + d, 2.0f, GameMakeColor( 255, 255, 255, 255 ) );
			TheDisplay->drawLine( cx, cy + d, cx - d, cy, 2.0f, GameMakeColor( 255, 255, 255, 255 ) );
		}
		// the start positions: a box each, moved off one another where a map has them closer than a box is wide (as
		// Zero Hour's screen moves its buttons)
		const Int box = max( 12, (Int)( 18.0f * u ) );
		Int boxX[MAX_SLOTS], boxY[MAX_SLOTS];
		Bool has[MAX_SLOTS];
		for( Int i = 0; i < MAX_SLOTS; ++i )
		{
			has[i] = FALSE;
			if( i >= md->m_numPlayers || !md->m_isMultiplayer )
				continue;
			AsciiString name;
			name.format( "Player_%d_Start", (int)( i + 1 ) );
			WaypointMap::const_iterator at = md->m_waypoints.find( name );
			if( at == md->m_waypoints.end() )
				continue;
			has[i] = TRUE;
			boxX[i] = MAPX( at->second.x ) - box / 2;
			boxY[i] = MAPY( at->second.y ) - box / 2;
			for( Int pass = 0; pass < 4; ++pass )
			{
				Bool moved = FALSE;
				for( Int j = 0; j < i; ++j )
				{
					if( !has[j] )
						continue;
					const Int dx = boxX[i] - boxX[j], dy = boxY[i] - boxY[j];
					const Int inX = box + 1 - ( dx < 0 ? -dx : dx ), inY = box + 1 - ( dy < 0 ? -dy : dy );
					if( inX <= 0 || inY <= 0 )
						continue;
					// (the shorter way out of the other box)
					if( inX <= inY )
						boxX[i] += dx < 0 ? -inX : inX;
					else
						boxY[i] += dy < 0 ? -inY : inY;
					moved = TRUE;
				}
				if( !moved )
					break;
			}
		}
		for( Int i = 0; i < MAX_SLOTS; ++i )
		{
			if( !has[i] )
				continue;
			const Int bx = boxX[i], by = boxY[i];
			// the player who has it, where the screen is live (the map list shows the positions bare)
			Int player = -1;
			Color color = l.accent;
			for( Int s = 0; live && s < s_rowCount && TheSkirmishGameInfo != nullptr; ++s )
			{
				const GameSlot *slot = TheSkirmishGameInfo->getConstSlot( s );
				if( slot != nullptr && slot->isOccupied() && slot->getStartPos() == i )
				{
					player = s;
					const Int chosen = selectedOf( s_field[s][FIELD_COLOR] );
					if( chosen > 0 )
						entryOf( s_field[s][FIELD_COLOR], chosen, &color );
					break;
				}
			}
			const Bool hover = live && s_start[i] != nullptr && addHit( ACT_START, i, 0, bx, by, box, box );
			TheDisplay->drawFillRect( bx, by, box, box, player >= 0 ? ( color | 0xFF000000 ) : GameMakeColor( 0, 0, 0, 170 ) );
			TheDisplay->drawOpenRect( bx, by, box, box, hover ? 2.0f : 1.0f, hover ? l.hi : GameMakeColor( 255, 255, 255, 255 ) );
			if( player >= 0 )
			{
				UnicodeString number;
				number.format( L"%d", (int)( player + 1 ) );
				DisplayString *s = textOf( number, size, TRUE );
				if( s != nullptr )
					s->draw( bx + ( box - s->getWidth() ) / 2, by + ( box - s->getFont()->height ) / 2, GameMakeColor( 255, 255, 255, 255 ), GameMakeColor( 0, 0, 0, 255 ) );
			}
		}
		#undef MAPX
		#undef MAPY
	}

	//-----------------------------------------------------------------------------------------------
	/** a field showing a combo box's choice: opens its list where the combo box is one Zero Hour's screen lets be set */
	void drawCombo( const Look &l, Real u, Int row, Int field, Int x, Int y, Int w, Int h, Int size, Bool live )
	{
		GameWindow *combo = comboOf( row, field );
		if( combo == nullptr )
			return;
		const Bool can = live && combo->winGetEnabled();
		const Bool open = s_list == combo;
		const Bool hover = can && addHit( ACT_LIST, row, field, x, y, w, h );
		Int inset = 0;
		const Bool colours = row != ROW_CASH && field == FIELD_COLOR;
		drawField( l, x, y, w, h, u, colours ? UnicodeString::TheEmptyString : GadgetComboBoxGetText( combo ), size, hover || open, !can, TRUE );
		if( colours )
		{
			// a colour: its own patch before its name (the first entry is the random one: no patch)
			const Int chosen = selectedOf( combo );
			Color color = 0;
			const UnicodeString name = entryOf( combo, chosen, &color );
			const Int pad = max( 3, (Int)( 6.0f * u ) );
			if( chosen > 0 )
			{
				inset = h - 2 * pad + pad;
				TheDisplay->drawFillRect( x + pad, y + pad, h - 2 * pad, h - 2 * pad, color | 0xFF000000 );
			}
			DisplayString *s = textOf( name, size, FALSE );
			drawTextIn( name, size, FALSE, x + pad + inset, s != nullptr ? y + ( h - s->getFont()->height ) / 2 : y, w - 3 * pad - inset - max( 5, (Int)( 9.0f * u ) ),
				can ? l.text : l.textDim );
		}
		if( open )
		{
			s_listX = x;
			s_listY = y + h;
			s_listW = w;
			s_listH = h;
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** a player's card: the slot's number on its colour, who plays it, the side, the team, the colour, the start */
	void drawCard( const Look &l, Real u, Int i, Int x, Int y, Int w, Int h, Int size, Bool live )
	{
		const GameSlot *slot = TheSkirmishGameInfo->getConstSlot( i );
		if( slot == nullptr )
			return;
		const Bool in = slot->isOccupied();
		#define CS( v ) ( (Int)( ( v ) * u ) )
		TheDisplay->drawFillRect( x, y, w, h, in ? l.panel : ( ( l.panel & 0x00FFFFFF ) | ( 150u << 24 ) ) );
		TheDisplay->drawOpenRect( x, y, w, h, 1.0f, in ? l.accent : l.accentDim );
		// the number, on the player's colour
		Color color = in ? l.accentDim : l.fill;
		const Int chosen = selectedOf( s_field[i][FIELD_COLOR] );
		if( in && chosen > 0 )
			entryOf( s_field[i][FIELD_COLOR], chosen, &color );
		const Int tab = CS( 30 );
		TheDisplay->drawFillRect( x + 1, y + 1, tab, h - 2, color | 0xFF000000 );
		UnicodeString number;
		number.format( L"%d", (int)( i + 1 ) );
		DisplayString *n = textOf( number, size + 3, TRUE );
		if( n != nullptr )
			n->draw( x + 1 + ( tab - n->getWidth() ) / 2, y + ( h - n->getFont()->height ) / 2, in ? GameMakeColor( 255, 255, 255, 255 ) : l.textDim, GameMakeColor( 0, 0, 0, 255 ) );
		const Int fh = CS( 22 ), x0 = x + tab + CS( 8 ), y0 = y + CS( 5 ), y1 = y + h - CS( 5 ) - fh;
		const Int wide0 = CS( 166 ), gap = CS( 6 ), right = x + w - CS( 6 );
		// who plays it: the player's own name on the first, a list on the others
		if( i == 0 )
			drawField( l, x0, y0, wide0, fh, u, s_name != nullptr ? GadgetTextEntryGetText( s_name ) : slot->getName(), size, FALSE, FALSE, FALSE );
		else
			drawCombo( l, u, i, FIELD_PLAYER, x0, y0, wide0, fh, size, live );
		if( in )
		{
			drawCombo( l, u, i, FIELD_FACTION, x0 + wide0 + gap, y0, right - ( x0 + wide0 + gap ), fh, size, live );
			// (a team's name is a number or "None"; a colour's name needs more room, a font's letters being wider on some
			// machines: "Purple" was cut short on the PC)
			const Int teamW = CS( 70 ), colourW = CS( 112 );
			drawCombo( l, u, i, FIELD_TEAM, x0, y1, teamW, fh, size, live );
			drawCombo( l, u, i, FIELD_COLOR, x0 + teamW + gap, y1, colourW, fh, size, live );
			UnicodeString start;
			if( slot->getStartPos() >= 0 )
				start.format( L"Start %d", (int)( slot->getStartPos() + 1 ) );
			else
				start = L"Start: any";
			DisplayString *s = textOf( start, size, FALSE );
			drawText( start, size, FALSE, x0 + teamW + colourW + 2 * gap + CS( 6 ), s != nullptr ? y1 + ( fh - s->getFont()->height ) / 2 : y1, live ? l.textDim : l.accentDim );
		}
		#undef CS
	}

	//-----------------------------------------------------------------------------------------------
	/** the screen, on a 1280 by 720 sheet scaled to the screen's height and centred across it. Zero Hour's menu map
		goes on behind it, darkened. */
	void draw( GameWindow *, WinInstanceData * )
	{
		if( TheDisplay == nullptr || TheDisplayStringManager == nullptr || TheFontLibrary == nullptr || TheSkirmishGameInfo == nullptr || TheMapCache == nullptr )
			return;
		if( TheMouse != nullptr )
		{
			s_mx = TheMouse->getMouseStatus()->pos.x;
			s_my = TheMouse->getMouseStatus()->pos.y;
		}
		const Int sw = TheDisplay->getWidth(), sh = TheDisplay->getHeight();
		const Real u = sh / 720.0f;
		const Int ox = ( sw - (Int)( 1280.0f * u ) ) / 2;
		const Look l = lookOf( TiberiumSettings::getStyle() );
		#define PX( v ) ( ox + (Int)( ( v ) * u ) )
		#define PY( v ) ( (Int)( ( v ) * u ) )
		#define PS( v ) ( (Int)( ( v ) * u ) )
		const Int buttonSize = max( 9, PS( 15.0f ) ), labelSize = max( 8, PS( 12.0f ) ), textSize = max( 8, PS( 12.0f ) ), nameSize = max( 9, PS( 16.0f ) );
		const Bool maps = mapsUp();
		if( maps )
		{
			s_options = FALSE;
			s_editOption = -1;	// Tiberium Project
		}
		s_hits.clear();
		s_under = s_list != nullptr || s_options;

		drawBackdrop( l, ox, u, sw, sh, maps ? "SKIRMISH: CHOOSE A MAP" : "SKIRMISH", 205 );

		// the players: half down each side of the map (the first half's place is the map list's while that is up)
		const Bool alone = TheMapCache->findMap( TheSkirmishGameInfo->getMap() ) != nullptr && !TheMapCache->findMap( TheSkirmishGameInfo->getMap() )->m_isMultiplayer;
		const Int perSide = ( s_rowCount + 1 ) / 2;
		for( Int i = 0; i < s_rowCount; ++i )
		{
			const Int side = perSide > 0 ? i / perSide : 0;
			if( ( maps && side == 0 ) || ( alone && i > 0 ) )
				continue;
			drawCard( l, u, i, PX( side == 0 ? 34 : 862 ), PY( 74 + ( i - side * perSide ) * 64 ), PS( 384 ), PS( 58 ), textSize, !maps );
		}

		// the map, in C&C4's frame between them
		const MapMetaData *md = nullptr;
		const Image *image = nullptr;
		if( maps )
		{
			// (Zero Hour's list keeps the map its chosen row names with its own picture window)
			if( s_mapPreview != nullptr )
			{
				md = (const MapMetaData *)s_mapPreview->winGetUserData();
				if( BitIsSet( s_mapPreview->winGetStatus(), WIN_STATUS_IMAGE ) )
					image = s_mapPreview->winGetEnabledImage( 0 );
			}
		}
		else
		{
			md = TheMapCache->findMap( TheSkirmishGameInfo->getMap() );
			if( s_map != nullptr && BitIsSet( s_map->winGetStatus(), WIN_STATUS_IMAGE ) )
				image = s_map->winGetEnabledImage( 0 );
		}
		drawFrame( l, PX( 436 ), PY( 82 ), PS( 408 ), PS( 300 ), u );
		drawMap( l, u, md, image, PX( 448 ), PY( 94 ), PS( 384 ), PS( 276 ), !maps, textSize );
		if( md != nullptr )
		{
			drawText( md->m_displayName, nameSize, TRUE, PX( 640 ) - widthOf( md->m_displayName, nameSize, TRUE ) / 2, PY( 392 ), l.text );
			UnicodeString players;
			if( md->m_isMultiplayer )
				players.format( L"%d players", (int)md->m_numPlayers );
			else
				players = L"Played alone";
			drawText( players, textSize, FALSE, PX( 640 ) - widthOf( players, textSize, FALSE ) / 2, PY( 416 ), l.textDim );
		}

		if( !maps )
		{
			// under the map: the map's list and the match's options
			{
				const Int x = PX( 436 ), y = PY( 444 ), w = PS( 198 ), h = PS( 36 );
				drawButton( l, x, y, w, h, u, "SELECT MAP", labelSize, FALSE, addHit( ACT_MAPS, 0, 0, x, y, w, h ), FALSE );
			}
			{
				const Int x = PX( 646 ), y = PY( 444 ), w = PS( 198 ), h = PS( 36 );
				drawButton( l, x, y, w, h, u, "MATCH OPTIONS", labelSize, s_options, addHit( ACT_OPTIONS, 0, 0, x, y, w, h ), FALSE );
			}
			// (a font's letters are not as wide on every machine: a label is cut short before it runs into its field)
			drawTextIn( wide( "STARTING CASH" ), labelSize, TRUE, PX( 436 ), PY( 500 ), PS( 202 ), l.textDim );
			drawCombo( l, u, ROW_CASH, 0, PX( 646 ), PY( 495 ), PS( 198 ), PS( 24 ), textSize, TRUE );
			if( s_limit != nullptr )
			{
				drawTextIn( wide( "LIMIT SUPERWEAPONS" ), labelSize, TRUE, PX( 436 ), PY( 530 ), PS( 202 ), l.textDim );
				const Int x = PX( 646 ), y = PY( 525 ), w = PS( 198 ), h = PS( 24 );
				drawField( l, x, y, w, h, u, GadgetCheckBoxIsChecked( s_limit ) ? UnicodeString( L"On" ) : UnicodeString( L"Off" ), textSize,
					addHit( ACT_LIMIT, 0, 0, x, y, w, h ), FALSE, FALSE );
			}
			// the player's record against the computer, as Zero Hour's screen keeps it
			// (spaced to fit before the Reset field whatever the font's width)
			Int used = 0, shown = 0;
			for( Int i = 0; i < STAT_COUNT; ++i )
			{
				if( s_stat[i] == nullptr )
					continue;
				used += widthOf( s_statLabel[i], labelSize, TRUE ) + PS( 5 ) + widthOf( GadgetStaticTextGetText( s_stat[i] ), labelSize, TRUE );
				++shown;
			}
			const Int room = PS( 774 - 436 - 8 ), gap = shown > 1 ? max( PS( 4 ), min( PS( 16 ), ( room - used ) / ( shown - 1 ) ) ) : 0;
			Int sx = PX( 436 );
			for( Int i = 0; i < STAT_COUNT; ++i )
			{
				if( s_stat[i] == nullptr )
					continue;
				drawText( s_statLabel[i], labelSize, TRUE, sx, PY( 562 ), l.textDim );
				sx += widthOf( s_statLabel[i], labelSize, TRUE ) + PS( 5 );
				const UnicodeString value = GadgetStaticTextGetText( s_stat[i] );
				drawText( value, labelSize, TRUE, sx, PY( 562 ), l.text );
				sx += widthOf( value, labelSize, TRUE ) + gap;
			}
			if( s_buttonReset != nullptr )
			{
				const Int x = PX( 774 ), y = PY( 556 ), w = PS( 70 ), h = PS( 24 );
				drawField( l, x, y, w, h, u, UnicodeString( L"Reset" ), textSize, addHit( ACT_RESET, 0, 0, x, y, w, h ), FALSE, FALSE );
			}
			const char *hint = alone ? "This map is played alone, as a campaign's." : "Click a start position on the map to take it. Right-click it to leave it.";
			drawText( hint, textSize, FALSE, PX( 640 ) - widthOf( hint, textSize, FALSE ) / 2, PY( 652 ), l.textDim );
			{
				const Int x = PX( 34 ), y = PY( 636 ), w = PS( 220 ), h = PS( 46 );
				drawButton( l, x, y, w, h, u, "BACK", buttonSize, FALSE, addHit( ACT_BACK, 0, 0, x, y, w, h ), FALSE );
			}
			{
				const Int x = PX( 1026 ), y = PY( 636 ), w = PS( 220 ), h = PS( 46 );
				drawButton( l, x, y, w, h, u, "PLAY", buttonSize, TRUE, addHit( ACT_PLAY, 0, 0, x, y, w, h ), FALSE );
			}
		}
		else
		{
			// Zero Hour's map list, in the first players' place: the game's maps or the player's own, a row a map
			const Int x = PX( 34 ), y = PY( 82 ), w = PS( 384 ), h = PS( 498 );
			drawFrame( l, x, y, w, h, u );
			for( Int t = 0; t < 2; ++t )
			{
				if( s_mapTab[t] == nullptr )
					continue;
				const Int tx = x + PS( 10 ) + t * PS( 185 ), ty = y + PS( 12 ), tw = PS( 179 ), th = PS( 24 );
				const Bool on = BitIsSet( s_mapTab[t]->winGetInstanceData()->getState(), WIN_STATE_SELECTED );
				const Bool hover = addHit( ACT_MAP_TAB, t, 0, tx, ty, tw, th );
				TheDisplay->drawFillRect( tx, ty, tw, th, on ? l.fillOn : l.fill );
				TheDisplay->drawOpenRect( tx, ty, tw, th, on ? 2.0f : 1.0f, on || hover ? l.hi : l.accentDim );
				const char *name = t == 0 ? "THE GAME'S MAPS" : "YOUR MAPS";
				drawText( name, labelSize, TRUE, tx + ( tw - widthOf( name, labelSize, TRUE ) ) / 2, ty + PS( 5 ), on || hover ? l.text : l.textDim );
			}
			const Int rowH = max( 12, PS( 24 ) ), top = y + PS( 46 );
			const Int count = s_mapList != nullptr ? GadgetListBoxGetNumEntries( s_mapList ) : 0;
			s_mapRows = max( 1, ( y + h - PS( 10 ) - top ) / rowH );
			Int chosen = -1;
			if( s_mapList != nullptr )
				GadgetListBoxGetSelected( s_mapList, &chosen );
			if( s_mapCentre )
			{
				// (the map in use in the middle of the rows shown, when the list comes up)
				s_mapTop = chosen - s_mapRows / 2;
				s_mapCentre = FALSE;
			}
			s_mapTop = max( 0, min( s_mapTop, count - s_mapRows ) );
			const Int column = mapColumn();
			for( Int r = 0; r < s_mapRows && s_mapTop + r < count; ++r )
			{
				const Int row = s_mapTop + r, rx = x + PS( 10 ), ry = top + r * rowH, rw = w - PS( 28 );
				const Bool hover = addHit( ACT_MAP_ROW, row, 0, rx, ry, rw, rowH );
				if( row == chosen )
					TheDisplay->drawFillRect( rx, ry, rw, rowH - 1, l.fillOn );
				if( row == chosen || hover )
					TheDisplay->drawOpenRect( rx, ry, rw, rowH - 1, 1.0f, row == chosen ? l.hi : l.accentDim );
				const UnicodeString name = GadgetListBoxGetText( s_mapList, row, column );
				DisplayString *s = textOf( name, textSize, FALSE );
				drawTextIn( name, textSize, FALSE, rx + PS( 8 ), s != nullptr ? ry + ( rowH - 1 - s->getFont()->height ) / 2 : ry, rw - PS( 16 ), row == chosen || hover ? l.text : l.textDim );
			}
			if( count > s_mapRows )
			{
				// where the rows shown lie in the list
				const Int bx = x + w - PS( 14 ), bh = s_mapRows * rowH;
				TheDisplay->drawFillRect( bx, top, PS( 4 ), bh, l.fill );
				TheDisplay->drawFillRect( bx, top + bh * s_mapTop / count, PS( 4 ), max( 4, bh * s_mapRows / count ), l.accent );
			}
			const char *hint = "Click a map to see it. The mouse wheel turns the list.";
			drawText( hint, textSize, FALSE, PX( 640 ) - widthOf( hint, textSize, FALSE ) / 2, PY( 652 ), l.textDim );
			{
				const Int bx = PX( 34 ), by = PY( 636 ), bw = PS( 220 ), bh = PS( 46 );
				drawButton( l, bx, by, bw, bh, u, "BACK", buttonSize, FALSE, addHit( ACT_MAP_BACK, 0, 0, bx, by, bw, bh ), FALSE );
			}
			{
				const Int bx = PX( 1026 ), by = PY( 636 ), bw = PS( 220 ), bh = PS( 46 );
				drawButton( l, bx, by, bw, bh, u, "USE THIS MAP", buttonSize, TRUE, addHit( ACT_MAP_USE, 0, 0, bx, by, bw, bh ), chosen < 0 );
			}
		}

		// the match options' menu, over the screen: the mod's options (cash multiplier, build speed, the long queue,
		// the superweapon limit, the map revealed, everything unlocked, the Tiberium zones, the cheats), a line each as
		// the mod words it, its name left and its setting in a field right; a click on a line changes it
		if( s_options && !maps )
		{
			// (only this menu answers the mouse while it is open)
			s_hits.clear();
			s_under = FALSE;
			const Int count = AbramMatchOptionsMenu::getOptionCount();
			const Int rowH = PS( 30 ), pitch = PS( 36 );
			const Int w = PS( 520 ), h = PS( 96 ) + count * pitch + PS( 70 );
			const Int x = PX( 640 ) - w / 2, y = max( PS( 30 ), ( sh - h ) / 2 );
			TheDisplay->drawFillRect( 0, 0, sw, sh, ( l.shade & 0x00FFFFFF ) | ( 150u << 24 ) );
			drawFrame( l, x, y, w, h, u );
			TheDisplay->drawFillRect( x + 2, y + 2, w - 4, h - 4, ( l.shade & 0x00FFFFFF ) | ( 120u << 24 ) );
			drawText( "MATCH OPTIONS", nameSize, TRUE, x + PS( 20 ), y + PS( 16 ), l.text );
			TheDisplay->drawLine( x + PS( 14 ), y + PS( 44 ), x + w - PS( 14 ), y + PS( 44 ), 1.0f, l.accentDim );
			drawWrapped( "Click an option to change it. They apply to everyone in this match.", textSize, FALSE, x + PS( 20 ), y + PS( 54 ), w - PS( 40 ), l.textDim );
			for( Int i = 0; i < count; ++i )
			{
				const Int rx = x + PS( 20 ), ry = y + PS( 96 ) + i * pitch, rw = w - PS( 40 );
				const Bool hover = addHit( ACT_OPTION, i, 0, rx, ry, rw, rowH );
				// the line as the mod words it, "name: setting", taken apart at its last colon
				const UnicodeString line = AbramMatchOptionsMenu::getOptionText( i, TheSkirmishGameInfo );
				UnicodeString name = line, setting;
				const WideChar *text = line.str();
				Int colon = -1;
				for( Int c = 0; text != nullptr && text[c] != 0; ++c )
					if( text[c] == L':' && text[c + 1] == L' ' )
						colon = c;
				if( colon >= 0 )
				{
					name.clear();
					for( Int c = 0; c < colon; ++c )
						name.concat( text[c] );
					setting = UnicodeString( text + colon + 2 );
				}
				if( hover )
					TheDisplay->drawFillRect( rx - PS( 6 ), ry - PS( 2 ), rw + PS( 12 ), rowH + PS( 4 ), l.fill );
				DisplayString *ns = textOf( name, labelSize + 1, TRUE );
				drawTextIn( name, labelSize + 1, TRUE, rx, ns != nullptr ? ry + ( rowH - ns->getFont()->height ) / 2 : ry, rw - PS( 200 ), hover ? l.text : l.textDim );
				// (a number being typed: what is typed so far, with a caret)
				const Bool editing = i == s_editOption;
				if( editing )
				{
					setting.translate( AsciiString( s_editDigits.c_str() ) );
					if( ( timeGetTime() / 500 ) & 1 )
						setting.concat( L'_' );
				}
				// (a setting other than the game's own stands out: "Off" and "x1" are the game as it always was)
				const Bool plain = !editing && ( setting.compare( L"Off" ) == 0 || setting.compare( L"x1" ) == 0 || setting.compare( L"Classic" ) == 0
					|| setting.compare( L"by team size" ) == 0 );
				const Int fx = rx + rw - PS( 180 ), fw = PS( 180 );
				TheDisplay->drawFillRect( fx, ry, fw, rowH, plain ? l.fill : l.fillOn );
				TheDisplay->drawOpenRect( fx, ry, fw, rowH, plain ? 1.0f : 2.0f, hover || !plain ? l.hi : l.accentDim );
				DisplayString *ss = textOf( setting, labelSize + 1, TRUE );
				if( ss != nullptr )
					ss->draw( fx + ( fw - ss->getWidth() ) / 2, ry + ( rowH - ss->getFont()->height ) / 2, plain ? l.textDim : l.text, GameMakeColor( 0, 0, 0, 160 ) );
			}
			{
				const Int bw = PS( 200 ), bh = PS( 42 ), bx = PX( 640 ) - bw / 2, by = y + h - PS( 58 );
				drawButton( l, bx, by, bw, bh, u, "DONE", buttonSize, TRUE, addHit( ACT_OPTIONS_DONE, 0, 0, bx, by, bw, bh ), FALSE );
			}
		}

		// the open list, over everything: Zero Hour's combo box's entries, a row each
		if( s_list != nullptr && s_listW > 0 )
		{
			const Int count = GadgetComboBoxGetLength( s_list ), shown = min( count, (Int)LIST_ROWS ), rowH = max( 12, PS( 22 ) );
			s_listTop = max( 0, min( s_listTop, count - shown ) );
			const Bool colours = s_listRow != ROW_CASH && s_listField == FIELD_COLOR;
			// as wide as its field, or as its longest entry needs
			Int w = max( s_listW, PS( 110 ) );
			for( Int i = 0; i < count; ++i )
				w = max( w, widthOf( entryOf( s_list, i, nullptr ), textSize, FALSE ) + PS( 22 ) + ( colours ? rowH : 0 ) );
			w = min( w, PS( 340 ) );
			const Int h = shown * rowH + 2;
			const Int x = min( s_listX, sw - w - 2 );
			// (under its field, or over it where the screen ends first)
			const Int y = s_listY + h > sh - 2 ? s_listY - s_listH - h : s_listY;
			TheDisplay->drawFillRect( x, y, w, h, ( l.shade & 0x00FFFFFF ) | 0xFF000000 );
			TheDisplay->drawOpenRect( x, y, w, h, 1.0f, l.hi );
			const Int chosen = selectedOf( s_list );
			for( Int r = 0; r < shown; ++r )
			{
				const Int index = s_listTop + r, ry = y + 1 + r * rowH;
				Hit hit;
				hit.act.kind = ACT_CHOOSE;
				hit.act.a = index;
				hit.act.b = 0;
				hit.x = x;
				hit.y = ry;
				hit.w = w;
				hit.h = rowH;
				s_hits.push_back( hit );
				const Bool hover = s_mx >= x && s_mx < x + w && s_my >= ry && s_my < ry + rowH;
				if( hover || index == chosen )
					TheDisplay->drawFillRect( x + 1, ry, w - 2, rowH, hover ? l.fillOn : l.fill );
				Color color = 0;
				const UnicodeString name = entryOf( s_list, index, &color );
				Int tx = x + PS( 8 );
				if( colours && index > 0 )
				{
					TheDisplay->drawFillRect( tx, ry + PS( 5 ), rowH - PS( 10 ), rowH - PS( 10 ), color | 0xFF000000 );
					tx += rowH - PS( 4 );
				}
				DisplayString *s = textOf( name, textSize, FALSE );
				drawTextIn( name, textSize, FALSE, tx, s != nullptr ? ry + ( rowH - s->getFont()->height ) / 2 : ry, x + w - PS( 8 ) - tx, hover || index == chosen ? l.text : l.textDim );
			}
			if( count > shown )
			{
				TheDisplay->drawFillRect( x + w - PS( 5 ), y + 1 + ( h - 2 ) * s_listTop / count, PS( 4 ), max( 4, ( h - 2 ) * shown / count ), l.accent );
			}
		}
		#undef PX
		#undef PY
		#undef PS
	}

	//-----------------------------------------------------------------------------------------------
	/** the part at a point: the last drawn there (the open list lies over the screen) */
	Int hitAt( Int mx, Int my )
	{
		for( Int i = (Int)s_hits.size() - 1; i >= 0; --i )
			if( mx >= s_hits[i].x && mx < s_hits[i].x + s_hits[i].w && my >= s_hits[i].y && my < s_hits[i].y + s_hits[i].h )
				return i;
		return -1;
	}

	//-----------------------------------------------------------------------------------------------
	WindowMsgHandledType input( GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 )
	{
		const Int mx = (Short)( mData1 & 0xFFFF ), my = (Short)( ( mData1 >> 16 ) & 0xFFFF );
		switch( msg )
		{
			case GWM_MOUSE_POS:
				s_mx = mx;
				s_my = my;
				return MSG_HANDLED;
			case GWM_LEFT_UP:
			case GWM_LEFT_DOUBLE_CLICK:
			case GWM_RIGHT_UP:
			{
				const Int index = hitAt( mx, my );
				Act act;
				act.kind = ACT_CHOOSE;
				act.a = -1;	// (a click beside an open list shuts it and does nothing else)
				act.b = 0;
				if( index >= 0 && ( s_list == nullptr || s_hits[index].act.kind == ACT_CHOOSE ) )
					act = s_hits[index].act;
				else if( s_list == nullptr )
					return MSG_HANDLED;
				if( msg == GWM_RIGHT_UP )
				{
					// the right button: a start position left; an open list shut
					if( act.kind == ACT_START )
						act.kind = ACT_UNSTART;
					else if( s_list != nullptr )
						act.a = -1;
					else
						return MSG_HANDLED;
				}
				else if( msg == GWM_LEFT_DOUBLE_CLICK )
				{
					if( act.kind != ACT_MAP_ROW )
						return MSG_HANDLED;
					act.b = 1;
				}
				s_acts.push_back( act );
				return MSG_HANDLED;
			}
			case GWM_WHEEL_UP:
			case GWM_WHEEL_DOWN:
			{
				const Int step = msg == GWM_WHEEL_UP ? -3 : 3;
				if( s_list != nullptr )
					s_listTop = max( 0, s_listTop + step );
				else if( mapsUp() )
					s_mapTop = max( 0, s_mapTop + step );
				return MSG_HANDLED;
			}
			case GWM_CHAR:
				// Tiberium Project @feature a number typed into a match option: digits (the number row and the keypad),
				// Backspace, Enter sets it, Escape leaves it as it was
				if( s_editOption >= 0 )
				{
					const AbramMatchOptionsMenu::NumberKey did = AbramMatchOptionsMenu::typeNumberKey( s_editDigits, s_editFresh,
						(UnsignedByte)mData1, (UnsignedByte)mData2, AbramMatchOptionsMenu::getOptionNumberMax( s_editOption ) );
					if( did == AbramMatchOptionsMenu::NUMBER_KEY_SET )
						commitEdit();
					else if( did == AbramMatchOptionsMenu::NUMBER_KEY_CANCEL )
						s_editOption = -1;
					return MSG_HANDLED;
				}
				// Escape: the open list shut, else the match options' menu, else the map list left, else the screen (as
				// Zero Hour's own do it)
				if( (UnsignedByte)mData1 == KEY_ESC && BitIsSet( mData2, KEY_STATE_UP ) )
				{
					Act act;
					act.kind = s_list != nullptr ? ACT_CHOOSE : ( s_options ? ACT_OPTIONS_DONE : ( mapsUp() ? ACT_MAP_BACK : ACT_BACK ) );
					act.a = -1;
					act.b = 0;
					s_acts.push_back( act );
					return MSG_HANDLED;
				}
				break;
			default:
				break;
		}
		return MSG_HANDLED;
	}

	//-----------------------------------------------------------------------------------------------
	/** the screen's window takes the keyboard (Escape), as Zero Hour's screens' own do */
	WindowMsgHandledType system( GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 )
	{
		if( msg != GWM_INPUT_FOCUS )
			return MSG_IGNORED;
		if( mData1 == TRUE )
			*(Bool *)mData2 = TRUE;
		return MSG_HANDLED;
	}

	//-----------------------------------------------------------------------------------------------
	void hideAll( WindowLayout *layout )
	{
		if( layout == nullptr )
			return;
		for( GameWindow *win = layout->getFirstWindow(); win != nullptr; win = win->winGetNextInLayout() )
			if( !win->winIsHidden() )
				win->winHide( TRUE );
	}
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumSkirmish::isOn()
{
	return s_window != nullptr;
}

//-------------------------------------------------------------------------------------------------
void TiberiumSkirmish::init( WindowLayout *layout )
{
	s_closed = FALSE;
	// (not to be seen for a frame before the first update)
	if( TiberiumSettings::wanted() )
		hideAll( layout );
}

//-------------------------------------------------------------------------------------------------
void TiberiumSkirmish::update( WindowLayout *layout, GameWindow *parent )
{
	// (Zero Hour's screen goes on updating while it fades out for the next one: not to be drawn over again then)
	if( s_closed )
		return;
	if( !TiberiumSettings::wanted() || layout == nullptr || parent == nullptr || TheSkirmishGameInfo == nullptr )
	{
		shutdown();
		s_closed = FALSE;
		return;
	}
	if( s_window == nullptr )
	{
		// (no window "above": Zero Hour's message boxes and the match options' dialog open over this one)
		s_window = TheWindowManager->winCreate( nullptr, WIN_STATUS_ENABLED, 0, 0, TheDisplay->getWidth(), TheDisplay->getHeight(), system, nullptr );
		if( s_window == nullptr )
			return;
		s_window->winSetDrawFunc( draw );
		s_window->winSetInputFunc( input );
		s_parent = parent;
		findGadgets();
		forgetMapGadgets();
		s_list = nullptr;
		s_options = FALSE;
		s_editOption = -1;	// Tiberium Project
		s_hits.clear();
		s_acts.clear();
	}
	// Zero Hour's screen stays under this one, out of sight; its windows are hidden, not its screen (the shell goes on
	// taking it for the screen that is up). Its map list the same, while that is up.
	hideAll( layout );
	followMapList();
	hideAll( s_mapLayout );
	// Escape comes here while no box or dialog of Zero Hour's has the keyboard: its own screen would leave at once,
	// an open list or not, and its map list gives the keyboard to nothing (the misspelt name again)
	GameWindow *focus = TheWindowManager->winGetFocus();
	if( focus == nullptr || focus == s_parent )
		TheWindowManager->winSetFocus( s_window );

	// what was clicked since, done in its order (one may take the screen away: nothing after it then)
	std::vector<Act> acts;
	acts.swap( s_acts );
	for( size_t i = 0; i < acts.size() && s_window != nullptr && TheSkirmishGameInfo != nullptr; ++i )
	{
		run( acts[i] );
		// (the act may have shut Zero Hour's map list, or opened it: what is kept of it follows at once, before
		// anything is drawn from it; the acts after it were clicks on a screen that is no longer the one shown)
		if( skirmishMapSelectLayout != s_mapLayout )
		{
			followMapList();
			hideAll( s_mapLayout );
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumSkirmish::shutdown()
{
	if( s_window != nullptr && TheWindowManager != nullptr )
		TheWindowManager->winDestroy( s_window );
	s_window = nullptr;
	s_parent = nullptr;
	s_rowCount = 0;
	s_list = nullptr;
	s_options = FALSE;
	s_editOption = -1;	// Tiberium Project
	forgetMapGadgets();
	s_hits.clear();
	s_acts.clear();
	s_closed = TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumSkirmish::testClick( const char *part, Int a, Int b )
{
	if( s_window == nullptr || part == nullptr || TheSkirmishGameInfo == nullptr )
		return FALSE;
	static const char *const fieldName[FIELD_COUNT] = { "player", "faction", "team", "color" };
	Act act;
	act.a = a;
	act.b = b;
	for( Int f = 0; f < FIELD_COUNT; ++f )
	{
		if( strcmp( part, fieldName[f] ) != 0 )
			continue;
		// the row's list opened and its entry chosen, as two clicks do it
		GameWindow *combo = comboOf( a, f );
		if( combo == nullptr || !combo->winGetEnabled() || b < 0 || b >= GadgetComboBoxGetLength( combo ) )
			return FALSE;
		s_list = nullptr;
		act.kind = ACT_LIST;
		act.b = f;
		run( act );
		act.kind = ACT_CHOOSE;
		act.a = b;
		run( act );
		return TRUE;
	}
	if( strcmp( part, "cash" ) == 0 )
	{
		if( s_cash == nullptr || a < 0 || a >= GadgetComboBoxGetLength( s_cash ) )
			return FALSE;
		s_list = nullptr;
		act.kind = ACT_LIST;
		act.a = ROW_CASH;
		act.b = 0;
		run( act );
		act.kind = ACT_CHOOSE;
		act.a = a;
		run( act );
		return TRUE;
	}
	// Tiberium Project @feature (the test) keys while a number is typed into an option: "type,N" types N's digits;
	// "enter", "escape", "backspace" press that key (their codes differ between the Windows and Linux builds)
	const Bool named = strcmp( part, "enter" ) == 0 || strcmp( part, "escape" ) == 0 || strcmp( part, "backspace" ) == 0;
	if( strcmp( part, "type" ) == 0 || named )
	{
		std::string keys;
		if( !named )
		{
			static const UnsignedByte ROW[10] = { KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };
			char digits[16];
			snprintf( digits, sizeof( digits ), "%d", (int)a );
			for( const char *d = digits; *d != '\0'; ++d )
				if( *d >= '0' && *d <= '9' )
					keys += (char)ROW[*d - '0'];
		}
		else
			keys += (char)(UnsignedByte)( part[0] == 'e' ? ( part[1] == 'n' ? KEY_ENTER : KEY_ESC ) : KEY_BACKSPACE );
		for( size_t i = 0; i < keys.size(); ++i )
		{
			const UnsignedByte key = (UnsignedByte)keys[i];
			input( nullptr, GWM_CHAR, key, KEY_STATE_DOWN );
			input( nullptr, GWM_CHAR, key, KEY_STATE_UP );
		}
		return TRUE;
	}
	static const struct { const char *name; ActKind kind; } simple[] =
	{
		{ "open", ACT_LIST }, { "start", ACT_START }, { "unstart", ACT_UNSTART }, { "limit", ACT_LIMIT }, { "maps", ACT_MAPS },
		{ "options", ACT_OPTIONS }, { "option", ACT_OPTION }, { "optionsdone", ACT_OPTIONS_DONE }, { "reset", ACT_RESET }, { "play", ACT_PLAY }, { "back", ACT_BACK }, { "maprow", ACT_MAP_ROW },
		{ "maptab", ACT_MAP_TAB }, { "mapuse", ACT_MAP_USE }, { "mapback", ACT_MAP_BACK },
	};
	for( size_t i = 0; i < ARRAY_SIZE( simple ); ++i )
	{
		if( strcmp( part, simple[i].name ) != 0 )
			continue;
		act.kind = simple[i].kind;
		s_acts.push_back( act );
		return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumSkirmish::describe( char *buffer, Int size )
{
	if( buffer == nullptr || size <= 0 )
		return;
	if( s_window == nullptr || TheSkirmishGameInfo == nullptr )
	{
		snprintf( buffer, size, "off" );
		return;
	}
	std::string text;
	char part[160];
	snprintf( part, sizeof( part ), "on; %d rows; %d parts; map %s; ", (int)s_rowCount, (int)s_hits.size(), TheSkirmishGameInfo->getMap().str() );
	text += part;
	if( mapsUp() )
	{
		Int chosen = -1;
		if( s_mapList != nullptr )
			GadgetListBoxGetSelected( s_mapList, &chosen );
		const MapMetaData *md = s_mapPreview != nullptr ? (const MapMetaData *)s_mapPreview->winGetUserData() : nullptr;
		snprintf( part, sizeof( part ), "map list up: %d maps, from row %d, chosen %d (%d players); ", s_mapList != nullptr ? (int)GadgetListBoxGetNumEntries( s_mapList ) : 0,
			(int)s_mapTop, (int)chosen, md != nullptr ? (int)md->m_numPlayers : 0 );
		text += part;
	}
	if( s_list != nullptr )
	{
		snprintf( part, sizeof( part ), "list open: row %d field %d, %d entries; ", (int)s_listRow, (int)s_listField, (int)GadgetComboBoxGetLength( s_list ) );
		text += part;
	}
	snprintf( part, sizeof( part ), "cash %d; limit %d; ", (int)TheSkirmishGameInfo->getStartingCash().countMoney(), (int)TheSkirmishGameInfo->getSuperweaponRestriction() );
	text += part;
	// the mod's match options, each as its own line words it (Tiberium Project: the rules' value, and a number being typed)
	snprintf( part, sizeof( part ), "rules 0x%X; ", (unsigned int)TheSkirmishGameInfo->getAbramRules() );
	text += part;
	if( s_editOption >= 0 )
	{
		snprintf( part, sizeof( part ), "typing into option %d: '%s'; ", (int)s_editOption, s_editDigits.c_str() );
		text += part;
	}
	text += s_options ? "match options open:" : "match options:";
	for( Int i = 0; i < AbramMatchOptionsMenu::getOptionCount(); ++i )
	{
		AsciiString line;
		line.translate( AbramMatchOptionsMenu::getOptionText( i, TheSkirmishGameInfo ) );
		text += i == 0 ? " " : " | ";
		text += line.str();
	}
	text += "; players (slot:state/side/team/colour/start):";
	for( Int i = 0; i < MAX_SLOTS; ++i )
	{
		const GameSlot *slot = TheSkirmishGameInfo->getConstSlot( i );
		if( slot == nullptr || !slot->isOccupied() )
			continue;
		snprintf( part, sizeof( part ), " %d:%d/%d/%d/%d/%d", (int)( i + 1 ), (int)slot->getState(), (int)slot->getPlayerTemplate(), (int)slot->getTeamNumber(), (int)slot->getColor(),
			(int)slot->getStartPos() );
		text += part;
	}
	snprintf( buffer, size, "%s", text.c_str() );
}
