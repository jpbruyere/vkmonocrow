#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "vke.h"
#include "compute.h"
#include "vkhelpers.h"
#include "vkh_buffer.h"
#include "vkh_image.h"
#include "vkcrow.h"



typedef struct{
    float position[3];
    float color[3];
}Vertex;

vkh_image img = {};
vkh_buffer vertices = {};
vkh_buffer indices = {};
uint32_t indicesCount = 0;
vkh_device device = {};

VkPipeline pipeline;
VkPipelineCache pipelineCache;
VkPipelineLayout pipelineLayout;

void vke_swapchain_destroy (vkh_presenter* r);
void vke_swapchain_create (VkEngine* e);

bool vkeCheckPhyPropBlitSource (VkEngine *e) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(e->phy, e->renderer.format, &formatProps);
    assert((formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) && "Format cannot be used as transfer source");
}

void initPhySurface(VkEngine* e, VkFormat preferedFormat, VkPresentModeKHR presentMode){
    vkh_presenter* r = &e->renderer;

    uint32_t count;
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR (e->phy, r->surface, &count, NULL));
    assert (count>0);
    VkSurfaceFormatKHR formats[count];
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR (e->phy, r->surface, &count, formats));

    for (int i=0; i<count; i++){
        if (formats[i].format == preferedFormat) {
            r->format = formats[i].format;
            r->colorSpace = formats[i].colorSpace;
            break;
        }
    }
    assert (r->format != VK_FORMAT_UNDEFINED);

    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(e->phy, r->surface, &count, NULL));
    assert (count>0);
    VkPresentModeKHR presentModes[count];
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(e->phy, r->surface, &count, presentModes));
    r->presentMode = -1;
    for (int i=0; i<count; i++){
        if (presentModes[i] == presentMode) {
            r->presentMode = presentModes[i];
            break;
        }
    }
    assert (r->presentMode >= 0);
}

