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

// Tiberium Project @feature Direct3D 9 renderer (SAGE_USE_D3D9)
//
// The W3D renderer was written against Direct3D 8. With SAGE_USE_D3D9 this directory is put
// in front of the include path, so every #include <d3d8.h> lands here: the engine is built
// against Direct3D 9 instead, under the Direct3D 8 names it already uses.
//
//  - Resources (textures, surfaces, vertex and index buffers) are the Direct3D 9 interfaces
//    themselves; their Direct3D 8 names are aliases.
//  - The interface and the device are small adapter classes (D3D8On9Interface,
//    D3D8On9Device) with the Direct3D 8 methods the engine calls. Each forwards to Direct3D 9,
//    translating where the two APIs differ (sampler states, depth bias, stream offsets, base
//    vertex index, shader handles, surface copies).
//  - New rendering code (C&C4 shaders) uses the Direct3D 9 device directly:
//    DX8Wrapper::_Get_D3D_Device8()->Get_D3D9(). After changing Direct3D 9 state that the
//    adapter tracks (vertex shader / FVF, pixel shader, indices), call Invalidate_State().
//
// This keeps the engine's renderer code, and merges from upstream, untouched; the Direct3D 8
// build (SAGE_USE_D3D9 off) is unchanged.

#pragma once

#include <d3d9.h>

#include <vector>

#ifndef RTS_RENDERER_D3D9
#define RTS_RENDERER_D3D9 1
#endif

//-------------------------------------------------------------------------------------------------
// Resources: the Direct3D 9 interfaces under their Direct3D 8 names
//-------------------------------------------------------------------------------------------------
// Macros rather than typedefs, so the engine's forward declarations (struct IDirect3DTexture8;)
// still declare the right type.
#define IDirect3DResource8				IDirect3DResource9
#define IDirect3DBaseTexture8			IDirect3DBaseTexture9
#define IDirect3DTexture8					IDirect3DTexture9
#define IDirect3DCubeTexture8			IDirect3DCubeTexture9
#define IDirect3DVolumeTexture8		IDirect3DVolumeTexture9
#define IDirect3DSurface8					IDirect3DSurface9
#define IDirect3DVolume8					IDirect3DVolume9
#define IDirect3DVertexBuffer8		IDirect3DVertexBuffer9
#define IDirect3DIndexBuffer8			IDirect3DIndexBuffer9

#define LPDIRECT3DRESOURCE8				LPDIRECT3DRESOURCE9
#define LPDIRECT3DBASETEXTURE8		LPDIRECT3DBASETEXTURE9
#define LPDIRECT3DTEXTURE8				LPDIRECT3DTEXTURE9
#define LPDIRECT3DCUBETEXTURE8		LPDIRECT3DCUBETEXTURE9
#define LPDIRECT3DVOLUMETEXTURE8	LPDIRECT3DVOLUMETEXTURE9
#define LPDIRECT3DSURFACE8				LPDIRECT3DSURFACE9
#define LPDIRECT3DVOLUME8					LPDIRECT3DVOLUME9
#define LPDIRECT3DVERTEXBUFFER8		LPDIRECT3DVERTEXBUFFER9
#define LPDIRECT3DINDEXBUFFER8		LPDIRECT3DINDEXBUFFER9

typedef D3DCAPS9								D3DCAPS8;
typedef D3DLIGHT9								D3DLIGHT8;
typedef D3DMATERIAL9						D3DMATERIAL8;
typedef D3DVIEWPORT9						D3DVIEWPORT8;
typedef D3DCLIPSTATUS9					D3DCLIPSTATUS8;
typedef D3DADAPTER_IDENTIFIER9	D3DADAPTER_IDENTIFIER8;

