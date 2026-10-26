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

// Tiberium Project @feature Direct3D 9 renderer: the Direct3D 8 adapter (see d3d8on9.h)

#include "d3d8on9.h"

#include "../shaders/fixedfunction_vs.h"	// Tiberium Project @feature from shaders/fixedfunction.hlsl (build_shaders.py)
#include "../shaders/fixedfunction_screen_vs.h"
#include "../shaders/fixedfunction_trees_vs.h"
#include "../shaders/fixedfunction_ps.h"
#include "../shaders/fixedfunction_modulate_ps.h"
#include "../shaders/fixedfunction_diffuse_ps.h"
#include "../shaders/fixedfunction_texture_ps.h"

#include <cstring>
#include <cmath>

namespace
{
	/// Direct3D 8 shader handles: FVF codes have bit 0 clear, shader handles have it set
	const DWORD SHADER_HANDLE_BIT = 1;

	/// Direct3D 8's depth bias was an integer in steps of the depth buffer; Direct3D 9's is a float
	/// in depth units. Same scale as DXVK's Direct3D 8 (one step of a 16-bit depth buffer).
	const float ZBIAS_SCALE = -1.0f / ((1u << 16) - 1);

	bool isSamplerState(D3DTEXTURESTAGESTATETYPE type, D3DSAMPLERSTATETYPE* out)
	{
		switch ((int)type)
		{
			case 13: *out = D3DSAMP_ADDRESSU; return true;
			case 14: *out = D3DSAMP_ADDRESSV; return true;
			case 15: *out = D3DSAMP_BORDERCOLOR; return true;
			case 16: *out = D3DSAMP_MAGFILTER; return true;
			case 17: *out = D3DSAMP_MINFILTER; return true;
			case 18: *out = D3DSAMP_MIPFILTER; return true;
			case 19: *out = D3DSAMP_MIPMAPLODBIAS; return true;
			case 20: *out = D3DSAMP_MAXMIPLEVEL; return true;
			case 21: *out = D3DSAMP_MAXANISOTROPY; return true;
			case 25: *out = D3DSAMP_ADDRESSW; return true;
			default: return false;
		}
	}

	/// Bytes in one row of blocks, and the block size, of a surface format
	void formatLayout(D3DFORMAT format, UINT* bytesPerBlock, UINT* blockSize)
	{
		*blockSize = 1;
		switch (format)
		{
			case D3DFMT_DXT1: *bytesPerBlock = 8; *blockSize = 4; return;
			case D3DFMT_DXT2: case D3DFMT_DXT3: case D3DFMT_DXT4: case D3DFMT_DXT5:
				*bytesPerBlock = 16; *blockSize = 4; return;
			case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8:
			case D3DFMT_A2R10G10B10: case D3DFMT_A2B10G10R10: case D3DFMT_G16R16: case D3DFMT_D32:
			case D3DFMT_D24S8: case D3DFMT_D24X8: case D3DFMT_Q8W8V8U8: case D3DFMT_X8L8V8U8:
				*bytesPerBlock = 4; return;
			case D3DFMT_R8G8B8: *bytesPerBlock = 3; return;
			case D3DFMT_A8: case D3DFMT_L8: case D3DFMT_P8: case D3DFMT_A4L4: case D3DFMT_R3G3B2:
				*bytesPerBlock = 1; return;
			default: *bytesPerBlock = 2; return;	// 16-bit colour, luminance-alpha, bump and depth formats
		}
	}

	/// Tiberium Project @bugfix A4R4G4B4 pixels to A8R8G8B8 (X8R8G8B8 alike), for a copy into a
	/// texture the device made in a 32-bit format because it does not offer the 16-bit one
	HRESULT lockedCopyA4R4G4B4ToA8R8G8B8(IDirect3DSurface9* src, const RECT& r, IDirect3DSurface9* dst, const POINT& p)
	{
		RECT d = { p.x, p.y, p.x + (r.right - r.left), p.y + (r.bottom - r.top) };
		D3DLOCKED_RECT sl, dl;
		HRESULT hr = src->LockRect(&sl, &r, D3DLOCK_READONLY);
		if (FAILED(hr))
			return hr;
		hr = dst->LockRect(&dl, &d, 0);
		if (FAILED(hr))
		{
			src->UnlockRect();
			return hr;
		}
		const UINT width = (UINT)(r.right - r.left);
		const UINT height = (UINT)(r.bottom - r.top);
		for (UINT y = 0; y < height; ++y)
		{
			const WORD* in = (const WORD*)((const BYTE*)sl.pBits + y * sl.Pitch);
			DWORD* out = (DWORD*)((BYTE*)dl.pBits + y * dl.Pitch);
			for (UINT x = 0; x < width; ++x)
			{
				const DWORD v = in[x];
				// each 4-bit channel to 8 bits (0xF -> 0xFF)
				const DWORD a = ((v >> 12) & 0xF) * 0x11, rr = ((v >> 8) & 0xF) * 0x11;
				const DWORD g = ((v >> 4) & 0xF) * 0x11, b = (v & 0xF) * 0x11;
				out[x] = (a << 24) | (rr << 16) | (g << 8) | b;
			}
		}
		dst->UnlockRect();
		src->UnlockRect();
		return D3D_OK;
	}

	/// Copy a rectangle between two lockable surfaces of the same format
	HRESULT lockedCopy(IDirect3DSurface9* src, const RECT& r, IDirect3DSurface9* dst, const POINT& p, D3DFORMAT format)
	{
		UINT bpb, block;
		formatLayout(format, &bpb, &block);
		RECT d = { p.x, p.y, p.x + (r.right - r.left), p.y + (r.bottom - r.top) };
		D3DLOCKED_RECT sl, dl;
		HRESULT hr = src->LockRect(&sl, &r, D3DLOCK_READONLY);
		if (FAILED(hr))
			return hr;
		hr = dst->LockRect(&dl, &d, 0);
		if (FAILED(hr))
		{
			src->UnlockRect();
			return hr;
		}
		UINT rows = (r.bottom - r.top + block - 1) / block;
		UINT rowBytes = ((r.right - r.left + block - 1) / block) * bpb;
		for (UINT y = 0; y < rows; ++y)
			memcpy((BYTE*)dl.pBits + y * dl.Pitch, (const BYTE*)sl.pBits + y * sl.Pitch, rowBytes);
		dst->UnlockRect();
		src->UnlockRect();
		return D3D_OK;
	}

	/// Direct3D 8 declaration data type -> Direct3D 9 vertex element type
	BYTE declType(DWORD type8)
	{
		switch (type8)
		{
			case 0: return D3DDECLTYPE_FLOAT1;
			case 1: return D3DDECLTYPE_FLOAT2;
			case 2: return D3DDECLTYPE_FLOAT3;
			case 3: return D3DDECLTYPE_FLOAT4;
			case 4: return D3DDECLTYPE_D3DCOLOR;
			case 5: return D3DDECLTYPE_UBYTE4;
			case 6: return D3DDECLTYPE_SHORT2;
			case 7: return D3DDECLTYPE_SHORT4;
			default: return D3DDECLTYPE_UNUSED;
		}
	}

	UINT declSize(DWORD type8)
	{
		static const UINT sizes[] = { 4, 8, 12, 16, 4, 4, 4, 8 };
		return type8 < 8 ? sizes[type8] : 0;
	}

