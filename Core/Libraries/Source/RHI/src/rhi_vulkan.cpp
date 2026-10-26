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

// Tiberium Project @feature The RHI on Vulkan 1.3 (see rhi.h).
//
// Device: one queue that does graphics and compute; Vulkan 1.3's dynamic rendering (no render pass
// objects) and synchronization2; buffer device addresses; acceleration structures and ray queries
// when the device has them. Memory: blocks of 64 MB per memory type, buffers and images apart,
// first-fit with merging; device-local memory for GPU resources, host-visible for the frame rings.
// Frames: two in flight, each with its command buffer, descriptor pool, uniform ring, staging ring
// and the objects destroyed while it was recorded, retired by a timeline semaphore.
// Binding: one descriptor set layout for every pipeline (the slots in rhi.h); a draw or dispatch
// allocates a set from the frame's pool when its bindings changed and writes the slots bound.
// Barriers: each image keeps its layout; using it otherwise (sampled, target, storage, copy) moves
// it, ending and resuming the render pass when that happens inside one. Buffers: after a copy, a
// dispatch or an acceleration-structure build, one memory barrier before the next command.

#include "rhi/rhi.h"
#include "vk_loader.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

namespace rhi
{
using namespace vk;

//------------------------------------------------------------------------------------------ formats
namespace
{
	struct FormatInfo { Format format; VkFormat vk; uint32_t size; bool depth; bool compressed; };
	const FormatInfo FORMATS[] =
	{
		{ Format::Unknown, VK_FORMAT_UNDEFINED, 0, false, false },
		{ Format::R8_UNORM, VK_FORMAT_R8_UNORM, 1, false, false },
		{ Format::R8G8_UNORM, VK_FORMAT_R8G8_UNORM, 2, false, false },
		{ Format::R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, 4, false, false },
		{ Format::R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB, 4, false, false },
		{ Format::B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM, 4, false, false },
		{ Format::B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB, 4, false, false },
		{ Format::R16_FLOAT, VK_FORMAT_R16_SFLOAT, 2, false, false },
		{ Format::R16G16_FLOAT, VK_FORMAT_R16G16_SFLOAT, 4, false, false },
		{ Format::R16G16B16A16_FLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, 8, false, false },
		{ Format::R32_FLOAT, VK_FORMAT_R32_SFLOAT, 4, false, false },
		{ Format::R32G32_FLOAT, VK_FORMAT_R32G32_SFLOAT, 8, false, false },
		{ Format::R32G32B32_FLOAT, VK_FORMAT_R32G32B32_SFLOAT, 12, false, false },
		{ Format::R32G32B32A32_FLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, 16, false, false },
		{ Format::R16_UINT, VK_FORMAT_R16_UINT, 2, false, false },
		{ Format::R32_UINT, VK_FORMAT_R32_UINT, 4, false, false },
		{ Format::BC1_UNORM, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8, false, true },
		{ Format::BC1_SRGB, VK_FORMAT_BC1_RGBA_SRGB_BLOCK, 8, false, true },
		{ Format::BC2_UNORM, VK_FORMAT_BC2_UNORM_BLOCK, 16, false, true },
		{ Format::BC3_UNORM, VK_FORMAT_BC3_UNORM_BLOCK, 16, false, true },
		{ Format::BC3_SRGB, VK_FORMAT_BC3_SRGB_BLOCK, 16, false, true },
		{ Format::D32_FLOAT, VK_FORMAT_D32_SFLOAT, 4, true, false },
		{ Format::D24_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, 4, true, false },
		{ Format::D32_FLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, 8, true, false },
	};

	const FormatInfo& Info(Format f)
	{
		for (const FormatInfo& i : FORMATS)
			if (i.format == f)
				return i;
		return FORMATS[0];
	}

	VkFormat To_Vk(Format f) { return Info(f).vk; }

	Format From_Vk(VkFormat f)
	{
		for (const FormatInfo& i : FORMATS)
			if (i.vk == f)
				return i.format;
		return Format::Unknown;
	}

	bool Has_Stencil(Format f) { return f == Format::D24_UNORM_S8_UINT || f == Format::D32_FLOAT_S8_UINT; }

	VkCompareOp To_Vk(Compare c)
	{
		static const VkCompareOp table[] = { VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL,
			VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS };
		return table[(int)c];
	}

	VkBlendFactor To_Vk(Blend b)
	{
		static const VkBlendFactor table[] = { VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_COLOR,
			VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA };
		return table[(int)b];
	}

	VkBlendOp To_Vk(BlendOp o)
	{
		static const VkBlendOp table[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_MIN, VK_BLEND_OP_MAX };
		return table[(int)o];
	}

	VkSamplerAddressMode To_Vk(Address a)
	{
		switch (a)
		{
			case Address::WRAP: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case Address::CLAMP: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			case Address::MIRROR: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			default: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		}
	}

	uint64_t Align_Up(uint64_t v, uint64_t a) { return a > 1 ? (v + a - 1) / a * a : v; }

	// the fixed binding table (rhi.h)
	const uint32_t UNIFORM_BASE = 0, UNIFORM_COUNT = 8;
	const uint32_t TEXTURE_BASE = 8, TEXTURE_COUNT = 16;
	const uint32_t STORAGE_BUFFER_BASE = 24, STORAGE_BUFFER_COUNT = 8;
	const uint32_t STORAGE_IMAGE_BASE = 32, STORAGE_IMAGE_COUNT = 4;
	const uint32_t ACCEL_BINDING = 36;
	const uint32_t PUSH_CONSTANT_SIZE = 128;
	const uint32_t FRAMES = 2;
	const VkDeviceSize BLOCK_SIZE = 64ull << 20;
	const VkDeviceSize UNIFORM_RING_SIZE = 8ull << 20;
	const VkDeviceSize STAGING_RING_SIZE = 32ull << 20;
	const uint32_t DESCRIPTOR_SETS_PER_POOL = 4096;
	const uint32_t BUFFER_ACCEL_STORAGE = 1u << 31;	// internal: holds an acceleration structure

	const VkAccessFlags2 WRITE_ACCESS = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
		| VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT
		| VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
}

uint32_t Format_Size(Format format) { return Info(format).size; }
bool Format_Is_Depth(Format format) { return Info(format).depth; }
bool Format_Is_Compressed(Format format) { return Info(format).compressed; }

//------------------------------------------------------------------------------------------- memory
namespace
{
	struct Block;
	struct Allocation
	{
		Block* block = nullptr;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDeviceSize offset = 0;
		VkDeviceSize size = 0;
		void* mapped = nullptr;
	};

	struct Range { VkDeviceSize offset, size; };

	struct Block
	{
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDeviceSize size = 0;
		void* mapped = nullptr;
		uint32_t typeIndex = 0;
		bool linear = true;			// buffers and images never share a block (buffer-image granularity)
		bool dedicated = false;
		std::vector<Range> free;	// sorted by offset
	};

	class Allocator
	{
	public:
		VkDevice device = VK_NULL_HANDLE;
		VkPhysicalDeviceMemoryProperties props = {};
		bool deviceAddresses = true;	// the device has buffer device addresses on
		std::vector<Block*> blocks;

		int Find_Type(uint32_t bits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags avoid = 0) const
		{
			for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
				if ((bits & (1u << i)) && (props.memoryTypes[i].propertyFlags & want) == want && !(props.memoryTypes[i].propertyFlags & avoid))
					return (int)i;
			if (avoid != 0)
				return Find_Type(bits, want, 0);
			return -1;
		}

		bool Allocate(const VkMemoryRequirements& req, Memory memory, bool linear, bool deviceAddress, Allocation& out)
		{
			// GPU: device local, not the host-visible window (a discrete card's BAR) when there is
			// other device-local memory; READBACK: cached for reading, and coherent, so nothing is
			// invalidated by hand
			VkMemoryPropertyFlags want = 0, avoid = 0;
			switch (memory)
			{
				case Memory::GPU: want = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; avoid = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; break;
				case Memory::UPLOAD: want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; break;
				case Memory::READBACK: want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT; break;
			}
			int type = Find_Type(req.memoryTypeBits, want, avoid);
			if (type < 0 && memory == Memory::READBACK)
				type = Find_Type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			if (type < 0 && memory == Memory::GPU)
				type = Find_Type(req.memoryTypeBits, 0);
			if (type < 0)
				return false;
			const bool host = (props.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;

			if (req.size > BLOCK_SIZE / 2)
			{
				Block* b = New_Block((uint32_t)type, req.size, linear, host, deviceAddress);
				if (b == nullptr)
					return false;
				b->dedicated = true;
				b->free.clear();
				out = Allocation{ b, b->memory, 0, req.size, b->mapped };
				return true;
			}
			for (Block* b : blocks)
			{
				if (b->dedicated || b->typeIndex != (uint32_t)type || b->linear != linear)
					continue;
				if (Take(b, req.size, req.alignment, out))
					return true;
			}
			Block* b = New_Block((uint32_t)type, BLOCK_SIZE, linear, host, true);
			return b != nullptr && Take(b, req.size, req.alignment, out);
		}

		void Free(const Allocation& a)
		{
			Block* b = a.block;
			if (b == nullptr)
				return;
			if (b->dedicated)
			{
				Release_Block(b);
				return;
			}
			b->free.push_back(Range{ a.offset, a.size });
			std::sort(b->free.begin(), b->free.end(), [](const Range& x, const Range& y) { return x.offset < y.offset; });
			std::vector<Range> merged;
			for (const Range& r : b->free)
			{
				if (!merged.empty() && merged.back().offset + merged.back().size == r.offset)
					merged.back().size += r.size;
				else
					merged.push_back(r);
			}
			b->free.swap(merged);
		}

		void Shutdown()
		{
			for (Block* b : blocks)
			{
				vkFreeMemory(device, b->memory, nullptr);
				delete b;
			}
			blocks.clear();
		}

	private:
		Block* New_Block(uint32_t type, VkDeviceSize size, bool linear, bool host, bool deviceAddress)
		{
			VkMemoryAllocateFlagsInfo flags = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
			flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
			VkMemoryAllocateInfo info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			info.pNext = (linear && deviceAddress && deviceAddresses) ? &flags : nullptr;
			info.allocationSize = size;
			info.memoryTypeIndex = type;
			VkDeviceMemory memory = VK_NULL_HANDLE;
			if (vkAllocateMemory(device, &info, nullptr, &memory) != VK_SUCCESS)
				return nullptr;
			Block* b = new Block;
			b->memory = memory;
			b->size = size;
			b->typeIndex = type;
			b->linear = linear;
			b->free.push_back(Range{ 0, size });
			if (host && vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &b->mapped) != VK_SUCCESS)
				b->mapped = nullptr;
			blocks.push_back(b);
			return b;
		}

		void Release_Block(Block* b)
		{
			vkFreeMemory(device, b->memory, nullptr);
			blocks.erase(std::remove(blocks.begin(), blocks.end(), b), blocks.end());
			delete b;
		}

