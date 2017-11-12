#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

#include "vke.h"
#include "utils.h"


void vkeFindPhy (VkEngine* e, VkPhysicalDeviceType phyType) {
    uint32_t gpu_count = 0;

    VK_CHECK_RESULT(vkEnumeratePhysicalDevices (e->inst, &gpu_count, NULL));

    VkPhysicalDevice phys[gpu_count];

    VK_CHECK_RESULT(vkEnumeratePhysicalDevices (e->inst, &gpu_count, &phys));

    for (int i=0; i<gpu_count; i++){
        VkPhysicalDeviceProperties phy;
        vkGetPhysicalDeviceProperties (phys[i], &phy);
        if (phy.deviceType & phyType){
            printf ("GPU: %s  vulkan:%d.%d.%d driver:%d\n", phy.deviceName,
                    phy.apiVersion>>22, phy.apiVersion>>12&2048, phy.apiVersion&8191,
                    phy.driverVersion);
            e->phy = phys[i];
            return ;
        }
    }
    fprintf (stderr, "No suitable GPU found\n");
    exit (-1);
}
void vkeFindQueueIdx (VkEngine* e, VkQueueFlags qType) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties (e->phy, &queue_family_count, NULL);
    VkQueueFamilyProperties qfams[queue_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties (e->phy, &queue_family_count, &qfams);

    for (int j=0; j<queue_family_count; j++){
        if (qfams[j].queueFlags&qType){
            e->qFam = j;
            return;
        }
    }
    fprintf (stderr, "No suitable Queue found\n");
    exit (-1);
}


void vkeInitDevice (VkEngine* e) {
    float queue_priorities[1] = {0.0};

    vkGetPhysicalDeviceMemoryProperties(e->phy, &e->memory_properties);
    vkGetPhysicalDeviceProperties(e->phy, &e->gpu_props);

    VkDeviceQueueCreateInfo queue_info = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                           .queueCount = 1,
                                           .queueFamilyIndex = e->qFam,
                                           .pQueuePriorities = queue_priorities };
    VkDeviceCreateInfo device_info = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                       .queueCreateInfoCount = 1,
                                       .pQueueCreateInfos = &queue_info};
    VK_CHECK_RESULT(vkCreateDevice(e->phy, &device_info, NULL, &e->dev));
}
void vkeInitCmdPool (VkEngine* e, uint32_t buffCount){
    VkCommandPoolCreateInfo cmd_pool_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                              .pNext = NULL,
                                              .queueFamilyIndex = e->qFam,
                                              .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
    assert (vkCreateCommandPool(e->dev, &cmd_pool_info, NULL, &e->cmdPool) == VK_SUCCESS);
}

