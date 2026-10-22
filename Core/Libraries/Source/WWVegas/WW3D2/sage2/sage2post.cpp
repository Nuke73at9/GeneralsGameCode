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

// Tiberium Project @feature C&C4's post-processing chain (see sage2post.h)

#include "sage2post.h"

#include <map>
#include <string>
#include <string.h>
#include <vector>

#include "dx8wrapper.h"
#include "sage2effect.h"
#include "sage2globals.h"
#include "sage2material.h"
#include "WWLib/ffactory.h"
#include "WWLib/WWFILE.h"

namespace
{
	enum InitState { NOT_TRIED, READY, UNAVAILABLE };

	bool s_enabled = true;
	InitState s_state = NOT_TRIED;

	// the look
	std::string s_lookupName = "pan_am_lut_vol";
	float s_lookupBlend = 0.745f;
	float s_bloomIntensity = 0.09f;
	float s_exposure = 1.0f;

	Sage2EffectClass* s_bloom = nullptr;
	Sage2EffectClass* s_lookup = nullptr;
	int s_techDownsampleInitial = -1, s_techDownsample4x4 = -1, s_techBlurU = -1, s_techBlurV = -1;
	int s_techAccumulate = -1, s_techAccumulateFinal = -1, s_techLookup = -1;

	// the grading tables, by name (kept for the session; they are small)
	std::map<std::string, IDirect3DVolumeTexture9*> s_lookupTextures;

	// render targets, made for the frame size
	UINT s_width = 0, s_height = 0;
	IDirect3DTexture9* s_frame = nullptr;		// copy of the frame, display-encoded
	IDirect3DTexture9* s_linear = nullptr;		// the frame plus bloom, linear
	IDirect3DTexture9* s_high[2] = { nullptr, nullptr };	// 256 wide
	IDirect3DTexture9* s_medium[2] = { nullptr, nullptr };	// 64 wide
	IDirect3DVertexBuffer9* s_quad = nullptr;
	IDirect3DVertexDeclaration9* s_quadDeclaration = nullptr;
	IDirect3DStateBlock9* s_savedState = nullptr;

	template <class T> void Release(T*& p) { if (p) { p->Release(); p = nullptr; } }

	IDirect3DDevice9* Device()
	{
		IDirect3DDevice8* device = DX8Wrapper::_Get_D3D_Device8();
		return device ? device->Get_D3D9() : nullptr;
	}

	bool Make_Effects()
	{
		if (s_bloom && s_lookup)
			return true;
		s_bloom = Sage2MaterialClass::Get_Effect("postfx_bloom");
		s_lookup = Sage2MaterialClass::Get_Effect("postfx_lookuptable");
		if (s_bloom == nullptr || s_lookup == nullptr)
			return false;
		s_techDownsampleInitial = s_bloom->Find_Technique("DownsampleInitial");
		s_techDownsample4x4 = s_bloom->Find_Technique("Downsample4x4");
		s_techBlurU = s_bloom->Find_Technique("BlurGaussian11x11U");
		s_techBlurV = s_bloom->Find_Technique("BlurGaussian11x11V");
		s_techAccumulate = s_bloom->Find_Technique("Accumulate");
		s_techAccumulateFinal = s_bloom->Find_Technique("AccumulateFinal");
		s_techLookup = s_lookup->Find_Technique("Default");
		return s_techDownsampleInitial >= 0 && s_techDownsample4x4 >= 0 && s_techBlurU >= 0 && s_techBlurV >= 0
			&& s_techAccumulate >= 0 && s_techAccumulateFinal >= 0 && s_techLookup >= 0;
	}

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