void vke_swapchain_create (VkEngine* e){
    // Ensure all operations on the device have been finished before destroying resources
    vkDeviceWaitIdle(e->dev);
    vkh_presenter* r = &e->renderer;

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

    VkSwapchainKHR newSwapchain;
    VkSwapchainCreateInfoKHR createInfo = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                            .surface = r->surface,
                                            .minImageCount = surfCapabilities.minImageCount,
                                            .imageFormat = r->format,
                                            .imageColorSpace = r->colorSpace,
                                            .imageExtent = {r->width,r->height},
                                            .imageArrayLayers = 1,
                                            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                            .preTransform = surfCapabilities.currentTransform,
                                            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                            .presentMode = r->presentMode,
                                            .clipped = VK_TRUE,
                                            .oldSwapchain = r->swapChain};

    VK_CHECK_RESULT(vkCreateSwapchainKHR (e->dev, &createInfo, NULL, &newSwapchain));
    if (r->swapChain != VK_NULL_HANDLE)
        vke_swapchain_destroy(r);
    r->swapChain = newSwapchain;

    VK_CHECK_RESULT(vkGetSwapchainImagesKHR(e->dev, r->swapChain, &r->imgCount, NULL));
    assert (r->imgCount>0);

    VkImage images[r->imgCount];
    VK_CHECK_RESULT(vkGetSwapchainImagesKHR(e->dev, r->swapChain, &r->imgCount,images));

    r->ScBuffers = (ImageBuffer*)malloc(sizeof(ImageBuffer)*r->imgCount);
    r->cmdBuffs = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*r->imgCount);

    for (int i=0; i<r->imgCount; i++) {
        ImageBuffer sc_buffer = {};
        VkImageViewCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                             .image = images[i],
                                             .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                             .format = r->format,
                                             .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
        VK_CHECK_RESULT(vkCreateImageView(e->dev, &createInfo, NULL, &sc_buffer.view));
        sc_buffer.image = images[i];
        r->ScBuffers [i] = sc_buffer;
        r->cmdBuffs [i] = vkh_cmd_buff_create(e->dev, e->renderer.cmdPool);
    }
    r->currentScBufferIndex = 0;
}
void vke_swapchain_destroy (vkh_presenter* r){
    for (uint32_t i = 0; i < r->imgCount; i++)
    {
        vkDestroyImageView(r->dev, r->ScBuffers[i].view, NULL);
        vkFreeCommandBuffers (r->dev, r->cmdPool, 1, &r->cmdBuffs[i]);
    }
    vkDestroySwapchainKHR(r->dev, r->swapChain, NULL);
    free(r->ScBuffers);
    free(r->cmdBuffs);
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
    e->renderer.width = 1024;
    e->renderer.height = 800;

    const uint32_t enabledLayersCount = 1;

    const char* enabledLayers[] = {"VK_LAYER_LUNARG_core_validation"};
    //const char* enabledLayers[] = {"VK_LAYER_LUNARG_standard_validation"};

    VkInstanceCreateInfo inst_info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                       .pNext = NULL,
                                       .flags = 0,
                                       .pApplicationInfo = &e->infos,
                                       .enabledExtensionCount = e->EnabledExtensionsCount,
                                       .ppEnabledExtensionNames = e->ExtensionNames,
                                       .enabledLayerCount = enabledLayersCount,
                                       .ppEnabledLayerNames = enabledLayers };

    VK_CHECK_RESULT(vkCreateInstance (&inst_info, NULL, &e->inst));

    e->phy = vkh_find_phy (e->inst, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);

    vkGetPhysicalDeviceMemoryProperties(e->phy, &e->memory_properties);
    vkGetPhysicalDeviceProperties(e->phy, &e->gpu_props);

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
    float queue_priorities[] = {0.0};

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

    vkh_presenter* r = &e->renderer;
    r->dev = e->dev;

    r->window = glfwCreateWindow(r->width, r->height, "Window Title", NULL, NULL);

    assert (glfwCreateWindowSurface(e->inst, r->window, NULL, &r->surface)==VK_SUCCESS);

    VkBool32 isSupported;
    vkGetPhysicalDeviceSurfaceSupportKHR(e->phy, gQueue, r->surface, &isSupported);
    assert (isSupported && "vkGetPhysicalDeviceSurfaceSupportKHR");

    vkGetDeviceQueue(e->dev, gQueue, 0, &e->renderer.queue);
    vkGetDeviceQueue(e->dev, cQueue, 0, &e->computer.queue);
    vkGetDeviceQueue(e->dev, tQueue, 0, &e->loader.queue);

    e->renderer.cmdPool = vkh_cmd_pool_create (e->dev, gQueue, 0);
    e->computer.cmdPool = vkh_cmd_pool_create (e->dev, cQueue, 0);
    e->loader.cmdPool = vkh_cmd_pool_create (e->dev, tQueue, 0);

    r->semaPresentEnd = vkh_semaphore_create(e->dev);
    r->semaDrawEnd = vkh_semaphore_create(e->dev);

    initPhySurface(e,VK_FORMAT_B8G8R8A8_UNORM,VK_PRESENT_MODE_FIFO_KHR);

    device.phyMemProps = e->memory_properties;
    device.renderPass = e->renderer.renderPass;
    device.vkDev = e->dev;
}
void EngineTerminate (VkEngine* e) {
    vkDeviceWaitIdle(e->dev);
    vkh_presenter* r = &e->renderer;

    vkDestroySemaphore(e->dev, r->semaDrawEnd, NULL);
    vkDestroySemaphore(e->dev, r->semaPresentEnd, NULL);

    vkDestroyCommandPool (e->dev, e->renderer.cmdPool, NULL);
    vkDestroyCommandPool (e->dev, e->computer.cmdPool, NULL);
    vkDestroyCommandPool (e->dev, e->loader.cmdPool, NULL);

    vkDestroyDevice (e->dev, NULL);
    vkDestroySurfaceKHR (e->inst, r->surface, NULL);
    glfwDestroyWindow (r->window);
    glfwTerminate ();

    vkDestroyInstance (e->inst, NULL);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS)
        return;
    switch (key) {
    case GLFW_KEY_ESCAPE :
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        break;
    }
}
static void char_callback (GLFWwindow* window, uint32_t c){}
static void mouse_move_callback(GLFWwindow* window, double x, double y){}
static void mouse_button_callback(GLFWwindow* window, int but, int state, int modif){}

