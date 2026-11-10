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
#include "Common/SpecialPower.h"
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
#include "Common/Upgrade.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/PartitionManager.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/CreateModule.h"
#include "Common/BuildAssistant.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Weapon.h"
#include "Common/DrawModule.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Mouse.h"
#if RTS_ZEROHOUR
#include "Common/Science.h"
#include "GameClient/AbramSpawnMenu.h"
#include "GameClient/AbramTeamMenu.h"
#include "GameClient/TiberiumHUD.h"
#include "GameClient/TiberiumSidebar.h"
#include "GameLogic/AbramCheats.h"
#if RTS_ZEROHOUR
#include "GameClient/TiberiumZoneView.h"
#include "GameLogic/TiberiumZones.h"
#endif
#include "GameLogic/Module/BaseConstructionUpdate.h"
#include "GameLogic/Module/ToggleAbilityUpdate.h"
#include "GameLogic/Module/SuppressionUpdate.h"
#include "GameLogic/Module/GrabSpecialAbilityUpdate.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"
#include "GameLogic/Module/DockUpdate.h"
#endif
#include "GameClient/GameWindow.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindowManager.h"
#include "GameNetwork/GameInfo.h"
#include "Common/MessageStream.h"
#include "Common/Money.h"
#include "Common/NameKeyGenerator.h"

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
	// -testSpawn: the objects to place (a row, or rows of six), what to do with them, and what was made
	Int s_spawnFrame = -1;
	std::vector<std::string> s_spawnNames;
	Bool s_spawnMove = FALSE, s_spawnFight = FALSE, s_spawnDamage = FALSE, s_spawnExplode = FALSE;
	Bool s_spawnRear = FALSE;		// "rear": the fight targets face away (their backs to the spawned objects)
	std::string s_spawnTarget = "ChinaTankBattleMaster";	// "target=Template": the fight targets ("A/B/C": for each, the first it can attack)
	Bool s_spawnBack = FALSE;		// "back": told to move to a spot close behind them (backing up)
	Bool s_spawnDrive = FALSE;		// "drive": the fight targets drive over the spawned objects (mines)
	Bool s_spawnRide = FALSE;		// "ride": the others get into the first spawned object that takes them (passengers)
	Bool s_spawnHarvest = FALSE;	// "harvest": harvesters go to work, as a supply center's new ones do
	Bool s_spawnMutual = FALSE;		// "mutual": the fight targets attack the spawned objects too
	Bool s_spawnFoeAI = FALSE;		// "foe=ai": the fight targets are the skirmish AI's (a real enemy), not the first other player's
	Bool s_spawnEachTarget = FALSE;	// Tiberium Project @feature (the test) "eachtarget": the k-th object gets the k-th kind of target= (A/B/C)
	Bool s_spawnNoFire = FALSE;		// Tiberium Project @feature (the test) "nofire": the fight targets stand there, nobody told to attack
	Real s_spawnTargetDistance = 160.0f;	// "dist=N": how far ahead the fight targets stand
	Real s_spawnSpacing = 70.0f;	// between the objects of a row ("space=N": structures need more)
	Int s_spawnBuildFrames = 0;		// "build=N": structures built up over N frames, then sold over N
	std::vector<ObjectID> s_spawned, s_spawnTargets;
	ObjectID s_baseVictim = INVALID_ID;
	Coord3D s_baseVictimPos;
	const Int BASE_BUILD_FRAMES = 600;
	const Int BASE_KILL_FRAMES = 300;
	// -testUI: steps on the command bar, the selection and the cursor, each at its frame
	struct UIStep { Int frame; std::string action, arg; };
	std::vector<UIStep> s_uiSteps;
	Bool s_uiMouse = FALSE;
	Real s_uiMouseX = 0.5f, s_uiMouseY = 0.5f;
	std::string s_side;		// -testSide: the local player's side in a skirmish on the test map
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
	// Tiberium Project @bugfix In game the test's frames are steps of the logic, not drawn frames:
	// a fast machine draws two or more frames for each step (the logic runs at 30 a second), which
	// cut every spawn, status line and quit frame short there. Frame times stay per drawn frame.
	Int s_drawnFrame = 0;
	UnsignedInt s_lastLogicFrame = 0xFFFFFFFFu;
	Int s_lastShotFrame = -1000;
	std::chrono::steady_clock::time_point s_lastShotTime;
	std::chrono::steady_clock::time_point s_spanStart;	// frame timing: since the last screenshot (or the first frame)
	Int s_spanStartFrame = 0;
	Int s_spanStartDrawn = 0;
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
		s_spawnExplode = parts[2].find( "explode" ) != std::string::npos;	// the killing blow blows them up (their exploded death)
		s_spawnRear = parts[2].find( "rear" ) != std::string::npos;
		const size_t tgt = parts[2].find( "target=" );
		if ( tgt != std::string::npos )
		{
			s_spawnTarget = parts[2].substr( tgt + 7 );
			const size_t end = s_spawnTarget.find( ' ' );
			if ( end != std::string::npos )
				s_spawnTarget = s_spawnTarget.substr( 0, end );
		}
		s_spawnBack = parts[2].find( "back" ) != std::string::npos;
		s_spawnDrive = parts[2].find( "drive" ) != std::string::npos;
		s_spawnRide = parts[2].find( "ride" ) != std::string::npos;
		s_spawnHarvest = parts[2].find( "harvest" ) != std::string::npos;
		s_spawnMutual = parts[2].find( "mutual" ) != std::string::npos;
		s_spawnFoeAI = parts[2].find( "foe=ai" ) != std::string::npos;
		s_spawnEachTarget = parts[2].find( "eachtarget" ) != std::string::npos;
		s_spawnNoFire = parts[2].find( "nofire" ) != std::string::npos;
		const size_t dist = parts[2].find( "dist=" );
		if ( dist != std::string::npos )
			s_spawnTargetDistance = (Real)atof( parts[2].c_str() + dist + 5 );
		const size_t build = parts[2].find( "build=" );
		if ( build != std::string::npos )
			s_spawnBuildFrames = atoi( parts[2].c_str() + build + 6 );
		const size_t space = parts[2].find( "space=" );
		if ( space != std::string::npos )
			s_spawnSpacing = (Real)atof( parts[2].c_str() + space + 6 );
	}
	if ( s_spawnFrame >= 0 && !s_spawnNames.empty() )
		s_active = TRUE;
}
// "frame:action[=arg];...": select[=index] (the spawned objects), command=Button (a targeted command,
// as from the bar), click=slot (a command bar button, from 1), mouse=x,y (the cursor, as fractions of
// the screen), bar (the buttons shown, to the log), money=amount (for the local player),
// build=Template[,dx,dy] (the selected dozer builds it at the view's centre), sell (the selection),
// power=Button[,dx,dy] (the selection uses a location power at dx,dy from the first spawned object),
// upgrade=Upgrade (the local player's, or each spawned object's), players (each player's money and
// objects, to the log)
void TiberiumTest::setUI( const char *spec )
{
	if ( spec == nullptr )
		return;
	for ( const std::string &part : split( spec, ';' ) )
	{
		const std::string::size_type colon = part.find( ':' );
		if ( colon == std::string::npos )
			continue;
		UIStep step;
		step.frame = atoi( part.substr( 0, colon ).c_str() );
		std::string rest = part.substr( colon + 1 );
		const std::string::size_type eq = rest.find( '=' );
		step.action = rest.substr( 0, eq );
		step.arg = eq == std::string::npos ? std::string() : rest.substr( eq + 1 );
		s_uiSteps.push_back( step );
	}
	if ( !s_uiSteps.empty() )
		s_active = TRUE;
}
void TiberiumTest::setSide( const char *side ) { if ( side != nullptr && *side != '\0' ) { s_side = side; s_active = TRUE; } }
const char *TiberiumTest::getSide() { return s_side.empty() ? nullptr : s_side.c_str(); }
static unsigned int s_rules = 0;
void TiberiumTest::setRules( unsigned int rules ) { s_rules = rules; }
unsigned int TiberiumTest::getRules() { return s_rules; }
static Int s_players = 2;
void TiberiumTest::setPlayers( Int players ) { if ( players >= 2 ) s_players = players; }
Int TiberiumTest::getPlayers() { return s_players; }
void TiberiumTest::setReveal( Bool reveal ) { s_reveal = reveal; if ( reveal ) s_active = TRUE; }
static Bool s_noDraw = FALSE;
void TiberiumTest::setNoDraw( Bool noDraw ) { s_noDraw = noDraw; if ( noDraw ) s_active = TRUE; }
Bool TiberiumTest::isNoDraw() { return s_noDraw; }
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
	++s_drawnFrame;
	if ( TheGameLogic != nullptr && TheGameLogic->isInGame() )
	{
		const UnsignedInt logicFrame = TheGameLogic->getFrame();
		if ( logicFrame == s_lastLogicFrame )
			return;	// (drawn again, the logic not stepped since)
		s_lastLogicFrame = logicFrame;
	}
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

	// the game's slots and their starts, once (read only: a check of a test skirmish's players)
	static Bool s_slotsLogged = FALSE;
	if ( !s_slotsLogged && inGame && s_frame >= 2 && ThePlayerList != nullptr && TheGameInfo != nullptr )
	{
		s_slotsLogged = TRUE;
		std::string starts;
		Int players = 0;
		for ( Int i = 0; i < MAX_SLOTS; ++i )
		{
			const GameSlot *slot = TheGameInfo->getConstSlot( i );
			if ( slot == nullptr || !slot->isOccupied() || ThePlayerList->getPlayerFromSlotIndex( i ) == nullptr )
				continue;
			++players;
			char buf[32];
			snprintf( buf, sizeof(buf), " %d:%d", i, slot->getStartPos() );
			starts += buf;
		}
		log( "frame %d: %d players in slots (slot:start)%s; %d players in all", (int)s_frame, players, starts.c_str(), ThePlayerList->getPlayerCount() );
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
		const Real spacing = s_spawnSpacing > 1.0f ? s_spawnSpacing : 70.0f;
		for ( Int k = 0; k < count; ++k )
		{
			const ThingTemplate *tmpl = TheThingFactory->findTemplate( s_spawnNames[k].c_str() );
			Object *obj = tmpl != nullptr && team != nullptr ? TheThingFactory->newObject( tmpl, team ) : nullptr;
			if ( obj == nullptr )
			{
				log( "frame %d: no object %s", (int)s_frame, s_spawnNames[k].c_str() );
				continue;
			}
			// one row (fighting: their targets stand ahead), or rows of six going away from the view
			const Int perRow = s_spawnFight ? count : 6;
			const Int col = k % perRow, row = k / perRow;
			const Int inRow = ( count - row * perRow ) < perRow ? ( count - row * perRow ) : perRow;
			Coord3D p = center;
			p.x += ( col - ( inRow - 1 ) * 0.5f ) * spacing;
			p.y += row * ( spacing > 90.0f ? spacing : 90.0f ) - 25.0f;
			p.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( p.x, p.y ) : 0.0f;
			obj->setPosition( &p );
			obj->setOrientation( PI * 0.5f );
			s_spawned.push_back( obj->getID() );
			// a structure that is not to be built up is as one the map placed: built (a supply center
			// takes deliveries only once it is); Tiberium Project @tweak a unit as one a factory made (what it is
			// given as it is made: GrantUpgradeCreate, the Firehawk's bomb load)
			if ( s_spawnBuildFrames <= 0 || !obj->isKindOf( KINDOF_STRUCTURE ) )
			{
				for ( BehaviorModule **m = obj->getBehaviorModules(); *m != nullptr; ++m )
				{
					CreateModuleInterface *create = (*m)->getCreate();
					if ( create != nullptr )
						create->onBuildComplete();
				}
			}
#if RTS_ZEROHOUR
			if ( s_spawnHarvest && obj->getAIUpdateInterface() != nullptr && obj->getAIUpdateInterface()->getSupplyTruckAIInterface() != nullptr )
				obj->getAIUpdateInterface()->getSupplyTruckAIInterface()->setForceWantingState( TRUE );
#endif
		}
		// the targets: one of Zero Hour's tanks ahead of each, for another player, enemies both ways
		// (a test map's spawned units can fall to the neutral side, which counts its own as allies,
		// and splash damage, a shell bursting by its target, skips allies)
		if ( s_spawnFight && !s_spawned.empty() )
		{
			const Object *first = TheGameLogic->findObjectByID( s_spawned[0] );
			Player *ours = first != nullptr ? first->getControllingPlayer() : nullptr;
			// the target kinds, in order of preference (each object gets the first it can attack)
			std::vector<const ThingTemplate *> kinds;
			for ( const std::string &name : split( s_spawnTarget.c_str(), '/' ) )
			{
				const ThingTemplate *kind = TheThingFactory->findTemplate( name.c_str() );
				if ( kind != nullptr )
					kinds.push_back( kind );
				else
					log( "fight targets: no object %s", name.c_str() );
			}
			const ThingTemplate *tt = kinds.empty() ? nullptr : kinds[0];
			Player *foe = nullptr;
			for ( Int n = 0; n < ThePlayerList->getPlayerCount() && foe == nullptr && tt != nullptr; ++n )
			{
				Player *candidate = ThePlayerList->getNthPlayer( n );
				if ( candidate == nullptr || candidate == ours || candidate->getDefaultTeam() == nullptr )
					continue;
				if ( s_spawnFoeAI && !candidate->isSkirmishAIPlayer() )
					continue;
				Object *probe = TheThingFactory->newObject( tt, candidate->getDefaultTeam() );
				if ( probe != nullptr && probe->getControllingPlayer() != ours )
					foe = probe->getControllingPlayer();
				if ( probe != nullptr )
					TheGameLogic->destroyObject( probe );
			}
			if ( foe != nullptr && ours != nullptr )
			{
				foe->setPlayerRelationship( ours, ENEMIES );
				ours->setPlayerRelationship( foe, ENEMIES );
			}
			log( "fight targets: player %d against player %d", foe != nullptr ? (int)foe->getPlayerIndex() : -1,
				ours != nullptr ? (int)ours->getPlayerIndex() : -1 );
			for ( size_t k = 0; k < s_spawned.size() && tt != nullptr; ++k )
			{
				const Object *obj = TheGameLogic->findObjectByID( s_spawned[k] );
				Player *owner = foe != nullptr ? foe : ThePlayerList->getNeutralPlayer();
				Object *target = nullptr;
				for ( size_t n = 0; n < kinds.size() && obj != nullptr && owner != nullptr; ++n )
				{
					// (Tiberium Project: "eachtarget": the k-th object's kind is the k-th, whatever it can attack)
					if ( s_spawnEachTarget && n != k % kinds.size() )
						continue;
					Object *candidate = TheThingFactory->newObject( kinds[n], owner->getDefaultTeam() );
					if ( candidate == nullptr )
						continue;
					Coord3D q = *obj->getPosition();
					q.y += s_spawnTargetDistance;
					q.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( q.x, q.y ) : 0.0f;
					candidate->setPosition( &q );
					candidate->setOrientation( s_spawnRear ? PI * 0.5f : -PI * 0.5f );
					// the first kind it can attack, else the first kind (a unit that can attack none of them)
					const CanAttackResult able = obj->getAbleToAttackSpecificObject( ATTACK_NEW_TARGET, candidate, CMD_FROM_SCRIPT );
					if ( s_spawnEachTarget )
					{
						target = candidate;
						break;
					}
					if ( able >= ATTACKRESULT_POSSIBLE_AFTER_MOVING || n + 1 == kinds.size() )
					{
						if ( able < ATTACKRESULT_POSSIBLE_AFTER_MOVING )
						{
							TheGameLogic->destroyObject( candidate );
							candidate = TheThingFactory->newObject( kinds[0], owner->getDefaultTeam() );
							if ( candidate != nullptr )
							{
								candidate->setPosition( &q );
								candidate->setOrientation( s_spawnRear ? PI * 0.5f : -PI * 0.5f );
							}
							log( "fight targets: #%d %s can attack none of them", (int)k, obj->getTemplate()->getName().str() );
						}
						target = candidate;
						break;
					}
					TheGameLogic->destroyObject( candidate );
				}
				// (one for each spawned object, in its order: none where none could be made)
				s_spawnTargets.push_back( target != nullptr ? target->getID() : INVALID_ID );
				if ( target != nullptr )
					log( "fight targets: #%d %s against %s", (int)k, obj->getTemplate()->getName().str(), target->getTemplate()->getName().str() );
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
			{
				// gone (a mine set off): how its target is doing
				const Object *target = s_spawnDrive && k < s_spawnTargets.size() ? TheGameLogic->findObjectByID( s_spawnTargets[k] ) : nullptr;
				if ( target != nullptr && since % 30 == 0 && target->getBodyModule() != nullptr )
					log( "frame %d: #%d gone, its target %s health %.0f/%.0f", (int)s_frame, (int)k, target->getTemplate()->getName().str(),
						target->getBodyModule()->getHealth(), target->getBodyModule()->getMaxHealth() );
				continue;
			}
			AIUpdateInterface *ai = obj->getAIUpdateInterface();
			if ( since == 10 && s_spawnDrive && k < s_spawnTargets.size() )
			{
				// the target drives onto it
				Object *target = TheGameLogic->findObjectByID( s_spawnTargets[k] );
				AIUpdateInterface *tai = target != nullptr ? target->getAIUpdateInterface() : nullptr;
				if ( tai != nullptr )
				{
					Coord3D dest = *obj->getPosition();
					tai->aiMoveToPosition( &dest, CMD_FROM_SCRIPT );
				}
			}
			else if ( since == 10 && ai != nullptr && s_spawnRide )
			{
				// into the first spawned object that takes it
				for ( size_t j = 0; j < s_spawned.size(); ++j )
				{
					Object *ride = j != k ? TheGameLogic->findObjectByID( s_spawned[j] ) : nullptr;
					if ( ride != nullptr && ride->getContain() != nullptr && ride->getContain()->isValidContainerFor( obj, TRUE ) )
					{
						ai->aiEnter( ride, CMD_FROM_SCRIPT );
						break;
					}
				}
			}
			else if ( since == 10 && ai != nullptr )
			{
				if ( s_spawnFight && k < s_spawnTargets.size() )
				{
					Object *target = s_spawnNoFire ? nullptr : TheGameLogic->findObjectByID( s_spawnTargets[k] );
					if ( target != nullptr )
						ai->aiForceAttackObject( target, NO_MAX_SHOTS_LIMIT, CMD_FROM_SCRIPT );
					AIUpdateInterface *tai = target != nullptr ? target->getAIUpdateInterface() : nullptr;
					if ( s_spawnMutual && tai != nullptr )
						tai->aiForceAttackObject( obj, NO_MAX_SHOTS_LIMIT, CMD_FROM_SCRIPT );
				}
				else if ( s_spawnBack )
				{
					// a spot close behind it, whichever way it faces
					Coord3D dest = *obj->getPosition();
					dest.x -= 90.0f * Cos( obj->getOrientation() );
					dest.y -= 90.0f * Sin( obj->getOrientation() );
					ai->aiMoveToPosition( &dest, CMD_FROM_SCRIPT );
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
				const DamageInfo* last = tb != nullptr ? tb->getLastDamageInfo() : nullptr;
				std::string conditions;
				const ModelConditionFlags flags = obj->getDrawable() != nullptr ? obj->getDrawable()->getModelConditionFlags() : ModelConditionFlags();
				for ( Int bit = 0; bit < flags.size(); ++bit )
				{
					const char *name = flags.getBitNameIfSet( bit );
					if ( name != nullptr )
						conditions += std::string( conditions.empty() ? "" : " " ) + name;
				}
				// the attribute modifiers on it and on its target (Zero Hour's), as speed armour damage rate range
				char modifiers[512] = "";
#if RTS_ZEROHOUR
				{
					const Object *who[2] = { obj, target };
					for ( Int w = 0; w < 2; ++w )
					{
						if ( who[w] == nullptr || !who[w]->hasAttributeModifiers() )
							continue;
						char one[80];
						snprintf( one, sizeof( one ), ", %s modifiers %.2f %.2f %.2f %.2f %.2f", w == 0 ? "its" : "target",
							who[w]->getAttributeModifierScalar( MODIFIER_SPEED ), who[w]->getAttributeModifierScalar( MODIFIER_ARMOR ),
							who[w]->getAttributeModifierScalar( MODIFIER_DAMAGE ), who[w]->getAttributeModifierScalar( MODIFIER_RATE_OF_FIRE ),
							who[w]->getAttributeModifierScalar( MODIFIER_RANGE ) );
						strlcat( modifiers, one, sizeof( modifiers ) );
					}
				}
#endif
				// the animation its draw plays now, and the frame (for sounds on frames)
#if RTS_ZEROHOUR
				{
					AsciiString anim;
					Real animFrame = 0.0f;
					const Drawable *draw = obj->getDrawable();
					for ( DrawModule const **dm = draw != nullptr ? draw->getDrawModules() : nullptr; dm != nullptr && *dm; ++dm )
					{
						const ObjectDrawInterface *odi = (*dm)->getObjectDrawInterface();
						if ( odi != nullptr && odi->clientOnly_getAnimationFrame( &anim, &animFrame ) )
						{
							char one[96];
							snprintf( one, sizeof( one ), ", playing %s at %.0f", anim.str(), animFrame );
							strlcat( modifiers, one, sizeof( modifiers ) );
							break;
						}
					}
				}
#endif
				{
					char facing[48];
					snprintf( facing, sizeof( facing ), ", facing %.0f", obj->getOrientation() * 180.0f / PI );
					strlcat( modifiers, facing, sizeof( modifiers ) );
					if ( target != nullptr )
					{
						snprintf( facing, sizeof( facing ), ", target at %.0f %.0f", target->getPosition()->x, target->getPosition()->y );
						strlcat( modifiers, facing, sizeof( modifiers ) );
					}
					// each slot's last shot (several turrets firing at once)
					{
						std::string shots;
						for ( Int s = 0; s < WEAPONSLOT_COUNT; ++s )
						{
							const Weapon *sw = obj->getWeaponInWeaponSlot( (WeaponSlotType)s );
							if ( sw == nullptr )
								continue;
							char one[24];
							snprintf( one, sizeof( one ), "%s%c:%d", shots.empty() ? "" : " ", 'A' + s, (int)sw->getLastShotFrame() );
							shots += one;
						}
						if ( shots.find( ' ' ) != std::string::npos )
						{
							strlcat( modifiers, ", slots ", sizeof( modifiers ) );
							strlcat( modifiers, shots.c_str(), sizeof( modifiers ) );
						}
					}
					// (what its AI is doing, and what holds it)
					if ( obj->getAI() != nullptr )
					{
						snprintf( facing, sizeof( facing ), ", ai state %d", (int)obj->getAI()->getCurrentStateID() );
						strlcat( modifiers, facing, sizeof( modifiers ) );
					}
					if ( obj->getContainedBy() != nullptr )
					{
						snprintf( facing, sizeof( facing ), ", inside %s", obj->getContainedBy()->getTemplate()->getName().str() );
						strlcat( modifiers, facing, sizeof( modifiers ) );
					}
					// (its target held by an EMP or a stasis field)
					if ( target != nullptr && ( target->isDisabledByType( DISABLED_EMP ) || target->isDisabledByType( DISABLED_PARALYZED ) ) )
					{
						snprintf( facing, sizeof( facing ), ", target %s", target->isDisabledByType( DISABLED_EMP ) ? "EMP" : "paralyzed" );
						strlcat( modifiers, facing, sizeof( modifiers ) );
					}
					const DamageInfo *own = body->getLastDamageInfo();
					if ( own != nullptr && own->in.m_sourceID != INVALID_ID )
					{
						snprintf( facing, sizeof( facing ), ", hit last %.1f type %d", own->out.m_actualDamageDealt, (int)own->in.m_damageType );
						strlcat( modifiers, facing, sizeof( modifiers ) );
					}
				}
				log( "frame %d: %s #%d at %.0f %.0f, health %.0f/%.0f, target health %.0f (last hit %.1f, type %d, from %d), "
					"shots fired %d, weapon %s, status %d, range %.0f, conditions %s%s%s", (int)s_frame,
					obj->getTemplate()->getName().str(), (int)k, pos->x, pos->y, body->getHealth(), body->getMaxHealth(),
					tb != nullptr ? tb->getHealth() : -1.0f, last != nullptr ? last->out.m_actualDamageDealt : -1.0f,
					last != nullptr ? (int)last->in.m_damageType : -1, last != nullptr ? (int)last->in.m_sourceID : -1,
					w != nullptr ? (int)w->getLastShotFrame() : -1, w != nullptr ? w->getName().str() : "none",
					w != nullptr ? (int)w->getStatus() : -1, w != nullptr ? w->getAttackRange( obj ) : 0.0f, conditions.empty() ? "none" : conditions.c_str(), modifiers,
					obj->testStatus( OBJECT_STATUS_STEALTHED ) ? ", stealthed" : "" );
			}
			// built up by its construction's progress, then sold (unbuilt the same way)
			if ( s_spawnBuildFrames > 0 && obj->isKindOf( KINDOF_STRUCTURE ) && since <= 2 * s_spawnBuildFrames + 1 )
			{
				if ( since == 1 )
				{
					obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION ) );
					obj->clearAndSetModelConditionFlags( MAKE_MODELCONDITION_MASK( MODELCONDITION_AWAITING_CONSTRUCTION ),
						MAKE_MODELCONDITION_MASK2( MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED ) );
				}
				const Int b = s_spawnBuildFrames;
				const Real pct = since <= b ? 100.0f * since / b : 100.0f * ( 2 * b - since ) / b;
				obj->setConstructionPercent( pct < 0.0f ? 0.0f : pct );
				if ( since == b )
				{
					obj->clearStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_UNDER_CONSTRUCTION ) );
					obj->clearModelConditionFlags( MAKE_MODELCONDITION_MASK2( MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED ) );
					obj->setConstructionPercent( CONSTRUCTION_COMPLETE );
				}
				else if ( since == b + b / 4 )
				{
					// sold: Zero Hour unbuilds a sold structure the same way, with SOLD set
					obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_SOLD ) );
					obj->setModelConditionFlags( MAKE_MODELCONDITION_MASK( MODELCONDITION_PARTIALLY_CONSTRUCTED ) );
				}
			}
			if ( s_spawnDamage && body != nullptr && ( since == 90 || since == 180 || since == 270 ) )
			{
				DamageInfo damage;
				damage.in.m_damageType = DAMAGE_UNRESISTABLE;
				damage.in.m_deathType = ( since == 270 && s_spawnExplode ) ? DEATH_EXPLODED : DEATH_NORMAL;
				damage.in.m_sourceID = INVALID_ID;
				damage.in.m_amount = since == 270 ? body->getMaxHealth() * 2.0f : body->getMaxHealth() * 0.3f;
				obj->attemptDamage( &damage );
			}
		}
	}

	// Tiberium Project @feature -testUI: the command bar driven as a player would (client side, but its
	// commands go to the logic like a player's: not with -testCRC)
	if ( s_uiMouse && TheMouse != nullptr && TheDisplay != nullptr )
		TheMouse->setPosition( (Int)( s_uiMouseX * TheDisplay->getWidth() ), (Int)( s_uiMouseY * TheDisplay->getHeight() ) );
	// press=Layout.wnd:ButtonName: a window's button pressed as a click presses it, in a shell screen too
	for ( size_t n = 0; n < s_uiSteps.size() && TheWindowManager != nullptr; ++n )
	{
		const UIStep &step = s_uiSteps[n];
		if ( step.frame != s_frame || step.action != "press" )
			continue;
		GameWindow *win = TheWindowManager->winGetWindowFromId( nullptr, TheNameKeyGenerator->nameToKey( step.arg.c_str() ) );
		if ( win != nullptr && win->winGetOwner() != nullptr )
		{
			TheWindowManager->winSendSystemMsg( win->winGetOwner(), GBM_SELECTED, (WindowMsgData)win, win->winGetWindowId() );
			log( "frame %d: pressed %s", (int)s_frame, step.arg.c_str() );
		}
		else
			log( "frame %d: no window %s to press", (int)s_frame, step.arg.c_str() );
	}
	for ( size_t n = 0; n < s_uiSteps.size() && inGame; ++n )
	{
		const UIStep &step = s_uiSteps[n];
		if ( step.frame != s_frame )
			continue;
		if ( step.action == "select" && TheInGameUI != nullptr && TheGameLogic != nullptr )
		{
			TheInGameUI->deselectAllDrawables();
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_CREATE_SELECTED_GROUP_NO_SOUND );
			msg->appendBooleanArgument( TRUE );
			const Int only = step.arg.empty() ? -1 : atoi( step.arg.c_str() );
			Int picked = 0;
			if ( step.arg.compare( 0, 5, "name:" ) == 0 )
			{
				// "select=name:Template": every one of the local player's objects of that kind (what an ability made)
				const std::string kind = step.arg.substr( 5 );
				for ( Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject() )
				{
					if ( obj->getDrawable() == nullptr || obj->getControllingPlayer() != ThePlayerList->getLocalPlayer()
						|| obj->getTemplate()->getName().compareNoCase( kind.c_str() ) != 0 )
						continue;
					TheInGameUI->selectDrawable( obj->getDrawable() );
					msg->appendObjectIDArgument( obj->getID() );
					++picked;
				}
			}
			for ( size_t k = 0; k < s_spawned.size() && step.arg.compare( 0, 5, "name:" ) != 0; ++k )
			{
				Object *obj = TheGameLogic->findObjectByID( s_spawned[k] );
				if ( obj == nullptr || obj->getDrawable() == nullptr || ( only >= 0 && (Int)k != only ) )
					continue;
				TheInGameUI->selectDrawable( obj->getDrawable() );
				msg->appendObjectIDArgument( obj->getID() );
				++picked;
			}
			log( "frame %d: ui: selected %d", (int)s_frame, (int)picked );
		}
