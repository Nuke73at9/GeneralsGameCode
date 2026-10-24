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

// Tiberium Project @feature Renderer test options (see TiberiumTest.h).

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/TiberiumTest.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "Common/GameEngine.h"
#include "Common/GlobalData.h"
#include "GameClient/Display.h"
#include "GameClient/GameClient.h"
#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"

namespace
{
	struct CameraStep
	{
		Int frame;
		Bool hasZoom, hasPitch, hasPosition;
		Real zoom, pitchDegrees, x, y;
	};

	Bool s_active = FALSE;
	Bool s_announced = FALSE;
	std::string s_map;
	std::vector<CameraStep> s_cameras;
	Int s_shotInterval = 0;
	Int s_scorchFrame = -1;
	Int s_crcFrame = -1;
	Bool s_crcLogged = FALSE;
	Int s_quitFrame = -1;
	Int s_frame = 0;
	Int s_lastShotFrame = -1000;
	Int s_shotCount = 0;
	FILE *s_log = nullptr;
	Bool s_logTried = FALSE;

	// A field like "0.45", or "-" / empty for "keep as it is"
	Bool parseField( const std::string &text, Real &value )
	{
		if ( text.empty() || text == "-" )
			return FALSE;
		char *end = nullptr;
		double v = strtod( text.c_str(), &end );
		if ( end == text.c_str() )
			return FALSE;
		value = (Real)v;
		return TRUE;
	}

	std::vector<std::string> split( const std::string &text, char separator )
	{
		std::vector<std::string> parts;
		std::string::size_type start = 0;
		for (;;)
		{
			std::string::size_type at = text.find( separator, start );
			parts.push_back( text.substr( start, at == std::string::npos ? std::string::npos : at - start ) );
			if ( at == std::string::npos )
				break;
			start = at + 1;
		}
		return parts;
	}

