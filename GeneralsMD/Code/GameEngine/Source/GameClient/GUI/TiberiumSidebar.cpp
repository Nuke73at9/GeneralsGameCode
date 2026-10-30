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

// FILE: TiberiumSidebar.cpp //////////////////////////////////////////////////////////////////////
// Tiberium Project @feature the Red Alert 3 style sidebar (see TiberiumSidebar.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/TiberiumSidebar.h"

#include "Common/BuildAssistant.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Image.h"
#include "GameClient/RadiusDecal.h"
#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/BaseConstructionUpdate.h"
#include "GameLogic/Module/BuildAreaBehavior.h"
#include "GameLogic/Module/ProductionUpdate.h"

#include <vector>

namespace
{
	enum SidebarTab
	{
		TAB_STRUCTURES,
		TAB_DEFENSES,
		TAB_INFANTRY,
		TAB_VEHICLES,
		TAB_AIRCRAFT,
		TAB_UPGRADES,
		TAB_COUNT
	};
	const char *const TAB_NAMES[TAB_COUNT] = { "Base", "Def", "Inf", "Veh", "Air", "Tech" };

	struct SidebarItem
	{
		const CommandButton *button;
		const ThingTemplate *thing;			///< a unit or a structure
		const UpgradeTemplate *upgrade;	///< or an upgrade
	};

	/// a producer and what it builds of a tab (a factory's units; for structures, every builder of a kind)
	struct SidebarQueue
	{
		ObjectID producer;						///< the factory (a structure tab's: one of the builders)
		const ThingTemplate *kind;		///< the producer's (the label; builders are grouped by it)
		std::vector<SidebarItem> items;
	};

	struct SidebarLayout
	{
		Int x, y, w, h;							///< the whole sidebar
		Int headerH, tabsY, tabsH, chipsY, chipH, chipsPerRow, gridY, cellW, cellH, gap, cols, infoY, infoH;
	};

	GameWindow *s_window = nullptr;
	Bool s_hasYard = FALSE;		///< the player has a construction yard (Kane's Wrath's base): builders keep to defenses
	Int s_tab = TAB_INFANTRY;
	ObjectID s_queueOf[TAB_COUNT];								///< each tab's chosen queue (its producer)
	const ThingTemplate *s_queueKindOf[TAB_COUNT];			///< ... and its kind, when the producer is gone
	std::vector<SidebarQueue> s_queues[TAB_COUNT];
	Int s_hoverItem = -1;
	UnsignedInt s_lastRefresh = 0;
	DisplayString *s_label = nullptr;
	DisplayString *s_small = nullptr;
	DisplayString *s_info = nullptr;
	Int s_fontScreenHeight = 0;

	RadiusDecalTemplate s_areaCursor;							///< how a build area is drawn (TiberiumGameData)
	std::vector<RadiusDecal *> s_areaDecals;			///< the build areas drawn, while a yard's structure is placed
	std::vector<Real> s_areaShown;								///< ... their centres and radii, to see when they change

	//-----------------------------------------------------------------------------------------------
	void clearBuildAreas()
	{
		for( size_t i = 0; i < s_areaDecals.size(); ++i )
			delete s_areaDecals[i];
		s_areaDecals.clear();
		s_areaShown.clear();
	}

	//-----------------------------------------------------------------------------------------------
	void collectArea( const Coord3D *center, Real radius, void *data )
	{
		std::vector<Real> *areas = (std::vector<Real> *)data;
		areas->push_back( center->x );
		areas->push_back( center->y );
		areas->push_back( center->z );
		areas->push_back( radius );
	}

	//-----------------------------------------------------------------------------------------------
	/** Kane's Wrath shows where a structure may go while it is placed: the base's build areas, drawn
		* on the ground, while the local player places a construction yard's structure */
	void updateBuildAreas()
	{
		const Player *me = ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
		Object *source = TheInGameUI != nullptr && TheInGameUI->getPendingPlaceType() != nullptr && TheGameLogic != nullptr
			? TheGameLogic->findObjectByID( TheInGameUI->getPendingPlaceSourceObjectID() ) : nullptr;
		const BaseConstructionUpdate *yard = BaseConstructionUpdate::find( source );
		if( yard != nullptr && !( yard->isReady() && yard->getBuilding() == TheInGameUI->getPendingPlaceType() ) )
		{
			// the yard's structure was placed (or cancelled, or the yard lost): nothing left to place
			TheInGameUI->placeBuildAvailable( nullptr, nullptr );
			yard = nullptr;
		}
		if( me == nullptr || !s_areaCursor.valid() || yard == nullptr )
		{
			clearBuildAreas();
			return;
		}
		std::vector<Real> areas;
		BuildAreaBehavior::iterateAreas( me, collectArea, &areas );
		if( areas != s_areaShown )
		{
			clearBuildAreas();
			for( size_t i = 0; i + 3 < areas.size(); i += 4 )
			{
				const Coord3D center = { areas[i], areas[i + 1], areas[i + 2] };
				RadiusDecal *decal = new RadiusDecal;
				s_areaCursor.createRadiusDecal( center, areas[i + 3], me, *decal );
				s_areaDecals.push_back( decal );
			}
			s_areaShown = areas;
		}
		for( size_t i = 0; i < s_areaDecals.size(); ++i )
			s_areaDecals[i]->update();
	}

