/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 Electronic Arts Inc.
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

// Tiberium Project @feature Ray-traced passes on DXVK's Vulkan device (see sage2raytracing.h)

#include "sage2raytracing.h"

#include <stdlib.h>
#include <string.h>

#include "sage2globals.h"

static bool s_enabled = true;
static float s_aoRadius = 40.0f;
static int s_aoRays = 6;
static float s_aoIntensity = 1.0f;

void Sage2RayTracingClass::Set_Enabled(bool enabled) { s_enabled = enabled; }
bool Sage2RayTracingClass::Is_Enabled() { return s_enabled; }

void Sage2RayTracingClass::Set_AO_Params(float radius, int rays, float intensity)
{
	s_aoRadius = radius > 1.0f ? radius : 1.0f;
	s_aoRays = rays < 1 ? 1 : (rays > 64 ? 64 : rays);
	s_aoIntensity = intensity > 0.0f ? intensity : 0.0f;
}

void Sage2RayTracingClass::Before_Device_Creation()
{
	// DXVK reads it when it makes its Vulkan device; a DXVK without our patch ignores it. Only in a
	// build with the render device: buffer device addresses cost DXVK something on some drivers
#if defined(RTS_RHI_VULKAN)
	if (!s_enabled)
		return;
#if defined(_WIN32)
	SetEnvironmentVariableA("DXVK_ENABLE_RAY_QUERY", "1");
#else
	setenv("DXVK_ENABLE_RAY_QUERY", "1", 1);
#endif
#endif
}

#if defined(RTS_RHI_VULKAN)

#include <stdint.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "rhi/rhi.h"
#include "../shaders/rt/spv_rtao.h"	// from shaders/rt/rtao.hlsl (scripts/tooling/rhi/build_spirv.py)

#include "WWMath/matrix3d.h"
#include "meshmdl.h"
#include "sage2shadow.h"
#include "ww3d.h"

//--------------------------------------------------------------------------------- DXVK's interop
// Binary copies of DXVK's ID3D9VkInteropDevice and ID3D9VkInteropTexture (src/d3d9/
// d3d9_interfaces.h, DXVK 2.x), with Vulkan's handles as plain integers and pointers so the
// renderer needs no Vulkan header: a VkImage is 64 bits, a layout or a format 32. Not in an
// anonymous namespace: the objects are DXVK's, and a compiler that sees every class of an
// interface with internal linkage may call its pure functions directly.
namespace Sage2DXVKInterop
{
	struct D3D9VkExtImageDescCopy
	{
		D3DRESOURCETYPE Type;
		UINT Width;
		UINT Height;
		UINT Depth;
		UINT MipLevels;
		DWORD Usage;
		D3DFORMAT Format;
		D3DPOOL Pool;
		D3DMULTISAMPLE_TYPE MultiSample;
		DWORD MultiSampleQuality;
		bool Discard;
		bool IsAttachmentOnly;
		bool IsLockable;
		uint32_t ImageUsage;
	};

	struct VkImageCreateInfoCopy
	{
		int32_t sType;			// VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO = 14
		const void* pNext;
		uint32_t flags;
		int32_t imageType;
		int32_t format;
		uint32_t extent[3];
		uint32_t mipLevels;
		uint32_t arrayLayers;
		int32_t samples;
		int32_t tiling;
		uint32_t usage;
		int32_t sharingMode;
		uint32_t queueFamilyIndexCount;
		const uint32_t* pQueueFamilyIndices;
		int32_t initialLayout;
	};

	struct InteropTexture : public IUnknown
	{
		virtual HRESULT STDMETHODCALLTYPE GetVulkanImageInfo(uint64_t* image, int32_t* layout, VkImageCreateInfoCopy* info) = 0;
	};

