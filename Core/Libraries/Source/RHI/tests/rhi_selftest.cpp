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

// Tiberium Project @feature The RHI's headless self-test, on any Vulkan 1.3 device (lavapipe included):
//   raster   a textured, tinted triangle with depth and culling, Direct3D's orientation (y up in
//            clip space is the top of the image), and a buffer update in the middle of the pass
//            (rendering suspended and resumed without losing what was drawn)
//   compute  a storage buffer written by a dispatch, between a copy into it and a copy out of it
//   rays     acceleration structures built on the GPU and inline ray queries from a compute
//            shader, against the answer worked out on the CPU (closest hit and instance IDs)
//   frames   objects created and destroyed over frames in flight
//   shared   all of the above again on a device made by someone else (as DXVK's is shared
//            through its Direct3D 9 interop), and that device's image written by a dispatch,
//            given back in its layout and read by its owner
// Exit code: 0 passed, 1 failed, 77 no Vulkan 1.3 device (skipped).
//
//   rhi_selftest [--validation] [--require-ray-query] [--ppm <file>] [--no-shared]

#include "rhi/rhi.h"

#include "../src/vk_loader.h"

#include "spirv/spv_selftest_compute.h"
#include "spirv/spv_selftest_image.h"
#include "spirv/spv_selftest_raster_ps.h"
#include "spirv/spv_selftest_raster_vs.h"
#include "spirv/spv_selftest_rayquery.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace rhi;

namespace
{
	int g_failures = 0;
	int g_validationMessages = 0;

	void Check(bool ok, const char* fmt, ...)
	{
		char line[512];
		va_list args;
		va_start(args, fmt);
		vsnprintf(line, sizeof(line), fmt, args);
		va_end(args);
		printf("%s %s\n", ok ? "  ok  " : "  FAIL", line);
		if (!ok)
			++g_failures;
	}

	void Log(const char* line)
	{
		printf("  rhi: %s\n", line);
		if (strncmp(line, "validation:", 11) == 0)
			++g_validationMessages;
	}

	struct Vertex
	{
		float x, y, u, v;
	};

	struct RasterPush
	{
		float offset[2];
		float depth;
		float pad;
	};

	struct Pixel
	{
		uint8_t r, g, b, a;
	};

	bool Near(const Pixel& p, int r, int g, int b)
	{
		return std::abs(p.r - r) <= 3 && std::abs(p.g - g) <= 3 && std::abs(p.b - b) <= 3;
	}

	Shader* Make_Shader(Device* dev, Stage stage, const uint32_t* code, size_t size, const char* entry, const char* name)
	{
		ShaderDesc d;
		d.stage = stage;
		d.code = code;
		d.size = size;
		d.entry = entry;
		d.name = name;
		return dev->Create_Shader(d);
	}

