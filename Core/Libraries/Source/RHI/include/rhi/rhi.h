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

// Tiberium Project @feature The render-device interface (RHI): the layer the renderer is moving onto,
// with a Vulkan 1.3 implementation that can trace rays (VK_KHR_acceleration_structure and
// VK_KHR_ray_query) and, later, a Direct3D 9 one for the current path. Explicit objects (buffers,
// textures, samplers, shaders, pipelines, acceleration structures) and command lists; the caller
// says what a resource is used for and the device places the barriers.
//
// The binding model is one fixed table, the same for every pipeline, so a draw binds by slot as the
// old renderer did (shaders name the bindings explicitly, [[vk::binding(N, 0)]] in HLSL):
//
//     binding  0 ..  7   uniform buffers  (setConstants: copied into the frame's ring)
//     binding  8 .. 23   textures with their samplers, t0 .. t15 (setTexture)
//     binding 24 .. 31   storage buffers (setStorageBuffer)
//     binding 32 .. 35   storage images (setStorageImage)
//     binding 36         the top-level acceleration structure (setAccelerationStructure)
//
// and 128 bytes of push constants (setPushConstants), seen by every stage.
//
// Nothing here touches game logic: rendering stays visual only.

#pragma once

#include <cstddef>
#include <cstdint>