		bool Take(Block* b, VkDeviceSize size, VkDeviceSize alignment, Allocation& out)
		{
			for (size_t i = 0; i < b->free.size(); ++i)
			{
				Range& r = b->free[i];
				VkDeviceSize start = Align_Up(r.offset, alignment);
				if (start + size > r.offset + r.size)
					continue;
				VkDeviceSize before = start - r.offset;
				VkDeviceSize after = r.offset + r.size - (start + size);
				Range tail = { start + size, after };
				if (before > 0)
				{
					r.size = before;
					if (after > 0)
						b->free.insert(b->free.begin() + (long)i + 1, tail);
				}
				else if (after > 0)
					r = tail;
				else
					b->free.erase(b->free.begin() + (long)i);
				out = Allocation{ b, b->memory, start, size, b->mapped ? (char*)b->mapped + start : nullptr };
				return true;
			}
			return false;
		}
	};
}

//------------------------------------------------------------------------------------------ objects
struct Buffer
{
	VkBuffer buffer = VK_NULL_HANDLE;
	Allocation alloc;
	BufferDesc desc;
	VkDeviceAddress address = 0;
	void* mapped = nullptr;
};

struct Texture
{
	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;		// all mips and layers, for sampling and storage
	std::vector<VkImageView> targetViews;	// mip 0 of each layer, for rendering
	Allocation alloc;
	TextureDesc desc;
	uint32_t layers = 1;
	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
	VkAccessFlags2 access = VK_ACCESS_2_NONE;
	// another renderer's image (Import_Texture): the layout it goes back to after each frame
	bool imported = false;
	VkImageLayout homeLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	uint64_t usedInFrame = 0;
};

struct Sampler { VkSampler sampler = VK_NULL_HANDLE; };

struct Shader
{
	VkShaderModule module = VK_NULL_HANDLE;
	Stage stage = Stage::VERTEX;
	std::string entry;
};

struct Pipeline
{
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

struct AccelStruct
{
	VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
	Buffer* storage = nullptr;
	VkDeviceAddress address = 0;
	bool topLevel = false;
	uint32_t maxInstances = 0;
	VkDeviceSize scratchSize = 0;
};

//--------------------------------------------------------------------------------------- the device
class VulkanDevice;

namespace
{
	struct Ring
	{
		Buffer* buffer = nullptr;
		VkDeviceSize used = 0;
	};

	struct FrameData
	{
		VkCommandPool pool = VK_NULL_HANDLE;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		std::vector<VkDescriptorPool> descriptors;	// grows when a frame binds more than a pool holds
		uint32_t descriptorPool = 0;				// the one being allocated from
		Ring uniforms;
		Ring staging;
		std::vector<Buffer*> extraStaging;	// overflow, gone with the frame
		uint64_t completion = 0;			// the timeline value that says the GPU is done with it
	};

	// an object destroyed by the caller, freed when the GPU passes the timeline value
	struct Garbage
	{
		enum Kind { BUFFER, TEXTURE, SAMPLER, SHADER, PIPELINE, ACCEL } kind;
		void* object;
		uint64_t after;
	};

	struct Binding
	{
		bool set = false;
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceSize offset = 0, range = 0;
		Texture* texture = nullptr;
		VkSampler sampler = VK_NULL_HANDLE;
	};
}

class VulkanCommandList : public CommandList
{
public:
	VulkanDevice* dev = nullptr;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	FrameData* frame = nullptr;

	// render pass state: open between Begin_Render_Pass and End_Render_Pass; rendering is suspended
	// (vkCmdEndRendering) for barriers and copies, and resumed with what was drawn by the next draw
	bool passOpen = false;
	bool inPass = false;
	RenderPassDesc pass;
	// bindings
	Binding uniforms[UNIFORM_COUNT];
	Binding textures[TEXTURE_COUNT];
	Binding storageBuffers[STORAGE_BUFFER_COUNT];
	Binding storageImages[STORAGE_IMAGE_COUNT];
	AccelStruct* tlas = nullptr;
	bool dirtyGraphics = true;	// descriptor sets are bound per bind point
	bool dirtyCompute = true;
	Pipeline* pipeline = nullptr;
	// buffer hazards: the writes since the last barrier (read before the next command), and
	// whether anything ran since it (a copy or a build into a buffer waits for it)
	VkPipelineStageFlags2 pendingStage = VK_PIPELINE_STAGE_2_NONE;
	VkAccessFlags2 pendingAccess = VK_ACCESS_2_NONE;
	bool workSinceBarrier = false;
	// another renderer's images used this frame (put back in their layouts when it is submitted)
	std::vector<Texture*> importedUsed;
	uint64_t serial = 0;

	void Reset(VkCommandBuffer c, FrameData* f);

	virtual void Begin_Render_Pass(const RenderPassDesc& desc) override;
	virtual void End_Render_Pass() override;
	virtual void Set_Viewport(float x, float y, float width, float height, float minDepth, float maxDepth) override;
	virtual void Set_Scissor(int32_t x, int32_t y, uint32_t width, uint32_t height) override;
	virtual void Set_Pipeline(Pipeline* p) override;
	virtual void Set_Vertex_Buffer(uint32_t binding, Buffer* buffer, uint64_t offset) override;
	virtual void Set_Index_Buffer(Buffer* buffer, uint64_t offset, bool indices16) override;
	virtual void Set_Constants(uint32_t slot, const void* data, size_t size) override;
	virtual void Set_Push_Constants(const void* data, uint32_t size, uint32_t offset) override;
	virtual void Set_Texture(uint32_t slot, Texture* texture, Sampler* sampler) override;
	virtual void Set_Storage_Buffer(uint32_t slot, Buffer* buffer, uint64_t offset, uint64_t size) override;
	virtual void Set_Storage_Image(uint32_t slot, Texture* texture) override;
	virtual void Set_Acceleration_Structure(AccelStruct* accel) override;
	virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
	virtual void Draw_Indexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) override;
	virtual void Dispatch(uint32_t x, uint32_t y, uint32_t z) override;
	virtual void Update_Buffer(Buffer* buffer, const void* data, size_t size, uint64_t offset) override;
	virtual void Update_Texture(Texture* texture, uint32_t mip, uint32_t layer, const void* data, size_t rowPitch) override;
	virtual void Copy_Buffer(Buffer* dst, uint64_t dstOffset, Buffer* src, uint64_t srcOffset, uint64_t size) override;
	virtual void Copy_Texture_To_Buffer(Buffer* dst, Texture* src, uint32_t mip, uint32_t layer) override;
	virtual void Build_Bottom_Level(AccelStruct* blas, const TriangleGeometry* geometry, uint32_t count) override;
	virtual void Build_Top_Level(AccelStruct* tlas, const Instance* instances, uint32_t count) override;
	virtual void Begin_Marker(const char* name) override;
	virtual void End_Marker() override;

	// internals
	void Transition(Texture* t, VkImageLayout layout, VkPipelineStageFlags2 stage, VkAccessFlags2 access);
	void Flush_Buffer_Barrier();
	void Before_Transfer_Write();
	void Global_Barrier();
	void Suspend_Pass();
	void Begin_Rendering(const RenderPassDesc& desc);
	void Prepare_Draw(VkPipelineBindPoint bindPoint);
	static bool Is_Target(const Texture* t, const RenderPassDesc& desc)
	{
		for (uint32_t i = 0; i < desc.colorCount; ++i)
			if (desc.colors[i].texture == t)
				return true;
		return desc.depth == t;
	}
	bool Is_Target(const Texture* t) const { return passOpen && Is_Target(t, pass); }
	VkDescriptorSet Allocate_Set();
	bool Stage_Copy(const void* data, size_t size, VkDeviceSize alignment, Buffer*& buffer, VkDeviceSize& offset);
};

class VulkanDevice : public Device
{
public:
	VulkanOptions options;
	Capabilities caps;
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
	VkPhysicalDevice phys = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties props = {};
	VkDevice device = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t queueFamily = 0;
	bool rayQuery = false;
	bool debugUtils = false;
	bool external = false;				// on another renderer's device (Create_Vulkan_Device_External)
	bool bufferDeviceAddress = true;	// buffers have device addresses (needed by acceleration structures)
	VulkanExternal shared;
	uint64_t frameSerial = 0;
	Allocator allocator;
	VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipelineCache pipelineCache = VK_NULL_HANDLE;
	VkSemaphore timeline = VK_NULL_HANDLE;
	uint64_t timelineValue = 0;
	FrameData frames[FRAMES];
	uint32_t frameIndex = 0;
	bool frameOpen = false;
	VulkanCommandList cmdList;
	std::vector<Garbage> garbage;

	void Log(const char* fmt, ...)
	{
		char line[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(line, sizeof(line), fmt, args);
		va_end(args);
		if (options.log)
			options.log(line);
		else
			fprintf(stderr, "rhi: %s\n", line);
	}

	void Name(VkObjectType type, uint64_t handle, const char* name)
	{
		if (!debugUtils || name == nullptr || vkSetDebugUtilsObjectNameEXT == nullptr)
			return;
		VkDebugUtilsObjectNameInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
		info.objectType = type;
		info.objectHandle = handle;
		info.pObjectName = name;
		vkSetDebugUtilsObjectNameEXT(device, &info);
	}

	bool Init();
	bool Init_External(const VulkanExternal& e);
	bool Init_Common();
	void Wait_Timeline(uint64_t value);
	void Shutdown();
	~VulkanDevice() { Shutdown(); }

	FrameData& Current() { return frames[frameIndex]; }
	void Retire(FrameData& f);
	/// Frees what the GPU is done with (completed: the timeline's value; UINT64_MAX: everything)
	void Collect(uint64_t completed);
	/// The objects in use until this timeline value: the frame being recorded, else the last one sent
	void Defer(Garbage::Kind kind, void* object) { if (object) garbage.push_back(Garbage{ kind, object, frameOpen ? timelineValue + 1 : timelineValue }); }
	VkDescriptorPool Create_Descriptor_Pool();
	Buffer* Create_Buffer_Internal(const BufferDesc& desc);
	void Free_Buffer(Buffer* b);
	void Free_Texture(Texture* t);

	virtual const Capabilities& Get_Capabilities() const override { return caps; }
	virtual Buffer* Create_Buffer(const BufferDesc& desc, const void* initialData) override;
	virtual Texture* Create_Texture(const TextureDesc& desc) override;
	virtual Sampler* Create_Sampler(const SamplerDesc& desc) override;
	virtual Shader* Create_Shader(const ShaderDesc& desc) override;
	virtual Pipeline* Create_Graphics_Pipeline(const GraphicsPipelineDesc& desc) override;
	virtual Pipeline* Create_Compute_Pipeline(const ComputePipelineDesc& desc) override;
	virtual AccelStruct* Create_Bottom_Level(const TriangleGeometry* geometry, uint32_t count) override;
	virtual AccelStruct* Create_Top_Level(uint32_t maxInstances) override;
	virtual Texture* Import_Texture(const ExternalTexture& texture) override;
	virtual void Set_External_Layout(Texture* texture, uint32_t layout) override
	{
		if (texture == nullptr || !texture->imported || layout == 0)
			return;
		texture->layout = texture->homeLayout = (VkImageLayout)layout;
		texture->stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		texture->access = VK_ACCESS_2_MEMORY_WRITE_BIT;
	}
	virtual void Destroy(Buffer* buffer) override { Defer(Garbage::BUFFER, buffer); }
	virtual void Destroy(Texture* texture) override { Defer(Garbage::TEXTURE, texture); }
	virtual void Destroy(Sampler* sampler) override { Defer(Garbage::SAMPLER, sampler); }
	virtual void Destroy(Shader* shader) override { Defer(Garbage::SHADER, shader); }
	virtual void Destroy(Pipeline* pipeline) override { Defer(Garbage::PIPELINE, pipeline); }
	virtual void Destroy(AccelStruct* accel) override { Defer(Garbage::ACCEL, accel); }
	virtual void* Map(Buffer* buffer) override { return buffer ? buffer->mapped : nullptr; }
	virtual const TextureDesc& Get_Desc(Texture* texture) const override { return texture->desc; }
	virtual CommandList* Begin_Frame() override;
	virtual void Submit_Frame() override;
	virtual void Wait_Idle() override;

	void Build_Sizes(const TriangleGeometry* geometry, uint32_t count, std::vector<VkAccelerationStructureGeometryKHR>& geos,
		std::vector<VkAccelerationStructureBuildRangeInfoKHR>& ranges, std::vector<uint32_t>& primitives);
};

//------------------------------------------------------------------------------------ device set-up
namespace
{
	VKAPI_ATTR VkBool32 VKAPI_CALL Debug_Callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
		const VkDebugUtilsMessengerCallbackDataEXT* data, void* user)
	{
		if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			static_cast<VulkanDevice*>(user)->Log("validation: %s", data->pMessage);
		return VK_FALSE;
	}