	//------------------------------------------------------------------------------------- raster
	void Test_Raster(Device* dev, const char* ppmPath)
	{
		printf("raster\n");
		const uint32_t W = 64, H = 64;
		TextureDesc td;
		td.width = W;
		td.height = H;
		td.format = Format::R8G8B8A8_UNORM;
		td.usage = TEXTURE_RENDER_TARGET | TEXTURE_COPY_SRC | TEXTURE_SAMPLED;
		td.name = "selftest target";
		Texture* target = dev->Create_Texture(td);
		TextureDesc dd;
		dd.width = W;
		dd.height = H;
		dd.format = Format::D32_FLOAT;
		dd.usage = TEXTURE_DEPTH_STENCIL;
		dd.name = "selftest depth";
		Texture* depth = dev->Create_Texture(dd);
		// 2x2: white at the top left, grey elsewhere
		TextureDesc xd;
		xd.width = 2;
		xd.height = 2;
		xd.format = Format::R8G8B8A8_UNORM;
		xd.name = "selftest texture";
		Texture* texture = dev->Create_Texture(xd);
		const uint32_t texels[4] = { 0xFFFFFFFFu, 0xFF808080u, 0xFF808080u, 0xFF808080u };
		SamplerDesc sd;
		sd.minFilter = sd.magFilter = sd.mipFilter = Filter::NEAREST;
		sd.u = sd.v = Address::CLAMP;
		Sampler* sampler = dev->Create_Sampler(sd);

		// the triangle (clockwise on screen, Direct3D's front), a full-screen triangle behind it,
		// a counter-clockwise one in the top left corner (culled)
		const Vertex vertices[] =
		{
			{ 0.0f, 0.8f, 0.25f, 0.25f }, { 0.8f, -0.8f, 0.25f, 0.25f }, { -0.8f, -0.8f, 0.25f, 0.25f },
			{ -1.0f, -1.0f, 0.75f, 0.75f }, { -1.0f, 3.0f, 0.75f, 0.75f }, { 3.0f, -1.0f, 0.75f, 0.75f },
			{ -0.9f, 0.95f, 0.25f, 0.25f }, { -0.9f, 0.55f, 0.25f, 0.25f }, { -0.5f, 0.95f, 0.25f, 0.25f },
		};
		const uint16_t indices[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
		BufferDesc vb;
		vb.size = sizeof(vertices);
		vb.usage = BUFFER_VERTEX;
		vb.name = "selftest vertices";
		Buffer* vertexBuffer = dev->Create_Buffer(vb, vertices);
		BufferDesc ib;
		ib.size = sizeof(indices);
		ib.usage = BUFFER_INDEX;
		ib.name = "selftest indices";
		Buffer* indexBuffer = dev->Create_Buffer(ib, indices);
		// written in the middle of the pass: a small triangle in the top right corner
		BufferDesc lb;
		lb.size = 3 * sizeof(Vertex);
		lb.usage = BUFFER_VERTEX;
		lb.name = "selftest late vertices";
		Buffer* lateBuffer = dev->Create_Buffer(lb);
		BufferDesc rb;
		rb.size = W * H * 4;
		rb.usage = BUFFER_COPY_DST;
		rb.memory = Memory::READBACK;
		rb.name = "selftest readback";
		Buffer* readback = dev->Create_Buffer(rb);

		Shader* vs = Make_Shader(dev, Stage::VERTEX, spv_selftest_raster_vs, sizeof(spv_selftest_raster_vs), "VSMain", "selftest vs");
		Shader* ps = Make_Shader(dev, Stage::PIXEL, spv_selftest_raster_ps, sizeof(spv_selftest_raster_ps), "PSMain", "selftest ps");
		GraphicsPipelineDesc pd;
		pd.vs = vs;
		pd.ps = ps;
		pd.bindings[0].binding = 0;
		pd.bindings[0].stride = sizeof(Vertex);
		pd.bindingCount = 1;
		pd.attributes[0] = { 0, Format::R32G32_FLOAT, 0, 0 };
		pd.attributes[1] = { 1, Format::R32G32_FLOAT, 0, 8 };
		pd.attributeCount = 2;
		pd.raster.cull = Cull::BACK;
		pd.depth.test = true;
		pd.depth.write = true;
		pd.depth.func = Compare::LESS;
		pd.colorFormats[0] = Format::R8G8B8A8_UNORM;
		pd.colorCount = 1;
		pd.depthFormat = Format::D32_FLOAT;
		pd.name = "selftest raster";
		Pipeline* pipeline = dev->Create_Graphics_Pipeline(pd);
		Check(target && depth && texture && sampler && vertexBuffer && indexBuffer && lateBuffer && readback && vs && ps && pipeline,
			"resources created");
		if (!pipeline || !target || !readback)
			return;

		CommandList* cl = dev->Begin_Frame();
		cl->Update_Texture(texture, 0, 0, texels, 0);
		RenderPassDesc pass;
		pass.colors[0].texture = target;
		pass.colors[0].load = Load::CLEAR;
		pass.colorCount = 1;
		pass.depth = depth;
		pass.depthLoad = Load::CLEAR;
		pass.clearDepth = 1.0f;
		cl->Begin_Render_Pass(pass);
		cl->Set_Pipeline(pipeline);
		cl->Set_Vertex_Buffer(0, vertexBuffer);
		cl->Set_Index_Buffer(indexBuffer, 0, true);
		cl->Set_Texture(0, texture, sampler);
		const float red[4] = { 1, 0, 0, 1 }, green[4] = { 0, 1, 0, 1 }, blue[4] = { 0, 0, 1, 1 };
		RasterPush push = { { 0, 0 }, 0.5f, 0 };
		cl->Set_Constants(0, red, sizeof(red));
		cl->Set_Push_Constants(&push, sizeof(push));
		cl->Draw_Indexed(3, 1, 0, 0, 0);
		// culled: would be in front of everything
		push.depth = 0.1f;
		cl->Set_Constants(0, blue, sizeof(blue));
		cl->Set_Push_Constants(&push, sizeof(push));
		cl->Draw_Indexed(3, 1, 6, 0, 0);
		// a copy in the middle of the pass, then a draw from what it wrote
		const Vertex late[3] = { { 0.6f, 0.95f, 0.25f, 0.25f }, { 0.95f, 0.6f, 0.25f, 0.25f }, { 0.6f, 0.6f, 0.25f, 0.25f } };
		cl->Update_Buffer(lateBuffer, late, sizeof(late));
		cl->Set_Vertex_Buffer(0, lateBuffer);
		push.depth = 0.3f;
		cl->Set_Constants(0, blue, sizeof(blue));
		cl->Set_Push_Constants(&push, sizeof(push));
		cl->Draw(3, 1, 0, 0);
		// behind the first triangle, grey from the texture
		cl->Set_Vertex_Buffer(0, vertexBuffer);
		push.depth = 0.7f;
		cl->Set_Constants(0, green, sizeof(green));
		cl->Set_Push_Constants(&push, sizeof(push));
		cl->Draw_Indexed(3, 1, 3, 0, 0);
		cl->End_Render_Pass();
		cl->Copy_Texture_To_Buffer(readback, target);
		dev->Submit_Frame();
		dev->Wait_Idle();

		const Pixel* px = (const Pixel*)dev->Map(readback);
		auto at = [&](int x, int y) { return px[y * W + x]; };
		auto describe = [&](int x, int y) { static char s[64]; Pixel p = at(x, y); snprintf(s, sizeof(s), "(%d %d %d %d)", p.r, p.g, p.b, p.a); return s; };
		Check(Near(at(32, 40), 255, 0, 0), "the triangle is drawn, nearer than the one behind it: %s", describe(32, 40));
		Check(Near(at(10, 54), 255, 0, 0), "its base is at the bottom of the image: %s", describe(10, 54));
		Check(Near(at(10, 10), 0, 128, 0), "its apex at the top (beside it, the grey from the texture): %s", describe(10, 10));
		Check(Near(at(6, 4), 0, 128, 0), "the counter-clockwise triangle is culled: %s", describe(6, 4));
		Check(Near(at(53, 10), 0, 0, 255), "the vertices copied in the middle of the pass are drawn: %s", describe(53, 10));
		Check(Near(at(32, 62), 0, 128, 0), "the background: %s", describe(32, 62));
		if (ppmPath)
		{
			if (FILE* f = fopen(ppmPath, "wb"))
			{
				fprintf(f, "P6\n%u %u\n255\n", W, H);
				for (uint32_t i = 0; i < W * H; ++i)
					fwrite(&px[i], 1, 3, f);
				fclose(f);
				printf("  wrote %s\n", ppmPath);
			}
		}

		dev->Destroy(pipeline);
		dev->Destroy(vs);
		dev->Destroy(ps);
		dev->Destroy(readback);
		dev->Destroy(lateBuffer);
		dev->Destroy(indexBuffer);
		dev->Destroy(vertexBuffer);
		dev->Destroy(sampler);
		dev->Destroy(texture);
		dev->Destroy(depth);
		dev->Destroy(target);
	}

	//------------------------------------------------------------------------------------ compute
	void Test_Compute(Device* dev)
	{
		printf("compute\n");
		const uint32_t N = 1024, WRITTEN = 1000;
		BufferDesc sd;
		sd.size = N * 4;
		sd.usage = BUFFER_STORAGE;
		sd.name = "selftest storage";
		Buffer* storage = dev->Create_Buffer(sd);
		BufferDesc rd;
		rd.size = N * 4;
		rd.usage = BUFFER_COPY_DST;
		rd.memory = Memory::READBACK;
		Buffer* readback = dev->Create_Buffer(rd);
		Shader* cs = Make_Shader(dev, Stage::COMPUTE, spv_selftest_compute, sizeof(spv_selftest_compute), "main", "selftest cs");
		ComputePipelineDesc cd;
		cd.cs = cs;
		cd.name = "selftest compute";
		Pipeline* pipeline = dev->Create_Compute_Pipeline(cd);
		Check(storage && readback && cs && pipeline, "resources created");
		if (!pipeline || !storage || !readback)
			return;
		std::vector<uint32_t> fill(N, 0xDEADBEEFu);
		CommandList* cl = dev->Begin_Frame();
		cl->Update_Buffer(storage, fill.data(), N * 4);
		cl->Set_Pipeline(pipeline);
		cl->Set_Storage_Buffer(0, storage);
		const uint32_t push[4] = { WRITTEN, 3, 7, 0 };
		cl->Set_Push_Constants(push, sizeof(push));
		cl->Dispatch((WRITTEN + 63) / 64);
		cl->Copy_Buffer(readback, 0, storage, 0, N * 4);
		dev->Submit_Frame();
		dev->Wait_Idle();
		const uint32_t* out = (const uint32_t*)dev->Map(readback);
		uint32_t wrong = 0, firstWrong = N;
		for (uint32_t i = 0; i < N; ++i)
		{
			const uint32_t want = i < WRITTEN ? i * 3 + 7 : 0xDEADBEEFu;
			if (out[i] != want)
			{
				++wrong;
				firstWrong = std::min(firstWrong, i);
			}
		}
		Check(wrong == 0, "the dispatch wrote %u values after the copy and before the readback (%u wrong, first at %u)", WRITTEN, wrong, firstWrong);
		dev->Destroy(pipeline);
		dev->Destroy(cs);
		dev->Destroy(readback);
		dev->Destroy(storage);
	}

	//------------------------------------------------------------------------------- ray queries
	void Test_Ray_Query(Device* dev)
	{
		printf("ray queries\n");
		// a unit quad in the xz plane, as two triangles
		const float quad[] = { -0.5f, 0, -0.5f, 0.5f, 0, -0.5f, 0.5f, 0, 0.5f, -0.5f, 0, 0.5f };
		const uint16_t quadIndices[] = { 0, 1, 2, 0, 2, 3 };
		BufferDesc vd;
		vd.size = sizeof(quad);
		vd.usage = BUFFER_VERTEX | BUFFER_ACCEL_INPUT;
		Buffer* vertices = dev->Create_Buffer(vd, quad);
		BufferDesc id;
		id.size = 16;	// 12 bytes of indices, padded
		id.usage = BUFFER_INDEX | BUFFER_ACCEL_INPUT;
		uint16_t paddedIndices[8] = {};
		memcpy(paddedIndices, quadIndices, sizeof(quadIndices));
		Buffer* indices = dev->Create_Buffer(id, paddedIndices);
		TriangleGeometry geometry;
		geometry.vertices = vertices;
		geometry.vertexStride = 12;
		geometry.vertexCount = 4;
		geometry.indices = indices;
		geometry.indexCount = 6;
		geometry.indices16 = true;
		AccelStruct* blas = dev->Create_Bottom_Level(&geometry, 1);
		AccelStruct* tlas = dev->Create_Top_Level(4);
		const uint32_t SIZE = 32;
		const float EXTENT = 1.0f;
		BufferDesc od;
		od.size = SIZE * SIZE * 4;
		od.usage = BUFFER_STORAGE;
		Buffer* output = dev->Create_Buffer(od);
		BufferDesc rd;
		rd.size = SIZE * SIZE * 4;
		rd.usage = BUFFER_COPY_DST;
		rd.memory = Memory::READBACK;
		Buffer* readback = dev->Create_Buffer(rd);
		Shader* cs = Make_Shader(dev, Stage::COMPUTE, spv_selftest_rayquery, sizeof(spv_selftest_rayquery), "main", "selftest ray query");
		ComputePipelineDesc cd;
		cd.cs = cs;
		cd.name = "selftest ray query";
		Pipeline* pipeline = dev->Create_Compute_Pipeline(cd);
		Check(vertices && indices && blas && tlas && output && readback && cs && pipeline, "resources created");
		if (!pipeline || !blas || !tlas || !readback)
			return;

		// two quads, overlapping in x from -0.1 to 0.1; the first is nearer the ground
		Instance instances[2];
		instances[0].transform[0][3] = -0.4f;
		instances[0].transform[1][3] = 1.0f;
		instances[0].id = 0;
		instances[0].blas = blas;
		instances[1].transform[0][3] = 0.4f;
		instances[1].transform[1][3] = 2.0f;
		instances[1].id = 5;
		instances[1].blas = blas;

		CommandList* cl = dev->Begin_Frame();
		cl->Build_Bottom_Level(blas, &geometry, 1);
		cl->Build_Top_Level(tlas, instances, 2);
		cl->Set_Pipeline(pipeline);
		cl->Set_Storage_Buffer(0, output);
		cl->Set_Acceleration_Structure(tlas);
		struct { uint32_t size; float extent; uint32_t pad0, pad1; } push = { SIZE, EXTENT, 0, 0 };
		cl->Set_Push_Constants(&push, sizeof(push));
		cl->Dispatch(SIZE / 8, SIZE / 8);
		cl->Copy_Buffer(readback, 0, output, 0, SIZE * SIZE * 4);
		dev->Submit_Frame();
		dev->Wait_Idle();

		const uint32_t* out = (const uint32_t*)dev->Map(readback);
		uint32_t wrong = 0, hits[2] = {}, misses = 0;
		for (uint32_t y = 0; y < SIZE; ++y)
			for (uint32_t x = 0; x < SIZE; ++x)
			{
				const float px = ((x + 0.5f) / SIZE * 2.0f - 1.0f) * EXTENT;
				const float pz = ((y + 0.5f) / SIZE * 2.0f - 1.0f) * EXTENT;
				const bool inZ = std::fabs(pz) < 0.5f;
				uint32_t want = 0;
				if (inZ && std::fabs(px + 0.4f) < 0.5f)
					want = 1;		// the lower quad, instance 0
				else if (inZ && std::fabs(px - 0.4f) < 0.5f)
					want = 6;		// instance 5
				const uint32_t got = out[y * SIZE + x];
				if (got != want)
					++wrong;
				if (got == 1) ++hits[0];
				else if (got == 6) ++hits[1];
				else if (got == 0) ++misses;
			}
		Check(wrong == 0, "%u rays: %u hit the lower quad, %u the upper, %u nothing; %u differ from the CPU", SIZE * SIZE, hits[0], hits[1], misses, wrong);

		dev->Destroy(pipeline);
		dev->Destroy(cs);
		dev->Destroy(readback);
		dev->Destroy(output);
		dev->Destroy(tlas);
		dev->Destroy(blas);
		dev->Destroy(indices);
		dev->Destroy(vertices);
	}

	//------------------------------------------------------------------------------------ frames
	void Test_Frames(Device* dev)
	{
		printf("frames\n");
		// objects made and dropped while frames are in flight: freed after the GPU is done with them
		std::vector<uint8_t> data(256 * 1024, 0x5A);
		BufferDesc rd;
		rd.size = 16;
		rd.usage = BUFFER_COPY_DST;
		rd.memory = Memory::READBACK;
		Buffer* readback = dev->Create_Buffer(rd);
		uint32_t good = 0;
		const uint32_t FRAMES = 8;
		for (uint32_t frame = 0; frame < FRAMES; ++frame)
		{
			CommandList* cl = dev->Begin_Frame();
			data[0] = (uint8_t)frame;
			BufferDesc bd;
			bd.size = data.size();
			bd.usage = BUFFER_STORAGE;
			Buffer* b = dev->Create_Buffer(bd, data.data());	// staged in this frame
			cl->Copy_Buffer(readback, 0, b, 0, 16);
			TextureDesc td;
			td.width = td.height = 64;
			Texture* t = dev->Create_Texture(td);
			std::vector<uint32_t> texels(64 * 64, frame);
			cl->Update_Texture(t, 0, 0, texels.data(), 0);
			dev->Destroy(b);
			dev->Destroy(t);
			dev->Submit_Frame();
			if (frame + 1 == FRAMES)
				dev->Wait_Idle();
		}
		const uint8_t* out = (const uint8_t*)dev->Map(readback);
		good = out[0] == FRAMES - 1 && out[1] == 0x5A ? 1 : 0;
		Check(good == 1, "%u frames of buffers and textures made, used and dropped (last copy: %u %u)", FRAMES, out[0], out[1]);
		dev->Destroy(readback);
	}

	//------------------------------------------------------------------------- a shared device
	// The other renderer: its own instance, device and an image of its own (as DXVK would have)
	struct Host
	{
		VkInstance instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
		VkPhysicalDevice phys = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		VkQueue queue = VK_NULL_HANDLE;
		uint32_t family = 0;
		bool rayQuery = false;
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory imageMemory = VK_NULL_HANDLE;
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
		VkCommandPool pool = VK_NULL_HANDLE;
		int locks = 0, unlocks = 0;
	};

	VKAPI_ATTR VkBool32 VKAPI_CALL Host_Debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
		const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
	{
		if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			char line[2048];
			snprintf(line, sizeof(line), "validation: %s", data->pMessage);
			Log(line);
		}
		return VK_FALSE;
	}

