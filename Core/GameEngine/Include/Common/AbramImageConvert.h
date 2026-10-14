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

// Abram's Personal Test @feature Converts a picture (PNG, JPG, BMP or TGA) into an uncompressed
// 32-bit TGA of the given size, which the game's texture loader can read. The picture is scaled
// to fill the whole target size. Returns false if the picture cannot be read or written.
bool AbramConvertImageToTga(const char* sourcePath, const char* targetPath, int targetWidth, int targetHeight);
