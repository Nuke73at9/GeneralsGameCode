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

#include <cstring>

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
	m_vertexShaderOverride(nullptr)
{
}

D3D8On9Device::~D3D8On9Device()
{
	for (size_t i = 0; i < m_vertexShaders.size(); ++i)
	{
		if (m_vertexShaders[i].decl) m_vertexShaders[i].decl->Release();
		if (m_vertexShaders[i].shader) m_vertexShaders[i].shader->Release();
	}
	for (size_t i = 0; i < m_pixelShaders.size(); ++i)
	{
		if (m_pixelShaders[i]) m_pixelShaders[i]->Release();
	}
	m_device->Release();
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
}

void D3D8On9Device::Set_Vertex_Shader_Override(IDirect3DVertexShader9* shader)
{
	if (shader == m_vertexShaderOverride)
		return;
	m_vertexShaderOverride = shader;
	if (m_vertexShader != ~0u && (m_vertexShader & SHADER_HANDLE_BIT) == 0)
		m_device->SetVertexShader(shader);
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
	HRESULT hr = m_device->Reset(&params9);
	returnPresentParameters(params9, params);
	m_baseVertexIndex = 0;
	Invalidate_State();
	return hr;
}

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
			if (sd.Format != dd.Format)
				return D3DERR_INVALIDCALL;
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
			return m_device->SetRenderState(state, value);
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

	VertexShader vs = { nullptr, nullptr };
	HRESULT hr = m_device->CreateVertexDeclaration(&elements[0], &vs.decl);
	if (SUCCEEDED(hr) && function != nullptr)
		hr = m_device->CreateVertexShader(function, &vs.shader);	// vs_1_1 bytecode is valid in Direct3D 9
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
		return D3D_OK;
	m_vertexShader = handle;
	if ((handle & SHADER_HANDLE_BIT) == 0)
	{
		m_device->SetVertexShader(m_vertexShaderOverride);	// usually none: fixed-function
		return m_device->SetFVF(handle);
	}
	size_t index = handle >> 1;
	if (index >= m_vertexShaders.size() || m_vertexShaders[index].decl == nullptr)
		return D3DERR_INVALIDCALL;
	m_device->SetVertexDeclaration(m_vertexShaders[index].decl);
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
	}
	if (vs.decl) vs.decl->Release();
	if (vs.shader) vs.shader->Release();
	vs.decl = nullptr;
	vs.shader = nullptr;
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
	return hr;
}