	struct InteropDevice : public IUnknown
	{
		virtual void STDMETHODCALLTYPE GetVulkanHandles(void** instance, void** physicalDevice, void** device) = 0;
		virtual void STDMETHODCALLTYPE GetSubmissionQueue(void** queue, uint32_t* queueIndex, uint32_t* queueFamily) = 0;
		virtual void STDMETHODCALLTYPE TransitionTextureLayout(InteropTexture* texture, const void* subresources, int32_t oldLayout, int32_t newLayout) = 0;
		virtual void STDMETHODCALLTYPE FlushRenderingCommands() = 0;
		virtual void STDMETHODCALLTYPE LockSubmissionQueue() = 0;
		virtual void STDMETHODCALLTYPE ReleaseSubmissionQueue() = 0;
		virtual void STDMETHODCALLTYPE LockDevice() = 0;
		virtual void STDMETHODCALLTYPE UnlockDevice() = 0;
		virtual bool STDMETHODCALLTYPE WaitForResource(IDirect3DResource9* resource, DWORD mapFlags) = 0;
		virtual HRESULT STDMETHODCALLTYPE CreateImage(const D3D9VkExtImageDescCopy* desc, IDirect3DResource9** result) = 0;
	};

	const GUID IID_InteropDevice = { 0x2eaa4b89, 0x0107, 0x4bdb, { 0x87, 0xf7, 0x0f, 0x54, 0x1c, 0x49, 0x3c, 0xe0 } };
	const GUID IID_InteropTexture = { 0xd56344f5, 0x8d35, 0x46fd, { 0x80, 0x6d, 0x94, 0xc3, 0x51, 0xb4, 0x72, 0xc1 } };
}

namespace
{
	using namespace Sage2DXVKInterop;

	const uint32_t VK_IMAGE_USAGE_STORAGE = 0x00000008;
	const int32_t VK_FORMAT_R32_SFLOAT_VALUE = 100;

	//------------------------------------------------------------------------------- state
	enum State { NOT_TRIED, READY, UNAVAILABLE };
	State s_state = NOT_TRIED;
	InteropDevice* s_interop = nullptr;
	rhi::Device* s_rhi = nullptr;
	rhi::Shader* s_aoShader = nullptr;
	rhi::Pipeline* s_aoPipeline = nullptr;
	rhi::Sampler* s_pointSampler = nullptr;
	rhi::AccelStruct* s_tlas = nullptr;
	uint32_t s_tlasCapacity = 0;

	// the AO target: a Direct3D 9 texture DXVK made with storage use, and the RHI's view of it
	IDirect3DTexture9* s_aoTexture = nullptr;
	rhi::Texture* s_aoImage = nullptr;
	unsigned s_aoWidth = 0, s_aoHeight = 0;
	// the depth buffer, as the RHI sees it (made again when the device's changes)
	IDirect3DTexture9* s_depthTexture = nullptr;
	rhi::Texture* s_depthImage = nullptr;

	// the terrain
	std::vector<float> s_terrainPositions;
	std::vector<uint32_t> s_terrainIndices;
	unsigned s_terrainVersion = 0, s_terrainBuilt = 0;
	rhi::Buffer* s_terrainVertices = nullptr;
	rhi::Buffer* s_terrainIndexBuffer = nullptr;
	rhi::AccelStruct* s_terrainBLAS = nullptr;

	// the meshes: a bottom level per model, the instances drawn this frame
	struct ModelEntry
	{
		rhi::Buffer* vertices = nullptr;
		rhi::Buffer* indices = nullptr;
		rhi::AccelStruct* blas = nullptr;
		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;
		bool built = false;
		bool unusable = false;
	};
	std::unordered_map<MeshModelClass*, ModelEntry> s_models;
	struct MeshInstance
	{
		MeshModelClass* model;
		float transform[3][4];
	};
	std::vector<MeshInstance> s_instances;
	std::unordered_set<uint64_t> s_seen;		// this frame's (model, position) pairs, drawn once each
	unsigned s_instanceFrame = 0xFFFFFFFF;
	const uint32_t MAX_BUILDS_PER_FRAME = 48;	// new models' structures built a frame (the rest wait)

	bool s_loggedOn = false;
	unsigned s_frames = 0;

	template <class T> void Release(T*& p) { if (p) { p->Release(); p = nullptr; } }

