/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

// Tiberium Project @feature DDS textures for the SAGE 2 effects (see sage2texture.h)

#include "sage2texture.h"

#include <map>
#include <string>
#include <string.h>
#include <vector>

#include "dx8wrapper.h"
#include "sage2globals.h"
#include "WWLib/ffactory.h"
#include "WWLib/WWFILE.h"

namespace
{
	std::map<std::string, IDirect3DBaseTexture9*> s_textures;

	const unsigned DDSD_MIPMAPCOUNT = 0x20000, DDSD_DEPTH = 0x800000;
	const unsigned DDPF_ALPHAPIXELS = 0x1, DDPF_FOURCC = 0x4, DDPF_RGB = 0x40;
	const unsigned DDSCAPS2_CUBEMAP = 0x200;

	unsigned FourCC(const char* s) { return (unsigned)s[0] | ((unsigned)s[1] << 8) | ((unsigned)s[2] << 16) | ((unsigned)s[3] << 24); }

	bool Read_File(const std::string& path, std::vector<unsigned char>& data)
	{
		data.clear();
		file_auto_ptr file(_TheFileFactory, path.c_str());
		if (!file->Is_Available() || !file->Open())
			return false;
		data.resize((size_t)file->Size());
		bool ok = !data.empty() && file->Read(&data[0], (int)data.size()) == (int)data.size();
		file->Close();
		if (!ok)
			data.clear();
		return ok;
	}

	// bytes of one level of the format, and the row pitch (block rows for compressed formats)
	bool Level_Size(D3DFORMAT format, unsigned w, unsigned h, unsigned& bytes, unsigned& rows, unsigned& rowBytes)
	{
		switch (format)
		{
			case D3DFMT_DXT1:
				rows = (h + 3) / 4; rowBytes = ((w + 3) / 4) * 8; break;
			case D3DFMT_DXT3:
			case D3DFMT_DXT5:
				rows = (h + 3) / 4; rowBytes = ((w + 3) / 4) * 16; break;
			case D3DFMT_A8R8G8B8:
			case D3DFMT_X8R8G8B8:
				rows = h; rowBytes = w * 4; break;
			case D3DFMT_A16B16G16R16F:
				rows = h; rowBytes = w * 8; break;
			case D3DFMT_A32B32G32R32F:
				rows = h; rowBytes = w * 16; break;
			case D3DFMT_R16F:
				rows = h; rowBytes = w * 2; break;
			default:
				return false;
		}
		bytes = rows * rowBytes;
		return true;
	}

	// the DDS pixel format as a Direct3D format, or D3DFMT_UNKNOWN
	D3DFORMAT Pixel_Format(unsigned pfFlags, unsigned fourCC, unsigned bits, unsigned rMask, unsigned gMask, unsigned bMask, unsigned aMask)
	{
		if (pfFlags & DDPF_FOURCC)
		{
			if (fourCC == FourCC("DXT1")) return D3DFMT_DXT1;
			if (fourCC == FourCC("DXT3")) return D3DFMT_DXT3;
			if (fourCC == FourCC("DXT5")) return D3DFMT_DXT5;
			if (fourCC == 113) return D3DFMT_A16B16G16R16F;
			if (fourCC == 116) return D3DFMT_A32B32G32R32F;
			if (fourCC == 111) return D3DFMT_R16F;
			return D3DFMT_UNKNOWN;
		}
		if ((pfFlags & DDPF_RGB) && bits == 32 && rMask == 0x00FF0000 && gMask == 0x0000FF00 && bMask == 0x000000FF)
			return ((pfFlags & DDPF_ALPHAPIXELS) && aMask == 0xFF000000) ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
		// Tiberium Project @bugfix 24-bit RGB (C&C4's FXOceanCubeMap): expanded to X8R8G8B8 on load,
		// Direct3D 9 devices do not take R8G8B8 textures
		if ((pfFlags & DDPF_RGB) && bits == 24 && rMask == 0x00FF0000 && gMask == 0x0000FF00 && bMask == 0x000000FF)
			return D3DFMT_R8G8B8;
		return D3DFMT_UNKNOWN;
	}

