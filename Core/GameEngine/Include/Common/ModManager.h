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

// Abram's Personal Test @feature Finds mods installed in the game's "Mods" folder.
//
// Each mod lives in its own folder next to the game executable:
//   <game folder>\Mods\<Mod Name>\*.big
// A folder counts as a mod when it contains at least one .big file. The engine loads a mod
// through the existing -mod option, which reads every .big file in the folder on top of the
// game's own data.

class ModManager
{
public:
	enum
	{
		MAX_MODS = 64,
		NAME_SIZE = 64
	};

	// Looks for mods. Creates the Mods folder if it does not exist yet, so it is easy to find.
	static void scan();

	static int getModCount();

	// Folder name of the mod, used as its display name.
	static const char* getModName(int index);

	// Full path of the mod folder, ending with a backslash.
	static const char* getModFolder(int index);

	// Returns the index of the mod whose folder matches, or -1 for none (vanilla).
	static int findModByFolder(const char* folder);

	// Full path of the Mods folder, ending with a backslash.
	static const char* getModsRoot();

	// Number of .big files in the mod folder.
	static int countBigFiles(int index);

	// Finds the mod's picture: preview.tga/png/jpg/jpeg/bmp in the mod folder, or else the first
	// .png or .jpg file. Returns false when the mod has no picture.
	static bool findPreviewImage(int index, char* path, unsigned int pathSize);

	// Reads the start of description.txt (or readme.txt) from the mod folder.
	// Returns false when there is no such file.
	static bool readDescription(int index, char* text, unsigned int textSize);

private:
	static int s_modCount;
	static char s_modNames[MAX_MODS][NAME_SIZE];
	static char s_modFolders[MAX_MODS][260];
	static char s_modsRoot[260];
};