	void Log_Line(const char* line) { Sage2_Log("raytracing: %s", line); }

	void Lock_Queue(void*)
	{
		// DXVK's commands so far go first; its queue is ours until the release
		s_interop->FlushRenderingCommands();
		s_interop->LockSubmissionQueue();
	}

	void Unlock_Queue(void*) { s_interop->ReleaseSubmissionQueue(); }

	void Destroy_Model(ModelEntry& m)
	{
		if (s_rhi == nullptr)
			return;
		s_rhi->Destroy(m.blas);
		s_rhi->Destroy(m.vertices);
		s_rhi->Destroy(m.indices);
		m = ModelEntry();
	}

	//------------------------------------------------------------------------------- matrices
	// Direct3D's row-vector 4x4 matrices, 16 floats row by row
	void Multiply(const float* a, const float* b, float* out)
	{
		for (int r = 0; r < 4; ++r)
			for (int c = 0; c < 4; ++c)
				out[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] + a[r * 4 + 1] * b[1 * 4 + c] + a[r * 4 + 2] * b[2 * 4 + c] + a[r * 4 + 3] * b[3 * 4 + c];
	}

	bool Invert(const float* m, float* out)
	{
		// Gauss-Jordan in double, with partial pivoting
		double a[4][8];
		for (int r = 0; r < 4; ++r)
			for (int c = 0; c < 8; ++c)
				a[r][c] = c < 4 ? m[r * 4 + c] : (c - 4 == r ? 1.0 : 0.0);
		for (int col = 0; col < 4; ++col)
		{
			int pivot = col;
			for (int r = col + 1; r < 4; ++r)
				if ((a[r][col] < 0 ? -a[r][col] : a[r][col]) > (a[pivot][col] < 0 ? -a[pivot][col] : a[pivot][col]))
					pivot = r;
			if (a[pivot][col] == 0.0)
				return false;
			if (pivot != col)
				for (int c = 0; c < 8; ++c)
				{
					double t = a[col][c];
					a[col][c] = a[pivot][c];
					a[pivot][c] = t;
				}
			const double inv = 1.0 / a[col][col];
			for (int c = 0; c < 8; ++c)
				a[col][c] *= inv;
			for (int r = 0; r < 4; ++r)
				if (r != col && a[r][col] != 0.0)
				{
					const double f = a[r][col];
					for (int c = 0; c < 8; ++c)
						a[r][c] -= f * a[col][c];
				}
		}
		for (int r = 0; r < 4; ++r)
			for (int c = 0; c < 4; ++c)
				out[r * 4 + c] = (float)a[r][c + 4];
		return true;
	}

	//------------------------------------------------------------------------------- set-up
	bool Init(IDirect3DDevice9* device)
	{
		s_state = UNAVAILABLE;
		if (FAILED(device->QueryInterface(IID_InteropDevice, (void**)&s_interop)) || s_interop == nullptr)
		{
			Sage2_Log("raytracing: the Direct3D 9 is not DXVK's (no Vulkan interop); off");
			s_interop = nullptr;
			return false;
		}
		rhi::VulkanExternal e;
		s_interop->GetVulkanHandles(&e.instance, &e.physicalDevice, &e.device);
		uint32_t queueIndex = 0;
		s_interop->GetSubmissionQueue(&e.queue, &queueIndex, &e.queueFamily);
		e.rayQuery = true;
		e.lockQueue = Lock_Queue;
		e.unlockQueue = Unlock_Queue;
		e.log = Log_Line;
		s_rhi = rhi::Create_Vulkan_Device_External(e);
		if (s_rhi == nullptr)
		{
			Sage2_Log("raytracing: no render device on DXVK's Vulkan device; off");
			Release(s_interop);
			return false;
		}
		if (!s_rhi->Get_Capabilities().rayQuery)
		{
			Sage2_Log("raytracing: DXVK's device does not trace rays (a DXVK without the ray query patch, or a GPU without them); off");
			delete s_rhi;
			s_rhi = nullptr;
			Release(s_interop);
			return false;
		}
		rhi::ShaderDesc sd;
		sd.stage = rhi::Stage::COMPUTE;
		sd.code = spv_rtao;
		sd.size = sizeof(spv_rtao);
		sd.name = "rtao";
		s_aoShader = s_rhi->Create_Shader(sd);
		rhi::ComputePipelineDesc cd;
		cd.cs = s_aoShader;
		cd.name = "rtao";
		s_aoPipeline = s_aoShader ? s_rhi->Create_Compute_Pipeline(cd) : nullptr;
		rhi::SamplerDesc samp;
		samp.minFilter = samp.magFilter = samp.mipFilter = rhi::Filter::NEAREST;
		samp.u = samp.v = rhi::Address::CLAMP;
		s_pointSampler = s_rhi->Create_Sampler(samp);
		if (s_aoPipeline == nullptr || s_pointSampler == nullptr)
		{
			Sage2_Log("raytracing: the ambient occlusion shader could not be made; off");
			Sage2RayTracingClass::Shutdown();
			s_state = UNAVAILABLE;
			return false;
		}
		Sage2_Log("raytracing: on, sharing DXVK's device (%s)", s_rhi->Get_Capabilities().deviceName);
		s_state = READY;
		return true;
	}