	//-----------------------------------------------------------------------------------------------
	/** the side's colours: GDI's (America's slots) steel blue, Nod's (China's) red, C&C4's */
	void sideColors( Color *accent, Color *accentDim )
	{
		const Player *me = ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
		const Bool nod = me != nullptr && me->getSide().compareNoCase( "China" ) == 0;
		*accent = nod ? GameMakeColor( 214, 40, 40, 255 ) : GameMakeColor( 74, 160, 220, 255 );
		*accentDim = nod ? GameMakeColor( 110, 20, 20, 200 ) : GameMakeColor( 30, 80, 120, 200 );
	}

	//-----------------------------------------------------------------------------------------------
	void computeLayout( SidebarLayout *L )
	{
		const Int sw = TheDisplay->getWidth(), sh = TheDisplay->getHeight();
		L->w = max( 200, sw * 21 / 100 > 360 ? 360 : sw * 21 / 100 );
		L->x = sw - L->w;
		// below the game's clock line, above Zero Hour's bar, which spans the bottom
		L->y = max( 16, sh / 40 );
		L->h = sh * 74 / 100 - L->y;
		L->gap = max( 3, L->w / 80 );
		L->headerH = max( 26, sh / 32 );
		L->tabsY = L->y + L->headerH;
		L->tabsH = max( 24, sh / 34 );
		L->chipsY = L->tabsY + L->tabsH + L->gap;
		L->chipH = max( 20, sh / 44 );
		L->chipsPerRow = 2;
		L->cols = 3;
		L->cellW = ( L->w - L->gap * ( L->cols + 1 ) ) / L->cols;
		L->cellH = L->cellW * 4 / 5;
		L->infoH = max( 48, sh / 14 );
		L->infoY = L->y + L->h - L->infoH;
		L->gridY = 0;	// after the chips (they wrap)
	}

	//-----------------------------------------------------------------------------------------------
	Int chipRows( const SidebarLayout &L, Int count )
	{
		return count <= 0 ? 0 : ( count + L.chipsPerRow - 1 ) / L.chipsPerRow;
	}

	//-----------------------------------------------------------------------------------------------
	Int gridTop( const SidebarLayout &L )
	{
		return L.chipsY + chipRows( L, (Int)s_queues[s_tab].size() ) * ( L.chipH + L.gap ) + L.gap;
	}

	//-----------------------------------------------------------------------------------------------
	SidebarQueue *currentQueue()
	{
		std::vector<SidebarQueue> &qs = s_queues[s_tab];
		if( qs.empty() )
			return nullptr;
		for( size_t i = 0; i < qs.size(); ++i )
			if( qs[i].producer == s_queueOf[s_tab] )
				return &qs[i];
		for( size_t i = 0; i < qs.size(); ++i )
			if( qs[i].kind == s_queueKindOf[s_tab] )
				return &qs[i];
		return &qs[0];
	}

	//-----------------------------------------------------------------------------------------------
	/** the tab a factory's thing goes on */
	Int tabOf( const ThingTemplate *t )
	{
		if( t->isKindOf( KINDOF_STRUCTURE ) )
			return t->isKindOf( KINDOF_FS_BASE_DEFENSE ) || t->isKindOf( KINDOF_CAN_ATTACK ) ? TAB_DEFENSES : TAB_STRUCTURES;
		if( t->isKindOf( KINDOF_AIRCRAFT ) )
			return TAB_AIRCRAFT;
		if( t->isKindOf( KINDOF_INFANTRY ) )
			return TAB_INFANTRY;
		return TAB_VEHICLES;
	}

	//-----------------------------------------------------------------------------------------------
	void addItem( Int tab, ObjectID producer, const ThingTemplate *kind, Bool groupByKind, const SidebarItem &item )
	{
		std::vector<SidebarQueue> &qs = s_queues[tab];
		SidebarQueue *q = nullptr;
		for( size_t i = 0; i < qs.size() && q == nullptr; ++i )
			if( groupByKind ? qs[i].kind == kind : qs[i].producer == producer )
				q = &qs[i];
		if( q == nullptr )
		{
			SidebarQueue nq;
			nq.producer = producer;
			nq.kind = kind;
			qs.push_back( nq );
			q = &qs.back();
		}
		for( size_t i = 0; i < q->items.size(); ++i )
			if( q->items[i].thing == item.thing && q->items[i].upgrade == item.upgrade )
				return;
		q->items.push_back( item );
	}