//-------------------------------------------------------------------------------------------------
// Direct3D 8 values that Direct3D 9 dropped or moved (same numbers as Direct3D 8)
//-------------------------------------------------------------------------------------------------
// texture stage states that became sampler states
#define D3DTSS_ADDRESSU				((D3DTEXTURESTAGESTATETYPE)13)
#define D3DTSS_ADDRESSV				((D3DTEXTURESTAGESTATETYPE)14)
#define D3DTSS_BORDERCOLOR		((D3DTEXTURESTAGESTATETYPE)15)
#define D3DTSS_MAGFILTER			((D3DTEXTURESTAGESTATETYPE)16)
#define D3DTSS_MINFILTER			((D3DTEXTURESTAGESTATETYPE)17)
#define D3DTSS_MIPFILTER			((D3DTEXTURESTAGESTATETYPE)18)
#define D3DTSS_MIPMAPLODBIAS	((D3DTEXTURESTAGESTATETYPE)19)
#define D3DTSS_MAXMIPLEVEL		((D3DTEXTURESTAGESTATETYPE)20)
#define D3DTSS_MAXANISOTROPY	((D3DTEXTURESTAGESTATETYPE)21)
#define D3DTSS_ADDRESSW				((D3DTEXTURESTAGESTATETYPE)25)

// render states
#define D3DRS_LINEPATTERN							((D3DRENDERSTATETYPE)10)
#define D3DRS_ZVISIBLE								((D3DRENDERSTATETYPE)30)
#define D3DRS_EDGEANTIALIAS						((D3DRENDERSTATETYPE)40)
#define D3DRS_ZBIAS										((D3DRENDERSTATETYPE)47)
#define D3DRS_SOFTWAREVERTEXPROCESSING	((D3DRENDERSTATETYPE)153)
#define D3DRS_PATCHSEGMENTS						((D3DRENDERSTATETYPE)164)
#define D3DRS_POSITIONORDER						((D3DRENDERSTATETYPE)172)
#define D3DRS_NORMALORDER							((D3DRENDERSTATETYPE)173)

// present parameters: the Direct3D 8 field (fullscreen only) is Direct3D 9's PresentationInterval;
// the adapter gives windowed devices Direct3D 8's behaviour (see D3D8On9_Present_Parameters)
#define FullScreen_PresentationInterval	PresentationInterval

// caps
#define D3DCAPS2_NO2DDURING3DSCENE				0x00000002
#define D3DCAPS2_CANRENDERWINDOWED				0x00080000
#define D3DPRASTERCAPS_PAT								0x00000008
#define D3DPRASTERCAPS_ANTIALIASEDGES			0x00001000
#define D3DPRASTERCAPS_ZBIAS							0x00004000
#define D3DPRASTERCAPS_STRETCHBLTMULTISAMPLE	0x00800000
#define D3DPTFILTERCAPS_MAGFAFLATCUBIC		0x08000000
#define D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC	0x10000000
#define D3DVTXPCAPS_NO_VSDT_UBYTE4				0x00000080
#define D3DENUM_NO_WHQL_LEVEL							0x00000002
#define D3DSWAPEFFECT_COPY_VSYNC					((D3DSWAPEFFECT)4)
#define D3DFMT_W11V11U10									((D3DFORMAT)65)