void createSwapChain (VkEngine* e, VkFormat preferedFormat){
    // Ensure all operations on the device have been finished before destroying resources
    vkDeviceWaitIdle(e->dev);    

    VkSwapchainKHR oldSwapchain = e->swapChain;

    VkSurfaceCapabilitiesKHR surfCapabilities;
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(e->phy, e->surface, &surfCapabilities));
    assert (surfCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    // width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
    if (surfCapabilities.currentExtent.width == 0xFFFFFFFF) {
        // If the surface size is undefined, the size is set to
        // the size of the images requested
        if (e->width < surfCapabilities.minImageExtent.width)
            e->width = surfCapabilities.minImageExtent.width;
        else if (e->width > surfCapabilities.maxImageExtent.width)
            e->width = surfCapabilities.maxImageExtent.width;

        if (e->height < surfCapabilities.minImageExtent.height)
            e->height = surfCapabilities.minImageExtent.height;
        else if (e->height > surfCapabilities.maxImageExtent.height)
            e->height = surfCapabilities.maxImageExtent.height;
    } else {
        // If the surface size is defined, the swap chain size must match
        e->width = surfCapabilities.currentExtent.width;
        e->height= surfCapabilities.currentExtent.height;
    }

    uint32_t formatCount;
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR (e->phy, e->surface, &formatCount, NULL));
    assert (formatCount>0);
    VkSurfaceFormatKHR formats[formatCount];
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR (e->phy, e->surface, &formatCount, formats));

    int formatIdx = -1;
    for (int i=0; i<formatCount; i++){
        if (formats[i].format == preferedFormat) {
            formatIdx=i;
            break;
        }
    }
    assert (formatIdx>=0);

    e->format = preferedFormat;

    uint32_t presentModeCount;
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(e->phy, e->surface, &presentModeCount, NULL));
    assert (presentModeCount>0);
    VkPresentModeKHR presentModes[presentModeCount];
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(e->phy, e->surface, &presentModeCount, presentModes));

    VkSwapchainCreateInfoKHR createInfo = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                            .surface = e->surface,
                                            .minImageCount = surfCapabilities.minImageCount,
                                            .imageFormat = formats[formatIdx].format,
                                            .imageColorSpace = formats[formatIdx].colorSpace,
                                            .imageExtent = {e->width,e->height},
                                            .imageArrayLayers = 1,
                                            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                            .preTransform = surfCapabilities.currentTransform,
                                            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                                            .clipped = VK_TRUE,
                                            .oldSwapchain = oldSwapchain};

    VK_CHECK_RESULT(vkCreateSwapchainKHR (e->dev, &createInfo, NULL, &e->swapChain));

    if (oldSwapchain != VK_NULL_HANDLE)
    {
        for (uint32_t i = 0; i < e->imgCount; i++)
        {
            vkDestroyImageView(e->dev, e->ScBuffers[i].view, NULL);
            vkFreeCommandBuffers (e->dev, e->cmdPool, 1, &e->cmdBuffs[i]);
        }
        vkDestroySwapchainKHR(e->dev, oldSwapchain, NULL);
        free(e->ScBuffers);
        free(e->cmdBuffs);
    }



    VK_CHECK_RESULT(vkGetSwapchainImagesKHR(e->dev, e->swapChain, &e->imgCount, NULL));
    assert (e->imgCount>0);

    VkImage images[e->imgCount];
    VK_CHECK_RESULT(vkGetSwapchainImagesKHR(e->dev, e->swapChain, &e->imgCount,images));

    e->ScBuffers = (ImageBuffer*)malloc(sizeof(ImageBuffer)*e->imgCount);
    e->cmdBuffs = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*e->imgCount);

    for (int i=0; i<e->imgCount; i++) {
        ImageBuffer sc_buffer;

        VkImageViewCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                             .image = images[i],
                                             .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                             .format = e->format,
                                             .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
        assert (vkCreateImageView(e->dev, &createInfo, NULL, &sc_buffer.view) == VK_SUCCESS);
        sc_buffer.image = images[i];
        e->ScBuffers [i] = sc_buffer;
        e->cmdBuffs [i] = vkeCreateCmdBuff(e, 1);
    }
    e->currentScBufferIndex = 0;
    vkDeviceWaitIdle(e->dev);
}


VkCommandBuffer vkeCreateCmdBuff (VkEngine* e, uint32_t buffCount){
    VkCommandBuffer cmdBuff;
    VkCommandBufferAllocateInfo cmd = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                        .pNext = NULL,
                                        .commandPool = e->cmdPool,
                                        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                        .commandBufferCount = buffCount };
    assert (vkAllocateCommandBuffers (e->dev, &cmd, &cmdBuff) == VK_SUCCESS);
    return cmdBuff;
}