	//-----------------------------------------------------------------------------------------------
	/** a producer's command set, and the pages it leads to (a Crawler's) */
	void collectCommandSet( Object *obj, const CommandSet *set, Int depth )
	{
		if( set == nullptr || depth > 4 )
			return;
		const Bool builder = obj->isKindOf( KINDOF_DOZER );
		ProductionUpdateInterface *pu = obj->getProductionUpdateInterface();
		for( Int i = 0; i < MAX_COMMANDS_PER_SET; ++i )
		{
			const CommandButton *b = set->getCommandButton( i );
			if( b == nullptr )
				continue;
			SidebarItem item = { b, nullptr, nullptr };
			switch( b->getCommandType() )
			{
				case GUI_COMMAND_UNIT_BUILD:
					item.thing = b->getThingTemplate();
					if( item.thing != nullptr && pu != nullptr )
						addItem( tabOf( item.thing ), obj->getID(), obj->getTemplate(), FALSE, item );
					break;
				case GUI_COMMAND_DOZER_CONSTRUCT:
					item.thing = b->getThingTemplate();
					// (with a construction yard, its builders keep to the defenses)
					if( item.thing != nullptr && builder && !( s_hasYard && tabOf( item.thing ) == TAB_STRUCTURES ) )
						addItem( tabOf( item.thing ), obj->getID(), obj->getTemplate(), TRUE, item );
					break;
				case GUI_COMMAND_PLAYER_UPGRADE:
					item.upgrade = b->getUpgradeTemplate();
					if( item.upgrade != nullptr && pu != nullptr )
						addItem( TAB_UPGRADES, obj->getID(), obj->getTemplate(), FALSE, item );
					break;
				case GUI_COMMAND_COMMAND_SET_PAGE:
					collectCommandSet( obj, TheControlBar->findCommandSet( b->getPageCommandSetName() ), depth + 1 );
					break;
				default:
					break;
			}
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** a construction yard's structures (BaseConstructionUpdate's command set) on the Base tab */
	void collectYard( Object *obj, BaseConstructionUpdate *base )
	{
		const CommandSet *set = TheControlBar->findCommandSet( base->getCommandSetName() );
		for( Int i = 0; set != nullptr && i < MAX_COMMANDS_PER_SET; ++i )
		{
			const CommandButton *b = set->getCommandButton( i );
			if( b == nullptr || b->getCommandType() != GUI_COMMAND_DOZER_CONSTRUCT || !base->builds( b->getThingTemplate() ) )
				continue;
			SidebarItem item = { b, b->getThingTemplate(), nullptr };
			addItem( TAB_STRUCTURES, obj->getID(), obj->getTemplate(), FALSE, item );
		}
	}

	//-----------------------------------------------------------------------------------------------
	void findYard( Object *obj, void * )
	{
		if( obj != nullptr && !obj->isEffectivelyDead() && !obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION )
			&& BaseConstructionUpdate::find( obj ) != nullptr )
			s_hasYard = TRUE;
	}