namespace rhi
{

//------------------------------------------------------------------------------------------- types
enum class Format : uint16_t
{
	Unknown = 0,
	R8_UNORM,
	R8G8_UNORM,
	R8G8B8A8_UNORM,
	R8G8B8A8_SRGB,
	B8G8R8A8_UNORM,
	B8G8R8A8_SRGB,
	R16_FLOAT,
	R16G16_FLOAT,
	R16G16B16A16_FLOAT,
	R32_FLOAT,
	R32G32_FLOAT,
	R32G32B32_FLOAT,
	R32G32B32A32_FLOAT,
	R16_UINT,
	R32_UINT,
	BC1_UNORM,
	BC1_SRGB,
	BC2_UNORM,
	BC3_UNORM,
	BC3_SRGB,
	D32_FLOAT,
	D24_UNORM_S8_UINT,
	D32_FLOAT_S8_UINT,
};

/// Bytes per texel, or per 4x4 block for the block-compressed formats
uint32_t Format_Size(Format format);
bool Format_Is_Depth(Format format);
bool Format_Is_Compressed(Format format);

enum BufferUsage : uint32_t
{
	BUFFER_VERTEX = 1u << 0,
	BUFFER_INDEX = 1u << 1,
	BUFFER_UNIFORM = 1u << 2,
	BUFFER_STORAGE = 1u << 3,
	BUFFER_COPY_SRC = 1u << 4,
	BUFFER_COPY_DST = 1u << 5,
	BUFFER_ACCEL_INPUT = 1u << 6,	///< vertices or indices an acceleration structure is built from
	BUFFER_INDIRECT = 1u << 7,
};

enum class Memory : uint8_t
{
	GPU,		///< device local; written by uploads or the GPU
	UPLOAD,		///< host visible and mapped, written by the CPU each frame
	READBACK,	///< host visible and mapped, read by the CPU after the GPU wrote it
};

struct BufferDesc
{
	uint64_t size = 0;
	uint32_t usage = 0;			///< BufferUsage bits
	Memory memory = Memory::GPU;
	const char* name = nullptr;
};

enum TextureUsage : uint32_t
{
	TEXTURE_SAMPLED = 1u << 0,
	TEXTURE_RENDER_TARGET = 1u << 1,
	TEXTURE_DEPTH_STENCIL = 1u << 2,
	TEXTURE_STORAGE = 1u << 3,
	TEXTURE_COPY_SRC = 1u << 4,
	TEXTURE_COPY_DST = 1u << 5,
};

enum class TextureType : uint8_t { TEX_2D, TEX_CUBE, TEX_3D };

struct TextureDesc
{
	TextureType type = TextureType::TEX_2D;
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t depth = 1;			///< 3D textures
	uint32_t mips = 1;
	Format format = Format::R8G8B8A8_UNORM;
	uint32_t usage = TEXTURE_SAMPLED | TEXTURE_COPY_DST;
	const char* name = nullptr;
};

enum class Filter : uint8_t { NEAREST, LINEAR };
enum class Address : uint8_t { WRAP, CLAMP, MIRROR, BORDER_BLACK, BORDER_WHITE };
enum class Compare : uint8_t { NEVER, LESS, EQUAL, LESS_EQUAL, GREATER, NOT_EQUAL, GREATER_EQUAL, ALWAYS };

struct SamplerDesc
{
	Filter minFilter = Filter::LINEAR;
	Filter magFilter = Filter::LINEAR;
	Filter mipFilter = Filter::LINEAR;
	Address u = Address::WRAP;
	Address v = Address::WRAP;
	Address w = Address::WRAP;
	float maxAnisotropy = 1.0f;		///< above 1 turns anisotropic filtering on
	float mipBias = 0.0f;
	float maxLod = 1000.0f;
	bool compare = false;			///< a depth comparison sampler (shadow maps)
	Compare compareOp = Compare::LESS_EQUAL;
};

enum class Stage : uint8_t { VERTEX, PIXEL, COMPUTE };

/// A shader: SPIR-V words for Vulkan (HLSL compiled by DXC with -spirv), Direct3D bytecode for the
/// Direct3D 9 device
struct ShaderDesc
{
	Stage stage = Stage::VERTEX;
	const void* code = nullptr;
	size_t size = 0;					///< in bytes
	const char* entry = "main";
	const char* name = nullptr;
};

struct VertexAttribute
{
	uint32_t location = 0;		///< the shader input's location ([[vk::location(N)]])
	Format format = Format::R32G32B32_FLOAT;
	uint32_t binding = 0;		///< which vertex buffer
	uint32_t offset = 0;
};

struct VertexBinding
{
	uint32_t binding = 0;
	uint32_t stride = 0;
	bool perInstance = false;
};

enum class Blend : uint8_t { ZERO, ONE, SRC_COLOR, INV_SRC_COLOR, SRC_ALPHA, INV_SRC_ALPHA, DST_COLOR, INV_DST_COLOR, DST_ALPHA, INV_DST_ALPHA };
enum class BlendOp : uint8_t { ADD, SUBTRACT, REV_SUBTRACT, MIN, MAX };

struct BlendState
{
	bool enable = false;
	Blend src = Blend::ONE;
	Blend dst = Blend::ZERO;
	BlendOp op = BlendOp::ADD;
	Blend srcAlpha = Blend::ONE;
	Blend dstAlpha = Blend::ZERO;
	BlendOp opAlpha = BlendOp::ADD;
	uint8_t writeMask = 0xF;	///< RGBA bits
};

struct DepthState
{
	bool test = true;
	bool write = true;
	Compare func = Compare::LESS_EQUAL;
};

enum class Cull : uint8_t { NONE, FRONT, BACK };
enum class Topology : uint8_t { TRIANGLE_LIST, TRIANGLE_STRIP, LINE_LIST, POINT_LIST };

struct RasterState
{
	Cull cull = Cull::BACK;
	bool frontCounterClockwise = false;
	bool wireframe = false;
	float depthBias = 0.0f;
	float slopeBias = 0.0f;
};

struct Shader;
struct Buffer;
struct Texture;
struct Sampler;
struct Pipeline;
struct AccelStruct;

enum { MAX_VERTEX_BINDINGS = 4, MAX_VERTEX_ATTRIBUTES = 16, MAX_COLOR_TARGETS = 4 };

struct GraphicsPipelineDesc
{
	Shader* vs = nullptr;
	Shader* ps = nullptr;
	VertexBinding bindings[MAX_VERTEX_BINDINGS];
	uint32_t bindingCount = 0;
	VertexAttribute attributes[MAX_VERTEX_ATTRIBUTES];
	uint32_t attributeCount = 0;
	Topology topology = Topology::TRIANGLE_LIST;
	RasterState raster;
	DepthState depth;
	BlendState blend[MAX_COLOR_TARGETS];
	Format colorFormats[MAX_COLOR_TARGETS] = {};
	uint32_t colorCount = 0;
	Format depthFormat = Format::Unknown;
	const char* name = nullptr;
};

struct ComputePipelineDesc
{
	Shader* cs = nullptr;
	const char* name = nullptr;
};

//---------------------------------------------------------------------------- ray tracing
/// Triangles a bottom-level acceleration structure holds: 32-bit float positions (x y z at the
/// stride), 16- or 32-bit indices
struct TriangleGeometry
{
	Buffer* vertices = nullptr;
	uint64_t vertexOffset = 0;
	uint32_t vertexStride = 12;
	uint32_t vertexCount = 0;
	Buffer* indices = nullptr;		///< none: the vertices are a triangle list
	uint64_t indexOffset = 0;
	uint32_t indexCount = 0;
	bool indices16 = false;
	bool opaque = true;
};

/// One instance in a top-level acceleration structure
struct Instance
{
	float transform[3][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } };	///< row-major, world from object
	uint32_t id = 0;			///< InstanceID() in the shaders (24 bits)
	uint8_t mask = 0xFF;		///< the rays' instance mask selects by these bits
	AccelStruct* blas = nullptr;
};