	bool Has_Extension(const std::vector<VkExtensionProperties>& list, const char* name)
	{
		for (const VkExtensionProperties& e : list)
			if (strcmp(e.extensionName, name) == 0)
				return true;
		return false;
	}

	struct Features
	{
		VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		VkPhysicalDeviceVulkan11Features v11 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
		VkPhysicalDeviceVulkan12Features v12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		VkPhysicalDeviceVulkan13Features v13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		VkPhysicalDeviceAccelerationStructureFeaturesKHR as = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
		VkPhysicalDeviceRayQueryFeaturesKHR rq = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
		void Link(bool rt)
		{
			f2.pNext = &v11;
			v11.pNext = &v12;
			v12.pNext = &v13;
			v13.pNext = rt ? (void*)&as : nullptr;
			as.pNext = &rq;
			rq.pNext = nullptr;
		}
	};
}

bool VulkanDevice::Init()
{
	if (!Load_Library())
	{
		Log("no Vulkan loader on this system");
		return false;
	}
	uint32_t apiVersion = VK_API_VERSION_1_0;
	if (vkEnumerateInstanceVersion)
		vkEnumerateInstanceVersion(&apiVersion);
	if (apiVersion < VK_API_VERSION_1_3)
	{
		Log("the Vulkan loader is %u.%u, 1.3 is needed", VK_API_VERSION_MAJOR(apiVersion), VK_API_VERSION_MINOR(apiVersion));
		return false;
	}

	// instance
	std::vector<const char*> layers, extensions;
	if (options.validation)
	{
		uint32_t n = 0;
		vkEnumerateInstanceLayerProperties(&n, nullptr);
		std::vector<VkLayerProperties> list(n);
		vkEnumerateInstanceLayerProperties(&n, list.data());
		for (const VkLayerProperties& l : list)
			if (strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0)
				layers.push_back("VK_LAYER_KHRONOS_validation");
		if (layers.empty())
			Log("validation asked for, but VK_LAYER_KHRONOS_validation is not installed");
	}
	{
		uint32_t n = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
		std::vector<VkExtensionProperties> list(n);
		vkEnumerateInstanceExtensionProperties(nullptr, &n, list.data());
		if (Has_Extension(list, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			debugUtils = true;
		}
	}
	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.pApplicationName = options.applicationName;
	app.pEngineName = "WW3D2 RHI";
	app.apiVersion = VK_API_VERSION_1_3;
	VkDebugUtilsMessengerCreateInfoEXT dbg = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	dbg.pfnUserCallback = Debug_Callback;
	dbg.pUserData = this;
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pNext = (!layers.empty() && debugUtils) ? &dbg : nullptr;
	ici.pApplicationInfo = &app;
	ici.enabledLayerCount = (uint32_t)layers.size();
	ici.ppEnabledLayerNames = layers.data();
	ici.enabledExtensionCount = (uint32_t)extensions.size();
	ici.ppEnabledExtensionNames = extensions.data();
	if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS)
	{
		Log("vkCreateInstance failed");
		return false;
	}
	Load_Instance(instance);
	if (!layers.empty() && debugUtils && vkCreateDebugUtilsMessengerEXT)
		vkCreateDebugUtilsMessengerEXT(instance, &dbg, nullptr, &messenger);

	// the physical device: Vulkan 1.3 with dynamic rendering, synchronization2 and buffer device
	// addresses; ray queries when there; discrete, then integrated, then anything
	uint32_t count = 0;
	vkEnumeratePhysicalDevices(instance, &count, nullptr);
	std::vector<VkPhysicalDevice> devices(count);
	vkEnumeratePhysicalDevices(instance, &count, devices.data());
	int best = -1, bestScore = -1;
	bool bestRT = false;
	for (uint32_t i = 0; i < count; ++i)
	{
		VkPhysicalDeviceProperties p;
		vkGetPhysicalDeviceProperties(devices[i], &p);
		uint32_t n = 0;
		vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &n, nullptr);
		std::vector<VkExtensionProperties> ext(n);
		vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &n, ext.data());
		bool rt = Has_Extension(ext, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) && Has_Extension(ext, VK_KHR_RAY_QUERY_EXTENSION_NAME)
			&& Has_Extension(ext, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		Features f;
		f.Link(rt);
		vkGetPhysicalDeviceFeatures2(devices[i], &f.f2);
		rt = rt && f.as.accelerationStructure && f.rq.rayQuery;
		const char* why = nullptr;
		if (p.apiVersion < VK_API_VERSION_1_3)
			why = "Vulkan before 1.3";
		else if (!f.v13.dynamicRendering || !f.v13.synchronization2 || !f.v12.bufferDeviceAddress || !f.v12.timelineSemaphore)
			why = "no dynamic rendering, synchronization2, buffer device address or timeline semaphores";
		else if (options.requireRayQuery && !rt)
			why = "no ray queries";
		else if (!options.allowSoftware && p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
			why = "a CPU device";
		Log("device %u: %s (%u.%u), %s%s", i, p.deviceName, VK_API_VERSION_MAJOR(p.apiVersion), VK_API_VERSION_MINOR(p.apiVersion),
			why ? "not usable: " : (rt ? "ray queries" : "no ray queries"), why ? why : "");
		if (why)
			continue;
		int score = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 4 : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 3
			: p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? 1 : 2;
		if ((int)i == options.deviceIndex)
			score = 100;
		if (score > bestScore)
		{
			best = (int)i;
			bestScore = score;
			bestRT = rt;
		}
	}
	if (best < 0)
	{
		Log("no usable Vulkan device");
		return false;
	}
	phys = devices[best];
	rayQuery = bestRT;
	vkGetPhysicalDeviceProperties(phys, &props);
	vkGetPhysicalDeviceMemoryProperties(phys, &allocator.props);

	// the queue: graphics and compute
	uint32_t qn = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, nullptr);
	std::vector<VkQueueFamilyProperties> families(qn);
	vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, families.data());
	queueFamily = UINT32_MAX;
	for (uint32_t i = 0; i < qn; ++i)
	{
		const VkQueueFlags want = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
		if ((families[i].queueFlags & want) == want)
		{
			queueFamily = i;
			break;
		}
	}
	if (queueFamily == UINT32_MAX)
	{
		Log("no queue with graphics and compute");
		return false;
	}
	float priority = 1.0f;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qci.queueFamilyIndex = queueFamily;
	qci.queueCount = 1;
	qci.pQueuePriorities = &priority;

	Features supported;
	supported.Link(rayQuery);
	vkGetPhysicalDeviceFeatures2(phys, &supported.f2);
	Features enable;
	enable.Link(rayQuery);
	enable.f2.features.samplerAnisotropy = supported.f2.features.samplerAnisotropy;
	enable.f2.features.textureCompressionBC = supported.f2.features.textureCompressionBC;
	enable.f2.features.fillModeNonSolid = supported.f2.features.fillModeNonSolid;
	enable.f2.features.geometryShader = supported.f2.features.geometryShader;	// SV_PrimitiveID in pixel shaders
	enable.f2.features.independentBlend = supported.f2.features.independentBlend;
	enable.f2.features.shaderInt16 = supported.f2.features.shaderInt16;
	enable.v11.shaderDrawParameters = supported.v11.shaderDrawParameters;
	enable.v12.bufferDeviceAddress = VK_TRUE;
	enable.v12.timelineSemaphore = VK_TRUE;
	enable.v12.scalarBlockLayout = supported.v12.scalarBlockLayout;
	enable.v12.descriptorIndexing = supported.v12.descriptorIndexing;
	enable.v13.dynamicRendering = VK_TRUE;
	enable.v13.synchronization2 = VK_TRUE;
	enable.v13.maintenance4 = supported.v13.maintenance4;
	if (rayQuery)
	{
		enable.as.accelerationStructure = VK_TRUE;
		enable.rq.rayQuery = VK_TRUE;
	}
	std::vector<const char*> deviceExtensions;
	if (rayQuery)
	{
		deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		deviceExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
		deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	}
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	dci.pNext = &enable.f2;
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledExtensionCount = (uint32_t)deviceExtensions.size();
	dci.ppEnabledExtensionNames = deviceExtensions.data();
	if (vkCreateDevice(phys, &dci, nullptr, &device) != VK_SUCCESS)
	{
		Log("vkCreateDevice failed");
		return false;
	}
	if (!Load_Device(device))
	{
		Log("the device lacks a Vulkan 1.3 entry point");
		return false;
	}
	vkGetDeviceQueue(device, queueFamily, 0, &queue);
	allocator.device = device;

	return Init_Common();
}