// vertex shader declarations (Direct3D 9 uses vertex declarations instead)
enum D3DVSD_TOKENTYPE_8 {
	D3DVSD_TOKEN_NOP = 0, D3DVSD_TOKEN_STREAM = 1, D3DVSD_TOKEN_STREAMDATA = 2, D3DVSD_TOKEN_TESSELLATOR = 3,
	D3DVSD_TOKEN_CONSTMEM = 4, D3DVSD_TOKEN_EXT = 5, D3DVSD_TOKEN_END = 7
};
enum D3DVSDE_REGISTER_8 {
	D3DVSDE_POSITION = 0, D3DVSDE_BLENDWEIGHT = 1, D3DVSDE_BLENDINDICES = 2, D3DVSDE_NORMAL = 3,
	D3DVSDE_PSIZE = 4, D3DVSDE_DIFFUSE = 5, D3DVSDE_SPECULAR = 6, D3DVSDE_TEXCOORD0 = 7,
	D3DVSDE_TEXCOORD1 = 8, D3DVSDE_TEXCOORD2 = 9, D3DVSDE_TEXCOORD3 = 10, D3DVSDE_TEXCOORD4 = 11,
	D3DVSDE_TEXCOORD5 = 12, D3DVSDE_TEXCOORD6 = 13, D3DVSDE_TEXCOORD7 = 14, D3DVSDE_POSITION2 = 15,
	D3DVSDE_NORMAL2 = 16
};
enum D3DVSDT_TYPE_8 {
	D3DVSDT_FLOAT1 = 0, D3DVSDT_FLOAT2 = 1, D3DVSDT_FLOAT3 = 2, D3DVSDT_FLOAT4 = 3,
	D3DVSDT_D3DCOLOR = 4, D3DVSDT_UBYTE4 = 5, D3DVSDT_SHORT2 = 6, D3DVSDT_SHORT4 = 7
};
#define D3DVSD_DATATYPESHIFT		16
#define D3DVSD_SKIPCOUNTSHIFT		16
#define D3DVSD_CONSTCOUNTSHIFT	25
#define D3DVSD_TOKENTYPESHIFT		29
#define D3DVSD_MAKETOKENTYPE(t)	((((DWORD)(t)) << D3DVSD_TOKENTYPESHIFT) & (0x7u << D3DVSD_TOKENTYPESHIFT))
#define D3DVSD_STREAM(n)				(D3DVSD_MAKETOKENTYPE(D3DVSD_TOKEN_STREAM) | (n))
#define D3DVSD_REG(reg, type)		(D3DVSD_MAKETOKENTYPE(D3DVSD_TOKEN_STREAMDATA) | ((type) << D3DVSD_DATATYPESHIFT) | (reg))
#define D3DVSD_SKIP(n)					(D3DVSD_MAKETOKENTYPE(D3DVSD_TOKEN_STREAMDATA) | 0x10000000 | ((n) << D3DVSD_SKIPCOUNTSHIFT))
#define D3DVSD_CONST(addr, n)		(D3DVSD_MAKETOKENTYPE(D3DVSD_TOKEN_CONSTMEM) | ((n) << D3DVSD_CONSTCOUNTSHIFT) | (addr))
#define D3DVSD_NOP()						0x00000000
#define D3DVSD_END()						0xFFFFFFFF

//-------------------------------------------------------------------------------------------------
// The adapters
//-------------------------------------------------------------------------------------------------
class D3D8On9Device;

/// A Direct3D 9 swap chain with the Direct3D 8 methods
class D3D8On9SwapChain
{
public:
	explicit D3D8On9SwapChain(IDirect3DSwapChain9* chain) : m_chain(chain), m_refs(1) {}
	ULONG AddRef() { return ++m_refs; }
	ULONG Release();
	HRESULT Present(const RECT* src, const RECT* dst, HWND window, const RGNDATA* dirty)
		{ return m_chain->Present(src, dst, window, dirty, 0); }
	HRESULT GetBackBuffer(UINT index, D3DBACKBUFFER_TYPE type, IDirect3DSurface9** surface)
		{ return m_chain->GetBackBuffer(index, type, surface); }
	IDirect3DSwapChain9* Get_D3D9() const { return m_chain; }
private:
	IDirect3DSwapChain9* m_chain;
	ULONG m_refs;
};
#define IDirect3DSwapChain8 D3D8On9SwapChain
#define LPDIRECT3DSWAPCHAIN8 D3D8On9SwapChain*

/// A Direct3D 9 device with the Direct3D 8 methods the W3D renderer uses
class D3D8On9Device
{
public:
	D3D8On9Device(IDirect3DDevice9* device, DWORD behaviorFlags);

	ULONG AddRef() { return ++m_refs; }
	ULONG Release();
	IDirect3DDevice9* Get_D3D9() const { return m_device; }
	/// Forget what the adapter knows about shader, FVF and index state (after changing it through Direct3D 9)
	void Invalidate_State();

