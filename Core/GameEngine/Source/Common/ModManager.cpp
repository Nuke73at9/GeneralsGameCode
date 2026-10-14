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

int countFiles(const char* folder, const char* wildcard)
{
	char pattern[260];
	_snprintf(pattern, sizeof(pattern), "%s%s", folder, wildcard);
	pattern[sizeof(pattern) - 1] = '\0';

	WIN32_FIND_DATAA findData;
	HANDLE find = FindFirstFileA(pattern, &findData);
	if (find == INVALID_HANDLE_VALUE)
		return 0;

	int count = 0;
	do
	{
		if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			++count;
	}
	while (FindNextFileA(find, &findData));

	FindClose(find);
	return count;
}

bool folderContainsBigFiles(const char* folder)
{
	return countFiles(folder, "*.big") > 0;
}

// Finds the first file matching the wildcard in the folder. Writes its full path.
bool findFirstFile(const char* folder, const char* wildcard, char* path, unsigned int pathSize)
{
	char pattern[260];
	_snprintf(pattern, sizeof(pattern), "%s%s", folder, wildcard);
	pattern[sizeof(pattern) - 1] = '\0';

	WIN32_FIND_DATAA findData;
	HANDLE find = FindFirstFileA(pattern, &findData);
	if (find == INVALID_HANDLE_VALUE)
		return false;

	bool found = false;
	do
	{
		if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			_snprintf(path, pathSize, "%s%s", folder, findData.cFileName);
			path[pathSize - 1] = '\0';
			found = true;
			break;
		}
	}
	while (FindNextFileA(find, &findData));

	FindClose(find);
	return found;
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

int ModManager::countBigFiles(int index)
{
	if (index < 0 || index >= s_modCount)
		return 0;
	return countFiles(s_modFolders[index], "*.big");
}

bool ModManager::findPreviewImage(int index, char* path, unsigned int pathSize)
{
	if (index < 0 || index >= s_modCount || path == nullptr || pathSize == 0)
		return false;

	const char* const preferred[] = { "preview.tga", "preview.png", "preview.jpg", "preview.jpeg", "preview.bmp" };
	for (unsigned int i = 0; i < sizeof(preferred) / sizeof(preferred[0]); ++i)
	{
		if (findFirstFile(s_modFolders[index], preferred[i], path, pathSize))
			return true;
	}

	const char* const fallback[] = { "*.png", "*.jpg", "*.jpeg" };
	for (unsigned int i = 0; i < sizeof(fallback) / sizeof(fallback[0]); ++i)
	{
		if (findFirstFile(s_modFolders[index], fallback[i], path, pathSize))
			return true;
	}

	path[0] = '\0';
	return false;
}

bool ModManager::readDescription(int index, char* text, unsigned int textSize)
{
	if (text == nullptr || textSize == 0)
		return false;
	text[0] = '\0';

	if (index < 0 || index >= s_modCount)
		return false;

	char path[260];
	if (!findFirstFile(s_modFolders[index], "description.txt", path, sizeof(path))
		&& !findFirstFile(s_modFolders[index], "readme.txt", path, sizeof(path)))
		return false;

	HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return false;

	DWORD bytesRead = 0;
	BOOL ok = ReadFile(file, text, textSize - 1, &bytesRead, nullptr);
	CloseHandle(file);
	if (!ok)
		return false;

	text[bytesRead] = '\0';

	// Skip a UTF-8 byte order mark, and drop carriage returns so line breaks show correctly.
	char* src = text;
	if ((unsigned char)src[0] == 0xEF && (unsigned char)src[1] == 0xBB && (unsigned char)src[2] == 0xBF)
		src += 3;
	char* dst = text;
	while (*src != '\0')
	{
		if (*src != '\r')
			*dst++ = *src;
		++src;
	}
	*dst = '\0';

	return text[0] != '\0';
}