	/// Direct3D 8 vertex register (v0..v16) -> Direct3D 9 usage, as the fixed declaration registers mean
	void registerUsage(DWORD reg, BYTE* usage, BYTE* index)
	{
		*index = 0;
		switch (reg)
		{
			case 0: *usage = D3DDECLUSAGE_POSITION; return;
			case 1: *usage = D3DDECLUSAGE_BLENDWEIGHT; return;
			case 2: *usage = D3DDECLUSAGE_BLENDINDICES; return;
			case 3: *usage = D3DDECLUSAGE_NORMAL; return;
			case 4: *usage = D3DDECLUSAGE_PSIZE; return;
			case 5: *usage = D3DDECLUSAGE_COLOR; return;
			case 6: *usage = D3DDECLUSAGE_COLOR; *index = 1; return;
			case 15: *usage = D3DDECLUSAGE_POSITION; *index = 1; return;
			case 16: *usage = D3DDECLUSAGE_NORMAL; *index = 1; return;
			default: *usage = D3DDECLUSAGE_TEXCOORD; *index = (BYTE)(reg - 7); return;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void D3D8On9_Convert_Caps(D3DCAPS9* caps)
{
	// Direct3D 9's depth bias replaces Direct3D 8's z-bias, which SetRenderState translates
	if (caps->RasterCaps & D3DPRASTERCAPS_DEPTHBIAS)
		caps->RasterCaps |= D3DPRASTERCAPS_ZBIAS;
	caps->Caps2 |= D3DCAPS2_CANRENDERWINDOWED;
}

//-------------------------------------------------------------------------------------------------
UINT D3D8On9_Surface_Size(const D3DSURFACE_DESC& desc)
{
	UINT bpb, block;
	formatLayout(desc.Format, &bpb, &block);
	return ((desc.Width + block - 1) / block) * ((desc.Height + block - 1) / block) * bpb;
}

//-------------------------------------------------------------------------------------------------
D3DPRESENT_PARAMETERS D3D8On9_Present_Parameters(const D3DPRESENT_PARAMETERS& params8)
{
	D3DPRESENT_PARAMETERS params = params8;
	if (params.SwapEffect == D3DSWAPEFFECT_COPY_VSYNC)
	{
		params.SwapEffect = D3DSWAPEFFECT_COPY;
		params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	}
	else if (params.Windowed)
	{
		params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	}
	return params;
}

/// Hand back what Direct3D 9 filled in (back buffer size and format), keeping the caller's settings
static void returnPresentParameters(const D3DPRESENT_PARAMETERS& params, D3DPRESENT_PARAMETERS* params8)
{
	params8->BackBufferWidth = params.BackBufferWidth;
	params8->BackBufferHeight = params.BackBufferHeight;
	params8->BackBufferFormat = params.BackBufferFormat;
	params8->BackBufferCount = params.BackBufferCount;
}

//-------------------------------------------------------------------------------------------------
ULONG D3D8On9SwapChain::Release()
{
	ULONG refs = --m_refs;
	if (refs == 0)
	{
		m_chain->Release();
		delete this;
	}
	return refs;
}

//-------------------------------------------------------------------------------------------------
D3D8On9Device::D3D8On9Device(IDirect3DDevice9* device, DWORD behaviorFlags) :
	m_device(device),
	m_behaviorFlags(behaviorFlags),
	m_refs(1),
	m_baseVertexIndex(0),
	m_vertexShader(~0u),	// nothing set yet, so the first SetVertexShader always applies
	m_vertexShaderOverride(nullptr),
	m_vertexDeclarationOverride(nullptr),
	m_readableDepth(nullptr),
	m_readableDepthSurface(nullptr),
	m_ffVertexShader(nullptr),
	m_ffScreenShader(nullptr),
	m_ffPixelShader(nullptr),
	m_ffModulateShader(nullptr),
	m_ffDiffuseShader(nullptr),
	m_ffTextureShader(nullptr),
	m_ffTried(false)
{
	memset(&m_ff, 0, sizeof(m_ff));	// nothing known: read from the device before the first draw
}

D3D8On9Device::~D3D8On9Device()
{
	for (size_t i = 0; i < m_vertexShaders.size(); ++i)
	{
		if (m_vertexShaders[i].decl) m_vertexShaders[i].decl->Release();
		if (m_vertexShaders[i].shader) m_vertexShaders[i].shader->Release();
		if (m_vertexShaders[i].substitute) m_vertexShaders[i].substitute->Release();
	}
	for (size_t i = 0; i < m_pixelShaders.size(); ++i)
	{
		if (m_pixelShaders[i]) m_pixelShaders[i]->Release();
	}
	Release_Readable_Depth();
	Release_Fixed_Function_Shaders();
	m_device->Release();
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature The scene's depth buffer, readable by shaders (for SSAO): an INTZ texture
// (24-bit depth and 8-bit stencil, as the automatic D24S8 buffer) bound in its place. The automatic one
// stays made (a reset brings it back by itself) and takes over again whenever this can't be had: no
// INTZ on the device, or a multisampled back buffer (a depth texture can't be multisampled).
namespace
{
	bool s_wantReadableDepth = false;

	// draw statistics (D3D8On9_Get_Draw_Stats)
	const int MAX_DRAW_PHASES = 24;
	bool s_drawStats = false;
	const char* s_drawPhase = "unnamed";
	D3D8On9DrawStats s_drawPhases[MAX_DRAW_PHASES];
	int s_drawPhaseCount = 0;
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature The fixed-function pipeline as shaders (see shaders/fixedfunction.hlsl)
namespace
{
	bool s_ffShaders = false;
	unsigned s_ffEmulated = 0, s_ffLeft = 0, s_ffReasons = 0;

	enum FFReason
	{
		FF_FOG = 1, FF_BLEND = 2, FF_POSITION = 4, FF_POINT_SIZE = 8, FF_STAGES = 16, FF_OPERATION = 32,
		FF_LIGHTS = 64, FF_TEXGEN = 128, FF_PIXEL_SHADER = 256, FF_SHADERS = 512
	};
	const DWORD FF_PROGRAMMABLE = 0xFFFFFFFF;	// Build_Fixed_Function: the pixel stage after a stand-in vertex shader

	void Color_To_Float(D3DCOLOR c, float out[4])
	{
		out[0] = ((c >> 16) & 0xFF) / 255.0f;
		out[1] = ((c >> 8) & 0xFF) / 255.0f;
		out[2] = (c & 0xFF) / 255.0f;
		out[3] = ((c >> 24) & 0xFF) / 255.0f;
	}

	void Value_To_Float(const D3DCOLORVALUE& c, float out[4])
	{
		out[0] = c.r; out[1] = c.g; out[2] = c.b; out[3] = c.a;
	}

	void Multiply_Matrix(const D3DMATRIX& a, const D3DMATRIX& b, D3DMATRIX& out)
	{
		D3DMATRIX r;
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
		out = r;
	}

	void Transposed_Matrix(const D3DMATRIX& m, float out[16])
	{
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				out[i * 4 + j] = m.m[j][i];
	}

	float DWORD_Float(DWORD v)
	{
		float f;
		memcpy(&f, &v, sizeof(f));
		return f;
	}

	/// a texture stage argument as the pixel shader takes it (which, complemented, alpha replicated),
	/// false for the ones it does not do (temp, constant)
	bool Stage_Argument(DWORD arg, float& which, float& complement, float& replicate)
	{
		const DWORD select = arg & D3DTA_SELECTMASK;
		if (select > D3DTA_SPECULAR)
			return false;
		which = (float)select;	// diffuse 0, current 1, texture 2, texture factor 3, specular 4
		complement = (arg & D3DTA_COMPLEMENT) ? 1.0f : 0.0f;
		replicate = (arg & D3DTA_ALPHAREPLICATE) ? 1.0f : 0.0f;
		return true;
	}

	bool Supported_Operation(DWORD op)
	{
		return op >= D3DTOP_DISABLE && op <= D3DTOP_DOTPRODUCT3 && op != D3DTOP_PREMODULATE
			&& op != D3DTOP_BUMPENVMAP && op != D3DTOP_BUMPENVMAPLUMINANCE;
	}
}

void D3D8On9_Set_Fixed_Function_Shaders(bool enabled)
{
	s_ffShaders = enabled;
}

void D3D8On9_Get_Fixed_Function_Counts(unsigned& emulated, unsigned& left, unsigned& reasonMask)
{
	emulated = s_ffEmulated;
	left = s_ffLeft;
	reasonMask = s_ffReasons;
}

namespace
{
	// Tiberium Project @performance the render states and texture stage states the fixed-function
	// shaders read: the adapter keeps them as the renderer sets them, so a draw reads nothing back
	const D3DRENDERSTATETYPE FF_RENDER_STATES[] =
	{
		D3DRS_FOGENABLE, D3DRS_SPECULARENABLE, D3DRS_TEXTUREFACTOR, D3DRS_LIGHTING, D3DRS_AMBIENT,
		D3DRS_COLORVERTEX, D3DRS_LOCALVIEWER, D3DRS_NORMALIZENORMALS, D3DRS_DIFFUSEMATERIALSOURCE,
		D3DRS_SPECULARMATERIALSOURCE, D3DRS_AMBIENTMATERIALSOURCE, D3DRS_EMISSIVEMATERIALSOURCE,
		D3DRS_VERTEXBLEND, D3DRS_INDEXEDVERTEXBLENDENABLE, D3DRS_POINTSIZE, D3DRS_POINTSIZE_MIN,
		D3DRS_POINTSIZE_MAX, D3DRS_POINTSCALEENABLE, D3DRS_POINTSCALE_A, D3DRS_POINTSCALE_B, D3DRS_POINTSCALE_C
	};
	const D3DTEXTURESTAGESTATETYPE FF_STAGE_STATES[] =
	{
		D3DTSS_COLOROP, D3DTSS_COLORARG1, D3DTSS_COLORARG2, D3DTSS_ALPHAOP, D3DTSS_ALPHAARG1, D3DTSS_ALPHAARG2,
		D3DTSS_TEXCOORDINDEX, D3DTSS_TEXTURETRANSFORMFLAGS
	};

	bool Is_Fixed_Function_Render_State(D3DRENDERSTATETYPE state)
	{
		for (size_t i = 0; i < sizeof(FF_RENDER_STATES) / sizeof(FF_RENDER_STATES[0]); ++i)
			if (FF_RENDER_STATES[i] == state)
				return true;
		return false;
	}

	bool Is_Fixed_Function_Stage_State(D3DTEXTURESTAGESTATETYPE type)
	{
		for (size_t i = 0; i < sizeof(FF_STAGE_STATES) / sizeof(FF_STAGE_STATES[0]); ++i)
			if (FF_STAGE_STATES[i] == type)
				return true;
		return false;
	}

	bool Same_Matrix(const D3DMATRIX& a, const D3DMATRIX& b)
	{
		return memcmp(&a, &b, sizeof(D3DMATRIX)) == 0;
	}
}

void D3D8On9Device::Read_Fixed_Function_State()
{
	for (size_t i = 0; i < sizeof(FF_RENDER_STATES) / sizeof(FF_RENDER_STATES[0]); ++i)
	{
		DWORD value = 0;
		m_device->GetRenderState(FF_RENDER_STATES[i], &value);
		m_ff.renderStates[FF_RENDER_STATES[i]] = value;
	}
	for (DWORD stage = 0; stage < 3; ++stage)
		for (size_t i = 0; i < sizeof(FF_STAGE_STATES) / sizeof(FF_STAGE_STATES[0]); ++i)
		{
			DWORD value = 0;
			m_device->GetTextureStageState(stage, FF_STAGE_STATES[i], &value);
			m_ff.stages[stage][FF_STAGE_STATES[i]] = value;
		}
	for (DWORD stage = 0; stage < 2; ++stage)
	{
		IDirect3DBaseTexture9* texture = nullptr;
		m_device->GetTexture(stage, &texture);
		m_ff.textures[stage] = texture != nullptr;
		if (texture != nullptr)
			texture->Release();
	}
	m_device->GetTransform(D3DTS_WORLD, &m_ff.world);
	m_device->GetTransform(D3DTS_VIEW, &m_ff.view);
	m_device->GetTransform(D3DTS_PROJECTION, &m_ff.projection);
	m_device->GetTransform(D3DTS_TEXTURE0, &m_ff.texture[0]);
	m_device->GetTransform(D3DTS_TEXTURE1, &m_ff.texture[1]);
	for (DWORD index = 0; index < 8; ++index)
	{
		// all eight, enabled or not: the renderer may switch one on later without setting it again
		if (FAILED(m_device->GetLight(index, &m_ff.lights[index])))
			memset(&m_ff.lights[index], 0, sizeof(D3DLIGHT9));
		m_ff.lightOn[index] = FALSE;
		m_device->GetLightEnable(index, &m_ff.lightOn[index]);
	}
	memset(&m_ff.material, 0, sizeof(m_ff.material));
	m_device->GetMaterial(&m_ff.material);
	D3DVIEWPORT9 viewport;
	memset(&viewport, 0, sizeof(viewport));
	m_device->GetViewport(&viewport);
	m_ff.viewport = viewport;
}

void D3D8On9Device::Build_Fixed_Function(DWORD fvf)
{
	m_ff.reason = 0;
	m_ff.pixelShader = nullptr;
	m_ff.lightRows = 0;
	const DWORD* rs = m_ff.renderStates;
	unsigned reason = 0;
	// Tiberium Project @feature pre-transformed vertices through the screen vertex shader; blended
	// ones stay fixed-function, as do pre-transformed ones while a declaration stands in for FVFs
	// Tiberium Project @feature the pixel stage only, after a stand-in vertex shader: the vertex side's
	// state (position, point size, blending, lights, coordinate generation) is the shader's
	const bool programmable = fvf == FF_PROGRAMMABLE;
	const DWORD position = programmable ? (DWORD)D3DFVF_XYZ : (fvf & D3DFVF_POSITION_MASK);
	const bool transformed = position == D3DFVF_XYZRHW;
	m_ff.transformed = transformed;
	if (position != D3DFVF_XYZ && (!transformed || m_vertexDeclarationOverride != nullptr))
		reason |= FF_POSITION;
	if (!programmable && (fvf & D3DFVF_PSIZE))
		reason |= FF_POINT_SIZE;
	if (rs[D3DRS_FOGENABLE])
		reason |= FF_FOG;
	if (!programmable && (rs[D3DRS_VERTEXBLEND] != D3DVBF_DISABLE || rs[D3DRS_INDEXEDVERTEXBLENDENABLE]))
		reason |= FF_BLEND;

	// the texture stages: two at most, the operations and arguments the pixel shader does
	float stageConstants[8][4];
	memset(stageConstants, 0, sizeof(stageConstants));
	float texGen[2][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
	float hasTexture[4] = { 0, 0, 0, 0 };
	bool stage1On = false;
	for (DWORD stage = 0; stage < 2 && reason == 0; ++stage)
	{
		const DWORD* ts = m_ff.stages[stage];
		const DWORD colorOp = ts[D3DTSS_COLOROP];
		if (stage == 1)
		{
			// a disabled first stage ends the cascade: the second is not looked at
			stage1On = colorOp != D3DTOP_DISABLE && stageConstants[0][0] != (float)D3DTOP_DISABLE;
			if (!stage1On)
				break;
		}
		const DWORD alphaOp = ts[D3DTSS_ALPHAOP];
		if (!Supported_Operation(colorOp) || !Supported_Operation(alphaOp))
		{
			reason |= FF_OPERATION;
			break;
		}
		float* c = stageConstants[stage * 4 + 0];
		float* cm = stageConstants[stage * 4 + 1];
		float* a = stageConstants[stage * 4 + 2];
		float* am = stageConstants[stage * 4 + 3];
		c[0] = (float)colorOp;
		a[0] = (float)alphaOp;
		if (!Stage_Argument(ts[D3DTSS_COLORARG1], c[1], cm[0], cm[1]) || !Stage_Argument(ts[D3DTSS_COLORARG2], c[2], cm[2], cm[3])
			|| !Stage_Argument(ts[D3DTSS_ALPHAARG1], a[1], am[0], am[1]) || !Stage_Argument(ts[D3DTSS_ALPHAARG2], a[2], am[2], am[3]))
		{
			reason |= FF_OPERATION;
			break;
		}
		// the texture coordinates: a stored set, or generated in camera space; the texture transform
		const DWORD tci = ts[D3DTSS_TEXCOORDINDEX];
		const DWORD ttf = ts[D3DTSS_TEXTURETRANSFORMFLAGS];
		const DWORD generation = tci & 0xFFFF0000;
		const DWORD set = tci & 0xFFFF;
		if (programmable && ((tci & 0xFFFF) != stage || ttf != D3DTTFF_DISABLE))
		{
			reason |= FF_TEXGEN;	// after a vertex shader: each stage its own output, as Direct3D asks
			break;
		}
		texGen[stage][0] = (float)set;
		if (generation == D3DTSS_TCI_PASSTHRU)
			texGen[stage][1] = 0.0f;
		else if (generation == D3DTSS_TCI_CAMERASPACENORMAL)
			texGen[stage][1] = 1.0f;
		else if (generation == D3DTSS_TCI_CAMERASPACEPOSITION)
			texGen[stage][1] = 2.0f;
		else if (generation == D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR)
			texGen[stage][1] = 3.0f;
		else
		{
			reason |= FF_TEXGEN;
			break;
		}
		if (set > 3)
		{
			reason |= FF_TEXGEN;
			break;
		}
		if (transformed && (generation != D3DTSS_TCI_PASSTHRU || ttf != D3DTTFF_DISABLE))
		{
			reason |= FF_TEXGEN;	// screen vertices: the coordinates as stored only
			break;
		}
		texGen[stage][2] = (float)(ttf & 0xFF);
		texGen[stage][3] = (ttf & D3DTTFF_PROJECTED) ? 1.0f : 0.0f;
		hasTexture[stage] = m_ff.textures[stage] ? 1.0f : 0.0f;
	}
	if (stage1On && reason == 0 && m_ff.stages[2][D3DTSS_COLOROP] != D3DTOP_DISABLE)
		reason |= FF_STAGES;

	// the lights (Direct3D does not light pre-transformed vertices)
	const DWORD lighting = (transformed || programmable) ? 0 : rs[D3DRS_LIGHTING];
	int lightIndex[4];
	int lightCount = 0;
	if (lighting && reason == 0)
	{
		for (int index = 0; index < 8; ++index)
		{
			if (!m_ff.lightOn[index])
				continue;
			if (lightCount == 4)
			{
				reason |= FF_LIGHTS;
				break;
			}
			lightIndex[lightCount++] = index;
		}
	}
	if (reason != 0)
	{
		m_ff.reason = reason;
		return;
	}

	// vertex shader constants
	const D3DMATRIX& view = m_ff.view;
	D3DMATRIX worldView, worldViewProjection;
	Multiply_Matrix(m_ff.world, view, worldView);
	Multiply_Matrix(worldView, m_ff.projection, worldViewProjection);
	float (*vsc)[4] = m_ff.vs;
	memset(m_ff.vs, 0, sizeof(m_ff.vs));
	Transposed_Matrix(worldViewProjection, &vsc[0][0]);
	Transposed_Matrix(worldView, &vsc[4][0]);
	const DWORD specularOn = rs[D3DRS_SPECULARENABLE];
	vsc[8][0] = lighting ? 1.0f : 0.0f;
	vsc[8][1] = (fvf & D3DFVF_NORMAL) ? 1.0f : 0.0f;
	vsc[8][2] = (fvf & D3DFVF_DIFFUSE) ? 1.0f : 0.0f;
	vsc[8][3] = (fvf & D3DFVF_SPECULAR) ? 1.0f : 0.0f;
	vsc[9][0] = specularOn ? 1.0f : 0.0f;
	vsc[9][1] = rs[D3DRS_LOCALVIEWER] ? 1.0f : 0.0f;
	vsc[9][2] = rs[D3DRS_COLORVERTEX] ? 1.0f : 0.0f;
	vsc[9][3] = rs[D3DRS_NORMALIZENORMALS] ? 1.0f : 0.0f;
	vsc[10][0] = (float)rs[D3DRS_DIFFUSEMATERIALSOURCE];
	vsc[10][1] = (float)rs[D3DRS_AMBIENTMATERIALSOURCE];
	vsc[10][2] = (float)rs[D3DRS_SPECULARMATERIALSOURCE];
	vsc[10][3] = (float)rs[D3DRS_EMISSIVEMATERIALSOURCE];
	const D3DMATERIAL9& material = m_ff.material;
	Value_To_Float(material.Diffuse, vsc[11]);
	Value_To_Float(material.Ambient, vsc[12]);
	Value_To_Float(material.Specular, vsc[13]);
	Value_To_Float(material.Emissive, vsc[14]);
	vsc[15][0] = material.Power;
	Color_To_Float(rs[D3DRS_AMBIENT], vsc[16]);
	vsc[17][0] = (float)lightCount;
	for (int i = 0; i < lightCount; ++i)
	{
		const D3DLIGHT9& l = m_ff.lights[lightIndex[i]];
		float* r = vsc[18 + i * 7];
		Value_To_Float(l.Diffuse, r);
		Value_To_Float(l.Specular, r + 4);
		Value_To_Float(l.Ambient, r + 8);
		// position and direction into camera space
		const float px = l.Position.x, py = l.Position.y, pz = l.Position.z;
		r[12] = px * view._11 + py * view._21 + pz * view._31 + view._41;
		r[13] = px * view._12 + py * view._22 + pz * view._32 + view._42;
		r[14] = px * view._13 + py * view._23 + pz * view._33 + view._43;
		r[15] = l.Type == D3DLIGHT_POINT ? 1.0f : (l.Type == D3DLIGHT_SPOT ? 2.0f : 3.0f);
		float dx = l.Direction.x * view._11 + l.Direction.y * view._21 + l.Direction.z * view._31;
		float dy = l.Direction.x * view._12 + l.Direction.y * view._22 + l.Direction.z * view._32;
		float dz = l.Direction.x * view._13 + l.Direction.y * view._23 + l.Direction.z * view._33;
		const float length = sqrtf(dx * dx + dy * dy + dz * dz);
		if (length > 0.0f) { dx /= length; dy /= length; dz /= length; }
		r[16] = dx; r[17] = dy; r[18] = dz;
		r[19] = l.Range > 0.0f ? l.Range : 1e30f;
		r[20] = l.Attenuation0; r[21] = l.Attenuation1; r[22] = l.Attenuation2; r[23] = l.Falloff;
		r[24] = cosf(l.Theta * 0.5f); r[25] = cosf(l.Phi * 0.5f);
	}
	m_ff.lightRows = lighting ? 7 * lightCount : 0;	// the lights only when lit
	memcpy(vsc[46], texGen, sizeof(texGen));
	Transposed_Matrix(m_ff.texture[0], &vsc[48][0]);
	Transposed_Matrix(m_ff.texture[1], &vsc[52][0]);
	vsc[56][0] = DWORD_Float(rs[D3DRS_POINTSIZE]);
	vsc[56][1] = rs[D3DRS_POINTSCALEENABLE] ? 1.0f : 0.0f;
	vsc[56][2] = DWORD_Float(rs[D3DRS_POINTSIZE_MIN]);
	vsc[56][3] = DWORD_Float(rs[D3DRS_POINTSIZE_MAX]) > 0.0f ? DWORD_Float(rs[D3DRS_POINTSIZE_MAX]) : 256.0f;
	vsc[57][0] = DWORD_Float(rs[D3DRS_POINTSCALE_A]);
	vsc[57][1] = DWORD_Float(rs[D3DRS_POINTSCALE_B]);
	vsc[57][2] = DWORD_Float(rs[D3DRS_POINTSCALE_C]);
	vsc[57][3] = (float)m_ff.viewport.Height;
	// the screen vertex shader's viewport: its corner, and 2 over its size
	const D3DVIEWPORT9& viewport = m_ff.viewport;
	vsc[58][0] = (float)viewport.X;
	vsc[58][1] = (float)viewport.Y;
	vsc[58][2] = viewport.Width > 0 ? 2.0f / (float)viewport.Width : 0.0f;
	vsc[58][3] = viewport.Height > 0 ? 2.0f / (float)viewport.Height : 0.0f;

	// pixel shader constants
	float (*psc)[4] = m_ff.ps;
	memset(m_ff.ps, 0, sizeof(m_ff.ps));
	memcpy(psc[0], stageConstants, sizeof(stageConstants));
	Color_To_Float(rs[D3DRS_TEXTUREFACTOR], psc[8]);
	psc[9][0] = specularOn ? 1.0f : 0.0f;
	psc[9][1] = stage1On ? 1.0f : 0.0f;
	psc[9][2] = texGen[0][3];
	psc[9][3] = texGen[1][3];
	psc[10][0] = hasTexture[0];
	psc[10][1] = hasTexture[1];

	// one of the small pixel shaders when the stages are one of the common set-ups (one stage,
	// colour and alpha alike, plain arguments, no specular, no projection), else the general one
	IDirect3DPixelShader9* pixelShader = m_ffPixelShader;
	const float* c = stageConstants[0];
	const float* cm = stageConstants[1];
	const float* a = stageConstants[2];
	const float* am = stageConstants[3];
	const bool plain = !stage1On && !specularOn && texGen[0][3] == 0.0f
		&& cm[0] == 0.0f && cm[1] == 0.0f && cm[2] == 0.0f && cm[3] == 0.0f
		&& am[0] == 0.0f && am[1] == 0.0f && am[2] == 0.0f && am[3] == 0.0f;
	if (plain)
	{
		// what a channel's operation picks: 1 the diffuse colour, 2 the texture, 3 the two multiplied
		struct Local
		{
			static int Pick(const float* op)
			{
				const int o = (int)op[0], a1 = (int)op[1], a2 = (int)op[2];
				const int arg = o == D3DTOP_SELECTARG1 ? a1 : (o == D3DTOP_SELECTARG2 ? a2 : -1);
				if (arg == 0) return 1;
				if (arg == 2) return 2;
				if (o == D3DTOP_MODULATE && ((a1 == 2 && a2 == 0) || (a1 == 0 && a2 == 2))) return 3;
				return 0;
			}
		};
		const int colorPick = Local::Pick(c), alphaPick = Local::Pick(a);
		if (colorPick == alphaPick)
		{
			if (colorPick == 1)
				pixelShader = m_ffDiffuseShader;
			else if (colorPick == 2 && hasTexture[0] > 0.5f)
				pixelShader = m_ffTextureShader;
			else if (colorPick == 3 && hasTexture[0] > 0.5f)
				pixelShader = m_ffModulateShader;
		}
	}
	m_ff.pixelShader = pixelShader;
}

void D3D8On9Device::Unbind_Bound_Fixed_Function_Shaders()
{
	if (m_ff.substituteBound)
	{
		// Tiberium Project @feature the game's own vertex shader back in place of its stand-in
		const size_t index = m_vertexShader >> 1;
		if (m_vertexShader != ~0u && (m_vertexShader & SHADER_HANDLE_BIT) != 0 && index < m_vertexShaders.size())
			m_device->SetVertexShader(m_vertexShaders[index].shader);
		m_ff.substituteBound = false;
	}
	if (m_ff.vsBound)
		m_device->SetVertexShader(nullptr);
	if (m_ff.psBound != nullptr)
		m_device->SetPixelShader(nullptr);
	m_ff.vsBound = false;
	m_ff.psBound = nullptr;
}

bool D3D8On9Device::Emulate_Fixed_Function()
{
	if (!s_ffShaders)
		return false;
	const bool drawn = Emulate_Fixed_Function_Draw();
	if (m_ff.declarationFvf != 0 && !(drawn && m_ff.transformed))
		Restore_FVF();	// Tiberium Project @feature the screen declaration only for screen vertices
	return drawn;
}

bool D3D8On9Device::Emulate_Fixed_Function_Draw()
{
	if (m_vertexShaderOverride != nullptr)
	{
		Unbind_Fixed_Function_Shaders();	// the override draws in place of the vertex shader; no pixel shader of ours
		return false;
	}
	DWORD fvf = Get_Current_FVF();
	if (fvf == 0 && m_vertexShader == ~0u)
	{
		// forgotten after a SAGE 2 effect (Invalidate_State) while still set on the device, or in place
		// of the screen declaration still there
		if (m_ff.declarationFvf != 0)
			fvf = m_ff.declarationFvf;
		else
			m_device->GetFVF(&fvf);
	}
	if (!m_ffTried)
	{
		m_ffTried = true;
		if (FAILED(m_device->CreateVertexShader(FixedFunctionVertexShader, &m_ffVertexShader))
			|| FAILED(m_device->CreateVertexShader(FixedFunctionScreenVertexShader, &m_ffScreenShader))
			|| FAILED(m_device->CreatePixelShader(FixedFunctionPixelShader, &m_ffPixelShader))
			|| FAILED(m_device->CreatePixelShader(FixedFunctionModulatePixelShader, &m_ffModulateShader))
			|| FAILED(m_device->CreatePixelShader(FixedFunctionDiffusePixelShader, &m_ffDiffuseShader))
			|| FAILED(m_device->CreatePixelShader(FixedFunctionTexturePixelShader, &m_ffTextureShader)))
			Release_Fixed_Function_Shaders();
	}
	if (fvf == 0)
	{
		// a Direct3D 8 vertex shader of the game's own (Tiberium Project @feature its stand-in, when it has
		// one), or a declaration of someone else's
		if (m_vertexShader != ~0u && (m_vertexShader & SHADER_HANDLE_BIT) != 0)
			return Emulate_Pixel_Stage();
		Unbind_Fixed_Function_Shaders();
		return false;
	}
	if (m_ffVertexShader == nullptr)
	{
		++s_ffLeft;
		s_ffReasons |= FF_SHADERS;
		return false;
	}

	// what the device has bound: ours (left from the draw before), nothing, or someone else's
	IDirect3DVertexShader9* vs = nullptr;
	m_device->GetVertexShader(&vs);
	if (vs != nullptr)
		vs->Release();
	if (vs != nullptr && vs != m_ffVertexShader && vs != m_ffScreenShader)
	{
		m_ff.vsBound = false;
		Unbind_Fixed_Function_Shaders();
		return false;	// someone else's shaders (the SAGE 2 effects, the tree buffer)
	}
	m_ff.vsBound = vs != nullptr;
	m_ff.vsBoundTransformed = vs != nullptr && vs == m_ffScreenShader;
	IDirect3DPixelShader9* ps = nullptr;
	m_device->GetPixelShader(&ps);
	if (ps != nullptr)
		ps->Release();
	if (ps != nullptr && ps != m_ffPixelShader && ps != m_ffModulateShader && ps != m_ffDiffuseShader && ps != m_ffTextureShader)
	{
		m_ff.psBound = nullptr;
		Unbind_Fixed_Function_Shaders();
		++s_ffLeft;
		s_ffReasons |= FF_PIXEL_SHADER;
		return false;	// the renderer's own pixel shader
	}
	m_ff.psBound = ps;

	// what the state makes, again only when it changed
	if (!m_ff.stateKnown)
	{
		Read_Fixed_Function_State();
		m_ff.stateKnown = true;
		m_ff.changed = true;
	}
	if (m_ff.changed || fvf != m_ff.fvf)
	{
		Build_Fixed_Function(fvf);
		m_ff.changed = false;
		m_ff.fvf = fvf;
	}
	if (m_ff.reason != 0)
	{
		++s_ffLeft;
		s_ffReasons |= m_ff.reason;
		Unbind_Fixed_Function_Shaders();
		return false;
	}

	// pre-transformed vertices: their FVF as a declaration with a plain position
	if (m_ff.transformed && m_ff.declarationFvf != fvf)
	{
		IDirect3DVertexDeclaration9* declaration = Screen_Declaration(fvf);
		if (declaration == nullptr)
		{
			++s_ffLeft;
			s_ffReasons |= FF_POSITION;
			Unbind_Fixed_Function_Shaders();
			return false;
		}
		m_device->SetVertexDeclaration(declaration);
		m_ff.declarationFvf = fvf;
	}

	// the shaders, when not bound already, and the constants that differ from what the device has
	if (!m_ff.vsBound || m_ff.vsBoundTransformed != m_ff.transformed)
	{
		m_device->SetVertexShader(m_ff.transformed ? m_ffScreenShader : m_ffVertexShader);
		m_ff.vsBound = true;
		m_ff.vsBoundTransformed = m_ff.transformed;
	}
	if (m_ff.psBound != m_ff.pixelShader)
	{
		m_device->SetPixelShader(m_ff.pixelShader);
		m_ff.psBound = m_ff.pixelShader;
	}
	const size_t ROW = sizeof(float) * 4;
	const bool vsKnown = m_ff.vsConstantsKnown;
	if (!vsKnown || memcmp(m_ff.vsUploaded[0], m_ff.vs[0], 18 * ROW) != 0)
	{
		m_device->SetVertexShaderConstantF(0, m_ff.vs[0], 18);
		memcpy(m_ff.vsUploaded[0], m_ff.vs[0], 18 * ROW);
	}
	const int lightRows = m_ff.lightRows;
	if (lightRows > 0)
	{
		const int known = vsKnown ? m_ff.lightRowsUploaded : 0;
		if (lightRows > known || memcmp(m_ff.vsUploaded[18], m_ff.vs[18], lightRows * ROW) != 0)
		{
			m_device->SetVertexShaderConstantF(18, m_ff.vs[18], lightRows);
			memcpy(m_ff.vsUploaded[18], m_ff.vs[18], lightRows * ROW);
			m_ff.lightRowsUploaded = lightRows > known ? lightRows : known;
		}
	}
	else if (!vsKnown)
		m_ff.lightRowsUploaded = 0;
	if (!vsKnown || memcmp(m_ff.vsUploaded[46], m_ff.vs[46], 13 * ROW) != 0)
	{
		m_device->SetVertexShaderConstantF(46, m_ff.vs[46], 13);
		memcpy(m_ff.vsUploaded[46], m_ff.vs[46], 13 * ROW);
	}
	m_ff.vsConstantsKnown = true;
	Upload_Pixel_Constants();
	++s_ffEmulated;
	return true;
}

void D3D8On9Device::Upload_Pixel_Constants()
{
	if (m_ff.pixelShader == m_ffPixelShader
		&& (!m_ff.psConstantsKnown || memcmp(m_ff.psUploaded, m_ff.ps, sizeof(m_ff.ps)) != 0))
	{
		m_device->SetPixelShaderConstantF(0, m_ff.ps[0], 11);
		memcpy(m_ff.psUploaded, m_ff.ps, sizeof(m_ff.ps));
		m_ff.psConstantsKnown = true;
	}
}

// Tiberium Project @feature the game's own vertex shader (the trees') through its vs_3_0 stand-in, so the
// texture stages after it go through the emulation's pixel shader; its constants are the game's, as
// it set them
bool D3D8On9Device::Emulate_Pixel_Stage()
{
	const size_t index = m_vertexShader >> 1;
	if (index >= m_vertexShaders.size() || m_vertexShaders[index].substitute == nullptr || m_ffPixelShader == nullptr)
	{
		Unbind_Fixed_Function_Shaders();
		return false;
	}
	IDirect3DPixelShader9* ps = nullptr;
	m_device->GetPixelShader(&ps);
	if (ps != nullptr)
		ps->Release();
	if (ps != nullptr && ps != m_ffPixelShader && ps != m_ffModulateShader && ps != m_ffDiffuseShader && ps != m_ffTextureShader)
	{
		m_ff.psBound = nullptr;
		Unbind_Fixed_Function_Shaders();
		return false;	// a pixel shader of the renderer's own: the game's shader goes with it
	}
	m_ff.psBound = ps;
	if (!m_ff.stateKnown)
	{
		Read_Fixed_Function_State();
		m_ff.stateKnown = true;
		m_ff.changed = true;
	}
	if (m_ff.changed || m_ff.fvf != FF_PROGRAMMABLE)
	{
		Build_Fixed_Function(FF_PROGRAMMABLE);
		m_ff.changed = false;
		m_ff.fvf = FF_PROGRAMMABLE;
	}
	if (m_ff.reason != 0)
	{
		++s_ffLeft;
		s_ffReasons |= m_ff.reason;
		Unbind_Fixed_Function_Shaders();	// the game's own shader back, if the stand-in was bound
		return false;
	}
	if (!m_ff.substituteBound)
	{
		m_device->SetVertexShader(m_vertexShaders[index].substitute);
		m_ff.substituteBound = true;
		m_ff.vsBound = false;
	}
	if (m_ff.psBound != m_ff.pixelShader)
	{
		m_device->SetPixelShader(m_ff.pixelShader);
		m_ff.psBound = m_ff.pixelShader;
	}
	Upload_Pixel_Constants();
	++s_ffEmulated;
	return true;
}

HRESULT D3D8On9Device::Set_Vertex_Shader_Substitute(DWORD handle, Substitute which)
{
	const size_t index = handle >> 1;
	if ((handle & SHADER_HANDLE_BIT) == 0 || index >= m_vertexShaders.size() || m_vertexShaders[index].shader == nullptr)
		return D3DERR_INVALIDCALL;
	const DWORD* function = nullptr;
	switch (which)
	{
		case SUBSTITUTE_TREES: function = FixedFunctionTreesVertexShader; break;
	}
	if (function == nullptr)
		return D3DERR_INVALIDCALL;
	IDirect3DVertexShader9* substitute = nullptr;
	HRESULT hr = m_device->CreateVertexShader(function, &substitute);
	if (FAILED(hr))
		return hr;
	VertexShader& entry = m_vertexShaders[index];
	if (m_ff.substituteBound && m_vertexShader == handle)
		Unbind_Fixed_Function_Shaders();
	if (entry.substitute != nullptr)
		entry.substitute->Release();
	entry.substitute = substitute;
	return D3D_OK;
}

// Tiberium Project @performance the state the fixed-function shaders read, kept as it is set
HRESULT D3D8On9Device::SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m)
{
	D3DMATRIX* kept = nullptr;
	switch ((int)state)
	{
		case D3DTS_WORLD: kept = &m_ff.world; break;
		case D3DTS_VIEW: kept = &m_ff.view; break;
		case D3DTS_PROJECTION: kept = &m_ff.projection; break;
		case D3DTS_TEXTURE0: kept = &m_ff.texture[0]; break;
		case D3DTS_TEXTURE1: kept = &m_ff.texture[1]; break;
		default: break;
	}
	if (kept != nullptr && m != nullptr && !Same_Matrix(*kept, *m))
	{
		*kept = *m;
		m_ff.changed = true;
	}
	return m_device->SetTransform(state, m);
}

HRESULT D3D8On9Device::SetViewport(const D3DVIEWPORT9* vp)
{
	if (vp != nullptr && (vp->X != m_ff.viewport.X || vp->Y != m_ff.viewport.Y || vp->Width != m_ff.viewport.Width
		|| vp->Height != m_ff.viewport.Height))
	{
		m_ff.viewport = *vp;
		m_ff.changed = true;
	}
	return m_device->SetViewport(vp);
}

HRESULT D3D8On9Device::SetMaterial(const D3DMATERIAL9* m)
{
	if (m != nullptr && memcmp(&m_ff.material, m, sizeof(D3DMATERIAL9)) != 0)
	{
		m_ff.material = *m;
		m_ff.changed = true;
	}
	return m_device->SetMaterial(m);
}

HRESULT D3D8On9Device::SetLight(DWORD index, const D3DLIGHT9* light)
{
	if (index < 8 && light != nullptr && memcmp(&m_ff.lights[index], light, sizeof(D3DLIGHT9)) != 0)
	{
		m_ff.lights[index] = *light;
		m_ff.changed = true;
	}
	return m_device->SetLight(index, light);
}

HRESULT D3D8On9Device::LightEnable(DWORD index, BOOL enable)
{
	if (index < 8 && (m_ff.lightOn[index] != FALSE) != (enable != FALSE))
	{
		m_ff.lightOn[index] = enable;
		m_ff.changed = true;
	}
	return m_device->LightEnable(index, enable);
}

HRESULT D3D8On9Device::SetTexture(DWORD stage, IDirect3DBaseTexture9* texture)
{
	if (stage < 2 && m_ff.textures[stage] != (texture != nullptr))
	{
		m_ff.textures[stage] = texture != nullptr;
		m_ff.changed = true;
	}
	return m_device->SetTexture(stage, texture);
}

// Tiberium Project @feature a pre-transformed FVF as a declaration the vertex shader runs for: its
// position as a plain four-float POSITION (POSITIONT, what SetFVF makes of it, skips the vertex shader)
IDirect3DVertexDeclaration9* D3D8On9Device::Screen_Declaration(DWORD fvf)
{
	for (size_t i = 0; i < m_ffScreenDeclarations.size(); ++i)
		if (m_ffScreenDeclarations[i].first == fvf)
			return m_ffScreenDeclarations[i].second;
	D3DVERTEXELEMENT9 elements[16];
	int count = 0;
	WORD offset = 0;
	struct Local
	{
		static void Add(D3DVERTEXELEMENT9* e, int& n, WORD& offset, BYTE type, BYTE usage, BYTE index, WORD size)
		{
			D3DVERTEXELEMENT9& element = e[n++];
			element.Stream = 0;
			element.Offset = offset;
			element.Type = type;
			element.Method = D3DDECLMETHOD_DEFAULT;
			element.Usage = usage;
			element.UsageIndex = index;
			offset = (WORD)(offset + size);
		}
	};
	Local::Add(elements, count, offset, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITION, 0, 16);
	if (fvf & D3DFVF_PSIZE)
		Local::Add(elements, count, offset, D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_PSIZE, 0, 4);
	if (fvf & D3DFVF_DIFFUSE)
		Local::Add(elements, count, offset, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0, 4);
	if (fvf & D3DFVF_SPECULAR)
		Local::Add(elements, count, offset, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 1, 4);
	const DWORD sets = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	static const BYTE TYPES[4] = { D3DDECLTYPE_FLOAT2, D3DDECLTYPE_FLOAT3, D3DDECLTYPE_FLOAT4, D3DDECLTYPE_FLOAT1 };
	static const WORD SIZES[4] = { 8, 12, 16, 4 };
	for (DWORD set = 0; set < sets && set < 8; ++set)
	{
		const DWORD format = (fvf >> (set * 2 + 16)) & 3;	// D3DFVF_TEXCOORDSIZEn
		Local::Add(elements, count, offset, TYPES[format], D3DDECLUSAGE_TEXCOORD, (BYTE)set, SIZES[format]);
	}
	const D3DVERTEXELEMENT9 end = D3DDECL_END();
	elements[count] = end;
	IDirect3DVertexDeclaration9* declaration = nullptr;
	if (FAILED(m_device->CreateVertexDeclaration(elements, &declaration)))
		declaration = nullptr;
	m_ffScreenDeclarations.push_back(std::make_pair(fvf, declaration));	// a failure is remembered too
	return declaration;
}

void D3D8On9Device::Release_Fixed_Function_Shaders()
{
	if (m_ffVertexShader) { m_ffVertexShader->Release(); m_ffVertexShader = nullptr; }
	if (m_ffScreenShader) { m_ffScreenShader->Release(); m_ffScreenShader = nullptr; }
	Restore_FVF();
	for (size_t i = 0; i < m_ffScreenDeclarations.size(); ++i)
		if (m_ffScreenDeclarations[i].second)
			m_ffScreenDeclarations[i].second->Release();
	m_ffScreenDeclarations.clear();
	if (m_ffPixelShader) { m_ffPixelShader->Release(); m_ffPixelShader = nullptr; }
	if (m_ffModulateShader) { m_ffModulateShader->Release(); m_ffModulateShader = nullptr; }
	if (m_ffDiffuseShader) { m_ffDiffuseShader->Release(); m_ffDiffuseShader = nullptr; }
	if (m_ffTextureShader) { m_ffTextureShader->Release(); m_ffTextureShader = nullptr; }
}

void D3D8On9_Set_Draw_Phase(const char* phase)
{
	s_drawPhase = phase != nullptr ? phase : "unnamed";
}

void D3D8On9_Set_Draw_Stats(bool counting)
{
	if (counting && !s_drawStats)
		s_drawPhaseCount = 0;
	s_drawStats = counting;
}

int D3D8On9_Get_Draw_Stats(D3D8On9DrawStats* stats, int max)
{
	int n = s_drawPhaseCount < max ? s_drawPhaseCount : max;
	for (int i = 0; i < n; ++i)
		stats[i] = s_drawPhases[i];
	return n;
}

void D3D8On9Device::Count_Draw(UINT primitives)
{
	if (!s_drawStats)
		return;
	D3D8On9DrawStats* entry = nullptr;
	for (int i = 0; i < s_drawPhaseCount && entry == nullptr; ++i)
		if (s_drawPhases[i].phase == s_drawPhase || strcmp(s_drawPhases[i].phase, s_drawPhase) == 0)
			entry = &s_drawPhases[i];
	if (entry == nullptr)
	{
		if (s_drawPhaseCount >= MAX_DRAW_PHASES)
			return;
		entry = &s_drawPhases[s_drawPhaseCount++];
		entry->phase = s_drawPhase;
		entry->draws = entry->fixedVertex = entry->fixedPixel = entry->primitives = 0;
		entry->fixedLit = entry->fixedFog = entry->fixedTwoStages = 0;
		for (int k = 0; k < 4; ++k)
			entry->fixedFVF[k] = entry->fixedFVFDraws[k] = 0;
	}
	++entry->draws;
	entry->primitives += primitives;
	IDirect3DVertexShader9* vs = nullptr;
	IDirect3DPixelShader9* ps = nullptr;
	m_device->GetVertexShader(&vs);
	m_device->GetPixelShader(&ps);
	if (vs == nullptr)
		++entry->fixedVertex;
	else
		vs->Release();
	if (ps == nullptr)
		++entry->fixedPixel;
	else
		ps->Release();
	if (vs == nullptr || ps == nullptr)
	{
		const DWORD fvf = Get_Current_FVF();
		for (int k = 0; k < 4; ++k)
		{
			if (entry->fixedFVFDraws[k] == 0 || entry->fixedFVF[k] == fvf)
			{
				entry->fixedFVF[k] = fvf;
				++entry->fixedFVFDraws[k];
				break;
			}
		}
		DWORD lit = 0, fog = 0, stage1 = D3DTOP_DISABLE;
		m_device->GetRenderState(D3DRS_LIGHTING, &lit);
		m_device->GetRenderState(D3DRS_FOGENABLE, &fog);
		m_device->GetTextureStageState(1, D3DTSS_COLOROP, &stage1);
		if (lit) ++entry->fixedLit;
		if (fog) ++entry->fixedFog;
		if (stage1 != D3DTOP_DISABLE) ++entry->fixedTwoStages;
	}
}

void D3D8On9_Set_Readable_Depth(bool enabled)
{
	s_wantReadableDepth = enabled;
}

void D3D8On9Device::Make_Readable_Depth(const D3DPRESENT_PARAMETERS& params)
{
	Release_Readable_Depth();
	if (!s_wantReadableDepth || !params.EnableAutoDepthStencil || params.MultiSampleType != D3DMULTISAMPLE_NONE)
		return;
	const D3DFORMAT FORMAT_INTZ = (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z');
	IDirect3D9* d3d = nullptr;
	D3DDEVICE_CREATION_PARAMETERS creation;
	D3DDISPLAYMODE mode;
	bool supported = false;
	if (SUCCEEDED(m_device->GetDirect3D(&d3d)) && d3d != nullptr)
	{
		if (SUCCEEDED(m_device->GetCreationParameters(&creation)) && SUCCEEDED(m_device->GetDisplayMode(0, &mode)))
			supported = SUCCEEDED(d3d->CheckDeviceFormat(creation.AdapterOrdinal, creation.DeviceType, mode.Format,
				D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, FORMAT_INTZ));
		d3d->Release();
	}
	if (!supported)
		return;
	IDirect3DSurface9* backBuffer = nullptr;
	if (FAILED(m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || backBuffer == nullptr)
		return;
	D3DSURFACE_DESC desc;
	backBuffer->GetDesc(&desc);
	backBuffer->Release();
	if (FAILED(m_device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_DEPTHSTENCIL, FORMAT_INTZ, D3DPOOL_DEFAULT, &m_readableDepth, nullptr)))
	{
		m_readableDepth = nullptr;
		return;
	}
	if (FAILED(m_readableDepth->GetSurfaceLevel(0, &m_readableDepthSurface)))
	{
		Release_Readable_Depth();
		return;
	}
	m_device->SetDepthStencilSurface(m_readableDepthSurface);
	m_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
}

void D3D8On9Device::Release_Readable_Depth()
{
	if (m_readableDepth == nullptr)
		return;
	// the automatic depth buffer back in its place, so the device holds nothing of this one
	IDirect3DSurface9* current = nullptr;
	if (SUCCEEDED(m_device->GetDepthStencilSurface(&current)) && current != nullptr)
	{
		if (current == m_readableDepthSurface)
			m_device->SetDepthStencilSurface(nullptr);
		current->Release();
	}
	if (m_readableDepthSurface != nullptr)
		m_readableDepthSurface->Release();
	m_readableDepthSurface = nullptr;
	m_readableDepth->Release();
	m_readableDepth = nullptr;
}

ULONG D3D8On9Device::Release()
{
	ULONG refs = --m_refs;
	if (refs == 0)
		delete this;
	return refs;
}

void D3D8On9Device::Invalidate_State()
{
	m_vertexShader = ~0u;
	m_ff.stateKnown = false;	// Tiberium Project @performance
}

void D3D8On9Device::Set_Vertex_Shader_Override(IDirect3DVertexShader9* shader)
{
	if (shader == m_vertexShaderOverride)
		return;
	m_vertexShaderOverride = shader;
	if (m_vertexShader != ~0u && (m_vertexShader & SHADER_HANDLE_BIT) == 0)
	{
		m_device->SetVertexShader(shader);
		m_ff.vsBound = false;	// Tiberium Project @performance in place of the fixed-function shader
	}
}

void D3D8On9Device::Set_Vertex_Declaration_Override(IDirect3DVertexDeclaration9* declaration)
{
	if (declaration == m_vertexDeclarationOverride)
		return;
	m_vertexDeclarationOverride = declaration;
	if (m_vertexShader != ~0u && (m_vertexShader & SHADER_HANDLE_BIT) == 0)
	{
		m_ff.declarationFvf = 0;	// Tiberium Project @feature in place of the screen declaration too
		if (declaration != nullptr)
			m_device->SetVertexDeclaration(declaration);
		else
			m_device->SetFVF(m_vertexShader);
	}
}

HRESULT D3D8On9Device::GetDeviceCaps(D3DCAPS8* caps)
{
	HRESULT hr = m_device->GetDeviceCaps(caps);
	if (SUCCEEDED(hr))
		D3D8On9_Convert_Caps(caps);
	return hr;
}

HRESULT D3D8On9Device::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* params, D3D8On9SwapChain** chain)
{
	IDirect3DSwapChain9* chain9 = nullptr;
	D3DPRESENT_PARAMETERS params9 = D3D8On9_Present_Parameters(*params);
	HRESULT hr = m_device->CreateAdditionalSwapChain(&params9, &chain9);
	returnPresentParameters(params9, params);
	*chain = SUCCEEDED(hr) ? new D3D8On9SwapChain(chain9) : nullptr;
	return hr;
}

HRESULT D3D8On9Device::Reset(D3DPRESENT_PARAMETERS* params)
{
	D3DPRESENT_PARAMETERS params9 = D3D8On9_Present_Parameters(*params);
	Release_Readable_Depth();	// a default-pool texture: Reset fails while it lives
	HRESULT hr = m_device->Reset(&params9);
	if (SUCCEEDED(hr))
		Make_Readable_Depth(params9);
	returnPresentParameters(params9, params);
	m_baseVertexIndex = 0;
	Invalidate_State();
	// Tiberium Project @performance a reset device has no shaders bound and its state back to the defaults
	m_ff.vsBound = false;
	m_ff.declarationFvf = 0;
	m_ff.substituteBound = false;
	m_ff.psBound = nullptr;
	m_ff.vsConstantsKnown = false;
	m_ff.psConstantsKnown = false;
	return hr;
}

//-------------------------------------------------------------------------------------------------
HRESULT D3D8On9Device::CreateImageSurface(UINT w, UINT h, D3DFORMAT format, IDirect3DSurface9** surface)
{
	HRESULT hr = m_device->CreateOffscreenPlainSurface(w, h, format, D3DPOOL_SYSTEMMEM, surface, nullptr);
	// Tiberium Project @bugfix a format the device does not offer: the scratch pool takes any (the
	// surface is only locked and copied from, which is all W3D does with an image surface)
	if (FAILED(hr))
		hr = m_device->CreateOffscreenPlainSurface(w, h, format, D3DPOOL_SCRATCH, surface, nullptr);
	return hr;
}

//-------------------------------------------------------------------------------------------------
HRESULT D3D8On9Device::CopyRects(IDirect3DSurface9* src, const RECT* rects, UINT count, IDirect3DSurface9* dst, const POINT* points)
{
	if (src == nullptr || dst == nullptr || src == dst)
		return D3DERR_INVALIDCALL;
	D3DSURFACE_DESC sd, dd;
	src->GetDesc(&sd);
	dst->GetDesc(&dd);
	RECT whole = { 0, 0, (LONG)sd.Width, (LONG)sd.Height };
	if (count == 0)
	{
		rects = &whole;
		count = 1;
	}
	for (UINT i = 0; i < count; ++i)
	{
		const RECT& r = rects[i];
		// without destination points, each rectangle keeps its position
		POINT p;
		p.x = points ? points[i].x : r.left;
		p.y = points ? points[i].y : r.top;
		RECT d = { p.x, p.y, p.x + (r.right - r.left), p.y + (r.bottom - r.top) };
		HRESULT hr;
		if (sd.Pool == D3DPOOL_DEFAULT && dd.Pool == D3DPOOL_DEFAULT)
		{
			hr = m_device->StretchRect(src, &r, dst, &d, D3DTEXF_NONE);
		}
		else if (sd.Pool == D3DPOOL_SYSTEMMEM && dd.Pool == D3DPOOL_DEFAULT)
		{
			hr = m_device->UpdateSurface(src, &r, dst, &p);
		}
		else if (sd.Pool == D3DPOOL_DEFAULT)
		{
			// render target -> lockable memory: read the whole target back, then copy the part
			if (dd.Pool == D3DPOOL_SYSTEMMEM && dd.Width == sd.Width && dd.Height == sd.Height &&
					r.left == 0 && r.top == 0 && (UINT)r.right == sd.Width && (UINT)r.bottom == sd.Height && p.x == 0 && p.y == 0)
			{
				hr = m_device->GetRenderTargetData(src, dst);
			}
			else
			{
				IDirect3DSurface9* tmp = nullptr;
				hr = m_device->CreateOffscreenPlainSurface(sd.Width, sd.Height, sd.Format, D3DPOOL_SYSTEMMEM, &tmp, nullptr);
				if (SUCCEEDED(hr))
				{
					hr = m_device->GetRenderTargetData(src, tmp);
					if (SUCCEEDED(hr))
						hr = lockedCopy(tmp, r, dst, p, sd.Format);
					tmp->Release();
				}
			}
		}
		else
		{
			// Tiberium Project @bugfix a 16-bit image into a texture the device made 32-bit
			if (sd.Format == D3DFMT_A4R4G4B4 && (dd.Format == D3DFMT_A8R8G8B8 || dd.Format == D3DFMT_X8R8G8B8))
				hr = lockedCopyA4R4G4B4ToA8R8G8B8(src, r, dst, p);
			else if (sd.Format != dd.Format)
				return D3DERR_INVALIDCALL;
			else
				hr = lockedCopy(src, r, dst, p, sd.Format);
		}
		if (FAILED(hr))
			return hr;
	}
	return D3D_OK;
}

HRESULT D3D8On9Device::SetRenderTarget(IDirect3DSurface9* target, IDirect3DSurface9* depth)
{
	// Direct3D 8: a null target keeps the current one; the depth buffer is always set
	if (target != nullptr)
	{
		HRESULT hr = m_device->SetRenderTarget(0, target);
		if (FAILED(hr))
			return hr;
	}
	return m_device->SetDepthStencilSurface(depth);
}

HRESULT D3D8On9Device::SetRenderState(D3DRENDERSTATETYPE state, DWORD value)
{
	switch ((int)state)
	{
		case 47: // D3DRS_ZBIAS
		{
			float bias = (float)value * ZBIAS_SCALE;
			DWORD bits;
			memcpy(&bits, &bias, sizeof(bits));
			return m_device->SetRenderState(D3DRS_DEPTHBIAS, bits);
		}
		case 40: // D3DRS_EDGEANTIALIAS
			return m_device->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, value);
		case 153: // D3DRS_SOFTWAREVERTEXPROCESSING
			if (m_behaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING)
				return m_device->SetSoftwareVertexProcessing(value);
			return D3D_OK;
		case 10: case 30: case 164: case 172: case 173: // line pattern, z-visible, patches: unused
			return D3D_OK;
		default:
		{
			// Tiberium Project @performance kept for the fixed-function shaders
			if (Is_Fixed_Function_Render_State(state) && m_ff.renderStates[state] != value)
			{
				m_ff.renderStates[state] = value;
				m_ff.changed = true;
			}
			return m_device->SetRenderState(state, value);
		}
	}
}

HRESULT D3D8On9Device::GetRenderState(D3DRENDERSTATETYPE state, DWORD* value)
{
	switch ((int)state)
	{
		case 47:
		{
			DWORD bits = 0;
			HRESULT hr = m_device->GetRenderState(D3DRS_DEPTHBIAS, &bits);
			float bias;
			memcpy(&bias, &bits, sizeof(bias));
			*value = (DWORD)(bias / ZBIAS_SCALE + 0.5f);
			return hr;
		}
		case 40:
			return m_device->GetRenderState(D3DRS_ANTIALIASEDLINEENABLE, value);
		case 153:
			*value = m_device->GetSoftwareVertexProcessing();
			return D3D_OK;
		case 10: case 30: case 164: case 172: case 173:
			*value = 0;
			return D3D_OK;
		default:
			return m_device->GetRenderState(state, value);
	}
}

HRESULT D3D8On9Device::SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value)
{
	D3DSAMPLERSTATETYPE sampler;
	if (isSamplerState(type, &sampler))
		return m_device->SetSamplerState(stage, sampler, value);
	// Tiberium Project @performance kept for the fixed-function shaders
	if (stage < 3 && Is_Fixed_Function_Stage_State(type) && m_ff.stages[stage][type] != value)
	{
		m_ff.stages[stage][type] = value;
		m_ff.changed = true;
	}
	return m_device->SetTextureStageState(stage, type, value);
}

HRESULT D3D8On9Device::GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD* value)
{
	D3DSAMPLERSTATETYPE sampler;
	if (isSamplerState(type, &sampler))
		return m_device->GetSamplerState(stage, sampler, value);
	return m_device->GetTextureStageState(stage, type, value);
}

HRESULT D3D8On9Device::SetIndices(IDirect3DIndexBuffer9* buffer, UINT baseVertexIndex)
{
	m_baseVertexIndex = baseVertexIndex;
	return m_device->SetIndices(buffer);
}

HRESULT D3D8On9Device::CreateVertexShader(const DWORD* declaration, const DWORD* function, DWORD* handle, DWORD /*usage*/)
{
	if (declaration == nullptr || handle == nullptr)
		return D3DERR_INVALIDCALL;

	// Direct3D 8 declaration tokens -> Direct3D 9 vertex elements
	std::vector<D3DVERTEXELEMENT9> elements;
	std::vector<DWORD> inputs;	// Tiberium Project @bugfix the shader's input registers, for its dcl instructions
	WORD stream = 0, offset = 0;
	for (const DWORD* t = declaration; *t != D3DVSD_END(); ++t)
	{
		DWORD token = *t;
		DWORD kind = (token >> D3DVSD_TOKENTYPESHIFT) & 7;
		if (kind == D3DVSD_TOKEN_STREAM)
		{
			stream = (WORD)(token & 0xF);
			offset = 0;
		}
		else if (kind == D3DVSD_TOKEN_STREAMDATA)
		{
			if (token & 0x10000000) // skip
			{
				offset += (WORD)(((token >> D3DVSD_SKIPCOUNTSHIFT) & 0xF) * 4);
				continue;
			}
			DWORD type8 = (token >> D3DVSD_DATATYPESHIFT) & 0xF;
			D3DVERTEXELEMENT9 e;
			e.Stream = stream;
			e.Offset = offset;
			e.Type = declType(type8);
			e.Method = D3DDECLMETHOD_DEFAULT;
			registerUsage(token & 0x1F, &e.Usage, &e.UsageIndex);
			elements.push_back(e);
			inputs.push_back(token & 0x1F);
			offset += (WORD)declSize(type8);
		}
		else if (kind == D3DVSD_TOKEN_CONSTMEM)
		{
			// inline constants: (count << 25) | address, followed by count * 4 floats
			DWORD n = (token >> D3DVSD_CONSTCOUNTSHIFT) & 0xF;
			m_device->SetVertexShaderConstantF(token & 0x7F, (const float*)(t + 1), n);
			t += n * 4;
		}
	}
	D3DVERTEXELEMENT9 end = D3DDECL_END();
	elements.push_back(end);

	// Tiberium Project @bugfix Direct3D 8's vertex shaders name their inputs by register alone; Direct3D 9
	// wants each declared (dcl_usage vN) before the first instruction. Without them DXVK reads the
	// inputs as zero (the trees drew nothing on the Linux rig) and Windows' runtime may refuse the
	// shader. The usages are the ones the declaration above gives each register.
	std::vector<DWORD> code;
	if (function != nullptr && (function[0] & 0xFFFF0000) == 0xFFFE0000 && ((function[0] >> 8) & 0xFF) == 1)
	{
		size_t first = 1;
		while ((function[first] & 0xFFFF) == 0xFFFE)	// comments
			first += 1 + (function[first] >> 16);
		if ((function[first] & 0xFFFF) != D3DSIO_DCL)
		{
			code.push_back(function[0]);
			for (size_t i = 0; i < inputs.size(); ++i)
			{
				BYTE usage = 0, index = 0;
				registerUsage(inputs[i], &usage, &index);
				code.push_back(D3DSIO_DCL);
				code.push_back(0x80000000u | usage | ((DWORD)index << 16));
				code.push_back(0x80000000u | (D3DSPR_INPUT << D3DSP_REGTYPE_SHIFT) | D3DSP_WRITEMASK_ALL | inputs[i]);
			}
			for (const DWORD* t = function + 1; ; ++t)
			{
				code.push_back(*t);
				if (*t == D3DVS_END())
					break;
			}
		}
	}

	VertexShader vs = { nullptr, nullptr, nullptr };
	HRESULT hr = m_device->CreateVertexDeclaration(&elements[0], &vs.decl);
	if (SUCCEEDED(hr) && function != nullptr)
		hr = m_device->CreateVertexShader(code.empty() ? function : &code[0], &vs.shader);	// vs_1_1 bytecode is valid in Direct3D 9
	if (FAILED(hr))
	{
		if (vs.decl) vs.decl->Release();
		return hr;
	}
	m_vertexShaders.push_back(vs);
	*handle = (DWORD)((m_vertexShaders.size() - 1) << 1) | SHADER_HANDLE_BIT;
	return D3D_OK;
}

HRESULT D3D8On9Device::SetVertexShader(DWORD handle)
{
	if (handle == m_vertexShader)
		return D3D_OK;	// Tiberium Project @feature a screen declaration in its place goes at the next draw
	m_vertexShader = handle;
	m_ff.declarationFvf = 0;	// Tiberium Project @feature what is set now replaces the screen declaration
	m_ff.substituteBound = false;	// and a stand-in
	if ((handle & SHADER_HANDLE_BIT) == 0)
	{
		m_device->SetVertexShader(m_vertexShaderOverride);	// usually none: fixed-function
		m_ff.vsBound = false;	// Tiberium Project @performance
		if (m_vertexDeclarationOverride != nullptr)
			return m_device->SetVertexDeclaration(m_vertexDeclarationOverride);
		return m_device->SetFVF(handle);
	}
	size_t index = handle >> 1;
	if (index >= m_vertexShaders.size() || m_vertexShaders[index].decl == nullptr)
		return D3DERR_INVALIDCALL;
	m_device->SetVertexDeclaration(m_vertexShaders[index].decl);
	m_ff.vsBound = false;	// Tiberium Project @performance
	return m_device->SetVertexShader(m_vertexShaders[index].shader);
}

HRESULT D3D8On9Device::DeleteVertexShader(DWORD handle)
{
	size_t index = handle >> 1;
	if ((handle & SHADER_HANDLE_BIT) == 0 || index >= m_vertexShaders.size())
		return D3DERR_INVALIDCALL;
	VertexShader& vs = m_vertexShaders[index];
	if (m_vertexShader == handle)
	{
		m_device->SetVertexShader(nullptr);
		m_vertexShader = ~0u;
		m_ff.vsBound = false;	// Tiberium Project @performance
		m_ff.substituteBound = false;
	}
	if (vs.decl) vs.decl->Release();
	if (vs.shader) vs.shader->Release();
	if (vs.substitute) vs.substitute->Release();	// Tiberium Project @feature
	vs.decl = nullptr;
	vs.shader = nullptr;
	vs.substitute = nullptr;
	return D3D_OK;
}

HRESULT D3D8On9Device::CreatePixelShader(const DWORD* function, DWORD* handle)
{
	IDirect3DPixelShader9* shader = nullptr;
	HRESULT hr = m_device->CreatePixelShader(function, &shader);	// ps_1_x bytecode is valid in Direct3D 9
	if (FAILED(hr))
		return hr;
	m_pixelShaders.push_back(shader);
	*handle = (DWORD)m_pixelShaders.size();	// 0 means "no pixel shader"
	return D3D_OK;
}

HRESULT D3D8On9Device::SetPixelShader(DWORD handle)
{
	m_ff.psBound = nullptr;	// Tiberium Project @performance the renderer's own, in place of the fixed-function one
	if (handle == 0)
		return m_device->SetPixelShader(nullptr);
	if (handle > m_pixelShaders.size())
		return D3DERR_INVALIDCALL;
	return m_device->SetPixelShader(m_pixelShaders[handle - 1]);
}

HRESULT D3D8On9Device::DeletePixelShader(DWORD handle)
{
	if (handle == 0 || handle > m_pixelShaders.size() || m_pixelShaders[handle - 1] == nullptr)
		return D3DERR_INVALIDCALL;
	IDirect3DPixelShader9* current = nullptr;
	if (SUCCEEDED(m_device->GetPixelShader(&current)) && current != nullptr)
	{
		if (current == m_pixelShaders[handle - 1])
			m_device->SetPixelShader(nullptr);
		current->Release();
	}
	m_pixelShaders[handle - 1]->Release();
	m_pixelShaders[handle - 1] = nullptr;
	return D3D_OK;
}

//-------------------------------------------------------------------------------------------------
D3D8On9Interface::D3D8On9Interface(IDirect3D9* d3d) :
	m_d3d(d3d),
	m_refs(1),
	m_modesAdapter(~0u)
{
}

ULONG D3D8On9Interface::Release()
{
	ULONG refs = --m_refs;
	if (refs == 0)
		delete this;
	return refs;
}

HRESULT D3D8On9Interface::GetAdapterIdentifier(UINT adapter, DWORD /*flags*/, D3DADAPTER_IDENTIFIER9* id)
{
	// Direct3D 8's D3DENUM_NO_WHQL_LEVEL is Direct3D 9's default
	return m_d3d->GetAdapterIdentifier(adapter, 0, id);
}

void D3D8On9Interface::listModes(UINT adapter)
{
	if (m_modesAdapter == adapter)
		return;
	m_modes.clear();
	static const D3DFORMAT formats[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5 };
	for (size_t f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f)
	{
		UINT n = m_d3d->GetAdapterModeCount(adapter, formats[f]);
		for (UINT i = 0; i < n; ++i)
		{
			D3DDISPLAYMODE mode;
			if (SUCCEEDED(m_d3d->EnumAdapterModes(adapter, formats[f], i, &mode)))
				m_modes.push_back(mode);
		}
	}
	m_modesAdapter = adapter;
}

UINT D3D8On9Interface::GetAdapterModeCount(UINT adapter)
{
	listModes(adapter);
	return (UINT)m_modes.size();
}

HRESULT D3D8On9Interface::EnumAdapterModes(UINT adapter, UINT mode, D3DDISPLAYMODE* out)
{
	listModes(adapter);
	if (mode >= m_modes.size())
		return D3DERR_INVALIDCALL;
	*out = m_modes[mode];
	return D3D_OK;
}

HRESULT D3D8On9Interface::GetDeviceCaps(UINT adapter, D3DDEVTYPE type, D3DCAPS8* caps)
{
	HRESULT hr = m_d3d->GetDeviceCaps(adapter, type, caps);
	if (SUCCEEDED(hr))
		D3D8On9_Convert_Caps(caps);
	return hr;
}

HRESULT D3D8On9Interface::CreateDevice(UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags, D3DPRESENT_PARAMETERS* params, D3D8On9Device** device)
{
	IDirect3DDevice9* device9 = nullptr;
	D3DPRESENT_PARAMETERS params9 = D3D8On9_Present_Parameters(*params);
	HRESULT hr = m_d3d->CreateDevice(adapter, type, window, flags, &params9, &device9);
	returnPresentParameters(params9, params);
	*device = SUCCEEDED(hr) ? new D3D8On9Device(device9, flags) : nullptr;
	if (*device != nullptr)
		(*device)->Make_Readable_Depth(params9);
	return hr;
}
