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
#include "Common/ModManager.h"

#include <string.h>

int ModManager::s_modCount = 0;
char ModManager::s_modNames[MAX_MODS][NAME_SIZE] = { { 0 } };
char ModManager::s_modFolders[MAX_MODS][260] = { { 0 } };
char ModManager::s_modsRoot[260] = { 0 };

namespace
{

bool folderContainsBigFiles(const char* folder)
{
	char pattern[260];
	_snprintf(pattern, sizeof(pattern), "%s*.big", folder);
	pattern[sizeof(pattern) - 1] = '\0';

	WIN32_FIND_DATAA findData;
	HANDLE find = FindFirstFileA(pattern, &findData);
	if (find == INVALID_HANDLE_VALUE)
		return false;

	FindClose(find);
	return true;
}

} // namespace

void ModManager::scan()
{
	s_modCount = 0;
	s_modsRoot[0] = '\0';

	// The Mods folder sits next to the game executable.
	char exePath[260];
	DWORD len = GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
	if (len == 0 || len >= sizeof(exePath))
		return;

	char* lastSlash = strrchr(exePath, '\\');
	if (lastSlash == nullptr)
		return;
	lastSlash[1] = '\0';

	_snprintf(s_modsRoot, sizeof(s_modsRoot), "%sMods\\", exePath);
	s_modsRoot[sizeof(s_modsRoot) - 1] = '\0';

	CreateDirectoryA(s_modsRoot, nullptr);

	char pattern[260];
	_snprintf(pattern, sizeof(pattern), "%s*", s_modsRoot);
	pattern[sizeof(pattern) - 1] = '\0';

	WIN32_FIND_DATAA findData;
	HANDLE find = FindFirstFileA(pattern, &findData);
	if (find == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;
		if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
			continue;
		if (strlen(findData.cFileName) >= NAME_SIZE)
			continue;

		char folder[260];
		_snprintf(folder, sizeof(folder), "%s%s\\", s_modsRoot, findData.cFileName);
		folder[sizeof(folder) - 1] = '\0';

		if (!folderContainsBigFiles(folder))
			continue;

		// Insert sorted by name, so the list order is stable.
		int insertAt = s_modCount;
		while (insertAt > 0 && _stricmp(s_modNames[insertAt - 1], findData.cFileName) > 0)
		{
			strcpy(s_modNames[insertAt], s_modNames[insertAt - 1]);
			strcpy(s_modFolders[insertAt], s_modFolders[insertAt - 1]);
			--insertAt;
		}
		strcpy(s_modNames[insertAt], findData.cFileName);
		strcpy(s_modFolders[insertAt], folder);
		++s_modCount;
	}
	while (s_modCount < MAX_MODS && FindNextFileA(find, &findData));

	FindClose(find);
}

int ModManager::getModCount()
{
	return s_modCount;
}

const char* ModManager::getModName(int index)
{
	return (index >= 0 && index < s_modCount) ? s_modNames[index] : "";
}

const char* ModManager::getModFolder(int index)
{
	return (index >= 0 && index < s_modCount) ? s_modFolders[index] : "";
}

int ModManager::findModByFolder(const char* folder)
{
	if (folder == nullptr || folder[0] == '\0')
		return -1;

	// Compare without a trailing backslash, ignoring case.
	size_t len = strlen(folder);
	if (folder[len - 1] == '\\' || folder[len - 1] == '/')
		--len;

	for (int i = 0; i < s_modCount; ++i)
	{
		size_t modLen = strlen(s_modFolders[i]) - 1; // stored folders always end with a backslash
		if (modLen == len && _strnicmp(s_modFolders[i], folder, len) == 0)
			return i;
	}

	return -1;
}

const char* ModManager::getModsRoot()
{
	return s_modsRoot;
}