//------------------------------------------------------------------------------- rendering
enum class Load : uint8_t { LOAD, CLEAR, DONT_CARE };

struct ColorTarget
{
	Texture* texture = nullptr;
	Load load = Load::LOAD;
	float clear[4] = { 0, 0, 0, 0 };
};

struct RenderPassDesc
{
	ColorTarget colors[MAX_COLOR_TARGETS];
	uint32_t colorCount = 0;
	Texture* depth = nullptr;
	Load depthLoad = Load::LOAD;
	float clearDepth = 1.0f;
	uint8_t clearStencil = 0;
};

struct Capabilities
{
	char deviceName[256] = {};
	bool rayQuery = false;			///< acceleration structures and ray queries from any shader
	bool software = false;			///< a CPU implementation (lavapipe, WARP): correct, slow
	uint32_t maxTextureSize = 0;
	uint64_t uniformAlignment = 256;
};

/// Commands for one frame, recorded in order and submitted with Device::Submit_Frame
class CommandList
{
public:
	virtual ~CommandList() {}

	virtual void Begin_Render_Pass(const RenderPassDesc& desc) = 0;
	virtual void End_Render_Pass() = 0;
	virtual void Set_Viewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) = 0;
	virtual void Set_Scissor(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;

	virtual void Set_Pipeline(Pipeline* pipeline) = 0;
	virtual void Set_Vertex_Buffer(uint32_t binding, Buffer* buffer, uint64_t offset = 0) = 0;
	virtual void Set_Index_Buffer(Buffer* buffer, uint64_t offset = 0, bool indices16 = true) = 0;
	/// Copies the data into this frame's uniform ring and binds it at slot (0 .. 7)
	virtual void Set_Constants(uint32_t slot, const void* data, size_t size) = 0;
	virtual void Set_Push_Constants(const void* data, uint32_t size, uint32_t offset = 0) = 0;
	virtual void Set_Texture(uint32_t slot, Texture* texture, Sampler* sampler) = 0;	///< slot 0 .. 15
	virtual void Set_Storage_Buffer(uint32_t slot, Buffer* buffer, uint64_t offset = 0, uint64_t size = ~0ull) = 0;	///< 0 .. 7
	virtual void Set_Storage_Image(uint32_t slot, Texture* texture) = 0;	///< 0 .. 3
	virtual void Set_Acceleration_Structure(AccelStruct* tlas) = 0;

	virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
	virtual void Draw_Indexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t baseVertex = 0,
		uint32_t firstInstance = 0) = 0;
	virtual void Dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) = 0;

	/// Copies through the frame's staging ring (any buffer or texture, whatever its memory)
	virtual void Update_Buffer(Buffer* buffer, const void* data, size_t size, uint64_t offset = 0) = 0;
	virtual void Update_Texture(Texture* texture, uint32_t mip, uint32_t layer, const void* data, size_t rowPitch) = 0;
	virtual void Copy_Buffer(Buffer* dst, uint64_t dstOffset, Buffer* src, uint64_t srcOffset, uint64_t size) = 0;
	/// The texture's mip into a READBACK buffer, rows packed tightly
	virtual void Copy_Texture_To_Buffer(Buffer* dst, Texture* src, uint32_t mip = 0, uint32_t layer = 0) = 0;

	virtual void Build_Bottom_Level(AccelStruct* blas, const TriangleGeometry* geometry, uint32_t count) = 0;
	virtual void Build_Top_Level(AccelStruct* tlas, const Instance* instances, uint32_t count) = 0;

	virtual void Begin_Marker(const char* name) = 0;
	virtual void End_Marker() = 0;
};