void EngineInit (VkEngine* engine) {
    glfwInit();
    assert (glfwVulkanSupported()==GLFW_TRUE);
    engine->ExtensionNames = glfwGetRequiredInstanceExtensions (&engine->EnabledExtensionsCount);

    engine->infos.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    engine->infos.pNext = NULL;
    engine->infos.pApplicationName = APP_SHORT_NAME;
    engine->infos.applicationVersion = 1;
    engine->infos.pEngineName = APP_SHORT_NAME;
    engine->infos.engineVersion = 1;
    engine->infos.apiVersion = VK_API_VERSION_1_0;
    engine->width = 600;
    engine->height = 480;

    const uint32_t enabledLayersCount = 1;
    const char* enabledLayers[] = {"VK_LAYER_LUNARG_core_validation"};

    VkInstanceCreateInfo inst_info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                       .pNext = NULL,
                                       .flags = 0,
                                       .pApplicationInfo = &engine->infos,
                                       .enabledExtensionCount = engine->EnabledExtensionsCount,
                                       .ppEnabledExtensionNames = engine->ExtensionNames,
                                       .enabledLayerCount = enabledLayersCount,
                                       .ppEnabledLayerNames = enabledLayers };

    VK_CHECK_RESULT(vkCreateInstance (&inst_info, NULL, &engine->inst));

    vkeFindPhy (engine, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
    vkeFindQueueIdx (engine, VK_QUEUE_GRAPHICS_BIT);

    vkeInitDevice (engine);

    assert (glfwGetPhysicalDevicePresentationSupport (engine->inst, engine->phy, engine->qFam)==GLFW_TRUE);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    engine->window = glfwCreateWindow(engine->width, engine->height, "Window Title", NULL, NULL);

    assert (glfwCreateWindowSurface(engine->inst, engine->window, NULL, &engine->surface)==VK_SUCCESS);

    VkBool32 isSupported;
    vkGetPhysicalDeviceSurfaceSupportKHR(engine->phy, engine->qFam, engine->surface, &isSupported);
    assert (isSupported && "vkGetPhysicalDeviceSurfaceSupportKHR");

    vkGetDeviceQueue(engine->dev, engine->qFam, 0, &engine->presentQueue);

    vkeInitCmdPool (engine, 1);

    engine->semaPresentEnd = vkeCreateSemaphore(engine);
    engine->semaDrawEnd = vkeCreateSemaphore(engine);
}

void EngineTerminate (VkEngine* engine) {

    vkDestroySemaphore(engine->dev, engine->semaDrawEnd, NULL);
    vkDestroySemaphore(engine->dev, engine->semaPresentEnd, NULL);

    for (int i=0; i<engine->imgCount; i++){
        vkDestroyImageView (engine->dev, engine->ScBuffers[i].view, NULL);
        vkFreeCommandBuffers (engine->dev, engine->cmdPool, 1, &engine->cmdBuffs[i]);
    }
    free (engine->ScBuffers);
    free (engine->cmdBuffs);
    vkDestroyCommandPool (engine->dev, engine->cmdPool, NULL);

    vkDestroySwapchainKHR(engine->dev, engine->swapChain, NULL);
    vkDestroyDevice (engine->dev, NULL);
    vkDestroySurfaceKHR (engine->inst, engine->surface, NULL);
    vkDestroyInstance (engine->inst, NULL);
    glfwDestroyWindow (engine->window);
    glfwTerminate ();
}

void set_image_layout(VkCommandBuffer cmdBuff, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout,
                      VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {
    VkImageMemoryBarrier image_memory_barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                  .oldLayout = old_image_layout,
                                                  .newLayout = new_image_layout,
                                                  .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                  .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                  .image = image,
                                                  .subresourceRange = {aspectMask,0,1,0,1}};

    switch (old_image_layout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        default:
            break;
    }

    switch (new_image_layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        default:
            break;
    }

    vkCmdPipelineBarrier(cmdBuff, src_stages, dest_stages, 0, 0, NULL, 0, NULL, 1, &image_memory_barrier);
}

bool memory_type_from_properties(VkEngine* e, uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex) {
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < e->memory_properties.memoryTypeCount; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((e->memory_properties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    // No memory types matched, return failure
    return false;
}

VkFence vkeCreateFence (VkEngine* e) {
    VkFence fence;
    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                    .pNext = NULL,
                                    .flags = 0 };
    VK_CHECK_RESULT(vkCreateFence(e->dev, &fenceInfo, NULL, &fence));
    return fence;
}
VkSemaphore vkeCreateSemaphore (VkEngine* e) {
    VkSemaphore semaphore;
    VkSemaphoreCreateInfo info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                           .pNext = NULL,
                                                           .flags = 0};
    VK_CHECK_RESULT(vkCreateSemaphore(e->dev, &info, NULL, &semaphore));
    return semaphore;
}