	void Copy_Level(void* dst, unsigned dstPitch, const unsigned char* src, unsigned rows, unsigned rowBytes)
	{
		for (unsigned r = 0; r < rows; ++r)
			memcpy((char*)dst + r * dstPitch, src + r * rowBytes, rowBytes);
	}

	IDirect3DBaseTexture9* Load(IDirect3DDevice9* device, const std::string& name, std::string& what)
	{
		std::vector<unsigned char> data;
		std::string path = name + ".dds";
		if (!Read_File(path, data))
		{
			path = "Art\\Textures\\" + name + ".dds";
			if (!Read_File(path, data))
			{
				what = "not found";
				return nullptr;
			}
		}
		if (data.size() < 128 || memcmp(&data[0], "DDS ", 4) != 0)
		{
			what = "not a DDS file";
			return nullptr;
		}
		const unsigned* h = (const unsigned*)&data[4];
		unsigned flags = h[1], height = h[2], width = h[3], depth = h[5], mips = h[6];
		unsigned pfFlags = h[19], fourCC = h[20], bits = h[21];
		unsigned caps2 = h[27];
		D3DFORMAT format = Pixel_Format(pfFlags, fourCC, bits, h[22], h[23], h[24], h[25]);
		if (format == D3DFMT_UNKNOWN)
		{
			what = "pixel format not handled";
			return nullptr;
		}
		if (format == D3DFMT_R8G8B8)
		{
			// tightly packed blue, green, red triples (every level, every face) to four bytes each
			std::vector<unsigned char> expanded(128 + (data.size() - 128) / 3 * 4);
			memcpy(&expanded[0], &data[0], 128);
			const unsigned char* in = &data[128];
			unsigned char* out = &expanded[128];
			for (size_t i = 0, n = (data.size() - 128) / 3; i < n; ++i, in += 3, out += 4)
			{
				out[0] = in[0]; out[1] = in[1]; out[2] = in[2]; out[3] = 0xFF;
			}
			data.swap(expanded);
			h = (const unsigned*)&data[4];
			format = D3DFMT_X8R8G8B8;
		}
		if (!(flags & DDSD_MIPMAPCOUNT) || mips == 0)
			mips = 1;
		bool cube = (caps2 & DDSCAPS2_CUBEMAP) != 0;
		bool volume = (flags & DDSD_DEPTH) != 0 && depth > 1;
		const unsigned char* src = &data[128];
		size_t left = data.size() - 128;

		if (volume)
		{
			IDirect3DVolumeTexture9* t = nullptr;
			if (FAILED(device->CreateVolumeTexture(width, height, depth, mips, 0, format, D3DPOOL_MANAGED, &t, nullptr)))
			{
				what = "volume texture not created";
				return nullptr;
			}
			unsigned w = width, hh = height, d = depth;
			for (unsigned level = 0; level < mips; ++level)
			{
				unsigned bytes, rows, rowBytes;
				if (!Level_Size(format, w, hh, bytes, rows, rowBytes) || left < (size_t)bytes * d)
					break;
				D3DLOCKED_BOX box;
				if (SUCCEEDED(t->LockBox(level, &box, nullptr, 0)))
				{
					for (unsigned z = 0; z < d; ++z)
						Copy_Level((char*)box.pBits + z * box.SlicePitch, box.RowPitch, src + (size_t)z * bytes, rows, rowBytes);
					t->UnlockBox(level);
				}
				src += (size_t)bytes * d; left -= (size_t)bytes * d;
				w = w > 1 ? w / 2 : 1; hh = hh > 1 ? hh / 2 : 1; d = d > 1 ? d / 2 : 1;
			}
			what = "volume";
			return t;
		}
		if (cube)
		{
			IDirect3DCubeTexture9* t = nullptr;
			if (FAILED(device->CreateCubeTexture(width, mips, 0, format, D3DPOOL_MANAGED, &t, nullptr)))
			{
				what = "cube texture not created";
				return nullptr;
			}
			for (unsigned face = 0; face < 6; ++face)
			{
				unsigned w = width, hh = height;
				for (unsigned level = 0; level < mips; ++level)
				{
					unsigned bytes, rows, rowBytes;
					if (!Level_Size(format, w, hh, bytes, rows, rowBytes) || left < bytes)
						break;
					D3DLOCKED_RECT lr;
					if (SUCCEEDED(t->LockRect((D3DCUBEMAP_FACES)face, level, &lr, nullptr, 0)))
					{
						Copy_Level(lr.pBits, lr.Pitch, src, rows, rowBytes);
						t->UnlockRect((D3DCUBEMAP_FACES)face, level);
					}
					src += bytes; left -= bytes;
					w = w > 1 ? w / 2 : 1; hh = hh > 1 ? hh / 2 : 1;
				}
			}
			what = "cube";
			return t;
		}
		IDirect3DTexture9* t = nullptr;
		if (FAILED(device->CreateTexture(width, height, mips, 0, format, D3DPOOL_MANAGED, &t, nullptr)))
		{
			what = "texture not created";
			return nullptr;
		}
		unsigned w = width, hh = height;
		for (unsigned level = 0; level < mips; ++level)
		{
			unsigned bytes, rows, rowBytes;
			if (!Level_Size(format, w, hh, bytes, rows, rowBytes) || left < bytes)
				break;
			D3DLOCKED_RECT lr;
			if (SUCCEEDED(t->LockRect(level, &lr, nullptr, 0)))
			{
				Copy_Level(lr.pBits, lr.Pitch, src, rows, rowBytes);
				t->UnlockRect(level);
			}
			src += bytes; left -= bytes;
			w = w > 1 ? w / 2 : 1; hh = hh > 1 ? hh / 2 : 1;
		}
		what = "2D";
		return t;
	}
}

