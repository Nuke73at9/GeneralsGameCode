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

// FILE: TiberiumStartView.cpp ////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's match start and redeployment on screen (see TiberiumStartView.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/TiberiumStartView.h"

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingTemplate.h"
#include "Common/PlayerTemplate.h"
#include "Common/TiberiumTest.h"
#include "Common/Upgrade.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Image.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Mouse.h"
#include "GameClient/TiberiumSettings.h"
#include "GameClient/TiberiumSkin.h"
#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/TiberiumStart.h"

#include <math.h>
#include <stdlib.h>
#include <wctype.h>
#include <string>
#include <vector>

using namespace TiberiumSkin;

namespace
{
	enum Mode
	{
		MODE_NONE,
		MODE_BOX,			///< SELECT YOUR CLASS is up
		MODE_PLACE		///< the marker follows the mouse
	};

	enum TopButton
	{
		TOP_CLASS,				///< the start: the class picked; opens the box
		TOP_DEPLOY,				///< after it: call another Crawler
		TOP_DECOMMISSION,	///< the player's Crawler, selected
		TOP_CANCEL,				///< a redeployment's marker put away
		TOP_POWER,				///< its Crawler power (the Tiberium factions' Tier IV)
		TOP_TIER					///< the Crawler power's upgrade, researched
	};

	const UnsignedInt HOLD_FRAMES = LOGICFRAMES_PER_SECOND;						///< C&C4's CenterHoldDuration
	const Int PAN_MS = 5000;																						///< and its CameraPanDuration
	const UnsignedInt BANNER_FRAMES = LOGICFRAMES_PER_SECOND * 5 / 2;	///< DEPLOY! up this long
	const UnsignedInt SENT_FRAMES = LOGICFRAMES_PER_SECOND * 3;				///< a spot sent is shown this long before the logic's
	const UnsignedInt CONFIRM_FRAMES = LOGICFRAMES_PER_SECOND * 3;		///< DECOMMISSION's second click within this

	Mode s_mode = MODE_NONE;
	Bool s_redeploy = FALSE;					///< the box and the marker call a Crawler (after the start)
	Int s_list = TiberiumStart::LIST_START;	///< whose list: the start's, or the Crawler power's
	Int s_class = -1;									///< the card picked (a redeployment's; the start's goes to the logic at once)
	Int s_hoverCard = -1;
	Bool s_hoverCancel = FALSE;
	Int s_hoverTop = -1;
	GameWindow *s_box = nullptr;
	GameWindow *s_top = nullptr;
	Bool s_boxOpened = FALSE;					///< the start's box was opened at the intro's end
	Int s_intro = 0;									///< 0: not begun, 1: held on the middle, 2: panning, 3: done
	UnsignedInt s_introFrame = 0;
	Bool s_sent = FALSE;							///< a spot sent, not yet the logic's
	Coord3D s_sentSpot;
	UnsignedInt s_sentFrame = 0;
	UnsignedInt s_confirmUntil = 0;
	ICoord2D s_rightDown;
	Bool s_active = FALSE;						///< anything of this is up
	DisplayString *s_count = nullptr;	///< the countdown's digits (a new text each frame: not TiberiumSkin's kept ones)
	Int s_countSize = 0;

	//-----------------------------------------------------------------------------------------------
	Player *localPlayer()
	{
		return ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
	}

	Bool isNod( const Player *p )
	{
		return p != nullptr && ( p->getSide().compareNoCase( "China" ) == 0 || p->getBaseSide().compareNoCase( "China" ) == 0 );
	}

	Look look()
	{
		return lookOf( isNod( localPlayer() ) ? TiberiumSettings::STYLE_NOD : TiberiumSettings::STYLE_GDI );
	}

	Real scale()
	{
		return TheDisplay != nullptr ? (Real)TheDisplay->getHeight() / 720.0f : 1.0f;
	}

	Int px( Real v )
	{
		return max( 1, (Int)( v * scale() + 0.5f ) );
	}

	UnicodeString upper( const UnicodeString &text )
	{
		UnicodeString out;
		for( Int i = 0; i < text.getLength(); ++i )
			out.concat( (WideChar)towupper( text.getCharAt( i ) ) );
		return out;
	}

	UnicodeString nameOf( const ThingTemplate *t )
	{
		if( t == nullptr )
			return UnicodeString::TheEmptyString;
		if( !t->getDisplayName().isEmpty() )
			return t->getDisplayName();
		UnicodeString n;
		n.translate( t->getName() );
		return n;
	}

	const Image *portraitOf( const ThingTemplate *t )
	{
		if( t == nullptr )
			return nullptr;
		const Image *image = t->getSelectedPortraitImage();
		return image != nullptr ? image : t->getButtonImage();
	}

	UnsignedInt now()
	{
		return TheGameLogic != nullptr ? TheGameLogic->getFrame() : 0;
	}

	/// the upgrade that gives the local player its Crawler power
	const UpgradeTemplate *tierUpgrade()
	{
		const Player *me = localPlayer();
		const PlayerTemplate *pt = me != nullptr ? me->getPlayerTemplate() : nullptr;
		if( pt == nullptr || pt->getCrawlerPowerUpgrade().isEmpty() || TheUpgradeCenter == nullptr )
			return nullptr;
		return TheUpgradeCenter->findUpgrade( pt->getCrawlerPowerUpgrade() );
	}

	UnicodeString tierName()
	{
		const UpgradeTemplate *up = tierUpgrade();
		if( up != nullptr && TheGameText != nullptr && up->getDisplayNameLabel().isNotEmpty() )
			return upper( TheGameText->fetchOrSubstitute( up->getDisplayNameLabel().str(), L"Tier IV" ) );
		return UnicodeString( L"TIER IV" );
	}

