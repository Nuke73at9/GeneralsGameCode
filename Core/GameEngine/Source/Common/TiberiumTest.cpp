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

#include <chrono>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "Common/GameEngine.h"
#include "Common/GlobalData.h"
#include "GameClient/ControlBar.h"
#include "GameClient/FXList.h"
#include "WWMath/matrix3d.h"
#include "GameClient/Display.h"
#include "GameClient/GameClient.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/Shell.h"
#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/TerrainLogic.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameLogic/PartitionManager.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/BodyModule.h"
#include "Common/BuildAssistant.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Weapon.h"

namespace
{
	struct CameraStep
	{
		Int frame;
		Bool hasZoom, hasPitch, hasPosition, hasAngle;
		Real zoom, pitchDegrees, x, y, angleDegrees;
	};

	Bool s_active = FALSE;
	Bool s_announced = FALSE;
	std::string s_map;
	std::vector<CameraStep> s_cameras;
	Int s_shotInterval = 0;
	Int s_shotStart = 0;		// no screenshot before this frame (a tour's first view)
	Int s_scorchFrame = -1;
	Int s_lightsFrame = -1;
	Int s_fireFrame = -1;
	Int s_fxFrame = -1;
	Int s_fxEvery = 0;
	std::vector<std::string> s_fxLists;	// -testFXLists: the lists -testFX plays, in place of its own
	Int s_unitsFrame = -1;
	Int s_baseFrame = -1;
	// -testBase: the structure being built (its construction runs over BASE_BUILD_FRAMES) and the one
	// destroyed BASE_KILL_FRAMES in
	std::vector<ObjectID> s_baseBuilding;
	// -testSpawn: the objects to place, what to do with them, and what was made
	Int s_spawnFrame = -1;
	std::vector<std::string> s_spawnNames;
	Bool s_spawnMove = FALSE, s_spawnFight = FALSE, s_spawnDamage = FALSE;
	std::vector<ObjectID> s_spawned, s_spawnTargets;
	ObjectID s_baseVictim = INVALID_ID;
	Coord3D s_baseVictimPos;
	const Int BASE_BUILD_FRAMES = 600;
	const Int BASE_KILL_FRAMES = 300;
	Bool s_reveal = FALSE;
	Bool s_revealed = FALSE;
	Int s_controlBarStage = -1;	// ControlBarStages, or -1: as it is
	Int s_tourSize = 0;			// a tour of the map: a grid of tourSize x tourSize stops
	Bool s_tourBuilt = FALSE;
	const Int TOUR_VIEW_FRAMES = 45;	// per view: the camera moves, a moment later the screenshot
	Bool s_controlBarSet = FALSE;
	Int s_crcFrame = -1;
	Bool s_crcLogged = FALSE;
	Int s_quitFrame = -1;
	Bool s_quitIssued = FALSE;
	std::string s_menu;			// a shell screen to open before a game
	Bool s_menuPushed = FALSE;
	const Int MENU_FRAME = 60;
	Int s_frame = 0;
	Int s_lastShotFrame = -1000;
	std::chrono::steady_clock::time_point s_lastShotTime;
	std::chrono::steady_clock::time_point s_spanStart;	// frame timing: since the last screenshot (or the first frame)
	Int s_spanStartFrame = 0;
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

	// Tiberium Project @feature Crash watch (see TiberiumTest.h). Plain Win32 calls only: it must
	// work while the process is exiting, after the C runtime has begun shutting down.
#ifdef _WIN32
	HANDLE s_crashFile = INVALID_HANDLE_VALUE;
	volatile LONG s_crashCount = 0;
	const LONG MAX_CRASH_REPORTS = 4;

	void rawWrite( const char *text )
	{
		if ( s_crashFile == INVALID_HANDLE_VALUE )
			return;
		DWORD written = 0;
		WriteFile( s_crashFile, text, (DWORD)strlen( text ), &written, nullptr );
	}

	// "module.dll+0x1234" for a code address
	void describeAddress( const void *address, char *out, size_t size )
	{
		HMODULE module = nullptr;
		char path[MAX_PATH] = "";
		if ( GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)address, &module ) && module != nullptr
			&& GetModuleFileNameA( module, path, MAX_PATH ) > 0 )
		{
			const char *name = strrchr( path, '\\' );
			snprintf( out, size, "%s+0x%llx", name != nullptr ? name + 1 : path,
				(unsigned long long)( (const char *)address - (const char *)module ) );
		}
		else
		{
			snprintf( out, size, "0x%llx (no module)", (unsigned long long)(uintptr_t)address );
		}
	}