	// device
	HRESULT TestCooperativeLevel() { return m_device->TestCooperativeLevel(); }
	UINT GetAvailableTextureMem() { return m_device->GetAvailableTextureMem(); }
	HRESULT ResourceManagerDiscardBytes(DWORD) { return m_device->EvictManagedResources(); }
	HRESULT GetDeviceCaps(D3DCAPS8* caps);
	HRESULT GetDisplayMode(D3DDISPLAYMODE* mode) { return m_device->GetDisplayMode(0, mode); }
	HRESULT SetCursorProperties(UINT x, UINT y, IDirect3DSurface9* bitmap) { return m_device->SetCursorProperties(x, y, bitmap); }
	void SetCursorPosition(int x, int y, DWORD flags) { m_device->SetCursorPosition(x, y, flags); }
	BOOL ShowCursor(BOOL show) { return m_device->ShowCursor(show); }
	HRESULT CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* params, D3D8On9SwapChain** chain);
	HRESULT Reset(D3DPRESENT_PARAMETERS* params);
	HRESULT Present(const RECT* src, const RECT* dst, HWND window, const RGNDATA* dirty)
		{ return m_device->Present(src, dst, window, dirty); }
	HRESULT GetBackBuffer(UINT index, D3DBACKBUFFER_TYPE type, IDirect3DSurface9** surface)
		{ return m_device->GetBackBuffer(0, index, type, surface); }
	void SetGammaRamp(DWORD flags, const D3DGAMMARAMP* ramp) { m_device->SetGammaRamp(0, flags, ramp); }

	// resources
	HRESULT CreateTexture(UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** texture)
		{ return m_device->CreateTexture(w, h, levels, usage, format, pool, texture, nullptr); }
	HRESULT CreateVolumeTexture(UINT w, UINT h, UINT d, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DVolumeTexture9** texture)
		{ return m_device->CreateVolumeTexture(w, h, d, levels, usage, format, pool, texture, nullptr); }
	HRESULT CreateCubeTexture(UINT size, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DCubeTexture9** texture)
		{ return m_device->CreateCubeTexture(size, levels, usage, format, pool, texture, nullptr); }
	HRESULT CreateVertexBuffer(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool, IDirect3DVertexBuffer9** buffer)
		{ return m_device->CreateVertexBuffer(length, usage, fvf, pool, buffer, nullptr); }
	HRESULT CreateIndexBuffer(UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DIndexBuffer9** buffer)
		{ return m_device->CreateIndexBuffer(length, usage, format, pool, buffer, nullptr); }
	HRESULT CreateRenderTarget(UINT w, UINT h, D3DFORMAT format, D3DMULTISAMPLE_TYPE ms, BOOL lockable, IDirect3DSurface9** surface)
		{ return m_device->CreateRenderTarget(w, h, format, ms, 0, lockable, surface, nullptr); }
	HRESULT CreateDepthStencilSurface(UINT w, UINT h, D3DFORMAT format, D3DMULTISAMPLE_TYPE ms, IDirect3DSurface9** surface)
		{ return m_device->CreateDepthStencilSurface(w, h, format, ms, 0, FALSE, surface, nullptr); }
	/// Direct3D 8 image surfaces are lockable system-memory surfaces
	HRESULT CreateImageSurface(UINT w, UINT h, D3DFORMAT format, IDirect3DSurface9** surface)
		{ return m_device->CreateOffscreenPlainSurface(w, h, format, D3DPOOL_SYSTEMMEM, surface, nullptr); }
	HRESULT CopyRects(IDirect3DSurface9* src, const RECT* rects, UINT count, IDirect3DSurface9* dst, const POINT* points);
	HRESULT UpdateTexture(IDirect3DBaseTexture9* src, IDirect3DBaseTexture9* dst) { return m_device->UpdateTexture(src, dst); }
	HRESULT GetFrontBuffer(IDirect3DSurface9* dst) { return m_device->GetFrontBufferData(0, dst); }

	// render targets
	HRESULT SetRenderTarget(IDirect3DSurface9* target, IDirect3DSurface9* depth);
	HRESULT GetRenderTarget(IDirect3DSurface9** target) { return m_device->GetRenderTarget(0, target); }
	HRESULT GetDepthStencilSurface(IDirect3DSurface9** depth) { return m_device->GetDepthStencilSurface(depth); }

	// scene
	HRESULT BeginScene() { return m_device->BeginScene(); }
	HRESULT EndScene() { return m_device->EndScene(); }
	HRESULT Clear(DWORD count, const D3DRECT* rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil)
		{ return m_device->Clear(count, rects, flags, color, z, stencil); }
	HRESULT SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m) { return m_device->SetTransform(state, m); }
	HRESULT GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX* m) { return m_device->GetTransform(state, m); }
	HRESULT SetViewport(const D3DVIEWPORT9* vp) { return m_device->SetViewport(vp); }
	HRESULT GetViewport(D3DVIEWPORT9* vp) { return m_device->GetViewport(vp); }
	HRESULT SetMaterial(const D3DMATERIAL9* m) { return m_device->SetMaterial(m); }
	HRESULT SetLight(DWORD index, const D3DLIGHT9* light) { return m_device->SetLight(index, light); }
	HRESULT LightEnable(DWORD index, BOOL enable) { return m_device->LightEnable(index, enable); }
	HRESULT SetClipPlane(DWORD index, const float* plane) { return m_device->SetClipPlane(index, plane); }
	HRESULT SetRenderState(D3DRENDERSTATETYPE state, DWORD value);
	HRESULT GetRenderState(D3DRENDERSTATETYPE state, DWORD* value);
	HRESULT SetTexture(DWORD stage, IDirect3DBaseTexture9* texture) { return m_device->SetTexture(stage, texture); }
	HRESULT SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value);
	HRESULT GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD* value);
	HRESULT ValidateDevice(DWORD* passes) { return m_device->ValidateDevice(passes); }

	// geometry
	HRESULT SetStreamSource(UINT stream, IDirect3DVertexBuffer9* buffer, UINT stride)
		{ return m_device->SetStreamSource(stream, buffer, 0, stride); }
	HRESULT SetIndices(IDirect3DIndexBuffer9* buffer, UINT baseVertexIndex);
	HRESULT DrawPrimitive(D3DPRIMITIVETYPE type, UINT start, UINT count) { return m_device->DrawPrimitive(type, start, count); }
	HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount)
		{ return m_device->DrawIndexedPrimitive(type, (INT)m_baseVertexIndex, minIndex, numVertices, startIndex, primCount); }
	HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE type, UINT primCount, const void* data, UINT stride)
		{ return m_device->DrawPrimitiveUP(type, primCount, data, stride); }
	HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT primCount, const void* indices, D3DFORMAT format, const void* data, UINT stride)
		{ return m_device->DrawIndexedPrimitiveUP(type, minIndex, numVertices, primCount, indices, format, data, stride); }
	HRESULT ProcessVertices(UINT srcStart, UINT dstIndex, UINT count, IDirect3DVertexBuffer9* dst, DWORD flags)
		{ return m_device->ProcessVertices(srcStart, dstIndex, count, dst, nullptr, flags); }

	// shaders: Direct3D 8 hands out DWORD handles; an FVF code is a valid vertex shader handle
	HRESULT CreateVertexShader(const DWORD* declaration, const DWORD* function, DWORD* handle, DWORD usage);
	HRESULT SetVertexShader(DWORD handle);
	HRESULT DeleteVertexShader(DWORD handle);
	HRESULT SetVertexShaderConstant(DWORD reg, const void* data, DWORD count)
		{ return m_device->SetVertexShaderConstantF(reg, (const float*)data, count); }
	HRESULT CreatePixelShader(const DWORD* function, DWORD* handle);
	HRESULT SetPixelShader(DWORD handle);
	HRESULT DeletePixelShader(DWORD handle);
	HRESULT SetPixelShaderConstant(DWORD reg, const void* data, DWORD count)
		{ return m_device->SetPixelShaderConstantF(reg, (const float*)data, count); }