	//-----------------------------------------------------------------------------------------------
	void collectObject( Object *obj, void * )
	{
		if( obj == nullptr || obj->isEffectivelyDead() || obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION )
			|| obj->testStatus( OBJECT_STATUS_SOLD ) || obj->isContained() )
			return;
		BaseConstructionUpdate *base = BaseConstructionUpdate::find( obj );
		if( base != nullptr )
			collectYard( obj, base );
		if( obj->getProductionUpdateInterface() == nullptr && !obj->isKindOf( KINDOF_DOZER ) )
			return;
		const CommandSet *set = TheControlBar->findCommandSet( obj->getCommandSetString() );
		collectCommandSet( obj, set, 0 );
	}

	//-----------------------------------------------------------------------------------------------
	void refresh()
	{
		for( Int t = 0; t < TAB_COUNT; ++t )
			s_queues[t].clear();
		Player *me = ThePlayerList->getLocalPlayer();
		if( me == nullptr )
			return;
		s_hasYard = FALSE;
		me->iterateObjects( findYard, nullptr );
		me->iterateObjects( collectObject, nullptr );
	}

	//-----------------------------------------------------------------------------------------------
	/** how many of the item the queue's producer has queued, and the first one's progress (or -1) */
	void queued( const SidebarQueue &q, const SidebarItem &item, Int *count, Real *percent )
	{
		*count = 0;
		*percent = -1.0f;
		Object *producer = TheGameLogic->findObjectByID( q.producer );
		BaseConstructionUpdate *base = BaseConstructionUpdate::find( producer );
		if( base != nullptr && item.thing != nullptr && base->builds( item.thing ) )
		{
			if( base->getBuilding() == item.thing )
			{
				*count = 1;
				*percent = base->getPercent();
			}
			return;
		}
		ProductionUpdateInterface *pu = producer != nullptr ? producer->getProductionUpdateInterface() : nullptr;
		if( pu == nullptr )
			return;
		Bool first = TRUE;
		for( const ProductionEntry *e = pu->firstProduction(); e != nullptr; e = pu->nextProduction( e ) )
		{
			const Bool same = item.thing != nullptr ? e->getProductionObject() == item.thing : e->getProductionUpgrade() == item.upgrade;
			if( same )
			{
				++*count;
				if( first )
					*percent = e->getPercentComplete();
			}
			first = FALSE;
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** whether the item can be had now (its prerequisites; money aside) */
	Bool available( const SidebarQueue &q, const SidebarItem &item )
	{
		Object *producer = TheGameLogic->findObjectByID( q.producer );
		Player *me = ThePlayerList->getLocalPlayer();
		if( producer == nullptr || me == nullptr )
			return FALSE;
		if( item.upgrade != nullptr )
			return !me->hasUpgradeComplete( item.upgrade ) && !me->hasUpgradeInProduction( item.upgrade )
				&& TheUpgradeCenter->canAffordUpgrade( me, item.upgrade, FALSE );
		// a construction yard's: by the yard's own list (its command set is not the yard's)
		const BaseConstructionUpdate *yard = BaseConstructionUpdate::find( producer );
		if( yard != nullptr && yard->builds( item.thing ) )
			return yard->canBuildNow( item.thing );
		return TheBuildAssistant->isPossibleToMakeUnit( producer, item.thing );
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
			s_label = TheDisplayStringManager->newDisplayString();
			s_small = TheDisplayStringManager->newDisplayString();
			s_info = TheDisplayStringManager->newDisplayString();
		}
		const Int big = max( 11, sh / 60 ), small = max( 9, sh / 80 );
		s_label->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), big, TRUE ) );
		s_small->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), small, TRUE ) );
		s_info->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), small, FALSE ) );
	}

	//-----------------------------------------------------------------------------------------------
	void drawText( DisplayString *s, const UnicodeString &text, Int x, Int y, Color color )
	{
		s->setText( text );
		s->draw( x, y, color, GameMakeColor( 0, 0, 0, 200 ) );
	}

	//-----------------------------------------------------------------------------------------------
	UnicodeString labelOf( const SidebarItem &item )
	{
		if( item.button != nullptr && item.button->getTextLabel().isNotEmpty() )
			return TheGameText->fetch( item.button->getTextLabel() );
		if( item.thing != nullptr )
			return item.thing->getDisplayName();
		return UnicodeString::TheEmptyString;
	}

	//-----------------------------------------------------------------------------------------------
	UnicodeString queueLabel( const SidebarQueue &q )
	{
		UnicodeString name = q.kind != nullptr ? q.kind->getDisplayName() : UnicodeString::TheEmptyString;
		if( name.isEmpty() && q.kind != nullptr )
			name.translate( q.kind->getName() );
		return name;
	}

	//-----------------------------------------------------------------------------------------------
	void drawSidebar( GameWindow *, WinInstanceData * )
	{
		if( TheDisplay == nullptr || ThePlayerList == nullptr || ThePlayerList->getLocalPlayer() == nullptr )
			return;
		ensureStrings();
		SidebarLayout L;
		computeLayout( &L );
		Color accent, accentDim;
		sideColors( &accent, &accentDim );
		const Color panel = GameMakeColor( 14, 18, 22, 225 ), panelEdge = GameMakeColor( 60, 70, 78, 255 );
		const Color text = GameMakeColor( 230, 236, 240, 255 ), textDim = GameMakeColor( 140, 150, 158, 255 );

		TheDisplay->drawFillRect( L.x, L.y, L.w, L.h, panel );
		TheDisplay->drawFillRect( L.x, L.y, 2, L.h, accent );
		TheDisplay->drawOpenRect( L.x, L.y, L.w, L.h, 1.0f, panelEdge );

		// the header: money and power
		Player *me = ThePlayerList->getLocalPlayer();
		UnicodeString s;
		s.format( L"$ %u", me->getMoney()->countMoney() );
		drawText( s_label, s, L.x + L.gap * 3, L.y + L.gap, GameMakeColor( 255, 214, 90, 255 ) );
		const Int produced = me->getEnergy()->getProduction(), used = me->getEnergy()->getConsumption();
		s.format( L"Power %d / %d", used, produced );
		drawText( s_small, s, L.x + L.w / 2, L.y + L.gap + 2, used > produced ? GameMakeColor( 255, 80, 60, 255 ) : textDim );

		// the tabs
		const Int tabW = ( L.w - L.gap ) / TAB_COUNT;
		for( Int t = 0; t < TAB_COUNT; ++t )
		{
			const Int tx = L.x + L.gap / 2 + t * tabW;
			const Bool on = t == s_tab;
			TheDisplay->drawFillRect( tx + 1, L.tabsY, tabW - 2, L.tabsH, on ? accentDim : GameMakeColor( 30, 36, 42, 230 ) );
			if( on )
				TheDisplay->drawFillRect( tx + 1, L.tabsY + L.tabsH - 3, tabW - 2, 3, accent );
			UnicodeString name;
			name.translate( AsciiString( TAB_NAMES[t] ) );
			s_small->setText( name );
			Int w = 0, h = 0;
			s_small->getSize( &w, &h );
			const Color c = s_queues[t].empty() ? GameMakeColor( 80, 86, 92, 255 ) : ( on ? text : textDim );
			drawText( s_small, name, tx + ( tabW - w ) / 2, L.tabsY + ( L.tabsH - h ) / 2, c );
		}

		// the queues (producers) of this tab
		std::vector<SidebarQueue> &qs = s_queues[s_tab];
		SidebarQueue *cur = currentQueue();
		const Int chipW = ( L.w - L.gap * ( L.chipsPerRow + 1 ) ) / L.chipsPerRow;
		for( size_t i = 0; i < qs.size(); ++i )
		{
			const Int row = (Int)i / L.chipsPerRow, col = (Int)i % L.chipsPerRow;
			const Int cx = L.x + L.gap + col * ( chipW + L.gap ), cy = L.chipsY + row * ( L.chipH + L.gap );
			const Bool on = &qs[i] == cur;
			TheDisplay->drawFillRect( cx, cy, chipW, L.chipH, on ? accentDim : GameMakeColor( 26, 31, 36, 230 ) );
			TheDisplay->drawOpenRect( cx, cy, chipW, L.chipH, 1.0f, on ? accent : panelEdge );
			UnicodeString name = queueLabel( qs[i] );
			// how busy it is
			Object *producer = TheGameLogic->findObjectByID( qs[i].producer );
			ProductionUpdateInterface *pu = producer != nullptr ? producer->getProductionUpdateInterface() : nullptr;
			if( pu != nullptr && pu->getProductionCount() > 0 )
			{
				UnicodeString n;
				n.format( L" (%d)", (Int)pu->getProductionCount() );
				name.concat( n );
			}
			drawText( s_small, name, cx + 4, cy + 2, on ? text : textDim );
		}

		// the items
		const Int gy = gridTop( L );
		const Int rowsFit = max( 1, ( L.infoY - gy - L.gap ) / ( L.cellH + L.gap ) );
		const Color cellBack = GameMakeColor( 22, 26, 30, 255 );
		if( cur != nullptr )
		{
			for( size_t i = 0; i < cur->items.size() && (Int)i < rowsFit * L.cols; ++i )
			{
				const SidebarItem &item = cur->items[i];
				const Int row = (Int)i / L.cols, col = (Int)i % L.cols;
				const Int ix = L.x + L.gap + col * ( L.cellW + L.gap ), iy = gy + row * ( L.cellH + L.gap );
				const Bool ok = available( *cur, item );
				TheDisplay->drawFillRect( ix, iy, L.cellW, L.cellH, cellBack );
				const Image *image = item.button != nullptr ? item.button->getButtonImage() : nullptr;
				if( image != nullptr )
					TheDisplay->drawImage( image, ix + 1, iy + 1, ix + L.cellW - 1, iy + L.cellH - 1,
						ok ? 0xFFFFFFFF : GameMakeColor( 110, 110, 110, 255 ) );
				else
					drawText( s_small, labelOf( item ), ix + 3, iy + 3, ok ? text : textDim );
				Int count = 0;
				Real percent = -1.0f;
				queued( *cur, item, &count, &percent );
				if( percent >= 0.0f )
				{
					// the time left, a clock over what is still to come
					TheDisplay->drawRemainingRectClock( ix + 1, iy + 1, L.cellW - 2, L.cellH - 2, (Int)percent, GameMakeColor( 0, 0, 0, 150 ) );
				}
				BaseConstructionUpdate *yard = BaseConstructionUpdate::find( TheGameLogic->findObjectByID( cur->producer ) );
				if( yard != nullptr && yard->isReady() && yard->getBuilding() == item.thing )
				{
					// ready to place: Kane's Wrath's flashing label
					UnicodeString ready;
					ready.translate( AsciiString( "READY" ) );
					if( ( TheGameLogic->getFrame() / 10 ) & 1 )
						drawText( s_label, ready, ix + 4, iy + L.cellH / 2 - 8, GameMakeColor( 120, 255, 120, 255 ) );
				}
				else if( count > 0 && yard == nullptr )
				{
					UnicodeString n;
					n.format( L"%d", count );
					drawText( s_label, n, ix + L.cellW - 14, iy + 2, GameMakeColor( 255, 230, 120, 255 ) );
				}
				const Bool hover = (Int)i == s_hoverItem;
				TheDisplay->drawOpenRect( ix, iy, L.cellW, L.cellH, hover ? 2.0f : 1.0f, hover ? accent : panelEdge );
			}
		}
		else
		{
			UnicodeString none;
			none.translate( AsciiString( s_tab == TAB_STRUCTURES || s_tab == TAB_DEFENSES ? "No builders" : "Nothing builds these" ) );
			drawText( s_small, none, L.x + L.gap * 2, gy + L.gap, textDim );
		}

		// what the cursor is on: its name, cost and time
		TheDisplay->drawFillRect( L.x + L.gap, L.infoY, L.w - 2 * L.gap, L.infoH - L.gap, GameMakeColor( 24, 28, 33, 240 ) );
		TheDisplay->drawFillRect( L.x + L.gap, L.infoY, L.w - 2 * L.gap, 2, accentDim );
		if( cur != nullptr && s_hoverItem >= 0 && s_hoverItem < (Int)cur->items.size() )
		{
			const SidebarItem &item = cur->items[s_hoverItem];
			drawText( s_label, labelOf( item ), L.x + L.gap * 2, L.infoY + 4, text );
			Int cost = 0, frames = 0;
			if( item.thing != nullptr )
			{
				cost = item.thing->calcCostToBuild( me );
				frames = item.thing->calcTimeToBuild( me );
			}
			else if( item.upgrade != nullptr )
			{
				cost = item.upgrade->calcCostToBuild( me );
				frames = item.upgrade->calcTimeToBuild( me );
			}
			UnicodeString line;
			line.format( L"$%d    %d s    left click: build, right click: cancel", cost, frames / LOGICFRAMES_PER_SECOND );
			drawText( s_info, line, L.x + L.gap * 2, L.infoY + L.infoH / 2, textDim );
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** what is under the point: a tab (0..), a queue chip (100..), an item (200..), or -1 */
	Int hitTest( Int px, Int py )
	{
		SidebarLayout L;
		computeLayout( &L );
		if( px < L.x || py < L.y || py >= L.y + L.h )
			return -1;
		if( py >= L.tabsY && py < L.tabsY + L.tabsH )
		{
			const Int tabW = ( L.w - L.gap ) / TAB_COUNT;
			const Int t = ( px - L.x - L.gap / 2 ) / max( 1, tabW );
			return t >= 0 && t < TAB_COUNT ? t : -1;
		}
		const std::vector<SidebarQueue> &qs = s_queues[s_tab];
		const Int chipW = ( L.w - L.gap * ( L.chipsPerRow + 1 ) ) / L.chipsPerRow;
		for( size_t i = 0; i < qs.size(); ++i )
		{
			const Int row = (Int)i / L.chipsPerRow, col = (Int)i % L.chipsPerRow;
			const Int cx = L.x + L.gap + col * ( chipW + L.gap ), cy = L.chipsY + row * ( L.chipH + L.gap );
			if( px >= cx && px < cx + chipW && py >= cy && py < cy + L.chipH )
				return 100 + (Int)i;
		}
		const SidebarQueue *cur = currentQueue();
		if( cur == nullptr )
			return -1;
		const Int gy = gridTop( L );
		for( size_t i = 0; i < cur->items.size(); ++i )
		{
			const Int row = (Int)i / L.cols, col = (Int)i % L.cols;
			const Int ix = L.x + L.gap + col * ( L.cellW + L.gap ), iy = gy + row * ( L.cellH + L.gap );
			if( iy + L.cellH > L.infoY )
				break;
			if( px >= ix && px < ix + L.cellW && py >= iy && py < iy + L.cellH )
				return 200 + (Int)i;
		}
		return -1;
	}

	//-----------------------------------------------------------------------------------------------
	/** the builder for a structure: the selected one of the queue's kind, else the nearest to the view */
	Object *pickBuilder( const SidebarQueue &q )
	{
		const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();
		for( DrawableList::const_iterator it = selected->begin(); it != selected->end(); ++it )
		{
			Object *o = (*it)->getObject();
			if( o != nullptr && o->getTemplate()->isEquivalentTo( q.kind ) && !o->isEffectivelyDead() )
				return o;
		}
		struct Nearest { const ThingTemplate *kind; Coord3D at; Object *best; Real bestDist;
			static void each( Object *o, void *data )
			{
				Nearest *n = (Nearest *)data;
				if( o->isEffectivelyDead() || o->isContained() || !o->getTemplate()->isEquivalentTo( n->kind ) )
					return;
				const Real dx = o->getPosition()->x - n->at.x, dy = o->getPosition()->y - n->at.y;
				const Real d = dx * dx + dy * dy;
				if( n->best == nullptr || d < n->bestDist )
				{
					n->best = o;
					n->bestDist = d;
				}
			} };
		Nearest n = { q.kind, TheTacticalView->getPosition(), nullptr, 0.0f };
		ThePlayerList->getLocalPlayer()->iterateObjects( Nearest::each, &n );
		return n.best;
	}

	//-----------------------------------------------------------------------------------------------
	void clickItem( SidebarQueue &q, const SidebarItem &item, Bool cancel )
	{
		Player *me = ThePlayerList->getLocalPlayer();
		Object *producer = TheGameLogic->findObjectByID( q.producer );
		if( me == nullptr )
			return;
		// a construction yard's structure (Kane's Wrath): build it, then place it once ready
		BaseConstructionUpdate *yard = BaseConstructionUpdate::find( producer );
		if( yard != nullptr && item.thing != nullptr && yard->builds( item.thing ) )
		{
			if( cancel )
			{
				if( yard->getBuilding() == item.thing )
				{
					GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_TIBERIUM_BASE_CANCEL );
					msg->appendObjectIDArgument( producer->getID() );
				}
				return;
			}
			if( yard->isReady() && yard->getBuilding() == item.thing && producer->getDrawable() != nullptr )
			{
				TheInGameUI->placeBuildAvailable( item.thing, producer->getDrawable() );
				return;
			}
			if( yard->getBuilding() != nullptr || !yard->canBuildNow( item.thing ) )
				return;		// one at a time, and what its owner may have
			if( me->getMoney()->countMoney() < (UnsignedInt)item.thing->calcCostToBuild( me ) )
			{
				TheInGameUI->message( "GUI:NotEnoughMoneyToBuild" );
				return;
			}
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_TIBERIUM_BASE_QUEUE );
			msg->appendObjectIDArgument( producer->getID() );
			msg->appendIntegerArgument( item.thing->getTemplateID() );
			return;
		}
		// a structure: a builder places it (Zero Hour's builders walk there and build)
		if( item.thing != nullptr && ( q.kind != nullptr && q.kind->isKindOf( KINDOF_DOZER ) ) )
		{
			if( cancel )
				return;
			Object *builder = pickBuilder( q );
			if( builder == nullptr || builder->getDrawable() == nullptr )
				return;
			const CanMakeType cmt = TheBuildAssistant->canMakeUnit( builder, item.thing );
			if( cmt == CANMAKE_NO_MONEY )
			{
				TheInGameUI->message( "GUI:NotEnoughMoneyToBuild" );
				return;
			}
			if( cmt != CANMAKE_OK )
				return;
			TheInGameUI->deselectAllDrawables();
			TheInGameUI->selectDrawable( builder->getDrawable() );
			GameMessage *sel = TheMessageStream->appendMessage( GameMessage::MSG_CREATE_SELECTED_GROUP_NO_SOUND );
			sel->appendBooleanArgument( TRUE );
			sel->appendObjectIDArgument( builder->getID() );
			TheInGameUI->placeBuildAvailable( item.thing, builder->getDrawable() );
			return;
		}
		if( producer == nullptr )
			return;
		ProductionUpdateInterface *pu = producer->getProductionUpdateInterface();
		if( pu == nullptr )
			return;
		if( item.upgrade != nullptr )
		{
			if( cancel )
			{
				GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_CANCEL_UPGRADE );
				msg->appendIntegerArgument( item.upgrade->getUpgradeNameKey() );
				msg->appendObjectIDArgument( producer->getID() );		// Tiberium Project: the producer, selected or not
				return;
			}
			if( !TheUpgradeCenter->canAffordUpgrade( me, item.upgrade, TRUE ) || pu->canQueueUpgrade( item.upgrade ) != CANMAKE_OK )
				return;
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_QUEUE_UPGRADE );
			msg->appendObjectIDArgument( producer->getID() );
			msg->appendIntegerArgument( item.upgrade->getUpgradeNameKey() );
			return;
		}
		if( cancel )
		{
			// the last of it queued
			const ProductionEntry *last = nullptr;
			for( const ProductionEntry *e = pu->firstProduction(); e != nullptr; e = pu->nextProduction( e ) )
				if( e->getProductionObject() == item.thing )
					last = e;
			if( last != nullptr )
			{
				GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_CANCEL_UNIT_CREATE );
				msg->appendIntegerArgument( last->getProductionID() );
				msg->appendObjectIDArgument( producer->getID() );		// Tiberium Project
			}
			return;
		}
		const CanMakeType cmt = TheBuildAssistant->canMakeUnit( producer, item.thing );
		if( cmt == CANMAKE_NO_MONEY )
		{
			TheInGameUI->message( "GUI:NotEnoughMoneyToBuild" );
			return;
		}
		if( cmt == CANMAKE_QUEUE_FULL )
		{
			TheInGameUI->message( "GUI:ProductionQueueFull" );
			return;
		}
		if( cmt != CANMAKE_OK )
			return;
		GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_QUEUE_UNIT_CREATE );
		msg->appendIntegerArgument( item.thing->getTemplateID() );
		msg->appendIntegerArgument( pu->requestUniqueUnitID() );
		msg->appendObjectIDArgument( producer->getID() );		// Tiberium Project: the producer, selected or not
	}

	//-----------------------------------------------------------------------------------------------
	WindowMsgHandledType inputSidebar( GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData )
	{
		const Int px = (Short)( mData1 & 0xFFFF ), py = (Short)( ( mData1 >> 16 ) & 0xFFFF );
		switch( msg )
		{
			case GWM_MOUSE_POS:
			{
				const Int hit = hitTest( px, py );
				s_hoverItem = hit >= 200 ? hit - 200 : -1;
				return MSG_HANDLED;
			}
			case GWM_MOUSE_LEAVING:
				s_hoverItem = -1;
				return MSG_HANDLED;
			case GWM_LEFT_DOWN:
			case GWM_RIGHT_DOWN:
			{
				const Int hit = hitTest( px, py );
				if( hit >= 0 && hit < TAB_COUNT )
					s_tab = hit;
				else if( hit >= 100 && hit < 200 && hit - 100 < (Int)s_queues[s_tab].size() )
				{
					s_queueOf[s_tab] = s_queues[s_tab][hit - 100].producer;
					s_queueKindOf[s_tab] = s_queues[s_tab][hit - 100].kind;
				}
				else if( hit >= 200 )
				{
					SidebarQueue *q = currentQueue();
					if( q != nullptr && hit - 200 < (Int)q->items.size() )
						clickItem( *q, q->items[hit - 200], msg == GWM_RIGHT_DOWN );
				}
				return MSG_HANDLED;
			}
			case GWM_LEFT_UP:
			case GWM_RIGHT_UP:
			case GWM_WHEEL_UP:
			case GWM_WHEEL_DOWN:
				return MSG_HANDLED;
			default:
				return MSG_IGNORED;
		}
	}
}