namespace
{
	void Decode_565(unsigned c, unsigned char* rgb)
	{
		rgb[0] = (unsigned char)(((c >> 11) & 31) * 255 / 31);
		rgb[1] = (unsigned char)(((c >> 5) & 63) * 255 / 63);
		rgb[2] = (unsigned char)((c & 31) * 255 / 31);
	}

	// one DXT1 colour block (8 bytes) into 4x4 BGRA pixels; alphaBlock: the DXT5 alpha block or null
	void Decode_Block(const unsigned char* block, const unsigned char* alphaBlock, bool dxt1, unsigned char* dst, unsigned pitch)
	{
		unsigned c0 = block[0] | (block[1] << 8), c1 = block[2] | (block[3] << 8);
		unsigned char pal[4][4];
		Decode_565(c0, pal[0]); Decode_565(c1, pal[1]);
		bool fourColour = !dxt1 || c0 > c1;
		for (int k = 0; k < 3; ++k)
		{
			pal[2][k] = fourColour ? (unsigned char)((2 * pal[0][k] + pal[1][k]) / 3) : (unsigned char)((pal[0][k] + pal[1][k]) / 2);
			pal[3][k] = fourColour ? (unsigned char)((pal[0][k] + 2 * pal[1][k]) / 3) : 0;
		}
		pal[0][3] = pal[1][3] = pal[2][3] = 255;
		pal[3][3] = fourColour ? 255 : 0;
		unsigned bits = block[4] | (block[5] << 8) | (block[6] << 16) | ((unsigned)block[7] << 24);
		unsigned char alpha[16];
		if (alphaBlock != nullptr)
		{
			unsigned a0 = alphaBlock[0], a1 = alphaBlock[1];
			unsigned char ramp[8];
			ramp[0] = (unsigned char)a0; ramp[1] = (unsigned char)a1;
			if (a0 > a1)
				for (int i = 1; i < 7; ++i) ramp[i + 1] = (unsigned char)(((7 - i) * a0 + i * a1) / 7);
			else
			{
				for (int i = 1; i < 5; ++i) ramp[i + 1] = (unsigned char)(((5 - i) * a0 + i * a1) / 5);
				ramp[6] = 0; ramp[7] = 255;
			}
			unsigned long long abits = 0;
			for (int i = 0; i < 6; ++i)
				abits |= (unsigned long long)alphaBlock[2 + i] << (8 * i);
			for (int i = 0; i < 16; ++i)
				alpha[i] = ramp[(abits >> (3 * i)) & 7];
		}
		for (int y = 0; y < 4; ++y)
			for (int x = 0; x < 4; ++x)
			{
				const unsigned char* p = pal[(bits >> (2 * (y * 4 + x))) & 3];
				unsigned char* o = dst + y * pitch + x * 4;
				o[0] = p[2]; o[1] = p[1]; o[2] = p[0];
				o[3] = alphaBlock != nullptr ? alpha[y * 4 + x] : p[3];
			}
	}
}