private:
	~D3D8On9Device();

	struct VertexShader { IDirect3DVertexDeclaration9* decl; IDirect3DVertexShader9* shader; };

	IDirect3DDevice9* m_device;
	DWORD m_behaviorFlags;
	ULONG m_refs;
	UINT m_baseVertexIndex;
	DWORD m_vertexShader;		///< the current Direct3D 8 vertex shader handle or FVF
	std::vector<VertexShader> m_vertexShaders;
	std::vector<IDirect3DPixelShader9*> m_pixelShaders;
};
#define IDirect3DDevice8 D3D8On9Device
#define LPDIRECT3DDEVICE8 D3D8On9Device*

/// A Direct3D 9 interface with the Direct3D 8 methods
class D3D8On9Interface
{
public:
	explicit D3D8On9Interface(IDirect3D9* d3d);
	ULONG AddRef() { return ++m_refs; }
	ULONG Release();
	IDirect3D9* Get_D3D9() const { return m_d3d; }

	UINT GetAdapterCount() { return m_d3d->GetAdapterCount(); }
	HRESULT GetAdapterIdentifier(UINT adapter, DWORD flags, D3DADAPTER_IDENTIFIER9* id);
	/// Direct3D 8 lists the modes of all display formats in one list
	UINT GetAdapterModeCount(UINT adapter);
	HRESULT EnumAdapterModes(UINT adapter, UINT mode, D3DDISPLAYMODE* out);
	HRESULT GetAdapterDisplayMode(UINT adapter, D3DDISPLAYMODE* mode) { return m_d3d->GetAdapterDisplayMode(adapter, mode); }
	HRESULT CheckDeviceType(UINT adapter, D3DDEVTYPE type, D3DFORMAT display, D3DFORMAT backbuffer, BOOL windowed)
		{ return m_d3d->CheckDeviceType(adapter, type, display, backbuffer, windowed); }
	HRESULT CheckDeviceFormat(UINT adapter, D3DDEVTYPE type, D3DFORMAT adapterFormat, DWORD usage, D3DRESOURCETYPE rtype, D3DFORMAT format)
		{ return m_d3d->CheckDeviceFormat(adapter, type, adapterFormat, usage, rtype, format); }
	HRESULT CheckDeviceMultiSampleType(UINT adapter, D3DDEVTYPE type, D3DFORMAT format, BOOL windowed, D3DMULTISAMPLE_TYPE ms)
		{ return m_d3d->CheckDeviceMultiSampleType(adapter, type, format, windowed, ms, nullptr); }
	HRESULT CheckDepthStencilMatch(UINT adapter, D3DDEVTYPE type, D3DFORMAT adapterFormat, D3DFORMAT rtFormat, D3DFORMAT dsFormat)
		{ return m_d3d->CheckDepthStencilMatch(adapter, type, adapterFormat, rtFormat, dsFormat); }
	HRESULT GetDeviceCaps(UINT adapter, D3DDEVTYPE type, D3DCAPS8* caps);
	HRESULT CreateDevice(UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags, D3DPRESENT_PARAMETERS* params, D3D8On9Device** device);

private:
	~D3D8On9Interface() { m_d3d->Release(); }
	void listModes(UINT adapter);

	IDirect3D9* m_d3d;
	ULONG m_refs;
	UINT m_modesAdapter;
	std::vector<D3DDISPLAYMODE> m_modes;
};
#define IDirect3D8 D3D8On9Interface
#define LPDIRECT3D8 D3D8On9Interface*

/// Turns Direct3D 9 caps into what a Direct3D 8 application expects (adds D3DPRASTERCAPS_ZBIAS)
void D3D8On9_Convert_Caps(D3DCAPS9* caps);
/// Bytes in a surface (Direct3D 8's D3DSURFACE_DESC::Size, which Direct3D 9 dropped)
UINT D3D8On9_Surface_Size(const D3DSURFACE_DESC& desc);

/// Direct3D 8 present parameters as Direct3D 9 expects them: windowed swap chains present at once
/// unless the swap effect is D3DSWAPEFFECT_COPY_VSYNC (which Direct3D 9 doesn't have)
D3DPRESENT_PARAMETERS D3D8On9_Present_Parameters(const D3DPRESENT_PARAMETERS& params8);
