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

// Tiberium Project @feature The Vulkan entry points the RHI uses, loaded at run time from the
// system's Vulkan loader (vulkan-1.dll, libvulkan.so.1, libvulkan.1.dylib): the game links no
// Vulkan library and runs without one, falling back to Direct3D 9.

#pragma once

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

namespace rhi
{
namespace vk
{

#define RHI_VK_GLOBAL_FUNCTIONS(X) \
	X(vkCreateInstance) \
	X(vkEnumerateInstanceLayerProperties) \
	X(vkEnumerateInstanceExtensionProperties) \
	X(vkEnumerateInstanceVersion)

#define RHI_VK_INSTANCE_FUNCTIONS(X) \
	X(vkDestroyInstance) \
	X(vkEnumeratePhysicalDevices) \
	X(vkGetPhysicalDeviceProperties) \
	X(vkGetPhysicalDeviceProperties2) \
	X(vkGetPhysicalDeviceFeatures2) \
	X(vkGetPhysicalDeviceMemoryProperties) \
	X(vkGetPhysicalDeviceQueueFamilyProperties) \
	X(vkGetPhysicalDeviceFormatProperties) \
	X(vkEnumerateDeviceExtensionProperties) \
	X(vkCreateDevice) \
	X(vkGetDeviceProcAddr) \
	X(vkCreateDebugUtilsMessengerEXT) \
	X(vkDestroyDebugUtilsMessengerEXT)

#define RHI_VK_DEVICE_FUNCTIONS(X) \
	X(vkDestroyDevice) \
	X(vkGetDeviceQueue) \
	X(vkDeviceWaitIdle) \
	X(vkQueueSubmit2) \
	X(vkQueueWaitIdle) \
	X(vkCreateCommandPool) \
	X(vkDestroyCommandPool) \
	X(vkResetCommandPool) \
	X(vkAllocateCommandBuffers) \
	X(vkBeginCommandBuffer) \
	X(vkEndCommandBuffer) \
	X(vkCreateSemaphore) \
	X(vkDestroySemaphore) \
	X(vkWaitSemaphores) \
	X(vkGetSemaphoreCounterValue) \
	X(vkAllocateMemory) \
	X(vkFreeMemory) \
	X(vkMapMemory) \
	X(vkUnmapMemory) \
	X(vkCreateBuffer) \
	X(vkDestroyBuffer) \
	X(vkGetBufferMemoryRequirements) \
	X(vkBindBufferMemory) \
	X(vkGetBufferDeviceAddress) \
	X(vkCreateImage) \
	X(vkDestroyImage) \
	X(vkGetImageMemoryRequirements) \
	X(vkBindImageMemory) \
	X(vkCreateImageView) \
	X(vkDestroyImageView) \
	X(vkCreateSampler) \
	X(vkDestroySampler) \
	X(vkCreateShaderModule) \
	X(vkDestroyShaderModule) \
	X(vkCreateDescriptorSetLayout) \
	X(vkDestroyDescriptorSetLayout) \
	X(vkCreatePipelineLayout) \
	X(vkDestroyPipelineLayout) \
	X(vkCreatePipelineCache) \
	X(vkDestroyPipelineCache) \
	X(vkCreateGraphicsPipelines) \
	X(vkCreateComputePipelines) \
	X(vkDestroyPipeline) \
	X(vkCmdPipelineBarrier2) \
	X(vkCmdBeginRendering) \
	X(vkCmdEndRendering) \
	X(vkCmdBindPipeline) \
	X(vkCmdSetViewport) \
	X(vkCmdSetScissor) \
	X(vkCmdBindVertexBuffers) \
	X(vkCmdBindIndexBuffer) \
	X(vkCmdPushConstants) \
	X(vkCmdDraw) \
	X(vkCmdDrawIndexed) \
	X(vkCmdDispatch) \
	X(vkCmdCopyBuffer) \
	X(vkCmdCopyBufferToImage) \
	X(vkCmdCopyImageToBuffer) \
	X(vkCmdFillBuffer) \
	X(vkCreateDescriptorPool) \
	X(vkDestroyDescriptorPool) \
	X(vkResetDescriptorPool) \
	X(vkAllocateDescriptorSets) \
	X(vkUpdateDescriptorSets) \
	X(vkCmdBindDescriptorSets)

#define RHI_VK_DEVICE_EXTENSION_FUNCTIONS(X) \
	X(vkCreateAccelerationStructureKHR) \
	X(vkDestroyAccelerationStructureKHR) \
	X(vkGetAccelerationStructureBuildSizesKHR) \
	X(vkCmdBuildAccelerationStructuresKHR) \
	X(vkGetAccelerationStructureDeviceAddressKHR) \
	X(vkSetDebugUtilsObjectNameEXT) \
	X(vkCmdBeginDebugUtilsLabelEXT) \
	X(vkCmdEndDebugUtilsLabelEXT)

#define RHI_VK_DECLARE(name) extern PFN_##name name;
RHI_VK_GLOBAL_FUNCTIONS(RHI_VK_DECLARE)
RHI_VK_INSTANCE_FUNCTIONS(RHI_VK_DECLARE)
RHI_VK_DEVICE_FUNCTIONS(RHI_VK_DECLARE)
RHI_VK_DEVICE_EXTENSION_FUNCTIONS(RHI_VK_DECLARE)
#undef RHI_VK_DECLARE
extern PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;

/// The loader library and its global entry points; false when there is no Vulkan
bool Load_Library();
/// The instance's entry points (missing optional ones stay null)
void Load_Instance(VkInstance instance);
/// The device's entry points; false when a required one is missing
bool Load_Device(VkDevice device);

}	// namespace vk
}	// namespace rhi