#if RTS_ZEROHOUR
		else if ( step.action == "toggle" && TheGameLogic != nullptr && TheThingFactory != nullptr )
		{
			// Tiberium Project @feature (the test) "toggle=Template": the first object of that template, any player's
			// (an AI's too), switches each of its toggles (ToggleAbilityUpdate: a C&C4 unit burrowing)
			const ThingTemplate *tmpl = TheThingFactory->findTemplate( step.arg.c_str() );
			Object *found = nullptr;
			for ( Object *obj = TheGameLogic->getFirstObject(); obj != nullptr && found == nullptr; obj = obj->getNextObject() )
				if ( tmpl != nullptr && obj->getTemplate()->isEquivalentTo( tmpl ) && !obj->isEffectivelyDead() )
					found = obj;
			Int switched = 0;
			static const NameKeyType key = NAMEKEY( "ToggleAbilityUpdate" );
			for ( BehaviorModule **m = found != nullptr ? found->getBehaviorModules() : nullptr; m != nullptr && *m != nullptr; ++m )
			{
				if ( (*m)->getModuleNameKey() != key )
					continue;
				ToggleAbilityUpdate *t = static_cast<ToggleAbilityUpdate *>( *m );
				const SpecialPowerTemplate *sp = t->friend_getSpecialPowerTemplate();
				if ( t->initiateIntentToDoSpecialPower( sp, nullptr, nullptr, nullptr, 0 ) )
					++switched;
			}
			log( "frame %d: ui: toggle %s: %d switched%s", (int)s_frame, step.arg.c_str(), (int)switched, found == nullptr ? " (none found)" : "" );
		}