	void openLog()
	{
		if ( s_logTried || TheGlobalData == nullptr )
			return;
		s_logTried = TRUE;
		std::string path = TheGlobalData->getPath_UserData().str();
		path += "TiberiumTest.log";
		s_log = fopen( path.c_str(), "w" );
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumTest::log( const char *format, ... )
{
	char line[1024];
	va_list args;
	va_start( args, format );
	vsnprintf( line, sizeof(line), format, args );
	va_end( args );
	openLog();
	if ( s_log != nullptr )
	{
		fprintf( s_log, "%s\n", line );
		fflush( s_log );
	}
	fprintf( stderr, "TIBTEST %s\n", line );
	fflush( stderr );
}

//-------------------------------------------------------------------------------------------------
void TiberiumTest::setMap( const char *path )
{
	if ( path == nullptr || path[0] == '\0' )
		return;
	s_map = path;
	s_active = TRUE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumTest::setCameras( const char *spec )
{
	if ( spec == nullptr )
		return;
	std::vector<std::string> steps = split( spec, ';' );
	for ( size_t i = 0; i < steps.size(); ++i )
	{
		std::vector<std::string> f = split( steps[i], ',' );
		Real frame = 0.0f;
		if ( f.empty() || !parseField( f[0], frame ) )
			continue;
		CameraStep step;
		step.frame = (Int)frame;
		step.zoom = step.pitchDegrees = step.x = step.y = 0.0f;
		step.hasZoom = f.size() > 1 && parseField( f[1], step.zoom );
		step.hasPitch = f.size() > 2 && parseField( f[2], step.pitchDegrees );
		step.hasPosition = f.size() > 4 && parseField( f[3], step.x ) && parseField( f[4], step.y );
		s_cameras.push_back( step );
		s_active = TRUE;
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumTest::setShotInterval( Int frames ) { if ( frames > 0 ) { s_shotInterval = frames; s_active = TRUE; } }
void TiberiumTest::setScorchFrame( Int frame ) { if ( frame >= 0 ) { s_scorchFrame = frame; s_active = TRUE; } }
void TiberiumTest::setCRCFrame( Int frame ) { if ( frame >= 0 ) { s_crcFrame = frame; s_active = TRUE; } }
void TiberiumTest::setQuitFrame( Int frame ) { if ( frame > 0 ) { s_quitFrame = frame; s_active = TRUE; } }
Bool TiberiumTest::isActive() { return s_active; }

//-------------------------------------------------------------------------------------------------
void TiberiumTest::update()
{
	if ( !s_active )
		return;
	++s_frame;

	if ( !s_announced )
	{
		s_announced = TRUE;
		log( "test run: map '%s', %d camera steps, screenshot every %d frames, scorch at %d, CRC at logic frame %d, quit at %d",
			s_map.c_str(), (int)s_cameras.size(), (int)s_shotInterval, (int)s_scorchFrame, (int)s_crcFrame, (int)s_quitFrame );
	}

	const Bool inGame = TheGameLogic != nullptr && TheGameLogic->isInGame();

	// the camera, client-side only
	if ( TheTacticalView != nullptr )
	{
		for ( size_t i = 0; i < s_cameras.size(); ++i )
		{
			const CameraStep &step = s_cameras[i];
			if ( step.frame != s_frame )
				continue;
			if ( step.hasPosition )
			{
				Coord3D at;
				at.x = step.x;
				at.y = step.y;
				at.z = 0.0f;
				TheTacticalView->lookAt( &at );
			}
			if ( step.hasZoom )
				TheTacticalView->setZoom( step.zoom );
			if ( step.hasPitch )
				TheTacticalView->setPitch( step.pitchDegrees * PI / 180.0f );
			const Coord3D pos = TheTacticalView->getPosition();
			log( "frame %d: camera zoom %.2f pitch %.1f at %.0f %.0f", (int)s_frame, TheTacticalView->getZoom(),
				TheTacticalView->getPitch() * 180.0f / PI, pos.x, pos.y );
		}
	}

	// scorch marks around the view, client-side only
	if ( s_scorchFrame >= 0 && s_frame == s_scorchFrame && TheGameClient != nullptr && TheTacticalView != nullptr )
	{
		const Coord3D center = TheTacticalView->getPosition();
		for ( Int k = 0; k < 121; ++k )
		{
			Coord3D p = center;
			p.x += ( k % 11 - 5 ) * 150.0f;
			p.y += ( k / 11 - 5 ) * 150.0f;
			TheGameClient->addScorch( &p, 30.0f + 5.0f * ( k % 3 ), (Scorches)( k % 4 ) );
		}
		log( "frame %d: 121 scorch marks around %.0f %.0f", (int)s_frame, center.x, center.y );
	}

	// the logic's CRC, read only
	if ( s_crcFrame >= 0 && !s_crcLogged && inGame && (Int)TheGameLogic->getFrame() >= s_crcFrame )
	{
		s_crcLogged = TRUE;
		const UnsignedInt crc = TheGameLogic->getCRC( CRC_RECALC );
		log( "logic frame %u: CRC %08X", (unsigned)TheGameLogic->getFrame(), (unsigned)crc );
	}

	// screenshots
	if ( s_shotInterval > 0 && TheDisplay != nullptr && s_frame % s_shotInterval == 0 )
	{
		TheDisplay->takeScreenShot( SCREENSHOT_PNG );
		s_lastShotFrame = s_frame;
		++s_shotCount;
		log( "frame %d: screenshot %d (logic frame %u, in game %d)", (int)s_frame, (int)s_shotCount,
			TheGameLogic != nullptr ? (unsigned)TheGameLogic->getFrame() : 0u, inGame ? 1 : 0 );
	}

	// quit, once the last screenshot has had time to be written
	if ( s_quitFrame > 0 && s_frame >= s_quitFrame && s_frame >= s_lastShotFrame + 15 && TheGameEngine != nullptr )
	{
		log( "frame %d: quitting", (int)s_frame );
		TheGameEngine->setQuitting( TRUE );
		s_quitFrame = -1;
	}
}
