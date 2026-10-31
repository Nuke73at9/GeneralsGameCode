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

#include "Common/GameUtility.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
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

#include <map>
#include <vector>

namespace
{
	enum
	{
		SLOT_COUNT = 3,		///< C&C4's ability slots
		BUTTON_COUNT = 4,
		MAX_GROUPS = 14,
		MAX_ROW_LINES = 2
	};

	/// the column's buttons: Zero Hour's own (their clicks are sent as if on them)
	const char *const BUTTON_WINDOWS[BUTTON_COUNT] = { "ControlBar.wnd:ButtonOptions", "ControlBar.wnd:ButtonIdleWorker",
		"ControlBar.wnd:ButtonPlaceBeacon", "ControlBar.wnd:ButtonGeneral" };
	const char *const BUTTON_LABELS[BUTTON_COUNT] = { "Menu", "Idle", "Beacon", "General" };

	/// the panel's art (TibHUD_PanelGDI and Nod), in whose pixels its parts are placed
	const Real ART_W = 470.0f;
	const Real ART_H = 145.0f;

	struct HUDLayout
	{
		Int sw, sh, gap;
		Int colX, colY, colW;										///< the column's top part (the sidebar is below sideY)
		Int meterX, meterW, radarX, radarY, radarS;
		Int moneyY, moneyH, buttonsY, buttonsH, sideY;
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

	//-----------------------------------------------------------------------------------------------
	Player *localPlayer()
	{
		return ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
	}

