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

// FILE: TiberiumHUD.cpp //////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature the HUD as C&C3 and C&C4 together (see TiberiumHUD.h)
//
// Zero Hour's bar is not taken apart: its parent window goes below the screen (out of sight, but not
// hidden, so what the control bar does while it is up goes on: the shortcut bar, the idle worker
// count), and the windows the HUD keeps are moved into the HUD's own: the radar into the column, the
// command buttons into the panel. A command button keeps its owner, so its clicks still go to the
// control bar; and its hotkey, which only asks whether the button itself is hidden. Buttons the HUD
// does not show (the builds, which the sidebar has) go into a hidden window. When the game hides
// the control bar (a cinematic), the HUD hides too. Taken down, every window goes back.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/TiberiumHUD.h"
#include "GameClient/TiberiumZoneView.h"

#include "Common/GameUtility.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/SpecialPower.h"
#include "Common/Radar.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/Drawable.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Image.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Mouse.h"
#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/ShieldSphereUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"

#include <map>
#include <vector>

namespace
{
	enum
	{
		SLOT_COUNT = 3,		///< C&C4's ability slots
		BUTTON_COUNT = 5,
		BUTTON_ZONES = 4,		///< C&C4's zones shown over the world (a game with zones only)
		MAX_GROUPS = 14,
		MAX_ROW_LINES = 2
	};

	/// the column's buttons: Zero Hour's own (their clicks are sent as if on them)
	const char *const BUTTON_WINDOWS[BUTTON_COUNT] = { "ControlBar.wnd:ButtonOptions", "ControlBar.wnd:ButtonIdleWorker",
		"ControlBar.wnd:ButtonPlaceBeacon", "ControlBar.wnd:ButtonGeneral", nullptr };
	const char *const BUTTON_LABELS[BUTTON_COUNT] = { "Menu", "Idle", "Beacon", "General", "Zones" };

	/// the column's buttons now: the zones' only in a game with zones
	Int buttonCount()
	{
		return TiberiumZoneView::isAvailable() ? BUTTON_COUNT : BUTTON_ZONES;
	}

	/// the panel's art (TibHUD_PanelGDI and Nod), in whose pixels its parts are placed
	const Real ART_W = 470.0f;
	const Real ART_H = 145.0f;

	/// Tiberium Project @feature C&C4's bottom panels (TibHUD_RadarFrame and TibHUD_CommandBar: drawn from its
	/// TacticalHUDCommandModeBar and TacticalHUDMainCommandBar screens), in C&C4's units (its screen 768 tall)
	const Real BAR_W = 258.33f;
	const Real BAR_H = 211.33f;
	const Real BAR_REF_H = 768.0f;
	const Real SELECTION_W = 487.0f;		///< C&C4's selection panel, between the two
	/// the radar's panel (bottom left): the radar's square, its column's top square and four slots
	const Real RADAR_X = 2.0f, RADAR_Y = 12.0f, RADAR_S = 195.0f;
	const Real COLUMN_TOP_X = 203.0f, COLUMN_TOP_Y = 13.0f, COLUMN_TOP_S = 42.3f;
	const Real COLUMN_X = 203.0f, COLUMN_Y = 65.0f, COLUMN_W = 38.3f, COLUMN_H = 143.3f;

	struct HUDLayout
	{
		Int sw, sh, gap;
		Real b;																	///< C&C4's units to the screen's
		Int lx, ly, lw, lh;											///< the radar's panel (bottom left)
		Int rx, ry, rw, rh;											///< the build panel (bottom right: the sidebar's)
		Int radarX, radarY, radarS;
		Int slotX, slotY, slotW, slotH;					///< the radar's panel's column: its slots (the buttons)
		Int topX, topY, topS;										///< ... and its top square
		Real s;																	///< the panel's scale (screen pixels to its art's)
		Int px, py, pw, ph;											///< the panel
		Int cmdW, cmdH, perRow, rowX;						///< the command buttons above it
		Int tierX, tierY, tierW, tierH, tierLabelH;	///< the tier bar
	};

	/// a window of Zero Hour's bar the HUD has moved, and where it was
	struct Moved
	{
		GameWindow *win;
		GameWindow *parent;
		Int x, y, w, h;
	};

	/// one kind in the selection
	struct Group
	{
		const ThingTemplate *kind;
		Int count;
		Real health;		///< their health over their most, together
	};

	/// the faction's pictures (GDI's on America's slots, Nod's on China's)
	struct Art
	{
		const Image *panel, *bar, *slot, *tier, *tierFill, *emblem;
		const Image *radarFrame;			///< C&C4's bottom left panel
		const Image *plateOff, *plateOn, *plateHover, *grid;	///< the sidebar's (C&C4's build panel)
		const Image *rank[4];
		Color accent, accentDim, text, textDim;
	};

	Bool s_on = FALSE;
	Bool s_hidden = FALSE;
	GameWindow *s_column = nullptr;		///< the radar, the money and power, the buttons
	GameWindow *s_panel = nullptr;		///< the selection panel and the command row above it
	GameWindow *s_tier = nullptr;			///< the tier bar
	GameWindow *s_hold = nullptr;			///< hidden: the command buttons the HUD does not show
	GameWindow *s_master = nullptr;		///< Zero Hour's bar
	ICoord2D s_masterAt;
	GameWindow *s_shortcut = nullptr;	///< the support powers' bar
	ICoord2D s_shortcutAt;
	std::vector<Moved> s_moved;
	std::vector<Int> s_row;						///< the command buttons in the row above the panel, by index
	std::vector<Int> s_slots;					///< ... and in the panel's ability slots
	std::vector<Group> s_groups;
	Int s_selected = 0;
	ObjectID s_viewed = INVALID_ID;		///< the one object shown (one selected)
	Int s_hoverButton = -1;
	Int s_hoverGroup = -1;
	DisplayString *s_name = nullptr;
	DisplayString *s_text = nullptr;
	DisplayString *s_body = nullptr;
	Int s_fontHeight = 0;
	std::map<const ThingTemplate *, const CommandButton *> s_buildButtons;

	/// Tiberium Project @feature the Powers bar: a faction without General's Points (the Tiberium factions) has its
	/// powers (its buildings' or its Support Crawler's: its shortcut command set) up the left edge, each with its
	/// price and its countdown, and C&C4's support meter beside them
	struct PowerEntry
	{
		const CommandButton *button;
		const SpecialPowerTemplate *power;
		ObjectID source;
		Real ready;							///< 0..1
		UnsignedInt readyFrame;
		Bool off;								///< its source is switched off (no power, hacked, EMP)
	};
	GameWindow *s_powers = nullptr;
	std::vector<PowerEntry> s_powerList;
	UnsignedInt s_powerListFrame = 0xFFFFFFFFu;
	Int s_hoverPower = -1;
	Bool s_powersHidden = FALSE;		///< the column's Powers button hides the bar

	//-----------------------------------------------------------------------------------------------
	Player *localPlayer()
	{
		return ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
	}

	//-----------------------------------------------------------------------------------------------
	Bool isNod()
	{
		const Player *me = localPlayer();
		return me != nullptr && ( me->getSide().compareNoCase( "China" ) == 0 || me->getBaseSide().compareNoCase( "China" ) == 0 );
	}

	//-----------------------------------------------------------------------------------------------
	const Image *findImage( const char *base, const char *side )
	{
		if( TheMappedImageCollection == nullptr )
			return nullptr;
		AsciiString name;
		name.format( "%s%s", base, side );
		return TheMappedImageCollection->findImageByName( name );
	}

	//-----------------------------------------------------------------------------------------------
	void getArt( Art *a )
	{
		const Bool nod = isNod();
		const char *side = nod ? "Nod" : "GDI";
		a->panel = findImage( "TibHUD_Panel", side );
		a->bar = findImage( "TibHUD_Bar", side );
		a->slot = findImage( "TibHUD_Slot", side );
		a->tier = findImage( "TibHUD_Tier", side );
		a->tierFill = findImage( "TibHUD_TierFill", side );
		a->emblem = findImage( "TibHUD_Emblem", side );
		a->radarFrame = findImage( "TibHUD_RadarFrame", side );
		a->plateOff = findImage( "TibHUD_Plate", nod ? "NodOff" : "GDIOff" );
		a->plateOn = findImage( "TibHUD_Plate", nod ? "NodOn" : "GDIOn" );
		a->plateHover = findImage( "TibHUD_Plate", "Hover" );
		a->grid = findImage( "TibHUD_Grid", side );
		a->rank[0] = nullptr;
		for( Int i = 1; i < 4; ++i )
		{
			AsciiString base;
			base.format( "TibHUD_Rank%s", side );
			a->rank[i] = findImage( base.str(), i == 1 ? "1" : ( i == 2 ? "2" : "3" ) );
		}
		a->accent = nod ? GameMakeColor( 230, 50, 40, 255 ) : GameMakeColor( 70, 200, 245, 255 );
		a->accentDim = nod ? GameMakeColor( 110, 20, 20, 220 ) : GameMakeColor( 20, 70, 110, 220 );
		a->text = GameMakeColor( 232, 244, 250, 255 );
		a->textDim = nod ? GameMakeColor( 230, 150, 140, 255 ) : GameMakeColor( 120, 200, 235, 255 );
	}

	//-----------------------------------------------------------------------------------------------
	void computeLayout( HUDLayout *L )
	{
		const Int sw = TheDisplay->getWidth(), sh = TheDisplay->getHeight();
		L->sw = sw;
		L->sh = sh;
		// Tiberium Project @feature C&C4's layout: its screen is 768 of its units tall; the radar's panel, the
		// selection panel and the build panel side by side along the bottom must fit across
		Real b = sh / BAR_REF_H;
		if( b * ( 2.0f * BAR_W + SELECTION_W ) > sw )
			b = sw / ( 2.0f * BAR_W + SELECTION_W );
		L->b = b;
		L->gap = max( 2, (Int)( 2.0f * b ) );
		// the radar's panel, bottom left
		L->lw = (Int)( BAR_W * b );
		L->lh = (Int)( BAR_H * b );
		L->lx = 0;
		L->ly = sh - L->lh;
		L->radarX = L->lx + (Int)( RADAR_X * b );
		L->radarY = L->ly + (Int)( RADAR_Y * b );
		L->radarS = (Int)( RADAR_S * b );
		L->topX = L->lx + (Int)( COLUMN_TOP_X * b );
		L->topY = L->ly + (Int)( COLUMN_TOP_Y * b );
		L->topS = (Int)( COLUMN_TOP_S * b );
		L->slotX = L->lx + (Int)( COLUMN_X * b );
		L->slotY = L->ly + (Int)( COLUMN_Y * b );
		L->slotW = (Int)( COLUMN_W * b );
		L->slotH = (Int)( COLUMN_H * b / 4.0f );
		// the build panel, bottom right (the sidebar draws it)
		L->rw = L->lw;
		L->rh = L->lh;
		L->rx = sw - L->rw;
		L->ry = sh - L->rh;
		// the tier bar, at the bottom in the middle (C&C4's experience bar; its art is 469 by 21)
		L->tierW = (Int)( 469.0f * b );
		L->tierH = max( 8, (Int)( 21.0f * b ) );
		L->tierX = ( sw - L->tierW ) / 2;
		L->tierY = sh - L->tierH - max( 1, (Int)( 2.0f * b ) );
		L->tierLabelH = 0;
		// the selection panel above it, C&C4's size
		L->s = b;
		L->pw = (Int)( ART_W * b );
		L->ph = (Int)( ART_H * b );
		L->px = ( sw - L->pw ) / 2;
		L->py = L->tierY - L->ph - L->gap;
		// Zero Hour's command buttons above the panel, their shape kept (50 by 44 on the bar's 800 by 600)
		L->cmdH = max( 22, (Int)( 36.0f * b ) );
		L->cmdW = L->cmdH * 50 / 44;
		L->rowX = L->px + (Int)( 16.0f * b );
		L->perRow = max( 1, ( L->pw - (Int)( 32.0f * b ) ) / ( L->cmdW + L->gap ) );
	}