bool VulkanDevice::Init_Common()
{
	// capabilities
	snprintf(caps.deviceName, sizeof(caps.deviceName), "%s", props.deviceName);
	caps.rayQuery = rayQuery;
	caps.software = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
	caps.external = external;
	allocator.deviceAddresses = bufferDeviceAddress;
	caps.maxTextureSize = props.limits.maxImageDimension2D;
	caps.uniformAlignment = props.limits.minUniformBufferOffsetAlignment;
	Log("%s %s, Vulkan %u.%u, %s", external ? "sharing" : "using", props.deviceName, VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion),
		rayQuery ? "ray queries on" : "no ray queries");

	// the binding table
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	const VkShaderStageFlags all = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
	auto add = [&](uint32_t base, uint32_t n, VkDescriptorType type)
	{
		for (uint32_t i = 0; i < n; ++i)
			bindings.push_back(VkDescriptorSetLayoutBinding{ base + i, type, 1, all, nullptr });
	};
	add(UNIFORM_BASE, UNIFORM_COUNT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	add(TEXTURE_BASE, TEXTURE_COUNT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	add(STORAGE_BUFFER_BASE, STORAGE_BUFFER_COUNT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	add(STORAGE_IMAGE_BASE, STORAGE_IMAGE_COUNT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	if (rayQuery)
		add(ACCEL_BINDING, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	VkDescriptorSetLayoutCreateInfo lci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = (uint32_t)bindings.size();
	lci.pBindings = bindings.data();
	if (vkCreateDescriptorSetLayout(device, &lci, nullptr, &setLayout) != VK_SUCCESS)
		return false;
	VkPushConstantRange push = { all, 0, PUSH_CONSTANT_SIZE };
	VkPipelineLayoutCreateInfo pli = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &setLayout;
	pli.pushConstantRangeCount = 1;
	pli.pPushConstantRanges = &push;
	if (vkCreatePipelineLayout(device, &pli, nullptr, &pipelineLayout) != VK_SUCCESS)
		return false;
	VkPipelineCacheCreateInfo pcci = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
	vkCreatePipelineCache(device, &pcci, nullptr, &pipelineCache);

	// the frame timeline
	VkSemaphoreTypeCreateInfo stci = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
	stci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	sci.pNext = &stci;
	if (vkCreateSemaphore(device, &sci, nullptr, &timeline) != VK_SUCCESS)
		return false;

	// the frames
	for (FrameData& f : frames)
	{
		VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		pci.queueFamilyIndex = queueFamily;
		if (vkCreateCommandPool(device, &pci, nullptr, &f.pool) != VK_SUCCESS)
			return false;
		VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cai.commandPool = f.pool;
		cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cai.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(device, &cai, &f.cmd) != VK_SUCCESS)
			return false;
		VkDescriptorPool pool = Create_Descriptor_Pool();
		if (pool == VK_NULL_HANDLE)
			return false;
		f.descriptors.push_back(pool);
		BufferDesc ud;
		ud.size = UNIFORM_RING_SIZE;
		ud.usage = BUFFER_UNIFORM;
		ud.memory = Memory::UPLOAD;
		ud.name = "uniform ring";
		f.uniforms.buffer = Create_Buffer_Internal(ud);
		BufferDesc sd;
		sd.size = STAGING_RING_SIZE;
		sd.usage = BUFFER_COPY_SRC | BUFFER_STORAGE | (rayQuery ? (uint32_t)BUFFER_ACCEL_INPUT : 0u);
		sd.memory = Memory::UPLOAD;
		sd.name = "staging ring";
		f.staging.buffer = Create_Buffer_Internal(sd);
		if (f.uniforms.buffer == nullptr || f.staging.buffer == nullptr)
			return false;
	}
	cmdList.dev = this;
	return true;
}

bool VulkanDevice::Init_External(const VulkanExternal& e)
{
	external = true;
	shared = e;
	if (e.instance == nullptr || e.physicalDevice == nullptr || e.device == nullptr || e.queue == nullptr)
		return false;
	if (!Load_Library())
	{
		Log("no Vulkan loader on this system");
		return false;
	}
	instance = (VkInstance)e.instance;
	phys = (VkPhysicalDevice)e.physicalDevice;
	device = (VkDevice)e.device;
	queue = (VkQueue)e.queue;
	queueFamily = e.queueFamily;
	Load_Instance(instance);
	vkGetPhysicalDeviceProperties(phys, &props);
	if (props.apiVersion < VK_API_VERSION_1_3)
	{
		Log("the shared device is Vulkan %u.%u, 1.3 is needed", VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion));
		return false;
	}
	vkGetPhysicalDeviceMemoryProperties(phys, &allocator.props);
	if (!Load_Device(device))
	{
		Log("the shared device lacks a Vulkan 1.3 entry point");
		return false;
	}
	// the extension's entry points are there only when the device was made with it
	rayQuery = e.rayQuery && vkCreateAccelerationStructureKHR && vkDestroyAccelerationStructureKHR && vkCmdBuildAccelerationStructuresKHR
		&& vkGetAccelerationStructureBuildSizesKHR && vkGetAccelerationStructureDeviceAddressKHR;
	if (e.rayQuery && !rayQuery)
		Log("the shared device was made without acceleration structures: no ray queries");
	bufferDeviceAddress = rayQuery;
	debugUtils = false;
	allocator.device = device;
	return Init_Common();
}

void VulkanDevice::Wait_Timeline(uint64_t value)
{
	if (timeline == VK_NULL_HANDLE || value == 0)
		return;
	VkSemaphoreWaitInfo wi = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
	wi.semaphoreCount = 1;
	wi.pSemaphores = &timeline;
	wi.pValues = &value;
	vkWaitSemaphores(device, &wi, UINT64_MAX);
}

VkDescriptorPool VulkanDevice::Create_Descriptor_Pool()
{
	const uint32_t n = DESCRIPTOR_SETS_PER_POOL;
	std::vector<VkDescriptorPoolSize> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, n * 4 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, n * 8 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, n },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, n / 4 },
	};
	if (rayQuery)
		sizes.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, n / 4 });
	VkDescriptorPoolCreateInfo dpci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	dpci.maxSets = n;
	dpci.poolSizeCount = (uint32_t)sizes.size();
	dpci.pPoolSizes = sizes.data();
	VkDescriptorPool pool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(device, &dpci, nullptr, &pool) != VK_SUCCESS)
		return VK_NULL_HANDLE;
	return pool;
}

void VulkanDevice::Shutdown()
{
	if (device != VK_NULL_HANDLE)
	{
		// a shared device's queue is the other renderer's: wait for this device's own work only
		if (external)
			Wait_Timeline(timelineValue);
		else
			vkDeviceWaitIdle(device);
		Collect(UINT64_MAX);
		for (FrameData& f : frames)
		{
			Retire(f);
			if (f.uniforms.buffer) Free_Buffer(f.uniforms.buffer);
			if (f.staging.buffer) Free_Buffer(f.staging.buffer);
			for (VkDescriptorPool p : f.descriptors)
				vkDestroyDescriptorPool(device, p, nullptr);
			if (f.pool) vkDestroyCommandPool(device, f.pool, nullptr);
			f = FrameData();
		}
		if (timeline) vkDestroySemaphore(device, timeline, nullptr);
		if (pipelineCache) vkDestroyPipelineCache(device, pipelineCache, nullptr);
		if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		if (setLayout) vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
		allocator.Shutdown();
		if (!external)
			vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}
	if (instance != VK_NULL_HANDLE)
	{
		if (!external)
		{
			if (messenger && vkDestroyDebugUtilsMessengerEXT)
				vkDestroyDebugUtilsMessengerEXT(instance, messenger, nullptr);
			vkDestroyInstance(instance, nullptr);
		}
		instance = VK_NULL_HANDLE;
	}
}

//---------------------------------------------------------------------------------------- resources
Buffer* VulkanDevice::Create_Buffer_Internal(const BufferDesc& desc)
{
	VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = std::max<uint64_t>(desc.size, 4);
	bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if (bufferDeviceAddress)
		bci.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	if (desc.usage & BUFFER_VERTEX) bci.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (desc.usage & BUFFER_INDEX) bci.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (desc.usage & BUFFER_UNIFORM) bci.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (desc.usage & BUFFER_STORAGE) bci.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (desc.usage & BUFFER_INDIRECT) bci.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	if ((desc.usage & BUFFER_ACCEL_INPUT) && rayQuery)
		bci.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
	if ((desc.usage & BUFFER_ACCEL_STORAGE) && rayQuery)
		bci.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
	Buffer* b = new Buffer;
	b->desc = desc;
	if (vkCreateBuffer(device, &bci, nullptr, &b->buffer) != VK_SUCCESS)
	{
		delete b;
		return nullptr;
	}
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(device, b->buffer, &req);
	if (!allocator.Allocate(req, desc.memory, true, true, b->alloc)
		|| vkBindBufferMemory(device, b->buffer, b->alloc.memory, b->alloc.offset) != VK_SUCCESS)
	{
		Log("out of memory for a buffer of %llu bytes (%s)", (unsigned long long)desc.size, desc.name ? desc.name : "unnamed");
		vkDestroyBuffer(device, b->buffer, nullptr);
		allocator.Free(b->alloc);
		delete b;
		return nullptr;
	}
	b->mapped = b->alloc.mapped;
	if (bufferDeviceAddress)
	{
		VkBufferDeviceAddressInfo ai = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
		ai.buffer = b->buffer;
		b->address = vkGetBufferDeviceAddress(device, &ai);
	}
	Name(VK_OBJECT_TYPE_BUFFER, (uint64_t)b->buffer, desc.name);
	return b;
}

Buffer* VulkanDevice::Create_Buffer(const BufferDesc& desc, const void* initialData)
{
	Buffer* b = Create_Buffer_Internal(desc);
	if (b != nullptr && initialData != nullptr)
	{
		if (b->mapped != nullptr)
			memcpy(b->mapped, initialData, (size_t)desc.size);
		else
		{
			// through the frame's staging ring: the copy runs before the frame's other commands
			CommandList* c = frameOpen ? (CommandList*)&cmdList : nullptr;
			if (c == nullptr)
			{
				Begin_Frame();
				cmdList.Update_Buffer(b, initialData, (size_t)desc.size, 0);
				Submit_Frame();
			}
			else
				c->Update_Buffer(b, initialData, (size_t)desc.size, 0);
		}
	}
	return b;
}

void VulkanDevice::Free_Buffer(Buffer* b)
{
	vkDestroyBuffer(device, b->buffer, nullptr);
	allocator.Free(b->alloc);
	delete b;
}

