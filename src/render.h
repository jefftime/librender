/* This file is a part of librender
 *
 * Copyright date, Jeffery Stager
 *
 * librender is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * librender is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with librender.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef RENDER_H
#define RENDER_H

#include <sized_types.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#ifdef TARGET_OS_LINUX

typedef struct xcb_connection_t xcb_connection_t;
typedef uint32_t xcb_window_t;
typedef uint32_t xcb_visualid_t;
#include <vulkan/vulkan_xcb.h>

#endif /* TARGET_OS_LINUX */

struct window;

enum render_error {
  RENDER_ERROR_NONE = 0,
  RENDER_ERROR_MEMORY,
  RENDER_ERROR_FILE,
  RENDER_ERROR_NULL,
  RENDER_ERROR_VK_LOAD,
  RENDER_ERROR_VK_INSTANCE,
  RENDER_ERROR_VK_PREINST_LOAD,
  RENDER_ERROR_VK_INST_LOAD,
  RENDER_ERROR_VK_PHYSICAL_DEVICE,
  RENDER_ERROR_VK_NO_DEVICES,
  RENDER_ERROR_VULKAN_INSTANCE_FUNC_LOAD,
  RENDER_ERROR_VK_DEVICE_FUNC_LOAD,
  RENDER_ERROR_VK_SURFACE,
  RENDER_ERROR_VK_QUEUE_INDICES,
  RENDER_ERROR_VK_QUEUE_INDEX_MISMATCH,
  RENDER_ERROR_VULKAN_CREATE_DEVICE,
  RENDER_ERROR_VK_SURFACE_FORMAT,
  RENDER_ERROR_VK_SURFACE_CAPABILITIES,
  RENDER_ERROR_VK_SWAPCHAIN,
  RENDER_ERROR_VULKAN_SHADER_MODULE,
  RENDER_ERROR_VULKAN_PIPELINE_LAYOUT,
  RENDER_ERROR_VULKAN_RENDER_PASS,
  RENDER_ERROR_VULKAN_SWAPCHAIN_IMAGES,
  RENDER_ERROR_VULKAN_IMAGE_VIEW,
  RENDER_ERROR_VULKAN_FRAMEBUFFER,
  RENDER_ERROR_VULKAN_COMMAND_POOL,
  RENDER_ERROR_VULKAN_COMMAND_BUFFER,
  RENDER_ERROR_VULKAN_BUFFER,
  RENDER_ERROR_VULKAN_MEMORY,
  RENDER_ERROR_VULKAN_MEMORY_MAP,
  RENDER_ERROR_VULKAN_COMMAND_BUFFER_BEGIN,
  RENDER_ERROR_VULKAN_COMMAND_BUFFER_END,
  RENDER_ERROR_VULKAN_SEMAPHORE,
  RENDER_ERROR_VULKAN_ACQUIRE_IMAGE,
  RENDER_ERROR_VULKAN_QUEUE_SUBMIT,
  RENDER_ERROR_VULKAN_QUEUE_PRESENT
};

/* Easily get vulkan function definitions */
#define vkfunc(f) PFN_##f f

struct render {
  void *vklib;

  /* Pre-instance functions */
  vkfunc(vkGetInstanceProcAddr);
  vkfunc(vkCreateInstance);
  vkfunc(vkDestroyInstance);