	/// The RHI's view of a Direct3D 9 texture DXVK made (null if it is not DXVK's)
	rhi::Texture* Import(IDirect3DTexture9* texture, uint32_t usage, const char* name)
	{
		InteropTexture* interop = nullptr;
		if (FAILED(texture->QueryInterface(IID_InteropTexture, (void**)&interop)) || interop == nullptr)
			return nullptr;
		uint64_t image = 0;
		int32_t layout = 0;
		VkImageCreateInfoCopy info;
		memset(&info, 0, sizeof(info));
		info.sType = 14;
		const HRESULT hr = interop->GetVulkanImageInfo(&image, &layout, &info);
		interop->Release();
		if (FAILED(hr) || image == 0)
			return nullptr;
		rhi::ExternalTexture e;
		e.image = image;
		e.vkFormat = (uint32_t)info.format;
		e.width = info.extent[0];
		e.height = info.extent[1];
		e.usage = usage;
		e.layout = (uint32_t)layout;
		e.name = name;
		return s_rhi->Import_Texture(e);
	}

	/// The layout DXVK will have the texture in when its commands so far are done
	uint32_t Current_Layout(IDirect3DTexture9* texture)
	{
		InteropTexture* interop = nullptr;
		if (FAILED(texture->QueryInterface(IID_InteropTexture, (void**)&interop)) || interop == nullptr)
			return 0;
		uint64_t image = 0;
		int32_t layout = 0;
		interop->GetVulkanImageInfo(&image, &layout, nullptr);
		interop->Release();
		return (uint32_t)layout;
	}

	bool Make_AO_Target(unsigned width, unsigned height)
	{
		if (s_aoTexture != nullptr && s_aoWidth == width && s_aoHeight == height)
			return true;
		if (s_aoImage)
			s_rhi->Destroy(s_aoImage);
		s_aoImage = nullptr;
		Release(s_aoTexture);
		D3D9VkExtImageDescCopy desc;
		memset(&desc, 0, sizeof(desc));
		desc.Type = D3DRTYPE_TEXTURE;
		desc.Width = width;
		desc.Height = height;
		desc.Depth = 1;
		desc.MipLevels = 1;
		desc.Usage = 0;
		desc.Format = D3DFMT_R32F;
		desc.Pool = D3DPOOL_DEFAULT;
		desc.MultiSample = D3DMULTISAMPLE_NONE;
		desc.ImageUsage = VK_IMAGE_USAGE_STORAGE;
		IDirect3DResource9* resource = nullptr;
		if (FAILED(s_interop->CreateImage(&desc, &resource)) || resource == nullptr)
		{
			Sage2_Log("raytracing: the %ux%u ambient occlusion target could not be made", width, height);
			return false;
		}
		s_aoTexture = static_cast<IDirect3DTexture9*>(resource);
		s_aoImage = Import(s_aoTexture, rhi::TEXTURE_SAMPLED | rhi::TEXTURE_STORAGE, "ray-traced ambient occlusion");
		if (s_aoImage == nullptr)
		{
			Release(s_aoTexture);
			return false;
		}
		s_aoWidth = width;
		s_aoHeight = height;
		return true;
	}

