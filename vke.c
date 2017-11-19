

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "utils.h"
#include "vke.h"
#include "compute.h"
#include "vkeBuffer.h"

#include "crow.h"

typedef struct VkePBO_t {
    VkImage image;
    VkImageView view;
    VkDeviceMemory mem;
    uint8_t* map;
}VkePBO;


static VkeBuffer crowBuff;

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
            vkGetPhysicalDeviceMemoryProperties(e->phy, &e->memory_properties);
            vkGetPhysicalDeviceProperties(e->phy, &e->gpu_props);
            return ;
        }
    }
    fprintf (stderr, "No suitable GPU found\n");
    exit (-1);
}

VkFence vkeCreateFence (VkDevice dev) {
    VkFence fence;
    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                    .pNext = NULL,
                                    .flags = 0 };
    VK_CHECK_RESULT(vkCreateFence(dev, &fenceInfo, NULL, &fence));
    return fence;
}
VkSemaphore vkeCreateSemaphore (VkDevice dev) {
    VkSemaphore semaphore;
    VkSemaphoreCreateInfo info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                           .pNext = NULL,
                                                           .flags = 0};
    VK_CHECK_RESULT(vkCreateSemaphore(dev, &info, NULL, &semaphore));
    return semaphore;
}
VkCommandPool vkeCreateCmdPool (VkEngine* e, uint32_t qFamIndex){
    VkCommandPool cmdPool;
    VkCommandPoolCreateInfo cmd_pool_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                              .pNext = NULL,
                                              .queueFamilyIndex = qFamIndex,
                                              .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
    assert (vkCreateCommandPool(e->dev, &cmd_pool_info, NULL, &cmdPool) == VK_SUCCESS);
    return cmdPool;
}
VkCommandBuffer vkeCreateCmdBuff (VkEngine* e, VkCommandPool cmdPool, uint32_t buffCount){
    VkCommandBuffer cmdBuff;
    VkCommandBufferAllocateInfo cmd = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                        .pNext = NULL,
                                        .commandPool = cmdPool,
                                        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                        .commandBufferCount = buffCount };
    assert (vkAllocateCommandBuffers (e->dev, &cmd, &cmdBuff) == VK_SUCCESS);
    return cmdBuff;
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

void vkeDestroyImage(VkEngine* e, VkeImage* img) {
    vkDestroyImage(e->dev, img->image, NULL);
    vkFreeMemory(e->dev, img->mem, NULL);
}