  /* Instance functions */
  vkfunc(vkGetDeviceProcAddr);
  vkfunc(vkEnumeratePhysicalDevices);
  vkfunc(vkGetPhysicalDeviceProperties);
  vkfunc(vkCreateXcbSurfaceKHR);
  vkfunc(vkGetPhysicalDeviceQueueFamilyProperties);
  vkfunc(vkCreateDevice);
  vkfunc(vkGetDeviceQueue);
  vkfunc(vkGetPhysicalDeviceSurfaceSupportKHR);
  vkfunc(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
  vkfunc(vkGetPhysicalDeviceSurfaceFormatsKHR);
  vkfunc(vkGetPhysicalDeviceMemoryProperties);
  vkfunc(vkDestroyDevice);
  vkfunc(vkDestroySwapchainKHR);
  vkfunc(vkDestroySurfaceKHR);

  /* Device functions */
  vkfunc(vkCreateSwapchainKHR);
  vkfunc(vkCreateShaderModule);
  vkfunc(vkCreatePipelineLayout);
  vkfunc(vkCreateRenderPass);
  vkfunc(vkCreateGraphicsPipelines);
  vkfunc(vkGetSwapchainImagesKHR);
  vkfunc(vkCreateImageView);
  vkfunc(vkCreateFramebuffer);
  vkfunc(vkCreateCommandPool);
  vkfunc(vkAllocateCommandBuffers);
  vkfunc(vkCreateBuffer);
  vkfunc(vkGetBufferMemoryRequirements);
  vkfunc(vkAllocateMemory);
  vkfunc(vkBindBufferMemory);
  vkfunc(vkMapMemory);
  vkfunc(vkFlushMappedMemoryRanges);
  vkfunc(vkInvalidateMappedMemoryRanges);
  vkfunc(vkUnmapMemory);
  vkfunc(vkBeginCommandBuffer);
  vkfunc(vkCmdBeginRenderPass);
  vkfunc(vkCmdBindPipeline);
  vkfunc(vkCmdBindVertexBuffers);
  vkfunc(vkCmdBindIndexBuffer);
  vkfunc(vkCmdDrawIndexed);
  vkfunc(vkCmdEndRenderPass);
  vkfunc(vkEndCommandBuffer);
  vkfunc(vkCreateSemaphore);
  vkfunc(vkAcquireNextImageKHR);
  vkfunc(vkQueueSubmit);
  vkfunc(vkQueuePresentKHR);
  vkfunc(vkQueueWaitIdle);
  vkfunc(vkDestroyShaderModule);
  vkfunc(vkDestroyRenderPass);
  vkfunc(vkDestroyPipelineLayout);
  vkfunc(vkDestroyPipeline);
  vkfunc(vkDestroyImageView);
  vkfunc(vkDestroyFramebuffer);
  vkfunc(vkDestroyCommandPool);
  vkfunc(vkFreeCommandBuffers);
  vkfunc(vkDestroyBuffer);
  vkfunc(vkFreeMemory);
  vkfunc(vkDestroySemaphore);

  /* Vulkan state */
  VkInstance instance;
  size_t n_devices;
  size_t phys_id;
  VkPhysicalDevice *phys_devices;
  VkSurfaceKHR surface;
  size_t n_queue_props;
  size_t queue_index_graphics;
  size_t queue_index_present;
  VkQueueFamilyProperties *queue_props;
  VkDevice device;
  VkSurfaceFormatKHR format;
  VkSwapchainKHR swapchain;
  VkExtent2D swap_extent;
  VkBuffer vertex_buffer;
  VkBuffer index_buffer;
  VkDeviceMemory vertex_memory;
  VkDeviceMemory index_memory;
  VkQueue graphics_queue;
  VkQueue present_queue;

  /* Pipeline */
  int has_pipeline;
  VkShaderModule vert_module;
  VkShaderModule frag_module;
  VkRenderPass render_pass;
  VkPipeline pipeline;
  size_t n_swapchain_images;
  VkImage *swapchain_images;
  VkImageView *image_views;
  VkFramebuffer *framebuffers;
  VkCommandPool command_pool;
  VkCommandBuffer *command_buffers;
  VkSemaphore image_semaphore;
  VkSemaphore render_semaphore;
};

/* **************************************** */
/* render.c */
enum render_error render_init(struct render *r, struct window *w);
void render_deinit(struct render *r);
enum render_error render_create_pipeline(struct render *r,
                                         unsigned int width,
                                         unsigned int height,
                                         size_t phys_id,
                                         char *vshader,
                                         char *fshader);
void render_destroy_pipeline(struct render *r);
enum render_error render_update(struct render *r);
/* **************************************** */

#endif