	//------------------------------------------------------------------------------- the scene
	bool Build_Terrain(rhi::CommandList* cl)
	{
		if (s_terrainBuilt == s_terrainVersion)
			return s_terrainBLAS != nullptr;
		s_rhi->Destroy(s_terrainBLAS);
		s_rhi->Destroy(s_terrainVertices);
		s_rhi->Destroy(s_terrainIndexBuffer);
		s_terrainBLAS = nullptr;
		s_terrainVertices = s_terrainIndexBuffer = nullptr;
		s_terrainBuilt = s_terrainVersion;
		if (s_terrainIndices.empty())
			return false;
		rhi::BufferDesc vd;
		vd.size = s_terrainPositions.size() * sizeof(float);
		vd.usage = rhi::BUFFER_ACCEL_INPUT;
		vd.name = "terrain positions";
		s_terrainVertices = s_rhi->Create_Buffer(vd, s_terrainPositions.data());
		rhi::BufferDesc id;
		id.size = s_terrainIndices.size() * sizeof(uint32_t);
		id.usage = rhi::BUFFER_ACCEL_INPUT;
		id.name = "terrain indices";
		s_terrainIndexBuffer = s_rhi->Create_Buffer(id, s_terrainIndices.data());
		if (s_terrainVertices == nullptr || s_terrainIndexBuffer == nullptr)
			return false;
		rhi::TriangleGeometry g;
		g.vertices = s_terrainVertices;
		g.vertexStride = 12;
		g.vertexCount = (uint32_t)(s_terrainPositions.size() / 3);
		g.indices = s_terrainIndexBuffer;
		g.indexCount = (uint32_t)s_terrainIndices.size();
		g.indices16 = false;
		s_terrainBLAS = s_rhi->Create_Bottom_Level(&g, 1);
		if (s_terrainBLAS == nullptr)
			return false;
		cl->Build_Bottom_Level(s_terrainBLAS, &g, 1);
		Sage2_Log("raytracing: the terrain's structure, %u triangles", g.indexCount / 3);
		return true;
	}

	bool Build_Model(rhi::CommandList* cl, MeshModelClass* model, ModelEntry& m)
	{
		const int vertexCount = model->Get_Vertex_Count();
		const int polyCount = model->Get_Polygon_Count();
		if (vertexCount <= 0 || polyCount <= 0 || vertexCount > 65535)
		{
			m.unusable = true;
			return false;
		}
		const Vector3* positions = model->Get_Vertex_Array();
		const TriIndex* polys = model->Get_Polygon_Array();
		if (positions == nullptr || polys == nullptr)
		{
			m.unusable = true;
			return false;
		}
		std::vector<uint16_t> indices((size_t)polyCount * 3 + 1, 0);	// one more: a multiple of 4 bytes
		for (int i = 0; i < polyCount; ++i)
		{
			indices[i * 3 + 0] = polys[i].I;
			indices[i * 3 + 1] = polys[i].J;
			indices[i * 3 + 2] = polys[i].K;
		}
		rhi::BufferDesc vd;
		vd.size = (uint64_t)vertexCount * 12;
		vd.usage = rhi::BUFFER_ACCEL_INPUT;
		m.vertices = s_rhi->Create_Buffer(vd, positions);
		rhi::BufferDesc id;
		id.size = (uint64_t)((polyCount * 3 + 1) / 2) * 4;
		id.usage = rhi::BUFFER_ACCEL_INPUT;
		m.indices = s_rhi->Create_Buffer(id, indices.data());
		if (m.vertices == nullptr || m.indices == nullptr)
		{
			Destroy_Model(m);
			m.unusable = true;
			return false;
		}
		rhi::TriangleGeometry g;
		g.vertices = m.vertices;
		g.vertexStride = 12;
		g.vertexCount = (uint32_t)vertexCount;
		g.indices = m.indices;
		g.indexCount = (uint32_t)polyCount * 3;
		g.indices16 = true;
		m.blas = s_rhi->Create_Bottom_Level(&g, 1);
		if (m.blas == nullptr)
		{
			Destroy_Model(m);
			m.unusable = true;
			return false;
		}
		cl->Build_Bottom_Level(m.blas, &g, 1);
		m.vertexCount = (uint32_t)vertexCount;
		m.indexCount = g.indexCount;
		m.built = true;
		return true;
	}
}

