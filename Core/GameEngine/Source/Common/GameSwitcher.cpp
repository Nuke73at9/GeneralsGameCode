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

#include "PreRTS.h"
#include "Common/GameSwitcher.h"

#include <string.h>

// Older Windows SDKs (Visual C++ 6) do not define these.
#ifndef KEY_WOW64_64KEY
#define KEY_WOW64_64KEY 0x0100
#endif
#ifndef KEY_WOW64_32KEY
#define KEY_WOW64_32KEY 0x0200
#endif
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

namespace
{

const unsigned int SWITCH_PATH_SIZE = MAX_PATH;

struct RegistryLocation
{
	const char* subKey;
	const char* valueName;
};

// Registry locations of the OTHER game's install folder. These are the same locations the CMake
// install step uses (EA App/CD, The First Decade, Steam). Value names are not case sensitive.
#if RTS_ZEROHOUR
const RegistryLocation OtherGameLocations[] =
{
	{ "SOFTWARE\\Electronic Arts\\EA Games\\Generals", "InstallPath" },
	{ "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer The First Decade", "gr_folder" },
};
// Our own build first, then the retail executable as a fallback.
const char* const OtherGameExeNames[] = { "generalsv.exe", "generals.exe" };
const wchar_t* const SwitchButtonLabel = L"Switch to Generals";
#else
const RegistryLocation OtherGameLocations[] =
{
	{ "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer Generals Zero Hour", "InstallPath" },
	{ "SOFTWARE\\Electronic Arts\\EA Games\\ZeroHour", "InstallPath" },
	{ "SOFTWARE\\Electronic Arts\\EA Games\\Command and Conquer The First Decade", "zh_folder" },
};
const char* const OtherGameExeNames[] = { "generalszh.exe", "generals.exe" };
const wchar_t* const SwitchButtonLabel = L"Switch to Zero Hour";
#endif

// Reads a string value from HKEY_LOCAL_MACHINE, trying both the 32-bit and 64-bit registry views.
bool readRegistryString(const char* subKey, const char* valueName, char* out, unsigned int outSize)
{
	const REGSAM views[] = { KEY_WOW64_32KEY, KEY_WOW64_64KEY };

	for (unsigned int i = 0; i < sizeof(views) / sizeof(views[0]); ++i)
	{
		HKEY key;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ | views[i], &key) != ERROR_SUCCESS)
			continue;

		DWORD type = 0;
		DWORD size = outSize - 1;
		LONG result = RegQueryValueExA(key, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(out), &size);
		RegCloseKey(key);

		if (result == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && size > 1)
		{
			out[size < outSize ? size : outSize - 1] = '\0';
			return true;
		}
	}

	out[0] = '\0';
	return false;
}

// Turns a registry value into a folder path that ends with a backslash.
// Removes surrounding quotes, and removes a file name if the value points at an executable.
void normalizeFolder(char* path, unsigned int pathSize)
{
	// Remove surrounding quotes.
	size_t len = strlen(path);
	if (len >= 2 && path[0] == '"' && path[len - 1] == '"')
	{
		memmove(path, path + 1, len - 2);
		path[len - 2] = '\0';
		len -= 2;
	}

	// If the value is an executable path, keep only its folder.
	if (len > 4 && _stricmp(path + len - 4, ".exe") == 0)
	{
		char* lastSlash = strrchr(path, '\\');
		if (lastSlash)
		{
			lastSlash[1] = '\0';
			len = strlen(path);
		}
	}

	// Make sure the folder ends with a backslash.
	if (len > 0 && path[len - 1] != '\\' && path[len - 1] != '/' && len + 1 < pathSize)
	{
		path[len] = '\\';
		path[len + 1] = '\0';
	}
}

bool fileExists(const char* path)
{
	DWORD attributes = GetFileAttributesA(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// Returns the command line arguments of this process, without the program name,
// so options such as -win carry over to the other game.
const char* getCommandLineArguments()
{
	const char* cmd = GetCommandLineA();
	if (cmd == nullptr)
		return "";

	// Skip the program name, which may be quoted.
	if (*cmd == '"')
	{
		++cmd;
		while (*cmd != '\0' && *cmd != '"')
			++cmd;
		if (*cmd == '"')
			++cmd;
	}
	else
	{
		while (*cmd != '\0' && *cmd != ' ' && *cmd != '\t')
			++cmd;
	}

	while (*cmd == ' ' || *cmd == '\t')
		++cmd;

	return cmd;
}

} // namespace

bool GameSwitcher::s_switchRequested = false;
char GameSwitcher::s_exePath[SWITCH_PATH_SIZE] = { 0 };
char GameSwitcher::s_workingDir[SWITCH_PATH_SIZE] = { 0 };

bool GameSwitcher::findOtherGame(char* exePath, unsigned int exePathSize, char* workingDir, unsigned int workingDirSize)
{
	char folder[SWITCH_PATH_SIZE];

	for (unsigned int i = 0; i < sizeof(OtherGameLocations) / sizeof(OtherGameLocations[0]); ++i)
	{
		if (!readRegistryString(OtherGameLocations[i].subKey, OtherGameLocations[i].valueName, folder, sizeof(folder)))
			continue;

		normalizeFolder(folder, sizeof(folder));

		for (unsigned int j = 0; j < sizeof(OtherGameExeNames) / sizeof(OtherGameExeNames[0]); ++j)
		{
			char candidate[SWITCH_PATH_SIZE];
			_snprintf(candidate, sizeof(candidate), "%s%s", folder, OtherGameExeNames[j]);
			candidate[sizeof(candidate) - 1] = '\0';

			if (fileExists(candidate))
			{
				strncpy(exePath, candidate, exePathSize);
				exePath[exePathSize - 1] = '\0';
				strncpy(workingDir, folder, workingDirSize);
				workingDir[workingDirSize - 1] = '\0';
				return true;
			}
		}
	}

	return false;
}

bool GameSwitcher::isOtherGameAvailable()
{
	char exePath[SWITCH_PATH_SIZE];
	char workingDir[SWITCH_PATH_SIZE];
	return findOtherGame(exePath, sizeof(exePath), workingDir, sizeof(workingDir));
}

bool GameSwitcher::requestSwitch()
{
	s_switchRequested = findOtherGame(s_exePath, SWITCH_PATH_SIZE, s_workingDir, SWITCH_PATH_SIZE);
	DEBUG_LOG(("GameSwitcher: switch requested, target '%s' (%s)", s_exePath, s_switchRequested ? "found" : "not found"));
	return s_switchRequested;
}

bool GameSwitcher::isSwitchRequested()
{
	return s_switchRequested;
}

bool GameSwitcher::launchRequested()
{
	if (!s_switchRequested)
		return false;

	s_switchRequested = false;

	// CreateProcess may modify the command line buffer, so build it in a writable array.
	char commandLine[SWITCH_PATH_SIZE * 2];
	_snprintf(commandLine, sizeof(commandLine), "\"%s\" %s", s_exePath, getCommandLineArguments());
	commandLine[sizeof(commandLine) - 1] = '\0';

	STARTUPINFOA startupInfo;
	memset(&startupInfo, 0, sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);

	PROCESS_INFORMATION processInfo;
	memset(&processInfo, 0, sizeof(processInfo));

	if (!CreateProcessA(s_exePath, commandLine, nullptr, nullptr, FALSE, 0, nullptr, s_workingDir, &startupInfo, &processInfo))
		return false;

	CloseHandle(processInfo.hThread);
	CloseHandle(processInfo.hProcess);
	return true;
}

const wchar_t* GameSwitcher::getButtonLabel()
{
	return SwitchButtonLabel;
}