	// A C&C4 grading table: a DDS volume texture, 32-bit colour, one level
	IDirect3DVolumeTexture9* Load_Lookup_Texture(IDirect3DDevice9* device, const std::string& name)
	{
		std::map<std::string, IDirect3DVolumeTexture9*>::iterator it = s_lookupTextures.find(name);
		if (it != s_lookupTextures.end())
			return it->second;
		IDirect3DVolumeTexture9* texture = nullptr;
		// the game's file factory puts .dds names under Art\Textures itself; a plain one needs the path
		std::string path = name + ".dds";
		std::vector<unsigned char> data;
		if (!Read_File(path, data))
		{
			path = "Art\\Textures\\" + name + ".dds";
			Read_File(path, data);
		}
		{
			if (data.size() > 128 && memcmp(&data[0], "DDS ", 4) == 0)
			{
				const unsigned* h = (const unsigned*)&data[4];	// the DDS header, after the magic
				unsigned flags = h[1], height = h[2], width = h[3], depth = h[5];
				unsigned pfFlags = h[19], fourCC = h[20], bits = h[21];
				const unsigned DDSD_DEPTH = 0x800000, DDPF_FOURCC = 0x4;
				if ((flags & DDSD_DEPTH) && !(pfFlags & DDPF_FOURCC) && fourCC == 0 && bits == 32
					&& data.size() >= 128 + (size_t)width * height * depth * 4
					&& SUCCEEDED(device->CreateVolumeTexture(width, height, depth, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr)))
				{
					D3DLOCKED_BOX box;
					if (SUCCEEDED(texture->LockBox(0, &box, nullptr, 0)))
					{
						const unsigned char* src = &data[128];
						for (unsigned z = 0; z < depth; ++z)
							for (unsigned y = 0; y < height; ++y)
								memcpy((char*)box.pBits + z * box.SlicePitch + y * box.RowPitch, src + ((size_t)z * height + y) * width * 4, (size_t)width * 4);
						texture->UnlockBox(0);
					}
					else
						Release(texture);
				}
				else
					Sage2_Log("post: %s is not a 32-bit volume texture (flags %x, pixel format %x/%x/%u)", path.c_str(), flags, pfFlags, fourCC, bits);
			}
		}
		Sage2_Log("post: grading table %s %s", path.c_str(), texture ? "loaded" : "not loaded");
		s_lookupTextures[name] = texture;
		return texture;
	}