//-------------------------------------------------------------------------------------------------
void Sage2RayTracingClass::Set_Height_Map(const unsigned char* data, int width, int height, int border, float cellSize, float heightScale)
{
	s_terrainPositions.clear();
	s_terrainIndices.clear();
	++s_terrainVersion;
	if (data == nullptr || width < 2 || height < 2)
		return;
	s_terrainPositions.resize((size_t)width * height * 3);
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			float* p = &s_terrainPositions[((size_t)y * width + x) * 3];
			p[0] = (float)(x - border) * cellSize;
			p[1] = (float)(y - border) * cellSize;
			p[2] = (float)data[(size_t)y * width + x] * heightScale;
		}
	s_terrainIndices.reserve((size_t)(width - 1) * (height - 1) * 6);
	for (int y = 0; y + 1 < height; ++y)
		for (int x = 0; x + 1 < width; ++x)
		{
			const uint32_t a = (uint32_t)(y * width + x), b = a + 1, c = a + (uint32_t)width, d = c + 1;
			s_terrainIndices.push_back(a); s_terrainIndices.push_back(b); s_terrainIndices.push_back(d);
			s_terrainIndices.push_back(a); s_terrainIndices.push_back(d); s_terrainIndices.push_back(c);
		}
}

void Sage2RayTracingClass::Add_Mesh(MeshModelClass* model, const Matrix3D& transform)
{
	if (!s_enabled || s_state == UNAVAILABLE || model == nullptr || Sage2ShadowClass::Is_Rendering())
		return;
	const unsigned frame = WW3D::Get_Frame_Count();
	if (frame != s_instanceFrame)
	{
		s_instanceFrame = frame;
		s_instances.clear();
		s_seen.clear();
	}
	// once per model and place (a mesh can be drawn twice a frame: reflections, the shadow map)
	const Vector3 position = transform.Get_Translation();
	uint64_t key = (uint64_t)(uintptr_t)model;
	uint32_t px, py, pz;
	memcpy(&px, &position.X, 4);
	memcpy(&py, &position.Y, 4);
	memcpy(&pz, &position.Z, 4);
	key ^= ((uint64_t)px * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)py * 0xC2B2AE3D27D4EB4Full) ^ ((uint64_t)pz * 0x165667B19E3779F9ull);
	if (!s_seen.insert(key).second)
		return;
	MeshInstance instance;
	instance.model = model;
	for (int r = 0; r < 3; ++r)
	{
		const Vector4& row = transform[r];
		instance.transform[r][0] = row.X;
		instance.transform[r][1] = row.Y;
		instance.transform[r][2] = row.Z;
		instance.transform[r][3] = row.W;
	}
	s_instances.push_back(instance);
}

void Sage2RayTracingClass::Forget_Model(MeshModelClass* model)
{
	auto it = s_models.find(model);
	if (it != s_models.end())
	{
		Destroy_Model(it->second);
		s_models.erase(it);
	}
	for (size_t i = 0; i < s_instances.size();)
	{
		if (s_instances[i].model == model)
		{
			s_instances[i] = s_instances.back();
			s_instances.pop_back();
		}
		else
			++i;
	}
}

bool Sage2RayTracingClass::Is_Available() { return s_state == READY; }