Texture* VulkanDevice::Create_Texture(const TextureDesc& desc)
{
	Texture* t = new Texture;
	t->desc = desc;
	t->layers = desc.type == TextureType::TEX_CUBE ? 6 : 1;
	const bool depth = Format_Is_Depth(desc.format);
	t->aspect = depth ? (VK_IMAGE_ASPECT_DEPTH_BIT | (Has_Stencil(desc.format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)) : VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ici.flags = desc.type == TextureType::TEX_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	ici.imageType = desc.type == TextureType::TEX_3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	ici.format = To_Vk(desc.format);
	ici.extent = { desc.width, desc.height, desc.type == TextureType::TEX_3D ? desc.depth : 1 };
	ici.mipLevels = std::max(1u, desc.mips);
	ici.arrayLayers = t->layers;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (desc.usage & TEXTURE_SAMPLED) ici.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if (desc.usage & TEXTURE_RENDER_TARGET) ici.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (desc.usage & TEXTURE_DEPTH_STENCIL) ici.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if (desc.usage & TEXTURE_STORAGE) ici.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (vkCreateImage(device, &ici, nullptr, &t->image) != VK_SUCCESS)
	{
		Log("vkCreateImage failed (%ux%u, format %d)", desc.width, desc.height, (int)desc.format);
		delete t;
		return nullptr;
	}
	VkMemoryRequirements req;
	vkGetImageMemoryRequirements(device, t->image, &req);
	if (!allocator.Allocate(req, Memory::GPU, false, false, t->alloc)
		|| vkBindImageMemory(device, t->image, t->alloc.memory, t->alloc.offset) != VK_SUCCESS)
	{
		Log("out of memory for a texture (%ux%u)", desc.width, desc.height);
		vkDestroyImage(device, t->image, nullptr);
		allocator.Free(t->alloc);
		delete t;
		return nullptr;
	}
	VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	vci.image = t->image;
	vci.viewType = desc.type == TextureType::TEX_CUBE ? VK_IMAGE_VIEW_TYPE_CUBE : desc.type == TextureType::TEX_3D ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
	vci.format = ici.format;
	// a depth texture is sampled by its depth (a view may name one aspect when sampled)
	vci.subresourceRange = { depth ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT : t->aspect, 0, ici.mipLevels, 0, t->layers };
	vkCreateImageView(device, &vci, nullptr, &t->view);
	if (desc.usage & (TEXTURE_RENDER_TARGET | TEXTURE_DEPTH_STENCIL))
	{
		for (uint32_t layer = 0; layer < t->layers; ++layer)
		{
			VkImageViewCreateInfo tv = vci;
			tv.viewType = VK_IMAGE_VIEW_TYPE_2D;
			tv.subresourceRange = { t->aspect, 0, 1, layer, 1 };
			VkImageView view = VK_NULL_HANDLE;
			vkCreateImageView(device, &tv, nullptr, &view);
			t->targetViews.push_back(view);
		}
	}
	Name(VK_OBJECT_TYPE_IMAGE, (uint64_t)t->image, desc.name);
	return t;
}

void VulkanDevice::Free_Texture(Texture* t)
{
	for (VkImageView v : t->targetViews)
		vkDestroyImageView(device, v, nullptr);
	if (t->view)
		vkDestroyImageView(device, t->view, nullptr);
	if (!t->imported)
	{
		vkDestroyImage(device, t->image, nullptr);
		allocator.Free(t->alloc);
	}
	delete t;
}

Texture* VulkanDevice::Import_Texture(const ExternalTexture& e)
{
	if (!external || e.image == 0)
		return nullptr;
	Texture* t = new Texture;
	t->imported = true;
	t->image = (VkImage)e.image;
	t->desc.width = e.width;
	t->desc.height = e.height;
	t->desc.mips = 1;
	t->desc.format = From_Vk((VkFormat)e.vkFormat);
	t->desc.usage = e.usage;
	t->desc.name = e.name;
	const VkFormat format = (VkFormat)e.vkFormat;
	const bool depth = format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D24_UNORM_S8_UINT
		|| format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_X8_D24_UNORM_PACK32 || format == VK_FORMAT_D16_UNORM_S8_UINT;
	const bool stencil = format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D16_UNORM_S8_UINT;
	t->aspect = depth ? (VK_IMAGE_ASPECT_DEPTH_BIT | (stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)) : VK_IMAGE_ASPECT_COLOR_BIT;
	t->layout = t->homeLayout = (VkImageLayout)e.layout;
	// the other renderer may have written it: the first use waits for everything before it
	t->stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	t->access = VK_ACCESS_2_MEMORY_WRITE_BIT;
	if (e.usage & (TEXTURE_SAMPLED | TEXTURE_STORAGE))
	{
		VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		vci.image = t->image;
		vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vci.format = format;
		vci.subresourceRange = { depth ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT : t->aspect, 0, 1, 0, 1 };
		VkImageViewUsageCreateInfo usage = { VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
		usage.usage = ((e.usage & TEXTURE_SAMPLED) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0) | ((e.usage & TEXTURE_STORAGE) ? VK_IMAGE_USAGE_STORAGE_BIT : 0);
		vci.pNext = &usage;
		if (vkCreateImageView(device, &vci, nullptr, &t->view) != VK_SUCCESS)
		{
			Log("imported texture %s: no view", e.name ? e.name : "");
			delete t;
			return nullptr;
		}
	}
	return t;
}

Sampler* VulkanDevice::Create_Sampler(const SamplerDesc& d)
{
	VkSamplerCreateInfo sci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sci.magFilter = d.magFilter == Filter::LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	sci.minFilter = d.minFilter == Filter::LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	sci.mipmapMode = d.mipFilter == Filter::LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sci.addressModeU = To_Vk(d.u);
	sci.addressModeV = To_Vk(d.v);
	sci.addressModeW = To_Vk(d.w);
	sci.mipLodBias = d.mipBias;
	sci.anisotropyEnable = d.maxAnisotropy > 1.0f ? VK_TRUE : VK_FALSE;
	sci.maxAnisotropy = std::min(d.maxAnisotropy, props.limits.maxSamplerAnisotropy);
	sci.compareEnable = d.compare ? VK_TRUE : VK_FALSE;
	sci.compareOp = To_Vk(d.compareOp);
	sci.minLod = 0.0f;
	sci.maxLod = d.maxLod;
	sci.borderColor = (d.u == Address::BORDER_WHITE || d.v == Address::BORDER_WHITE) ? VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE : VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	Sampler* s = new Sampler;
	if (vkCreateSampler(device, &sci, nullptr, &s->sampler) != VK_SUCCESS)
	{
		delete s;
		return nullptr;
	}
	return s;
}

Shader* VulkanDevice::Create_Shader(const ShaderDesc& desc)
{
	if (desc.code == nullptr || desc.size < 4 || (desc.size & 3) != 0 || *(const uint32_t*)desc.code != 0x07230203)
	{
		Log("shader %s: not SPIR-V", desc.name ? desc.name : "");
		return nullptr;
	}
	VkShaderModuleCreateInfo smci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	smci.codeSize = desc.size;
	smci.pCode = (const uint32_t*)desc.code;
	Shader* s = new Shader;
	s->stage = desc.stage;
	s->entry = desc.entry ? desc.entry : "main";
	if (vkCreateShaderModule(device, &smci, nullptr, &s->module) != VK_SUCCESS)
	{
		delete s;
		return nullptr;
	}
	Name(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)s->module, desc.name);
	return s;
}

Pipeline* VulkanDevice::Create_Graphics_Pipeline(const GraphicsPipelineDesc& d)
{
	if (d.vs == nullptr)
		return nullptr;
	VkPipelineShaderStageCreateInfo stages[2] = {};
	uint32_t stageCount = 0;
	stages[stageCount++] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, d.vs->module, d.vs->entry.c_str(), nullptr };
	if (d.ps != nullptr)
		stages[stageCount++] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, d.ps->module, d.ps->entry.c_str(), nullptr };

	VkVertexInputBindingDescription bindings[MAX_VERTEX_BINDINGS];
	VkVertexInputAttributeDescription attributes[MAX_VERTEX_ATTRIBUTES];
	for (uint32_t i = 0; i < d.bindingCount; ++i)
		bindings[i] = { d.bindings[i].binding, d.bindings[i].stride, d.bindings[i].perInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX };
	for (uint32_t i = 0; i < d.attributeCount; ++i)
		attributes[i] = { d.attributes[i].location, d.attributes[i].binding, To_Vk(d.attributes[i].format), d.attributes[i].offset };
	VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = d.bindingCount;
	vi.pVertexBindingDescriptions = bindings;
	vi.vertexAttributeDescriptionCount = d.attributeCount;
	vi.pVertexAttributeDescriptions = attributes;

	VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	static const VkPrimitiveTopology topologies[] = { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_POINT_LIST };
	ia.topology = topologies[(int)d.topology];

	VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = d.raster.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	rs.cullMode = d.raster.cull == Cull::NONE ? VK_CULL_MODE_NONE : d.raster.cull == Cull::FRONT ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
	rs.frontFace = d.raster.frontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
	rs.depthBiasEnable = (d.raster.depthBias != 0.0f || d.raster.slopeBias != 0.0f) ? VK_TRUE : VK_FALSE;
	rs.depthBiasConstantFactor = d.raster.depthBias;
	rs.depthBiasSlopeFactor = d.raster.slopeBias;
	rs.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = (d.depthFormat != Format::Unknown && d.depth.test) ? VK_TRUE : VK_FALSE;
	ds.depthWriteEnable = (d.depthFormat != Format::Unknown && d.depth.write) ? VK_TRUE : VK_FALSE;
	ds.depthCompareOp = To_Vk(d.depth.func);

	VkPipelineColorBlendAttachmentState blends[MAX_COLOR_TARGETS] = {};
	for (uint32_t i = 0; i < d.colorCount; ++i)
	{
		const BlendState& b = d.blend[i];
		blends[i].blendEnable = b.enable ? VK_TRUE : VK_FALSE;
		blends[i].srcColorBlendFactor = To_Vk(b.src);
		blends[i].dstColorBlendFactor = To_Vk(b.dst);
		blends[i].colorBlendOp = To_Vk(b.op);
		blends[i].srcAlphaBlendFactor = To_Vk(b.srcAlpha);
		blends[i].dstAlphaBlendFactor = To_Vk(b.dstAlpha);
		blends[i].alphaBlendOp = To_Vk(b.opAlpha);
		blends[i].colorWriteMask = b.writeMask;
	}
	VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cb.attachmentCount = d.colorCount;
	cb.pAttachments = blends;

	VkDynamicState dynamics[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dy = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dy.dynamicStateCount = 2;
	dy.pDynamicStates = dynamics;

	VkFormat colorFormats[MAX_COLOR_TARGETS];
	for (uint32_t i = 0; i < d.colorCount; ++i)
		colorFormats[i] = To_Vk(d.colorFormats[i]);
	VkPipelineRenderingCreateInfo rendering = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	rendering.colorAttachmentCount = d.colorCount;
	rendering.pColorAttachmentFormats = colorFormats;
	rendering.depthAttachmentFormat = To_Vk(d.depthFormat);
	rendering.stencilAttachmentFormat = Has_Stencil(d.depthFormat) ? To_Vk(d.depthFormat) : VK_FORMAT_UNDEFINED;

	VkGraphicsPipelineCreateInfo gci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	gci.pNext = &rendering;
	gci.stageCount = stageCount;
	gci.pStages = stages;
	gci.pVertexInputState = &vi;
	gci.pInputAssemblyState = &ia;
	gci.pViewportState = &vp;
	gci.pRasterizationState = &rs;
	gci.pMultisampleState = &ms;
	gci.pDepthStencilState = &ds;
	gci.pColorBlendState = &cb;
	gci.pDynamicState = &dy;
	gci.layout = pipelineLayout;
	Pipeline* p = new Pipeline;
	p->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &gci, nullptr, &p->pipeline) != VK_SUCCESS)
	{
		Log("graphics pipeline %s: creation failed", d.name ? d.name : "");
		delete p;
		return nullptr;
	}
	Name(VK_OBJECT_TYPE_PIPELINE, (uint64_t)p->pipeline, d.name);
	return p;
}

Pipeline* VulkanDevice::Create_Compute_Pipeline(const ComputePipelineDesc& d)
{
	if (d.cs == nullptr)
		return nullptr;
	VkComputePipelineCreateInfo cci = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	cci.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, d.cs->module, d.cs->entry.c_str(), nullptr };
	cci.layout = pipelineLayout;
	Pipeline* p = new Pipeline;
	p->bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
	if (vkCreateComputePipelines(device, pipelineCache, 1, &cci, nullptr, &p->pipeline) != VK_SUCCESS)
	{
		Log("compute pipeline %s: creation failed", d.name ? d.name : "");
		delete p;
		return nullptr;
	}
	Name(VK_OBJECT_TYPE_PIPELINE, (uint64_t)p->pipeline, d.name);
	return p;
}

void VulkanDevice::Build_Sizes(const TriangleGeometry* geometry, uint32_t count, std::vector<VkAccelerationStructureGeometryKHR>& geos,
	std::vector<VkAccelerationStructureBuildRangeInfoKHR>& ranges, std::vector<uint32_t>& primitives)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const TriangleGeometry& g = geometry[i];
		VkAccelerationStructureGeometryKHR geo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geo.flags = g.opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;
		VkAccelerationStructureGeometryTrianglesDataKHR& tri = geo.geometry.triangles;
		tri.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		tri.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		tri.vertexData.deviceAddress = g.vertices ? g.vertices->address + g.vertexOffset : 0;
		tri.vertexStride = g.vertexStride;
		tri.maxVertex = g.vertexCount > 0 ? g.vertexCount - 1 : 0;
		tri.indexType = g.indices ? (g.indices16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32) : VK_INDEX_TYPE_NONE_KHR;
		tri.indexData.deviceAddress = g.indices ? g.indices->address + g.indexOffset : 0;
		geos.push_back(geo);
		const uint32_t prims = (g.indices ? g.indexCount : g.vertexCount) / 3;
		primitives.push_back(prims);
		ranges.push_back(VkAccelerationStructureBuildRangeInfoKHR{ prims, 0, 0, 0 });
	}
}

