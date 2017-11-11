#ifndef VKE_H
#define VKE_H

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

 #define STB_IMAGE_IMPLEMENTATION
 #include "stb_image.h"

#define APP_SHORT_NAME "vulkansamples_instance"
/* Number of samples needs to be the same at image creation,      */
/* renderpass creation and pipeline creation.                     */
#define NUM_SAMPLES VK_SAMPLE_COUNT_1_BIT
#define FENCE_TIMEOUT 100000000

// Used for validating return values of Vulkan API calls.
#define VK_CHECK_RESULT(f) 																				\
{																										\
    VkResult res = (f);																					\
    if (res != VK_SUCCESS)																				\
    {																									\
        printf("Fatal : VkResult is %d in %s at line %d\n", res,  __FILE__, __LINE__); \
        assert(res == VK_SUCCESS);																		\
    }																									\
}

typedef struct ImageBuffer_t {
    VkImage image;
    VkImageView view;
}ImageBuffer;

typedef struct VkEngine_t {
    VkApplicationInfo infos;
    VkInstance inst;
    VkPhysicalDevice phy;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkPhysicalDeviceProperties gpu_props;
    VkDevice dev;
    VkQueue presentQueue;
    VkCommandPool cmdPool;
    VkCommandBuffer cmdBuff;
    uint32_t qFam;
    uint32_t EnabledExtensionsCount;
    const char** ExtensionNames;

    GLFWwindow* window;
    VkSurfaceKHR surface;

    VkSwapchainKHR swapChain;

    VkFormat format;
    uint32_t width;
    uint32_t height;

    uint32_t imgCount;
    uint32_t currentScBufferIndex;
    ImageBuffer* ScBuffers;
}VkEngine;


void vkeFindPhy (VkEngine* e, VkPhysicalDeviceType phyType);
void vkeFindQueueIdx (VkEngine* e, VkQueueFlags qType);

VkFence vkeCreateFence (VkEngine* e);
VkSemaphore vkeCreateSemaphore (VkEngine* e);


bool vkeCheckPhyPropBlitSource (VkEngine *e);

#endif