//-------------------------------------------------------------------------------------------------
IDirect3DTexture9* Sage2RayTracingClass::Trace_Ambient_Occlusion(IDirect3DDevice9* device, IDirect3DTexture9* depth, unsigned aoWidth, unsigned aoHeight,
	const float viewport[4], float minZ, float maxZ, const float view[16], const float projection[16])
{
	if (!s_enabled || s_state == UNAVAILABLE || device == nullptr || depth == nullptr || aoWidth == 0 || aoHeight == 0)
		return nullptr;
	if (s_state == NOT_TRIED && !Init(device))
		return nullptr;
	if (!Make_AO_Target(aoWidth, aoHeight))
		return nullptr;
	if (depth != s_depthTexture || s_depthImage == nullptr)
	{
		if (s_depthImage)
			s_rhi->Destroy(s_depthImage);
		s_depthTexture = depth;
		s_depthImage = Import(depth, rhi::TEXTURE_SAMPLED, "the frame's depth");
		if (s_depthImage == nullptr)
		{
			s_depthTexture = nullptr;
			return nullptr;
		}
	}
	// where DXVK will have left them when the commands so far are done
	s_rhi->Set_External_Layout(s_depthImage, Current_Layout(depth));
	s_rhi->Set_External_Layout(s_aoImage, Current_Layout(s_aoTexture));

	// the camera: clip space back to the world, and where the eye is
	float viewProjection[16], clipToWorld[16], viewToWorld[16];
	Multiply(view, projection, viewProjection);
	if (!Invert(viewProjection, clipToWorld) || !Invert(view, viewToWorld))
		return nullptr;

	if (WW3D::Get_Frame_Count() != s_instanceFrame)
	{
		s_instances.clear();	// nothing was drawn as a mesh this frame
		s_seen.clear();
		s_instanceFrame = WW3D::Get_Frame_Count();
	}

	rhi::CommandList* cl = s_rhi->Begin_Frame();
	const bool terrain = Build_Terrain(cl);
	// the frame's instances: the terrain, then each mesh whose model has its structure (new models
	// get theirs, a few a frame)
	std::vector<rhi::Instance> instances;
	instances.reserve(s_instances.size() + 1);
	if (terrain)
	{
		rhi::Instance t;
		t.blas = s_terrainBLAS;
		t.id = 0;
		t.mask = 0x01;
		instances.push_back(t);
	}
	uint32_t builds = 0;
	for (const MeshInstance& mi : s_instances)
	{
		ModelEntry& m = s_models[mi.model];
		if (m.unusable)
			continue;
		if (!m.built)
		{
			if (builds >= MAX_BUILDS_PER_FRAME || !Build_Model(cl, mi.model, m))
				continue;
			++builds;
		}
		rhi::Instance in;
		memcpy(in.transform, mi.transform, sizeof(in.transform));
		in.blas = m.blas;
		in.id = 1;
		in.mask = 0x02;
		instances.push_back(in);
	}
	if (instances.empty())
	{
		s_rhi->Submit_Frame();
		return nullptr;
	}
	if (instances.size() > s_tlasCapacity)
	{
		s_rhi->Destroy(s_tlas);
		s_tlasCapacity = 1024;
		while (s_tlasCapacity < instances.size())
			s_tlasCapacity *= 2;
		s_tlas = s_rhi->Create_Top_Level(s_tlasCapacity);
		if (s_tlas == nullptr)
		{
			s_tlasCapacity = 0;
			s_rhi->Submit_Frame();
			return nullptr;
		}
	}
	cl->Build_Top_Level(s_tlas, instances.data(), (uint32_t)instances.size());

	struct Constants
	{
		float clipToWorld[16];
		float viewport[4];
		float depthRange[4];
		float frame[4];
		float camera[4];
		float params[4];
	} c;
	memcpy(c.clipToWorld, clipToWorld, sizeof(c.clipToWorld));
	memcpy(c.viewport, viewport, sizeof(c.viewport));
	const float range = maxZ - minZ;
	c.depthRange[0] = minZ;
	c.depthRange[1] = range > 0.0f ? 1.0f / range : 1.0f;
	c.depthRange[2] = c.depthRange[3] = 0.0f;
	const rhi::TextureDesc& dd = s_rhi->Get_Desc(s_depthImage);
	c.frame[0] = 1.0f / (float)dd.width;
	c.frame[1] = 1.0f / (float)dd.height;
	c.frame[2] = (float)aoWidth;
	c.frame[3] = (float)aoHeight;
	c.camera[0] = viewToWorld[12];
	c.camera[1] = viewToWorld[13];
	c.camera[2] = viewToWorld[14];
	c.camera[3] = 1.0f;
	c.params[0] = s_aoRadius;
	c.params[1] = (float)s_aoRays;
	c.params[2] = s_aoIntensity;
	c.params[3] = 0.5f;
	cl->Set_Pipeline(s_aoPipeline);
	cl->Set_Constants(0, &c, sizeof(c));
	cl->Set_Texture(0, s_depthImage, s_pointSampler);
	cl->Set_Storage_Image(0, s_aoImage);
	cl->Set_Acceleration_Structure(s_tlas);
	cl->Dispatch((aoWidth + 7) / 8, (aoHeight + 7) / 8);
	s_rhi->Submit_Frame();
	++s_frames;
	if (!s_loggedOn || s_frames == 60)
	{
		s_loggedOn = true;
		Sage2_Log("raytracing: ambient occlusion %ux%u, %d rays of %.0f, %u instances (terrain %s), %u mesh models with structures",
			aoWidth, aoHeight, s_aoRays, s_aoRadius, (unsigned)instances.size(), terrain ? "in" : "missing", (unsigned)s_models.size());
	}
	return s_aoTexture;
}