	IDirect3DTexture9* Make_Target(IDirect3DDevice9* device, UINT w, UINT h, D3DFORMAT format)
	{
		IDirect3DTexture9* t = nullptr;
		if (FAILED(device->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &t, nullptr)))
			return nullptr;
		return t;
	}

	bool Make_Resources(IDirect3DDevice9* device, UINT w, UINT h)
	{
		if (s_frame && s_width == w && s_height == h)
			return true;
		Sage2PostClass::Release_Resources();
		s_width = w;
		s_height = h;
		D3DFORMAT linearFormat = D3DFMT_A16B16G16R16F;
		IDirect3D9* d3d = DX8Wrapper::_Get_D3D8()->Get_D3D9();
		D3DDISPLAYMODE mode;
		if (FAILED(d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode)))
			mode.Format = D3DFMT_X8R8G8B8;
		if (FAILED(d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, linearFormat)))
			linearFormat = D3DFMT_A8R8G8B8;
		// C&C4 blooms in a 256-wide buffer whatever the screen, then a 64-wide one
		UINT hw = 256, hh = (256 * h / w + 1) & ~1u;
		if (hh < 2) hh = 2;
		UINT mw = 64, mh = hh / 4;
		if (mh < 1) mh = 1;
		s_frame = Make_Target(device, w, h, D3DFMT_A8R8G8B8);
		s_linear = Make_Target(device, w, h, linearFormat);
		for (int i = 0; i < 2; ++i)
		{
			s_high[i] = Make_Target(device, hw, hh, linearFormat);
			s_medium[i] = Make_Target(device, mw, mh, linearFormat);
		}
		if (FAILED(device->CreateVertexBuffer(4 * 20, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &s_quad, nullptr)))
			s_quad = nullptr;
		if (s_quadDeclaration == nullptr)
		{
			D3DVERTEXELEMENT9 el[] =
			{
				{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
				{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
				D3DDECL_END()
			};
			if (FAILED(device->CreateVertexDeclaration(el, &s_quadDeclaration)))
				s_quadDeclaration = nullptr;
		}
		if (!s_frame || !s_linear || !s_high[0] || !s_high[1] || !s_medium[0] || !s_medium[1] || !s_quad || !s_quadDeclaration)
		{
			Sage2PostClass::Release_Resources();
			return false;
		}
		Sage2_Log("post: targets %ux%u, bloom %ux%u and %ux%u, %s", w, h, hw, hh, mw, mh, linearFormat == D3DFMT_A16B16G16R16F ? "half float" : "8 bit");
		return true;
	}

	void Set_Size(Sage2EffectClass& effect, const char* name, IDirect3DTexture9* texture)
	{
		D3DSURFACE_DESC desc;
		if (texture == nullptr || FAILED(texture->GetLevelDesc(0, &desc)))
			return;
		float v[2] = { (float)desc.Width, (float)desc.Height };
		effect.Set_Float(name, v, 2);
	}

	// Draws the full-screen quad with the technique into the target. Textures named in 'decode'
	// are read as sRGB (display-encoded to linear), the way C&C4's effects expect the frame.
	bool Draw(IDirect3DDevice9* device, Sage2EffectClass& effect, int technique, IDirect3DSurface9* target, UINT w, UINT h,
		IDirect3DBaseTexture9* decode)
	{
		device->SetRenderTarget(0, target);
		D3DVIEWPORT9 vp = { 0, 0, w, h, 0.0f, 1.0f };
		device->SetViewport(&vp);
		float hx = 1.0f / (float)w, hy = 1.0f / (float)h;	// half-pixel: texel centres on pixel centres
		const float v[4][5] =
		{
			{ -1.0f - hx,  1.0f + hy, 0.0f, 0.0f, 0.0f },
			{  1.0f - hx,  1.0f + hy, 0.0f, 1.0f, 0.0f },
			{ -1.0f - hx, -1.0f + hy, 0.0f, 0.0f, 1.0f },
			{  1.0f - hx, -1.0f + hy, 0.0f, 1.0f, 1.0f },
		};
		void* p = nullptr;
		if (FAILED(s_quad->Lock(0, 0, &p, D3DLOCK_DISCARD)))
			return false;
		memcpy(p, v, sizeof(v));
		s_quad->Unlock();
		device->SetVertexDeclaration(s_quadDeclaration);
		device->SetStreamSource(0, s_quad, 0, 20);
		if (!effect.Begin(device, technique, 0))
			return false;
		DWORD decoded[16];
		int decodedCount = 0;
		if (decode != nullptr)
		{
			for (DWORD s = 0; s < 16; ++s)
			{
				IDirect3DBaseTexture9* bound = nullptr;
				if (SUCCEEDED(device->GetTexture(s, &bound)) && bound != nullptr)
				{
					if (bound == decode)
					{
						device->SetSamplerState(s, D3DSAMP_SRGBTEXTURE, TRUE);
						decoded[decodedCount++] = s;
					}
					bound->Release();
				}
			}
		}
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
		device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		effect.End(device);
		for (int i = 0; i < decodedCount; ++i)
			device->SetSamplerState(decoded[i], D3DSAMP_SRGBTEXTURE, FALSE);
		return true;
	}

	IDirect3DSurface9* Surface(IDirect3DTexture9* t)
	{
		IDirect3DSurface9* s = nullptr;
		if (t) t->GetSurfaceLevel(0, &s);
		return s;
	}

	struct Target
	{
		IDirect3DTexture9* texture;
		IDirect3DSurface9* surface;
		UINT w, h;
		Target(IDirect3DTexture9* t) : texture(t), surface(Surface(t)), w(0), h(0)
		{
			D3DSURFACE_DESC desc;
			if (t && SUCCEEDED(t->GetLevelDesc(0, &desc))) { w = desc.Width; h = desc.Height; }
		}
		~Target() { Release(surface); }
	};
}