//-------------------------------------------------------------------------------------------------
Int TiberiumSidebar::getWidth()
{
	if( s_window == nullptr || TheDisplay == nullptr )
		return 0;
	SidebarLayout L;
	computeLayout( &L );
	return L.w;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumSidebar::testClick( Real fx, Real fy, Bool right )
{
	if( s_window == nullptr || TheDisplay == nullptr )
		return FALSE;
	const Int x = (Int)( fx * TheDisplay->getWidth() ), y = (Int)( fy * TheDisplay->getHeight() );
	const WindowMsgData packed = (WindowMsgData)( ( x & 0xFFFF ) | ( ( y & 0xFFFF ) << 16 ) );
	inputSidebar( s_window, GWM_MOUSE_POS, packed, 0 );
	return inputSidebar( s_window, right ? GWM_RIGHT_DOWN : GWM_LEFT_DOWN, packed, 0 ) == MSG_HANDLED && hitTest( x, y ) >= 0;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumSidebar::testClickPart( const char *part, Int index, Bool right )
{
	if( s_window == nullptr || TheDisplay == nullptr || part == nullptr || index < 0 )
		return FALSE;
	// the middle of the part, as hitTest lays it out
	SidebarLayout L;
	computeLayout( &L );
	Int x = -1, y = -1;
	if( strcmp( part, "tab" ) == 0 && index < TAB_COUNT )
	{
		const Int tabW = ( L.w - L.gap ) / TAB_COUNT;
		x = L.x + L.gap / 2 + index * tabW + tabW / 2;
		y = L.tabsY + L.tabsH / 2;
	}
	else if( strcmp( part, "queue" ) == 0 && index < (Int)s_queues[s_tab].size() )
	{
		const Int chipW = ( L.w - L.gap * ( L.chipsPerRow + 1 ) ) / L.chipsPerRow;
		x = L.x + L.gap + ( index % L.chipsPerRow ) * ( chipW + L.gap ) + chipW / 2;
		y = L.chipsY + ( index / L.chipsPerRow ) * ( L.chipH + L.gap ) + L.chipH / 2;
	}
	else if( strcmp( part, "item" ) == 0 )
	{
		const SidebarQueue *cur = currentQueue();
		if( cur == nullptr || index >= (Int)cur->items.size() )
			return FALSE;
		x = L.x + L.gap + ( index % L.cols ) * ( L.cellW + L.gap ) + L.cellW / 2;
		y = gridTop( L ) + ( index / L.cols ) * ( L.cellH + L.gap ) + L.cellH / 2;
	}
	if( x < 0 )
		return FALSE;
	return testClick( (Real)x / TheDisplay->getWidth(), (Real)y / TheDisplay->getHeight(), right );
}

//-------------------------------------------------------------------------------------------------
void TiberiumSidebar::describe( char *buffer, Int size )
{
	const SidebarQueue *cur = s_window != nullptr ? currentQueue() : nullptr;
	const ThingTemplate *placing = TheInGameUI != nullptr ? TheInGameUI->getPendingPlaceType() : nullptr;
	snprintf( buffer, size, "tab %s, %d queues, current %s with %d items%s (max camera %.0f); placing %s, %d build areas drawn", TAB_NAMES[s_tab], (Int)s_queues[s_tab].size(),
		cur != nullptr && cur->kind != nullptr ? cur->kind->getName().str() : "none", cur != nullptr ? (Int)cur->items.size() : 0,
		s_window == nullptr ? ( TheGlobalData != nullptr && TheGlobalData->m_tiberiumSidebar ? " (off: not made)" : " (off: GameData does not ask for it)" ) : "",
		TheGlobalData != nullptr ? TheGlobalData->m_maxCameraHeight : 0.0f, placing != nullptr ? placing->getName().str() : "nothing",
		(Int)s_areaDecals.size() );
}

//-------------------------------------------------------------------------------------------------
void TiberiumSidebar::parseBuildAreaCursor( INI *ini, void *instance, void *store, const void *userData )
{
	RadiusDecalTemplate::parseRadiusDecalTemplate( ini, instance, &s_areaCursor, userData );
}

//-------------------------------------------------------------------------------------------------
Int TiberiumSidebar::getBuildAreasDrawn()
{
	return (Int)s_areaDecals.size();
}

//-------------------------------------------------------------------------------------------------
void TiberiumSidebar::update()
{
	if( TheGlobalData == nullptr || !TheGlobalData->m_tiberiumSidebar || TheGameLogic == nullptr || !TheGameLogic->isInGame()
		|| TheGameLogic->isInShellGame() || TheWindowManager == nullptr || TheDisplay == nullptr || TheControlBar == nullptr )
	{
		reset();
		return;
	}
	SidebarLayout L;
	computeLayout( &L );
	if( s_window == nullptr )
	{
		for( Int t = 0; t < TAB_COUNT; ++t )
		{
			s_queueOf[t] = INVALID_ID;
			s_queueKindOf[t] = nullptr;
		}
		s_window = TheWindowManager->winCreate( nullptr, WIN_STATUS_ENABLED | WIN_STATUS_ABOVE | WIN_STATUS_RIGHT_CLICK,
			L.x, L.y, L.w, L.h, nullptr, nullptr );
		if( s_window == nullptr )
			return;
		s_window->winSetDrawFunc( drawSidebar );
		s_window->winSetInputFunc( inputSidebar );
		s_lastRefresh = 0;
	}
	else
	{
		// the screen may have changed size
		Int x = 0, y = 0, w = 0, h = 0;
		s_window->winGetScreenPosition( &x, &y );
		s_window->winGetSize( &w, &h );
		if( x != L.x || y != L.y || w != L.w || h != L.h )
		{
			s_window->winSetPosition( L.x, L.y );
			s_window->winSetSize( L.w, L.h );
		}
	}
	updateBuildAreas();
	const UnsignedInt now = TheGameLogic->getFrame();
	if( s_lastRefresh == 0 || now >= s_lastRefresh + 10 || now < s_lastRefresh )
	{
		refresh();
		s_lastRefresh = now == 0 ? 1 : now;
		// a tab with nothing: the first that has something
		if( s_queues[s_tab].empty() )
		{
			for( Int t = 0; t < TAB_COUNT; ++t )
			{
				if( !s_queues[t].empty() )
				{
					s_tab = t;
					break;
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumSidebar::reset()
{
	clearBuildAreas();
	if( s_window != nullptr && TheWindowManager != nullptr )
		TheWindowManager->winDestroy( s_window );
	s_window = nullptr;
	for( Int t = 0; t < TAB_COUNT; ++t )
		s_queues[t].clear();
	s_hoverItem = -1;
	if( s_label != nullptr && TheDisplayStringManager != nullptr )
	{
		TheDisplayStringManager->freeDisplayString( s_label );
		TheDisplayStringManager->freeDisplayString( s_small );
		TheDisplayStringManager->freeDisplayString( s_info );
	}
	s_label = s_small = s_info = nullptr;
	s_fontScreenHeight = 0;
}