#endif
		else if ( step.action == "vet" && TheGameLogic != nullptr )
		{
			// the spawned objects' ranks
			std::string ranks;
			for ( size_t k = 0; k < s_spawned.size(); ++k )
			{
				Object *obj = TheGameLogic->findObjectByID( s_spawned[k] );
				char one[64];
				snprintf( one, sizeof( one ), " %d:%d", (int)k, obj != nullptr && obj->getExperienceTracker() != nullptr ? (int)obj->getVeterancyLevel() : -1 );
				ranks += one;
			}
			log( "frame %d: ui: ranks%s", (int)s_frame, ranks.c_str() );
		}
		else if ( step.action == "enter" && TheGameLogic != nullptr )
		{
			// "enter=k,j": the k-th spawned object told to get into the j-th (a transport, a building)
			Int k = -1, j = -1;
			sscanf( step.arg.c_str(), "%d,%d", &k, &j );
			Object *obj = k >= 0 && k < (Int)s_spawned.size() ? TheGameLogic->findObjectByID( s_spawned[k] ) : nullptr;
			Object *into = j >= 0 && j < (Int)s_spawned.size() ? TheGameLogic->findObjectByID( s_spawned[j] ) : nullptr;
			if ( obj != nullptr && into != nullptr && obj->getAIUpdateInterface() != nullptr )
				obj->getAIUpdateInterface()->aiEnter( into, CMD_FROM_PLAYER );
			log( "frame %d: ui: enter %s%s", (int)s_frame, step.arg.c_str(), obj != nullptr && into != nullptr ? "" : " (no such object)" );
		}