	/// the player's own Crawler, the only one selected
	Object *selectedCrawler()
	{
		if( TheInGameUI == nullptr || TheInGameUI->getSelectCount() != 1 )
			return nullptr;
		Drawable *d = TheInGameUI->getFirstSelectedDrawable();
		Object *obj = d != nullptr ? d->getObject() : nullptr;
		return obj != nullptr && obj->getControllingPlayer() == localPlayer() && TiberiumStart::isCrawler( obj ) ? obj : nullptr;
	}

	//-----------------------------------------------------------------------------------------------
	void sendPick( Int choice, const Coord3D *spot )
	{
		if( TheMessageStream == nullptr )
			return;
		Coord3D at;
		at.zero();
		if( spot != nullptr )
			at = *spot;
		GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_TIBERIUM_CRAWLER_PICK );
		msg->appendIntegerArgument( choice );
		msg->appendLocationArgument( at );
		msg->appendIntegerArgument( spot != nullptr ? 1 : 0 );
		msg->appendIntegerArgument( s_redeploy ? s_list : (Int)TiberiumStart::LIST_START );
		if( spot != nullptr && !s_redeploy )
		{
			s_sent = TRUE;
			s_sentSpot = at;
			s_sentFrame = now();
		}
		if( TiberiumTest::isActive() )
			TiberiumTest::log( "frame %u: start view: pick %d sent%s (%.0f %.0f)", now(), (int)choice, spot != nullptr ? " with a spot" : "", at.x, at.y );
	}

	void openBox( Bool redeploy, Int list )
	{
		s_mode = MODE_BOX;
		s_redeploy = redeploy;
		s_list = redeploy ? list : (Int)TiberiumStart::LIST_START;
		s_hoverCard = -1;
		s_hoverCancel = FALSE;
	}

	void closeAll()
	{
		s_mode = MODE_NONE;
		s_redeploy = FALSE;
		s_list = TiberiumStart::LIST_START;
		s_class = -1;
	}

	/// a card of the box picked
	void pickCard( Int i )
	{
		Player *me = localPlayer();
		if( i < 0 || i >= TiberiumStart::getChoiceCount( me, s_list ) )
			return;
		s_class = i;
		if( s_redeploy )
		{
			s_mode = MODE_PLACE;
			return;
		}
		sendPick( i, nullptr );
		Coord3D spot;
		s_mode = ( TiberiumStart::getSpot( me, &spot ) || s_sent ) ? MODE_NONE : MODE_PLACE;
	}

	/// a click on the ground with the marker: the start's spot, or a Crawler called there
	void placeAt( const Coord3D &world )
	{
		Player *me = localPlayer();
		Coord3D spot = world;
		if( TheTerrainLogic != nullptr )
			spot.z = TheTerrainLogic->getGroundHeight( spot.x, spot.y );
		if( !TiberiumStart::isSpotAllowed( me, &spot, nullptr ) )
			return;
		if( s_redeploy )
		{
			sendPick( s_class, &spot );
			closeAll();
			return;
		}
		sendPick( -1, &spot );
		s_mode = MODE_NONE;
	}

	//-----------------------------------------------------------------------------------------------
	// the box's layout
	struct BoxLayout
	{
		Real u;
		Int x, y, w, h;
		Int count;
		Int cardX[TiberiumStart::MOST_CHOICES], cardY, cardW, cardH;
		Int titleH, lineY;
		Int cancelX, cancelY, cancelW, cancelH;
	};

	void boxLayout( BoxLayout *B )
	{
		const Real u = scale();
		B->u = u;
		B->count = max( 1, TiberiumStart::getChoiceCount( localPlayer(), s_list ) );
		const Int pad = px( 14.0f ), gap = px( 10.0f );
		B->cardW = px( 124.0f );
		B->cardH = px( 150.0f );
		B->titleH = px( 26.0f );
		B->w = pad * 2 + B->count * B->cardW + ( B->count - 1 ) * gap;
		B->cancelW = px( 130.0f );
		B->cancelH = px( 28.0f );
		B->h = B->titleH + pad + B->cardH + px( 8.0f ) + px( 18.0f ) + px( 10.0f ) + B->cancelH + pad;
		const Int sw = TheDisplay->getWidth();
		B->x = ( sw - B->w ) / 2;
		B->y = px( 84.0f );
		B->cardY = B->y + B->titleH + pad;
		for( Int i = 0; i < B->count && i < TiberiumStart::MOST_CHOICES; ++i )
			B->cardX[i] = B->x + pad + i * ( B->cardW + gap );
		B->lineY = B->cardY + B->cardH + px( 8.0f );
		B->cancelX = B->x + ( B->w - B->cancelW ) / 2;
		B->cancelY = B->lineY + px( 18.0f ) + px( 10.0f );
	}

	Int cardAt( const BoxLayout &B, Int mx, Int my )
	{
		for( Int i = 0; i < B.count && i < TiberiumStart::MOST_CHOICES; ++i )
			if( mx >= B.cardX[i] && mx < B.cardX[i] + B.cardW && my >= B.cardY && my < B.cardY + B.cardH )
				return i;
		return -1;
	}

	void drawBox( GameWindow *, WinInstanceData * )
	{
		Player *me = localPlayer();
		if( TheDisplay == nullptr || me == nullptr || s_mode != MODE_BOX )
			return;
		BoxLayout B;
		boxLayout( &B );
		const Look l = look();
		const Real u = B.u;
		drawFrame( l, B.x, B.y, B.w, B.h, u );
		TheDisplay->drawFillRect( B.x, B.y, B.w, B.titleH, l.accentDim );
		const Int titleSize = max( 9, px( 14.0f ) ), nameSize = max( 8, px( 12.0f ) ), lineSize = max( 8, px( 11.0f ) );
		const char *title = "SELECT YOUR CLASS";
		drawText( title, titleSize, TRUE, B.x + ( B.w - widthOf( title, titleSize, TRUE ) ) / 2, B.y + px( 5.0f ), l.text );
		const Int chosen = s_redeploy ? s_class : ( s_class >= 0 ? s_class : TiberiumStart::getChosen( me ) );
		for( Int i = 0; i < B.count && i < TiberiumStart::MOST_CHOICES; ++i )
		{
			const ThingTemplate *t = TiberiumStart::getChoice( me, i, s_list );
			const Bool hover = s_hoverCard == i, on = chosen == i;
			const Int x = B.cardX[i], y = B.cardY;
			TheDisplay->drawFillRect( x, y, B.cardW, B.cardH, on || hover ? l.fillOn : l.fill );
			TheDisplay->drawOpenRect( x, y, B.cardW, B.cardH, hover || on ? 2.0f : 1.0f, hover ? l.hi : ( on ? l.accent : l.accentDim ) );
			const UnicodeString name = upper( nameOf( t ) );
			drawTextIn( name, nameSize, TRUE, x + px( 6.0f ), y + px( 5.0f ), B.cardW - px( 12.0f ), hover || on ? l.hi : l.text );
			const Image *portrait = portraitOf( t );
			const Int ps = B.cardW - px( 16.0f );
			if( portrait != nullptr )
				TheDisplay->drawImage( portrait, x + px( 8.0f ), y + px( 24.0f ), x + px( 8.0f ) + ps, y + px( 24.0f ) + min( ps, B.cardH - px( 30.0f ) ) );
		}
		// the line on the card under the mouse
		if( s_hoverCard >= 0 )
		{
			UnicodeString line;
			line.format( L"Select this button to play as the %ls.", nameOf( TiberiumStart::getChoice( me, s_hoverCard, s_list ) ).str() );
			drawTextIn( line, lineSize, FALSE, B.x + px( 14.0f ), B.lineY, B.w - px( 28.0f ), l.textDim );
		}
		drawButton( l, B.cancelX, B.cancelY, B.cancelW, B.cancelH, u, "CANCEL", max( 8, px( 12.0f ) ), FALSE, s_hoverCancel, FALSE );
	}

	WindowMsgHandledType inputBox( GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData )
	{
		const Int mx = (Short)( mData1 & 0xFFFF ), my = (Short)( ( mData1 >> 16 ) & 0xFFFF );
		BoxLayout B;
		boxLayout( &B );
		const Int card = cardAt( B, mx, my );
		const Bool cancel = mx >= B.cancelX && mx < B.cancelX + B.cancelW && my >= B.cancelY && my < B.cancelY + B.cancelH;
		switch( msg )
		{
			case GWM_MOUSE_POS:
				s_hoverCard = card;
				s_hoverCancel = cancel;
				return MSG_HANDLED;
			case GWM_MOUSE_LEAVING:
				s_hoverCard = -1;
				s_hoverCancel = FALSE;
				return MSG_HANDLED;
			case GWM_LEFT_UP:
				if( card >= 0 )
					pickCard( card );
				else if( cancel )
				{
					if( s_redeploy )
						closeAll();
					else
						s_mode = MODE_NONE;
				}
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
	// the buttons at the top
	struct TopEntry
	{
		Int what;
		UnicodeString label;
		Bool dim;
	};

	void topButtons( std::vector<TopEntry> &out )
	{
		out.clear();
		Player *me = localPlayer();
		if( me == nullptr || !TiberiumStart::hasEntry( me ) )
			return;
		const UnsignedInt frame = now();
		if( TiberiumStart::isStartPending() )
		{
			if( frame < TiberiumStart::getIntroEndFrame() || s_mode == MODE_BOX )
				return;
			const Int chosen = s_class >= 0 ? s_class : TiberiumStart::getChosen( me );
			TopEntry e;
			e.what = TOP_CLASS;
			e.dim = FALSE;
			if( chosen >= 0 )
			{
				e.label = UnicodeString( L"CLASS: " );
				e.label.concat( upper( nameOf( TiberiumStart::getChoice( me, chosen ) ) ) );
			}
			else
				e.label = UnicodeString( L"SELECT YOUR CLASS" );
			out.push_back( e );
			return;
		}
		if( !me->isPlayerActive() )
			return;
		if( s_redeploy && s_mode == MODE_PLACE )
		{
			TopEntry e;
			e.what = TOP_CANCEL;
			e.dim = FALSE;
			e.label = UnicodeString( L"CANCEL DEPLOY" );
			out.push_back( e );
		}
		else if( s_mode == MODE_NONE )
		{
			// a Crawler called again (the start's list), or by the Crawler power; each with its wait counted down
			for( Int list = TiberiumStart::LIST_START; list <= TiberiumStart::LIST_POWER; ++list )
			{
				if( list == TiberiumStart::LIST_START ? !TiberiumStart::redeploys( me ) : !TiberiumStart::hasCrawlerPower( me ) )
					continue;
				if( TiberiumStart::hasCrawler( me, list ) )
					continue;
				const UnsignedInt when = TiberiumStart::getRedeployFrame( me, list );
				const wchar_t *name = list == TiberiumStart::LIST_START ? L"DEPLOY MCV" : L"CALL CRAWLER";
				TopEntry e;
				e.what = list == TiberiumStart::LIST_START ? TOP_DEPLOY : TOP_POWER;
				e.dim = when == 0 || frame < when;
				if( when != 0 && frame < when )
				{
					const UnsignedInt seconds = ( when - frame + LOGICFRAMES_PER_SECOND - 1 ) / LOGICFRAMES_PER_SECOND;
					e.label.format( L"%ls  %u:%02u", name, seconds / 60, seconds % 60 );
				}
				else
					e.label = UnicodeString( name );
				out.push_back( e );
			}
		}
		// the Crawler power's upgrade: its price, then its research's share done
		Real progress = 0.0f;
		const Int research = TiberiumStart::getCrawlerPowerResearch( me, &progress );
		const UpgradeTemplate *up = tierUpgrade();
		if( up != nullptr && ( research == TiberiumStart::RESEARCH_OPEN || research == TiberiumStart::RESEARCH_RUNNING ) )
		{
			TopEntry e;
			e.what = TOP_TIER;
			UnicodeString amount;
			if( research == TiberiumStart::RESEARCH_RUNNING )
			{
				amount.format( L"  %d%%", (Int)( progress * 100.0f ) );
				e.dim = TRUE;
			}
			else
			{
				amount.format( L"  $%d", up->calcCostToBuild( me ) );
				e.dim = !TheUpgradeCenter->canAffordUpgrade( me, up, FALSE );
			}
			e.label = tierName();
			e.label.concat( amount );
			out.push_back( e );
		}
		if( selectedCrawler() != nullptr && !TiberiumStart::isDecommissioning( me ) )
		{
			TopEntry e;
			e.what = TOP_DECOMMISSION;
			e.dim = FALSE;
			e.label = UnicodeString( frame < s_confirmUntil ? L"CONFIRM DECOMMISSION" : L"DECOMMISSION" );
			out.push_back( e );
		}
	}

	void topLayout( Int count, Int *x, Int *y, Int *w, Int *h, Int *bw, Int *gap )
	{
		*bw = px( 190.0f );
		*gap = px( 8.0f );
		*h = px( 30.0f );
		*w = count * *bw + max( 0, count - 1 ) * *gap;
		*x = ( TheDisplay->getWidth() - *w ) / 2;
		*y = TiberiumStart::isStartPending() ? px( 76.0f ) : px( 8.0f );
	}

	void pressTop( Int what )
	{
		Player *me = localPlayer();
		const UnsignedInt frame = now();
		switch( what )
		{
			case TOP_CLASS:
				openBox( FALSE, TiberiumStart::LIST_START );
				break;
			case TOP_DEPLOY:
			case TOP_POWER:
			{
				const Int list = what == TOP_POWER ? (Int)TiberiumStart::LIST_POWER : (Int)TiberiumStart::LIST_START;
				const UnsignedInt when = TiberiumStart::getRedeployFrame( me, list );
				if( when != 0 && frame >= when )
					openBox( TRUE, list );
				break;
			}
			case TOP_CANCEL:
				closeAll();
				break;
			case TOP_TIER:
				if( TheMessageStream != nullptr )
					TheMessageStream->appendMessage( GameMessage::MSG_TIBERIUM_CRAWLER_RESEARCH );
				break;
			case TOP_DECOMMISSION:
				if( frame < s_confirmUntil )
				{
					s_confirmUntil = 0;
					Object *crawler = selectedCrawler();
					if( TheMessageStream != nullptr && crawler != nullptr )
					{
						GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_TIBERIUM_CRAWLER_DECOMMISSION );
						msg->appendObjectIDArgument( crawler->getID() );
					}
				}
				else
					s_confirmUntil = frame + CONFIRM_FRAMES;
				break;
		}
	}

	void drawTop( GameWindow *, WinInstanceData * )
	{
		std::vector<TopEntry> buttons;
		topButtons( buttons );
		if( buttons.empty() || TheDisplay == nullptr )
			return;
		Int x, y, w, h, bw, gap;
		topLayout( (Int)buttons.size(), &x, &y, &w, &h, &bw, &gap );
		const Look l = look();
		const Int size = max( 8, px( 12.0f ) );
		for( size_t i = 0; i < buttons.size(); ++i )
		{
			AsciiString label;
			label.translate( buttons[i].label );
			drawButton( l, x + (Int)i * ( bw + gap ), y, bw, h, scale(), label.str(), size,
				buttons[i].what == TOP_DECOMMISSION && now() < s_confirmUntil, s_hoverTop == (Int)i, buttons[i].dim );
			// (under the upgrade's button: what must stand first)
			UnicodeString missing;
			if( buttons[i].what == TOP_TIER && TiberiumStart::getCrawlerPowerResearch( localPlayer(), nullptr ) == TiberiumStart::RESEARCH_OPEN
				&& tierUpgrade() != nullptr && !TheUpgradeCenter->hasNeededObjects( localPlayer(), tierUpgrade(), &missing ) )
			{
				UnicodeString line( L"Needs: " );
				line.concat( missing );
				const Int lineSize = max( 8, px( 11.0f ) );
				const Int lw = widthOf( line, lineSize, FALSE );
				const Int bx = x + (Int)i * ( bw + gap );
				drawText( line, lineSize, FALSE, bx + ( bw - lw ) / 2, y + h + px( 3.0f ), l.textDim );
			}
		}
	}

	WindowMsgHandledType inputTop( GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData )
	{
		const Int mx = (Short)( mData1 & 0xFFFF ), my = (Short)( ( mData1 >> 16 ) & 0xFFFF );
		std::vector<TopEntry> buttons;
		topButtons( buttons );
		Int x, y, w, h, bw, gap;
		topLayout( (Int)buttons.size(), &x, &y, &w, &h, &bw, &gap );
		Int hit = -1;
		for( size_t i = 0; i < buttons.size() && hit < 0; ++i )
		{
			const Int bx = x + (Int)i * ( bw + gap );
			if( mx >= bx && mx < bx + bw && my >= y && my < y + h )
				hit = (Int)i;
		}
		switch( msg )
		{
			case GWM_MOUSE_POS:
				s_hoverTop = hit;
				return MSG_HANDLED;
			case GWM_MOUSE_LEAVING:
				s_hoverTop = -1;
				return MSG_HANDLED;
			case GWM_LEFT_UP:
				if( hit >= 0 && !buttons[hit].dim )
					pressTop( buttons[hit].what );
				return MSG_HANDLED;
			case GWM_LEFT_DOWN:
			case GWM_RIGHT_DOWN:
			case GWM_RIGHT_UP:
				return MSG_HANDLED;
			default:
				return MSG_IGNORED;
		}
	}

	//-----------------------------------------------------------------------------------------------
	GameWindow *makeWindow( GameWinDrawFunc draw, GameWinInputFunc input )
	{
		GameWindow *win = TheWindowManager->winCreate( nullptr, WIN_STATUS_ENABLED | WIN_STATUS_ABOVE | WIN_STATUS_RIGHT_CLICK,
			0, 0, 1, 1, nullptr, nullptr );
		if( win != nullptr )
		{
			win->winSetDrawFunc( draw );
			win->winSetInputFunc( input );
		}
		return win;
	}

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

	void dropWindow( GameWindow *&win )
	{
		if( win != nullptr && TheWindowManager != nullptr )
			TheWindowManager->winDestroy( win );
		win = nullptr;
	}

	//-----------------------------------------------------------------------------------------------
	/** C&C4's landing marker on the ground: its ring, four marks pointing in, a pin from its middle with a
		head, and the player's name over the pin */
	void drawMarker( const Coord3D &at, Color color, const UnicodeString *name, Color nameColor )
	{
		if( TheTacticalView == nullptr || TheTerrainLogic == nullptr )
			return;
		const Real r = TiberiumStart::getMarkerRadius();
		const Real width = (Real)max( 2, (Int)TheDisplay->getHeight() / 360 );
		const Int SEGMENTS = 48;
		ICoord2D prev;
		Bool havePrev = FALSE;
		for( Int k = 0; k <= SEGMENTS; ++k )
		{
			const Real a = (Real)k * ( 2.0f * PI / SEGMENTS );
			Coord3D w;
			w.x = at.x + r * cosf( a );
			w.y = at.y + r * sinf( a );
			w.z = TheTerrainLogic->getGroundHeight( w.x, w.y ) + 2.0f;
			ICoord2D s;
			const Bool ok = TheTacticalView->worldToScreenTriReturn( &w, &s ) != View::WTS_INVALID;
			if( ok && havePrev )
				TheDisplay->drawLine( prev.x, prev.y, s.x, s.y, width, color );
			prev = s;
			havePrev = ok;
		}
		// the marks: a wedge at each quarter, its point towards the middle
		for( Int q = 0; q < 4; ++q )
		{
			const Real a = PI * 0.25f + (Real)q * PI * 0.5f;
			Coord3D tip, l, rr;
			tip.x = at.x + r * 0.70f * cosf( a );
			tip.y = at.y + r * 0.70f * sinf( a );
			l.x = at.x + r * 0.92f * cosf( a - 0.12f );
			l.y = at.y + r * 0.92f * sinf( a - 0.12f );
			rr.x = at.x + r * 0.92f * cosf( a + 0.12f );
			rr.y = at.y + r * 0.92f * sinf( a + 0.12f );
			tip.z = TheTerrainLogic->getGroundHeight( tip.x, tip.y ) + 2.0f;
			l.z = TheTerrainLogic->getGroundHeight( l.x, l.y ) + 2.0f;
			rr.z = TheTerrainLogic->getGroundHeight( rr.x, rr.y ) + 2.0f;
			ICoord2D st, sl, sr;
			if( TheTacticalView->worldToScreenTriReturn( &tip, &st ) == View::WTS_INVALID
				|| TheTacticalView->worldToScreenTriReturn( &l, &sl ) == View::WTS_INVALID
				|| TheTacticalView->worldToScreenTriReturn( &rr, &sr ) == View::WTS_INVALID )
				continue;
			TheDisplay->drawLine( st.x, st.y, sl.x, sl.y, width, color );
			TheDisplay->drawLine( sl.x, sl.y, sr.x, sr.y, width, color );
			TheDisplay->drawLine( sr.x, sr.y, st.x, st.y, width, color );
		}
		// the pin
		Coord3D base = at, top = at;
		base.z = TheTerrainLogic->getGroundHeight( at.x, at.y );
		top.z = base.z + 60.0f;
		ICoord2D sb, stop;
		if( TheTacticalView->worldToScreenTriReturn( &base, &sb ) == View::WTS_INVALID
			|| TheTacticalView->worldToScreenTriReturn( &top, &stop ) == View::WTS_INVALID )
			return;
		TheDisplay->drawLine( sb.x, sb.y, stop.x, stop.y, width, color );
		const Int head = max( 4, (Int)TheDisplay->getHeight() / 120 );
		TheDisplay->drawLine( stop.x, stop.y - head, stop.x + head, stop.y, width, color );
		TheDisplay->drawLine( stop.x + head, stop.y, stop.x, stop.y + head, width, color );
		TheDisplay->drawLine( stop.x, stop.y + head, stop.x - head, stop.y, width, color );
		TheDisplay->drawLine( stop.x - head, stop.y, stop.x, stop.y - head, width, color );
		if( name != nullptr && !name->isEmpty() )
		{
			const Int size = max( 8, px( 12.0f ) );
			drawText( *name, size, TRUE, stop.x - widthOf( *name, size, TRUE ) / 2, stop.y - head - px( 18.0f ), nameColor );
		}
	}

	//-----------------------------------------------------------------------------------------------
	/** C&C4's "Match Countdown": MM:SS:cc to the arrivals */
	void drawCountdown()
	{
		const UnsignedInt frame = now(), end = TiberiumStart::getDeployFrame();
		const UnsignedInt left = end > frame ? end - frame : 0;
		const UnsignedInt ms = left * 1000 / LOGICFRAMES_PER_SECOND;
		const Look l = look();
		const Int w = px( 240.0f ), h = px( 64.0f );
		const Int x = ( TheDisplay->getWidth() - w ) / 2, y = px( 6.0f );
		drawFrame( l, x, y, w, h, scale() );
		TheDisplay->drawFillRect( x, y, w, px( 20.0f ), l.accentDim );
		const Int titleSize = max( 8, px( 12.0f ) );
		drawText( "Match Countdown", titleSize, TRUE, x + ( w - widthOf( "Match Countdown", titleSize, TRUE ) ) / 2, y + px( 3.0f ), l.text );
		const Int size = max( 12, px( 30.0f ) );
		if( s_count == nullptr && TheDisplayStringManager != nullptr )
			s_count = TheDisplayStringManager->newDisplayString();
		if( s_count == nullptr )
			return;
		if( s_countSize != size && TheFontLibrary != nullptr )
		{
			s_count->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), size, TRUE ) );
			s_countSize = size;
		}
		UnicodeString digits;
		digits.format( L"%02u : %02u : %02u", ( ms / 60000 ) % 100, ( ms / 1000 ) % 60, ( ms % 1000 ) / 10 );
		s_count->setText( digits );
		Int tw = 0, th = 0;
		s_count->getSize( &tw, &th );
		s_count->draw( x + ( w - tw ) / 2, y + px( 22.0f ), l.text, GameMakeColor( 0, 0, 0, 160 ) );
	}

	/** DEPLOY! across the screen as the countdown ends: it slides in, holds, and fades */
	void drawBanner()
	{
		const UnsignedInt frame = now(), at = TiberiumStart::getDeployFrame();
		if( frame < at || frame >= at + BANNER_FRAMES )
			return;
		const Real t = (Real)( frame - at ) / (Real)BANNER_FRAMES;
		const Look l = look();
		const Int sw = TheDisplay->getWidth(), sh = TheDisplay->getHeight();
		const Int h = px( 58.0f ), y = sh * 18 / 100;
		const Real fade = t < 0.8f ? 1.0f : ( 1.0f - t ) / 0.2f;
		UnsignedByte r, g, b, a;
		GameGetColorComponents( l.accentDim, &r, &g, &b, &a );
		TheDisplay->drawFillRect( 0, y, sw, h, GameMakeColor( r, g, b, (Int)( 190.0f * fade ) ) );
		GameGetColorComponents( l.accent, &r, &g, &b, &a );
		TheDisplay->drawFillRect( 0, y, sw, max( 2, px( 2.0f ) ), GameMakeColor( r, g, b, (Int)( 255.0f * fade ) ) );
		TheDisplay->drawFillRect( 0, y + h - max( 2, px( 2.0f ) ), sw, max( 2, px( 2.0f ) ), GameMakeColor( r, g, b, (Int)( 255.0f * fade ) ) );
		const Int size = max( 14, px( 36.0f ) );
		const char *text = "DEPLOY!";
		const Int tw = widthOf( text, size, TRUE );
		const Real slide = t < 0.2f ? t / 0.2f : 1.0f;
		const Int tx = (Int)( -tw + ( ( sw - tw ) / 2 + tw ) * slide );
		GameGetColorComponents( l.text, &r, &g, &b, &a );
		drawText( text, size, TRUE, tx, y + ( h - size * 13 / 10 ) / 2, GameMakeColor( r, g, b, (Int)( 255.0f * fade ) ) );
	}

	//-----------------------------------------------------------------------------------------------
	/** the world clicks that put the marker down: before the selection's, after the windows' */
	class TiberiumStartTranslator : public GameMessageTranslator
	{
	public:
		virtual GameMessageDisposition translateGameMessage( const GameMessage *msg ) override
		{
			Player *me = localPlayer();
			if( me == nullptr || !TiberiumStart::hasEntry( me ) || TheTacticalView == nullptr )
				return KEEP_MESSAGE;
			// the start (its class picked, its box down): each click on the ground moves the marker; a redeployment's
			// marker: a click calls the Crawler there
			const Bool start = TiberiumStart::isStartPending() && now() >= TiberiumStart::getIntroEndFrame() && s_mode != MODE_BOX
				&& ( s_class >= 0 || TiberiumStart::getChosen( me ) >= 0 );
			const Bool placing = s_mode == MODE_PLACE;
			if( !start && !placing )
				return KEEP_MESSAGE;
			switch( msg->getType() )
			{
				case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN:
				case GameMessage::MSG_RAW_MOUSE_LEFT_DOUBLE_CLICK:
				{
					ICoord2D mouse = msg->getArgument( 0 )->pixel;
					Coord3D world;
					if( TheTacticalView->screenToTerrain( &mouse, &world ) )
						placeAt( world );
					return DESTROY_MESSAGE;
				}
				case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP:
					return DESTROY_MESSAGE;
				case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN:
					s_rightDown = msg->getArgument( 0 )->pixel;
					return KEEP_MESSAGE;
				case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP:
				{
					// a right click (not the end of a drag of the view) puts a redeployment's marker away
					const ICoord2D up = msg->getArgument( 0 )->pixel;
					if( placing && s_redeploy && abs( up.x - s_rightDown.x ) < 6 && abs( up.y - s_rightDown.y ) < 6 )
					{
						closeAll();
						return DESTROY_MESSAGE;
					}
					return KEEP_MESSAGE;
				}
				default:
					return KEEP_MESSAGE;
			}
		}
	};
}