#if defined(_M_X64) || defined(__x86_64__)
	// The call stack from the faulting instruction, unwound with the modules' own unwind tables
	void writeStack( const CONTEXT &faulting )
	{
		CONTEXT context = faulting;
		char line[400], where[MAX_PATH + 40];
		for ( int depth = 0; depth < 24 && context.Rip != 0; ++depth )
		{
			describeAddress( (const void *)context.Rip, where, sizeof(where) );
			snprintf( line, sizeof(line), "  %2d %s\r\n", depth, where );
			rawWrite( line );
			DWORD64 imageBase = 0;
			PRUNTIME_FUNCTION function = RtlLookupFunctionEntry( context.Rip, &imageBase, nullptr );
			if ( function == nullptr )
			{
				// a leaf function: the return address is on top of the stack
				MEMORY_BASIC_INFORMATION memory;
				if ( VirtualQuery( (LPCVOID)context.Rsp, &memory, sizeof(memory) ) == 0 || memory.State != MEM_COMMIT )
					break;
				context.Rip = *(const DWORD64 *)context.Rsp;
				context.Rsp += 8;
			}
			else
			{
				PVOID handlerData = nullptr;
				DWORD64 establisherFrame = 0;
				RtlVirtualUnwind( UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context, &handlerData,
					&establisherFrame, nullptr );
			}
		}
	}
#endif

	LONG WINAPI crashWatch( EXCEPTION_POINTERS *info )
	{
		const DWORD code = info->ExceptionRecord->ExceptionCode;
		if ( ( code & 0xF0000000u ) != 0xC0000000u )	// errors only: not C++ exceptions, debug output or thread names
			return EXCEPTION_CONTINUE_SEARCH;
		if ( InterlockedIncrement( &s_crashCount ) > MAX_CRASH_REPORTS )
			return EXCEPTION_CONTINUE_SEARCH;

		char line[600], where[MAX_PATH + 40];
		describeAddress( info->ExceptionRecord->ExceptionAddress, where, sizeof(where) );
		int length = snprintf( line, sizeof(line), "exception 0x%08lx at %s, thread %lu, frame %d",
			(unsigned long)code, where, (unsigned long)GetCurrentThreadId(), (int)s_frame );
		if ( code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2 && length > 0 )
		{
			const ULONG_PTR kind = info->ExceptionRecord->ExceptionInformation[0];
			snprintf( line + length, sizeof(line) - length, ", %s 0x%llx",
				kind == 0 ? "reading" : ( kind == 1 ? "writing" : "executing" ),
				(unsigned long long)info->ExceptionRecord->ExceptionInformation[1] );
		}
		rawWrite( line );
		rawWrite( "\r\n" );
#if defined(_M_X64) || defined(__x86_64__)
		writeStack( *info->ContextRecord );
#endif
		return EXCEPTION_CONTINUE_SEARCH;
	}

	void atExitStage()
	{
		TiberiumTest::exitStage( "C runtime exit: static objects are being destroyed" );
	}
#endif

	void installCrashWatch()
	{
#ifdef _WIN32
		if ( s_crashFile != INVALID_HANDLE_VALUE || TheGlobalData == nullptr )
			return;
		std::string path = TheGlobalData->getPath_UserData().str();
		path += "TiberiumCrash.log";
		s_crashFile = CreateFileA( path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr );
		if ( s_crashFile == INVALID_HANDLE_VALUE )
			return;
		rawWrite( "crash watch: on for this test run (error exceptions, with their call stacks, and the shutdown steps)\r\n" );
		AddVectoredExceptionHandler( 1, crashWatch );
		atexit( atExitStage );
#endif
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
		step.zoom = step.pitchDegrees = step.x = step.y = step.angleDegrees = 0.0f;
		step.hasZoom = f.size() > 1 && parseField( f[1], step.zoom );
		step.hasPitch = f.size() > 2 && parseField( f[2], step.pitchDegrees );
		step.hasPosition = f.size() > 4 && parseField( f[3], step.x ) && parseField( f[4], step.y );
		step.hasAngle = f.size() > 5 && parseField( f[5], step.angleDegrees );
		s_cameras.push_back( step );
		s_active = TRUE;
	}
}