void createSwapChain (VkEngine* e, VkFormat preferedFormat){
    // Ensure all operations on the device have been finished before destroying resources
    vkDeviceWaitIdle(e->dev);
    VkRenderer* r = &e->renderer;

    VkSwapchainKHR oldSwapchain = r->swapChain;

    VkSurfaceCapabilitiesKHR surfCapabilities;
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(e->phy, r->surface, &surfCapabilities));
    assert (surfCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    // width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
    if (surfCapabilities.currentExtent.width == 0xFFFFFFFF) {
        // If the surface size is undefined, the size is set to
        // the size of the images requested
        if (r->width < surfCapabilities.minImageExtent.width)
            r->width = surfCapabilities.minImageExtent.width;
        else if (r->width > surfCapabilities.maxImageExtent.width)
            r->width = surfCapabilities.maxImageExtent.width;

        if (r->height < surfCapabilities.minImageExtent.height)
            r->height = surfCapabilities.minImageExtent.height;
        else if (r->height > surfCapabilities.maxImageExtent.height)
            r->height = surfCapabilities.maxImageExtent.height;
    } else {
        // If the surface size is defined, the swap chain size must match
        r->width = surfCapabilities.currentExtent.width;
        r->height= surfCapabilities.currentExtent.height;
    }

    uint32_t formatCount;
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR (e->phy, r->surface, &formatCount, NULL));
    assert (formatCount>0);
    VkSurfaceFormatKHR formats[formatCount];
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR (e->phy, r->surface, &formatCount, formats));

    int formatIdx = -1;
    for (int i=0; i<formatCount; i++){
        if (formats[i].format == preferedFormat) {
            formatIdx=i;
            break;
        }
    }
    assert (formatIdx>=0);

    r->format = preferedFormat;

    uint32_t presentModeCount;
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(e->phy, r->surface, &presentModeCount, NULL));
    assert (presentModeCount>0);
    VkPresentModeKHR presentModes[presentModeCount];
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(e->phy, r->surface, &presentModeCount, presentModes));

    VkSwapchainCreateInfoKHR createInfo = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                            .surface = r->surface,
                                            .minImageCount = surfCapabilities.minImageCount,
                                            .imageFormat = formats[formatIdx].format,
                                            .imageColorSpace = formats[formatIdx].colorSpace,
                                            .imageExtent = {r->width,r->height},
                                            .imageArrayLayers = 1,
                                            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                            .preTransform = surfCapabilities.currentTransform,
                                            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                                            .clipped = VK_TRUE,
                                            .oldSwapchain = oldSwapchain};

    VK_CHECK_RESULT(vkCreateSwapchainKHR (e->dev, &createInfo, NULL, &r->swapChain));

    if (oldSwapchain != VK_NULL_HANDLE)
    {
        for (uint32_t i = 0; i < r->imgCount; i++)
        {
            vkDestroyImageView(e->dev, r->ScBuffers[i].view, NULL);
            vkFreeCommandBuffers (e->dev, e->renderer.cmdPool, 1, &r->cmdBuffs[i]);
        }
        vkDestroySwapchainKHR(e->dev, oldSwapchain, NULL);
        free(r->ScBuffers);
        free(r->cmdBuffs);
    }



    VK_CHECK_RESULT(vkGetSwapchainImagesKHR(e->dev, r->swapChain, &r->imgCount, NULL));
    assert (r->imgCount>0);

    VkImage images[r->imgCount];
    VK_CHECK_RESULT(vkGetSwapchainImagesKHR(e->dev, r->swapChain, &r->imgCount,images));

    r->ScBuffers = (ImageBuffer*)malloc(sizeof(ImageBuffer)*r->imgCount);
    r->cmdBuffs = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*r->imgCount);

    for (int i=0; i<r->imgCount; i++) {
        ImageBuffer sc_buffer;

        VkImageViewCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                             .image = images[i],
                                             .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                             .format = r->format,
                                             .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
        assert (vkCreateImageView(e->dev, &createInfo, NULL, &sc_buffer.view) == VK_SUCCESS);
        sc_buffer.image = images[i];
        r->ScBuffers [i] = sc_buffer;
        r->cmdBuffs [i] = vkeCreateCmdBuff(e, e->renderer.cmdPool, 1);
    }
    r->currentScBufferIndex = 0;
    vkDeviceWaitIdle(e->dev);
}