#if RTS_ZEROHOUR
		else if ( step.action == "suppress" && TheGameLogic != nullptr )
		{
			// Tiberium Project @feature (the test) "suppress=k,amount,ms": that much suppression on the k-th spawned object for
			// that long, as a weapon's (SuppressionUpdate: Kane's Wrath's infantry pinned down)
			Int k = -1;
			Real amount = 0.0f, ms = 0.0f;
			sscanf( step.arg.c_str(), "%d,%f,%f", &k, &amount, &ms );
			Object *obj = k >= 0 && k < (Int)s_spawned.size() ? TheGameLogic->findObjectByID( s_spawned[k] ) : nullptr;
			static const NameKeyType suppressionKey = NAMEKEY( "SuppressionUpdate" );
			SuppressionUpdate *sup = obj != nullptr ? (SuppressionUpdate *)obj->findUpdateModule( suppressionKey ) : nullptr;
			if ( sup != nullptr )
				sup->addSuppression( amount, (UnsignedInt)( ms * LOGICFRAMES_PER_SECOND / 1000.0f ) );
			log( "frame %d: ui: suppress %s%s", (int)s_frame, step.arg.c_str(), sup != nullptr ? "" : " (no such object or no SuppressionUpdate)" );
		}
		else if ( step.action == "exitfrom" && TheGameLogic != nullptr )
		{
			// Tiberium Project @feature (the test) "exitfrom=k,Template": one of that kind made as the k-th spawned object's
			// production makes one, out of its exit (an airfield's parking space, reserved for it), whatever its build rules
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			const Int k = f.empty() ? -1 : atoi( f[0].c_str() );
			Object *factory = k >= 0 && k < (Int)s_spawned.size() ? TheGameLogic->findObjectByID( s_spawned[k] ) : nullptr;
			ExitInterface *exit = factory != nullptr ? factory->getObjectExitInterface() : nullptr;
			const ThingTemplate *kind = f.size() > 1 ? TheThingFactory->findTemplate( f[1].c_str() ) : nullptr;
			const char *how = "";
			if ( exit == nullptr || kind == nullptr )
				how = " (no exit, or no such kind)";
			else
			{
				const ExitDoorType door = exit->reserveDoorForExit( kind, nullptr );
				if ( door == DOOR_NONE_AVAILABLE )
					how = " (no door free: its parking spaces taken)";
				else
				{
					Object *made = TheThingFactory->newObject( kind, factory->getControllingPlayer()->getDefaultTeam() );
					made->setProducer( factory );
					exit->exitObjectViaDoor( made, door );
					for ( BehaviorModule **m = made->getBehaviorModules(); *m; ++m )
					{
						CreateModuleInterface *create = (*m)->getCreate();
						if ( create != nullptr )
							create->onBuildComplete();
					}
				}
			}
			log( "frame %d: ui: exitfrom %s%s", (int)s_frame, step.arg.c_str(), how );
		}
		else if ( ( step.action == "produce" || step.action == "attack" || step.action == "stop" ) && TheGameLogic != nullptr
			&& ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
		{
			// Tiberium Project @feature (the test) "produce=k,Template": the k-th spawned object (a factory) queues one of
			// that kind, paid for as the player's click is; "attack=name:Template,Target": the local player's first of
			// that kind attacks the first of the target's kind there is (anyone's); "stop=name:Template": it is told to stop
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			Object *obj = nullptr;
			if ( !f.empty() && f[0].compare( 0, 5, "name:" ) == 0 )
			{
				const std::string kind = f[0].substr( 5 );
				for ( Object *o = TheGameLogic->getFirstObject(); o != nullptr && obj == nullptr; o = o->getNextObject() )
					if ( o->getControllingPlayer() == ThePlayerList->getLocalPlayer() && !o->isEffectivelyDead()
						&& o->getTemplate()->getName().compareNoCase( kind.c_str() ) == 0 )
						obj = o;
			}
			else if ( !f.empty() )
			{
				const Int k = atoi( f[0].c_str() );
				obj = k >= 0 && k < (Int)s_spawned.size() ? TheGameLogic->findObjectByID( s_spawned[k] ) : nullptr;
			}
			const char *how = obj != nullptr ? "" : " (no such object)";
			if ( obj != nullptr && step.action == "produce" )
			{
				ProductionUpdateInterface *pu = obj->getProductionUpdateInterface();
				const ThingTemplate *kind = f.size() > 1 ? TheThingFactory->findTemplate( f[1].c_str() ) : nullptr;
				if ( pu == nullptr || kind == nullptr )
					how = " (no production, or no such kind)";
				else if ( pu->canQueueCreateUnit( kind ) != CANMAKE_OK )
					how = " (refused: the queue)";
				else if ( !pu->queueCreateUnit( kind, pu->requestUniqueUnitID() ) )
				{
					// (the build rules': its prerequisites, money, the factory's buttons; else no parking space for it)
					static char why[64];
					snprintf( why, sizeof( why ), " (not queued: can make %d)", (int)TheBuildAssistant->canMakeUnit( obj, kind ) );
					how = why;
				}
			}
			else if ( obj != nullptr && step.action == "attack" && obj->getAI() != nullptr )
			{
				Object *target = nullptr;
				for ( Object *o = TheGameLogic->getFirstObject(); o != nullptr && target == nullptr && f.size() > 1; o = o->getNextObject() )
					if ( !o->isEffectivelyDead() && o->getTemplate()->getName().compareNoCase( f[1].c_str() ) == 0 )
						target = o;
				if ( target != nullptr )
					obj->getAI()->aiAttackObject( target, NO_MAX_SHOTS_LIMIT, CMD_FROM_PLAYER );
				else
					how = " (no target)";
			}
			else if ( obj != nullptr && step.action == "stop" && obj->getAI() != nullptr )
				obj->getAI()->aiIdle( CMD_FROM_PLAYER );
			log( "frame %d: ui: %s %s%s", (int)s_frame, step.action.c_str(), step.arg.c_str(), how );
		}
#endif
		else if ( step.action == "hurt" && TheGameLogic != nullptr )
		{
			// Tiberium Project @feature (the test) "hurt=k,amount,TYPE": the k-th spawned object takes that much
			// damage of the type (its armour applies), from nobody
			Int k = -1;
			Real amount = 0.0f;
			char type[64] = "EXPLOSION";
			sscanf( step.arg.c_str(), "%d,%f,%63s", &k, &amount, type );
			Object *obj = k >= 0 && k < (Int)s_spawned.size() ? TheGameLogic->findObjectByID( s_spawned[k] ) : nullptr;
			Int t = 0;
			while ( DamageTypeFlags::s_bitNameList[t] != nullptr && stricmp( DamageTypeFlags::s_bitNameList[t], type ) != 0 )
				++t;
			BodyModuleInterface *body = obj != nullptr ? obj->getBodyModule() : nullptr;
			const Real before = body != nullptr ? body->getHealth() : 0.0f;
			if ( body != nullptr && DamageTypeFlags::s_bitNameList[t] != nullptr )
			{
				DamageInfo damage;
				damage.in.m_damageType = (DamageType)t;
				damage.in.m_deathType = DEATH_NORMAL;
				damage.in.m_sourceID = INVALID_ID;
				damage.in.m_amount = amount;
				obj->attemptDamage( &damage );
			}
			log( "frame %d: ui: hurt %s: health %.0f to %.0f%s", (int)s_frame, step.arg.c_str(), before,
				body != nullptr ? body->getHealth() : 0.0f, body != nullptr && DamageTypeFlags::s_bitNameList[t] != nullptr ? "" : " (no such object or type)" );
		}
#if RTS_ZEROHOUR
		else if ( step.action == "modifier" && TheGameLogic != nullptr && TheAttributeModifierStore != nullptr )
		{
			// Tiberium Project @feature (the test) "modifier=k,Name,ms": the attribute modifier on the k-th spawned object for
			// that long (a debuff to see a cleanse take off)
			Int k = -1, ms = 0;
			char name[128] = "";
			sscanf( step.arg.c_str(), "%d,%127[^,],%d", &k, name, &ms );
			Object *obj = k >= 0 && k < (Int)s_spawned.size() ? TheGameLogic->findObjectByID( s_spawned[k] ) : nullptr;
			const AttributeModifierTemplate *tmpl = TheAttributeModifierStore->find( AsciiString( name ) );
			if ( obj != nullptr && tmpl != nullptr )
				obj->applyAttributeModifier( tmpl, INVALID_ID, (UnsignedInt)( ms * LOGICFRAMES_PER_SECOND / 1000 ) );
			log( "frame %d: ui: modifier %s%s; it has %s", (int)s_frame, step.arg.c_str(),
				obj == nullptr ? " (no such object)" : tmpl == nullptr ? " (no such modifier)" : "",
				obj != nullptr && obj->hasAttributeModifiers() ? "modifiers" : "none" );
		}
#endif
		else if ( step.action == "ready" && TheGameLogic != nullptr )
		{
			// Tiberium Project @feature (the test) "ready=Template": the special powers of every object of the
			// template ready now (a superweapon's seven minutes skipped)
			Int count = 0;
			for ( Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject() )
			{
				if ( obj->getTemplate()->getName().compareNoCase( step.arg.c_str() ) != 0 )
					continue;
				for ( BehaviorModule **m = obj->getBehaviorModules(); *m != nullptr; ++m )
				{
					SpecialPowerModuleInterface *sp = (*m)->getSpecialPower();
					if ( sp != nullptr )
					{
						sp->setReadyFrame( TheGameLogic->getFrame() );
						++count;
					}
				}
			}
			log( "frame %d: ui: ready %s: %d powers", (int)s_frame, step.arg.c_str(), (int)count );
		}
		else if ( step.action == "grant" && TheGameLogic != nullptr && TheUpgradeCenter != nullptr )
		{
			// Tiberium Project @feature (the test) "grant=k,Upgrade[,target]": the k-th spawned object (or the fight
			// target made for it) is given the object upgrade (a building's field, as its power gives it)
			Int k = -1;
			char name[128] = "";
			char which[16] = "";
			sscanf( step.arg.c_str(), "%d,%127[^,],%15s", &k, name, which );
			ObjectID id = INVALID_ID;
			if ( k >= 0 && k < (Int)s_spawned.size() )
				id = strcmp( which, "target" ) == 0 ? ( k < (Int)s_spawnTargets.size() ? s_spawnTargets[k] : INVALID_ID ) : s_spawned[k];
			Object *obj = TheGameLogic->findObjectByID( id );
			const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade( AsciiString( name ) );
			if ( obj != nullptr && upgrade != nullptr )
				obj->giveUpgrade( upgrade );
			log( "frame %d: ui: grant %s%s", (int)s_frame, step.arg.c_str(), obj != nullptr && upgrade != nullptr ? "" : " (no such object or upgrade)" );
		}
		else if ( ( step.action == "moveto" || step.action == "kill" ) && TheGameLogic != nullptr )
		{
			// "moveto=k,x,y": the k-th spawned object told to move there; "kill=k": it is destroyed (killed);
			// Tiberium Project @feature (the test) "kill=name:Template": the first of the local player's objects of
			// that kind (what a structure made: a Nod hub's turret); "moveto=k,@j": to where the j-th spawned object
			// stands (a crate it picks up)
			Int k = -1;
			Real x = 0.0f, y = 0.0f;
			sscanf( step.arg.c_str(), "%d,%f,%f", &k, &x, &y );
			Object *obj = k >= 0 && k < (Int)s_spawned.size() ? TheGameLogic->findObjectByID( s_spawned[k] ) : nullptr;
			const size_t at = step.arg.find( ",@" );
			if ( at != std::string::npos )
			{
				const Int j = atoi( step.arg.c_str() + at + 2 );
				const Object *there = j >= 0 && j < (Int)s_spawned.size() ? TheGameLogic->findObjectByID( s_spawned[j] ) : nullptr;
				if ( there != nullptr )
				{
					x = there->getPosition()->x;
					y = there->getPosition()->y;
				}
			}
			if ( step.arg.compare( 0, 5, "name:" ) == 0 && ThePlayerList != nullptr )
			{
				const std::string kind = step.arg.substr( 5 );
				for ( Object *o = TheGameLogic->getFirstObject(); o != nullptr && obj == nullptr; o = o->getNextObject() )
				{
					if ( o->getControllingPlayer() == ThePlayerList->getLocalPlayer() && !o->isEffectivelyDead()
						&& o->getTemplate()->getName().compareNoCase( kind.c_str() ) == 0 )
						obj = o;
				}
			}
			if ( obj != nullptr && step.action == "kill" )
				obj->kill();
			else if ( obj != nullptr && obj->getAIUpdateInterface() != nullptr )
			{
				Coord3D to;
				to.x = x;
				to.y = y;
				to.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( x, y ) : 0.0f;
				obj->getAIUpdateInterface()->aiMoveToPosition( &to, CMD_FROM_PLAYER );
			}
			log( "frame %d: ui: %s %s%s", (int)s_frame, step.action.c_str(), step.arg.c_str(), obj != nullptr ? "" : " (no such object)" );
		}
		else if ( step.action == "command" && TheControlBar != nullptr && TheInGameUI != nullptr )
		{
			const CommandButton *button = TheControlBar->findCommandButton( AsciiString( step.arg.c_str() ) );
			TheInGameUI->setGUICommand( button );
			log( "frame %d: ui: command %s%s", (int)s_frame, step.arg.c_str(), button != nullptr ? "" : " (no such button)" );
		}
		else if ( step.action == "click" && TheControlBar != nullptr )
		{
			GameWindow *win = TheControlBar->getCommandWindow( atoi( step.arg.c_str() ) - 1 );
			const CommandButton *button = win != nullptr ? (const CommandButton *)GadgetButtonGetData( win ) : nullptr;
			const Bool usable = win != nullptr && !win->winIsHidden() && win->winGetEnabled();
			if ( usable )
				TheControlBar->processContextSensitiveButtonClick( win, GBM_SELECTED );
			log( "frame %d: ui: click slot %s: %s%s", (int)s_frame, step.arg.c_str(),
				button != nullptr ? button->getName().str() : "(empty)", usable ? "" : " (not usable)" );
		}
		else if ( step.action == "pick" && TheTacticalView != nullptr && TheGameLogic != nullptr )
		{
			// a player's click on each spawned object (at its centre, raised by half its height): what the
			// view's pick finds there, and whether the object may be selected at all
			for ( size_t k = 0; k < s_spawned.size(); ++k )
			{
				Object *obj = TheGameLogic->findObjectByID( s_spawned[k] );
				if ( obj == nullptr )
					continue;
				Coord3D at = *obj->getPosition();
				at.z += obj->getGeometryInfo().getMaxHeightAbovePosition() * 0.5f;
				ICoord2D screen;
				TheTacticalView->worldToScreen( &at, &screen );
				Drawable *hit = TheTacticalView->pickDrawable( &screen, FALSE, PICK_TYPE_SELECTABLE );
				Drawable *any = TheTacticalView->pickDrawable( &screen, FALSE, (PickType)0xFF );
				const Drawable *mine = obj->getDrawable();
				log( "frame %d: ui: pick %s at %d,%d: selectable %d, kind %d, hidden %d; picked %s, any %s",
					(int)s_frame, obj->getTemplate()->getName().str(), (int)screen.x, (int)screen.y,
					(int)obj->isSelectable(), (int)obj->isKindOf( KINDOF_SELECTABLE ),
					mine != nullptr ? (int)mine->isDrawableEffectivelyHidden() : -1,
					hit != nullptr && hit->getObject() != nullptr ? hit->getObject()->getTemplate()->getName().str() : "nothing",
					any != nullptr && any->getObject() != nullptr ? any->getObject()->getTemplate()->getName().str() : "nothing" );
			}
		}
#if RTS_ZEROHOUR
		else if ( step.action == "sidebar" )
		{
			// a click on the sidebar at fx,fy (fractions of the screen), ",right" for a right click; and what it shows
			// (or on a part by what it is: "tab,N", "queue,N", "item,N", each with ",right" too)
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			Bool took = FALSE;
			// (",shift" last: as with shift held)
			const Bool shift = !f.empty() && f.back() == "shift";
			if ( shift )
				f.pop_back();
			TiberiumSidebar::setTestShift( shift );
			if ( f.size() >= 2 && ( f[0] == "tab" || f[0] == "queue" || f[0] == "item" ) )
				took = TiberiumSidebar::testClickPart( f[0].c_str(), atoi( f[1].c_str() ), f.size() > 2 && f[2] == "right" );
			else if ( f.size() >= 2 )
				took = TiberiumSidebar::testClick( (Real)atof( f[0].c_str() ), (Real)atof( f[1].c_str() ), f.size() > 2 && f[2] == "right" );
			TiberiumSidebar::setTestShift( FALSE );
			char what[768];
			TiberiumSidebar::describe( what, sizeof( what ) );
			log( "frame %d: ui: sidebar click %s: %s; %s", (int)s_frame, step.arg.c_str(), took ? "taken" : "missed", what );
		}
		else if ( step.action == "hud" )
		{
			// what the HUD shows; with a part, a click on it first: "button,N" (0 menu, 1 idle worker, 2 beacon,
			// 3 general), "row,N" and "slot,N" (the commands above the panel, the abilities in it), "group,N"
			// (the kinds of a selection of several), "portrait"; each with ",right" for a right click
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			const Bool clicked = !f.empty() && !f[0].empty();
			Bool took = FALSE;
			if ( clicked )
				took = TiberiumHUD::testClickPart( f[0].c_str(), f.size() > 1 ? atoi( f[1].c_str() ) : 0, f.size() > 2 && f[2] == "right" );
			char what[640];
			TiberiumHUD::describe( what, sizeof( what ) );
			log( "frame %d: ui: hud%s%s%s: %s", (int)s_frame, clicked ? " click " : "", clicked ? step.arg.c_str() : "",
				clicked ? ( took ? " (taken)" : " (missed)" ) : "", what );
		}
#if RTS_ZEROHOUR
		else if ( step.action == "zones" )
		{
			// C&C4's zones (-testRules): "show" / "hide" their borders over the world; each zone's start or not,
			// holders (players' bits) and middle
			if ( step.arg == "show" || step.arg == "hide" )
				TiberiumZoneView::setShown( step.arg == "show" );
			std::string all;
			for ( Int z = 0; z < TiberiumZones::getCount(); ++z )
			{
				char one[120];
				const Coord3D *c = TiberiumZones::getCenter( z );
				snprintf( one, sizeof( one ), " z%d%s holders %x at %.0f %.0f;", (int)z, TiberiumZones::isHome( z ) ? " start" : "",
					(unsigned)TiberiumZones::getOwners( z ), c->x, c->y );
				all += one;
			}
			log( "frame %d: ui: zones mode %d, %d zones, shown %d:%s", (int)s_frame, (int)TiberiumZones::getMode(),
				(int)TiberiumZones::getCount(), (int)TiberiumZoneView::isShown(), all.c_str() );
		}
		else if ( step.action == "zonebuild" && TheThingFactory != nullptr && TheTacticalView != nullptr && ThePlayerList != nullptr
			&& ThePlayerList->getLocalPlayer() != nullptr && TheBuildAssistant != nullptr )
		{
			// whether the local player may put Template at the view's centre + dx,dy: the zones' answer and the
			// whole check's (LBC_OUTSIDE_ZONE is 8)
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			const ThingTemplate *tmpl = f.empty() ? nullptr : TheThingFactory->findTemplate( f[0].c_str() );
			Coord3D at = TheTacticalView->getPosition();
			at.x += f.size() > 1 ? (Real)atof( f[1].c_str() ) : 0.0f;
			at.y += f.size() > 2 ? (Real)atof( f[2].c_str() ) : 0.0f;
			at.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( at.x, at.y ) : 0.0f;
			Player *me = ThePlayerList->getLocalPlayer();
			const Bool may = tmpl != nullptr && TiberiumZones::mayBuild( me, tmpl, &at );
			const Int code = tmpl != nullptr ? (Int)TheBuildAssistant->isLocationLegalToBuild( &at, tmpl, 0.0f, 0, nullptr, me ) : -1;
			log( "frame %d: ui: zonebuild %s at %.0f %.0f (zone %d): may %d, code %d", (int)s_frame, step.arg.c_str(), at.x, at.y,
				(int)TiberiumZones::zoneAt( &at ), (int)may, (int)code );
		}
		else if ( step.action == "arrive" && TheThingFactory != nullptr && TheGameLogic != nullptr && ThePlayerList != nullptr
			&& ThePlayerList->getLocalPlayer() != nullptr )
		{
			// Template made by the local player's first factory (its door, then its arrival in a zone);
			// "Template,dx,dy" with the factory's rally point at the view's centre + dx,dy
			// (a test, not a player's command: not with -testCRC)
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			const ThingTemplate *tmpl = f.empty() ? nullptr : TheThingFactory->findTemplate( f[0].c_str() );
			Player *me = ThePlayerList->getLocalPlayer();
			Object *factory = nullptr;
			for ( Object *o = TheGameLogic->getFirstObject(); o != nullptr && factory == nullptr; o = o->getNextObject() )
				if ( o->getControllingPlayer() == me && !o->isEffectivelyDead() && o->getProductionUpdateInterface() != nullptr && o->getObjectExitInterface() != nullptr )
					factory = o;
			if ( tmpl != nullptr && factory != nullptr )
			{
				if ( f.size() > 2 && TheTacticalView != nullptr )
				{
					Coord3D rally = TheTacticalView->getPosition();
					rally.x += (Real)atof( f[1].c_str() );
					rally.y += (Real)atof( f[2].c_str() );
					rally.z = TheTerrainLogic->getGroundHeight( rally.x, rally.y );
					factory->getObjectExitInterface()->setRallyPoint( &rally );
				}
				Object *made = TheThingFactory->newObject( tmpl, me->getDefaultTeam() );
				Coord3D door = *factory->getPosition();
				door.x += factory->getGeometryInfo().getBoundingCircleRadius() + 20.0f;
				made->setPosition( &door );
				made->setProducer( factory );
				s_spawned.push_back( made->getID() );
				log( "frame %d: ui: arrive %s from %s at %.0f %.0f", (int)s_frame, tmpl->getName().str(), factory->getTemplate()->getName().str(),
					door.x, door.y );
			}
			else
				log( "frame %d: ui: arrive %s: %s", (int)s_frame, step.arg.c_str(), tmpl == nullptr ? "no such object" : "no factory" );
		}
		else if ( step.action == "where" && TheGameLogic != nullptr )
		{
			// where the spawned objects are, and whether held
			std::string all;
			for ( size_t i = 0; i < s_spawned.size(); ++i )
			{
				const Object *o = TheGameLogic->findObjectByID( s_spawned[i] );
				if ( o == nullptr )
					continue;
				char one[160];
				snprintf( one, sizeof( one ), " %s at %.0f %.0f%s (zone %d);", o->getTemplate()->getName().str(), o->getPosition()->x,
					o->getPosition()->y, o->isDisabled() ? " held" : "", (int)TiberiumZones::zoneAt( o->getPosition() ) );
				all += one;
			}
			log( "frame %d: ui: where%s", (int)s_frame, all.c_str() );
		}
#endif
		else if ( step.action == "yard" && TheGameLogic != nullptr && ThePlayerList != nullptr )
		{
			// the local player's construction yard (Kane's Wrath's base): its state; "place,dx,dy" places the
			// ready structure at the nearest legal spot to the yard's position + dx,dy, by the message a
			// player's click on the ground sends (MSG_TIBERIUM_BASE_PLACE); "place,dx,dy,anywhere" picks the
			// spot by the ground alone, so the logic's own check of the build areas is what is tested
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			Player *me = ThePlayerList->getLocalPlayer();
			Object *yard = nullptr;
			BaseConstructionUpdate *base = nullptr;
			for ( Object *o = TheGameLogic->getFirstObject(); o != nullptr && base == nullptr; o = o->getNextObject() )
			{
				if ( o->getControllingPlayer() == me && !o->isEffectivelyDead() && BaseConstructionUpdate::find( o ) != nullptr )
				{
					yard = o;
					base = BaseConstructionUpdate::find( o );
				}
			}
			if ( base == nullptr )
			{
				log( "frame %d: ui: yard: none", (int)s_frame );
			}
			else
			{
				const ThingTemplate *tmpl = base->getBuilding();
				Bool sent = FALSE;
				Coord3D at = *yard->getPosition();
				if ( !f.empty() && f[0] == "place" && base->isReady() && TheBuildAssistant != nullptr )
				{
					at.x += f.size() > 1 ? (Real)atof( f[1].c_str() ) : 0.0f;
					at.y += f.size() > 2 ? (Real)atof( f[2].c_str() ) : 0.0f;
					const Coord3D wanted = at;
					const UnsignedInt opts = BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP;
					Bool found = FALSE;
					for ( Int ring = 0; ring <= 16 && !found; ++ring )
					{
						for ( Int k = 0; k < ( ring > 0 ? 8 * ring : 1 ) && !found; ++k )
						{
							const Real a = ring > 0 ? 2.0f * PI * k / ( 8 * ring ) : 0.0f;
							Coord3D p = wanted;
							p.x += ring * 50.0f * Cos( a );
							p.y += ring * 50.0f * Sin( a );
							p.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( p.x, p.y ) : 0.0f;
							if ( TheBuildAssistant->isLocationLegalToBuild( &p, tmpl, 0.0f, opts, nullptr, me ) == LBC_OK
								&& ( f.size() > 3 && f[3] == "anywhere" ? TRUE : base->isWithinBuildRadius( &p ) ) )
							{
								at = p;
								found = TRUE;
							}
						}
					}
					if ( found && TheMessageStream != nullptr )
					{
						GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_TIBERIUM_BASE_PLACE );
						msg->appendObjectIDArgument( yard->getID() );
						msg->appendLocationArgument( at );
						msg->appendRealArgument( 0.0f );
						sent = TRUE;
					}
				}
				if ( !f.empty() && f[0] == "check" && tmpl != nullptr && TheBuildAssistant != nullptr )
				{
					// "check,dx,dy": the placement test the player's cursor gets there (the shroud included)
					Coord3D p = *yard->getPosition();
					p.x += f.size() > 1 ? (Real)atof( f[1].c_str() ) : 0.0f;
					p.y += f.size() > 2 ? (Real)atof( f[2].c_str() ) : 0.0f;
					p.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( p.x, p.y ) : 0.0f;
					const LegalBuildCode lbc = TheBuildAssistant->isLocationLegalToBuild( &p, tmpl, 0.0f,
						BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP | BuildAssistant::SHROUD_REVEALED | BuildAssistant::IGNORE_STEALTHED,
						nullptr, me );
					log( "frame %d: ui: yard check at %.0f %.0f: legal code %d, within the build areas %d", (int)s_frame, p.x, p.y,
						(int)lbc, (int)base->isWithinBuildRadius( &p ) );
				}
				log( "frame %d: ui: yard %s: %s, %s %.0f%%%s", (int)s_frame, yard->getTemplate()->getName().str(),
					tmpl != nullptr ? tmpl->getName().str() : "idle", base->isReady() ? "ready" : "building", base->getPercent(),
					sent ? " (placed)" : "" );
				if ( sent )
					log( "frame %d: ui: yard place at %.0f %.0f", (int)s_frame, at.x, at.y );
			}
		}