//-------------------------------------------------------------------------------------------------
bool Sage2TextureClass::Load_Pixels(const char* name, unsigned& width, unsigned& height, std::vector<unsigned char>& bgra)
{
	bgra.clear();
	if (name == nullptr || name[0] == '\0')
		return false;
	std::string key(name);
	for (size_t i = 0; i < key.size(); ++i)
		key[i] = (char)tolower((unsigned char)key[i]);
	std::vector<unsigned char> data;
	if (!Read_File(key + ".dds", data) && !Read_File("Art\\Textures\\" + key + ".dds", data))
		return false;
	if (data.size() < 128 || memcmp(&data[0], "DDS ", 4) != 0)
		return false;
	const unsigned* h = (const unsigned*)&data[4];
	height = h[2]; width = h[3];
	D3DFORMAT format = Pixel_Format(h[19], h[20], h[21], h[22], h[23], h[24], h[25]);
	const unsigned char* src = &data[128];
	size_t left = data.size() - 128;
	if (width == 0 || height == 0)
		return false;
	bgra.resize((size_t)width * height * 4);
	if (format == D3DFMT_A8R8G8B8 || format == D3DFMT_X8R8G8B8)
	{
		if (left < bgra.size())
			return false;
		memcpy(&bgra[0], src, bgra.size());
		if (format == D3DFMT_X8R8G8B8)
			for (size_t i = 3; i < bgra.size(); i += 4)
				bgra[i] = 255;
		return true;
	}
	if (format == D3DFMT_DXT1 || format == D3DFMT_DXT3 || format == D3DFMT_DXT5)
	{
		bool dxt1 = format == D3DFMT_DXT1;
		unsigned blockBytes = dxt1 ? 8 : 16;
		unsigned bw = (width + 3) / 4, bh = (height + 3) / 4;
		if (left < (size_t)bw * bh * blockBytes || (width % 4) != 0 || (height % 4) != 0)
			return false;
		for (unsigned by = 0; by < bh; ++by)
			for (unsigned bx = 0; bx < bw; ++bx)
			{
				const unsigned char* block = src + ((size_t)by * bw + bx) * blockBytes;
				unsigned char* dst = &bgra[((size_t)by * 4 * width + bx * 4) * 4];
				if (format == D3DFMT_DXT3)
				{
					// 4-bit alpha, then the colour block
					Decode_Block(block + 8, nullptr, false, dst, width * 4);
					for (int y = 0; y < 4; ++y)
						for (int x = 0; x < 4; ++x)
						{
							unsigned a = (block[y * 2 + x / 2] >> ((x & 1) * 4)) & 15;
							dst[(y * width + x) * 4 + 3] = (unsigned char)(a * 17);
						}
				}
				else
					Decode_Block(dxt1 ? block : block + 8, dxt1 ? nullptr : block, dxt1, dst, width * 4);
			}
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
IDirect3DBaseTexture9* Sage2TextureClass::Get(const char* name)
{
	if (name == nullptr || name[0] == '\0')
		return nullptr;
	std::string key(name);
	for (size_t i = 0; i < key.size(); ++i)
		key[i] = (char)tolower((unsigned char)key[i]);
	std::map<std::string, IDirect3DBaseTexture9*>::iterator it = s_textures.find(key);
	if (it != s_textures.end())
		return it->second;
	IDirect3DBaseTexture9* texture = nullptr;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	if (device8 != nullptr)
	{
		std::string what;
		texture = Load(device8->Get_D3D9(), key, what);
		Sage2_Log("texture %s.dds: %s%s", key.c_str(), what.c_str(), texture ? " loaded" : "");
	}
	s_textures[key] = texture;
	return texture;
}

//-------------------------------------------------------------------------------------------------
void Sage2TextureClass::Shutdown()
{
	for (std::map<std::string, IDirect3DBaseTexture9*>::iterator it = s_textures.begin(); it != s_textures.end(); ++it)
		if (it->second != nullptr)
			it->second->Release();
	s_textures.clear();
}