AccelStruct* VulkanDevice::Create_Bottom_Level(const TriangleGeometry* geometry, uint32_t count)
{
	if (!rayQuery || geometry == nullptr || count == 0)
		return nullptr;
	std::vector<VkAccelerationStructureGeometryKHR> geos;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
	std::vector<uint32_t> primitives;
	Build_Sizes(geometry, count, geos, ranges, primitives);
	VkAccelerationStructureBuildGeometryInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	info.geometryCount = count;
	info.pGeometries = geos.data();
	VkAccelerationStructureBuildSizesInfoKHR sizes = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, primitives.data(), &sizes);

	AccelStruct* a = new AccelStruct;
	a->scratchSize = sizes.buildScratchSize;
	BufferDesc bd;
	bd.size = sizes.accelerationStructureSize;
	bd.usage = BUFFER_ACCEL_STORAGE;
	bd.name = "bottom-level acceleration structure";
	a->storage = Create_Buffer_Internal(bd);
	if (a->storage == nullptr)
	{
		delete a;
		return nullptr;
	}
	VkAccelerationStructureCreateInfoKHR aci = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	aci.buffer = a->storage->buffer;
	aci.size = sizes.accelerationStructureSize;
	aci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	vkCreateAccelerationStructureKHR(device, &aci, nullptr, &a->handle);
	VkAccelerationStructureDeviceAddressInfoKHR adi = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
	adi.accelerationStructure = a->handle;
	a->address = vkGetAccelerationStructureDeviceAddressKHR(device, &adi);
	return a;
}

AccelStruct* VulkanDevice::Create_Top_Level(uint32_t maxInstances)
{
	if (!rayQuery || maxInstances == 0)
		return nullptr;
	VkAccelerationStructureGeometryKHR geo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geo.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	VkAccelerationStructureBuildGeometryInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	info.geometryCount = 1;
	info.pGeometries = &geo;
	VkAccelerationStructureBuildSizesInfoKHR sizes = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, &maxInstances, &sizes);
	AccelStruct* a = new AccelStruct;
	a->topLevel = true;
	a->maxInstances = maxInstances;
	a->scratchSize = sizes.buildScratchSize;
	BufferDesc bd;
	bd.size = sizes.accelerationStructureSize;
	bd.usage = BUFFER_ACCEL_STORAGE;
	bd.name = "top-level acceleration structure";
	a->storage = Create_Buffer_Internal(bd);
	if (a->storage == nullptr)
	{
		delete a;
		return nullptr;
	}
	VkAccelerationStructureCreateInfoKHR aci = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	aci.buffer = a->storage->buffer;
	aci.size = sizes.accelerationStructureSize;
	aci.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	vkCreateAccelerationStructureKHR(device, &aci, nullptr, &a->handle);
	VkAccelerationStructureDeviceAddressInfoKHR adi = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
	adi.accelerationStructure = a->handle;
	a->address = vkGetAccelerationStructureDeviceAddressKHR(device, &adi);
	return a;
}

//------------------------------------------------------------------------------------------- frames
void VulkanDevice::Retire(FrameData& f)
{
	for (Buffer* b : f.extraStaging)
		Free_Buffer(b);
	f.extraStaging.clear();
}

void VulkanDevice::Collect(uint64_t completed)
{
	size_t kept = 0;
	for (size_t i = 0; i < garbage.size(); ++i)
	{
		const Garbage g = garbage[i];
		if (g.after > completed)
		{
			garbage[kept++] = g;
			continue;
		}
		switch (g.kind)
		{
			case Garbage::BUFFER: Free_Buffer((Buffer*)g.object); break;
			case Garbage::TEXTURE: Free_Texture((Texture*)g.object); break;
			case Garbage::SAMPLER: vkDestroySampler(device, ((Sampler*)g.object)->sampler, nullptr); delete (Sampler*)g.object; break;
			case Garbage::SHADER: vkDestroyShaderModule(device, ((Shader*)g.object)->module, nullptr); delete (Shader*)g.object; break;
			case Garbage::PIPELINE: vkDestroyPipeline(device, ((Pipeline*)g.object)->pipeline, nullptr); delete (Pipeline*)g.object; break;
			case Garbage::ACCEL:
			{
				AccelStruct* a = (AccelStruct*)g.object;
				vkDestroyAccelerationStructureKHR(device, a->handle, nullptr);
				Free_Buffer(a->storage);
				delete a;
				break;
			}
		}
	}
	garbage.resize(kept);
}

CommandList* VulkanDevice::Begin_Frame()
{
	if (frameOpen)
		return &cmdList;
	FrameData& f = Current();
	// wait until the GPU is done with this frame's last use
	if (f.completion > 0)
	{
		VkSemaphoreWaitInfo wi = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
		wi.semaphoreCount = 1;
		wi.pSemaphores = &timeline;
		wi.pValues = &f.completion;
		vkWaitSemaphores(device, &wi, UINT64_MAX);
	}
	Retire(f);
	uint64_t completed = 0;
	vkGetSemaphoreCounterValue(device, timeline, &completed);
	Collect(completed);
	vkResetCommandPool(device, f.pool, 0);
	for (VkDescriptorPool p : f.descriptors)
		vkResetDescriptorPool(device, p, 0);
	f.descriptorPool = 0;
	f.uniforms.used = 0;
	f.staging.used = 0;
	VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(f.cmd, &bi);
	cmdList.Reset(f.cmd, &f);
	cmdList.serial = ++frameSerial;
	if (external)
		cmdList.Global_Barrier();	// after everything the other renderer submitted before
	frameOpen = true;
	return &cmdList;
}