	uint32_t Host_Memory(const Host& h, uint32_t bits, VkMemoryPropertyFlags want)
	{
		VkPhysicalDeviceMemoryProperties mp;
		rhi::vk::vkGetPhysicalDeviceMemoryProperties(h.phys, &mp);
		for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
			if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want)
				return i;
		return 0;
	}

	// One command buffer of the host's, run to the end
	template <typename F> void Host_Run(Host& h, F record)
	{
		using namespace rhi::vk;
		VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		ai.commandPool = h.pool;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = 1;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		vkAllocateCommandBuffers(h.device, &ai, &cmd);
		VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &bi);
		record(cmd);
		vkEndCommandBuffer(cmd);
		VkCommandBufferSubmitInfo cbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
		cbi.commandBuffer = cmd;
		VkSubmitInfo2 si = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		si.commandBufferInfoCount = 1;
		si.pCommandBufferInfos = &cbi;
		vkQueueSubmit2(h.queue, 1, &si, VK_NULL_HANDLE);
		vkQueueWaitIdle(h.queue);
		vkResetCommandPool(h.device, h.pool, 0);
	}

	void Host_Image_Barrier(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to)
	{
		VkImageMemoryBarrier2 b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		b.oldLayout = from;
		b.newLayout = to;
		b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = image;
		b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		VkDependencyInfo di = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		di.imageMemoryBarrierCount = 1;
		di.pImageMemoryBarriers = &b;
		rhi::vk::vkCmdPipelineBarrier2(cmd, &di);
	}

	bool Host_Create(Host& h, bool validation)
	{
		using namespace rhi::vk;
		if (!Load_Library())
			return false;
		uint32_t version = VK_API_VERSION_1_0;
		if (vkEnumerateInstanceVersion)
			vkEnumerateInstanceVersion(&version);
		if (version < VK_API_VERSION_1_3)
			return false;
		std::vector<const char*> layers, extensions;
		uint32_t n = 0;
		vkEnumerateInstanceLayerProperties(&n, nullptr);
		std::vector<VkLayerProperties> layerList(n);
		vkEnumerateInstanceLayerProperties(&n, layerList.data());
		for (const VkLayerProperties& l : layerList)
			if (validation && strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0)
				layers.push_back("VK_LAYER_KHRONOS_validation");
		vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
		std::vector<VkExtensionProperties> extList(n);
		vkEnumerateInstanceExtensionProperties(nullptr, &n, extList.data());
		bool debugUtils = false;
		for (const VkExtensionProperties& e : extList)
			if (strcmp(e.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
				debugUtils = true;
		if (debugUtils && !layers.empty())
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		app.apiVersion = VK_API_VERSION_1_3;
		VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		ici.pApplicationInfo = &app;
		ici.enabledLayerCount = (uint32_t)layers.size();
		ici.ppEnabledLayerNames = layers.data();
		ici.enabledExtensionCount = (uint32_t)extensions.size();
		ici.ppEnabledExtensionNames = extensions.data();
		if (vkCreateInstance(&ici, nullptr, &h.instance) != VK_SUCCESS)
			return false;
		Load_Instance(h.instance);
		if (!extensions.empty() && vkCreateDebugUtilsMessengerEXT)
		{
			VkDebugUtilsMessengerCreateInfoEXT dbg = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
			dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
			dbg.pfnUserCallback = Host_Debug;
			vkCreateDebugUtilsMessengerEXT(h.instance, &dbg, nullptr, &h.messenger);
		}
		vkEnumeratePhysicalDevices(h.instance, &n, nullptr);
		if (n == 0)
			return false;
		std::vector<VkPhysicalDevice> devices(n);
		vkEnumeratePhysicalDevices(h.instance, &n, devices.data());
		h.phys = devices[0];
		vkEnumerateDeviceExtensionProperties(h.phys, nullptr, &n, nullptr);
		std::vector<VkExtensionProperties> devExt(n);
		vkEnumerateDeviceExtensionProperties(h.phys, nullptr, &n, devExt.data());
		auto has = [&](const char* name) { for (const VkExtensionProperties& e : devExt) if (strcmp(e.extensionName, name) == 0) return true; return false; };
		h.rayQuery = has(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) && has(VK_KHR_RAY_QUERY_EXTENSION_NAME) && has(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		vkGetPhysicalDeviceQueueFamilyProperties(h.phys, &n, nullptr);
		std::vector<VkQueueFamilyProperties> families(n);
		vkGetPhysicalDeviceQueueFamilyProperties(h.phys, &n, families.data());
		for (uint32_t i = 0; i < n; ++i)
			if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				h.family = i;
				break;
			}
		// the features DXVK 2 makes its device with, and what the ray queries add
		VkPhysicalDeviceRayQueryFeaturesKHR rq = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
		rq.rayQuery = VK_TRUE;
		VkPhysicalDeviceAccelerationStructureFeaturesKHR as = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
		as.accelerationStructure = VK_TRUE;
		as.pNext = &rq;
		VkPhysicalDeviceVulkan13Features v13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		v13.dynamicRendering = VK_TRUE;
		v13.synchronization2 = VK_TRUE;
		v13.pNext = h.rayQuery ? &as : nullptr;
		VkPhysicalDeviceVulkan12Features v12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		v12.timelineSemaphore = VK_TRUE;
		v12.bufferDeviceAddress = h.rayQuery ? VK_TRUE : VK_FALSE;
		v12.pNext = &v13;
		VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		f2.pNext = &v12;
		float priority = 1.0f;
		VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		qci.queueFamilyIndex = h.family;
		qci.queueCount = 1;
		qci.pQueuePriorities = &priority;
		const char* rtExtensions[] = { VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_QUERY_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME };
		VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		dci.pNext = &f2;
		dci.queueCreateInfoCount = 1;
		dci.pQueueCreateInfos = &qci;
		dci.enabledExtensionCount = h.rayQuery ? 3 : 0;
		dci.ppEnabledExtensionNames = rtExtensions;
		if (vkCreateDevice(h.phys, &dci, nullptr, &h.device) != VK_SUCCESS)
			return false;
		if (!Load_Device(h.device))
			return false;
		vkGetDeviceQueue(h.device, h.family, 0, &h.queue);
		VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		pci.queueFamilyIndex = h.family;
		vkCreateCommandPool(h.device, &pci, nullptr, &h.pool);

		// its image, which the shared device writes, and a buffer it reads the image back into
		VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		ii.imageType = VK_IMAGE_TYPE_2D;
		ii.format = VK_FORMAT_R8G8B8A8_UNORM;
		ii.extent = { 32, 32, 1 };
		ii.mipLevels = 1;
		ii.arrayLayers = 1;
		ii.samples = VK_SAMPLE_COUNT_1_BIT;
		ii.tiling = VK_IMAGE_TILING_OPTIMAL;
		ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		vkCreateImage(h.device, &ii, nullptr, &h.image);
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(h.device, h.image, &req);
		VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		mai.allocationSize = req.size;
		mai.memoryTypeIndex = Host_Memory(h, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkAllocateMemory(h.device, &mai, nullptr, &h.imageMemory);
		vkBindImageMemory(h.device, h.image, h.imageMemory, 0);
		VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bci.size = 32 * 32 * 4;
		bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vkCreateBuffer(h.device, &bci, nullptr, &h.buffer);
		vkGetBufferMemoryRequirements(h.device, h.buffer, &req);
		mai.allocationSize = req.size;
		mai.memoryTypeIndex = Host_Memory(h, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkAllocateMemory(h.device, &mai, nullptr, &h.bufferMemory);
		vkBindBufferMemory(h.device, h.buffer, h.bufferMemory, 0);
		// the host keeps it ready for sampling, as DXVK keeps a texture
		Host_Run(h, [&](VkCommandBuffer cmd) { Host_Image_Barrier(cmd, h.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); });
		return true;
	}

	void Host_Destroy(Host& h)
	{
		using namespace rhi::vk;
		if (h.device)
		{
			vkDeviceWaitIdle(h.device);
			vkDestroyCommandPool(h.device, h.pool, nullptr);
			vkDestroyBuffer(h.device, h.buffer, nullptr);
			vkFreeMemory(h.device, h.bufferMemory, nullptr);
			vkDestroyImage(h.device, h.image, nullptr);
			vkFreeMemory(h.device, h.imageMemory, nullptr);
			vkDestroyDevice(h.device, nullptr);
		}
		if (h.instance)
		{
			if (h.messenger)
				vkDestroyDebugUtilsMessengerEXT(h.instance, h.messenger, nullptr);
			vkDestroyInstance(h.instance, nullptr);
		}
	}

	void Test_Shared_Image(Device* dev, Host& h)
	{
		printf("shared image\n");
		ExternalTexture e;
		e.image = (uint64_t)h.image;
		e.vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
		e.width = e.height = 32;
		e.usage = TEXTURE_SAMPLED | TEXTURE_STORAGE | TEXTURE_COPY_SRC;
		e.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		e.name = "the host's image";
		Texture* image = dev->Import_Texture(e);
		Shader* cs = Make_Shader(dev, Stage::COMPUTE, spv_selftest_image, sizeof(spv_selftest_image), "main", "selftest image");
		ComputePipelineDesc cd;
		cd.cs = cs;
		Pipeline* pipeline = dev->Create_Compute_Pipeline(cd);
		Check(image && cs && pipeline, "the host's image imported");
		if (!image || !pipeline)
			return;
		const int locksBefore = h.locks;
		CommandList* cl = dev->Begin_Frame();
		cl->Set_Pipeline(pipeline);
		cl->Set_Storage_Image(0, image);
		const uint32_t push[4] = { 32, 32, 200, 0 };
		cl->Set_Push_Constants(push, sizeof(push));
		cl->Dispatch(4, 4);
		dev->Submit_Frame();
		dev->Wait_Idle();
		Check(h.locks == locksBefore + 1 && h.unlocks == h.locks, "the shared queue was locked around the submission (%d locks, %d unlocks)", h.locks, h.unlocks);
		// the host takes it back, in the layout it left it in, and reads it
		using namespace rhi::vk;
		Host_Run(h, [&](VkCommandBuffer cmd)
		{
			Host_Image_Barrier(cmd, h.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			VkBufferImageCopy region = {};
			region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.imageExtent = { 32, 32, 1 };
			vkCmdCopyImageToBuffer(cmd, h.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, h.buffer, 1, &region);
			Host_Image_Barrier(cmd, h.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});
		void* mapped = nullptr;
		vkMapMemory(h.device, h.bufferMemory, 0, VK_WHOLE_SIZE, 0, &mapped);
		const Pixel* px = (const Pixel*)mapped;
		uint32_t wrong = 0;
		for (uint32_t y = 0; y < 32; ++y)
			for (uint32_t x = 0; x < 32; ++x)
				if (!Near(px[y * 32 + x], (int)(x * 4), (int)(y * 4), 200))
					++wrong;
		vkUnmapMemory(h.device, h.bufferMemory);
		Check(wrong == 0, "the host reads what the dispatch wrote, in its own layout (%u of 1024 pixels wrong)", wrong);
		dev->Destroy(pipeline);
		dev->Destroy(cs);
		dev->Destroy(image);
	}

	void Run_All(Device* dev, const char* ppm)
	{
		const Capabilities& caps = dev->Get_Capabilities();
		printf("device: %s%s%s%s\n", caps.deviceName, caps.software ? " (software)" : "", caps.rayQuery ? ", ray queries" : ", no ray queries",
			caps.external ? ", shared" : "");
		Test_Raster(dev, ppm);
		Test_Compute(dev);
		if (caps.rayQuery)
			Test_Ray_Query(dev);
		else
			printf("ray queries: not on this device, skipped\n");
		Test_Frames(dev);
	}

	void Test_Shared(bool validation)
	{
		printf("--- a shared device\n");
		Host h;
		if (!Host_Create(h, validation))
		{
			Check(false, "the host device could not be made");
			Host_Destroy(h);
			return;
		}
		VulkanExternal e;
		e.instance = h.instance;
		e.physicalDevice = h.phys;
		e.device = h.device;
		e.queue = h.queue;
		e.queueFamily = h.family;
		e.rayQuery = h.rayQuery;
		e.lockQueue = [](void* user) { ++((Host*)user)->locks; };
		e.unlockQueue = [](void* user) { ++((Host*)user)->unlocks; };
		e.user = &h;
		e.log = Log;
		Device* dev = Create_Vulkan_Device_External(e);
		Check(dev != nullptr, "a device on the host's");
		if (dev)
		{
			Run_All(dev, nullptr);
			Test_Shared_Image(dev, h);
			dev->Wait_Idle();
			delete dev;
		}
		Host_Destroy(h);
	}
}

int main(int argc, char** argv)
{
	VulkanOptions options;
	options.log = Log;
	bool requireRayQuery = false;
	bool shared = true;
	const char* ppm = nullptr;
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--validation") == 0)
			options.validation = true;
		else if (strcmp(argv[i], "--require-ray-query") == 0)
			requireRayQuery = true;
		else if (strcmp(argv[i], "--ppm") == 0 && i + 1 < argc)
			ppm = argv[++i];
		else if (strcmp(argv[i], "--no-shared") == 0)
			shared = false;
	}
	options.requireRayQuery = requireRayQuery;
	Device* dev = Create_Vulkan_Device(options);
	if (dev == nullptr)
	{
		printf("SKIP: no Vulkan 1.3 device%s\n", requireRayQuery ? " with ray queries" : "");
		return 77;
	}
	Run_All(dev, ppm);
	dev->Wait_Idle();
	delete dev;
	if (shared)
		Test_Shared(options.validation);
	if (options.validation)
		Check(g_validationMessages == 0, "%d validation messages", g_validationMessages);
	printf("%s: %d failed\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
	return g_failures == 0 ? 0 : 1;
}
