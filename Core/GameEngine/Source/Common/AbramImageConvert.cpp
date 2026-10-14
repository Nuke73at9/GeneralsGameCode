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
#include "Common/AbramImageConvert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// stb_image decodes the picture. STB_IMAGE_STATIC keeps its functions private to this file.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <stb_image.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace
{

// Bilinear sample of an RGBA picture at (x, y) in source pixel coordinates.
void sampleBilinear(const unsigned char* src, int width, int height, float x, float y, unsigned char* out)
{
	if (x < 0.0f) x = 0.0f;
	if (y < 0.0f) y = 0.0f;
	if (x > (float)(width - 1)) x = (float)(width - 1);
	if (y > (float)(height - 1)) y = (float)(height - 1);

	const int x0 = (int)x;
	const int y0 = (int)y;
	const int x1 = (x0 + 1 < width) ? x0 + 1 : x0;
	const int y1 = (y0 + 1 < height) ? y0 + 1 : y0;
	const float fx = x - (float)x0;
	const float fy = y - (float)y0;

	const unsigned char* p00 = src + (y0 * width + x0) * 4;
	const unsigned char* p10 = src + (y0 * width + x1) * 4;
	const unsigned char* p01 = src + (y1 * width + x0) * 4;
	const unsigned char* p11 = src + (y1 * width + x1) * 4;

	for (int c = 0; c < 4; ++c)
	{
		const float top = p00[c] + (p10[c] - p00[c]) * fx;
		const float bottom = p01[c] + (p11[c] - p01[c]) * fx;
		const float value = top + (bottom - top) * fy;
		out[c] = (unsigned char)(value + 0.5f);
	}
}

} // namespace

bool AbramConvertImageToTga(const char* sourcePath, const char* targetPath, int targetWidth, int targetHeight)
{
	if (sourcePath == nullptr || targetPath == nullptr || targetWidth <= 0 || targetHeight <= 0)
		return false;

	int width = 0;
	int height = 0;
	int channels = 0;
	unsigned char* pixels = stbi_load(sourcePath, &width, &height, &channels, 4);
	if (pixels == nullptr || width <= 0 || height <= 0)
	{
		if (pixels)
			stbi_image_free(pixels);
		return false;
	}

	const size_t outSize = (size_t)targetWidth * (size_t)targetHeight * 4;
	unsigned char* out = (unsigned char*)malloc(outSize);
	if (out == nullptr)
	{
		stbi_image_free(pixels);
		return false;
	}

	// Scale to the target size. TGA rows are stored bottom to top, and pixels as BGRA.
	const float scaleX = (float)width / (float)targetWidth;
	const float scaleY = (float)height / (float)targetHeight;
	for (int ty = 0; ty < targetHeight; ++ty)
	{
		const int outRow = targetHeight - 1 - ty;
		for (int tx = 0; tx < targetWidth; ++tx)
		{
			unsigned char rgba[4];
			sampleBilinear(pixels, width, height, (tx + 0.5f) * scaleX - 0.5f, (ty + 0.5f) * scaleY - 0.5f, rgba);

			unsigned char* dst = out + ((size_t)outRow * targetWidth + tx) * 4;
			dst[0] = rgba[2];
			dst[1] = rgba[1];
			dst[2] = rgba[0];
			dst[3] = rgba[3];
		}
	}

	stbi_image_free(pixels);

	unsigned char header[18];
	memset(header, 0, sizeof(header));
	header[2] = 2;                                   // uncompressed true color
	header[12] = (unsigned char)(targetWidth & 0xFF);
	header[13] = (unsigned char)((targetWidth >> 8) & 0xFF);
	header[14] = (unsigned char)(targetHeight & 0xFF);
	header[15] = (unsigned char)((targetHeight >> 8) & 0xFF);
	header[16] = 32;                                 // bits per pixel
	header[17] = 8;                                  // 8 alpha bits, bottom-left origin

	bool success = false;
	FILE* file = fopen(targetPath, "wb");
	if (file)
	{
		success = fwrite(header, 1, sizeof(header), file) == sizeof(header)
			&& fwrite(out, 1, outSize, file) == outSize;
		fclose(file);
	}

	free(out);
	return success;
}