//-------------------------------------------------------------------------------------------------
void TiberiumTest::setControlBar( const char *stage )
{
	if ( stage == nullptr )
		return;
	if ( strcmp( stage, "default" ) == 0 ) s_controlBarStage = CONTROL_BAR_STAGE_DEFAULT;
	else if ( strcmp( stage, "low" ) == 0 ) s_controlBarStage = CONTROL_BAR_STAGE_LOW;
	else if ( strcmp( stage, "hidden" ) == 0 ) s_controlBarStage = CONTROL_BAR_STAGE_HIDDEN;
	else return;
	s_active = TRUE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumTest::setTour( Int size )
{
	if ( size <= 0 )
		return;
	s_tourSize = size > 6 ? 6 : size;
	s_active = TRUE;
}

//-------------------------------------------------------------------------------------------------
void TiberiumTest::setShotInterval( Int frames ) { if ( frames > 0 ) { s_shotInterval = frames; s_active = TRUE; } }
void TiberiumTest::setShotStart( Int frame ) { if ( frame > 0 ) s_shotStart = frame; }
void TiberiumTest::setScorchFrame( Int frame ) { if ( frame >= 0 ) { s_scorchFrame = frame; s_active = TRUE; } }
void TiberiumTest::setLightsFrame( Int frame ) { if ( frame >= 0 ) { s_lightsFrame = frame; s_active = TRUE; } }
void TiberiumTest::setFireFrame( Int frame ) { if ( frame >= 0 ) { s_fireFrame = frame; s_active = TRUE; } }
void TiberiumTest::setFXFrame( Int frame, Int every ) { if ( frame >= 0 ) { s_fxFrame = frame; s_fxEvery = every > 0 ? every : 0; s_active = TRUE; } }
void TiberiumTest::setFXLists( const char *lists ) { if ( lists != nullptr ) s_fxLists = split( lists, ';' ); }
void TiberiumTest::setUnitsFrame( Int frame ) { if ( frame >= 0 ) { s_unitsFrame = frame; s_active = TRUE; } }
void TiberiumTest::setBaseFrame( Int frame ) { if ( frame >= 0 ) { s_baseFrame = frame; s_active = TRUE; } }
void TiberiumTest::setSpawn( const char *spec )
{
	if ( spec == nullptr )
		return;
	std::vector<std::string> parts = split( spec, ';' );
	if ( parts.size() < 2 )
		return;
	s_spawnFrame = atoi( parts[0].c_str() );
	s_spawnNames = split( parts[1].c_str(), ',' );
	if ( parts.size() > 2 )
	{
		s_spawnMove = parts[2].find( "move" ) != std::string::npos;
		s_spawnFight = parts[2].find( "fight" ) != std::string::npos;
		s_spawnDamage = parts[2].find( "damage" ) != std::string::npos;
	}
	if ( s_spawnFrame >= 0 && !s_spawnNames.empty() )
		s_active = TRUE;
}
void TiberiumTest::setReveal( Bool reveal ) { s_reveal = reveal; if ( reveal ) s_active = TRUE; }
void TiberiumTest::setCRCFrame( Int frame ) { if ( frame >= 0 ) { s_crcFrame = frame; s_active = TRUE; } }
void TiberiumTest::setQuitFrame( Int frame ) { if ( frame > 0 ) { s_quitFrame = frame; s_active = TRUE; } }
void TiberiumTest::setMenu( const char *layout ) { if ( layout != nullptr && *layout != '\0' ) { s_menu = layout; s_active = TRUE; } }
Bool TiberiumTest::isActive() { return s_active; }

//-------------------------------------------------------------------------------------------------
void TiberiumTest::exitStage( const char *stage )
{
	if ( !s_active || stage == nullptr )
		return;
#ifdef _WIN32
	rawWrite( "exit: " );
	rawWrite( stage );
	rawWrite( "\r\n" );
#else
	fprintf( stderr, "TIBTEST exit: %s\n", stage );
	fflush( stderr );
#endif
}

//-------------------------------------------------------------------------------------------------
void TiberiumTest::update()
{
	if ( !s_active )
		return;
	++s_frame;

	if ( !s_announced )
	{
		s_announced = TRUE;
		installCrashWatch();
		log( "test run: map '%s', %d camera steps, screenshot every %d frames, scorch at %d, CRC at logic frame %d, quit at %d",
			s_map.c_str(), (int)s_cameras.size(), (int)s_shotInterval, (int)s_scorchFrame, (int)s_crcFrame, (int)s_quitFrame );
	}

	const Bool inGame = TheGameLogic != nullptr && TheGameLogic->isInGame();

	// the map tour: once in game, a grid of stops over the playable map, each seen close up, at the
	// default height, from afar and from low down, turned a quarter each time; a screenshot of each
	// view, then quit (unless a quit frame was given)
	Region3D extent;
	if ( s_tourSize > 0 && !s_tourBuilt && inGame && TheTerrainLogic != nullptr
		&& ( TheTerrainLogic->getExtent( &extent ), extent.hi.x > extent.lo.x && extent.hi.y > extent.lo.y ) )	// once the map is in
	{
		s_tourBuilt = TRUE;
		const Real views[4][3] = { { 0.3f, 37.5f, 0.0f }, { 0.6f, 37.5f, 90.0f }, { 1.2f, 37.5f, 180.0f }, { 0.5f, 20.0f, 270.0f } };
		const Int base = ( s_frame / TOUR_VIEW_FRAMES + 1 ) * TOUR_VIEW_FRAMES;
		Int n = 0;
		for ( Int row = 0; row < s_tourSize; ++row )
		{
			for ( Int col = 0; col < s_tourSize; ++col )
			{
				for ( Int v = 0; v < 4; ++v )
				{
					CameraStep step;
					step.frame = base + n * TOUR_VIEW_FRAMES + TOUR_VIEW_FRAMES / 4;
					step.hasZoom = step.hasPitch = step.hasPosition = step.hasAngle = TRUE;
					step.zoom = views[v][0];
					step.pitchDegrees = views[v][1];
					step.angleDegrees = views[v][2];
					step.x = extent.lo.x + ( col + 0.5f ) * ( extent.hi.x - extent.lo.x ) / s_tourSize;
					step.y = extent.lo.y + ( row + 0.5f ) * ( extent.hi.y - extent.lo.y ) / s_tourSize;
					s_cameras.push_back( step );
					++n;
				}
			}
		}
		s_shotInterval = TOUR_VIEW_FRAMES;
		s_shotStart = base + TOUR_VIEW_FRAMES;
		if ( s_quitFrame <= 0 )
			s_quitFrame = base + n * TOUR_VIEW_FRAMES + 1;
		log( "frame %d: tour of %d views over (%.0f %.0f)-(%.0f %.0f), screenshots from frame %d, quit at %d", (int)s_frame, (int)n,
			extent.lo.x, extent.lo.y, extent.hi.x, extent.hi.y, (int)( base + TOUR_VIEW_FRAMES ), (int)s_quitFrame );
	}

	// the control bar lowered or hidden, for a full view of the world (the interface only); a
	// moment into the game, as the game's start sets up its own bar
	if ( s_controlBarStage >= 0 && !s_controlBarSet && inGame && s_frame >= 30 && TheControlBar != nullptr )
	{
		s_controlBarSet = TRUE;
		TheControlBar->switchControlBarStage( (ControlBarStages)s_controlBarStage );
		log( "frame %d: control bar stage %d", (int)s_frame, (int)s_controlBarStage );
	}

	// a shell screen (the options menu, say), opened on top of the main menu before any game
	if ( !s_menu.empty() && !s_menuPushed && !inGame && s_frame >= MENU_FRAME && TheShell != nullptr )
	{
		s_menuPushed = TRUE;
		TheShell->push( AsciiString( s_menu.c_str() ) );
		log( "frame %d: shell screen '%s' opened", (int)s_frame, s_menu.c_str() );
	}

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
			if ( step.hasAngle )
				TheTacticalView->setAngle( step.angleDegrees * PI / 180.0f );
			// the control bar again: the game's start may still set its own after the first time
			if ( s_controlBarSet && TheControlBar != nullptr )
				TheControlBar->switchControlBarStage( (ControlBarStages)s_controlBarStage );
			const Coord3D pos = TheTacticalView->getPosition();
			log( "frame %d: camera zoom %.2f pitch %.1f angle %.0f at %.0f %.0f", (int)s_frame, TheTacticalView->getZoom(),
				TheTacticalView->getPitch() * 180.0f / PI, TheTacticalView->getAngle() * 180.0f / PI, pos.x, pos.y );
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

	// light pulses around the view that never fade (fire orange, white, blue), client side only
	if ( s_lightsFrame >= 0 && s_frame == s_lightsFrame && TheDisplay != nullptr && TheTacticalView != nullptr )
	{
		const Coord3D center = TheTacticalView->getPosition();
		const RGBColor colors[3] = { { 1.0f, 0.55f, 0.2f }, { 1.0f, 1.0f, 1.0f }, { 0.3f, 0.5f, 1.0f } };
		for ( Int k = 0; k < 9; ++k )
		{
			Coord3D p = center;
			p.x += ( k % 3 - 1 ) * 160.0f;
			p.y += ( k / 3 - 1 ) * 160.0f;
			p.z = ( TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( p.x, p.y ) : 0.0f ) + 20.0f;
			TheDisplay->createLightPulse( &p, &colors[k % 3], 30.0f, 60.0f, 0, 0 );
		}
		log( "frame %d: 9 light pulses around %.0f %.0f", (int)s_frame, center.x, center.y );
	}

	// the map revealed to the local player (particle systems do not emit under the shroud)
	if ( s_reveal && !s_revealed && inGame && ThePartitionManager != nullptr && ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
	{
		s_revealed = TRUE;
		ThePartitionManager->revealMapForPlayer( ThePlayerList->getLocalPlayer()->getPlayerIndex() );
		log( "frame %d: the map revealed to the local player", (int)s_frame );
	}

	// fires around the view (Zero Hour's own lasting fire particle systems), client side only
	if ( s_fireFrame >= 0 && s_frame == s_fireFrame && TheParticleSystemManager != nullptr && TheTacticalView != nullptr )
	{
		static const char *const FIRES[] = { "FireBuildingSmall", "AutoFireLarge01", "FireToxicMedium", "SmolderingFireLarge", "FireTreeSmall" };
		const Coord3D center = TheTacticalView->getPosition();
		Int made = 0;
		for ( Int k = 0; k < 5; ++k )
		{
			const ParticleSystemTemplate *tmpl = TheParticleSystemManager->findTemplate( FIRES[k] );
			ParticleSystem *sys = tmpl != nullptr ? TheParticleSystemManager->createParticleSystem( tmpl ) : nullptr;
			if ( sys == nullptr )
				continue;
			Coord3D p = center;
			p.x += ( k - 2 ) * 120.0f;
			p.y += ( k % 2 ) * 120.0f - 60.0f;
			p.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( p.x, p.y ) : 0.0f;
			sys->setPosition( &p );
			++made;
		}
		// smoke over three of the fires, and two columns of it away from them (lit smoke: the
		// ones over the fires take their light, the others only the scene's)
		static const Real SMOKES[5][2] = { { -240.0f, -60.0f }, { 0.0f, -60.0f }, { 240.0f, -60.0f }, { -120.0f, 260.0f }, { 120.0f, 260.0f } };
		const ParticleSystemTemplate *smoke = TheParticleSystemManager->findTemplate( "SmokeFactionLarge" );
		Int smokes = 0;
		for ( Int k = 0; k < 5 && smoke != nullptr; ++k )
		{
			ParticleSystem *sys = TheParticleSystemManager->createParticleSystem( smoke );
			if ( sys == nullptr )
				continue;
			Coord3D p = center;
			p.x += SMOKES[k][0];
			p.y += SMOKES[k][1];
			p.z = ( TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( p.x, p.y ) : 0.0f ) + 10.0f;
			sys->setPosition( &p );
			++smokes;
		}
		log( "frame %d: %d fires and %d smoke columns around %.0f %.0f", (int)s_frame, (int)made, (int)smokes, center.x, center.y );
	}
	// Tiberium Project @feature explosions and hits around the view (Zero Hour's own FX lists: a tank
	// shell's and a missile's detonation, a tank's, a car's, a light vehicle's and a structure's
	// death), client side only, to see what the particle systems they use look like
	if ( s_fxFrame >= 0 && s_frame >= s_fxFrame && TheFXListStore != nullptr && TheTacticalView != nullptr
		&& ( s_frame == s_fxFrame || ( s_fxEvery > 0 && ( s_frame - s_fxFrame ) % s_fxEvery == 0 ) ) )
	{
		static const char *const LISTS[6] = { "WeaponFX_GenericTankShellDetonation", "FX_GenericTankDeathExplosion", "FX_GenericCarExplode",
			"WeaponFX_RocketBuggyMissileDetonation", "FX_StructureMediumDeath", "FX_BattleMasterExplosionOneFinal" };
		// in a row across the view, left to right in that order
		static const Real SPOTS[6][2] = { { -250.0f, 0.0f }, { -150.0f, 0.0f }, { -50.0f, 0.0f }, { 50.0f, 0.0f }, { 150.0f, 0.0f }, { 250.0f, 0.0f } };
		const Coord3D center = TheTacticalView->getPosition();
		Int played = 0;
		for ( Int k = 0; k < 6; ++k )
		{
			const char *name = s_fxLists.empty() ? LISTS[k] : ( k < (Int)s_fxLists.size() ? s_fxLists[k].c_str() : nullptr );
			const FXList *fx = name != nullptr ? TheFXListStore->findFXList( name ) : nullptr;
			if ( fx == nullptr )
				continue;
			Coord3D p = center;
			p.x += SPOTS[k][0];
			p.y += SPOTS[k][1];
			p.z = ( TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( p.x, p.y ) : 0.0f ) + 10.0f;
			// facing along +x (the view's right), ten units up, as a weapon's muzzle would be: the
			// lists that orient their particles to the firer have a direction to take
			Matrix3D mtx( true );
			mtx.Set_Translation( Vector3( p.x, p.y, p.z ) );
			FXList::doFXPos( fx, &p, &mtx );
			++played;
		}
		log( "frame %d: %d explosions and hits around %.0f %.0f", (int)s_frame, (int)played, center.x, center.y );
	}
	if ( s_fxFrame >= 0 && s_frame > s_fxFrame && ( s_frame - s_fxFrame ) % 10 == 0 && TheParticleSystemManager != nullptr )
	{
		// which systems are alive, by template, with their particles and their largest particle
		std::string alive;
		ParticleSystemManager::ParticleSystemList &systems = TheParticleSystemManager->getAllParticleSystems();
		for ( ParticleSystemManager::ParticleSystemListIt it = systems.begin(); it != systems.end(); ++it )
		{
			ParticleSystem *sys = *it;
			if ( sys == nullptr || sys->getParticleCount() == 0 || sys->getTemplate() == nullptr )
				continue;
			Real largest = 0.0f;
			Real r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
			for ( Particle *p = sys->getFirstParticle(); p != nullptr; p = p->m_systemNext )
			{
				largest = max( largest, p->getSize() );
				r += p->getColor()->red;
				g += p->getColor()->green;
				b += p->getColor()->blue;
				a += p->getAlpha();
			}
			const Real n = (Real)sys->getParticleCount();
			char entry[256];
			snprintf( entry, sizeof( entry ), " %s:%u(%.0f)[%s %.2f %.2f %.2f a%.2f]", sys->getTemplate()->getName().str(), (unsigned)sys->getParticleCount(), largest,
				sys->getParticleTypeName().str(), r / n, g / n, b / n, a / n );
			alive += entry;
		}
		log( "frame %d: %u particles in %u systems:%s", (int)s_frame, (unsigned)TheParticleSystemManager->getParticleCount(),
			(unsigned)TheParticleSystemManager->getParticleSystemCount(), alive.c_str() );
	}
	// Tiberium Project @feature vehicles and buildings of the local player's around the view, to see
	// what lights them (headlights, floodlights, lit windows at night); this adds to the game
	if ( s_unitsFrame >= 0 && s_frame == s_unitsFrame && TheThingFactory != nullptr && ThePlayerList != nullptr
		&& ThePlayerList->getLocalPlayer() != nullptr && TheTacticalView != nullptr )
	{
		struct Placed { const char *name; Real dx, dy, angle; };
		static const Placed THINGS[] = {
			{ "AmericaVehicleHumvee", -120.0f, -40.0f, 0.0f }, { "AmericaTankCrusader", -40.0f, -40.0f, 0.5f },
			{ "ChinaTankOverlord", 40.0f, -40.0f, 1.57f }, { "ChinaVehicleDozer", 120.0f, -40.0f, 3.14f },
			{ "AmericaVehicleAmbulance", -80.0f, 60.0f, -0.8f }, { "ChinaTankBattleMaster", 80.0f, 60.0f, 2.4f },
			{ "AmericaWarFactory", -260.0f, 200.0f, 0.0f }, { "ChinaCommandCenter", 260.0f, 200.0f, 0.0f },
			{ "AmericaPowerPlant", 0.0f, 260.0f, 0.0f } };
		Team *team = ThePlayerList->getLocalPlayer()->getDefaultTeam();
		const Coord3D center = TheTacticalView->getPosition();
		Int made = 0;
		for ( Int k = 0; k < (Int)( sizeof( THINGS ) / sizeof( THINGS[0] ) ); ++k )
		{
			const ThingTemplate *tmpl = TheThingFactory->findTemplate( THINGS[k].name );
			Object *obj = tmpl != nullptr && team != nullptr ? TheThingFactory->newObject( tmpl, team ) : nullptr;
			if ( obj == nullptr )
				continue;
			Coord3D p = center;
			p.x += THINGS[k].dx;
			p.y += THINGS[k].dy;
			p.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( p.x, p.y ) : 0.0f;
			obj->setPosition( &p );
			obj->setOrientation( THINGS[k].angle );
			++made;
		}
		log( "frame %d: %d vehicles and buildings around %.0f %.0f", (int)s_frame, (int)made, center.x, center.y );
	}

	// Tiberium Project @feature -testSpawn: any objects (converted C&C4 units and structures) in a row
	// before the view, for their art, animations and effects: optionally driving forward, shooting at
	// a row of Zero Hour vehicles (neutral, attacked by order), and taken through their damage states to
	// their death; this adds to the game
	if ( s_spawnFrame >= 0 && s_frame == s_spawnFrame && TheThingFactory != nullptr && ThePlayerList != nullptr
		&& ThePlayerList->getLocalPlayer() != nullptr && TheTacticalView != nullptr )
	{
		Team *team = ThePlayerList->getLocalPlayer()->getDefaultTeam();
		const Coord3D center = TheTacticalView->getPosition();
		const Int count = (Int)s_spawnNames.size();
		const Real spacing = 70.0f;
		for ( Int k = 0; k < count; ++k )
		{
			const ThingTemplate *tmpl = TheThingFactory->findTemplate( s_spawnNames[k].c_str() );
			Object *obj = tmpl != nullptr && team != nullptr ? TheThingFactory->newObject( tmpl, team ) : nullptr;
			if ( obj == nullptr )
			{
				log( "frame %d: no object %s", (int)s_frame, s_spawnNames[k].c_str() );
				continue;
			}
			Coord3D p = center;
			p.x += ( k - ( count - 1 ) * 0.5f ) * spacing;
			p.y -= 25.0f;
			p.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( p.x, p.y ) : 0.0f;
			obj->setPosition( &p );
			obj->setOrientation( PI * 0.5f );
			s_spawned.push_back( obj->getID() );
			if ( s_spawnFight )
			{
				// a target for each: one of Zero Hour's tanks, neutral, ahead
				const ThingTemplate *tt = TheThingFactory->findTemplate( "ChinaTankBattleMaster" );
				Player *neutral = ThePlayerList->getNeutralPlayer();
				Object *target = tt != nullptr && neutral != nullptr ? TheThingFactory->newObject( tt, neutral->getDefaultTeam() ) : nullptr;
				if ( target != nullptr )
				{
					Coord3D q = p;
					q.y += 160.0f;
					q.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( q.x, q.y ) : 0.0f;
					target->setPosition( &q );
					target->setOrientation( -PI * 0.5f );
					s_spawnTargets.push_back( target->getID() );
				}
			}
		}
		log( "frame %d: %d objects spawned around %.0f %.0f", (int)s_frame, (int)s_spawned.size(), center.x, center.y );
	}
	if ( s_spawnFrame >= 0 && s_frame > s_spawnFrame && TheGameLogic != nullptr && !s_spawned.empty() )
	{
		const Int since = (Int)( s_frame - s_spawnFrame );
		for ( size_t k = 0; k < s_spawned.size(); ++k )
		{
			Object *obj = TheGameLogic->findObjectByID( s_spawned[k] );
			if ( obj == nullptr )
				continue;
			AIUpdateInterface *ai = obj->getAIUpdateInterface();
			if ( since == 10 && ai != nullptr )
			{
				if ( s_spawnFight && k < s_spawnTargets.size() )
				{
					Object *target = TheGameLogic->findObjectByID( s_spawnTargets[k] );
					if ( target != nullptr )
						ai->aiForceAttackObject( target, NO_MAX_SHOTS_LIMIT, CMD_FROM_SCRIPT );
				}
				else if ( s_spawnMove )
				{
					Coord3D dest = *obj->getPosition();
					dest.y += 200.0f;
					ai->aiMoveToPosition( &dest, CMD_FROM_SCRIPT );
				}
			}
			// down through the damage states: damaged, really damaged, then destroyed
			BodyModuleInterface *body = obj->getBodyModule();
			if ( since % 30 == 0 && body != nullptr )
			{
				// how each is doing, for runs nobody watches: where it is, its health, and its target's
				const Coord3D *pos = obj->getPosition();
				const Object *target = k < s_spawnTargets.size() ? TheGameLogic->findObjectByID( s_spawnTargets[k] ) : nullptr;
				const BodyModuleInterface *tb = target != nullptr ? target->getBodyModule() : nullptr;
				const Weapon *w = obj->getCurrentWeapon();
				log( "frame %d: %s #%d at %.0f %.0f, health %.0f/%.0f, target health %.0f, shots fired %d", (int)s_frame,
					obj->getTemplate()->getName().str(), (int)k, pos->x, pos->y, body->getHealth(), body->getMaxHealth(),
					tb != nullptr ? tb->getHealth() : -1.0f, w != nullptr ? (int)w->getLastShotFrame() : -1 );
			}
			if ( s_spawnDamage && body != nullptr && ( since == 90 || since == 180 || since == 270 ) )
			{
				DamageInfo damage;
				damage.in.m_damageType = DAMAGE_UNRESISTABLE;
				damage.in.m_deathType = DEATH_NORMAL;
				damage.in.m_sourceID = INVALID_ID;
				damage.in.m_amount = since == 270 ? body->getMaxHealth() * 2.0f : body->getMaxHealth() * 0.3f;
				obj->attemptDamage( &damage );
			}
		}
	}

	// Tiberium Project @feature America's core base around the view, in the states its art has: whole,
	// damaged, really damaged, one being built (its construction runs from nothing to done) and one
	// destroyed a while in; this adds to the game
	if ( s_baseFrame >= 0 && s_frame == s_baseFrame && TheThingFactory != nullptr && ThePlayerList != nullptr
		&& ThePlayerList->getLocalPlayer() != nullptr && TheTacticalView != nullptr )
	{
		enum { WHOLE, DAMAGED, REALLY_DAMAGED, BUILDING, VICTIM };
		struct Placed { const char *name; Real dx, dy, angle; Int state; };
		static const Placed THINGS[] = {
			{ "AmericaCommandCenter", -150.0f, 250.0f, 0.0f, WHOLE }, { "AmericaPowerPlant", 0.0f, 260.0f, 0.0f, WHOLE },
			{ "AmericaBarracks", 120.0f, 260.0f, 0.0f, WHOLE }, { "AmericaWarFactory", 290.0f, 250.0f, 0.0f, WHOLE },
			{ "AmericaSupplyCenter", 440.0f, 250.0f, 0.0f, WHOLE },
			{ "AmericaPowerPlant", -150.0f, -10.0f, 0.0f, BUILDING }, { "AmericaBarracks", -20.0f, 0.0f, 0.0f, DAMAGED },
			{ "AmericaWarFactory", 140.0f, -10.0f, 0.0f, REALLY_DAMAGED }, { "AmericaPowerPlant", 290.0f, -10.0f, 0.0f, VICTIM },
			{ "AmericaCommandCenter", -60.0f, -230.0f, 0.0f, BUILDING }, { "AmericaSupplyCenter", 180.0f, -230.0f, 0.0f, DAMAGED } };
		Team *team = ThePlayerList->getLocalPlayer()->getDefaultTeam();
		const Coord3D center = TheTacticalView->getPosition();
		Int made = 0;
		for ( Int k = 0; k < (Int)( sizeof( THINGS ) / sizeof( THINGS[0] ) ); ++k )
		{
			const ThingTemplate *tmpl = TheThingFactory->findTemplate( THINGS[k].name );
			Object *obj = tmpl != nullptr && team != nullptr ? TheThingFactory->newObject( tmpl, team ) : nullptr;
			if ( obj == nullptr )
				continue;
			Coord3D p = center;
			p.x += THINGS[k].dx;
			p.y += THINGS[k].dy;
			p.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( p.x, p.y ) : 0.0f;
			obj->setPosition( &p );
			obj->setOrientation( THINGS[k].angle );
			BodyModuleInterface *body = obj->getBodyModule();
			if ( THINGS[k].state == DAMAGED && body != nullptr )
				body->setInitialHealth( 45 );
			else if ( THINGS[k].state == REALLY_DAMAGED && body != nullptr )
				body->setInitialHealth( 15 );
			else if ( THINGS[k].state == BUILDING )
			{
				obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION ) );
				obj->setConstructionPercent( 0.0f );
				obj->setModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED ) );
				s_baseBuilding.push_back( obj->getID() );
			}
			else if ( THINGS[k].state == VICTIM )
				s_baseVictim = obj->getID();
			++made;
		}
		log( "frame %d: %d structures of America's base around %.0f %.0f", (int)s_frame, (int)made, center.x, center.y );
	}
	if ( s_baseFrame >= 0 && s_frame > s_baseFrame && TheGameLogic != nullptr )
	{
		const Real pct = 100.0f * (Real)( s_frame - s_baseFrame ) / (Real)BASE_BUILD_FRAMES;
		for ( size_t k = 0; k < s_baseBuilding.size(); ++k )
		{
			Object *building = TheGameLogic->findObjectByID( s_baseBuilding[k] );
			if ( building == nullptr )
				continue;
			if ( pct < 100.0f )
				building->setConstructionPercent( pct );
			else
			{
				building->setConstructionPercent( CONSTRUCTION_COMPLETE );
				building->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION ) );
				building->clearModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED ) );
			}
		}
		if ( pct >= 100.0f && !s_baseBuilding.empty() )
		{
			s_baseBuilding.clear();
			log( "frame %d: the structures being built are done", (int)s_frame );
		}
		if ( s_frame == s_baseFrame + BASE_KILL_FRAMES && s_baseVictim != INVALID_ID )
		{
			Object *victim = TheGameLogic->findObjectByID( s_baseVictim );
			if ( victim != nullptr )
			{
				s_baseVictimPos = *victim->getPosition();
				victim->kill();
				log( "frame %d: a structure destroyed", (int)s_frame );
			}
			s_baseVictim = INVALID_ID;
		}
		// what stands where it was a moment later (its collapse, debris)
		if ( s_frame == s_baseFrame + BASE_KILL_FRAMES + 60 )
		{
			std::string standing;	// (not "near": a macro in the Windows headers)
			for ( Object *o = TheGameLogic->getFirstObject(); o != nullptr; o = o->getNextObject() )
			{
				const Coord3D *q = o->getPosition();
				const Real dx = q->x - s_baseVictimPos.x, dy = q->y - s_baseVictimPos.y;
				if ( dx * dx + dy * dy < 80.0f * 80.0f && standing.size() < 400 )
					standing += std::string( " " ) + o->getTemplate()->getName().str();
			}
			log( "frame %d: where it stood:%s", (int)s_frame, standing.c_str() );
		}
	}

	if ( s_fireFrame >= 0 && s_frame == s_fireFrame + 30 && TheParticleSystemManager != nullptr )
		log( "frame %d: %u particles in %u systems", (int)s_frame, (unsigned)TheParticleSystemManager->getParticleCount(),
			(unsigned)TheParticleSystemManager->getAllParticleSystems().size() );

	// the logic's CRC, read only
	if ( s_crcFrame >= 0 && !s_crcLogged && inGame && (Int)TheGameLogic->getFrame() >= s_crcFrame )
	{
		s_crcLogged = TRUE;
		const UnsignedInt crc = TheGameLogic->getCRC( CRC_RECALC );
		log( "logic frame %u: CRC %08X", (unsigned)TheGameLogic->getFrame(), (unsigned)crc );
	}

	// screenshots, each with the average frame time since the one before (the first span starts
	// in game, past the map load); the screenshot's own cost falls in the next span's first frame
	const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if ( s_spanStartFrame == 0 && inGame )
	{
		s_spanStartFrame = s_frame;
		s_spanStart = now;
	}
	// (none from the quit frame on: on a fast machine more would keep the quit waiting forever)
	const Bool quitReached = s_quitFrame > 0 && s_frame > s_quitFrame;
	if ( s_shotInterval > 0 && TheDisplay != nullptr && s_frame % s_shotInterval == 0 && s_frame >= s_shotStart && !quitReached )
	{
		double msPerFrame = 0.0;
		if ( s_spanStartFrame > 0 && s_frame > s_spanStartFrame )
			msPerFrame = std::chrono::duration<double, std::milli>( now - s_spanStart ).count() / ( s_frame - s_spanStartFrame );
		TheDisplay->takeScreenShot( SCREENSHOT_PNG );
		s_lastShotFrame = s_frame;
		s_lastShotTime = now;
		++s_shotCount;
		log( "frame %d: screenshot %d (logic frame %u, in game %d), %.2f ms per frame since frame %d", (int)s_frame, (int)s_shotCount,
			TheGameLogic != nullptr ? (unsigned)TheGameLogic->getFrame() : 0u, inGame ? 1 : 0, msPerFrame, (int)s_spanStartFrame );
		s_spanStartFrame = s_frame + 1;
		s_spanStart = std::chrono::steady_clock::now();
	}

	// quit, once the last screenshot has had time to be written: its own thread encodes and writes
	// it, which takes a second or two at high resolutions, many frames on a fast machine
	const Bool shotPending = s_shotCount > 0 && ( s_frame < s_lastShotFrame + 15
		|| std::chrono::steady_clock::now() - s_lastShotTime < std::chrono::seconds( 5 ) );
	if ( s_quitFrame > 0 && !s_quitIssued && s_frame >= s_quitFrame && !shotPending && TheGameEngine != nullptr )
	{
		log( "frame %d: quitting", (int)s_frame );
		TheGameEngine->setQuitting( TRUE );
		s_quitIssued = TRUE;
	}
}