	//-----------------------------------------------------------------------------------------------
	/** an ability slot's rectangle (in the panel, at its right) */
	void slotRect( const HUDLayout &L, Int i, Int *x, Int *y, Int *w, Int *h )
	{
		*x = L.px + (Int)( 408.0f * L.s );
		*y = L.py + (Int)( ( 17.0f + 40.0f * i ) * L.s );
		*w = (Int)( 48.0f * L.s );
		*h = (Int)( 38.0f * L.s );
	}

	//-----------------------------------------------------------------------------------------------
	/** the i-th command button's rectangle in the row above the panel (more wrap upwards) */
	void rowRect( const HUDLayout &L, Int i, Int *x, Int *y )
	{
		const Int line = i / L.perRow, col = i % L.perRow;
		*x = L.rowX + col * ( L.cmdW + L.gap );
		*y = L.py - ( L.cmdH + L.gap ) * ( line + 1 );
	}

	//-----------------------------------------------------------------------------------------------
	void portraitRect( const HUDLayout &L, Int *x, Int *y, Int *size )
	{
		*x = L.px + (Int)( 16.0f * L.s );
		*y = L.py + (Int)( 24.0f * L.s );
		*size = (Int)( 94.0f * L.s );
	}

	//-----------------------------------------------------------------------------------------------
	/** a kind's tile (several kinds selected) */
	void groupRect( const HUDLayout &L, Int i, Int *x, Int *y, Int *size )
	{
		const Int perLine = 7;
		*size = (Int)( 50.0f * L.s );
		*x = L.px + (Int)( ( 16.0f + 54.0f * ( i % perLine ) ) * L.s );
		*y = L.py + (Int)( ( 32.0f + 54.0f * ( i / perLine ) ) * L.s );
	}

	//-----------------------------------------------------------------------------------------------
	void buttonRect( const HUDLayout &L, Int i, Int *x, Int *y, Int *w, Int *h )
	{
		// the radar's panel's column: the menu, idle worker, beacon and general's buttons in its four slots, the
		// zones' in its top square
		const Int inset = max( 1, (Int)( 3.0f * L.b ) );
		if( i >= BUTTON_ZONES )
		{
			*x = L.topX + inset;
			*y = L.topY + inset;
			*w = L.topS - 2 * inset;
			*h = L.topS - 2 * inset;
			return;
		}
		*x = L.slotX + inset;
		*y = L.slotY + i * L.slotH + inset;
		*w = L.slotW - 2 * inset;
		*h = L.slotH - 2 * inset;
	}

