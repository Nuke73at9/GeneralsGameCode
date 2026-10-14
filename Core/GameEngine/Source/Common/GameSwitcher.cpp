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

// Copies this process's command line arguments, without the program name and without any
// "-mod <path>" option, so options such as -win carry over when another game is launched.
void copyArgumentsWithoutMod(char* out, unsigned int outSize)
{
	out[0] = '\0';
	unsigned int outLen = 0;

	const char* cmd = GetCommandLineA();
	if (cmd == nullptr)
		return;

	bool isProgramName = true;
	bool skipNext = false;

	while (*cmd != '\0')
	{
		while (*cmd == ' ' || *cmd == '\t')
			++cmd;
		if (*cmd == '\0')
			break;

		// Find the end of this token. Quoted parts may contain spaces.
		const char* tokenStart = cmd;
		bool inQuotes = false;
		while (*cmd != '\0' && (inQuotes || (*cmd != ' ' && *cmd != '\t')))
		{
			if (*cmd == '"')
				inQuotes = !inQuotes;
			++cmd;
		}
		const unsigned int tokenLen = (unsigned int)(cmd - tokenStart);

		if (isProgramName)
		{
			isProgramName = false;
			continue;
		}
		if (skipNext)
		{
			skipNext = false;
			continue;
		}
		if (tokenLen == 4 && _strnicmp(tokenStart, "-mod", 4) == 0)
		{
			skipNext = true;
			continue;
		}

		if (outLen + tokenLen + 2 >= outSize)
			break;
		if (outLen > 0)
			out[outLen++] = ' ';
		memcpy(out + outLen, tokenStart, tokenLen);
		outLen += tokenLen;
		out[outLen] = '\0';
	}
}

} // namespace

bool GameSwitcher::s_switchRequested = false;
char GameSwitcher::s_exePath[SWITCH_PATH_SIZE] = { 0 };
char GameSwitcher::s_workingDir[SWITCH_PATH_SIZE] = { 0 };
char GameSwitcher::s_arguments[SWITCH_PATH_SIZE * 2] = { 0 };

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
	// Mods belong to one game, so the -mod option is not passed to the other game.
	copyArgumentsWithoutMod(s_arguments, sizeof(s_arguments));
	DEBUG_LOG(("GameSwitcher: switch requested, target '%s' (%s)", s_exePath, s_switchRequested ? "found" : "not found"));
	return s_switchRequested;
}

bool GameSwitcher::requestRestartWithMod(const char* modFolder)
{
	s_switchRequested = false;

	DWORD len = GetModuleFileNameA(nullptr, s_exePath, SWITCH_PATH_SIZE);
	if (len == 0 || len >= SWITCH_PATH_SIZE)
		return false;

	strncpy(s_workingDir, s_exePath, SWITCH_PATH_SIZE);
	s_workingDir[SWITCH_PATH_SIZE - 1] = '\0';
	char* lastSlash = strrchr(s_workingDir, '\\');
	if (lastSlash == nullptr)
		return false;
	lastSlash[1] = '\0';

	copyArgumentsWithoutMod(s_arguments, sizeof(s_arguments));

	if (modFolder != nullptr && modFolder[0] != '\0')
	{
		// The -mod option needs a full path. Drop a trailing backslash so the quotes stay intact.
		char folder[SWITCH_PATH_SIZE];
		strncpy(folder, modFolder, sizeof(folder));
		folder[sizeof(folder) - 1] = '\0';
		size_t folderLen = strlen(folder);
		if (folderLen > 0 && (folder[folderLen - 1] == '\\' || folder[folderLen - 1] == '/'))
			folder[folderLen - 1] = '\0';

		const size_t argLen = strlen(s_arguments);
		_snprintf(s_arguments + argLen, sizeof(s_arguments) - argLen, "%s-mod \"%s\"", argLen > 0 ? " " : "", folder);
		s_arguments[sizeof(s_arguments) - 1] = '\0';
	}

	s_switchRequested = true;
	DEBUG_LOG(("GameSwitcher: restart requested with arguments '%s'", s_arguments));
	return true;
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
	char commandLine[SWITCH_PATH_SIZE * 3];
	_snprintf(commandLine, sizeof(commandLine), "\"%s\" %s", s_exePath, s_arguments);
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