void EngineInit (VkEngine* e) {
    glfwInit();
    assert (glfwVulkanSupported()==GLFW_TRUE);
    e->ExtensionNames = glfwGetRequiredInstanceExtensions (&e->EnabledExtensionsCount);

    e->infos.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    e->infos.pNext = NULL;
    e->infos.pApplicationName = APP_SHORT_NAME;
    e->infos.applicationVersion = 1;
    e->infos.pEngineName = APP_SHORT_NAME;
    e->infos.engineVersion = 1;
    e->infos.apiVersion = VK_API_VERSION_1_0;
    e->renderer.width = 600;
    e->renderer.height = 480;

    const uint32_t enabledLayersCount = 1;
    const char* enabledLayers[] = {"VK_LAYER_LUNARG_core_validation"};

    VkInstanceCreateInfo inst_info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                       .pNext = NULL,
                                       .flags = 0,
                                       .pApplicationInfo = &e->infos,
                                       .enabledExtensionCount = e->EnabledExtensionsCount,
                                       .ppEnabledExtensionNames = e->ExtensionNames,
                                       .enabledLayerCount = enabledLayersCount,
                                       .ppEnabledLayerNames = enabledLayers };

    VK_CHECK_RESULT(vkCreateInstance (&inst_info, NULL, &e->inst));

    vkeFindPhy (e, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);

    uint32_t queue_family_count = 0;
    int cQueue = -1, gQueue = -1, tQueue = -1;
    vkGetPhysicalDeviceQueueFamilyProperties (e->phy, &queue_family_count, NULL);
    VkQueueFamilyProperties qfams[queue_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties (e->phy, &queue_family_count, &qfams);

    //try to find dedicated queues
    for (int j=0; j<queue_family_count; j++){
        switch (qfams[j].queueFlags) {
        case VK_QUEUE_GRAPHICS_BIT:
            if (gQueue<0)
                gQueue = j;
            break;
        case VK_QUEUE_COMPUTE_BIT:
            if (cQueue<0)
                cQueue = j;
            break;
        case VK_QUEUE_TRANSFER_BIT:
            if (tQueue<0)
                tQueue = j;
            break;
        }
    }
    //try to find suitable queue if no dedicated one found
    for (int j=0; j<queue_family_count; j++){
        if ((qfams[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (gQueue < 0))
            gQueue = j;
        if ((qfams[j].queueFlags & VK_QUEUE_COMPUTE_BIT) && (cQueue < 0))
            cQueue = j;
        if ((qfams[j].queueFlags & VK_QUEUE_TRANSFER_BIT) && (tQueue < 0))
            tQueue = j;
    }
    if (gQueue<0||cQueue<0||tQueue<0){
        fprintf (stderr, "Missing Queue type\n");
        exit (-1);
    }

    uint32_t qCount = 0;
    VkDeviceQueueCreateInfo pQueueInfos[3];
    float queue_priorities[1] = {0.0};

    VkDeviceQueueCreateInfo qiG = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                   .queueCount = 1,
                                   .queueFamilyIndex = gQueue,
                                   .pQueuePriorities = queue_priorities };
    VkDeviceQueueCreateInfo qiC = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                   .queueCount = 1,
                                   .queueFamilyIndex = cQueue,
                                   .pQueuePriorities = queue_priorities };
    VkDeviceQueueCreateInfo qiT = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                   .queueCount = 1,
                                   .queueFamilyIndex = tQueue,
                                   .pQueuePriorities = queue_priorities };

    if (gQueue == cQueue){
        if(gQueue == tQueue){
            qCount=1;
            pQueueInfos[0] = qiG;
        }else{
            qCount=2;
            pQueueInfos[0] = qiG;
            pQueueInfos[1] = qiT;
        }
    }else{
        if((gQueue == tQueue) || (cQueue==tQueue)){
            qCount=2;
            pQueueInfos[0] = qiG;
            pQueueInfos[1] = qiC;
        }else{
            qCount=3;
            pQueueInfos[0] = qiG;
            pQueueInfos[1] = qiC;
            pQueueInfos[2] = qiT;
        }
    }

    VkDeviceCreateInfo device_info = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                       .queueCreateInfoCount = qCount,
                                       .pQueueCreateInfos = &pQueueInfos};

    VK_CHECK_RESULT(vkCreateDevice(e->phy, &device_info, NULL, &e->dev));

    assert (glfwGetPhysicalDevicePresentationSupport (e->inst, e->phy, gQueue)==GLFW_TRUE);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    VkRenderer* r = &e->renderer;

    r->window = glfwCreateWindow(r->width, r->height, "Window Title", NULL, NULL);

    assert (glfwCreateWindowSurface(e->inst, r->window, NULL, &r->surface)==VK_SUCCESS);

    VkBool32 isSupported;
    vkGetPhysicalDeviceSurfaceSupportKHR(e->phy, gQueue, r->surface, &isSupported);
    assert (isSupported && "vkGetPhysicalDeviceSurfaceSupportKHR");

    vkGetDeviceQueue(e->dev, gQueue, 0, &e->renderer.presentQueue);
    vkGetDeviceQueue(e->dev, cQueue, 0, &e->computer.queue);
    vkGetDeviceQueue(e->dev, tQueue, 0, &e->loader.queue);

    e->renderer.cmdPool = vkeCreateCmdPool (e, gQueue);
    e->computer.cmdPool = vkeCreateCmdPool (e, cQueue);
    e->loader.cmdPool = vkeCreateCmdPool (e, tQueue);

    r->semaPresentEnd = vkeCreateSemaphore(e->dev);
    r->semaDrawEnd = vkeCreateSemaphore(e->dev);
}
void EngineTerminate (VkEngine* e) {
    vkDeviceWaitIdle(e->dev);
    destroyCrowStaggingBuf(&crowBuff);
    VkRenderer* r = &e->renderer;

    vkDestroySemaphore(e->dev, r->semaDrawEnd, NULL);
    vkDestroySemaphore(e->dev, r->semaPresentEnd, NULL);

    for (int i=0; i<r->imgCount; i++){
        vkDestroyImageView (e->dev, r->ScBuffers[i].view, NULL);
        vkFreeCommandBuffers (e->dev, r->cmdPool, 1, &r->cmdBuffs[i]);
    }
    free (r->ScBuffers);
    free (r->cmdBuffs);

    vkDestroyCommandPool (e->dev, e->renderer.cmdPool, NULL);
    vkDestroyCommandPool (e->dev, e->computer.cmdPool, NULL);
    vkDestroyCommandPool (e->dev, e->loader.cmdPool, NULL);

    vkDestroySwapchainKHR(e->dev, r->swapChain, NULL);
    vkDestroyDevice (e->dev, NULL);
    vkDestroySurfaceKHR (e->inst, r->surface, NULL);
    glfwDestroyWindow (r->window);
    glfwTerminate ();

    vkDestroyInstance (e->inst, NULL);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS){
        //printf("scancode: %d",key);
        //fflush(stdout);
        crow_evt_enqueue(crow_evt_create_int32(CROW_KEY_DOWN,key,scancode));
    }else if (action == GLFW_RELEASE)
        crow_evt_enqueue(crow_evt_create_int32(CROW_KEY_UP,key,scancode));

    if (action != GLFW_PRESS)
        return;
    switch (key) {
    case GLFW_KEY_ESCAPE :
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        break;
    case GLFW_KEY_F3 :
        crow_load("ifaces/0.crow");
        break;
    case GLFW_KEY_F4 :
        crow_load("ifaces/1.crow");
        break;
    case GLFW_KEY_F5 :
        crow_load("ifaces/2.crow");
        break;
    }
}