	//-----------------------------------------------------------------------------------------------
	void ensureStrings( const HUDLayout &L )
	{
		if( s_name != nullptr && s_fontHeight == L.sh )
			return;
		s_fontHeight = L.sh;
		if( s_name == nullptr )
		{
			s_name = TheDisplayStringManager->newDisplayString();
			s_text = TheDisplayStringManager->newDisplayString();
			s_body = TheDisplayStringManager->newDisplayString();
		}
		const Int nameSize = max( 11, (Int)( 15.0f * L.s ) ), textSize = max( 9, (Int)( 9.5f * L.s ) );
		s_name->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), nameSize, TRUE ) );
		s_text->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), textSize, TRUE ) );
		s_body->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), textSize, FALSE ) );
	}

	//-----------------------------------------------------------------------------------------------
	void drawText( DisplayString *s, const UnicodeString &text, Int x, Int y, Color color )
	{
		s->setText( text );
		s->draw( x, y, color, GameMakeColor( 0, 0, 0, 200 ) );
	}

	//-----------------------------------------------------------------------------------------------
	UnicodeString plain( const char *text )
	{
		UnicodeString u;
		u.translate( AsciiString( text ) );
		return u;
	}

	//-----------------------------------------------------------------------------------------------
	/** a thing's name: its display name, else its template's */
	UnicodeString nameOf( const ThingTemplate *t )
	{
		UnicodeString name = t->getDisplayName();
		if( name.isEmpty() )
			name.translate( t->getName() );
		return name;
	}

	//-----------------------------------------------------------------------------------------------
	/** the button that builds a kind (its description and picture) */
	const CommandButton *buildButtonOf( const ThingTemplate *t )
	{
		std::map<const ThingTemplate *, const CommandButton *>::const_iterator it = s_buildButtons.find( t );
		if( it != s_buildButtons.end() )
			return it->second;
		const CommandButton *found = nullptr;
		for( const CommandButton *b = TheControlBar->getCommandButtons(); b != nullptr && found == nullptr; b = b->getNext() )
		{
			const GUICommandType type = b->getCommandType();
			const ThingTemplate *thing = b->getThingTemplate();
			if( ( type == GUI_COMMAND_UNIT_BUILD || type == GUI_COMMAND_DOZER_CONSTRUCT ) && thing != nullptr && thing->isEquivalentTo( t ) )
				found = b;
		}
		s_buildButtons[t] = found;
		return found;
	}

	//-----------------------------------------------------------------------------------------------
	const Image *portraitOf( const ThingTemplate *t )
	{
		const Image *image = t->getSelectedPortraitImage();
		if( image == nullptr )
			image = t->getButtonImage();
		if( image == nullptr )
		{
			const CommandButton *b = buildButtonOf( t );
			image = b != nullptr ? b->getButtonImage() : nullptr;
		}
		return image;
	}

	//-----------------------------------------------------------------------------------------------
	/** what an object is, in a few words (and whose, when not the player's) */
	UnicodeString kindLine( const Object *obj )
	{
		const ThingTemplate *t = obj->getTemplate();
		const char *kind = "Unit";
		if( t->isKindOf( KINDOF_STRUCTURE ) )
			kind = "Structure";
		else if( t->isKindOf( KINDOF_HARVESTER ) )
			kind = "Harvester";
		else if( t->isKindOf( KINDOF_DOZER ) )
			kind = "Builder";
		else if( t->isKindOf( KINDOF_INFANTRY ) )
			kind = "Infantry";
		else if( t->isKindOf( KINDOF_AIRCRAFT ) )
			kind = "Aircraft";
		else if( t->isKindOf( KINDOF_VEHICLE ) )
			kind = "Vehicle";
		UnicodeString line = plain( kind );
		const Player *me = localPlayer();
		const Player *owner = obj->getControllingPlayer();
		if( me != nullptr && owner != nullptr && owner != me && obj->getTeam() != nullptr )
		{
			const Relationship r = me->getRelationship( obj->getTeam() );
			line.concat( r == ENEMIES ? L"  \x00B7  Enemy" : ( r == ALLIES ? L"  \x00B7  Ally" : L"  \x00B7  Neutral" ) );
		}
		const ContainModuleInterface *contain = obj->getContain();
		if( contain != nullptr && contain->getContainMax() > 0 && owner == me )
		{
			UnicodeString carry;
			carry.format( L"  \x00B7  %d / %d inside", (Int)contain->getContainCount(), (Int)contain->getContainMax() );
			line.concat( carry );
		}
		return line;
	}

	//-----------------------------------------------------------------------------------------------
	/** a framed bar (C&C4's): its frame, filled to frac */
	void drawBar( const Image *frame, Int x, Int y, Int w, Int h, Real frac, Color fill )
	{
		if( frac < 0.0f )
			frac = 0.0f;
		if( frac > 1.0f )
			frac = 1.0f;
		const Int inset = max( 1, h / 7 );
		if( frame != nullptr )
			TheDisplay->drawImage( frame, x, y, x + w, y + h );
		else
			TheDisplay->drawOpenRect( x, y, w, h, 1.0f, fill );
		const Int fw = (Int)( ( w - 2 * inset ) * frac );
		if( fw > 0 )
			TheDisplay->drawFillRect( x + inset, y + inset, fw, h - 2 * inset, fill );
	}

	//-----------------------------------------------------------------------------------------------
	/** a C&C4-like button: a dark plate, the faction's outline, its label */
	void drawPlate( const Art &a, Int x, Int y, Int w, Int h, const UnicodeString &label, Bool on, Bool hover, Bool lit )
	{
		if( a.plateOff != nullptr && a.plateOn != nullptr && a.plateHover != nullptr )
		{
			// C&C4's plate: lit is the bright one, the cursor's the yellow, a button Zero Hour has off dimmed
			const Image *plate = hover ? a.plateHover : ( lit ? a.plateOn : a.plateOff );
			TheDisplay->drawImage( plate, x, y, x + w, y + h, on ? 0xFFFFFFFF : GameMakeColor( 255, 255, 255, 90 ) );
		}
		else
		{
			const Color back = lit ? a.accentDim : GameMakeColor( 8, 16, 24, 220 );
			TheDisplay->drawFillRect( x, y, w, h, back );
			TheDisplay->drawOpenRect( x - 1, y - 1, w + 2, h + 2, 1.0f, GameMakeColor( 0, 0, 0, 120 ) );
			TheDisplay->drawOpenRect( x, y, w, h, hover ? 2.0f : 1.0f, on ? a.accent : GameMakeColor( 60, 70, 78, 255 ) );
		}
		s_text->setText( label );
		Int tw = 0, th = 0;
		s_text->getSize( &tw, &th );
		drawText( s_text, label, x + ( w - tw ) / 2, y + ( h - th ) / 2, on ? ( hover ? GameMakeColor( 255, 255, 255, 255 ) : a.textDim )
			: GameMakeColor( 90, 96, 102, 255 ) );
	}

	//-----------------------------------------------------------------------------------------------
	GameWindow *findWindow( const char *name )
	{
		return TheWindowManager->winGetWindowFromId( nullptr, TheNameKeyGenerator->nameToKey( name ) );
	}

	//-----------------------------------------------------------------------------------------------
	/** the column's buttons: whether Zero Hour has them up now */
	Bool powersWanted();

	Bool buttonUsable( Int i )
	{
		if( i == BUTTON_ZONES )
			return TiberiumZoneView::isAvailable();
		if( i == 3 && powersWanted() )
			return TRUE;	// (Tiberium Project: the Powers bar's button, in the general's place)
		GameWindow *win = findWindow( BUTTON_WINDOWS[i] );
		return win != nullptr && !win->winIsHidden() && win->winGetEnabled();
	}

	//-----------------------------------------------------------------------------------------------
	/** a click on one of Zero Hour's buttons, as if it were clicked */
	Bool pressButton( Int i )
	{
		if( i == BUTTON_ZONES )
		{
			if( !TiberiumZoneView::isAvailable() )
				return FALSE;
			TiberiumZoneView::toggle();
			return TRUE;
		}
		if( i == 3 && powersWanted() )
		{
			// Tiberium Project @feature a faction without General's Points: the Powers bar shown or put away
			s_powersHidden = !s_powersHidden;
			return TRUE;
		}
		GameWindow *win = findWindow( BUTTON_WINDOWS[i] );
		if( win == nullptr || win->winIsHidden() || !win->winGetEnabled() )
			return FALSE;
		TheWindowManager->winSendSystemMsg( win->winGetOwner(), GBM_SELECTED, (WindowMsgData)win, 0 );
		return TRUE;
	}

	//-----------------------------------------------------------------------------------------------
	/** the player's tier (C&C4's Tech Up upgrades: 1 to 3) and how far the next is researched (-1: not);
		a game without them: the general's rank and the way to the next */
	Int tierOf( Real *progress, Bool *isTier )
	{
		*progress = -1.0f;
		*isTier = FALSE;
		Player *me = localPlayer();
		if( me == nullptr )
			return 1;
		const UpgradeTemplate *up[2] = { TheUpgradeCenter != nullptr ? TheUpgradeCenter->findUpgrade( "C4Up_Upgrade_Tech_Up_Rank1" ) : nullptr,
			TheUpgradeCenter != nullptr ? TheUpgradeCenter->findUpgrade( "C4Up_Upgrade_Tech_Up_Rank2" ) : nullptr };
		if( up[0] != nullptr )
		{
			*isTier = TRUE;
			Int tier = 1;
			for( Int i = 0; i < 2; ++i )
				if( up[i] != nullptr && me->hasUpgradeComplete( up[i] ) )
					tier = i + 2;
			const UpgradeTemplate *next = tier < 3 ? up[tier - 1] : nullptr;
			if( next != nullptr && me->hasUpgradeInProduction( next ) )
			{
				struct Find { const UpgradeTemplate *up; Real percent;
					static void each( Object *obj, void *data )
					{
						Find *f = (Find *)data;
						ProductionUpdateInterface *pu = obj->getProductionUpdateInterface();
						for( const ProductionEntry *e = pu != nullptr ? pu->firstProduction() : nullptr; e != nullptr; e = pu->nextProduction( e ) )
							if( e->getProductionType() == PRODUCTION_UPGRADE && e->getProductionUpgrade() == f->up && e->getPercentComplete() > f->percent )
								f->percent = e->getPercentComplete();
					} };
				Find f = { next, 0.0f };
				me->iterateObjects( Find::each, &f );
				*progress = f.percent / 100.0f;
			}
			else if( tier == 3 )
				*progress = 1.0f;
			return tier;
		}
		const Int down = me->getSkillPointsLevelDown(), upTo = me->getSkillPointsLevelUp();
		*progress = upTo > down ? (Real)( me->getSkillPoints() - down ) / (Real)( upTo - down ) : 1.0f;
		return max( 1, me->getRankLevel() );
	}

	//-----------------------------------------------------------------------------------------------
	/** the selection, as kinds */
	void collectSelection()
	{
		s_groups.clear();
		s_selected = 0;
		s_viewed = INVALID_ID;
		const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
		for( DrawableList::const_iterator it = selected->begin(); it != selected->end(); ++it )
		{
			const Object *obj = ( *it )->getObject();
			if( obj == nullptr || obj->isEffectivelyDead() )
				continue;
			++s_selected;
			if( s_viewed == INVALID_ID )
				s_viewed = obj->getID();
			const BodyModuleInterface *body = obj->getBodyModule();
			const Real health = body != nullptr && body->getMaxHealth() > 0.0f ? body->getHealth() / body->getMaxHealth() : 1.0f;
			Group *g = nullptr;
			for( size_t i = 0; i < s_groups.size() && g == nullptr; ++i )
				if( s_groups[i].kind == obj->getTemplate() )
					g = &s_groups[i];
			if( g == nullptr )
			{
				if( (Int)s_groups.size() >= MAX_GROUPS )
					continue;
				Group ng = { obj->getTemplate(), 0, 0.0f };
				s_groups.push_back( ng );
				g = &s_groups.back();
			}
			++g->count;
			g->health += health;
		}
		if( s_selected != 1 )
			s_viewed = INVALID_ID;
	}

	//-----------------------------------------------------------------------------------------------
	/** a click on a kind's tile: only that kind stays selected (a right click: all but it) */
	void keepKind( Int g, Bool drop )
	{
		if( g < 0 || g >= (Int)s_groups.size() )
			return;
		const ThingTemplate *kind = s_groups[g].kind;
		std::vector<Drawable *> keep;
		const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
		for( DrawableList::const_iterator it = selected->begin(); it != selected->end(); ++it )
		{
			const Object *obj = ( *it )->getObject();
			if( obj != nullptr && ( obj->getTemplate() == kind ) != drop )
				keep.push_back( *it );
		}
		if( keep.empty() )
			return;
		TheInGameUI->deselectAllDrawables();
		GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_CREATE_SELECTED_GROUP_NO_SOUND );
		msg->appendBooleanArgument( TRUE );
		for( size_t i = 0; i < keep.size(); ++i )
		{
			TheInGameUI->selectDrawable( keep[i] );
			msg->appendObjectIDArgument( keep[i]->getObject()->getID() );
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** the command buttons Zero Hour shows now, sorted: the abilities to the slots, the commands to the
		row; the builds (the sidebar's) and anything while the bar shows no commands are held hidden */
	void sortCommands()
	{
		s_row.clear();
		s_slots.clear();
		static const NameKeyType commandWindowID = NAMEKEY( "ControlBar.wnd:CommandWindow" );
		GameWindow *commands = TheWindowManager->winGetWindowFromId( nullptr, commandWindowID );
		if( commands == nullptr || commands->winIsHidden() )
			return;
		Object *viewed = s_viewed != INVALID_ID ? TheGameLogic->findObjectByID( s_viewed ) : nullptr;
		const Bool producer = viewed != nullptr && viewed->getProductionUpdateInterface() != nullptr;
		std::vector<Int> extra;
		for( Int i = 0; i < MAX_COMMANDS_PER_SET; ++i )
		{
			GameWindow *win = TheControlBar->getCommandWindow( i );
			const CommandButton *b = win != nullptr ? (const CommandButton *)GadgetButtonGetData( win ) : nullptr;
			if( win == nullptr || b == nullptr || win->winIsHidden() )
				continue;
			switch( b->getCommandType() )
			{
				case GUI_COMMAND_UNIT_BUILD:
				case GUI_COMMAND_DOZER_CONSTRUCT:
				case GUI_COMMAND_COMMAND_SET_PAGE:
					break;	// the sidebar's
				case GUI_COMMAND_PLAYER_UPGRADE:
					if( !producer )
						s_row.push_back( i );
					break;	// (a producer's: the sidebar's Tech tab)
				case GUI_COMMAND_SPECIAL_POWER:
				case GUI_COMMAND_SPECIAL_POWER_CONSTRUCT:
				case GUI_COMMAND_FIRE_WEAPON:
				case GUI_COMMAND_SWITCH_WEAPON:
				case GUI_COMMAND_HACK_INTERNET:
				case GUI_COMMAND_TOGGLE_OVERCHARGE:
				case GUI_COMMAND_COMBATDROP:
				case GUICOMMANDMODE_HIJACK_VEHICLE:
				case GUICOMMANDMODE_CONVERT_TO_CARBOMB:
				case GUICOMMANDMODE_SABOTAGE_BUILDING:
					if( (Int)s_slots.size() < SLOT_COUNT )
						s_slots.push_back( i );
					else
						extra.push_back( i );
					break;
				default:
					s_row.push_back( i );
					break;
			}
		}
		s_row.insert( s_row.end(), extra.begin(), extra.end() );
		if( (Int)s_row.size() > MAX_COMMANDS_PER_SET )
			s_row.resize( MAX_COMMANDS_PER_SET );
	}

	//-----------------------------------------------------------------------------------------------
	/** takes one of Zero Hour's windows into the HUD (the first time, notes where it was) */
	void take( GameWindow *win, GameWindow *parent )
	{
		if( win == nullptr || parent == nullptr )
			return;
		Bool known = FALSE;
		for( size_t i = 0; i < s_moved.size() && !known; ++i )
			known = s_moved[i].win == win;
		if( !known )
		{
			Moved m;
			m.win = win;
			m.parent = win->winGetParent();
			win->winGetPosition( &m.x, &m.y );
			win->winGetSize( &m.w, &m.h );
			s_moved.push_back( m );
		}
		if( win->winGetParent() != parent )
			win->winSetParent( parent );
	}

	//-----------------------------------------------------------------------------------------------
	/** puts a taken window at a screen rectangle (in its new parent) */
	void place( GameWindow *win, GameWindow *parent, Int x, Int y, Int w, Int h )
	{
		take( win, parent );
		if( win == nullptr || parent == nullptr )
			return;
		Int ox = 0, oy = 0, cx = 0, cy = 0, cw = 0, ch = 0;
		parent->winGetScreenPosition( &ox, &oy );
		win->winGetPosition( &cx, &cy );
		win->winGetSize( &cw, &ch );
		if( cx != x - ox || cy != y - oy )
			win->winSetPosition( x - ox, y - oy );
		if( cw != w || ch != h )
			win->winSetSize( w, h );
	}

	//-----------------------------------------------------------------------------------------------
	/** the windows taken back where they were */
	void giveBack()
	{
		for( size_t i = s_moved.size(); i-- > 0; )
		{
			Moved &m = s_moved[i];
			if( m.win->winGetParent() != m.parent )
				m.win->winSetParent( m.parent );
			m.win->winSetPosition( m.x, m.y );
			m.win->winSetSize( m.w, m.h );
		}
		s_moved.clear();
		if( s_master != nullptr )
		{
			// the bar back where it was, and the view above it again
			s_master->winSetPosition( s_masterAt.x, s_masterAt.y );
			if( !s_master->winIsHidden() && TheControlBar != nullptr && TheTacticalView != nullptr && TheGlobalData != nullptr )
				TheControlBar->setScaledViewportHeight();
		}
		s_master = nullptr;
		if( s_shortcut != nullptr )
			s_shortcut->winSetPosition( s_shortcutAt.x, s_shortcutAt.y );
		s_shortcut = nullptr;
	}

	//-----------------------------------------------------------------------------------------------
	/** Tiberium Project @feature whether the local player's faction has the Powers bar (no General's Points) */
	Bool powersWanted()
	{
		const Player *me = localPlayer();
		const PlayerTemplate *pt = me != nullptr ? me->getPlayerTemplate() : nullptr;
		return pt != nullptr && !pt->hasGeneralsPoints() && pt->getSpecialPowerShortcutCommandSet().isNotEmpty();
	}

	//-----------------------------------------------------------------------------------------------
	/** the powers the player has now (a source for each), once a logic frame */
	void collectPowers()
	{
		const UnsignedInt now = TheGameLogic->getFrame();
		if( now == s_powerListFrame )
			return;
		s_powerListFrame = now;
		s_powerList.clear();
		Player *me = localPlayer();
		const PlayerTemplate *pt = me != nullptr ? me->getPlayerTemplate() : nullptr;
		const CommandSet *set = pt != nullptr ? TheControlBar->findCommandSet( pt->getSpecialPowerShortcutCommandSet() ) : nullptr;
		for( Int i = 0; set != nullptr && i < MAX_COMMANDS_PER_SET; ++i )
		{
			const CommandButton *b = set->getCommandButton( i );
			const SpecialPowerTemplate *power = b != nullptr ? b->getSpecialPowerTemplate() : nullptr;
			if( power == nullptr )
				continue;
			Object *source = me->findMostReadyShortcutSpecialPowerOfType( power->getSpecialPowerType() );
			SpecialPowerModuleInterface *mod = source != nullptr ? source->getSpecialPowerModule( power ) : nullptr;
			if( mod == nullptr )
				continue;
			PowerEntry e;
			e.button = b;
			e.power = power;
			e.source = source->getID();
			e.ready = mod->getPercentReady();
			e.readyFrame = mod->getReadyFrame();
			e.off = source->isDisabled();
			s_powerList.push_back( e );
		}
	}

	/// where the Powers bar's buttons go: up the left edge from above the radar's panel, two columns when
	/// one is too tall; the support meter beside them
	struct PowersLayout
	{
		Int x, bottom, size, gap, perCol, cols;
		Int meterX, meterY, meterW, meterH;
		Int x0, y0, w, h;			///< its window
	};

	void powersLayout( const HUDLayout &L, Int count, Bool meter, PowersLayout *P )
	{
		P->gap = L.gap;
		P->size = (Int)( 38.0f * L.b );
		const Int top = (Int)( 48.0f * L.b );
		P->bottom = L.ly - L.gap * 3;
		const Int room = max( 1, P->bottom - top );
		P->perCol = max( 1, ( room + P->gap ) / ( P->size + P->gap ) );
		if( count > P->perCol )
		{
			// smaller first, then a second column
			const Int smaller = max( (Int)( 30.0f * L.b ), ( room + P->gap ) / max( 1, count ) - P->gap );
			P->size = min( P->size, smaller );
			P->perCol = max( 1, ( room + P->gap ) / ( P->size + P->gap ) );
		}
		P->cols = max( 1, ( count + P->perCol - 1 ) / P->perCol );
		P->x = max( 4, (Int)( 6.0f * L.b ) );
		P->meterW = meter ? max( 6, (Int)( 11.0f * L.b ) ) : 0;
		P->meterH = meter ? min( room, (Int)( 190.0f * L.b ) ) : 0;
		P->meterX = P->x + P->cols * ( P->size + P->gap ) + P->gap;
		P->meterY = P->bottom - P->meterH;
		const Int rows = min( max( count, 1 ), P->perCol );
		P->x0 = 0;
		P->y0 = min( P->bottom - rows * ( P->size + P->gap ), P->meterY ) - (Int)( 16.0f * L.b );
		P->w = ( meter ? P->meterX + P->meterW : P->x + P->cols * ( P->size + P->gap ) ) + P->gap * 2;
		P->h = P->bottom - P->y0 + P->gap;
	}

	void powerRect( const PowersLayout &P, Int i, Int *x, Int *y )
	{
		const Int col = i / P.perCol, row = i % P.perCol;
		*x = P.x + col * ( P.size + P.gap );
		*y = P.bottom - ( row + 1 ) * ( P.size + P.gap ) + P.gap;
	}

	UnicodeString clockText( UnsignedInt frames )
	{
		const Int secs = (Int)( ( frames + LOGICFRAMES_PER_SECOND - 1 ) / LOGICFRAMES_PER_SECOND );
		UnicodeString s;
		s.format( L"%d:%02d", secs / 60, secs % 60 );
		return s;
	}

	//-----------------------------------------------------------------------------------------------
	/** the Powers bar: each power's picture, dark over the part still charging with its countdown, its price
		(red when the player has not got it), lit when it can be used; the support meter with its five ranks */
	void drawPowers( GameWindow *, WinInstanceData * )
	{
		Player *me = localPlayer();
		if( me == nullptr || TheDisplay == nullptr || s_powersHidden )
			return;
		HUDLayout L;
		computeLayout( &L );
		ensureStrings( L );
		Art a;
		getArt( &a );
		collectPowers();
		const Bool meter = me->hasSupportMeter();
		PowersLayout P;
		powersLayout( L, (Int)s_powerList.size(), meter, &P );
		const UnsignedInt now = TheGameLogic->getFrame();
		for( size_t i = 0; i < s_powerList.size(); ++i )
		{
			const PowerEntry &e = s_powerList[i];
			Int x, y;
			powerRect( P, (Int)i, &x, &y );
			const Bool ready = e.ready >= 1.0f && !e.off;
			const Bool afford = me->canAffordSpecialPower( e.power );
			const Bool hover = (Int)i == s_hoverPower;
			TheDisplay->drawFillRect( x, y, P.size, P.size, GameMakeColor( 2, 8, 14, 200 ) );
			const Image *img = e.button->getButtonImage();
			if( img != nullptr )
				TheDisplay->drawImage( img, x + 1, y + 1, x + P.size - 1, y + P.size - 1,
					ready && afford ? 0xFFFFFFFF : GameMakeColor( 150, 150, 150, 255 ) );
			if( e.off )
			{
				// its source is off (a building without power): dark, and said so
				TheDisplay->drawFillRect( x + 1, y + 1, P.size - 2, P.size - 2, GameMakeColor( 40, 0, 0, 170 ) );
				const UnicodeString off = plain( "Off" );
				s_text->setText( off );
				Int tw = 0, th = 0;
				s_text->getSize( &tw, &th );
				drawText( s_text, off, x + ( P.size - tw ) / 2, y + ( P.size - th ) / 2, GameMakeColor( 255, 90, 70, 255 ) );
			}
			else if( !ready )
			{
				// the part still to charge, dark from the top, and the time left
				const Int dark = (Int)( ( P.size - 2 ) * ( 1.0f - max( 0.0f, e.ready ) ) );
				if( dark > 0 )
					TheDisplay->drawFillRect( x + 1, y + 1, P.size - 2, dark, GameMakeColor( 0, 0, 0, 170 ) );
				if( e.readyFrame > now )
				{
					const UnicodeString clock = clockText( e.readyFrame - now );
					s_text->setText( clock );
					Int tw = 0, th = 0;
					s_text->getSize( &tw, &th );
					drawText( s_text, clock, x + ( P.size - tw ) / 2, y + ( P.size - th ) / 2, a.text );
				}
			}
			// the price: credits at the bottom, the support points (C&C4's rank) at the top
			if( e.power->getSupportPoints() > 0 )
			{
				UnicodeString pts;
				pts.format( L"%u", (unsigned)e.power->getSupportPoints() );
				drawText( s_text, pts, x + 2, y + 1, me->getSupportPoints() >= (Int)e.power->getSupportPoints() ? a.accent : GameMakeColor( 255, 70, 60, 255 ) );
			}
			UnicodeString price;
			if( e.power->getCost() > 0 )
				price.format( L"$%u", (unsigned)e.power->getCost() );
			if( !price.isEmpty() )
			{
				s_text->setText( price );
				Int tw = 0, th = 0;
				s_text->getSize( &tw, &th );
				const Bool money = me->getMoney()->countMoney() >= e.power->getCost();
				drawText( s_text, price, x + P.size - tw - 2, y + P.size - th, money ? a.text : GameMakeColor( 255, 70, 60, 255 ) );
			}
			const Color edge = hover ? GameMakeColor( 255, 220, 90, 255 ) : ( ready && afford ? a.accent : GameMakeColor( 60, 70, 78, 255 ) );
			TheDisplay->drawOpenRect( x, y, P.size, P.size, hover ? 2.0f : 1.0f, edge );
		}

		// C&C4's support meter: its points, a notch a rank (10 points), lit up to what it holds
		if( meter )
		{
			const Int pts = me->getSupportPoints(), most = max( 1, me->getSupportPointsMax() );
			TheDisplay->drawFillRect( P.meterX, P.meterY, P.meterW, P.meterH, GameMakeColor( 2, 8, 14, 210 ) );
			const Int fill = (Int)( ( P.meterH - 2 ) * min( 1.0f, (Real)pts / most ) );
			if( fill > 0 )
				TheDisplay->drawFillRect( P.meterX + 1, P.meterY + P.meterH - 1 - fill, P.meterW - 2, fill, a.accent );
			for( Int k = 1; k * 10 < most; ++k )
			{
				const Int ny = P.meterY + P.meterH - 1 - (Int)( ( P.meterH - 2 ) * ( 10.0f * k / most ) );
				TheDisplay->drawFillRect( P.meterX, ny, P.meterW, max( 1, P.gap / 2 ), GameMakeColor( 0, 0, 0, 230 ) );
			}
			TheDisplay->drawOpenRect( P.meterX, P.meterY, P.meterW, P.meterH, 1.0f, a.accentDim );
			UnicodeString label;
			label.format( L"%d", pts );
			s_text->setText( label );
			Int tw = 0, th = 0;
			s_text->getSize( &tw, &th );
			drawText( s_text, label, P.meterX + ( P.meterW - tw ) / 2, P.meterY - th - 1, a.text );
		}

		// the hovered power: its name, price and cooldown, and what it does, beside the bar
		if( s_hoverPower >= 0 && s_hoverPower < (Int)s_powerList.size() )
		{
			const PowerEntry &e = s_powerList[s_hoverPower];
			Int x, y;
			powerRect( P, s_hoverPower, &x, &y );
			UnicodeString name = TheGameText->fetch( e.button->getTextLabel() );
			UnicodeString line;
			const Int secs = (Int)( e.power->getReloadTime() / LOGICFRAMES_PER_SECOND );
			if( e.power->getCost() > 0 && e.power->getSupportPoints() > 0 )
				line.format( L"$%u and %u support points, ready again after %d:%02d", (unsigned)e.power->getCost(),
					(unsigned)e.power->getSupportPoints(), secs / 60, secs % 60 );
			else if( e.power->getCost() > 0 )
				line.format( L"$%u, ready again after %d:%02d", (unsigned)e.power->getCost(), secs / 60, secs % 60 );
			else
				line.format( L"Ready again after %d:%02d", secs / 60, secs % 60 );
			UnicodeString desc = e.button->getDescriptionLabel().isNotEmpty() ? TheGameText->fetch( e.button->getDescriptionLabel() ) : UnicodeString();
			const Int boxW = (Int)( 260.0f * L.b ), pad = max( 3, (Int)( 5.0f * L.b ) );
			s_body->setWordWrap( boxW - 2 * pad );
			Int nw = 0, nh = 0, lw = 0, lh = 0, dw = 0, dh = 0;
			s_name->setText( name );
			s_name->getSize( &nw, &nh );
			s_text->setText( line );
			s_text->getSize( &lw, &lh );
			if( !desc.isEmpty() )
			{
				s_body->setText( desc );
				s_body->getSize( &dw, &dh );
			}
			const Int boxH = pad * 3 + nh + lh + dh;
			const Int bx = P.w + P.gap, by = max( 0, min( y + P.size / 2 - boxH / 2, L.sh - boxH ) );
			TheDisplay->drawFillRect( bx, by, boxW, boxH, GameMakeColor( 2, 8, 14, 230 ) );
			TheDisplay->drawOpenRect( bx, by, boxW, boxH, 1.0f, a.accent );
			drawText( s_name, name, bx + pad, by + pad, a.text );
			drawText( s_text, line, bx + pad, by + pad + nh, me->canAffordSpecialPower( e.power ) ? a.textDim : GameMakeColor( 255, 70, 60, 255 ) );
			if( !desc.isEmpty() )
				drawText( s_body, desc, bx + pad, by + pad * 2 + nh + lh, a.textDim );
			// Tiberium Project @bugfix the panel wraps its own lines with this string: a wrap left on folded a long
			// line in two over the next one (a description of three lines at 2560 by 1440)
			s_body->setWordWrap( 0 );
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** a power's button pressed: refused when it cannot be paid for or is not ready; aimed (its targeting)
		or used at once */
	void pressPower( Int i )
	{
		if( i < 0 || i >= (Int)s_powerList.size() )
			return;
		const PowerEntry &e = s_powerList[i];
		if( ControlBar::refuseUnaffordablePower( e.button ) )
			return;
		Object *source = TheGameLogic->findObjectByID( e.source );
		SpecialPowerModuleInterface *mod = source != nullptr ? source->getSpecialPowerModule( e.power ) : nullptr;
		if( mod == nullptr || !mod->isReady() || source->isDisabled() )
			return;
		if( BitIsSet( e.button->getOptions(), COMMAND_OPTION_NEED_TARGET ) )
		{
			// (a power used in steps starts with its first: the Decoy Army's choice of units)
			const CommandButton *lead = e.button->getLeadCommand();
			TheInGameUI->setGUICommand( lead != nullptr ? lead : e.button );
			return;
		}
		GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_SPECIAL_POWER );
		msg->appendIntegerArgument( e.power->getID() );
		msg->appendIntegerArgument( e.button->getOptions() );
		msg->appendObjectIDArgument( source->getID() );
	}

	//-----------------------------------------------------------------------------------------------
	WindowMsgHandledType inputPowers( GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData )
	{
		const Int mx = (Short)( mData1 & 0xFFFF ), my = (Short)( ( mData1 >> 16 ) & 0xFFFF );
		Player *me = localPlayer();
		if( me == nullptr || s_powersHidden )
			return MSG_IGNORED;
		HUDLayout L;
		computeLayout( &L );
		PowersLayout P;
		powersLayout( L, (Int)s_powerList.size(), me->hasSupportMeter(), &P );
		Int hit = -1;
		for( Int i = 0; i < (Int)s_powerList.size() && hit < 0; ++i )
		{
			Int x, y;
			powerRect( P, i, &x, &y );
			if( mx >= x && mx < x + P.size && my >= y && my < y + P.size )
				hit = i;
		}
		switch( msg )
		{
			case GWM_MOUSE_POS:
				s_hoverPower = hit;
				return hit >= 0 ? MSG_HANDLED : MSG_IGNORED;
			case GWM_MOUSE_LEAVING:
				s_hoverPower = -1;
				return MSG_HANDLED;
			case GWM_LEFT_UP:
				if( hit < 0 )
					return MSG_IGNORED;
				pressPower( hit );
				return MSG_HANDLED;
			case GWM_LEFT_DOWN:
			case GWM_RIGHT_DOWN:
			case GWM_RIGHT_UP:
				return hit >= 0 ? MSG_HANDLED : MSG_IGNORED;
			default:
				return MSG_IGNORED;
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** the column: the radar's frame and its power meter, the money and power, the buttons */
	void drawColumn( GameWindow *, WinInstanceData * )
	{
		Player *me = localPlayer();
		if( me == nullptr || TheDisplay == nullptr )
			return;
		HUDLayout L;
		computeLayout( &L );
		ensureStrings( L );
		Art a;
		getArt( &a );
		// Tiberium Project @feature C&C4's bottom left panel: the radar in its square, the buttons in its column
		if( a.radarFrame != nullptr )
			TheDisplay->drawImage( a.radarFrame, L.lx, L.ly, L.lx + L.lw, L.ly + L.lh );
		else
		{
			TheDisplay->drawFillRect( L.lx, L.ly, L.lw, L.lh, GameMakeColor( 6, 11, 16, 225 ) );
			TheDisplay->drawOpenRect( L.radarX, L.radarY, L.radarS, L.radarS, 1.0f, a.accent );
		}
		// without a radar, the faction's emblem where it would be
		static const NameKeyType radarID = NAMEKEY( "ControlBar.wnd:LeftHUD" );
		GameWindow *radar = TheWindowManager->winGetWindowFromId( nullptr, radarID );
		if( radar != nullptr && a.emblem != nullptr && TheRadar != nullptr && !rts::localPlayerHasRadar() )
		{
			const Int e = L.radarS * 3 / 4;
			TheDisplay->drawImage( a.emblem, L.radarX + ( L.radarS - e ) / 2, L.radarY + ( L.radarS - e ) / 2,
				L.radarX + ( L.radarS + e ) / 2, L.radarY + ( L.radarS + e ) / 2, GameMakeColor( 255, 255, 255, 110 ) );
		}
		// without the zones' button, the faction's emblem in the column's top square
		if( buttonCount() <= BUTTON_ZONES && a.emblem != nullptr )
		{
			const Int in = L.topS / 6;
			TheDisplay->drawImage( a.emblem, L.topX + in, L.topY + in, L.topX + L.topS - in, L.topY + L.topS - in,
				GameMakeColor( 255, 255, 255, 150 ) );
		}

		// the buttons, in the column's slots (C&C4's art has their frames): their names; lit, hovered
		for( Int i = 0; i < buttonCount(); ++i )
		{
			Int x, y, w, h;
			buttonRect( L, i, &x, &y, &w, &h );
			const Bool usable = buttonUsable( i );
			// the general's: lit while there is a promotion to spend; the zones': lit while they are shown; the
			// Powers bar's (in the general's place for a faction without General's Points): lit while it is up
			Bool lit = i == 3 && me->getSciencePurchasePoints() > 0 && ( ( TheGameLogic->getFrame() / 15 ) & 1 );
			if( i == BUTTON_ZONES )
				lit = TiberiumZoneView::isShown();
			if( i == 3 && powersWanted() )
				lit = !s_powersHidden;
			if( lit )
				TheDisplay->drawFillRect( x, y, w, h, a.accentDim );
			if( usable && i == s_hoverButton )
				TheDisplay->drawOpenRect( x, y, w, h, 2.0f, a.accent );
			const UnicodeString label = plain( i == 3 && powersWanted() ? "Powers" : BUTTON_LABELS[i] );
			s_text->setText( label );
			Int tw = 0, th = 0;
			s_text->getSize( &tw, &th );
			drawText( s_text, label, x + ( w - tw ) / 2, y + ( h - th ) / 2, usable ? ( lit || i == s_hoverButton ? a.text : a.textDim )
				: GameMakeColor( 110, 116, 122, 200 ) );
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** the selection panel: C&C4's, with what is selected; the ability slots' frames */
	void drawPanel( GameWindow *, WinInstanceData * )
	{
		Player *me = localPlayer();
		if( me == nullptr || TheDisplay == nullptr )
			return;
		HUDLayout L;
		computeLayout( &L );
		ensureStrings( L );
		Art a;
		getArt( &a );
		const Real s = L.s;
		if( a.panel != nullptr )
			TheDisplay->drawImage( a.panel, L.px, L.py, L.px + L.pw, L.py + L.ph );
		for( Int i = 0; i < SLOT_COUNT; ++i )
		{
			Int x, y, w, h;
			slotRect( L, i, &x, &y, &w, &h );
			TheDisplay->drawFillRect( x + 2, y + 2, w - 4, h - 4, GameMakeColor( 2, 8, 14, 170 ) );
			if( a.slot != nullptr )
				TheDisplay->drawImage( a.slot, x, y, x + w, y + h );
			TheDisplay->drawOpenRect( x + 1, y + 1, w - 2, h - 2, 1.0f, i < (Int)s_slots.size() ? a.accent : a.accentDim );
		}
		// the row's backing: a strip behind Zero Hour's buttons
		if( !s_row.empty() )
		{
			const Int lines = ( (Int)s_row.size() + L.perRow - 1 ) / L.perRow;
			const Int count = min( (Int)s_row.size(), L.perRow );
			const Int top = L.py - ( L.cmdH + L.gap ) * lines - L.gap;
			TheDisplay->drawFillRect( L.rowX - L.gap, top, count * ( L.cmdW + L.gap ) + L.gap, L.py - top, GameMakeColor( 4, 10, 16, 170 ) );
			TheDisplay->drawOpenRect( L.rowX - L.gap, top, count * ( L.cmdW + L.gap ) + L.gap, L.py - top, 1.0f, a.accentDim );
		}

		const Int textX = L.px + (Int)( 120.0f * s ), textRight = L.px + (Int)( 400.0f * s );
		Int px, py, ps;
		portraitRect( L, &px, &py, &ps );
		const Object *obj = s_viewed != INVALID_ID ? TheGameLogic->findObjectByID( s_viewed ) : nullptr;
		if( obj != nullptr )
		{
			// one selected: its portrait, name, kind, health, shield, rank and what it is
			const ThingTemplate *t = obj->getTemplate();
			const Image *portrait = portraitOf( t );
			TheDisplay->drawFillRect( px, py, ps, ps, GameMakeColor( 2, 8, 14, 200 ) );
			if( portrait != nullptr )
				TheDisplay->drawImage( portrait, px + 2, py + 2, px + ps - 2, py + ps - 2 );
			TheDisplay->drawOpenRect( px, py, ps, ps, 1.0f, a.accent );
			drawText( s_name, nameOf( t ), textX, L.py + (Int)( 14.0f * s ), a.text );
			drawText( s_body, kindLine( obj ), textX, L.py + (Int)( 36.0f * s ), a.textDim );

			const Int barW = (Int)( 200.0f * s ), barH = max( 6, (Int)( 12.0f * s ) );
			const Int labelX = textX + barW + (Int)( 6.0f * s );
			const BodyModuleInterface *body = obj->getBodyModule();
			const Real hp = body != nullptr ? body->getHealth() : 0.0f, hpMost = body != nullptr ? body->getMaxHealth() : 0.0f;
			const Real frac = hpMost > 0.0f ? hp / hpMost : 0.0f;
			const Color hpColor = frac > 0.5f ? GameMakeColor( 80, 220, 110, 255 ) : ( frac > 0.25f ? GameMakeColor( 240, 200, 60, 255 ) : GameMakeColor( 230, 60, 50, 255 ) );
			Int y = L.py + (Int)( 56.0f * s );
			drawBar( a.bar, textX, y, barW, barH, frac, hpColor );
			UnicodeString line;
			line.format( L"%d / %d", (Int)( hp + 0.5f ), (Int)( hpMost + 0.5f ) );
			drawText( s_text, line, labelX, y + ( barH - s_text->getFont()->height ) / 2, a.text );

			Real left = 0.0f, full = 0.0f;
			Bool up = FALSE;
			y += (Int)( 18.0f * s );
			if( ShieldSphereUpdate::getShieldOf( obj, &left, &full, &up ) )
			{
				drawBar( a.bar, textX, y, barW, barH, up ? left / full : 0.0f, GameMakeColor( 80, 170, 255, 255 ) );
				if( up )
					line.format( L"Shield %d", (Int)( left + 0.5f ) );
				else
					line = plain( "Shield down" );
				drawText( s_text, line, labelX, y + ( barH - s_text->getFont()->height ) / 2, up ? a.text : a.textDim );
				y += (Int)( 18.0f * s );
			}

			// the rank, or the building of it, or what it is building
			Int x = textX;
			Bool rankLine = FALSE;
			const VeterancyLevel rank = obj->getVeterancyLevel();
			if( obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
			{
				line.format( L"Under construction  %d%%", (Int)obj->getConstructionPercent() );
				drawText( s_text, line, x, y, GameMakeColor( 255, 214, 90, 255 ) );
				rankLine = TRUE;
			}
			else
			{
				if( rank >= LEVEL_VETERAN && rank <= LEVEL_HEROIC && a.rank[rank] != nullptr )
				{
					const Int ih = (Int)( 18.0f * s );
					const Int iw = ih * a.rank[rank]->getImageWidth() / max( 1, a.rank[rank]->getImageHeight() );
					TheDisplay->drawImage( a.rank[rank], x, y, x + iw, y + ih );
					x += iw + (Int)( 5.0f * s );
					drawText( s_text, plain( rank == LEVEL_VETERAN ? "Veteran" : ( rank == LEVEL_ELITE ? "Elite" : "Heroic" ) ), x, y + ( ih - s_text->getFont()->height ) / 2,
						GameMakeColor( 255, 214, 90, 255 ) );
					x += s_text->getWidth() + (Int)( 14.0f * s );
					rankLine = TRUE;
				}
				ProductionUpdateInterface *pu = obj->getControllingPlayer() == me ? const_cast<Object *>( obj )->getProductionUpdateInterface() : nullptr;
				const ProductionEntry *e = pu != nullptr ? pu->firstProduction() : nullptr;
				if( e != nullptr )
				{
					// (the entry's unit and upgrade share their place: its type says which it has)
					UnicodeString what;
					if( e->getProductionType() == PRODUCTION_UNIT && e->getProductionObject() != nullptr )
						what = nameOf( e->getProductionObject() );
					else if( e->getProductionType() == PRODUCTION_UPGRADE && e->getProductionUpgrade() != nullptr )
						what = TheGameText->fetch( e->getProductionUpgrade()->getDisplayNameLabel() );
					line.format( L"Building %s  %d%%", what.str(), (Int)e->getPercentComplete() );
					if( pu->getProductionCount() > 1 )
					{
						UnicodeString more;
						more.format( L"  (+%d)", (Int)pu->getProductionCount() - 1 );
						line.concat( more );
					}
					drawText( s_text, line, x, y, a.textDim );
					rankLine = TRUE;
				}
			}
			if( rankLine )
				y += (Int)( 20.0f * s );

			// what it is (its build button's description), in the lines left (words wrapped, the rest cut)
			const CommandButton *b = buildButtonOf( t );
			if( b != nullptr && b->getDescriptionLabel().isNotEmpty() )
			{
				const UnicodeString text = TheGameText->fetch( b->getDescriptionLabel() );
				s_body->setWordWrap( 0 );	// (its own lines: the string must not fold them again)
				const Int lineH = s_body->getFont() != nullptr ? s_body->getFont()->height + 1 : 12;
				const Int bottom = L.py + (Int)( 138.0f * s );
				const Int width = textRight - textX;
				UnicodeString lineText, word;
				const WideChar *c = text.str();
				Bool more = TRUE;
				while( more && y + lineH <= bottom )
				{
					// a line: whole words while they fit (a word too long for any line is cut)
					lineText.clear();
					Bool full = FALSE;
					while( !full )
					{
						word.clear();
						while( *c == L' ' )
							++c;
						const WideChar *start = c;
						while( *c != 0 && *c != L' ' && *c != L'\n' )
							word.concat( *c++ );
						UnicodeString tryLine = lineText;
						if( !tryLine.isEmpty() && !word.isEmpty() )
							tryLine.concat( L' ' );
						tryLine.concat( word );
						s_body->setText( tryLine );
						if( s_body->getWidth() > width && !lineText.isEmpty() )
						{
							c = start;			// to the next line
							full = TRUE;
						}
						else
						{
							lineText = tryLine;
							if( *c == L'\n' )
							{
								++c;
								full = TRUE;
							}
							else if( *c == 0 )
							{
								full = TRUE;
								more = FALSE;
							}
						}
					}
					if( more && y + 2 * lineH > bottom )
					{
						// the last line that fits, and more to say
						UnicodeString cut = lineText;
						cut.concat( L".." );
						s_body->setText( cut );
						while( s_body->getWidth() > width && lineText.getLength() > 1 )
						{
							lineText.removeLastChar();
							cut = lineText;
							cut.concat( L".." );
							s_body->setText( cut );
						}
						lineText = cut;
						more = FALSE;
					}
					s_body->setText( lineText );
					s_body->draw( textX, y, a.textDim, GameMakeColor( 0, 0, 0, 200 ) );
					y += lineH;
				}
			}
		}
		else if( !s_groups.empty() )
		{
			// several selected: each kind's picture and how many
			UnicodeString head;
			head.format( L"%d selected", s_selected );
			drawText( s_name, head, L.px + (Int)( 18.0f * s ), L.py + (Int)( 12.0f * s ), a.text );
			for( size_t i = 0; i < s_groups.size(); ++i )
			{
				Int gx, gy, gs;
				groupRect( L, (Int)i, &gx, &gy, &gs );
				const Group &g = s_groups[i];
				const Image *portrait = portraitOf( g.kind );
				TheDisplay->drawFillRect( gx, gy, gs, gs, GameMakeColor( 2, 8, 14, 200 ) );
				if( portrait != nullptr )
					TheDisplay->drawImage( portrait, gx + 1, gy + 1, gx + gs - 1, gy + gs - 1 );
				const Real frac = g.count > 0 ? g.health / g.count : 0.0f;
				const Int hb = max( 2, gs / 14 );
				TheDisplay->drawFillRect( gx + 1, gy + gs - hb - 1, gs - 2, hb, GameMakeColor( 0, 0, 0, 200 ) );
				TheDisplay->drawFillRect( gx + 1, gy + gs - hb - 1, (Int)( ( gs - 2 ) * frac ), hb,
					frac > 0.5f ? GameMakeColor( 80, 220, 110, 255 ) : ( frac > 0.25f ? GameMakeColor( 240, 200, 60, 255 ) : GameMakeColor( 230, 60, 50, 255 ) ) );
				TheDisplay->drawOpenRect( gx, gy, gs, gs, (Int)i == s_hoverGroup ? 2.0f : 1.0f, (Int)i == s_hoverGroup ? a.text : a.accent );
				if( g.count > 1 )
				{
					UnicodeString n;
					n.format( L"%d", g.count );
					s_text->setText( n );
					drawText( s_text, n, gx + gs - s_text->getWidth() - 3, gy + 2, a.text );
				}
			}
		}
		else
		{
			// nothing selected: the faction's emblem and the player's name
			if( a.emblem != nullptr )
				TheDisplay->drawImage( a.emblem, px, py, px + ps, py + ps, GameMakeColor( 255, 255, 255, 140 ) );
			drawText( s_name, me->getPlayerDisplayName(), textX, L.py + (Int)( 14.0f * s ), a.text );
			drawText( s_body, plain( isNod() ? "Brotherhood of Nod" : "Global Defense Initiative" ), textX, L.py + (Int)( 36.0f * s ), a.textDim );
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** the tier bar: the tier, and the next being researched */
	void drawTier( GameWindow *, WinInstanceData * )
	{
		if( TheDisplay == nullptr || localPlayer() == nullptr )
			return;
		HUDLayout L;
		computeLayout( &L );
		ensureStrings( L );
		Art a;
		getArt( &a );
		Real progress = 0.0f;
		Bool isTier = FALSE;
		const Int tier = tierOf( &progress, &isTier );
		// (its label is drawn last, on the bar: under C&C4's selection panel there is no room above it)
		UnicodeString label;
		label.format( isTier ? L"Tier %d" : L"Rank %d", tier );
		if( isTier && progress >= 0.0f && tier < 3 )
		{
			UnicodeString next;
			next.format( L"   Tier %d  %d%%", tier + 1, (Int)( progress * 100.0f ) );
			label.concat( next );
		}
		if( a.tier != nullptr )
			TheDisplay->drawImage( a.tier, L.tierX, L.tierY, L.tierX + L.tierW, L.tierY + L.tierH );
		else
			TheDisplay->drawOpenRect( L.tierX, L.tierY, L.tierW, L.tierH, 1.0f, a.accent );
		// the fill, inside the frame (its art's inside: 5 to 465 across, 6 to 19 down, of 469 by 21)
		const Real sx = L.tierW / 469.0f, sy = L.tierH / 21.0f;
		const Int fx = L.tierX + (Int)( 5.0f * sx ), fy = L.tierY + (Int)( 6.0f * sy );
		const Int fwMost = (Int)( 460.0f * sx ), fh = max( 2, (Int)( 13.0f * sy ) );
		const Int fw = progress > 0.0f ? (Int)( fwMost * ( progress > 1.0f ? 1.0f : progress ) ) : 0;
		if( fw > 0 )
		{
			if( a.tierFill != nullptr )
				TheDisplay->drawImage( a.tierFill, fx, fy, fx + fw, fy + fh );
			else
				TheDisplay->drawFillRect( fx, fy, fw, fh, a.accent );
		}
		s_text->setText( label );
		Int lw = 0, lh = 0;
		s_text->getSize( &lw, &lh );
		drawText( s_text, label, L.tierX + ( L.tierW - lw ) / 2, L.tierY + ( L.tierH - lh ) / 2, a.text );
	}

	//-----------------------------------------------------------------------------------------------
	WindowMsgHandledType inputColumn( GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData )
	{
		const Int mx = (Short)( mData1 & 0xFFFF ), my = (Short)( ( mData1 >> 16 ) & 0xFFFF );
		HUDLayout L;
		computeLayout( &L );
		Int hit = -1;
		for( Int i = 0; i < buttonCount() && hit < 0; ++i )
		{
			Int x, y, w, h;
			buttonRect( L, i, &x, &y, &w, &h );
			if( mx >= x && mx < x + w && my >= y && my < y + h )
				hit = i;
		}
		switch( msg )
		{
			case GWM_MOUSE_POS:
				s_hoverButton = hit;
				return MSG_HANDLED;
			case GWM_MOUSE_LEAVING:
				s_hoverButton = -1;
				return MSG_HANDLED;
			case GWM_LEFT_UP:
				if( hit >= 0 )
					pressButton( hit );
				return MSG_HANDLED;
			case GWM_LEFT_DOWN:
			case GWM_RIGHT_DOWN:
			case GWM_RIGHT_UP:
			case GWM_WHEEL_UP:
			case GWM_WHEEL_DOWN:
				return MSG_HANDLED;
			default:
				return MSG_IGNORED;
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** what of the panel is at a point: a kind's tile (0..), the portrait (100), the panel (200), else -1 */
	Int panelHit( Int mx, Int my )
	{
		HUDLayout L;
		computeLayout( &L );
		if( mx < L.px || mx >= L.px + L.pw || my < L.py || my >= L.py + L.ph )
			return -1;
		if( s_viewed != INVALID_ID )
		{
			Int x, y, size;
			portraitRect( L, &x, &y, &size );
			if( mx >= x && mx < x + size && my >= y && my < y + size )
				return 100;
		}
		else
		{
			for( size_t i = 0; i < s_groups.size(); ++i )
			{
				Int x, y, size;
				groupRect( L, (Int)i, &x, &y, &size );
				if( mx >= x && mx < x + size && my >= y && my < y + size )
					return (Int)i;
			}
		}
		return 200;
	}

	//-----------------------------------------------------------------------------------------------
	void clickPanel( Int hit, Bool right )
	{
		if( hit == 100 )
		{
			// the portrait: the view goes to the unit
			const Object *obj = TheGameLogic->findObjectByID( s_viewed );
			if( obj != nullptr && TheTacticalView != nullptr )
				TheTacticalView->lookAt( obj->getPosition() );
		}
		else if( hit >= 0 && hit < (Int)s_groups.size() )
			keepKind( hit, right );
	}

	//-----------------------------------------------------------------------------------------------
	WindowMsgHandledType inputPanel( GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData )
	{
		const Int mx = (Short)( mData1 & 0xFFFF ), my = (Short)( ( mData1 >> 16 ) & 0xFFFF );
		const Int hit = panelHit( mx, my );
		switch( msg )
		{
			case GWM_MOUSE_POS:
				s_hoverGroup = hit >= 0 && hit < 100 ? hit : -1;
				return hit >= 0 ? MSG_HANDLED : MSG_IGNORED;
			case GWM_MOUSE_LEAVING:
				s_hoverGroup = -1;
				return MSG_IGNORED;
			case GWM_LEFT_UP:
			case GWM_RIGHT_UP:
				if( hit < 0 )
					return MSG_IGNORED;
				clickPanel( hit, msg == GWM_RIGHT_UP );
				return MSG_HANDLED;
			case GWM_LEFT_DOWN:
			case GWM_RIGHT_DOWN:
			case GWM_WHEEL_UP:
			case GWM_WHEEL_DOWN:
				// (above the panel, between the command buttons, the game's view has the mouse)
				return hit >= 0 ? MSG_HANDLED : MSG_IGNORED;
			default:
				return MSG_IGNORED;
		}
	}

	//-----------------------------------------------------------------------------------------------
	WindowMsgHandledType inputTier( GameWindow *, UnsignedInt, WindowMsgData, WindowMsgData )
	{
		return MSG_IGNORED;
	}

	//-----------------------------------------------------------------------------------------------
	GameWindow *makeWindow( Int x, Int y, Int w, Int h, GameWinDrawFunc draw, GameWinInputFunc input )
	{
		GameWindow *win = TheWindowManager->winCreate( nullptr, WIN_STATUS_ENABLED | WIN_STATUS_ABOVE | WIN_STATUS_RIGHT_CLICK,
			x, y, w, h, nullptr, nullptr );
		if( win != nullptr )
		{
			win->winSetDrawFunc( draw );
			win->winSetInputFunc( input );
		}
		return win;
	}

	//-----------------------------------------------------------------------------------------------
	void fit( GameWindow *win, Int x, Int y, Int w, Int h )
	{
		Int cx = 0, cy = 0, cw = 0, ch = 0;
		win->winGetScreenPosition( &cx, &cy );
		win->winGetSize( &cw, &ch );
		if( cx != x || cy != y )
			win->winSetPosition( x, y );
		if( cw != w || ch != h )
			win->winSetSize( w, h );
	}

	//-----------------------------------------------------------------------------------------------
	/** the build tooltip Zero Hour shows over a command button: placed by its bar (now below the screen),
		so put above the command row, or beside the support powers */
	void placeTooltip( const HUDLayout &L )
	{
		static const NameKeyType tipID = NAMEKEY( "ControlBarPopupDescription.wnd:Parent" );
		GameWindow *tip = TheWindowManager->winGetWindowFromId( nullptr, tipID );
		if( tip == nullptr || tip->winIsHidden() || TheMouse == nullptr )
			return;
		const MouseIO *mouse = TheMouse->getMouseStatus();
		Int w = 0, h = 0;
		tip->winGetSize( &w, &h );
		Int x, y;
		if( mouse->pos.x < L.sw / 6 )
		{
			// the support powers, on the left: beside them
			x = mouse->pos.x + L.cmdW;
			y = mouse->pos.y - h / 2;
		}
		else
		{
			const Int lines = s_row.empty() ? 0 : ( (Int)s_row.size() + L.perRow - 1 ) / L.perRow;
			x = mouse->pos.x - w / 2;
			y = L.py - ( L.cmdH + L.gap ) * lines - L.gap * 2 - h;
		}
		x = max( 0, min( x, L.rx - w ) );
		y = max( 0, min( y, L.sh - h ) );
		Int cx = 0, cy = 0;
		tip->winGetScreenPosition( &cx, &cy );
		if( cx != x || cy != y )
			tip->winSetPosition( x, y );
	}

	//-----------------------------------------------------------------------------------------------
	/** whether the HUD is wanted now: with the sidebar, its art there, the player one who plays */
	Bool wanted()
	{
		if( TheGlobalData == nullptr || !TheGlobalData->m_tiberiumSidebar || TheGameLogic == nullptr || !TheGameLogic->isInGame()
			|| TheGameLogic->isInShellGame() || TheWindowManager == nullptr || TheDisplay == nullptr || TheControlBar == nullptr
			|| TheInGameUI == nullptr || TheMappedImageCollection == nullptr )
			return FALSE;
		const Player *me = localPlayer();
		if( me == nullptr || me->isPlayerObserver() || TheGameLogic->isInReplayGame() )
			return FALSE;
		return TheMappedImageCollection->findImageByName( "TibHUD_PanelGDI" ) != nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumHUD::isOn()
{
	return s_on;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumHUD::isHidden()
{
	return s_on && s_hidden;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumHUD::getSidebarArea( Int *x, Int *y, Int *w, Int *h )
{
	if( !s_on || TheDisplay == nullptr )
		return FALSE;
	HUDLayout L;
	computeLayout( &L );
	*x = L.rx;
	*y = L.ry;
	*w = L.rw;
	*h = L.rh;
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumHUD::getCommandBar( Int *x, Int *y, Real *scale )
{
	if( !s_on || TheDisplay == nullptr )
		return FALSE;
	HUDLayout L;
	computeLayout( &L );
	*x = L.rx;
	*y = L.ry;
	*scale = L.b;
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumHUD::update()
{
	if( !wanted() )
	{
		reset();
		return;
	}
	HUDLayout L;
	computeLayout( &L );
	if( s_column == nullptr )
	{
		s_column = makeWindow( L.lx, L.ly, L.lw, L.lh, drawColumn, inputColumn );
		s_panel = makeWindow( L.px, 0, L.pw, L.sh, drawPanel, inputPanel );
		s_tier = makeWindow( L.tierX, L.tierY, L.tierW, L.tierH, drawTier, inputTier );
		s_hold = TheWindowManager->winCreate( nullptr, WIN_STATUS_HIDDEN, 0, 0, 1, 1, nullptr, nullptr );
		s_powers = makeWindow( 0, 0, 1, 1, drawPowers, inputPowers );
		if( s_column == nullptr || s_panel == nullptr || s_tier == nullptr || s_hold == nullptr || s_powers == nullptr )
		{
			reset();
			return;
		}
		s_on = TRUE;
		s_buildButtons.clear();
	}

	// Zero Hour's bar: below the screen (the game may put it back: a stage change, a cinematic's end)
	static const NameKeyType masterID = NAMEKEY( "ControlBar.wnd:ControlBarParent" );
	GameWindow *master = TheWindowManager->winGetWindowFromId( nullptr, masterID );
	if( master != s_master )
	{
		s_master = master;
		if( master != nullptr )
			master->winGetPosition( &s_masterAt.x, &s_masterAt.y );
	}
	if( master != nullptr )
	{
		Int mx = 0, my = 0;
		master->winGetPosition( &mx, &my );
		if( my < L.sh * 2 )
			master->winSetPosition( mx, L.sh * 3 );
		if( TheTacticalView != nullptr && TheTacticalView->getHeight() != L.sh )
			TheControlBar->setFullViewportHeight();
	}
	// the game hides its bar for a cinematic: the HUD hides with it
	s_hidden = master != nullptr && master->winIsHidden();
	s_column->winHide( s_hidden );
	s_panel->winHide( s_hidden );
	s_tier->winHide( s_hidden );

	// the screen may have changed size
	fit( s_column, L.lx, L.ly, L.lw, L.lh );
	const Int panelTop = L.py - ( L.cmdH + L.gap ) * MAX_ROW_LINES - L.gap * 2;
	fit( s_panel, L.px, panelTop, L.pw, L.sh - panelTop );
	fit( s_tier, L.tierX, L.tierY, L.tierW, L.tierH );

	// the radar, in the column
	static const NameKeyType radarID = NAMEKEY( "ControlBar.wnd:LeftHUD" );
	GameWindow *radar = TheWindowManager->winGetWindowFromId( nullptr, radarID );
	if( radar != nullptr )
		place( radar, s_column, L.radarX, L.radarY, L.radarS, L.radarS );

	// the command buttons: the abilities in the slots, the commands in the row, the rest held
	collectSelection();
	sortCommands();
	std::vector<Bool> shown( MAX_COMMANDS_PER_SET, FALSE );
	for( size_t i = 0; i < s_slots.size(); ++i )
	{
		Int x, y, w, h;
		slotRect( L, (Int)i, &x, &y, &w, &h );
		const Int inset = max( 2, (Int)( 4.0f * L.s ) );
		place( TheControlBar->getCommandWindow( s_slots[i] ), s_panel, x + inset, y + inset, w - 2 * inset, h - 2 * inset );
		shown[s_slots[i]] = TRUE;
	}
	for( size_t i = 0; i < s_row.size() && (Int)i < L.perRow * MAX_ROW_LINES; ++i )
	{
		Int x, y;
		rowRect( L, (Int)i, &x, &y );
		place( TheControlBar->getCommandWindow( s_row[i] ), s_panel, x, y, L.cmdW, L.cmdH );
		shown[s_row[i]] = TRUE;
	}
	for( Int i = 0; i < MAX_COMMANDS_PER_SET; ++i )
		if( !shown[i] && TheControlBar->getCommandWindow( i ) != nullptr )
			take( TheControlBar->getCommandWindow( i ), s_hold );

	// the support powers' bar, on the left (after the control bar's own update: it slides it in from the right)
	GameWindow *shortcut = nullptr;
	const PlayerTemplate *pt = localPlayer()->getPlayerTemplate();
	if( pt != nullptr && pt->getSpecialPowerShortcutWinName().isNotEmpty() )
	{
		AsciiString name = pt->getSpecialPowerShortcutWinName();
		name.concat( ":GenPowersShortcutBarParent" );
		shortcut = TheWindowManager->winGetWindowFromId( nullptr, TheNameKeyGenerator->nameToKey( name ) );
	}
	if( shortcut != s_shortcut )
	{
		s_shortcut = shortcut;
		if( shortcut != nullptr )
			shortcut->winGetPosition( &s_shortcutAt.x, &s_shortcutAt.y );
	}
	// Tiberium Project @feature a faction without General's Points: the Powers bar in place of Zero Hour's
	const Bool powers = powersWanted();
	if( powers )
	{
		collectPowers();
		PowersLayout P;
		powersLayout( L, (Int)s_powerList.size(), localPlayer()->hasSupportMeter(), &P );
		fit( s_powers, P.x0, P.y0, P.w, P.h );
		if( shortcut != nullptr && !shortcut->winIsHidden() )
			shortcut->winHide( TRUE );
	}
	s_powers->winHide( !powers || s_hidden || s_powersHidden || ( s_powerList.empty() && !localPlayer()->hasSupportMeter() ) );
	if( shortcut != nullptr && !powers )
	{
		Int x = 0, y = 0, w = 0, h = 0;
		shortcut->winGetPosition( &x, &y );
		shortcut->winGetSize( &w, &h );
		const Int left = max( 4, L.sw / 200 );
		// (up the left edge, above the radar's panel)
		const Int top = max( L.gap, min( s_shortcutAt.y, L.ly - L.gap * 2 - h ) );
		if( x != left || y != top )
			shortcut->winSetPosition( left, top );
		if( s_hidden && !shortcut->winIsHidden() )
			shortcut->winHide( TRUE );
	}

	placeTooltip( L );
}

//-------------------------------------------------------------------------------------------------
void TiberiumHUD::reset()
{
	if( !s_on && s_column == nullptr && s_moved.empty() )
		return;
	if( TheWindowManager != nullptr )
	{
		giveBack();
		if( s_column != nullptr )
			TheWindowManager->winDestroy( s_column );
		if( s_panel != nullptr )
			TheWindowManager->winDestroy( s_panel );
		if( s_tier != nullptr )
			TheWindowManager->winDestroy( s_tier );
		if( s_hold != nullptr )
			TheWindowManager->winDestroy( s_hold );
		if( s_powers != nullptr )
			TheWindowManager->winDestroy( s_powers );
	}
	s_moved.clear();
	s_column = s_panel = s_tier = s_hold = s_powers = nullptr;
	s_powerList.clear();
	s_powerListFrame = 0xFFFFFFFFu;
	s_hoverPower = -1;
	s_master = s_shortcut = nullptr;
	s_on = s_hidden = FALSE;
	s_row.clear();
	s_slots.clear();
	s_groups.clear();
	s_buildButtons.clear();
	s_viewed = INVALID_ID;
	s_hoverButton = s_hoverGroup = -1;
	if( s_name != nullptr && TheDisplayStringManager != nullptr )
	{
		TheDisplayStringManager->freeDisplayString( s_name );
		TheDisplayStringManager->freeDisplayString( s_text );
		TheDisplayStringManager->freeDisplayString( s_body );
	}
	s_name = s_text = s_body = nullptr;
	s_fontHeight = 0;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumHUD::testClickPart( const char *part, Int index, Bool right )
{
	if( !s_on || part == nullptr || index < 0 )
		return FALSE;
	if( strcmp( part, "button" ) == 0 && index < buttonCount() )
		return pressButton( index );
	if( strcmp( part, "row" ) == 0 || strcmp( part, "slot" ) == 0 )
	{
		const std::vector<Int> &which = strcmp( part, "row" ) == 0 ? s_row : s_slots;
		if( index >= (Int)which.size() )
			return FALSE;
		GameWindow *win = TheControlBar->getCommandWindow( which[index] );
		if( win == nullptr || !win->winGetEnabled() )
			return FALSE;
		TheWindowManager->winSendSystemMsg( win->winGetOwner(), right ? GBM_SELECTED_RIGHT : GBM_SELECTED, (WindowMsgData)win, 0 );
		return TRUE;
	}
	if( strcmp( part, "group" ) == 0 && index < (Int)s_groups.size() )
	{
		clickPanel( index, right );
		return TRUE;
	}
	if( strcmp( part, "portrait" ) == 0 && s_viewed != INVALID_ID )
	{
		clickPanel( 100, right );
		return TRUE;
	}
	if( strcmp( part, "power" ) == 0 && powersWanted() )
	{
		// Tiberium Project @feature a click on the Powers bar's index-th power
		collectPowers();
		if( index >= (Int)s_powerList.size() )
			return FALSE;
		pressPower( index );
		return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumHUD::describe( char *buffer, Int size )
{
	if( buffer == nullptr || size <= 0 )
		return;
	if( !s_on )
	{
		snprintf( buffer, size, "off" );
		return;
	}
	HUDLayout L;
	computeLayout( &L );
	AsciiString row, slots;
	for( size_t i = 0; i < s_row.size(); ++i )
	{
		const CommandButton *b = (const CommandButton *)GadgetButtonGetData( TheControlBar->getCommandWindow( s_row[i] ) );
		row.concat( i == 0 ? "" : " " );
		row.concat( b != nullptr ? b->getName().str() : "?" );
	}
	for( size_t i = 0; i < s_slots.size(); ++i )
	{
		const CommandButton *b = (const CommandButton *)GadgetButtonGetData( TheControlBar->getCommandWindow( s_slots[i] ) );
		slots.concat( i == 0 ? "" : " " );
		slots.concat( b != nullptr ? b->getName().str() : "?" );
	}
	const Object *obj = s_viewed != INVALID_ID ? TheGameLogic->findObjectByID( s_viewed ) : nullptr;
	Real left = 0.0f, full = 0.0f, progress = 0.0f;
	Bool up = FALSE, isTier = FALSE;
	const Bool shield = obj != nullptr && ShieldSphereUpdate::getShieldOf( obj, &left, &full, &up );
	const Int tier = tierOf( &progress, &isTier );
	Int rx = 0, ry = 0;
	static const NameKeyType radarID = NAMEKEY( "ControlBar.wnd:LeftHUD" );
	GameWindow *radar = TheWindowManager->winGetWindowFromId( nullptr, radarID );
	if( radar != nullptr )
		radar->winGetScreenPosition( &rx, &ry );
	// (Tiberium Project @feature the Powers bar: how many powers, how many ready, the support meter)
	Int powers = 0, readyPowers = 0;
	if( powersWanted() )
	{
		collectPowers();
		powers = (Int)s_powerList.size();
		for( size_t i = 0; i < s_powerList.size(); ++i )
			if( s_powerList[i].ready >= 1.0f && !s_powerList[i].off )
				++readyPowers;
	}
	const Player *me = localPlayer();
	snprintf( buffer, size, "on%s; selected %d (%d kinds)%s%s%s; shield %s; row [%s]; slots [%s]; %s %d (next %d%%); radar at %d,%d; view %d high; powers %d (%d ready)%s, meter %d/%d",
		s_hidden ? " (hidden)" : "", (int)s_selected, (int)s_groups.size(), obj != nullptr ? ": " : "",
		obj != nullptr ? obj->getTemplate()->getName().str() : "", obj != nullptr && obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) ? " (being built)" : "",
		shield ? ( up ? "up" : "down" ) : "none", row.str(), slots.str(), isTier ? "tier" : "rank", (int)tier, progress >= 0.0f ? (int)( progress * 100.0f ) : 0,
		(int)rx, (int)ry, TheTacticalView != nullptr ? (int)TheTacticalView->getHeight() : -1, (int)powers, (int)readyPowers,
		s_powersHidden ? " (put away)" : "", me != nullptr ? (int)me->getSupportPoints() : 0, me != nullptr ? (int)me->getSupportPointsMax() : 0 );
}
