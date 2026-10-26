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

// Tiberium Project @feature Vulkan entry points loaded at run time (see vk_loader.h)

#include "vk_loader.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace rhi
{
namespace vk
{

#define RHI_VK_DEFINE(name) PFN_##name name = nullptr;
RHI_VK_GLOBAL_FUNCTIONS(RHI_VK_DEFINE)
RHI_VK_INSTANCE_FUNCTIONS(RHI_VK_DEFINE)
RHI_VK_DEVICE_FUNCTIONS(RHI_VK_DEFINE)
RHI_VK_DEVICE_EXTENSION_FUNCTIONS(RHI_VK_DEFINE)
#undef RHI_VK_DEFINE
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;

namespace
{
	void* s_library = nullptr;

	void* Open_Library()
	{
#if defined(_WIN32)
		return (void*)LoadLibraryA("vulkan-1.dll");
#elif defined(__APPLE__)
		void* lib = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
		if (lib == nullptr)
			lib = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
		return lib;
#else
		void* lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
		if (lib == nullptr)
			lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
		return lib;
#endif
	}

	void* Symbol(void* lib, const char* name)
	{
#if defined(_WIN32)
		return (void*)GetProcAddress((HMODULE)lib, name);
#else
		return dlsym(lib, name);
#endif
	}
}

bool Load_Library()
{
	if (vkGetInstanceProcAddr != nullptr)
		return true;
	if (s_library == nullptr)
		s_library = Open_Library();
	if (s_library == nullptr)
		return false;
	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)Symbol(s_library, "vkGetInstanceProcAddr");
	if (vkGetInstanceProcAddr == nullptr)
		return false;
#define RHI_VK_LOAD_GLOBAL(name) name = (PFN_##name)vkGetInstanceProcAddr(nullptr, #name);
	RHI_VK_GLOBAL_FUNCTIONS(RHI_VK_LOAD_GLOBAL)
#undef RHI_VK_LOAD_GLOBAL
	return vkCreateInstance != nullptr;
}

void Load_Instance(VkInstance instance)
{
#define RHI_VK_LOAD_INSTANCE(name) name = (PFN_##name)vkGetInstanceProcAddr(instance, #name);
	RHI_VK_INSTANCE_FUNCTIONS(RHI_VK_LOAD_INSTANCE)
#undef RHI_VK_LOAD_INSTANCE
}

bool Load_Device(VkDevice device)
{
	bool ok = true;
#define RHI_VK_LOAD_DEVICE(name) name = (PFN_##name)vkGetDeviceProcAddr(device, #name); ok = ok && name != nullptr;
	RHI_VK_DEVICE_FUNCTIONS(RHI_VK_LOAD_DEVICE)
#undef RHI_VK_LOAD_DEVICE
	// extensions: optional, null when the device does not have them
#define RHI_VK_LOAD_OPTIONAL(name) name = (PFN_##name)vkGetDeviceProcAddr(device, #name);
	RHI_VK_DEVICE_EXTENSION_FUNCTIONS(RHI_VK_LOAD_OPTIONAL)
#undef RHI_VK_LOAD_OPTIONAL
	return ok;
}

}	// namespace vk
}	// namespace rhi