//-------------------------------------------------------------------------------------------------
void Sage2RayTracingClass::Release_Resources()
{
	// the textures DXVK made go before the device is reset; the structures stay
	if (s_rhi)
	{
		s_rhi->Wait_Idle();
		s_rhi->Destroy(s_aoImage);
		s_rhi->Destroy(s_depthImage);
		s_rhi->Wait_Idle();
	}
	s_aoImage = nullptr;
	s_depthImage = nullptr;
	s_depthTexture = nullptr;
	Release(s_aoTexture);
	s_aoWidth = s_aoHeight = 0;
}

void Sage2RayTracingClass::Shutdown()
{
	Release_Resources();
	if (s_rhi)
	{
		for (auto& m : s_models)
			Destroy_Model(m.second);
		s_rhi->Destroy(s_terrainBLAS);
		s_rhi->Destroy(s_terrainVertices);
		s_rhi->Destroy(s_terrainIndexBuffer);
		s_rhi->Destroy(s_tlas);
		s_rhi->Destroy(s_aoPipeline);
		s_rhi->Destroy(s_aoShader);
		s_rhi->Destroy(s_pointSampler);
		s_rhi->Wait_Idle();
		delete s_rhi;
	}
	s_rhi = nullptr;
	s_models.clear();
	s_instances.clear();
	s_seen.clear();
	s_terrainBLAS = nullptr;
	s_terrainVertices = s_terrainIndexBuffer = nullptr;
	s_terrainBuilt = 0;
	s_tlas = nullptr;
	s_tlasCapacity = 0;
	s_aoPipeline = nullptr;
	s_aoShader = nullptr;
	s_pointSampler = nullptr;
	Release(s_interop);
	s_state = NOT_TRIED;
	s_loggedOn = false;
}

#else	// no Vulkan render device in this build: nothing is traced

void Sage2RayTracingClass::Set_Height_Map(const unsigned char*, int, int, int, float, float) {}
void Sage2RayTracingClass::Add_Mesh(MeshModelClass*, const Matrix3D&) {}
void Sage2RayTracingClass::Forget_Model(MeshModelClass*) {}
IDirect3DTexture9* Sage2RayTracingClass::Trace_Ambient_Occlusion(IDirect3DDevice9*, IDirect3DTexture9*, unsigned, unsigned, const float*, float, float,
	const float*, const float*) { return nullptr; }
bool Sage2RayTracingClass::Is_Available() { return false; }
void Sage2RayTracingClass::Release_Resources() {}
void Sage2RayTracingClass::Shutdown() {}

#endif