//-------------------------------------------------------------------------------------------------
GameMessageTranslator *TiberiumStartView::makeTranslator()
{
	return MSGNEW( "GameClientSubsystem" ) TiberiumStartTranslator;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStartView::isPlacing()
{
	return s_mode == MODE_PLACE;
}

//-------------------------------------------------------------------------------------------------
Bool TiberiumStartView::showsZones()
{
	const Player *me = localPlayer();
	if( me == nullptr || !TiberiumStart::hasEntry( me ) )
		return FALSE;
	return TiberiumStart::isStartPending() || s_mode != MODE_NONE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumStartView::update()
{
	Player *me = localPlayer();
	if( TheGameLogic == nullptr || !TheGameLogic->isInGame() || TheGameLogic->isInShellGame() || TheWindowManager == nullptr
		|| TheDisplay == nullptr || me == nullptr || !TiberiumStart::hasEntry( me ) || TheGameLogic->isInReplayGame() )
	{
		if( s_active )
			reset();
		return;
	}
	s_active = TRUE;
	const UnsignedInt frame = now();

	// the intro: the camera held on the map's middle, then its pan to the player's start
	if( TiberiumStart::isStartPending() && TheTacticalView != nullptr && TheTerrainLogic != nullptr )
	{
		if( s_intro == 0 && frame < TiberiumStart::getIntroEndFrame() )
		{
			Region3D extent;
			TheTerrainLogic->getMaximumPathfindExtent( &extent );
			Coord3D middle;
			middle.x = ( extent.lo.x + extent.hi.x ) * 0.5f;
			middle.y = ( extent.lo.y + extent.hi.y ) * 0.5f;
			middle.z = TheTerrainLogic->getGroundHeight( middle.x, middle.y );
			TheTacticalView->lookAt( &middle );
			s_intro = 1;
			s_introFrame = frame;
		}
		else if( s_intro == 1 && frame >= s_introFrame + HOLD_FRAMES )
		{
			Coord3D home;
			if( TiberiumStart::getHome( me, &home ) )
				TheTacticalView->moveCameraTo( &home, PAN_MS, 0, FALSE, PAN_MS * 0.25f, PAN_MS * 0.25f );
			s_intro = 2;
		}
		// the box, as the intro ends
		if( !s_boxOpened && frame >= TiberiumStart::getIntroEndFrame() )
		{
			s_boxOpened = TRUE;
			if( TiberiumStart::getChosen( me ) < 0 )
				openBox( FALSE, TiberiumStart::LIST_START );
		}
	}
	else
	{
		s_intro = 3;
		// the start is over: its box and marker go
		if( !s_redeploy && s_mode != MODE_NONE )
			closeAll();
		// a redeployment no longer wanted (one came, or it may not now)
		if( s_redeploy && ( TiberiumStart::hasCrawler( me, s_list ) || TiberiumStart::getRedeployFrame( me, s_list ) == 0 ) )
			closeAll();
	}
	if( s_sent )
	{
		Coord3D logic;
		if( frame >= s_sentFrame + SENT_FRAMES
			|| ( TiberiumStart::getSpot( me, &logic ) && fabsf( logic.x - s_sentSpot.x ) < 1.0f && fabsf( logic.y - s_sentSpot.y ) < 1.0f ) )
			s_sent = FALSE;
	}
	if( frame >= s_confirmUntil )
		s_confirmUntil = 0;

	// the windows: the box while it is up, the buttons while there are any
	if( s_mode == MODE_BOX )
	{
		if( s_box == nullptr )
			s_box = makeWindow( drawBox, inputBox );
		if( s_box != nullptr )
		{
			BoxLayout B;
			boxLayout( &B );
			fit( s_box, B.x, B.y, B.w, B.h );
		}
	}
	else
		dropWindow( s_box );
	std::vector<TopEntry> buttons;
	topButtons( buttons );
	if( !buttons.empty() )
	{
		if( s_top == nullptr )
			s_top = makeWindow( drawTop, inputTop );
		if( s_top != nullptr )
		{
			Int x, y, w, h, bw, gap;
			topLayout( (Int)buttons.size(), &x, &y, &w, &h, &bw, &gap );
			fit( s_top, x, y, w, h );
		}
	}
	else
		dropWindow( s_top );
}

//-------------------------------------------------------------------------------------------------
void TiberiumStartView::draw()
{
	Player *me = localPlayer();
	if( !s_active || me == nullptr || TheDisplay == nullptr || TheTacticalView == nullptr || TheGameLogic == nullptr )
		return;
	const UnsignedInt frame = now();
	const Bool start = TiberiumStart::isStartPending();

	// the start's markers: its own and its allies'
	if( start && frame >= TiberiumStart::getIntroEndFrame() && ThePlayerList != nullptr )
	{
		for( Int i = 0; i < ThePlayerList->getPlayerCount(); ++i )
		{
			Player *p = ThePlayerList->getNthPlayer( i );
			if( p == nullptr || !TiberiumStart::hasEntry( p ) )
				continue;
			const Team *team = p->getDefaultTeam();
			if( p != me && ( team == nullptr || me->getRelationship( team ) != ALLIES ) )
				continue;
			Coord3D spot;
			Bool has = TiberiumStart::getSpot( p, &spot );
			if( p == me && s_sent )
			{
				spot = s_sentSpot;
				has = TRUE;
			}
			if( !has )
				continue;
			const UnicodeString name = p->getPlayerDisplayName();
			drawMarker( spot, GameMakeColor( 255, 255, 255, 230 ), &name, p == me ? GameMakeColor( 120, 255, 120, 255 ) : p->getPlayerColor() );
		}
	}

	// the marker following the mouse: green where it may land, red (and why) where not
	Coord3D mine;
	const Bool following = s_mode == MODE_PLACE || ( start && frame >= TiberiumStart::getIntroEndFrame() && s_mode == MODE_NONE
		&& !TiberiumStart::getSpot( me, &mine ) && !s_sent && ( s_class >= 0 || TiberiumStart::getChosen( me ) >= 0 ) );
	if( following && TheMouse != nullptr && ( s_mode == MODE_PLACE || start ) )
	{
		ICoord2D mouse = TheMouse->getMouseStatus()->pos;
		Coord3D world;
		if( TheTacticalView->screenToTerrain( &mouse, &world ) )
		{
			world.z = TheTerrainLogic->getGroundHeight( world.x, world.y );
			const char *why = nullptr;
			const Bool ok = TiberiumStart::isSpotAllowed( me, &world, &why );
			drawMarker( world, ok ? GameMakeColor( 90, 255, 90, 230 ) : GameMakeColor( 255, 70, 60, 230 ), nullptr, 0 );
			if( !ok && why != nullptr )
			{
				const Int size = max( 8, px( 12.0f ) );
				drawText( why, size, TRUE, mouse.x + px( 16.0f ), mouse.y + px( 14.0f ), GameMakeColor( 255, 110, 100, 255 ) );
			}
		}
	}

	if( start && frame >= TiberiumStart::getIntroEndFrame() )
		drawCountdown();
	if( !start )
		drawBanner();
}

//-------------------------------------------------------------------------------------------------
void TiberiumStartView::reset()
{
	dropWindow( s_box );
	dropWindow( s_top );
	if( s_count != nullptr && TheDisplayStringManager != nullptr )
		TheDisplayStringManager->freeDisplayString( s_count );
	s_count = nullptr;
	s_countSize = 0;
	s_mode = MODE_NONE;
	s_redeploy = FALSE;
	s_class = -1;
	s_hoverCard = -1;
	s_hoverCancel = FALSE;
	s_hoverTop = -1;
	s_boxOpened = FALSE;
	s_intro = 0;
	s_introFrame = 0;
	s_sent = FALSE;
	s_sentFrame = 0;
	s_confirmUntil = 0;
	s_active = FALSE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumStartView::testPick( Int choice, Real dx, Real dy, Bool hasSpot, Int list )
{
	Player *me = localPlayer();
	Coord3D home;
	if( me == nullptr || !TiberiumStart::getHome( me, &home ) )
		return;
	if( !TiberiumStart::isStartPending() )
	{
		// a redeployment (or the Crawler power): the box's card, then the click
		openBox( TRUE, list );
		pickCard( choice );
		if( hasSpot )
		{
			Coord3D at = home;
			at.x += dx;
			at.y += dy;
			at.z = TheTerrainLogic->getGroundHeight( at.x, at.y );
			// (sent even where it may not land: the logic's own check is what is tested)
			sendPick( s_class, &at );
			closeAll();
		}
		return;
	}
	if( choice >= 0 )
	{
		openBox( FALSE, TiberiumStart::LIST_START );
		pickCard( choice );
	}
	if( hasSpot )
	{
		Coord3D at = home;
		at.x += dx;
		at.y += dy;
		at.z = TheTerrainLogic->getGroundHeight( at.x, at.y );
		sendPick( -1, &at );
		s_mode = MODE_NONE;
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumStartView::testDecommission()
{
	// the selected Crawler, as the button's second click does (none selected: the start's list's first)
	Object *crawler = selectedCrawler();
	if( TheMessageStream != nullptr )
	{
		GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_TIBERIUM_CRAWLER_DECOMMISSION );
		msg->appendObjectIDArgument( crawler != nullptr ? crawler->getID() : INVALID_ID );
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumStartView::describe( char *buffer, Int size )
{
	Player *me = localPlayer();
	Coord3D spot, home;
	const Bool hasSpot = TiberiumStart::getSpot( me, &spot );
	TiberiumStart::getHome( me, &home );
	std::vector<TopEntry> buttons;
	topButtons( buttons );
	std::string top;
	for( size_t i = 0; i < buttons.size(); ++i )
	{
		AsciiString a;
		a.translate( buttons[i].label );
		top += a.str();
		top += buttons[i].dim ? " (dim); " : "; ";
	}
	snprintf( buffer, size, "entry %d, start pending %d (class box from %u, arrivals %u), mode %d%s, class %d, chosen %d, spot %s %.0f %.0f "
		"(home %.0f %.0f), crawler %d, redeploy frame %u, power %d (crawler %d, frame %u, upgrade %s, research %d), decommissioning %d, "
		"zones shown %d, buttons: %s",
		(int)TiberiumStart::hasEntry( me ), (int)TiberiumStart::isStartPending(), TiberiumStart::getIntroEndFrame(),
		TiberiumStart::getDeployFrame(), (int)s_mode, s_redeploy ? " (redeploy)" : "", (int)s_class, (int)TiberiumStart::getChosen( me ),
		hasSpot ? "at" : "none", hasSpot ? spot.x : 0.0f, hasSpot ? spot.y : 0.0f, home.x, home.y, (int)TiberiumStart::hasCrawler( me ),
		TiberiumStart::getRedeployFrame( me ), (int)TiberiumStart::hasCrawlerPower( me ),
		(int)TiberiumStart::hasCrawler( me, TiberiumStart::LIST_POWER ), TiberiumStart::getRedeployFrame( me, TiberiumStart::LIST_POWER ),
		tierUpgrade() != nullptr ? tierUpgrade()->getUpgradeName().str() : "none", (int)TiberiumStart::getCrawlerPowerResearch( me, nullptr ),
		(int)TiberiumStart::isDecommissioning( me ), (int)showsZones(), top.c_str() );
}
