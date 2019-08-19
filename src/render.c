/* This file is a part of librender
 *
 * Copyright 2019, Jeffery Stager
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

#include "render.h"

/* libwindow */
xcb_connection_t *window_xcb_connection(struct window *w);
xcb_window_t window_xcb_window(struct window *w);

#include <error.h>
#include <stdint.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#ifdef TARGET_OS_LINUX
#include <vulkan/vulkan_xcb.h>
#endif

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int load_vulkan(struct render *r) {
  r->vklib = dlopen("libvulkan.so", RTLD_NOW);
  if (!r->vklib) return RENDER_ERROR_VULKAN_LOAD;
  /* This gets around the -pedantic error */
  *(PFN_vkGetInstanceProcAddr **) (&r->vkGetInstanceProcAddr) =
    dlsym(r->vklib, "vkGetInstanceProcAddr");
  return RENDER_ERROR_NONE;
}

static int load_preinstance_functions(struct render *r) {
#define load(f) \
  if (!(r->f = (PFN_##f) r->vkGetInstanceProcAddr(NULL, #f))) \
    return RENDER_ERROR_VULKAN_PREINST_LOAD;

  load(vkCreateInstance);
  return RENDER_ERROR_NONE;

#undef load
}

static int create_instance(struct render *r) {
  char *extensions[] = {
    "VK_KHR_surface",
    "VK_KHR_xcb_surface"
  };
  VkInstanceCreateInfo create_info = { 0 };
  VkResult result;

  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.enabledExtensionCount = 2;
  create_info.ppEnabledExtensionNames = (const char * const *) extensions;
  result = r->vkCreateInstance(&create_info, NULL, &r->instance);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_INSTANCE;
  return RENDER_ERROR_NONE;
}

static int load_instance_functions(struct render *r) {
#define load(f) \
  if (!(r->f = (PFN_##f) r->vkGetInstanceProcAddr(r->instance, #f))) \
    return RENDER_ERROR_VULKAN_INSTANCE_FUNC_LOAD;

  load(vkGetDeviceProcAddr);
  load(vkEnumeratePhysicalDevices);
  load(vkGetPhysicalDeviceProperties);
  load(vkCreateXcbSurfaceKHR);
  load(vkGetPhysicalDeviceQueueFamilyProperties);
  load(vkCreateDevice);
  load(vkGetDeviceQueue);
  load(vkGetPhysicalDeviceSurfaceSupportKHR);
  load(vkGetPhysicalDeviceSurfaceFormatsKHR);
  load(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
  load(vkGetPhysicalDeviceMemoryProperties);
  load(vkCreateImageView);
  load(vkCreateFramebuffer);
  load(vkCreateCommandPool);
  load(vkDestroyDevice);
  load(vkDestroySwapchainKHR);
  load(vkDestroySurfaceKHR);
  load(vkDestroyInstance);
  load(vkDestroyImageView);
  load(vkDestroyFramebuffer);
  load(vkDestroyCommandPool);
  load(vkFreeCommandBuffers);
  return RENDER_ERROR_NONE;

#undef load
}

static int get_devices(struct render *r) {
  uint32_t n_devices;
  VkResult result;

  result = r->vkEnumeratePhysicalDevices(r->instance, &n_devices, NULL);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_PHYSICAL_DEVICE;
  if (n_devices == 0) return RENDER_ERROR_VULKAN_NO_DEVICES;
  r->n_devices = n_devices;
  r->phys_devices = malloc(sizeof(VkPhysicalDevice) * n_devices);
  if (!r->phys_devices) return RENDER_ERROR_MEMORY;
  result = r->vkEnumeratePhysicalDevices(r->instance,
                                         &n_devices,
                                         r->phys_devices);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_PHYSICAL_DEVICE;
  return RENDER_ERROR_NONE;
}

static int load_device_functions(struct render *r) {
#define load(f) \
  if (!(r->f = (PFN_##f) r->vkGetDeviceProcAddr(r->device, #f))) \
    return RENDER_ERROR_VULKAN_DEVICE_FUNC_LOAD;

  load(vkCreateSwapchainKHR);
  load(vkCreateShaderModule);
  load(vkCreatePipelineLayout);
  load(vkCreateRenderPass);
  load(vkCreateGraphicsPipelines);
  load(vkGetSwapchainImagesKHR);
  load(vkAllocateCommandBuffers);
  load(vkCreateBuffer);
  load(vkGetBufferMemoryRequirements);
  load(vkAllocateMemory);
  load(vkBindBufferMemory);
  load(vkMapMemory);
  load(vkFlushMappedMemoryRanges);
  load(vkInvalidateMappedMemoryRanges);
  load(vkUnmapMemory);
  load(vkBeginCommandBuffer);
  load(vkCmdBeginRenderPass);
  load(vkCmdBindPipeline);
  load(vkCmdBindVertexBuffers);
  load(vkCmdBindIndexBuffer);
  load(vkCmdDrawIndexed);
  load(vkCmdEndRenderPass);
  load(vkEndCommandBuffer);
  load(vkCreateSemaphore);
  load(vkAcquireNextImageKHR);
  load(vkQueueSubmit);
  load(vkQueuePresentKHR);
  load(vkQueueWaitIdle);
  load(vkDestroyShaderModule);
  load(vkDestroyRenderPass);
  load(vkDestroyPipelineLayout);
  load(vkDestroyPipeline);
  load(vkFreeCommandBuffers);
  load(vkDestroyBuffer);
  load(vkFreeMemory);
  load(vkDestroySemaphore);
  return RENDER_ERROR_NONE;

#undef load
}

static int create_surface(struct render *r, struct window *w) {
  VkXcbSurfaceCreateInfoKHR create_info = { 0 };
  VkResult result;

  create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  create_info.connection = window_xcb_connection(w);
  create_info.window = window_xcb_window(w);
  result = r->vkCreateXcbSurfaceKHR(r->instance,
                                    &create_info,
                                    NULL,
                                    &r->surface);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_SURFACE;
  return RENDER_ERROR_NONE;
}

static int get_queue_props(struct render *r) {
  uint32_t n_props;
  r->vkGetPhysicalDeviceQueueFamilyProperties(r->phys_devices[r->phys_id],
                                              &n_props,
                                              NULL);
  if (n_props == 0) return RENDER_ERROR_VULKAN_QUEUE_INDICES;
  r->n_queue_props = n_props;
  r->queue_props = malloc(sizeof(VkQueueFamilyProperties) * n_props);
  if (!r->queue_props) return RENDER_ERROR_MEMORY;
  r->vkGetPhysicalDeviceQueueFamilyProperties(r->phys_devices[r->phys_id],
                                              &n_props,
                                              r->queue_props);
  return RENDER_ERROR_NONE;
}

static int get_present_and_graphics_indices(struct render *r) {
  int graphics_isset = 0;
  int present_isset = 0;
  size_t i;
  VkResult result;

  for (i = 0; i < r->n_queue_props; ++i) {
    uint32_t present_support = 0;

    if (((r->queue_props[i].queueCount > 0) &&
         (r->queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))) {
      graphics_isset = 1;
      r->queue_index_graphics = i;
    }
    result =
      r->vkGetPhysicalDeviceSurfaceSupportKHR(r->phys_devices[r->phys_id],
                                              (uint32_t) i,
                                              r->surface,
                                              &present_support);
    if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_QUEUE_INDICES;
    if ((r->queue_props[i].queueCount > 0) && present_support) {
      present_isset = 1;
      r->queue_index_present = i;
    }
  }
  return RENDER_ERROR_NONE;
}

static int create_device(struct render *r) {
  char *extensions[] = { "VK_KHR_swapchain" };
  float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_create_infos[] = { { 0 }, { 0 } };
  VkDeviceCreateInfo create_info = { 0 };
  VkResult result;

  /* TODO: support separate graphics and present queues */
  if (r->queue_index_present != r->queue_index_graphics) {
    return RENDER_ERROR_VULKAN_QUEUE_INDEX_MISMATCH;
  }
  queue_create_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_infos[0].queueFamilyIndex = (uint32_t) r->queue_index_graphics;
  queue_create_infos[0].queueCount = 1;
  queue_create_infos[0].pQueuePriorities = &queue_priority;
  queue_create_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_infos[1].queueFamilyIndex = (uint32_t) r->queue_index_present;
  queue_create_infos[1].queueCount = 1;
  queue_create_infos[1].pQueuePriorities = &queue_priority;
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.queueCreateInfoCount = 2;
  create_info.pQueueCreateInfos = queue_create_infos;
  create_info.enabledExtensionCount = 1;
  create_info.ppEnabledExtensionNames = (const char * const *) extensions;
  result = r->vkCreateDevice(r->phys_devices[r->phys_id],
                             &create_info,
                             NULL,
                             &r->device);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_CREATE_DEVICE;
  r->vkGetDeviceQueue(r->device,
                      (uint32_t) r->queue_index_graphics,
                      0,
                      &r->graphics_queue);
  r->vkGetDeviceQueue(r->device,
                      (uint32_t) r->queue_index_present,
                      0,
                      &r->present_queue);
  return RENDER_ERROR_NONE;
}

static int get_surface_format(struct render *r) {
  uint32_t n_formats;
  VkSurfaceFormatKHR *formats;
  VkResult result;

  result = r->vkGetPhysicalDeviceSurfaceFormatsKHR(r->phys_devices[r->phys_id],
                                                   r->surface,
                                                   &n_formats,
                                                   NULL);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_SURFACE_FORMAT;
  formats = malloc(sizeof(VkSurfaceFormatKHR) * n_formats);
  if (!formats) return RENDER_ERROR_MEMORY;
  result = r->vkGetPhysicalDeviceSurfaceFormatsKHR(r->phys_devices[r->phys_id],
                                                   r->surface,
                                                   &n_formats,
                                                   formats);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_SURFACE_FORMAT;
  r->format = formats[0];
  free(formats);
  return RENDER_ERROR_NONE;
}

static int get_surface_caps(struct render *r,
                            VkSurfaceCapabilitiesKHR *out_caps) {
  VkSurfaceCapabilitiesKHR caps = { 0 };
  VkResult result;

  result =
    r->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(r->phys_devices[r->phys_id],
                                                 r->surface,
                                                 &caps);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_SURFACE_CAPABILITIES;
  *out_caps = caps;
  return RENDER_ERROR_NONE;
}

static int get_swapchain_images(struct render *r) {
  uint32_t n_images;
  VkResult result;

  result = r->vkGetSwapchainImagesKHR(r->device,
                                      r->swapchain,
                                      &n_images,
                                      NULL);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_SWAPCHAIN_IMAGES;
  r->swapchain_images = malloc(sizeof(VkImage) * n_images);
  if (!r->swapchain_images) return RENDER_ERROR_MEMORY;
  result = r->vkGetSwapchainImagesKHR(r->device,
                                      r->swapchain,
                                      &n_images,
                                      r->swapchain_images);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_SWAPCHAIN_IMAGES;
  r->n_swapchain_images = n_images;
  return RENDER_ERROR_NONE;
}

static int create_swapchain(struct render *r) {
  VkSwapchainCreateInfoKHR create_info = { 0 };
  VkSurfaceCapabilitiesKHR caps;
  VkResult result;

  chkerr(get_surface_caps(r, &caps));
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = r->surface;
  create_info.minImageCount = 2;
  create_info.imageFormat = r->format.format;
  create_info.imageColorSpace = r->format.colorSpace;
  create_info.imageExtent = caps.currentExtent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = caps.supportedUsageFlags;
  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.preTransform = caps.currentTransform;
  create_info.compositeAlpha = caps.supportedCompositeAlpha;
  create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  create_info.clipped = VK_TRUE;
  result = r->vkCreateSwapchainKHR(r->device,
                                   &create_info,
                                   NULL,
                                   &r->swapchain);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_SWAPCHAIN;
  r->swap_extent = caps.currentExtent;
  chkerr(get_swapchain_images(r));
  return RENDER_ERROR_NONE;
}

static int read_shader(char *filename,
                       unsigned char **out,
                       size_t *out_len) {
  unsigned char *buf;
  long size;
  size_t read;
  FILE *f;

  f = fopen(filename, "rb");
  if (!f) return RENDER_ERROR_FILE;
  fseek(f, 0, SEEK_END);
  size = ftell(f);
  if (size % 4) return RENDER_ERROR_VULKAN_SHADER_READ;
  buf = malloc((size_t) size);
  if (!buf) return RENDER_ERROR_MEMORY;
  fseek(f, 0, SEEK_SET);
  read = fread(buf, 1, (size_t) size, f);
  if (read != size) return RENDER_ERROR_FILE;
  *out = buf;
  *out_len = (size_t) size;
  fclose(f);
  return RENDER_ERROR_NONE;
}

static int create_shader(struct render *r,
                         unsigned char *source,
                         size_t len,
                         VkShaderModule *out_module) {
  VkShaderModuleCreateInfo create_info = { 0 };
  VkResult result;

  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = len;
  create_info.pCode = (uint32_t *) source;
  result = r->vkCreateShaderModule(r->device,
                                   &create_info,
                                   NULL,
                                   out_module);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_SHADER_MODULE;
  return 0;
}

static int create_pipeline_layout(struct render *r,
                                  VkPipelineLayout *out_layout) {
  VkResult result;
  VkPipelineLayoutCreateInfo create_info = {
    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
  };

  result = r->vkCreatePipelineLayout(r->device,
                                     &create_info,
                                     NULL,
                                     out_layout);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_PIPELINE_LAYOUT;
  return RENDER_ERROR_NONE;
}

static int create_render_pass(struct render *r) {
  VkResult result;
  VkSubpassDependency dependency = {
    VK_SUBPASS_EXTERNAL,
    0,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    0,
    (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
    0
  };
  VkAttachmentDescription attachment = { 0 };
  VkAttachmentReference attachment_ref = {
    0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  };
  VkSubpassDescription subpass = { 0 };
  VkRenderPassCreateInfo create_info = { 0 };

  attachment.format = r->format.format;
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &attachment_ref;
  create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  create_info.attachmentCount = 1;
  create_info.pAttachments = &attachment;
  create_info.subpassCount = 1;
  create_info.pSubpasses = &subpass;
  create_info.dependencyCount = 1;
  create_info.pDependencies = &dependency;
  result = r->vkCreateRenderPass(r->device,
                                 &create_info,
                                 NULL,
                                 &r->render_pass);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_RENDER_PASS;
  return RENDER_ERROR_NONE;
}

static int create_pipeline(struct render *r,
                           char *vshader,
                           char *fshader) {
  unsigned char *vert_shader, *frag_shader;
  size_t vlen, flen;
  VkPipelineShaderStageCreateInfo shader_info[] = { { 0 }, { 0 } };

  VkVertexInputBindingDescription bindings[] = {
    { 0, sizeof(float) * 6, VK_VERTEX_INPUT_RATE_VERTEX }
  };
  VkVertexInputAttributeDescription attrs[] = {
    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
    { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3 }
  };
  VkPipelineVertexInputStateCreateInfo vertex_info = { 0 };

  VkPipelineInputAssemblyStateCreateInfo assembly_info = { 0 };

  VkViewport viewport = { 0 };
  VkRect2D scissor = { 0 };

  VkPipelineViewportStateCreateInfo viewport_info = { 0 };

  VkPipelineRasterizationStateCreateInfo raster_info = { 0 };

  VkPipelineMultisampleStateCreateInfo multisample_info = { 0 };

  VkPipelineDepthStencilStateCreateInfo depth_info = { 0 };

  VkPipelineColorBlendAttachmentState color_attachment = { 0 };

  VkPipelineColorBlendStateCreateInfo color_info = { 0 };

  VkPipelineDynamicStateCreateInfo dynamic_info = { 0 };

  VkPipelineLayout layout;

  VkGraphicsPipelineCreateInfo graphics_pipeline = { 0 };

  VkResult result;

  chkerr(read_shader(vshader, &vert_shader, &vlen));
  chkerr(read_shader(fshader, &frag_shader, &flen));
  chkerr(create_shader(r, vert_shader, vlen, &r->vert_module));
  chkerr(create_shader(r, frag_shader, flen, &r->frag_module));
  free(vert_shader);
  free(frag_shader);
  shader_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shader_info[0].module = r->vert_module;
  shader_info[0].pName = "main";
  shader_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shader_info[1].module = r->frag_module;
  shader_info[1].pName = "main";

  vertex_info.sType =
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_info.vertexBindingDescriptionCount = 1;
  vertex_info.pVertexBindingDescriptions = bindings;
  vertex_info.vertexAttributeDescriptionCount =
    sizeof(attrs) / sizeof(attrs[0]);
  vertex_info.pVertexAttributeDescriptions = attrs;

  assembly_info.sType =
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  assembly_info.primitiveRestartEnable = VK_FALSE;

  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float) r->swap_extent.width;
  viewport.height = (float) r->swap_extent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = r->swap_extent.width;
  scissor.extent.height = r->swap_extent.height;

  viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_info.viewportCount = 1;
  viewport_info.pViewports = &viewport;
  viewport_info.scissorCount = 1;
  viewport_info.pScissors = &scissor;

  raster_info.sType =
    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster_info.depthClampEnable = VK_FALSE;
  raster_info.rasterizerDiscardEnable = VK_FALSE;
  raster_info.polygonMode = VK_POLYGON_MODE_FILL;
  raster_info.cullMode = VK_CULL_MODE_BACK_BIT;
  raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  raster_info.depthBiasEnable = VK_FALSE;

  multisample_info.sType =
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisample_info.sampleShadingEnable = VK_FALSE;
  multisample_info.minSampleShading = 1.0f;
  multisample_info.pSampleMask = NULL;
  multisample_info.alphaToCoverageEnable = VK_FALSE;
  multisample_info.alphaToOneEnable = VK_FALSE;

  depth_info.sType =
    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_info.depthTestEnable = VK_TRUE;
  depth_info.depthWriteEnable = VK_TRUE;
  depth_info.depthBoundsTestEnable = VK_FALSE;
  depth_info.stencilTestEnable = VK_FALSE;

  color_attachment.blendEnable = VK_FALSE;
  color_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                                  | VK_COLOR_COMPONENT_G_BIT
                                  | VK_COLOR_COMPONENT_B_BIT;

  color_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_info.logicOpEnable = VK_FALSE;
  color_info.attachmentCount = 1;
  color_info.pAttachments = &color_attachment;

  dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_info.dynamicStateCount = 0;

  chkerr(create_pipeline_layout(r, &layout));
  chkerr(create_render_pass(r));

  graphics_pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  graphics_pipeline.stageCount = sizeof(shader_info) / sizeof(shader_info[0]);
  graphics_pipeline.pStages = shader_info;
  graphics_pipeline.pVertexInputState = &vertex_info;
  graphics_pipeline.pInputAssemblyState = &assembly_info;
  graphics_pipeline.pViewportState = &viewport_info;
  graphics_pipeline.pRasterizationState = &raster_info;
  graphics_pipeline.pMultisampleState = &multisample_info;
  graphics_pipeline.pDepthStencilState = &depth_info;
  graphics_pipeline.pColorBlendState = &color_info;
  graphics_pipeline.pDynamicState = &dynamic_info;
  graphics_pipeline.layout = layout;
  graphics_pipeline.renderPass = r->render_pass;
  graphics_pipeline.subpass = 0;
  graphics_pipeline.basePipelineHandle = VK_NULL_HANDLE;
  graphics_pipeline.basePipelineIndex = -1;

  result = r->vkCreateGraphicsPipelines(r->device,
                                        VK_NULL_HANDLE,
                                        1,
                                        &graphics_pipeline,
                                        NULL,
                                        &r->pipeline);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_CREATE_PIPELINE;
  r->vkDestroyPipelineLayout(r->device, layout, NULL);

  return 0;
}

static int create_image_view(struct render *r,
                             size_t swapchain_index,
                             VkImageView *out_view) {
  VkImageViewCreateInfo create_info = { 0 };
  VkResult result;

  create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  create_info.image = r->swapchain_images[swapchain_index];
  create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  create_info.format = r->format.format;
  create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  create_info.subresourceRange.baseMipLevel = 0;
  create_info.subresourceRange.levelCount = 1;
  create_info.subresourceRange.baseArrayLayer = 0;
  create_info.subresourceRange.layerCount = 1;
  result = r->vkCreateImageView(r->device, &create_info, NULL, out_view);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_IMAGE_VIEW;
  return RENDER_ERROR_NONE;
}

static int create_framebuffers(struct render *r) {
  size_t i;

  r->image_views = malloc(sizeof(VkImageView) * r->n_swapchain_images);
  if (!r->image_views) return RENDER_ERROR_MEMORY;
  for (i = 0; i < r->n_swapchain_images; ++i) {
    /* &r->image_views[i] */
    chkerr(create_image_view(r, i, r->image_views + i));
  }
  r->framebuffers = malloc(sizeof(VkFramebuffer) * r->n_swapchain_images);
  if (!r->framebuffers) return RENDER_ERROR_MEMORY;
  for (i = 0; i < r->n_swapchain_images; ++i) {
    VkFramebufferCreateInfo create_info = { 0 };
    VkFramebuffer fb;
    VkResult result;

    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = r->render_pass;
    create_info.attachmentCount = 1;
    /* &r->image_views[i] */
    create_info.pAttachments = r->image_views + i;
    create_info.width = r->swap_extent.width;
    create_info.height = r->swap_extent.height;
    create_info.layers = 1;
    result = r->vkCreateFramebuffer(r->device,
                                    &create_info,
                                    NULL,
                                    &fb);
    if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_FRAMEBUFFER;
    r->framebuffers[i] = fb;
  }
  return RENDER_ERROR_NONE;
}

static int create_command_pool(struct render *r) {
  VkCommandPoolCreateInfo create_info = { 0 };
  VkResult result;

  create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  create_info.queueFamilyIndex = (uint32_t) r->queue_index_graphics;
  result = r->vkCreateCommandPool(r->device,
                                  &create_info,
                                  NULL,
                                  &r->command_pool);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_COMMAND_POOL;
  return RENDER_ERROR_NONE;
}

static int create_command_buffers(struct render *r) {
  VkCommandBufferAllocateInfo allocate_info = { 0 };
  VkResult result;

  r->command_buffers = malloc(sizeof(VkCommandBuffer) * r->n_swapchain_images);
  if (!r->command_buffers) return RENDER_ERROR_MEMORY;
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = r->command_pool;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = (uint32_t) r->n_swapchain_images;
  result = r->vkAllocateCommandBuffers(r->device,
                                       &allocate_info,
                                       r->command_buffers);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_COMMAND_BUFFER;
  return RENDER_ERROR_NONE;
}

static int create_buffer(struct render *r,
                         VkBuffer *out_buf,
                         size_t size,
                         VkBufferUsageFlags flags) {
  VkBufferCreateInfo create_info = { 0 };
  VkResult result;

  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.size = size;
  create_info.usage = flags;
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  result = r->vkCreateBuffer(r->device, &create_info, NULL, out_buf);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_BUFFER;
  return RENDER_ERROR_NONE;
}

static uint32_t get_heap_index(struct render *r, VkMemoryPropertyFlags flags) {
  uint32_t i, index = 0;
  VkPhysicalDeviceMemoryProperties props;

  r->vkGetPhysicalDeviceMemoryProperties(r->phys_devices[r->phys_id], &props);
  for (i = 0; i < props.memoryTypeCount; ++i) {
    if (props.memoryTypes[i].propertyFlags & flags) {
      index = i;
      break;
    }
  }
  return index;
}

static int write_data(struct render *r,
                      VkDeviceMemory *mem,
                      void *data,
                      size_t size) {
  void *dst;
  VkMappedMemoryRange range = { 0 };
  VkResult result;

  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = *mem;
  range.offset = 0;
  range.size = size;
  result = r->vkMapMemory(r->device,
                          range.memory,
                          range.offset,
                          range.size,
                          0,
                          &dst);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_MEMORY_MAP;
  memcpy(dst, data, range.size);
  result = r->vkFlushMappedMemoryRanges(r->device, 1, &range);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_MEMORY_MAP;
  result = r->vkInvalidateMappedMemoryRanges(r->device, 1, &range);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_MEMORY_MAP;
  r->vkUnmapMemory(r->device, *mem);
  return RENDER_ERROR_NONE;
}

static int allocate_buffer(struct render *r,
                           VkBuffer *buf,
                           VkDeviceMemory *mem) {
  VkMemoryRequirements reqs;
  VkMemoryAllocateInfo allocate_info = { 0 };
  VkResult result;

  r->vkGetBufferMemoryRequirements(r->device, *buf, &reqs);
  allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate_info.allocationSize = reqs.size;
  allocate_info.memoryTypeIndex =
    get_heap_index(r, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  result = r->vkAllocateMemory(r->device, &allocate_info, NULL, mem);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_MEMORY;
  result = r->vkBindBufferMemory(r->device, *buf, *mem, 0);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_MEMORY;
  return RENDER_ERROR_NONE;
}

static int create_vertex_data(struct render *r) {
  size_t size_verts = sizeof(float) * 6 * 4;
  size_t size_indices = sizeof(uint16_t) * 3 * 2;
  float vertices[] = {
    -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
     1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
  };
  uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };

  chkerr(create_buffer(r,
                       &r->vertex_buffer,
                       size_verts,
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
  chkerr(create_buffer(r,
                       &r->index_buffer,
                       size_indices,
                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT));
  chkerr(allocate_buffer(r, &r->vertex_buffer, &r->vertex_memory));
  chkerr(allocate_buffer(r, &r->index_buffer, &r->index_memory));
  chkerr(write_data(r, &r->vertex_memory, vertices, size_verts));
  chkerr(write_data(r, &r->index_memory, indices, size_indices));
  return RENDER_ERROR_NONE;
}

static int write_buffers(struct render *r) {
  size_t i;
  VkCommandBufferBeginInfo begin_info = { 0 };
  VkResult result;

  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  for (i = 0; i < r->n_swapchain_images; ++i) {
    VkRenderPassBeginInfo render_info = { 0 };
    VkClearValue clear_value = { { { 0 } } };

    result = r->vkBeginCommandBuffer(r->command_buffers[i], &begin_info);
    if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_COMMAND_BUFFER_BEGIN;
    clear_value.color.float32[3] = 1.0f;
    render_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_info.renderPass = r->render_pass;
    render_info.framebuffer = r->framebuffers[i];
    render_info.renderArea.offset.x = 0;
    render_info.renderArea.offset.y = 0;
    render_info.renderArea.extent = r->swap_extent;
    render_info.clearValueCount = 1;
    render_info.pClearValues = &clear_value;
    r->vkCmdBeginRenderPass(r->command_buffers[i],
                            &render_info,
                            VK_SUBPASS_CONTENTS_INLINE);
    {
      VkDeviceSize offsets[] = { 0 };

      r->vkCmdBindPipeline(r->command_buffers[i],
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           r->pipeline);
      r->vkCmdBindVertexBuffers(r->command_buffers[i],
                                0,
                                1,
                                &r->vertex_buffer,
                                offsets);
      r->vkCmdBindIndexBuffer(r->command_buffers[i],
                              r->index_buffer,
                              0,
                              VK_INDEX_TYPE_UINT16);
      r->vkCmdDrawIndexed(r->command_buffers[i], 6, 1, 0, 0, 0);
    }
    r->vkCmdEndRenderPass(r->command_buffers[i]);
    result = r->vkEndCommandBuffer(r->command_buffers[i]);
    if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_COMMAND_BUFFER_END;
  }
  return RENDER_ERROR_NONE;
}

static int create_semaphores(struct render *r) {
  VkSemaphoreCreateInfo create_info = { 0 };
  VkResult result;

  create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  result = r->vkCreateSemaphore(r->device,
                                &create_info,
                                NULL,
                                &r->image_semaphore);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_SEMAPHORE;
  result = r->vkCreateSemaphore(r->device,
                                &create_info,
                                NULL,
                                &r->render_semaphore);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_SEMAPHORE;
  return RENDER_ERROR_NONE;
}

/* **************************************** */
/* Public */
/* **************************************** */

int render_init(struct render *r, struct window *w) {
  if (!r) return RENDER_ERROR_NULL;
  memset((unsigned char *) r, 0, sizeof(struct render));
  chkerr(load_vulkan(r));
  chkerr(load_preinstance_functions(r));
  chkerr(create_instance(r));
  chkerr(load_instance_functions(r));
  chkerr(create_surface(r, w));
  chkerr(get_devices(r));
  return RENDER_ERROR_NONE;
}

void render_deinit(struct render *r) {
  if (!r) return;
  render_destroy_pipeline(r);
  free(r->phys_devices);
  r->vkDestroySwapchainKHR(r->device, r->swapchain, NULL);
  r->vkDestroySurfaceKHR(r->instance, r->surface, NULL);
  r->vkDestroyDevice(r->device, NULL);
  r->vkDestroyInstance(r->instance, NULL);
  dlclose(r->vklib);
  memset((unsigned char *) r, 0, sizeof(struct render));
}

int render_configure(struct render *r,
                     unsigned int width,
                     unsigned int height,
                     char *vshader,
                     char *fshader) {
  if (!r) return RENDER_ERROR_NULL;
  render_destroy_pipeline(r);
  r->phys_id = 0;
  /* r->phys_id = phys_id; */
  chkerr(get_queue_props(r));
  chkerr(get_present_and_graphics_indices(r));
  chkerr(create_device(r));
  chkerr(load_device_functions(r));
  chkerr(get_surface_format(r));
  chkerr(create_swapchain(r));
  chkerr(create_pipeline(r, vshader, fshader));
  chkerr(create_framebuffers(r));
  chkerr(create_command_pool(r));
  chkerr(create_command_buffers(r));
  chkerr(create_vertex_data(r));
  chkerr(write_buffers(r));
  chkerr(create_semaphores(r));
  r->has_pipeline = 1;
  return RENDER_ERROR_NONE;
}

void render_destroy_pipeline(struct render *r) {
  if (!r) return;
  if (r->has_pipeline) {
    size_t i;

    r->vkDestroySemaphore(r->device, r->image_semaphore, NULL);
    r->vkDestroySemaphore(r->device, r->render_semaphore, NULL);
    r->vkDestroyBuffer(r->device, r->vertex_buffer, NULL);
    r->vkDestroyBuffer(r->device, r->index_buffer, NULL);
    r->vkFreeMemory(r->device, r->vertex_memory, NULL);
    r->vkFreeMemory(r->device, r->index_memory, NULL);
    r->vkFreeCommandBuffers(r->device,
                            r->command_pool,
                            (uint32_t) r->n_swapchain_images,
                            r->command_buffers);
    free(r->command_buffers);
    r->vkDestroyCommandPool(r->device, r->command_pool, NULL);
    for (i = 0; i < r->n_swapchain_images; ++i) {
      r->vkDestroyFramebuffer(r->device, r->framebuffers[i], NULL);
      r->vkDestroyImageView(r->device, r->image_views[i], NULL);
    }
    free(r->framebuffers);
    free(r->image_views);
    free(r->swapchain_images);
    r->vkDestroyShaderModule(r->device, r->vert_module, NULL);
    r->vkDestroyShaderModule(r->device, r->frag_module, NULL);
    r->vkDestroyPipeline(r->device, r->pipeline, NULL);
    r->vkDestroyRenderPass(r->device, r->render_pass, NULL);
    free(r->queue_props);
    r->has_pipeline = 0;
  }
}

int render_update(struct render *r) {
  uint32_t image_index;
  VkSubmitInfo submit_info = { 0 };
  VkPipelineStageFlags wait_stages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };
  VkPresentInfoKHR present_info = { 0 };
  VkResult result;

  result = r->vkAcquireNextImageKHR(r->device,
                                    r->swapchain,
                                    (uint64_t) 2e9L,
                                    r->image_semaphore,
                                    VK_NULL_HANDLE,
                                    &image_index);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_ACQUIRE_IMAGE;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &r->image_semaphore;
  submit_info.pWaitDstStageMask = wait_stages;
  submit_info.commandBufferCount = 1;
  /* &r->command_buffers[image_index] */
  submit_info.pCommandBuffers = r->command_buffers + image_index;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &r->render_semaphore;
  result = r->vkQueueSubmit(r->graphics_queue,
                            1,
                            &submit_info,
                            VK_NULL_HANDLE);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_QUEUE_SUBMIT;
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &r->render_semaphore;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &r->swapchain;
  present_info.pImageIndices = &image_index;
  result = r->vkQueuePresentKHR(r->present_queue, &present_info);
  if (result != VK_SUCCESS) return RENDER_ERROR_VULKAN_QUEUE_PRESENT;
  r->vkQueueWaitIdle(r->present_queue);
  return RENDER_ERROR_NONE;
}
