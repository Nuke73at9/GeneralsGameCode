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

// Abram's Personal Test @feature Switch between Generals and Zero Hour, and restart with a mod.
//
// Generals and Zero Hour are separate executables with separate rules and data, and both use the
// same single-instance mutex. So a switch works like this:
//   1. The main menu button calls requestSwitch(), which finds the other game's executable.
//   2. The current game quits normally.
//   3. After the game loop has ended, WinMain releases the instance mutex and calls
//      launchRequested(), which starts the other game.
//
// This class uses only plain Win32 calls and fixed buffers, so it is safe to call after the
// engine and its memory manager have shut down.

class GameSwitcher
{
public:
	// Returns true if the other game's executable can be found on this computer.
	static bool isOtherGameAvailable();

	// Remembers the other game's executable so it is launched after this game exits.
	// Returns false if the other game cannot be found.
	static bool requestSwitch();

	// Remembers this game's own executable, so it restarts after exiting with the given mod folder
	// loaded through the -mod option. An empty or null folder restarts without a mod.
	static bool requestRestartWithMod(const char* modFolder);

	static bool isSwitchRequested();

	// Starts the other game. Call only after the instance mutex has been released.
	static bool launchRequested();

	// Text for the main menu button, e.g. "Switch to Generals".
	static const wchar_t* getButtonLabel();

private:
	static bool findOtherGame(char* exePath, unsigned int exePathSize, char* workingDir, unsigned int workingDirSize);

	static bool s_switchRequested;
	static char s_exePath[];
	static char s_workingDir[];
	static char s_arguments[];
};
