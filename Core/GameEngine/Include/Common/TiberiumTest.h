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

#pragma once

// Tiberium Project @feature Renderer test options, given on the command line.
//
// They let a test run load a map straight away, move the camera to set views at set frames, take
// screenshots, add scorch marks, log the game logic's CRC and quit, with nobody at the keyboard:
//
//   -testMap "Maps\Alpine Assault\Alpine Assault.map"   start that map as a single-player game
//   -testCamera "60,0.45,-,1500,950;200,0.3,40,-,-,90"  at frame 60: zoom 0.45, default pitch, look at
//                                                        (1500, 950); at frame 200: zoom 0.3, pitch 40,
//                                                        turned to 90 degrees (fields: frame, zoom, pitch,
//                                                        x, y, angle; "-" keeps a value)
//   -testControlBar low                                  the control bar lowered, as its green arrow does:
//                                                        the world drawn full height (hidden only hides it)
//   -testTour 3                                          a tour of the map: a 3 x 3 grid of stops, each seen
//                                                        close up, at the usual height, from afar and from low
//                                                        down, turned a quarter each time; a screenshot of each
//                                                        view, then quit
//   -testShots 100                                       a PNG screenshot every 100 frames
//   -testScorch 50                                       a field of scorch marks around the view at frame 50
//   -testCRC 300                                         log the logic CRC at logic frame 300
//   -testMenu Menus/OptionsMenu.wnd                      open that shell screen at frame 60 (before a game:
//                                                        with -noshellmap and no -testMap), for screenshots
//   -testQuit 400                                        quit at frame 400 (no sooner than 5 seconds after
//                                                        the last screenshot, which is still being written)
//
// Frames are the engine's update frames (one per drawn frame). Everything is written to
// TiberiumTest.log in the user data folder. Nothing here runs unless an option is given, and none
// of it touches the game logic: the camera, screenshots and scorch marks are client-side, and the
// CRC is only read. So a CRC logged with the C&C4 renderer on and off must be the same.

#include "Lib/BaseType.h"

class TiberiumTest
{
public:
	static void setMap( const char *path );
	static void setCameras( const char *spec );
	static void setControlBar( const char *stage );
	static void setTour( Int size );
	static void setShotInterval( Int frames );
	static void setShotStart( Int frame );	///< no screenshot before that frame
	static void setScorchFrame( Int frame );
	static void setLightsFrame( Int frame );	///< nine lasting light pulses around the view at that frame (client side)
	static void setFireFrame( Int frame );	///< five lasting fires (particle systems) around the view at that frame (client side)
	static void setFXLists( const char *lists );	///< up to six FX lists, separated by ';', for -testFX to play in place of its own
	static void setFXFrame( Int frame, Int every );	///< six of Zero Hour's explosion and hit effects (FX lists) around the view at that frame and every so many frames after (client side)
	static void setUnitsFrame( Int frame );	///< a few of the local player's vehicles and buildings around the view at that frame (changes the game: not with -testCRC)
	static void setBaseFrame( Int frame );	///< America's core base around the view at that frame, whole, damaged, being built and destroyed (changes the game: not with -testCRC)
	static void setReveal( Bool reveal );	///< reveal the map to the local player at the start (changes the logic's state: not with -testCRC)
	static void setCRCFrame( Int frame );
	static void setQuitFrame( Int frame );
	static void setMenu( const char *layout );	///< a shell screen (a .wnd) pushed at frame 60, before a game

	/// True when any test option was given
	static Bool isActive();

	/// Once per engine update, after the client and the logic have run
	static void update();

	/// A line in TiberiumTest.log (and on stderr)
	static void log( const char *format, ... );

	/// A step of the game's shutdown, in TiberiumCrash.log (test runs only). Written straight to the
	/// file, so it still works after the C runtime has started closing down.
	static void exitStage( const char *stage );
};

// Tiberium Project @feature Crash watch for test runs (Windows): from the first test frame on, any
// error exception (an access violation and the like, not C++ exceptions) is written to
// TiberiumCrash.log in the user data folder, with the module and offset it happened at and, on
// x64, the call stack as module+offset pairs (GeneralsXZH.pdb turns them into functions). With
// exitStage() marking the shutdown steps, a crash after the game's own crash handler is gone (it
// stops at the end of WinMain) still shows where it was.