class Device
{
public:
	virtual ~Device() {}

	virtual const Capabilities& Get_Capabilities() const = 0;

	virtual Buffer* Create_Buffer(const BufferDesc& desc, const void* initialData = nullptr) = 0;
	virtual Texture* Create_Texture(const TextureDesc& desc) = 0;
	virtual Sampler* Create_Sampler(const SamplerDesc& desc) = 0;
	virtual Shader* Create_Shader(const ShaderDesc& desc) = 0;
	virtual Pipeline* Create_Graphics_Pipeline(const GraphicsPipelineDesc& desc) = 0;
	virtual Pipeline* Create_Compute_Pipeline(const ComputePipelineDesc& desc) = 0;
	/// Sized for the geometry (bottom level) or the instance count (top level); built by a command list
	virtual AccelStruct* Create_Bottom_Level(const TriangleGeometry* geometry, uint32_t count) = 0;
	virtual AccelStruct* Create_Top_Level(uint32_t maxInstances) = 0;

	/// Released when the GPU is done with the frames that used them
	virtual void Destroy(Buffer* buffer) = 0;
	virtual void Destroy(Texture* texture) = 0;
	virtual void Destroy(Sampler* sampler) = 0;
	virtual void Destroy(Shader* shader) = 0;
	virtual void Destroy(Pipeline* pipeline) = 0;
	virtual void Destroy(AccelStruct* accel) = 0;

	/// UPLOAD and READBACK buffers stay mapped
	virtual void* Map(Buffer* buffer) = 0;
	virtual const TextureDesc& Get_Desc(Texture* texture) const = 0;

	/// The frame's command list (waits when the device is a whole ring of frames ahead)
	virtual CommandList* Begin_Frame() = 0;
	virtual void Submit_Frame() = 0;
	virtual void Wait_Idle() = 0;
};

struct VulkanOptions
{
	bool validation = false;		///< the Khronos validation layer, when installed
	bool requireRayQuery = false;	///< no device without ray queries
	int deviceIndex = -1;			///< -1: the best (discrete, then integrated, then CPU)
	bool allowSoftware = true;		///< lavapipe and the like
	const char* applicationName = "Tiberium";
	/// Messages (device chosen, failures, validation), one line each; nullptr: stderr
	void (*log)(const char* line) = nullptr;
};

/// A Vulkan 1.3 device, headless (presentation comes with the window work); nullptr when Vulkan
/// (the loader, a 1.3 device, the features the device layer needs) is not there
Device* Create_Vulkan_Device(const VulkanOptions& options);

}	// namespace rhi
