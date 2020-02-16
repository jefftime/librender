/* Copyright 2019, Jeffery Stager
 *
 * This file is part of librender.
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

/* libwindow */
struct window;

#define RENDER_ERROR_NONE                              0
#define RENDER_ERROR_MEMORY                           -1
#define RENDER_ERROR_FILE                             -2
#define RENDER_ERROR_NULL                             -3
#define RENDER_ERROR_VULKAN_LOAD                      -4
#define RENDER_ERROR_VULKAN_INSTANCE                  -5
#define RENDER_ERROR_VULKAN_PREINST_LOAD              -6
#define RENDER_ERROR_VULKAN_INST_LOAD                 -7
#define RENDER_ERROR_VULKAN_PHYSICAL_DEVICE           -8
#define RENDER_ERROR_VULKAN_NO_DEVICES                -9
#define RENDER_ERROR_VULKAN_INSTANCE_FUNC_LOAD        -10
#define RENDER_ERROR_VULKAN_DEVICE_FUNC_LOAD          -11
#define RENDER_ERROR_VULKAN_SURFACE                   -12
#define RENDER_ERROR_SURFACE_CAPS_IMAGE_USAGE         -13
#define RENDER_ERROR_VULKAN_QUEUE_INDICES             -14
#define RENDER_ERROR_VULKAN_QUEUE_INDEX_MISMATCH      -15
#define RENDER_ERROR_VULKAN_CREATE_DEVICE             -16
#define RENDER_ERROR_VULKAN_SURFACE_FORMAT            -17
#define RENDER_ERROR_VULKAN_FORMAT_PROPERTIES_LINEAR  -18
#define RENDER_ERROR_VULKAN_FORMAT_PROPERTIES_OPTIMAL -19
#define RENDER_ERROR_VULKAN_FORMAT_PROPERTIES_BUFFER  -20
#define RENDER_ERROR_VULKAN_SURFACE_CAPABILITIES      -21
#define RENDER_ERROR_VULKAN_SWAPCHAIN                 -22
#define RENDER_ERROR_VULKAN_SHADER_MODULE             -23
#define RENDER_ERROR_VULKAN_SHADER_READ               -24
#define RENDER_ERROR_VULKAN_DESCRIPTOR_SET_LAYOUT     -25
#define RENDER_ERROR_VULKAN_PIPELINE_LAYOUT           -26
#define RENDER_ERROR_VULKAN_CREATE_PIPELINE           -27
#define RENDER_ERROR_VULKAN_RENDER_PASS               -28
#define RENDER_ERROR_VULKAN_SWAPCHAIN_IMAGES          -29
#define RENDER_ERROR_VULKAN_IMAGE_VIEW                -30
#define RENDER_ERROR_VULKAN_FRAMEBUFFER               -31
#define RENDER_ERROR_VULKAN_COMMAND_POOL              -32
#define RENDER_ERROR_VULKAN_COMMAND_BUFFER            -33
#define RENDER_ERROR_VULKAN_BUFFER                    -34
#define RENDER_ERROR_VULKAN_MEMORY                    -35
#define RENDER_ERROR_VULKAN_MEMORY_MAP                -36
#define RENDER_ERROR_VULKAN_COMMAND_BUFFER_BEGIN      -37
#define RENDER_ERROR_VULKAN_COMMAND_BUFFER_END        -38
#define RENDER_ERROR_VULKAN_SEMAPHORE                 -39
#define RENDER_ERROR_VULKAN_ACQUIRE_IMAGE             -40
#define RENDER_ERROR_VULKAN_QUEUE_SUBMIT              -41
#define RENDER_ERROR_VULKAN_QUEUE_PRESENT             -42

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
  vkfunc(vkCreateDescriptorSetLayout);
  vkfunc(vkDestroyDescriptorSetLayout);

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
  VkDescriptorSetLayout descriptor_set_layout;

  /* Pipeline */
  int has_pipeline;
  /* VkShaderModule vert_module; */
  /* VkShaderModule frag_module; */
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
int render_init(struct render *r, struct window *w);
void render_deinit(struct render *r);
int render_configure(
  struct render *r,
  unsigned int width,
  unsigned int height,
  char *vshader,
  char *fshader
);
void render_destroy_pipeline(struct render *r);
int render_update(struct render *r);
int render_draw(struct render *r);
int render_load(struct render *r, size_t n, void *data);
/* **************************************** */

#endif