void vkeBeginCmd(VkCommandBuffer cmdBuff) {
    VkCommandBufferBeginInfo cmd_buf_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                              .pNext = NULL,
                                              .flags = 0,
                                              .pInheritanceInfo = NULL };

    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuff, &cmd_buf_info));
}
void vkeSubmitCmd(VkQueue queue, VkCommandBuffer *pCmdBuff, VkFence fence){
    VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                 .commandBufferCount = 1,
                                 .pCommandBuffers = pCmdBuff};
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submit_info, fence));
}
void vkeSubmitDrawCmd(VkQueue queue, VkCommandBuffer *pCmdBuff, VkSemaphore* pWaitSemaphore, VkSemaphore* pSignalSemaphore){
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                 .commandBufferCount = 1,
                                 .signalSemaphoreCount = 1,
                                 .pSignalSemaphores = pSignalSemaphore,
                                 .waitSemaphoreCount = 1,
                                 .pWaitSemaphores = pWaitSemaphore,
                                 .pWaitDstStageMask = &dstStageMask,
                                 .pCommandBuffers = pCmdBuff};
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submit_info, NULL));
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void buildCommandBuffers(VkEngine* e, VkImage bltSrcImage){

    for (int i=0;i<e->imgCount;i++) {
        VkImage bltDstImage = e->ScBuffers[i].image;

        vkeBeginCmd (e->cmdBuffs[i]);
        // We'll be blitting into the presentable image, set the layout accordingly
        set_image_layout(e->cmdBuffs[i], bltDstImage, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);


        // Do a 32x32 blit to all of the dst image - should get big squares
        VkImageBlit region = { .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
                               .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
                               .srcOffsets[0] = {0,0,0},
                               .dstOffsets[0] = {0,0,0},
                               .srcOffsets[1] = {64,64,1},
                               .dstOffsets[1] = {e->width,e->height,1} };

        vkCmdBlitImage(e->cmdBuffs[i], bltSrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bltDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &region, VK_FILTER_LINEAR);

        // Use a barrier to make sure the blit is finished before the copy starts
        set_image_layout(e->cmdBuffs[i], bltDstImage, VK_IMAGE_ASPECT_COLOR_BIT,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkImageCopy cregion = { .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                .srcOffset = {},
                                .dstOffset = {0,256,0},
                                .extent = {128,128,1}};
        vkCmdCopyImage(e->cmdBuffs[i], bltSrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bltDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &cregion);

        set_image_layout(e->cmdBuffs[i], bltDstImage, VK_IMAGE_ASPECT_COLOR_BIT,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        VK_CHECK_RESULT(vkEndCommandBuffer(e->cmdBuffs[i]));
    }
}

void draw(VkEngine* e, VkImage bltSrcImage) {
    // Get the index of the next available swapchain image:
    VkResult err = vkAcquireNextImageKHR(e->dev, e->swapChain, UINT64_MAX, e->semaPresentEnd, VK_NULL_HANDLE,
                                &e->currentScBufferIndex);
    if ((err == VK_ERROR_OUT_OF_DATE_KHR) || (err == VK_SUBOPTIMAL_KHR)){
        createSwapChain(e, VK_FORMAT_B8G8R8A8_UNORM);
        buildCommandBuffers(e, bltSrcImage);
        return;
    }

    VK_CHECK_RESULT(err);    

    vkeSubmitDrawCmd (e->presentQueue, &e->cmdBuffs[e->currentScBufferIndex], &e->semaPresentEnd, &e->semaDrawEnd);

    /* Now present the image in the window */
    VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                 .swapchainCount = 1,
                                 .pSwapchains = &e->swapChain,
                                 .waitSemaphoreCount = 1,
                                 .pWaitSemaphores = &e->semaDrawEnd,
                                 .pImageIndices = &e->currentScBufferIndex };

    /* Make sure command buffer is finished before presenting */
    //VK_CHECK_RESULT(vkWaitForFences(e.dev, 1, &drawFence, VK_TRUE, FENCE_TIMEOUT));
    VK_CHECK_RESULT(vkQueuePresentKHR(e->presentQueue, &present));
}

VkImage loadImg(VkEngine* e, VkDeviceMemory* pMem){
    VkImage image;
    // Create an image, map it, and write some values to the image
    VkImageCreateInfo image_info = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                     .imageType = VK_IMAGE_TYPE_2D,
                                     .tiling = VK_IMAGE_TILING_LINEAR,
                                     .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                     .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                     .format = e->format,
                                     .extent = {e->width,e->height,1},
                                     .mipLevels = 1,
                                     .arrayLayers = 1,
                                     .samples = NUM_SAMPLES };

    VK_CHECK_RESULT(vkCreateImage(e->dev, &image_info, NULL, &image));

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(e->dev, image, &memReq);
    VkMemoryAllocateInfo memAllocInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                          .allocationSize = memReq.size };
    assert(memory_type_from_properties(e, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                            &memAllocInfo.memoryTypeIndex));
    VK_CHECK_RESULT(vkAllocateMemory(e->dev, &memAllocInfo, NULL, pMem));
    VK_CHECK_RESULT(vkBindImageMemory(e->dev, image, *pMem, 0));

    unsigned char *pImgMem;
    VK_CHECK_RESULT(vkMapMemory(e->dev, *pMem, 0, memReq.size, 0, (void **)&pImgMem));
    // Checkerboard of 8x8 pixel squares
    for (int row = 0; row < e->height; row++) {
        for (int col = 0; col < e->width; col++) {
            unsigned char rgb = (((row & 0x8) == 0) ^ ((col & 0x8) == 0)) * 255;
            pImgMem[0] = rgb;
            pImgMem[1] = rgb;
            pImgMem[2] = rgb;
            pImgMem[3] = 255;
            pImgMem += 4;
        }
    }
    vkUnmapMemory(e->dev, *pMem);

    VkCommandBuffer cmdBuff = vkeCreateCmdBuff(e, 1);

    vkeBeginCmd (cmdBuff);
    // Intend to blit from this image, set the layout accordingly
    set_image_layout(cmdBuff, image, VK_IMAGE_ASPECT_COLOR_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuff));

    VkFence cmdFence = vkeCreateFence(e);
    vkeSubmitCmd (e->presentQueue, &cmdBuff, cmdFence);
    vkWaitForFences(e->dev, 1, &cmdFence, VK_TRUE, FENCE_TIMEOUT);

    vkFreeCommandBuffers (e->dev, e->cmdPool, 1, &cmdBuff);
    vkDestroyFence(e->dev, cmdFence, NULL);

    return image;
}

int main(int argc, char *argv[]) {
    dumpLayerExts();

    VkEngine e = {};

    EngineInit(&e);

    createSwapChain(&e, VK_FORMAT_B8G8R8A8_UNORM);

    vkeCheckPhyPropBlitSource (&e);


    VkDeviceMemory dmem;
    VkImage bltSrcImage = loadImg(&e, &dmem);


    buildCommandBuffers(&e, bltSrcImage);

    glfwSetKeyCallback(e.window, key_callback);

    while (!glfwWindowShouldClose(e.window)) {
        glfwPollEvents();
        draw(&e, bltSrcImage);
        VK_CHECK_RESULT(vkQueueWaitIdle(e.presentQueue));
    }

    vkDestroyImage(e.dev, bltSrcImage, NULL);
    vkFreeMemory(e.dev, dmem, NULL);

    EngineTerminate (&e);

    return 0;
}

//HELPERS
bool vkeCheckPhyPropBlitSource (VkEngine *e) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(e->phy, e->format, &formatProps);
    assert((formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) && "Format cannot be used as transfer source");
}