	//-----------------------------------------------------------------------------------------------
	Bool isNod()
	{
		const Player *me = localPlayer();
		return me != nullptr && me->getSide().compareNoCase( "China" ) == 0;
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
		// the column: as wide as the sidebar was on its own (TiberiumSidebar), from the top to the bottom
		L->colW = max( 200, sw * 21 / 100 > 360 ? 360 : sw * 21 / 100 );
		L->colX = sw - L->colW;
		L->colY = max( 16, sh / 40 );
		L->gap = max( 3, L->colW / 80 );
		L->meterW = max( 8, L->colW / 22 );
		L->meterX = L->colX + L->gap * 2;
		const Int room = L->colW - L->meterW - L->gap * 5;
		L->radarS = min( room, sh * 30 / 100 );
		L->radarX = L->meterX + L->meterW + L->gap * 2 + ( room - L->radarS ) / 2;
		L->radarY = L->colY + L->gap * 2;
		L->moneyY = L->radarY + L->radarS + L->gap * 2;
		L->moneyH = max( 18, sh / 36 );
		L->buttonsY = L->moneyY + L->moneyH + L->gap;
		L->buttonsH = max( 20, sh / 32 );
		L->sideY = L->buttonsY + L->buttonsH + L->gap * 2;
		// the panel: C&C4's art, a fifth of the screen tall (no wider than 60% of the room left of the column)
		Real s = sh * 0.19f / ART_H;
		if( s > L->colX * 0.6f / ART_W )
			s = L->colX * 0.6f / ART_W;
		if( s < 0.5f )
			s = 0.5f;
		L->s = s;
		L->pw = (Int)( ART_W * s );
		L->ph = (Int)( ART_H * s );
		L->px = ( L->colX - L->pw ) / 2;
		L->py = sh - L->ph - max( 2, sh / 200 );
		// Zero Hour's command buttons, their shape kept (50 by 44 on the bar's 800 by 600)
		L->cmdH = max( 22, (Int)( 36.0f * s ) );
		L->cmdW = L->cmdH * 50 / 44;
		L->rowX = L->px + (Int)( 16.0f * s );
		L->perRow = max( 1, ( L->pw - (Int)( 32.0f * s ) ) / ( L->cmdW + L->gap ) );
		// the tier bar, bottom left (its art is 469 by 21)
		const Int m = max( 8, sw / 160 );
		L->tierX = m;
		L->tierW = min( (Int)( 469.0f * s * 0.8f ), L->px - 2 * m );
		L->tierH = max( 8, L->tierW * 21 / 469 );
		L->tierLabelH = max( 14, sh / 40 );
		L->tierY = sh - L->tierH - m;
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
		const Int bw = ( L.colW - L.gap * ( BUTTON_COUNT + 3 ) ) / BUTTON_COUNT;
		*x = L.colX + L.gap * 2 + i * ( bw + L.gap );
		*y = L.buttonsY;
		*w = bw;
		*h = L.buttonsH;
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
		const Color back = lit ? a.accentDim : GameMakeColor( 8, 16, 24, 220 );
		TheDisplay->drawFillRect( x, y, w, h, back );
		TheDisplay->drawOpenRect( x - 1, y - 1, w + 2, h + 2, 1.0f, GameMakeColor( 0, 0, 0, 120 ) );
		TheDisplay->drawOpenRect( x, y, w, h, hover ? 2.0f : 1.0f, on ? a.accent : GameMakeColor( 60, 70, 78, 255 ) );
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
	Bool buttonUsable( Int i )
	{
		GameWindow *win = findWindow( BUTTON_WINDOWS[i] );
		return win != nullptr && !win->winIsHidden() && win->winGetEnabled();
	}

	//-----------------------------------------------------------------------------------------------
	/** a click on one of Zero Hour's buttons, as if it were clicked */
	Bool pressButton( Int i )
	{
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
							if( e->getProductionUpgrade() == f->up && e->getPercentComplete() > f->percent )
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
		TheDisplay->drawFillRect( L.colX, L.colY, L.colW, L.sideY - L.colY, GameMakeColor( 6, 11, 16, 225 ) );
		TheDisplay->drawFillRect( L.colX, L.colY, 2, L.sideY - L.colY, a.accent );
		// the radar's frame; without a radar, the faction's emblem where it would be
		TheDisplay->drawFillRect( L.radarX, L.radarY, L.radarS, L.radarS, GameMakeColor( 2, 6, 10, 240 ) );
		static const NameKeyType radarID = NAMEKEY( "ControlBar.wnd:LeftHUD" );
		GameWindow *radar = TheWindowManager->winGetWindowFromId( nullptr, radarID );
		if( radar != nullptr && a.emblem != nullptr && TheRadar != nullptr && !rts::localPlayerHasRadar() )
		{
			const Int e = L.radarS * 3 / 4;
			TheDisplay->drawImage( a.emblem, L.radarX + ( L.radarS - e ) / 2, L.radarY + ( L.radarS - e ) / 2,
				L.radarX + ( L.radarS + e ) / 2, L.radarY + ( L.radarS + e ) / 2, GameMakeColor( 255, 255, 255, 110 ) );
		}
		TheDisplay->drawOpenRect( L.radarX - 3, L.radarY - 3, L.radarS + 6, L.radarS + 6, 1.0f, a.accentDim );
		TheDisplay->drawOpenRect( L.radarX - 2, L.radarY - 2, L.radarS + 4, L.radarS + 4, 2.0f, a.accent );

		// the power meter: what the player makes (lit), what is used of it (bright), what is short (red)
		const Int produced = me->getEnergy()->getProduction(), used = me->getEnergy()->getConsumption();
		const Int most = max( 1, max( produced, used ) );
		const Int segH = max( 4, L.radarS / 26 ), segs = max( 4, ( L.radarS + 2 ) / segH );
		TheDisplay->drawFillRect( L.meterX - 2, L.radarY - 2, L.meterW + 4, L.radarS + 4, GameMakeColor( 2, 6, 10, 240 ) );
		TheDisplay->drawOpenRect( L.meterX - 2, L.radarY - 2, L.meterW + 4, L.radarS + 4, 1.0f, a.accentDim );
		const Int litMade = produced * segs / most, litUsed = min( used, produced ) * segs / most, litShort = used * segs / most;
		for( Int i = 0; i < segs; ++i )
		{
			const Int y = L.radarY + L.radarS - ( i + 1 ) * segH;
			if( y < L.radarY )
				break;
			Color c = GameMakeColor( 30, 36, 40, 255 );
			if( i < litUsed )
				c = used * 10 > produced * 8 ? GameMakeColor( 240, 200, 60, 255 ) : GameMakeColor( 90, 230, 120, 255 );
			else if( i < litMade )
				c = GameMakeColor( 40, 110, 60, 255 );
			else if( i < litShort )
				c = GameMakeColor( 240, 60, 50, 255 );
			TheDisplay->drawFillRect( L.meterX, y + 1, L.meterW, segH - 2, c );
		}

		// the money and the power
		UnicodeString money, digits;
		digits.format( L"%u", me->getMoney()->countMoney() );
		money = L"$ ";
		const Int n = digits.getLength();
		for( Int i = 0; i < n; ++i )
		{
			money.concat( digits.getCharAt( i ) );
			if( ( n - 1 - i ) % 3 == 0 && i != n - 1 )
				money.concat( L',' );
		}
		drawText( s_name, money, L.colX + L.gap * 3, L.moneyY, GameMakeColor( 255, 214, 90, 255 ) );
		UnicodeString power;
		power.format( L"Power %d / %d", used, produced );
		s_text->setText( power );
		drawText( s_text, power, L.colX + L.colW - s_text->getWidth() - L.gap * 3, L.moneyY + L.gap,
			used > produced ? GameMakeColor( 255, 80, 60, 255 ) : a.textDim );

		// the buttons
		for( Int i = 0; i < BUTTON_COUNT; ++i )
		{
			Int x, y, w, h;
			buttonRect( L, i, &x, &y, &w, &h );
			const Bool usable = buttonUsable( i );
			// (Zero Hour enables the idle worker's while there is one)
			const UnicodeString label = plain( BUTTON_LABELS[i] );
			// the general's: lit while there is a promotion to spend
			const Bool lit = i == 3 && me->getSciencePurchasePoints() > 0 && ( ( TheGameLogic->getFrame() / 15 ) & 1 );
			drawPlate( a, x, y, w, h, label, usable, usable && i == s_hoverButton, lit );
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
					UnicodeString what;
					if( e->getProductionObject() != nullptr )
						what = nameOf( e->getProductionObject() );
					else if( e->getProductionUpgrade() != nullptr )
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
		UnicodeString label;
		label.format( isTier ? L"Tier %d" : L"Rank %d", tier );
		drawText( s_name, label, L.tierX + 2, L.tierY - L.tierLabelH - 2, a.text );
		if( isTier && progress >= 0.0f && tier < 3 )
		{
			UnicodeString next;
			next.format( L"Tier %d  %d%%", tier + 1, (Int)( progress * 100.0f ) );
			s_text->setText( next );
			drawText( s_text, next, L.tierX + L.tierW - s_text->getWidth() - 2, L.tierY - L.tierLabelH, a.textDim );
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
	}

	//-----------------------------------------------------------------------------------------------
	WindowMsgHandledType inputColumn( GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData )
	{
		const Int mx = (Short)( mData1 & 0xFFFF ), my = (Short)( ( mData1 >> 16 ) & 0xFFFF );
		HUDLayout L;
		computeLayout( &L );
		Int hit = -1;
		for( Int i = 0; i < BUTTON_COUNT && hit < 0; ++i )
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
		x = max( 0, min( x, L.colX - w ) );
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
	*x = L.colX;
	*y = L.sideY;
	*w = L.colW;
	*h = L.sh - L.sideY;
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
		s_column = makeWindow( L.colX, L.colY, L.colW, L.sideY - L.colY, drawColumn, inputColumn );
		s_panel = makeWindow( L.px, 0, L.pw, L.sh, drawPanel, inputPanel );
		s_tier = makeWindow( L.tierX, L.tierY - L.tierLabelH - 2, L.tierW, L.tierH + L.tierLabelH + 2, drawTier, inputTier );
		s_hold = TheWindowManager->winCreate( nullptr, WIN_STATUS_HIDDEN, 0, 0, 1, 1, nullptr, nullptr );
		if( s_column == nullptr || s_panel == nullptr || s_tier == nullptr || s_hold == nullptr )
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
	fit( s_column, L.colX, L.colY, L.colW, L.sideY - L.colY );
	const Int panelTop = L.py - ( L.cmdH + L.gap ) * MAX_ROW_LINES - L.gap * 2;
	fit( s_panel, L.px, panelTop, L.pw, L.sh - panelTop );
	fit( s_tier, L.tierX, L.tierY - L.tierLabelH - 2, L.tierW, L.tierH + L.tierLabelH + 2 );

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
	if( shortcut != nullptr )
	{
		Int x = 0, y = 0;
		shortcut->winGetPosition( &x, &y );
		const Int left = max( 4, L.sw / 200 );
		if( x != left || y != s_shortcutAt.y )
			shortcut->winSetPosition( left, s_shortcutAt.y );
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
	}
	s_moved.clear();
	s_column = s_panel = s_tier = s_hold = nullptr;
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
	if( strcmp( part, "button" ) == 0 && index < BUTTON_COUNT )
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
	snprintf( buffer, size, "on%s; selected %d (%d kinds)%s%s%s; shield %s; row [%s]; slots [%s]; %s %d (next %d%%); radar at %d,%d; view %d high",
		s_hidden ? " (hidden)" : "", (int)s_selected, (int)s_groups.size(), obj != nullptr ? ": " : "",
		obj != nullptr ? obj->getTemplate()->getName().str() : "", obj != nullptr && obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) ? " (being built)" : "",
		shield ? ( up ? "up" : "down" ) : "none", row.str(), slots.str(), isTier ? "tier" : "rank", (int)tier, progress >= 0.0f ? (int)( progress * 100.0f ) : 0,
		(int)rx, (int)ry, TheTacticalView != nullptr ? (int)TheTacticalView->getHeight() : -1 );
}