void vke_frame_buffers_create(vkh_presenter * r){
    r->frameBuffs = (VkFramebuffer*)malloc(sizeof(VkFramebuffer)*r->imgCount);
    for (int i=0; i<r->imgCount; i++) {
        VkImageView attachments[1] = {r->ScBuffers[i].view};
        VkFramebufferCreateInfo frameBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                          .renderPass = r->renderPass,
                                                          .attachmentCount = 1,
                                                          .pAttachments = attachments,
                                                          .width = r->width,
                                                          .height = r->height,
                                                          .layers = 1 };
        VK_CHECK_RESULT(vkCreateFramebuffer(r->dev, &frameBufferCreateInfo, NULL, &r->frameBuffs[i]));
    }
}
void vke_frame_buffers_destroy(vkh_presenter* r){
    for (uint32_t i = 0; i < r->imgCount; i++)
        vkDestroyFramebuffer(r->dev, r->frameBuffs[i],NULL);
    free(r->frameBuffs);
}
void vke_pipeline_create (VkEngine* e){
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                        .renderPass = e->renderer.renderPass };
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                                  .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
    VkPipelineRasterizationStateCreateInfo rasterizationState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                                  .polygonMode = VK_POLYGON_MODE_FILL,
                                                                  .cullMode = VK_CULL_MODE_NONE,
                                                                  .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                                                                  .depthClampEnable = VK_FALSE,
                                                                  .rasterizerDiscardEnable = VK_FALSE,
                                                                  .depthBiasEnable = VK_FALSE,
                                                                  .lineWidth = 1.0f };
    // We need one blend attachment state per color attachment (even if blending is not used
    VkPipelineColorBlendAttachmentState blendAttachmentState = { .colorWriteMask = 0xf,
                                                                 .blendEnable = VK_FALSE};
    VkPipelineColorBlendStateCreateInfo colorBlendState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                            .attachmentCount = 1,
                                                            .pAttachments = &blendAttachmentState };
    VkRect2D scissor = { .offset = {0, 0}, .extent = {e->renderer.width,e->renderer.height}};
    VkViewport viewport = { .width = (float) e->renderer.width,
                            .height = (float) e->renderer.height};

    VkPipelineViewportStateCreateInfo viewportState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                        .viewportCount = 1,
                                                        .pViewports = &viewport,
                                                        .scissorCount = 1,
                                                        .pScissors = &scissor};
    VkPipelineMultisampleStateCreateInfo multisampleState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                              .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
    VkVertexInputBindingDescription vertexInputBinding = { .binding = 0,
                                                           .stride = sizeof(Vertex),
                                                           .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };

    VkVertexInputAttributeDescription vertexInputAttributs[2];
    vertexInputAttributs[0].binding = 0;// Position attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
    vertexInputAttributs[0].location = 0;
    vertexInputAttributs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributs[0].offset = 0;
    vertexInputAttributs[1].binding = 0;// Color attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
    vertexInputAttributs[1].location = 1;
    vertexInputAttributs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributs[1].offset = 3 * sizeof(float);

    VkPipelineVertexInputStateCreateInfo vertexInputState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                              .vertexBindingDescriptionCount = 1,
                                                              .pVertexBindingDescriptions = &vertexInputBinding,
                                                              .vertexAttributeDescriptionCount = 2,
                                                              .pVertexAttributeDescriptions = vertexInputAttributs };
    VkPipelineShaderStageCreateInfo vertStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vkh_load_module(e->dev, "shaders/triangle.vert.spv"),
        .pName = "main",
    };
    VkPipelineShaderStageCreateInfo fragStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = vkh_load_module(e->dev, "shaders/triangle.frag.spv"),
        .pName = "main",
    };
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage,fragStage};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                      .setLayoutCount = 0,
                                                      .pushConstantRangeCount = 0 };
    VK_CHECK_RESULT(vkCreatePipelineLayout(e->dev, &pipelineLayoutInfo, NULL, &pipelineLayout));

    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.layout = pipelineLayout;

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(e->dev, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &pipeline));

    vkDestroyShaderModule(e->dev, shaderStages[0].module, NULL);
    vkDestroyShaderModule(e->dev, shaderStages[1].module, NULL);
}
void vke_pipeline_destroy (VkEngine* e){
    vkDestroyPipeline(e->dev, pipeline, NULL);
}
void buildCommandBuffers(vkh_presenter* r){
    for (int32_t i = 0; i < r->imgCount; ++i)
    {
        VkImage bltDstImage = r->ScBuffers[i].image;
        VkImage bltSrcImage = img.image;

        VkCommandBuffer cb = r->cmdBuffs[i];
        vkh_cmd_begin(cb,VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

        set_image_layout(cb, bltDstImage, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        set_image_layout(cb, bltSrcImage, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkImageCopy cregion = { .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                .srcOffset = {},
                                .dstOffset = {0,0,0},
                                .extent = {512,512,1}};
        vkCmdCopyImage(cb, bltSrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bltDstImage, VK_IMAGE_LAYOUT_UNDEFINED,
                       1, &cregion);

        set_image_layout(cb, bltDstImage, VK_IMAGE_ASPECT_COLOR_BIT,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        vkh_cmd_end(cb);
    }
}

void draw(VkEngine* e) {
    vkh_presenter* r = &e->renderer;
    // Get the index of the next available swapchain image:
    VkResult err = vkAcquireNextImageKHR(e->dev, r->swapChain, UINT64_MAX, r->semaPresentEnd, VK_NULL_HANDLE,
                                &r->currentScBufferIndex);
    if ((err == VK_ERROR_OUT_OF_DATE_KHR) || (err == VK_SUBOPTIMAL_KHR)){
        vke_swapchain_create(e);
        buildCommandBuffers(r);
    }else{
        VK_CHECK_RESULT(err);
        vkcrow_cmd_copy_submit (r->queue, &r->cmdBuffs[r->currentScBufferIndex], &r->semaPresentEnd, &r->semaDrawEnd);

        /* Now present the image in the window */
        VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                     .swapchainCount = 1,
                                     .pSwapchains = &r->swapChain,
                                     .waitSemaphoreCount = 1,
                                     .pWaitSemaphores = &r->semaDrawEnd,
                                     .pImageIndices = &r->currentScBufferIndex };

        /* Make sure command buffer is finished before presenting */
        VK_CHECK_RESULT(vkQueuePresentKHR(r->queue, &present));
    }
}

void prepareVertices()
{
    Vertex vertexBuffer[] = {
        { {  100.5f,  100.0f, 0.0f }, { 0.0f, 1.0f, 1.0f } },
        { {  101.5f,  600.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
        { {  100.5f, 600.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
        { {  101.5f, 100.0f, 0.0f }, { 0.0f, 1.0f, 1.0f } },
        { {  110.0f,  100.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
        { {  115.0f,  600.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
        { {  114.0f, 600.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
        { {  111.0f, 100.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
    };
    uint32_t vertexBufferSize = sizeof(Vertex) * 8;

    // Setup indices
    uint32_t indexBuffer[] = {  0, 1, 2, 0, 3, 1,
                                4, 5, 6, 4, 7, 5};
    indicesCount = 12;
    uint32_t indexBufferSize = indicesCount * sizeof(uint32_t);

    vkh_buffer_create(&device,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertexBufferSize, &vertices);
    vkh_buffer_map(&vertices);
    memcpy(vertices.mapped, vertexBuffer, vertexBufferSize);
    vkh_buffer_unmap(&vertices);

    vkh_buffer_create(&device,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBufferSize,&indices);
    vkh_buffer_map(&indices);
    memcpy(indices.mapped, indexBuffer, indexBufferSize);
    vkh_buffer_unmap(&indices);
}
void setupRenderPass(vkh_presenter* r)
{
    // This example will use a single render pass with one subpass

    // Descriptors for the attachments used by this renderpass
    VkAttachmentDescription attachments[1];

    // Color attachment
    attachments[0].format = r->format;          									// Use the color format selected by the swapchain
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;									// We don't use multi sampling in this example
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;							// Clear this attachment at the start of the render pass
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;							// Keep it's contents after the render pass is finished (for displaying it)
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;					// We don't use stencil, so don't care for load
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;				// Same for store
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;						// Layout at render pass start. Initial doesn't matter, so we use undefined
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;					// Layout to which the attachment is transitioned when the render pass is finished
                                                                                    // As we want to present the color buffer to the swapchain, we transition to PRESENT_KHR
    // Setup attachment references
    VkAttachmentReference colorReference = {
        .attachment = 0,                                                            // Attachment 0 is color
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };       				// Attachment layout used as color during the subpass

    // Setup a single subpass reference
    VkSubpassDescription subpassDescription = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                        .colorAttachmentCount = 1,									// Subpass uses one color attachment
                        .pColorAttachments = &colorReference };						// Reference to the color attachment in slot 0
    // Setup subpass dependencies
    // These will add the implicit ttachment layout transitionss specified by the attachment descriptions
    // The actual usage layout is preserved through the layout specified in the attachment reference
    // Each subpass dependency will introduce a memory and execution dependency between the source and dest subpass described by
    // srcStageMask, dstStageMask, srcAccessMask, dstAccessMask (and dependencyFlags is set)
    // Note: VK_SUBPASS_EXTERNAL is a special constant that refers to all commands executed outside of the actual renderpass)
    VkSubpassDependency dependencies[2];

    // First dependency at the start of the renderpass
    // Does the transition from final to initial layout
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;								// Producer of the dependency
    dependencies[0].dstSubpass = 0;													// Consumer is our single subpass that will wait for the execution depdendency
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    // Second dependency at the end the renderpass
    // Does the transition from the initial to the final layout
    dependencies[1].srcSubpass = 0;													// Producer of the dependency is our single subpass
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;								// Consumer are all commands outside of the renderpass
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = 1,                               // Number of attachments used by this render pass
                .pAttachments = attachments,                        // Descriptions of the attachments used by the render pass
                .subpassCount = 1,									// We only use one subpass in this example
                .pSubpasses = &subpassDescription,					// Description of that subpass
                .dependencyCount = 2,                               // Number of subpass dependencies
                .pDependencies = dependencies };    				// Subpass dependencies used by the render pass

    VK_CHECK_RESULT(vkCreateRenderPass(r->dev, &renderPassInfo, NULL, &r->renderPass));
}

void setupOffscreen(VkEngine* e){
    vkh_presenter* r = &e->renderer;
    vkh_image_create(&device,FB_COLOR_FORMAT,r->width,r->height,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                     VK_IMAGE_LAYOUT_PREINITIALIZED,&img);
    vkh_image_create_descriptor(&img);

    prepareVertices();
    setupRenderPass(&e->renderer);

    vke_pipeline_create(e);

    VkFramebuffer fb;
    VkImageView attachments[1] = {img.pDescriptor->imageView};
    VkFramebufferCreateInfo frameBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                      .renderPass = r->renderPass,
                                                      .attachmentCount = 1,
                                                      .pAttachments = attachments,
                                                      .width = r->width,
                                                      .height = r->height,
                                                      .layers = 1 };
    VK_CHECK_RESULT(vkCreateFramebuffer(r->dev, &frameBufferCreateInfo, NULL, &fb));



    VkCommandBuffer cb = vkh_cmd_buff_create(e->dev, r->cmdPool);

    VkClearValue clearValues[1] = {{ { 0.0f, 0.0f, 0.1f, 1.0f } }};

    VkRenderPassBeginInfo renderPassBeginInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                  .renderPass = r->renderPass,
                                                  .renderArea.extent = {r->width,r->height},
                                                  .clearValueCount = 1,
                                                  .pClearValues = clearValues };

    renderPassBeginInfo.framebuffer = fb;

    vkh_cmd_begin(cb,VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
    vkCmdBeginRenderPass(cb, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(cb, 0, 1, &vertices.buffer, offsets);
    vkCmdBindIndexBuffer(cb, indices.buffer, 0, VK_INDEX_TYPE_UINT32);

    // Draw indexed triangle
    vkCmdDrawIndexed(cb, indicesCount, 1, 0, 0, 1);

    vkCmdEndRenderPass(cb);

    vkh_cmd_end(cb);

    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                 .commandBufferCount = 1,
                                 .signalSemaphoreCount = 0,
                                 .pSignalSemaphores = NULL,
                                 .waitSemaphoreCount = 0,
                                 .pWaitSemaphores = NULL,
                                 .pWaitDstStageMask = &dstStageMask,
                                 .pCommandBuffers = &cb};
    VK_CHECK_RESULT(vkQueueSubmit(r->queue, 1, &submit_info, NULL));
    vkDeviceWaitIdle(e->dev);
}

int main(int argc, char *argv[]) {
    dumpLayerExts();

    VkEngine e = {};


    EngineInit(&e);


    vkeCheckPhyPropBlitSource (&e);
    glfwSetKeyCallback(e.renderer.window, key_callback);

    vke_swapchain_create(&e);

    setupOffscreen(&e);

    buildCommandBuffers(&e.renderer);


    while (!glfwWindowShouldClose(e.renderer.window)) {
        glfwPollEvents();
        draw(&e);
    }

    vkDeviceWaitIdle(e.dev);
    vkh_buffer_destroy(&vertices);
    vkh_buffer_destroy(&indices);

    vke_pipeline_destroy(&e);
    vke_frame_buffers_destroy(&e.renderer);
    vke_swapchain_destroy(&e.renderer);

    EngineTerminate (&e);

    return 0;
}