#endif
		else if ( step.action == "ground" && TheMessageStream != nullptr && TheDisplay != nullptr )
		{
			// a player's left click at fx,fy (fractions of the screen), as the mouse sends it: button down, then
			// up (the translators make the click of them; a structure in hand is placed there)
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			ICoord2D at;
			at.x = (Int)( ( f.size() > 0 ? atof( f[0].c_str() ) : 0.5 ) * TheDisplay->getWidth() );
			at.y = (Int)( ( f.size() > 1 ? atof( f[1].c_str() ) : 0.5 ) * TheDisplay->getHeight() );
			if ( TheMouse != nullptr )
				TheMouse->setPosition( at.x, at.y );
			GameMessage *down = TheMessageStream->appendMessage( GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN );
			down->appendPixelArgument( at );
			down->appendIntegerArgument( 0 );
			down->appendIntegerArgument( 0 );
			GameMessage *up = TheMessageStream->appendMessage( GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP );
			up->appendPixelArgument( at );
			up->appendIntegerArgument( 0 );
			up->appendIntegerArgument( 0 );
			Coord3D world;
			const Bool onGround = TheTacticalView != nullptr && TheTacticalView->screenToTerrain( &at, &world );
			log( "frame %d: ui: ground click at %d,%d (world %.0f %.0f)%s", (int)s_frame, (int)at.x, (int)at.y,
				onGround ? world.x : 0.0f, onGround ? world.y : 0.0f, onGround ? "" : " (off the ground)" );
		}
		else if ( step.action == "groundat" && TheMessageStream != nullptr && TheTacticalView != nullptr && TheGameLogic != nullptr )
		{
			// Tiberium Project @feature (the test) a player's left click on the ground dx,dy from the first spawned
			// object, at where the view shows that spot (the second click of a power used in steps)
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			Object *from = !s_spawned.empty() ? TheGameLogic->findObjectByID( s_spawned[0] ) : nullptr;
			if ( from != nullptr )
			{
				Coord3D world = *from->getPosition();
				world.x += f.size() > 0 ? (Real)atof( f[0].c_str() ) : 0.0f;
				world.y += f.size() > 1 ? (Real)atof( f[1].c_str() ) : 0.0f;
				world.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( world.x, world.y ) : world.z;
				ICoord2D at;
				TheTacticalView->worldToScreen( &world, &at );
				if ( TheMouse != nullptr )
					TheMouse->setPosition( at.x, at.y );
				GameMessage *down = TheMessageStream->appendMessage( GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN );
				down->appendPixelArgument( at );
				down->appendIntegerArgument( 0 );
				down->appendIntegerArgument( 0 );
				GameMessage *up = TheMessageStream->appendMessage( GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP );
				up->appendPixelArgument( at );
				up->appendIntegerArgument( 0 );
				up->appendIntegerArgument( 0 );
				const CommandButton *pending = TheInGameUI != nullptr ? TheInGameUI->getGUICommand() : nullptr;
				log( "frame %d: ui: ground click at %d,%d (world %.0f %.0f), aiming %s", (int)s_frame, (int)at.x, (int)at.y,
					world.x, world.y, pending != nullptr ? pending->getName().str() : "nothing" );
			}
		}
		else if ( step.action == "fx" && TheFXListStore != nullptr && TheGameLogic != nullptr )
		{
			// Tiberium Project @feature (the test) an effect list played dx,dy from the first spawned object, ten
			// units above the ground and facing along +x as a weapon's muzzle would (client side only): to see
			// what its particle systems draw ("fx=<FX list>,dx,dy")
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			const FXList *fx = !f.empty() ? TheFXListStore->findFXList( f[0].c_str() ) : nullptr;
			Object *from = !s_spawned.empty() ? TheGameLogic->findObjectByID( s_spawned[0] ) : nullptr;
			if ( fx != nullptr && from != nullptr )
			{
				Coord3D p = *from->getPosition();
				p.x += f.size() > 1 ? (Real)atof( f[1].c_str() ) : 0.0f;
				p.y += f.size() > 2 ? (Real)atof( f[2].c_str() ) : 0.0f;
				p.z = ( TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( p.x, p.y ) : p.z ) + 10.0f;
				Matrix3D mtx( true );
				mtx.Set_Translation( Vector3( p.x, p.y, p.z ) );
				FXList::doFXPos( fx, &p, &mtx );
				log( "frame %d: ui: effect %s at %.0f %.0f", (int)s_frame, f[0].c_str(), p.x, p.y );
			}
			else
			{
				log( "frame %d: ui: effect %s not played (%s)", (int)s_frame, f.empty() ? "" : f[0].c_str(),
					fx == nullptr ? "no such effect list" : "nothing spawned" );
			}
		}
		else if ( step.action == "mouse" )
		{
			s_uiMouse = TRUE;
			s_uiMouseX = (Real)atof( step.arg.c_str() );
			const std::string::size_type comma = step.arg.find( ',' );
			s_uiMouseY = comma != std::string::npos ? (Real)atof( step.arg.c_str() + comma + 1 ) : 0.5f;
		}
		else if ( step.action == "build" && TheThingFactory != nullptr && TheTacticalView != nullptr )
		{
			// the selected dozer builds Template at the view's centre, offset by dx,dy (its build-up, watched)
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			const ThingTemplate *tmpl = f.empty() ? nullptr : TheThingFactory->findTemplate( f[0].c_str() );
			if ( tmpl != nullptr )
			{
				Coord3D at = TheTacticalView->getPosition();
				at.x += f.size() > 1 ? (Real)atof( f[1].c_str() ) : 0.0f;
				at.y += f.size() > 2 ? (Real)atof( f[2].c_str() ) : 0.0f;
				at.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( at.x, at.y ) : 0.0f;
				// the nearest place it may stand (rings out to 800 units), as a player would pick one
				Object *picker = !s_spawned.empty() ? TheGameLogic->findObjectByID( s_spawned[0] ) : nullptr;
				if ( picker != nullptr && TheBuildAssistant != nullptr )
				{
					const UnsignedInt opts = BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP;
					const Coord3D wanted = at;
					Bool found = TheBuildAssistant->isLocationLegalToBuild( &at, tmpl, 0.0f, opts, picker, picker->getControllingPlayer() ) == LBC_OK;
					for ( Int ring = 1; ring <= 16 && !found; ++ring )
					{
						for ( Int k = 0; k < 8 * ring && !found; ++k )
						{
							const Real a = 2.0f * PI * k / ( 8 * ring );
							Coord3D p = wanted;
							p.x += ring * 50.0f * Cos( a );
							p.y += ring * 50.0f * Sin( a );
							p.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( p.x, p.y ) : 0.0f;
							if ( TheBuildAssistant->isLocationLegalToBuild( &p, tmpl, 0.0f, opts, picker, picker->getControllingPlayer() ) == LBC_OK )
							{
								at = p;
								found = TRUE;
							}
						}
					}
				}
				// the first spawned object (a dozer) builds it, as the logic does a player's placement
				// (a test, not a player's command: not with -testCRC)
				Object *builder = !s_spawned.empty() ? TheGameLogic->findObjectByID( s_spawned[0] ) : nullptr;
				if ( builder != nullptr && TheBuildAssistant != nullptr )
				{
					const Int can = (Int)TheBuildAssistant->canMakeUnit( builder, tmpl );
					const Int where = (Int)TheBuildAssistant->isLocationLegalToBuild( &at, tmpl, 0.0f,
						BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP, builder, builder->getControllingPlayer() );
					Object *made = TheBuildAssistant->buildObjectNow( builder, tmpl, &at, 0.0f, builder->getControllingPlayer() );
					log( "frame %d: ui: build at %.0f %.0f: can make %d, location %d, %s", (int)s_frame, at.x, at.y, can, where,
						made != nullptr ? "started" : "refused" );
				}
			}
			log( "frame %d: ui: build %s%s", (int)s_frame, step.arg.c_str(), tmpl != nullptr ? "" : " (no such object)" );
		}
		else if ( step.action == "power" && TheControlBar != nullptr && TheGameLogic != nullptr )
		{
			// the selection uses a location power (Button) at dx,dy from the first spawned object, as a
			// player's click on the ground does (a test, not a player's command: not with -testCRC)
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			const CommandButton *button = f.empty() ? nullptr : TheControlBar->findCommandButton( AsciiString( f[0].c_str() ) );
			Object *from = !s_spawned.empty() ? TheGameLogic->findObjectByID( s_spawned[0] ) : nullptr;
			if ( button != nullptr && button->getSpecialPowerTemplate() != nullptr && f.size() > 1 && f[1] == "here" )
			{
				// "power=Button,here": a power with no target, used by the selection (as its button's click does)
				GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_SPECIAL_POWER );
				msg->appendIntegerArgument( button->getSpecialPowerTemplate()->getID() );
				msg->appendIntegerArgument( button->getOptions() );
				msg->appendObjectIDArgument( INVALID_ID );
				log( "frame %d: ui: power %s (no target)", (int)s_frame, f[0].c_str() );
			}
			else if ( button != nullptr && button->getSpecialPowerTemplate() != nullptr && f.size() > 1 && f[1].compare( 0, 5, "name:" ) == 0 )
			{
				// Tiberium Project @feature (the test) "power=Button,name:Template": the selection's power aimed at the first
				// object of that kind (anyone's: a friendly Obelisk to charge, an enemy for the reflector attack)
				const std::string kind = f[1].substr( 5 );
				Object *target = nullptr;
				for ( Object *o = TheGameLogic->getFirstObject(); o != nullptr && target == nullptr; o = o->getNextObject() )
					if ( !o->isEffectivelyDead() && o->getTemplate()->getName().compareNoCase( kind.c_str() ) == 0 )
						target = o;
				const char *why = "";
				if ( target != nullptr )
				{
					GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT );
					msg->appendIntegerArgument( button->getSpecialPowerTemplate()->getID() );
					msg->appendObjectIDArgument( target->getID() );
					msg->appendIntegerArgument( button->getOptions() );
					msg->appendObjectIDArgument( INVALID_ID );
					// (what the player's order meets: the target out of sight, the power not ready)
					const Player *me = ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
					if ( me != nullptr && target->getShroudedStatus( me->getPlayerIndex() ) >= OBJECTSHROUD_FOGGED )
						why = " (out of sight: refused)";
				}
				log( "frame %d: ui: power %s at %s #%u%s%s", (int)s_frame, f[0].c_str(), kind.c_str(),
					target != nullptr ? (unsigned)target->getID() : 0u, target != nullptr ? "" : " (none there)", why );
			}
			else if ( button != nullptr && button->getSpecialPowerTemplate() != nullptr && from != nullptr )
			{
				Coord3D at = *from->getPosition();
				at.x += f.size() > 1 ? (Real)atof( f[1].c_str() ) : 0.0f;
				at.y += f.size() > 2 ? (Real)atof( f[2].c_str() ) : 0.0f;
				at.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( at.x, at.y ) : 0.0f;
				GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION );
				msg->appendIntegerArgument( button->getSpecialPowerTemplate()->getID() );
				msg->appendLocationArgument( at );
				msg->appendRealArgument( INVALID_ANGLE );
				msg->appendObjectIDArgument( INVALID_ID );
				msg->appendIntegerArgument( button->getOptions() );
				msg->appendObjectIDArgument( INVALID_ID );
				log( "frame %d: ui: power %s at %.0f %.0f (from %.0f %.0f)", (int)s_frame, f[0].c_str(), at.x, at.y,
					from->getPosition()->x, from->getPosition()->y );
			}
			else
				log( "frame %d: ui: power %s: no such button, power or object", (int)s_frame, step.arg.c_str() );
		}
		else if ( step.action == "upgrade" && TheUpgradeCenter != nullptr && ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
		{
			// a player upgrade for the local player, or an object upgrade for each spawned object
			const UpgradeTemplate *up = TheUpgradeCenter->findUpgrade( step.arg.c_str() );
			if ( up != nullptr && up->getUpgradeType() == UPGRADE_TYPE_PLAYER )
				ThePlayerList->getLocalPlayer()->addUpgrade( up, UPGRADE_STATUS_COMPLETE );
			else if ( up != nullptr )
				for ( size_t k = 0; k < s_spawned.size(); ++k )
				{
					Object *obj = TheGameLogic->findObjectByID( s_spawned[k] );
					if ( obj != nullptr )
						obj->giveUpgrade( up );
				}
			log( "frame %d: ui: upgrade %s%s", (int)s_frame, step.arg.c_str(), up != nullptr ? "" : " (no such upgrade)" );
		}
		else if ( step.action == "players" && ThePlayerList != nullptr )
		{
			// what each player has: money, and its objects by kind, most first (an AI's build-up)
			for ( Int n = 0; n < ThePlayerList->getPlayerCount(); ++n )
			{
				Player *player = ThePlayerList->getNthPlayer( n );
				if ( player == nullptr || player == ThePlayerList->getNeutralPlayer() )
					continue;
				// (and where its base is, and how many of its units are away from it: an AI's attacks)
				struct Tally { std::map<std::string, Int> kinds; Int total; Real sx, sy; Int structures; std::vector<Coord3D> units;
					static void each( Object *obj, void *data )
				{
					Tally *me = (Tally *)data;
					if ( obj->isEffectivelyDead() || obj->isKindOf( KINDOF_PROJECTILE ) )
						return;
					++me->kinds[ obj->getTemplate()->getName().str() ];
					++me->total;
					if ( obj->isKindOf( KINDOF_STRUCTURE ) )
					{
						me->sx += obj->getPosition()->x;
						me->sy += obj->getPosition()->y;
						++me->structures;
					}
					else if ( obj->getAIUpdateInterface() != nullptr && !obj->isKindOf( KINDOF_DOZER ) && !obj->isKindOf( KINDOF_HARVESTER ) )
						me->units.push_back( *obj->getPosition() );
				} };
				Tally tally;
				tally.total = 0;
				tally.sx = tally.sy = 0.0f;
				tally.structures = 0;
				player->iterateObjects( Tally::each, &tally );
				if ( tally.total == 0 )
					continue;
				const Real bx = tally.structures > 0 ? tally.sx / tally.structures : 0.0f;
				const Real by = tally.structures > 0 ? tally.sy / tally.structures : 0.0f;
				Int away = 0;
				for ( size_t u = 0; u < tally.units.size(); ++u )
					if ( tally.structures > 0 && ( tally.units[u].x - bx ) * ( tally.units[u].x - bx ) + ( tally.units[u].y - by ) * ( tally.units[u].y - by ) > 800.0f * 800.0f )
						++away;
				std::vector< std::pair<Int, std::string> > sorted;
				for ( std::map<std::string, Int>::const_iterator it = tally.kinds.begin(); it != tally.kinds.end(); ++it )
					sorted.push_back( std::make_pair( -it->second, it->first ) );
				std::sort( sorted.begin(), sorted.end() );
				std::string all;
				for ( size_t k = 0; k < sorted.size() && k < 14; ++k )
				{
					char one[96];
					snprintf( one, sizeof( one ), " %s %d", sorted[k].second.c_str(), -sorted[k].first );
					all += one;
				}
				AsciiString playerName;
				playerName.translate( player->getPlayerDisplayName() );
				log( "frame %d: ui: player %d (%s, %s) $%u, base at %.0f %.0f, %d units (%d away), %d objects:%s", (int)s_frame, (int)n,
					playerName.str(), player->getPlayerType() == PLAYER_COMPUTER ? "AI" : "human",
					player->getMoney()->countMoney(), bx, by, (int)tally.units.size(), away, (int)tally.total, all.c_str() );
			}
		}
#if RTS_ZEROHOUR
		else if ( step.action == "teammenu" )
		{
			// Abram's Personal Test: the team menu (Ctrl+G), as the key opens it (its buttons: press=AbramPopup.wnd:ButtonChoiceN)
			AbramTeamMenu::open();
			log( "frame %d: ui: team menu", (int)s_frame );
		}
		else if ( step.action == "cheat" )
		{
			// "cheat=N,on": the cheats' command as the team menu sends it (0 freeze AI, 1 money, 2 power, 3 powers)
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_ABRAM_CHEAT );
			msg->appendIntegerArgument( f.empty() ? 0 : atoi( f[0].c_str() ) );
			msg->appendIntegerArgument( f.size() > 1 ? atoi( f[1].c_str() ) : 1 );
			log( "frame %d: ui: cheat %s", (int)s_frame, step.arg.c_str() );
		}
		else if ( step.action == "cheats" && ThePlayerList != nullptr && TheGameLogic != nullptr )
		{
			// what the cheats have done: on or off, and each player's objects (frozen, held in the air), money
			// and power
			for ( Int n = 0; n < ThePlayerList->getPlayerCount(); ++n )
			{
				Player *player = ThePlayerList->getNthPlayer( n );
				if ( player == nullptr || player == ThePlayerList->getNeutralPlayer() )
					continue;
				struct Count { Int all, frozen, held, tinted; static void each( Object *obj, void *data )
				{
					Count *me = (Count *)data;
					if ( obj->isEffectivelyDead() )
						return;
					++me->all;
					me->frozen += obj->isDisabledByType( DISABLED_SCRIPT_DISABLED ) ? 1 : 0;
					me->held += obj->isDisabledByType( DISABLED_HELD ) ? 1 : 0;
					me->tinted += obj->getDrawable() != nullptr && obj->getDrawable()->testTintStatus( TINT_STATUS_DISABLED ) ? 1 : 0;
				} };
				Count count = { 0, 0, 0, 0 };
				player->iterateObjects( Count::each, &count );
				if ( count.all == 0 )
					continue;
				// (and how many of the generals powers, the sciences bought with points, it has)
				Int powers = 0, powersAll = 0;
				const std::vector<AsciiString> names = TheScienceStore->friend_getScienceNames();
				for ( size_t k = 0; k < names.size(); ++k )
				{
					const ScienceType science = TheScienceStore->getScienceFromInternalName( names[k] );
					if ( science == SCIENCE_INVALID || TheScienceStore->getSciencePurchaseCost( science ) <= 0 )
						continue;
					++powersAll;
					powers += player->hasScience( science ) ? 1 : 0;
				}
				log( "frame %d: ui: cheats: player %d (%s) frozen AI %d, unlimited money %d, power %d, powers unlocked %d; %d objects, %d frozen, %d held, %d tinted; $%u, power %d / %d, enough %d; %d of %d generals powers",
					(int)s_frame, (int)n, player->getPlayerType() == PLAYER_COMPUTER ? "AI" : "human", (int)AbramCheats::isFrozenPlayer( player ),
					(int)AbramCheats::hasUnlimitedMoney( n ), (int)AbramCheats::hasUnlimitedPower( n ), (int)AbramCheats::hasUnlockedPowers( n ),
					(int)count.all, (int)count.frozen, (int)count.held, (int)count.tinted, player->getMoney()->countMoney(),
					player->getEnergy()->getConsumption(), player->getEnergy()->getProduction(), (int)player->getEnergy()->hasSufficientPower(),
					(int)powers, (int)powersAll );
			}
		}
		else if ( step.action == "spawnmenu" )
		{
			// the cheats' spawn menu: "spawnmenu" opens it, "spawnmenu=Template" clicks that one's picture, "spawnmenu=?" says what it shows
			if ( step.arg.empty() )
				AbramSpawnMenu::open();
			else if ( step.arg != "?" )
				AbramSpawnMenu::testPick( step.arg.c_str() );
			char text[512];
			AbramSpawnMenu::describe( text, sizeof( text ) );
			log( "frame %d: ui: spawn menu%s%s: %s", (int)s_frame, step.arg.empty() ? "" : " ", step.arg.c_str(), text );
		}
		else if ( step.action == "rground" && TheMessageStream != nullptr && TheDisplay != nullptr )
		{
			// a player's right click at fx,fy (fractions of the screen), as the mouse sends it
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			ICoord2D at;
			at.x = (Int)( ( f.size() > 0 ? atof( f[0].c_str() ) : 0.5 ) * TheDisplay->getWidth() );
			at.y = (Int)( ( f.size() > 1 ? atof( f[1].c_str() ) : 0.5 ) * TheDisplay->getHeight() );
			if ( TheMouse != nullptr )
				TheMouse->setPosition( at.x, at.y );
			GameMessage *down = TheMessageStream->appendMessage( GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN );
			down->appendPixelArgument( at );
			down->appendIntegerArgument( 0 );
			down->appendIntegerArgument( 0 );
			GameMessage *up = TheMessageStream->appendMessage( GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP );
			up->appendPixelArgument( at );
			up->appendIntegerArgument( 0 );
			up->appendIntegerArgument( 0 );
			log( "frame %d: ui: right click at %d,%d", (int)s_frame, (int)at.x, (int)at.y );
		}