//-------------------------------------------------------------------------------------------------
void Sage2PostClass::Set_Enabled(bool enabled) { s_enabled = enabled; }
bool Sage2PostClass::Is_Enabled() { return s_enabled; }

//-------------------------------------------------------------------------------------------------
void Sage2PostClass::Set_Look(const char* lookupTexture, float lookupBlend, float bloomIntensity)
{
	s_lookupName = lookupTexture != nullptr ? lookupTexture : "";
	s_lookupBlend = lookupBlend;
	s_bloomIntensity = bloomIntensity;
}

//-------------------------------------------------------------------------------------------------
void Sage2PostClass::Set_Exposure(float exposure)
{
	s_exposure = exposure > 0.0f ? exposure : 1.0f;
}

//-------------------------------------------------------------------------------------------------
bool Sage2PostClass::Apply()
{
	if (!s_enabled)
		return false;
	IDirect3DDevice9* device = Device();
	if (device == nullptr)
		return false;
	if (s_state == NOT_TRIED)
	{
		s_state = UNAVAILABLE;
		D3DCAPS9 caps;
		if (SUCCEEDED(device->GetDeviceCaps(&caps)) && caps.PixelShaderVersion >= D3DPS_VERSION(3, 0) && Make_Effects())
			s_state = READY;
		Sage2_Log("post: C&C4 chain %s", s_state == READY ? "ready" : "unavailable");
	}
	if (s_state != READY)
		return false;

	IDirect3DSurface9* frame = nullptr;
	if (FAILED(device->GetRenderTarget(0, &frame)) || frame == nullptr)
		return false;
	D3DSURFACE_DESC desc;
	frame->GetDesc(&desc);
	if (!Make_Resources(device, desc.Width, desc.Height))
	{
		frame->Release();
		return false;
	}
	IDirect3DVolumeTexture9* lookup = s_lookupName.empty() ? nullptr : Load_Lookup_Texture(device, s_lookupName);

	if (s_savedState == nullptr && FAILED(device->CreateStateBlock(D3DSBT_ALL, &s_savedState)))
	{
		s_savedState = nullptr;
		frame->Release();
		return false;
	}
	s_savedState->Capture();
	IDirect3DSurface9* depth = nullptr;
	device->GetDepthStencilSurface(&depth);
	device->SetDepthStencilSurface(nullptr);

	Target tFrame(s_frame), tLinear(s_linear), tHigh0(s_high[0]), tHigh1(s_high[1]), tMedium0(s_medium[0]), tMedium1(s_medium[1]);
	device->StretchRect(frame, nullptr, tFrame.surface, nullptr, D3DTEXF_NONE);

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_FOGENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);

	// the bloom, as C&C4's PostFX_Bloom script orders it
	Sage2EffectClass& bloom = *s_bloom;
	float one[3] = { s_exposure, s_exposure, s_exposure };
	bloom.Set_Float("ExposureLevel", one, 3);
	bloom.Set_Float1("BloomIntensity", s_bloomIntensity);
	bloom.Set_Texture("SourceTexture", s_frame);
	Set_Size(bloom, "SourceTextureSize", s_frame);
	Set_Size(bloom, "TargetTextureSize", s_high[0]);
	Draw(device, bloom, s_techDownsampleInitial, tHigh0.surface, tHigh0.w, tHigh0.h, s_frame);
	bloom.Set_Texture("SourceTexture", s_high[0]);
	Set_Size(bloom, "SourceTextureSize", s_high[0]);
	Draw(device, bloom, s_techDownsample4x4, tMedium0.surface, tMedium0.w, tMedium0.h, nullptr);
	bloom.Set_Texture("FrameBufferSampler", s_medium[0]);
	bloom.Set_Texture("SourceTexture", s_medium[0]);
	Draw(device, bloom, s_techAccumulate, tMedium1.surface, tMedium1.w, tMedium1.h, nullptr);
	bloom.Set_Texture("SourceTexture", s_medium[1]);
	Set_Size(bloom, "SourceTextureSize", s_medium[1]);
	Draw(device, bloom, s_techBlurU, tMedium0.surface, tMedium0.w, tMedium0.h, nullptr);
	bloom.Set_Texture("SourceTexture", s_medium[0]);
	Draw(device, bloom, s_techBlurV, tMedium1.surface, tMedium1.w, tMedium1.h, nullptr);
	bloom.Set_Texture("FrameBufferSampler", s_high[0]);
	bloom.Set_Texture("SourceTexture", s_medium[1]);
	Draw(device, bloom, s_techAccumulate, tHigh1.surface, tHigh1.w, tHigh1.h, nullptr);
	bloom.Set_Texture("SourceTexture", s_high[1]);
	Set_Size(bloom, "SourceTextureSize", s_high[1]);
	Draw(device, bloom, s_techBlurU, tHigh0.surface, tHigh0.w, tHigh0.h, nullptr);
	bloom.Set_Texture("SourceTexture", s_high[0]);
	Draw(device, bloom, s_techBlurV, tHigh1.surface, tHigh1.w, tHigh1.h, nullptr);
	bloom.Set_Texture("FrameBufferSampler", s_frame);
	bloom.Set_Texture("SourceTexture", s_high[1]);
	Draw(device, bloom, s_techAccumulateFinal, tLinear.surface, tLinear.w, tLinear.h, s_frame);

	// exposure, tone curve and the grading table, back into the frame
	Sage2EffectClass& grade = *s_lookup;
	grade.Set_Texture("FrameBufferSampler", s_linear);
	grade.Set_Texture("LookupTexture", lookup);
	grade.Set_Float("ExposureLevel", one, 3);
	float blend[4] = { lookup ? s_lookupBlend : 0.0f, 0.0f, 0.0f, 0.0f };
	grade.Set_Float("BlendFactor", blend, 4);
	grade.Set_Bool("ApplyLUT", lookup != nullptr);
	grade.Set_Bool("ApplyOverlay", false);
	grade.Set_Bool("ApplyFalloffAndGrain", false);
	grade.Set_Bool("ApplyColorMask", false);
	Draw(device, grade, s_techLookup, frame, desc.Width, desc.Height, nullptr);

	device->SetRenderTarget(0, frame);
	device->SetDepthStencilSurface(depth);
	Release(depth);
	s_savedState->Apply();
	DX8Wrapper::Invalidate_Shader_Constants();
	frame->Release();

	static bool logged = false;
	if (!logged)
	{
		logged = true;
		Sage2_Log("post: C&C4 chain applied: bloom %.2f, table %s blend %.2f, exposure %.2f", s_bloomIntensity, s_lookupName.c_str(), blend[0], s_exposure);
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
void Sage2PostClass::Release_Resources()
{
	Release(s_frame);
	Release(s_linear);
	for (int i = 0; i < 2; ++i)
	{
		Release(s_high[i]);
		Release(s_medium[i]);
	}
	Release(s_quad);
	Release(s_savedState);	// holds references to whatever was bound; made again on the next Apply()
	s_width = s_height = 0;
}

//-------------------------------------------------------------------------------------------------
void Sage2PostClass::Shutdown()
{
	Release_Resources();
	Release(s_quadDeclaration);
	for (std::map<std::string, IDirect3DVolumeTexture9*>::iterator it = s_lookupTextures.begin(); it != s_lookupTextures.end(); ++it)
		Release(it->second);
	s_lookupTextures.clear();
	s_bloom = s_lookup = nullptr;	// owned by the effect cache
	s_state = NOT_TRIED;
}
