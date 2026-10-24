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
	static void setScorchFrame( Int frame );
	static void setCRCFrame( Int frame );
	static void setQuitFrame( Int frame );

	/// True when any test option was given
	static Bool isActive();

	/// Once per engine update, after the client and the logic have run
	static void update();

	/// A line in TiberiumTest.log (and on stderr)
	static void log( const char *format, ... );
};