#endif
		else if ( step.action == "sell" )
		{
			TheMessageStream->appendMessage( GameMessage::MSG_SELL );
			log( "frame %d: ui: sell", (int)s_frame );
		}
		else if ( step.action == "money" && ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
		{
			ThePlayerList->getLocalPlayer()->getMoney()->deposit( (UnsignedInt)atoi( step.arg.c_str() ), FALSE );
			log( "frame %d: ui: money %s", (int)s_frame, step.arg.c_str() );
		}
		else if ( step.action == "count" && ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
		{
			// how many of an object the local player has (what a factory made); "Template,all": and where each stands
			std::string countName = step.arg;
			const Bool listAll = countName.size() > 4 && countName.compare( countName.size() - 4, 4, ",all" ) == 0;
			if ( listAll )
				countName.resize( countName.size() - 4 );
			const ThingTemplate *tmpl = TheThingFactory != nullptr ? TheThingFactory->findTemplate( countName.c_str() ) : nullptr;
			Int count = 0;
			if ( tmpl != nullptr )
				ThePlayerList->getLocalPlayer()->countObjectsByThingTemplate( 1, &tmpl, FALSE, &count );
			// and where the first of them stands
			struct First { const ThingTemplate *tmpl; Coord3D pos; Bool found; Real built; static void each( Object *obj, void *data )
			{
				First *me = (First *)data;
				if ( !me->found && obj->getTemplate()->isEquivalentTo( me->tmpl ) )
				{
					me->pos = *obj->getPosition();
					me->found = TRUE;
					me->built = obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) ? obj->getConstructionPercent() : -1.0f;
				}
			} };
			First first = { tmpl, Coord3D(), FALSE, -1.0f };
			if ( tmpl != nullptr )
				ThePlayerList->getLocalPlayer()->iterateObjects( First::each, &first );
			char where[96] = "";
			if ( first.found && first.built >= 0.0f )
				snprintf( where, sizeof( where ), ", the first at %.0f %.0f (%.0f%% built)", first.pos.x, first.pos.y, first.built );
			else if ( first.found )
				snprintf( where, sizeof( where ), ", the first at %.0f %.0f", first.pos.x, first.pos.y );
			// (Tiberium Project: and how many there are of anyone's, a crate no one's)
			Int anyone = 0;
			for ( Object *o = TheGameLogic != nullptr && tmpl != nullptr ? TheGameLogic->getFirstObject() : nullptr; o != nullptr; o = o->getNextObject() )
			{
				if ( !o->isDestroyed() && o->getTemplate()->isEquivalentTo( tmpl ) )	// (a crate is ever dead: InactiveBody)
					++anyone;
			}
			log( "frame %d: ui: %d of %s%s%s (%d of anyone's)", (int)s_frame, (int)count, countName.c_str(), tmpl != nullptr ? "" : " (no such object)", where, (int)anyone );
			if ( listAll && tmpl != nullptr )
			{
				struct Each { const ThingTemplate *tmpl; std::string text; Int n; static void each( Object *obj, void *data )
				{
					Each *me = (Each *)data;
					if ( me->n >= 24 || !obj->getTemplate()->isEquivalentTo( me->tmpl ) )
						return;
					char one[64];
					snprintf( one, sizeof( one ), " %.0f %.0f%s;", obj->getPosition()->x, obj->getPosition()->y,
						obj->getVeterancyLevel() > LEVEL_REGULAR ? " (veteran+)" : "" );
					me->text += one;
					++me->n;
				} };
				Each each = { tmpl, std::string(), 0 };
				ThePlayerList->getLocalPlayer()->iterateObjects( Each::each, &each );
				log( "frame %d: ui: %s at%s", (int)s_frame, countName.c_str(), each.text.c_str() );
			}
		}
		else if ( step.action == "cash" && ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
		{
			log( "frame %d: ui: cash %u", (int)s_frame, (unsigned)ThePlayerList->getLocalPlayer()->getMoney()->countMoney() );
		}
#if RTS_ZEROHOUR
		else if ( step.action == "shortcut" && TheControlBar != nullptr && ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
		{
			// "shortcut=Button[,dx,dy]": a shortcut bar power, as its button's click and the click on the ground do:
			// refused when its price is not there (as the button refuses it), else used from the most ready of the
			// local player's objects with it, aimed dx,dy from the first spawned object (none given: no target)
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			const CommandButton *button = f.empty() ? nullptr : TheControlBar->findCommandButton( AsciiString( f[0].c_str() ) );
			Player *me = ThePlayerList->getLocalPlayer();
			const SpecialPowerTemplate *power = button != nullptr ? button->getSpecialPowerTemplate() : nullptr;
			Object *source = power != nullptr ? me->findMostReadyShortcutSpecialPowerOfType( power->getSpecialPowerType() ) : nullptr;
			Object *from = !s_spawned.empty() ? TheGameLogic->findObjectByID( s_spawned[0] ) : nullptr;
			if ( power == nullptr || source == nullptr )
				log( "frame %d: ui: shortcut %s: no such button, power or source", (int)s_frame, step.arg.c_str() );
			else if ( ControlBar::refuseUnaffordablePower( button ) )
				log( "frame %d: ui: shortcut %s refused: $%u and %d points (it costs $%u and %u points)", (int)s_frame, f[0].c_str(),
					(unsigned)me->getMoney()->countMoney(), (int)me->getSupportPoints(), (unsigned)power->getCost(), (unsigned)power->getSupportPoints() );
			else if ( f.size() >= 3 && from != nullptr )
			{
				Coord3D at = *from->getPosition();
				at.x += (Real)atof( f[1].c_str() );
				at.y += (Real)atof( f[2].c_str() );
				at.z = TheTerrainLogic != nullptr ? TheTerrainLogic->getGroundHeight( at.x, at.y ) : 0.0f;
				GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION );
				msg->appendIntegerArgument( power->getID() );
				msg->appendLocationArgument( at );
				msg->appendRealArgument( INVALID_ANGLE );
				msg->appendObjectIDArgument( INVALID_ID );
				msg->appendIntegerArgument( button->getOptions() );
				msg->appendObjectIDArgument( source->getID() );
				log( "frame %d: ui: shortcut %s at %.0f %.0f from %s ($%u, %d points)", (int)s_frame, f[0].c_str(), at.x, at.y,
					source->getTemplate()->getName().str(), (unsigned)me->getMoney()->countMoney(), (int)me->getSupportPoints() );
			}
			else
			{
				GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_SPECIAL_POWER );
				msg->appendIntegerArgument( power->getID() );
				msg->appendIntegerArgument( button->getOptions() );
				msg->appendObjectIDArgument( source->getID() );
				log( "frame %d: ui: shortcut %s (no target) from %s ($%u, %d points)", (int)s_frame, f[0].c_str(),
					source->getTemplate()->getName().str(), (unsigned)me->getMoney()->countMoney(), (int)me->getSupportPoints() );
			}
		}
		else if ( step.action == "presspower" )
		{
			// Tiberium Project @feature (the test) a click on the Powers bar's power with that button (its targeting)
			const Bool took = TiberiumHUD::testPressPower( step.arg.c_str() );
			const CommandButton *pending = TheInGameUI != nullptr ? TheInGameUI->getGUICommand() : nullptr;
			log( "frame %d: ui: press power %s%s, aiming %s", (int)s_frame, step.arg.c_str(), took ? "" : " (not on the bar)",
				pending != nullptr ? pending->getName().str() : "nothing" );
		}
		else if ( step.action == "points" && ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
		{
			// Tiberium Project @feature (the test) support points for the local player (C&C4's meter, up to its top
			// once it fills again)
			Player *me = ThePlayerList->getLocalPlayer();
			me->spendSupportPoints( -atoi( step.arg.c_str() ) );
			log( "frame %d: ui: points %d of %d", (int)s_frame, (int)me->getSupportPoints(), (int)me->getSupportPointsMax() );
		}
		else if ( step.action == "shortcutat" && TheControlBar != nullptr && ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
		{
			// Tiberium Project @feature (the test) "shortcutat=Button,Template": a shortcut bar power aimed at the local
			// player's first object of that template (a power aimed at an object: the Sonic Repulsion Field)
			std::vector<std::string> f = split( step.arg.c_str(), ',' );
			const CommandButton *button = f.empty() ? nullptr : TheControlBar->findCommandButton( AsciiString( f[0].c_str() ) );
			Player *me = ThePlayerList->getLocalPlayer();
			const SpecialPowerTemplate *power = button != nullptr ? button->getSpecialPowerTemplate() : nullptr;
			Object *source = power != nullptr ? me->findMostReadyShortcutSpecialPowerOfType( power->getSpecialPowerType() ) : nullptr;
			const ThingTemplate *tmpl = f.size() > 1 && TheThingFactory != nullptr ? TheThingFactory->findTemplate( f[1].c_str() ) : nullptr;
			struct First { const ThingTemplate *tmpl; Object *obj; static void each( Object *obj, void *data )
			{
				First *me = (First *)data;
				if ( me->obj == nullptr && obj->getTemplate()->isEquivalentTo( me->tmpl ) )
					me->obj = obj;
			} };
			First first = { tmpl, nullptr };
			if ( tmpl != nullptr )
				me->iterateObjects( First::each, &first );
			if ( power == nullptr || source == nullptr || first.obj == nullptr )
				log( "frame %d: ui: shortcutat %s: no such button, power, source or target", (int)s_frame, step.arg.c_str() );
			else
			{
				GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT );
				msg->appendIntegerArgument( power->getID() );
				msg->appendObjectIDArgument( first.obj->getID() );
				msg->appendIntegerArgument( button->getOptions() );
				msg->appendObjectIDArgument( source->getID() );
				log( "frame %d: ui: shortcutat %s at %s #%u from %s ($%u)", (int)s_frame, f[0].c_str(), f[1].c_str(),
					(unsigned)first.obj->getID(), source->getTemplate()->getName().str(), (unsigned)me->getMoney()->countMoney() );
			}
		}
		else if ( step.action == "powers" && TheControlBar != nullptr && ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
		{
			// the local player's shortcut bar: each power's source and how ready it is; the money and the support meter
			Player *me = ThePlayerList->getLocalPlayer();
			const PlayerTemplate *pt = me->getPlayerTemplate();
			const CommandSet *set = pt != nullptr ? TheControlBar->findCommandSet( pt->getSpecialPowerShortcutCommandSet() ) : nullptr;
			std::string all;
			for ( Int i = 0; set != nullptr && i < MAX_COMMANDS_PER_SET; ++i )
			{
				const CommandButton *b = set->getCommandButton( i );
				const SpecialPowerTemplate *power = b != nullptr ? b->getSpecialPowerTemplate() : nullptr;
				if ( power == nullptr )
					continue;
				Object *source = me->findMostReadyShortcutSpecialPowerOfType( power->getSpecialPowerType() );
				SpecialPowerModuleInterface *mod = source != nullptr ? source->getSpecialPowerModule( power ) : nullptr;
				char one[160];
				snprintf( one, sizeof( one ), " %s:%s%s", power->getName().str(),
					mod == nullptr ? "-" : ( mod->isReady() ? "ready" : "charging" ), source != nullptr && source->isDisabled() ? "(disabled)" : "" );
				all += one;
			}
			log( "frame %d: ui: powers: $%u, %d of %d points, GP %d;%s", (int)s_frame, (unsigned)me->getMoney()->countMoney(),
				(int)me->getSupportPoints(), (int)me->getSupportPointsMax(), (int)me->getSciencePurchasePoints(), all.c_str() );
		}
#if RTS_ZEROHOUR
		else if ( step.action == "slots" && TheGameLogic != nullptr )
		{
			// Tiberium Project @feature (the test) "slots=Template": the first of that kind's weapon in each slot, how many
			// ride in it and its pips (the places its riders take: Kane's Wrath's MARV), and its object upgrades
			for ( Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject() )
			{
				if ( obj->getTemplate()->getName().compareNoCase( step.arg.c_str() ) != 0 )
					continue;
				AsciiString weapons;
				for ( Int k = 0; k < WEAPONSLOT_COUNT; ++k )
				{
					const Weapon *w = obj->getWeaponInWeaponSlot( (WeaponSlotType)k );
					char one[160];
					if ( w != nullptr && w->getLastShotFrame() != 0 )
						snprintf( one, sizeof( one ), " %d:%s@%u", (int)k, w->getName().str(), (unsigned)w->getLastShotFrame() );
					else
						snprintf( one, sizeof( one ), " %d:%s", (int)k, w != nullptr ? w->getName().str() : "-" );
					// (where its first barrel fires from, in the object: the bones of the weapon it holds)
					Matrix3D launch;
					if ( w != nullptr && obj->getDrawable() != nullptr
						&& obj->getDrawable()->getProjectileLaunchOffset( (WeaponSlotType)k, 0, &launch, TURRET_INVALID, nullptr ) )
						snprintf( one + strlen( one ), sizeof( one ) - strlen( one ), "(%.0f,%.0f,%.0f)",
							launch.Get_X_Translation(), launch.Get_Y_Translation(), launch.Get_Z_Translation() );
					weapons.concat( one );
				}
				// (and each turret's angle, in degrees from its object's front)
				const AIUpdateInterface *turretAI = obj->getAIUpdateInterface();
				for ( Int t = 0; turretAI != nullptr && t < MAX_TURRETS; ++t )
				{
					Real angle = 0.0f, pitch = 0.0f;
					if ( turretAI->getTurretRotAndPitch( (WhichTurretType)t, &angle, &pitch ) )
					{
						char one[32];
						snprintf( one, sizeof( one ), " t%d:%.0f", (int)t, angle * 180.0f / PI );
						weapons.concat( one );
					}
				}
				ContainModuleInterface *contain = obj->getContain();
				Int total = 0, full = 0;
				if ( contain != nullptr )
					contain->getContainerPipsToShow( total, full );
				AsciiString ups;
				if ( TheUpgradeCenter != nullptr )
				{
					for ( const UpgradeTemplate *up = TheUpgradeCenter->firstUpgradeTemplate(); up != nullptr; up = up->friend_getNext() )
					{
						if ( up->getUpgradeType() == UPGRADE_TYPE_OBJECT && obj->hasUpgrade( up ) )
						{
							ups.concat( ' ' );
							ups.concat( up->getUpgradeName() );
						}
					}
				}
				log( "frame %d: ui: slots %s #%u:%s; riders %d, pips %d of %d; upgrades%s", (int)s_frame, step.arg.c_str(),
					(unsigned)obj->getID(), weapons.str(), contain != nullptr ? (int)contain->getContainCount() : 0, (int)full,
					(int)total, ups.isEmpty() ? " none" : ups.str() );
				break;
			}
		}
#endif
		else if ( step.action == "state" && TheGameLogic != nullptr )
		{
			// Tiberium Project @feature (the test) every object of a template: its owner, health, stealth, EMP, sight,
			// dodge and the attribute modifiers on it (the area powers' effects)
			Int shown = 0;
			for ( Object *obj = TheGameLogic->getFirstObject(); obj != nullptr && shown < 12; obj = obj->getNextObject() )
			{
				if ( obj->getTemplate()->getName().compareNoCase( step.arg.c_str() ) != 0 )
					continue;
				++shown;
				const BodyModuleInterface *body = obj->getBodyModule();
				const Player *me = ThePlayerList != nullptr ? ThePlayerList->getLocalPlayer() : nullptr;
				const CellShroudStatus seen = me != nullptr ? ThePartitionManager->getShroudStatusForPlayer( me->getPlayerIndex(), obj->getPosition() ) : CELLSHROUD_SHROUDED;
				// (Kane's Wrath's building fields: the upgraded armour, the field's status and its look)
				BodyModuleInterface *armourBody = obj->getBodyModule();
				const Drawable *look = obj->getDrawable();
				char field[768];
				// (SAGE's general-purpose statuses, and the weapon set conditions following them: a crate's upgrade, a
				// cannon broken)
				char statuses[320] = "";
#if RTS_ZEROHOUR
				{
					static const ObjectStatusTypes s_shown[] = { OBJECT_STATUS_USER_1, OBJECT_STATUS_USER_2, OBJECT_STATUS_USER_3,
						OBJECT_STATUS_USER_4, OBJECT_STATUS_WEAPON_UPGRADED_01, OBJECT_STATUS_WEAPON_UPGRADED_02,
						OBJECT_STATUS_WEAPON_UPGRADED_03, OBJECT_STATUS_HAS_CRATE_UPGRADE,
						// (a claw's grip: the holder's and the held's, C&C4's Scorpion's)
						OBJECT_STATUS_NO_ATTACK, OBJECT_STATUS_IMMOBILE, OBJECT_STATUS_UNSELECTABLE, OBJECT_STATUS_NO_COLLISIONS,
						// (an aircraft in the air)
						OBJECT_STATUS_AIRBORNE_TARGET };
					AsciiString had, sets;
					for ( size_t k = 0; k < ARRAY_SIZE( s_shown ); ++k )
					{
						if ( obj->testStatus( s_shown[ k ] ) )
						{
							had.concat( ' ' );
							had.concat( ObjectStatusMaskType::getBitNames()[ s_shown[ k ] ] );
						}
					}
					for ( Int k = 0; k < WEAPONSET_COUNT; ++k )
					{
						if ( obj->testWeaponSetFlag( (WeaponSetType)k ) )
						{
							sets.concat( ' ' );
							sets.concat( WeaponSetFlags::getBitNames()[ k ] );
						}
					}
					// (and how fast it goes: a modifier's speed, the Mantis's as it loses its cannons; pinned down by fire)
					const Real speed = obj->getAttributeModifierScalar( MODIFIER_SPEED );
					static const NameKeyType suppressionKey = NAMEKEY( "SuppressionUpdate" );
					const SuppressionUpdate *sup = (const SuppressionUpdate *)obj->findUpdateModule( suppressionKey );
					const Bool pinned = sup != nullptr && sup->isSuppressed();
					// (a claw's: what it holds, and its command set meanwhile)
					static const NameKeyType grabKey = NAMEKEY( "GrabSpecialAbilityUpdate" );
					const GrabSpecialAbilityUpdate *grab = (const GrabSpecialAbilityUpdate *)obj->findUpdateModule( grabKey );
					char holding[96] = "";
					if ( grab != nullptr && grab->isHolding() )
						snprintf( holding, sizeof( holding ), ", holding #%u (set %s)", (unsigned)grab->getVictimID(), obj->getCommandSetString().str() );
					// (an aircraft's ammo, which it may go back to its pad for, and what its AI is doing: Kane's Wrath's VTOLs)
					const Weapon *primary = obj->getWeaponInWeaponSlot( PRIMARY_WEAPON );
					if ( obj->isKindOf( KINDOF_AIRCRAFT ) && obj->getAI() != nullptr )
					{
						if ( primary != nullptr && primary->getClipSize() > 0 )
							snprintf( holding + strlen( holding ), sizeof( holding ) - strlen( holding ), ", ammo %u of %d",
								primary->getRemainingAmmo(), primary->getClipSize() );
						snprintf( holding + strlen( holding ), sizeof( holding ) - strlen( holding ), ", ai state %d",
							(int)obj->getAI()->getCurrentStateID() );
					}
					if ( had.isNotEmpty() || sets.isNotEmpty() || speed != 1.0f || pinned || holding[0] != 0 )
						snprintf( statuses, sizeof( statuses ), ", status%s, weapon sets%s, speed x%.2f%s%s", had.str(), sets.str(), speed, pinned ? ", suppressed" : "", holding );
				}
#endif
				// (Tiberium Project: its false returns on the radar, a Signature Generator's)
				const Int falseReturns = look != nullptr ? look->getRadarFalseReturns( TheGameLogic->getFrame(), nullptr ) : 0;
				char returns[32] = "";
				if ( falseReturns > 0 )
					snprintf( returns, sizeof( returns ), ", %d false returns", (int)falseReturns );
				// (what its AI is after: a taunt turning it on the taunter)
				char aiming[32] = "";
				const Object *goal = obj->getAI() != nullptr ? obj->getAI()->getGoalObject() : nullptr;
				if ( goal != nullptr )
					snprintf( aiming, sizeof( aiming ), ", after #%u", (unsigned)goal->getID() );
				snprintf( field, sizeof( field ), "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
					statuses, aiming,
					look != nullptr && look->getModelConditionFlags().test( MODELCONDITION_USER_1 ) ? ", USER_1" : "",
					// (a burrowed unit, C&C4's: dug out by the Seismic Disturbance)
					look != nullptr && look->getModelConditionFlags().test( MODELCONDITION_BURROWING ) ? ", BURROWING" : "",
					armourBody != nullptr && armourBody->testArmorSetFlag( ARMORSET_PLAYER_UPGRADE ) ? ", armour upgraded" : "",
					obj->testStatus( OBJECT_STATUS_RIDER1 ) ? ", RIDER1" : "",
					look != nullptr && look->getModelConditionFlags().test( MODELCONDITION_USER_6 ) ? ", USER_6" : "",
					// (a superweapon: ready, launching)
					look != nullptr && look->getModelConditionFlags().test( MODELCONDITION_SPECIALPOWER1_READY ) ? ", SPECIALPOWER1_READY" : "",
					look != nullptr && look->getModelConditionFlags().test( MODELCONDITION_SPECIAL_WEAPON_ONE ) ? ", SPECIAL_WEAPON_ONE" : "",
					look != nullptr && look->getModelConditionFlags().test( MODELCONDITION_UNPACKING ) ? ", UNPACKING" : "",
					// (a teleport: its arrival, gone meanwhile)
					look != nullptr && look->getModelConditionFlags().test( MODELCONDITION_PACKING ) ? ", PACKING" : "",
					look != nullptr && look->isDrawableEffectivelyHidden() ? ", hidden" : "",
					obj->testStatus( OBJECT_STATUS_MASKED ) ? ", masked" : "",
					obj->isDisabledByType( DISABLED_HELD ) ? ", held" : "", returns );
				log( "frame %d: ui: state %s #%u player %d at %.0f %.0f height %.0f (%s to me): health %.0f of %.0f%s%s%s, sight %.0f (clears %.0f), dodge %.2f, reflect %.2f, %d modifiers%s, upgrades %s, veterancy %d%s",
					(int)s_frame, step.arg.c_str(), (unsigned)obj->getID(),
					obj->getControllingPlayer() != nullptr ? (int)obj->getControllingPlayer()->getPlayerIndex() : -1,
					obj->getPosition()->x, obj->getPosition()->y, obj->getHeightAboveTerrain(),
					seen == CELLSHROUD_CLEAR ? "clear" : ( seen == CELLSHROUD_FOGGED ? "fogged" : "shrouded" ),
					body != nullptr ? body->getHealth() : 0.0f, body != nullptr ? body->getMaxHealth() : 0.0f,
					obj->testStatus( OBJECT_STATUS_STEALTHED ) ? ", stealthed" : "",
					obj->isDisabledByType( DISABLED_EMP ) ? ", EMP" : "",
					obj->isResistingEMPByModifier() ? ", resists EMP" : "",
					obj->getVisionRange(), obj->getShroudClearingRange(), obj->getDodgeChance(), obj->getAttributeModifierChance( MODIFIER_CHANCE_REFLECT ),
					obj->hasAttributeModifiers() ? 1 : 0,
					obj->getAttributeModifierScalar( MODIFIER_DAMAGE ) != 1.0f ? " (damage scaled)" : "",
					obj->getObjectCompletedUpgradeMask().any() ? "some" : "none", (int)obj->getVeterancyLevel(), field );
			}
			if ( shown == 0 )
				log( "frame %d: ui: state %s: none", (int)s_frame, step.arg.c_str() );
		}
		else if ( step.action == "radar" && ThePlayerList != nullptr )
		{
			// Tiberium Project @feature (the test) each player's radar: had, jammed
			std::string all;
			for ( Int i = 0; i < ThePlayerList->getPlayerCount(); ++i )
			{
				Player *p = ThePlayerList->getNthPlayer( i );
				if ( p == nullptr || !p->isPlayerActive() )
					continue;
				char one[96];
				snprintf( one, sizeof( one ), " %d:%s%s", (int)i, p->hasRadar() ? "radar" : "none", p->isRadarJammed() ? "(jammed)" : "" );
				all += one;
			}
			log( "frame %d: ui: radar%s", (int)s_frame, all.c_str() );
		}
		else if ( step.action == "around" && TheGameLogic != nullptr && ThePartitionManager != nullptr && !s_spawned.empty()
			&& ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
		{
			// Tiberium Project @feature (the test) "around=dx,dy,r": what stands within r of the spot dx,dy from the first spawned
			// object (where power= and shortcut= aim): each kind there and whose (mine, ally, foe or no one's), and the enemies'
			// health, to see what a power made, hurt or changed there
			float dx = 0.0f, dy = 0.0f, r = 150.0f;
			sscanf( step.arg.c_str(), "%f,%f,%f", &dx, &dy, &r );
			const Object *first = TheGameLogic->findObjectByID( s_spawned[0] );
			if ( first != nullptr )
			{
				Coord3D at = *first->getPosition();
				at.x += dx;
				at.y += dy;
				Player *me = ThePlayerList->getLocalPlayer();
				std::vector< std::pair<std::string, int> > kinds;
				Int foes = 0, changed = 0;
				Real foeHealth = 0.0f, foeMost = 0.0f;
				ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( &at, r, FROM_CENTER_2D );
				MemoryPoolObjectHolder hold( iter );
				for ( Object *obj = iter->first(); obj != nullptr; obj = iter->next() )
				{
					if ( obj->isKindOf( KINDOF_PROJECTILE ) || obj->isEffectivelyDead() )
						continue;
					const char *whose = "none";
					if ( obj->getControllingPlayer() == me )
						whose = "mine";
					else if ( obj->getTeam() != nullptr && me->getRelationship( obj->getTeam() ) == ENEMIES )
						whose = "foe";
					else if ( obj->getTeam() != nullptr && me->getRelationship( obj->getTeam() ) == ALLIES )
						whose = "ally";
					std::string key = std::string( obj->getTemplate()->getName().str() ) + "(" + whose + ")";
					size_t k = 0;
					while ( k < kinds.size() && kinds[k].first != key )
						++k;
					if ( k == kinds.size() )
						kinds.push_back( std::make_pair( key, 0 ) );
					++kinds[k].second;
					BodyModuleInterface *body = obj->getBodyModule();
					if ( whose[0] == 'f' && body != nullptr )
					{
						++foes;
						foeHealth += body->getHealth();
						foeMost += body->getMaxHealth();
					}
#if RTS_ZEROHOUR
					changed += obj->hasAttributeModifiers() ? 1 : 0;
#endif
				}
				std::string list;
				for ( size_t k = 0; k < kinds.size(); ++k )
				{
					char one[160];
					snprintf( one, sizeof( one ), "%s%d %s", k ? ", " : "", kinds[k].second, kinds[k].first.c_str() );
					list += one;
				}
				log( "frame %d: ui: around %.0f %.0f within %.0f: %s; %d enemies, health %.0f of %.0f; %d with modifiers",
					(int)s_frame, at.x, at.y, r, list.empty() ? "nothing" : list.c_str(), (int)foes, foeHealth, foeMost, (int)changed );
			}
		}
		else if ( step.action == "census" && TheGameLogic != nullptr )
		{
			// every player's objects of a kind (a map's Tiberium crystals are nobody's), with the boxes
			// left in them when they are supply sources, and what the nearest to the view holds
			Int count = 0, boxes = 0, most = 0, empty = 0, riders = 0, waiting = 0, aiState = -1;
			Real nearest = 1.0e9f;
			Coord3D at;
			at.zero();
			const Coord3D view = TheTacticalView != nullptr ? TheTacticalView->getPosition() : Coord3D();
			static const NameKeyType key = NAMEKEY( "SupplyWarehouseDockUpdate" );
			for ( Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject() )
			{
				if ( obj->getTemplate()->getName().compareNoCase( step.arg.c_str() ) != 0 )
					continue;
				++count;
				SupplyWarehouseDockUpdate *dock = (SupplyWarehouseDockUpdate *)obj->findUpdateModule( key );
				if ( dock != nullptr )
				{
					boxes += dock->getBoxesStored();
					most += dock->getMaxBoxes();
					empty += dock->getBoxesStored() == 0 ? 1 : 0;
				}
				const Real dx = obj->getPosition()->x - view.x, dy = obj->getPosition()->y - view.y;
				if ( dx * dx + dy * dy < nearest )
				{
					nearest = dx * dx + dy * dy;
					at = *obj->getPosition();
				}
				// (and who is inside, for a transport, who wants in or out, and what its AI is doing)
				if ( obj->getContain() != nullptr )
				{
					riders += obj->getContain()->getContainCount();
					waiting += obj->getContain()->hasObjectsWantingToEnterOrExit() ? 1 : 0;
				}
				if ( obj->getAI() != nullptr )
					aiState = (Int)obj->getAI()->getCurrentStateID();
			}
			log( "frame %d: ui: census %d of %s, %d of %d boxes, %d empty, %d inside (%d with some on the way, ai state %d), the nearest at %.0f %.0f %.0f",
				(int)s_frame, (int)count, step.arg.c_str(), (int)boxes, (int)most, (int)empty, (int)riders, (int)waiting, (int)aiState,
				count > 0 ? at.x : 0.0f, count > 0 ? at.y : 0.0f, count > 0 ? at.z : 0.0f );
		}
#endif
#if RTS_ZEROHOUR
		else if ( step.action == "docks" && TheGameLogic != nullptr )
		{
			// every object of a kind with a dock (supply centers, crystals): who is docking there, and
			// every harvester with where it stands and what it is doing
			for ( Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject() )
			{
				const Bool mine = obj->getTemplate()->getName().compareNoCase( step.arg.c_str() ) == 0;
				if ( mine && obj->getDockUpdateInterface() != nullptr )
				{
					static const NameKeyType keys[] = { NAMEKEY( "SupplyCenterDockUpdate" ), NAMEKEY( "SupplyWarehouseDockUpdate" ) };
					for ( Int k = 0; k < 2; ++k )
					{
						const DockUpdate *dock = (const DockUpdate *)obj->findUpdateModule( keys[k] );
						if ( dock == nullptr )
							continue;
						char text[512];
						dock->describeForTest( text, sizeof( text ) );
						log( "frame %d: ui: dock %s #%u at %.0f %.0f: %s", (int)s_frame, step.arg.c_str(), (unsigned)obj->getID(),
							obj->getPosition()->x, obj->getPosition()->y, text );
					}
				}
				if ( obj->isKindOf( KINDOF_HARVESTER ) && obj->getDrawable() != nullptr )
				{
					std::string flags;
					const ModelConditionFlags f = obj->getDrawable()->getModelConditionFlags();
					for ( Int bit = 0; bit < f.size(); ++bit )
					{
						const char *name = f.getBitNameIfSet( bit );
						if ( name != nullptr ) { flags += " "; flags += name; }
					}
					AIUpdateInterface *ai = obj->getAIUpdateInterface();
					log( "frame %d: ui: harvester %s #%u at %.0f %.0f,%s, %s", (int)s_frame, obj->getTemplate()->getName().str(),
						(unsigned)obj->getID(), obj->getPosition()->x, obj->getPosition()->y, flags.c_str(),
						ai != nullptr && ai->isIdle() ? "idle" : "busy" );
				}
			}
		}
#endif
		else if ( step.action == "prod" && ThePlayerList != nullptr && ThePlayerList->getLocalPlayer() != nullptr )
		{
			// what the local player's factories are making: each queue's length and its first entry's progress
			struct Gather { static void each( Object *obj, void *data )
			{
				ProductionUpdateInterface *pu = obj->getProductionUpdateInterface();
				const ProductionEntry *first = pu != nullptr ? pu->firstProduction() : nullptr;
				if ( first == nullptr )
					return;
				char one[200];
				snprintf( one, sizeof( one ), " %s: %d queued, first %.0f%%%s", obj->getTemplate()->getName().str(), (int)pu->getProductionCount(),
					first->getPercentComplete(), obj->isDisabled() ? " (disabled)" : "" );
				*(std::string *)data += one;
			} };
			std::string all;
			ThePlayerList->getLocalPlayer()->iterateObjects( Gather::each, &all );
			log( "frame %d: ui: production%s", (int)s_frame, all.empty() ? " (none)" : all.c_str() );
		}
		else if ( step.action == "bar" && TheControlBar != nullptr )
		{
			std::string shown;
			for ( Int i = 0; i < 14; ++i )
			{
				GameWindow *win = TheControlBar->getCommandWindow( i );
				const CommandButton *button = win != nullptr ? (const CommandButton *)GadgetButtonGetData( win ) : nullptr;
				if ( win == nullptr || win->winIsHidden() || button == nullptr )
					continue;
				char one[160];
				snprintf( one, sizeof( one ), " %d=%s%s%s", (int)( i + 1 ), button->getName().str(), win->winGetEnabled() ? "" : "(off)",
					button->getButtonImage() != nullptr ? "" : "(no image)" );
				shown += one;
			}
			log( "frame %d: ui: bar%s", (int)s_frame, shown.empty() ? " (empty)" : shown.c_str() );
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
		s_spanStartDrawn = s_drawnFrame;
		s_spanStart = now;
	}
	// (none from the quit frame on: on a fast machine more would keep the quit waiting forever)
	const Bool quitReached = s_quitFrame > 0 && s_frame > s_quitFrame;
	if ( s_shotInterval > 0 && TheDisplay != nullptr && s_frame % s_shotInterval == 0 && s_frame >= s_shotStart && !quitReached )
	{
		double msPerFrame = 0.0;
		if ( s_spanStartFrame > 0 && s_drawnFrame > s_spanStartDrawn )
			msPerFrame = std::chrono::duration<double, std::milli>( now - s_spanStart ).count() / ( s_drawnFrame - s_spanStartDrawn );
		TheDisplay->takeScreenShot( SCREENSHOT_PNG );
		s_lastShotFrame = s_frame;
		s_lastShotTime = now;
		++s_shotCount;
		log( "frame %d: screenshot %d (logic frame %u, in game %d), %.2f ms per frame since frame %d", (int)s_frame, (int)s_shotCount,
			TheGameLogic != nullptr ? (unsigned)TheGameLogic->getFrame() : 0u, inGame ? 1 : 0, msPerFrame, (int)s_spanStartFrame );
		s_spanStartFrame = s_frame + 1;
		s_spanStartDrawn = s_drawnFrame + 1;
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