static void char_callback (GLFWwindow* window, uint32_t c){
    crow_evt_enqueue(crow_evt_create_int32(CROW_KEY_PRESS,c,0));
}

static void mouse_move_callback(GLFWwindow* window, double x, double y){
    crow_evt_enqueue(crow_evt_create_int32(CROW_MOUSE_MOVE,(int)x,(int)y));
}
static void mouse_button_callback(GLFWwindow* window, int but, int state, int modif){
    if (state == GLFW_PRESS)
        crow_evt_enqueue(crow_evt_create_int32(CROW_MOUSE_DOWN,but,0));
    else
        crow_evt_enqueue(crow_evt_create_int32(CROW_MOUSE_UP,but,0));
}
static void win_resize_callback(GLFWwindow* window, int width, int height){

}

void buildCommandBuffers(VkRenderer* r){
    for (int i=0;i<r->imgCount;i++) {
        VkImage bltDstImage = r->ScBuffers[i].image;

        VkCommandBufferBeginInfo cmd_buf_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                  .pNext = NULL,
                                                  .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
                                                  .pInheritanceInfo = NULL };
        VK_CHECK_RESULT(vkBeginCommandBuffer(r->cmdBuffs[i], &cmd_buf_info));

        //vkeBeginCmd (r->cmdBuffs[i]);
        set_image_layout(r->cmdBuffs[i], bltDstImage, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkBufferImageCopy bufferCopyRegion = { .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
                                               .imageExtent = {r->width,r->height,1}};

        vkCmdCopyBufferToImage(r->cmdBuffs[i], crowBuff.buffer, bltDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

        set_image_layout(r->cmdBuffs[i], bltDstImage, VK_IMAGE_ASPECT_COLOR_BIT,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        VK_CHECK_RESULT(vkEndCommandBuffer(r->cmdBuffs[i]));
    }
}

VkeBuffer createCrowStaggingBuff (VkEngine* e){
    VkeBuffer buff = vke_buffer_create(e->dev, &e->memory_properties, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, e->renderer.width * e->renderer.height * 4);
    VK_CHECK_RESULT(vke_buffer_map(&buff));
    return buff;
}
void destroyCrowStaggingBuf(VkeBuffer* buff){
    vke_buffer_unmap(buff);
    vke_buffer_destroy(buff);
}


void checkCrow (){
    crow_lock_update_mutex();
    if (dirtyLength>0){
        if (dirtyLength+dirtyOffset>crowBuff.size)
            dirtyLength = crowBuff.size - dirtyOffset;
        memcpy(crowBuff.mapped + dirtyOffset, crowBuffer + dirtyOffset, dirtyLength);
        dirtyLength = dirtyOffset = 0;
    }
    crow_release_update_mutex();
}

void draw(VkEngine* e) {
    VkRenderer* r = &e->renderer;
    // Get the index of the next available swapchain image:
    VkResult err = vkAcquireNextImageKHR(e->dev, r->swapChain, UINT64_MAX, r->semaPresentEnd, VK_NULL_HANDLE,
                                &r->currentScBufferIndex);
    if ((err == VK_ERROR_OUT_OF_DATE_KHR) || (err == VK_SUBOPTIMAL_KHR)){
        createSwapChain(e, VK_FORMAT_B8G8R8A8_UNORM);
        crow_evt_enqueue(crow_evt_create_int32(CROW_RESIZE,e->renderer.width,e->renderer.height));
        destroyCrowStaggingBuf(&crowBuff);
        crowBuff = createCrowStaggingBuff(e);
        buildCommandBuffers(r);
        return;
    }

    checkCrow();

    VK_CHECK_RESULT(err);
    vkeSubmitDrawCmd (r->presentQueue, &r->cmdBuffs[r->currentScBufferIndex], &r->semaPresentEnd, &r->semaDrawEnd);

    /* Now present the image in the window */
    VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                 .swapchainCount = 1,
                                 .pSwapchains = &r->swapChain,
                                 .waitSemaphoreCount = 1,
                                 .pWaitSemaphores = &r->semaDrawEnd,
                                 .pImageIndices = &r->currentScBufferIndex };

    /* Make sure command buffer is finished before presenting */
    VK_CHECK_RESULT(vkQueuePresentKHR(r->presentQueue, &present));
}

int main(int argc, char *argv[]) {
    dumpLayerExts();

    VkEngine e = {};

    EngineInit(&e);

    createSwapChain(&e, VK_FORMAT_B8G8R8A8_UNORM);

    crow_init();

    vkeCheckPhyPropBlitSource (&e);

    crowBuff = createCrowStaggingBuff(&e);
    buildCommandBuffers(&e.renderer);

    glfwSetKeyCallback(e.renderer.window, key_callback);
    glfwSetCharCallback(e.renderer.window, char_callback);
    glfwSetCursorPosCallback(e.renderer.window, mouse_move_callback);
    glfwSetMouseButtonCallback(e.renderer.window, mouse_button_callback);
    //glfwSetWindowSizeCallback(e.renderer.window, win_resize_callback);

    crow_evt_enqueue(crow_evt_create_int32(CROW_RESIZE,e.renderer.width,e.renderer.height));

    crow_load("/mnt/devel/gts/libvk/crow/Tests/Interfaces/Divers/0.crow");

    while (!glfwWindowShouldClose(e.renderer.window)) {
        glfwPollEvents();
        draw(&e);
    }

    EngineTerminate (&e);

    return 0;
}

//HELPERS
bool vkeCheckPhyPropBlitSource (VkEngine *e) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(e->phy, e->renderer.format, &formatProps);
    assert((formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) && "Format cannot be used as transfer source");
}