void VulkanDevice::Submit_Frame()
{
	if (!frameOpen)
		return;
	cmdList.End_Render_Pass();
	cmdList.Flush_Buffer_Barrier();
	if (external)
	{
		// the other renderer's images back in their layouts, and what comes after waits for this
		for (Texture* t : cmdList.importedUsed)
			cmdList.Transition(t, t->homeLayout, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
		cmdList.Global_Barrier();
		for (Texture* t : cmdList.importedUsed)
		{
			t->stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			t->access = VK_ACCESS_2_MEMORY_WRITE_BIT;
		}
		cmdList.importedUsed.clear();
	}
	FrameData& f = Current();
	vkEndCommandBuffer(f.cmd);
	f.completion = ++timelineValue;
	VkCommandBufferSubmitInfo cbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
	cbi.commandBuffer = f.cmd;
	VkSemaphoreSubmitInfo signal = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	signal.semaphore = timeline;
	signal.value = f.completion;
	signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 si = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
	si.commandBufferInfoCount = 1;
	si.pCommandBufferInfos = &cbi;
	si.signalSemaphoreInfoCount = 1;
	si.pSignalSemaphoreInfos = &signal;
	if (external && shared.lockQueue)
		shared.lockQueue(shared.user);
	const VkResult submitted = vkQueueSubmit2(queue, 1, &si, VK_NULL_HANDLE);
	if (external && shared.unlockQueue)
		shared.unlockQueue(shared.user);
	if (submitted != VK_SUCCESS)
		Log("vkQueueSubmit2 failed (%d)", (int)submitted);
	frameOpen = false;
	frameIndex = (frameIndex + 1) % FRAMES;
}

void VulkanDevice::Wait_Idle()
{
	if (frameOpen)
		Submit_Frame();
	if (external)
		Wait_Timeline(timelineValue);
	else
		vkDeviceWaitIdle(device);
	for (FrameData& f : frames)
		Retire(f);
	Collect(UINT64_MAX);
}

//------------------------------------------------------------------------------------ command lists
void VulkanCommandList::Reset(VkCommandBuffer c, FrameData* f)
{
	cmd = c;
	frame = f;
	passOpen = false;
	inPass = false;
	for (Binding& b : uniforms) b = Binding();
	for (Binding& b : textures) b = Binding();
	for (Binding& b : storageBuffers) b = Binding();
	for (Binding& b : storageImages) b = Binding();
	tlas = nullptr;
	dirtyGraphics = dirtyCompute = true;
	pipeline = nullptr;
	pendingStage = VK_PIPELINE_STAGE_2_NONE;
	pendingAccess = VK_ACCESS_2_NONE;
	workSinceBarrier = false;
}

void VulkanCommandList::Transition(Texture* t, VkImageLayout layout, VkPipelineStageFlags2 stage, VkAccessFlags2 access)
{
	if (t->imported && t->usedInFrame != serial)
	{
		t->usedInFrame = serial;
		importedUsed.push_back(t);
	}
	// reads after reads in the same layout need nothing
	if (t->layout == layout && !(t->access & WRITE_ACCESS) && !(access & WRITE_ACCESS))
	{
		t->stage |= stage;
		t->access |= access;
		return;
	}
	Suspend_Pass();
	VkImageMemoryBarrier2 b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	b.srcStageMask = t->stage ? t->stage : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	b.srcAccessMask = t->access;
	b.dstStageMask = stage;
	b.dstAccessMask = access;
	b.oldLayout = t->layout;
	b.newLayout = layout;
	b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.image = t->image;
	b.subresourceRange = { t->aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
	VkDependencyInfo di = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	di.imageMemoryBarrierCount = 1;
	di.pImageMemoryBarriers = &b;
	vkCmdPipelineBarrier2(cmd, &di);
	if (t->layout != layout)
		dirtyGraphics = dirtyCompute = true;	// descriptors name the layout
	t->layout = layout;
	t->stage = stage;
	t->access = access;
}

void VulkanCommandList::Flush_Buffer_Barrier()
{
	if (pendingStage == VK_PIPELINE_STAGE_2_NONE)
		return;
	Suspend_Pass();
	VkMemoryBarrier2 b = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	b.srcStageMask = pendingStage;
	b.srcAccessMask = pendingAccess;
	b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	VkDependencyInfo di = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	di.memoryBarrierCount = 1;
	di.pMemoryBarriers = &b;
	vkCmdPipelineBarrier2(cmd, &di);
	pendingStage = VK_PIPELINE_STAGE_2_NONE;
	pendingAccess = VK_ACCESS_2_NONE;
	workSinceBarrier = false;
}

void VulkanCommandList::Global_Barrier()
{
	Suspend_Pass();
	VkMemoryBarrier2 b = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	VkDependencyInfo di = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	di.memoryBarrierCount = 1;
	di.pMemoryBarriers = &b;
	vkCmdPipelineBarrier2(cmd, &di);
	pendingStage = VK_PIPELINE_STAGE_2_NONE;
	pendingAccess = VK_ACCESS_2_NONE;
	workSinceBarrier = false;
}

void VulkanCommandList::Before_Transfer_Write()
{
	// a copy or a build into a buffer waits for the draws and dispatches before it, which may read
	// what it overwrites (Direct3D's order: an update lands between the draws around it)
	Suspend_Pass();
	if (!workSinceBarrier && pendingStage == VK_PIPELINE_STAGE_2_NONE)
		return;
	VkMemoryBarrier2 b = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	VkDependencyInfo di = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	di.memoryBarrierCount = 1;
	di.pMemoryBarriers = &b;
	vkCmdPipelineBarrier2(cmd, &di);
	pendingStage = VK_PIPELINE_STAGE_2_NONE;
	pendingAccess = VK_ACCESS_2_NONE;
	workSinceBarrier = false;
}

void VulkanCommandList::Suspend_Pass()
{
	if (!inPass)
		return;
	vkCmdEndRendering(cmd);
	inPass = false;
}

void VulkanCommandList::Begin_Render_Pass(const RenderPassDesc& desc)
{
	End_Render_Pass();
	Begin_Rendering(desc);
	passOpen = true;
	// the whole target by default
	uint32_t width = 0, height = 0;
	if (desc.colorCount > 0)
	{
		width = desc.colors[0].texture->desc.width;
		height = desc.colors[0].texture->desc.height;
	}
	else if (desc.depth)
	{
		width = desc.depth->desc.width;
		height = desc.depth->desc.height;
	}
	Set_Viewport(0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
	Set_Scissor(0, 0, width, height);
}

void VulkanCommandList::Begin_Rendering(const RenderPassDesc& desc)
{
	Flush_Buffer_Barrier();
	for (uint32_t i = 0; i < desc.colorCount; ++i)
		Transition(desc.colors[i].texture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
	if (desc.depth)
		Transition(desc.depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
	// textures already bound: sampled before rendering starts (a barrier can't sit inside it)
	for (Binding& b : textures)
		if (b.set && b.texture && !Is_Target(b.texture, desc))
			Transition(b.texture, Format_Is_Depth(b.texture->desc.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

	VkRenderingAttachmentInfo colors[MAX_COLOR_TARGETS] = {};
	uint32_t width = 0, height = 0;
	for (uint32_t i = 0; i < desc.colorCount; ++i)
	{
		const ColorTarget& c = desc.colors[i];
		colors[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colors[i].imageView = c.texture->targetViews.empty() ? c.texture->view : c.texture->targetViews[0];
		colors[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colors[i].loadOp = c.load == Load::CLEAR ? VK_ATTACHMENT_LOAD_OP_CLEAR : c.load == Load::LOAD ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colors[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		memcpy(colors[i].clearValue.color.float32, c.clear, sizeof(c.clear));
		width = c.texture->desc.width;
		height = c.texture->desc.height;
	}
	VkRenderingAttachmentInfo depth = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	VkRenderingAttachmentInfo stencil = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	if (desc.depth)
	{
		depth.imageView = desc.depth->targetViews.empty() ? desc.depth->view : desc.depth->targetViews[0];
		depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth.loadOp = desc.depthLoad == Load::CLEAR ? VK_ATTACHMENT_LOAD_OP_CLEAR : desc.depthLoad == Load::LOAD ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth.clearValue.depthStencil = { desc.clearDepth, desc.clearStencil };
		stencil = depth;
		if (width == 0)
		{
			width = desc.depth->desc.width;
			height = desc.depth->desc.height;
		}
	}
	VkRenderingInfo ri = { VK_STRUCTURE_TYPE_RENDERING_INFO };
	ri.renderArea = { { 0, 0 }, { width, height } };
	ri.layerCount = 1;
	ri.colorAttachmentCount = desc.colorCount;
	ri.pColorAttachments = colors;
	ri.pDepthAttachment = desc.depth ? &depth : nullptr;
	ri.pStencilAttachment = (desc.depth && Has_Stencil(desc.depth->desc.format)) ? &stencil : nullptr;
	vkCmdBeginRendering(cmd, &ri);
	// resuming keeps what was drawn (the clear happened when the pass began)
	pass = desc;
	for (uint32_t i = 0; i < pass.colorCount; ++i)
		pass.colors[i].load = Load::LOAD;
	pass.depthLoad = Load::LOAD;
	inPass = true;
}

void VulkanCommandList::End_Render_Pass()
{
	Suspend_Pass();
	passOpen = false;
}

void VulkanCommandList::Set_Viewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
	// Direct3D's convention (y down, top-left origin) through a negative height
	VkViewport vp = { x, y + height, width, -height, minDepth, maxDepth };
	vkCmdSetViewport(cmd, 0, 1, &vp);
}

void VulkanCommandList::Set_Scissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
	VkRect2D r = { { x, y }, { width, height } };
	vkCmdSetScissor(cmd, 0, 1, &r);
}

void VulkanCommandList::Set_Pipeline(Pipeline* p)
{
	// every pipeline has the same layout, so the bound descriptor sets stay valid
	pipeline = p;
	if (p)
		vkCmdBindPipeline(cmd, p->bindPoint, p->pipeline);
}

void VulkanCommandList::Set_Vertex_Buffer(uint32_t binding, Buffer* buffer, uint64_t offset)
{
	if (buffer == nullptr)
		return;
	VkDeviceSize o = offset;
	vkCmdBindVertexBuffers(cmd, binding, 1, &buffer->buffer, &o);
}

void VulkanCommandList::Set_Index_Buffer(Buffer* buffer, uint64_t offset, bool indices16)
{
	if (buffer == nullptr)
		return;
	vkCmdBindIndexBuffer(cmd, buffer->buffer, offset, indices16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}

void VulkanCommandList::Set_Constants(uint32_t slot, const void* data, size_t size)
{
	if (slot >= UNIFORM_COUNT || data == nullptr || size == 0)
		return;
	Ring& ring = frame->uniforms;
	VkDeviceSize start = Align_Up(ring.used, dev->caps.uniformAlignment);
	if (start + size > ring.buffer->desc.size)
	{
		dev->Log("the uniform ring is full (%llu bytes this frame)", (unsigned long long)ring.used);
		return;
	}
	memcpy((char*)ring.buffer->mapped + start, data, size);
	ring.used = start + size;
	Binding& b = uniforms[slot];
	b.set = true;
	b.buffer = ring.buffer->buffer;
	b.offset = start;
	b.range = size;
	dirtyGraphics = dirtyCompute = true;
}

void VulkanCommandList::Set_Push_Constants(const void* data, uint32_t size, uint32_t offset)
{
	if (data == nullptr || offset >= PUSH_CONSTANT_SIZE)
		return;
	const VkShaderStageFlags all = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
	vkCmdPushConstants(cmd, dev->pipelineLayout, all, offset, std::min(size, PUSH_CONSTANT_SIZE - offset) & ~3u, data);
}

void VulkanCommandList::Set_Texture(uint32_t slot, Texture* texture, Sampler* sampler)
{
	if (slot >= TEXTURE_COUNT)
		return;
	Binding& b = textures[slot];
	b.set = texture != nullptr && sampler != nullptr;
	b.texture = texture;
	b.sampler = sampler ? sampler->sampler : VK_NULL_HANDLE;
	dirtyGraphics = dirtyCompute = true;
}

void VulkanCommandList::Set_Storage_Buffer(uint32_t slot, Buffer* buffer, uint64_t offset, uint64_t size)
{
	if (slot >= STORAGE_BUFFER_COUNT)
		return;
	Binding& b = storageBuffers[slot];
	b.set = buffer != nullptr;
	b.buffer = buffer ? buffer->buffer : VK_NULL_HANDLE;
	b.offset = offset;
	b.range = size == ~0ull ? VK_WHOLE_SIZE : size;
	dirtyGraphics = dirtyCompute = true;
}

void VulkanCommandList::Set_Storage_Image(uint32_t slot, Texture* texture)
{
	if (slot >= STORAGE_IMAGE_COUNT)
		return;
	Binding& b = storageImages[slot];
	b.set = texture != nullptr;
	b.texture = texture;
	dirtyGraphics = dirtyCompute = true;
}

void VulkanCommandList::Set_Acceleration_Structure(AccelStruct* accel)
{
	tlas = accel;
	dirtyGraphics = dirtyCompute = true;
}

VkDescriptorSet VulkanCommandList::Allocate_Set()
{
	for (;;)
	{
		if (frame->descriptorPool >= frame->descriptors.size())
		{
			VkDescriptorPool pool = dev->Create_Descriptor_Pool();
			if (pool == VK_NULL_HANDLE)
				return VK_NULL_HANDLE;
			frame->descriptors.push_back(pool);
		}
		VkDescriptorSetAllocateInfo ai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = frame->descriptors[frame->descriptorPool];
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &dev->setLayout;
		VkDescriptorSet set = VK_NULL_HANDLE;
		VkResult r = vkAllocateDescriptorSets(dev->device, &ai, &set);
		if (r == VK_SUCCESS)
			return set;
		if (r != VK_ERROR_OUT_OF_POOL_MEMORY && r != VK_ERROR_FRAGMENTED_POOL)
			return VK_NULL_HANDLE;
		++frame->descriptorPool;	// this pool is full: the next, or a new one
	}
}

void VulkanCommandList::Prepare_Draw(VkPipelineBindPoint bindPoint)
{
	const bool compute = bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE;
	// images into the layouts this draw reads and writes them in (outside rendering)
	const VkPipelineStageFlags2 shaderStages = compute ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
		: (VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);
	for (Binding& b : textures)
	{
		if (!b.set || Is_Target(b.texture))
			continue;
		const VkImageLayout want = Format_Is_Depth(b.texture->desc.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Transition(b.texture, want, shaderStages, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
	}
	for (Binding& b : storageImages)
		if (b.set)
			Transition(b.texture, VK_IMAGE_LAYOUT_GENERAL, shaderStages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
	Flush_Buffer_Barrier();
	if (!compute && passOpen && !inPass)
		Begin_Rendering(pass);
	workSinceBarrier = true;

	bool& dirty = compute ? dirtyCompute : dirtyGraphics;
	if (!dirty)
		return;
	VkDescriptorSet set = Allocate_Set();
	if (set == VK_NULL_HANDLE)
	{
		dev->Log("no descriptor set for a draw");
		return;
	}
	VkDescriptorBufferInfo bufferInfos[UNIFORM_COUNT + STORAGE_BUFFER_COUNT];
	VkDescriptorImageInfo imageInfos[TEXTURE_COUNT + STORAGE_IMAGE_COUNT];
	VkWriteDescriptorSet writes[UNIFORM_COUNT + TEXTURE_COUNT + STORAGE_BUFFER_COUNT + STORAGE_IMAGE_COUNT + 1];
	uint32_t nw = 0, nb = 0, ni = 0;
	auto write = [&](uint32_t binding, VkDescriptorType type) -> VkWriteDescriptorSet&
	{
		VkWriteDescriptorSet& w = writes[nw++];
		w = VkWriteDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		w.dstSet = set;
		w.dstBinding = binding;
		w.descriptorCount = 1;
		w.descriptorType = type;
		return w;
	};
	for (uint32_t i = 0; i < UNIFORM_COUNT; ++i)
	{
		if (!uniforms[i].set)
			continue;
		bufferInfos[nb] = { uniforms[i].buffer, uniforms[i].offset, uniforms[i].range };
		write(UNIFORM_BASE + i, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER).pBufferInfo = &bufferInfos[nb++];
	}
	for (uint32_t i = 0; i < TEXTURE_COUNT; ++i)
	{
		if (!textures[i].set || Is_Target(textures[i].texture))
			continue;	// a texture left bound that is drawn into now (Direct3D 9 unbinds it)
		imageInfos[ni] = { textures[i].sampler, textures[i].texture->view, textures[i].texture->layout };
		write(TEXTURE_BASE + i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER).pImageInfo = &imageInfos[ni++];
	}
	for (uint32_t i = 0; i < STORAGE_BUFFER_COUNT; ++i)
	{
		if (!storageBuffers[i].set)
			continue;
		bufferInfos[nb] = { storageBuffers[i].buffer, storageBuffers[i].offset, storageBuffers[i].range };
		write(STORAGE_BUFFER_BASE + i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER).pBufferInfo = &bufferInfos[nb++];
	}
	for (uint32_t i = 0; i < STORAGE_IMAGE_COUNT; ++i)
	{
		if (!storageImages[i].set)
			continue;
		imageInfos[ni] = { VK_NULL_HANDLE, storageImages[i].texture->view, VK_IMAGE_LAYOUT_GENERAL };
		write(STORAGE_IMAGE_BASE + i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE).pImageInfo = &imageInfos[ni++];
	}
	VkWriteDescriptorSetAccelerationStructureKHR asWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	if (tlas && dev->rayQuery)
	{
		asWrite.accelerationStructureCount = 1;
		asWrite.pAccelerationStructures = &tlas->handle;
		write(ACCEL_BINDING, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR).pNext = &asWrite;
	}
	vkUpdateDescriptorSets(dev->device, nw, writes, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, bindPoint, dev->pipelineLayout, 0, 1, &set, 0, nullptr);
	dirty = false;
}

void VulkanCommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	if (!passOpen)
		return;
	Prepare_Draw(VK_PIPELINE_BIND_POINT_GRAPHICS);
	vkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanCommandList::Draw_Indexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
	if (!passOpen)
		return;
	Prepare_Draw(VK_PIPELINE_BIND_POINT_GRAPHICS);
	vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void VulkanCommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z)
{
	Suspend_Pass();
	Prepare_Draw(VK_PIPELINE_BIND_POINT_COMPUTE);
	vkCmdDispatch(cmd, x, y, z);
	// what it wrote is seen by the next command that reads it
	pendingStage |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	pendingAccess |= VK_ACCESS_2_SHADER_WRITE_BIT;
	for (Binding& b : storageImages)
		if (b.set)
		{
			b.texture->stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			b.texture->access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
		}
}

bool VulkanCommandList::Stage_Copy(const void* data, size_t size, VkDeviceSize alignment, Buffer*& buffer, VkDeviceSize& offset)
{
	Ring& ring = frame->staging;
	alignment = std::lcm<VkDeviceSize>(std::max<VkDeviceSize>(alignment, 1), 16);
	VkDeviceSize start = Align_Up(ring.used, alignment);
	if (start + size <= ring.buffer->desc.size)
	{
		memcpy((char*)ring.buffer->mapped + start, data, size);
		ring.used = start + size;
		buffer = ring.buffer;
		offset = start;
		return true;
	}
	// more than the ring holds this frame: a buffer of its own, gone with the frame
	BufferDesc d;
	d.size = size;
	d.usage = BUFFER_COPY_SRC | (dev->rayQuery ? (uint32_t)BUFFER_ACCEL_INPUT : 0u);
	d.memory = Memory::UPLOAD;
	d.name = "staging overflow";
	Buffer* extra = dev->Create_Buffer_Internal(d);
	if (extra == nullptr)
		return false;
	memcpy(extra->mapped, data, size);
	frame->extraStaging.push_back(extra);
	buffer = extra;
	offset = 0;
	return true;
}

void VulkanCommandList::Update_Buffer(Buffer* buffer, const void* data, size_t size, uint64_t offset)
{
	if (buffer == nullptr || data == nullptr || size == 0)
		return;
	Buffer* src = nullptr;
	VkDeviceSize srcOffset = 0;
	if (!Stage_Copy(data, size, 4, src, srcOffset))
		return;
	Before_Transfer_Write();
	VkBufferCopy region = { srcOffset, offset, size };
	vkCmdCopyBuffer(cmd, src->buffer, buffer->buffer, 1, &region);
	pendingStage |= VK_PIPELINE_STAGE_2_COPY_BIT;
	pendingAccess |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
}

void VulkanCommandList::Update_Texture(Texture* texture, uint32_t mip, uint32_t layer, const void* data, size_t rowPitch)
{
	if (texture == nullptr || data == nullptr)
		return;
	const TextureDesc& d = texture->desc;
	const uint32_t w = std::max(1u, d.width >> mip), h = std::max(1u, d.height >> mip);
	const uint32_t depth = d.type == TextureType::TEX_3D ? std::max(1u, d.depth >> mip) : 1;
	const bool compressed = Format_Is_Compressed(d.format);
	const uint32_t rows = compressed ? (h + 3) / 4 : h;
	const size_t packedPitch = compressed ? ((w + 3) / 4) * Format_Size(d.format) : (size_t)w * Format_Size(d.format);
	const size_t pitch = rowPitch ? rowPitch : packedPitch;
	std::vector<uint8_t> packed;
	const void* src = data;
	if (pitch != packedPitch)
	{
		packed.resize(packedPitch * rows * depth);
		for (uint32_t z = 0; z < depth; ++z)
			for (uint32_t r = 0; r < rows; ++r)
				memcpy(&packed[(z * rows + r) * packedPitch], (const uint8_t*)data + (z * rows + r) * pitch, packedPitch);
		src = packed.data();
	}
	Buffer* staging = nullptr;
	VkDeviceSize stagingOffset = 0;
	if (!Stage_Copy(src, packedPitch * rows * depth, Format_Size(d.format), staging, stagingOffset))
		return;
	Transition(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
	VkBufferImageCopy region = {};
	region.bufferOffset = stagingOffset;
	region.imageSubresource = { texture->aspect, mip, layer, 1 };
	region.imageExtent = { w, h, depth };
	vkCmdCopyBufferToImage(cmd, staging->buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void VulkanCommandList::Copy_Buffer(Buffer* dst, uint64_t dstOffset, Buffer* src, uint64_t srcOffset, uint64_t size)
{
	if (dst == nullptr || src == nullptr || size == 0)
		return;
	Before_Transfer_Write();
	VkBufferCopy region = { srcOffset, dstOffset, size };
	vkCmdCopyBuffer(cmd, src->buffer, dst->buffer, 1, &region);
	pendingStage |= VK_PIPELINE_STAGE_2_COPY_BIT;
	pendingAccess |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
}

void VulkanCommandList::Copy_Texture_To_Buffer(Buffer* dst, Texture* src, uint32_t mip, uint32_t layer)
{
	if (dst == nullptr || src == nullptr)
		return;
	Before_Transfer_Write();
	Transition(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
	VkBufferImageCopy region = {};
	region.imageSubresource = { Format_Is_Depth(src->desc.format) ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT : src->aspect, mip, layer, 1 };
	region.imageExtent = { std::max(1u, src->desc.width >> mip), std::max(1u, src->desc.height >> mip), 1 };
	vkCmdCopyImageToBuffer(cmd, src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->buffer, 1, &region);
	// the host reads it once the frame is done; the GPU, after the next barrier
	VkMemoryBarrier2 b = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	b.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	b.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	b.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
	VkDependencyInfo di = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	di.memoryBarrierCount = 1;
	di.pMemoryBarriers = &b;
	vkCmdPipelineBarrier2(cmd, &di);
	pendingStage |= VK_PIPELINE_STAGE_2_COPY_BIT;
	pendingAccess |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
}

void VulkanCommandList::Build_Bottom_Level(AccelStruct* blas, const TriangleGeometry* geometry, uint32_t count)
{
	if (blas == nullptr || geometry == nullptr || count == 0 || !dev->rayQuery)
		return;
	std::vector<VkAccelerationStructureGeometryKHR> geos;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
	std::vector<uint32_t> primitives;
	dev->Build_Sizes(geometry, count, geos, ranges, primitives);
	BufferDesc sd;
	sd.size = blas->scratchSize + 256;
	sd.usage = BUFFER_STORAGE;
	sd.name = "acceleration structure scratch";
	Buffer* scratch = dev->Create_Buffer_Internal(sd);
	if (scratch == nullptr)
		return;
	dev->Destroy(scratch);	// freed once this frame is done
	Before_Transfer_Write();
	VkAccelerationStructureBuildGeometryInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	info.dstAccelerationStructure = blas->handle;
	info.geometryCount = count;
	info.pGeometries = geos.data();
	info.scratchData.deviceAddress = Align_Up(scratch->address, 256);
	const VkAccelerationStructureBuildRangeInfoKHR* pRanges = ranges.data();
	vkCmdBuildAccelerationStructuresKHR(cmd, 1, &info, &pRanges);
	pendingStage |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	pendingAccess |= VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	workSinceBarrier = true;
}

void VulkanCommandList::Build_Top_Level(AccelStruct* tlasStruct, const Instance* instances, uint32_t count)
{
	if (tlasStruct == nullptr || !dev->rayQuery)
		return;
	count = std::min(count, tlasStruct->maxInstances);
	// the instances, staged for this frame (the build reads them by address)
	std::vector<VkAccelerationStructureInstanceKHR> vkInstances(std::max(1u, count));
	for (uint32_t i = 0; i < count; ++i)
	{
		VkAccelerationStructureInstanceKHR& v = vkInstances[i];
		memcpy(v.transform.matrix, instances[i].transform, sizeof(v.transform.matrix));
		v.instanceCustomIndex = instances[i].id & 0xFFFFFF;
		v.mask = instances[i].mask;
		v.instanceShaderBindingTableRecordOffset = 0;
		v.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		v.accelerationStructureReference = instances[i].blas ? instances[i].blas->address : 0;
	}
	Buffer* staging = nullptr;
	VkDeviceSize stagingOffset = 0;
	if (!Stage_Copy(vkInstances.data(), vkInstances.size() * sizeof(VkAccelerationStructureInstanceKHR), 16, staging, stagingOffset))
		return;
	BufferDesc sd;
	sd.size = tlasStruct->scratchSize + 256;
	sd.usage = BUFFER_STORAGE;
	sd.name = "acceleration structure scratch";
	Buffer* scratch = dev->Create_Buffer_Internal(sd);
	if (scratch == nullptr)
		return;
	dev->Destroy(scratch);
	Before_Transfer_Write();
	VkAccelerationStructureGeometryKHR geo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geo.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geo.geometry.instances.data.deviceAddress = staging->address + stagingOffset;
	VkAccelerationStructureBuildGeometryInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	info.dstAccelerationStructure = tlasStruct->handle;
	info.geometryCount = 1;
	info.pGeometries = &geo;
	info.scratchData.deviceAddress = Align_Up(scratch->address, 256);
	VkAccelerationStructureBuildRangeInfoKHR range = { count, 0, 0, 0 };
	const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
	vkCmdBuildAccelerationStructuresKHR(cmd, 1, &info, &pRange);
	pendingStage |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	pendingAccess |= VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	workSinceBarrier = true;
}

void VulkanCommandList::Begin_Marker(const char* name)
{
	if (vkCmdBeginDebugUtilsLabelEXT == nullptr || !dev->debugUtils)
		return;
	VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
	label.pLabelName = name;
	vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
}

void VulkanCommandList::End_Marker()
{
	if (vkCmdEndDebugUtilsLabelEXT == nullptr || !dev->debugUtils)
		return;
	vkCmdEndDebugUtilsLabelEXT(cmd);
}

//------------------------------------------------------------------------------------------ factory
Device* Create_Vulkan_Device(const VulkanOptions& options)
{
	VulkanDevice* d = new VulkanDevice;
	d->options = options;
	if (!d->Init())
	{
		delete d;
		return nullptr;
	}
	return d;
}

Device* Create_Vulkan_Device_External(const VulkanExternal& external)
{
	VulkanDevice* d = new VulkanDevice;
	d->options.log = external.log;
	if (!d->Init_External(external))
	{
		delete d;
		return nullptr;
	}
	return d;
}

}	// namespace rhi
