#ifndef VKE_H
#define VKE_H

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

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

typedef struct VkeImage_t {
    VkImage image;
    VkImageView view;
    VkDeviceMemory mem;
}VkeImage;

typedef struct VkRenderer_t {
    VkQueue presentQueue;
    VkCommandPool cmdPool;

    GLFWwindow* window;
    VkSurfaceKHR surface;

    VkSemaphore semaPresentEnd;
    VkSemaphore semaDrawEnd;

    VkFormat format;
    uint32_t width;
    uint32_t height;

    uint32_t imgCount;
    uint32_t currentScBufferIndex;

    VkSwapchainKHR swapChain;
    ImageBuffer* ScBuffers;
    VkCommandBuffer* cmdBuffs;
}VkRenderer;

typedef struct VkLoader_t {
    VkQueue queue;
    VkCommandPool cmdPool;
}VkLoader;

typedef struct VkComputer_t {
    VkQueue queue;
    VkCommandPool cmdPool;
}VkComputer;

typedef struct VkEngine_t {
    VkApplicationInfo infos;
    VkInstance inst;
    VkPhysicalDevice phy;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkPhysicalDeviceProperties gpu_props;
    VkDevice dev;

    uint32_t EnabledExtensionsCount;
    const char** ExtensionNames;

    VkRenderer renderer;
    VkComputer computer;
    VkLoader loader;
}VkEngine;

typedef struct VkComputePipeline_t {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkShaderModule computeShaderModule;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;
    VkImage outImg;
    VkImageView view;
    VkSampler sampler;
    VkDeviceMemory bufferMemory;
    uint32_t bufferSize;
    VkCommandBuffer commandBuffer;
    int width;
    int height;
    int work_size;
}VkComputePipeline;

void dumpLayerExts ();
uint32_t* readFile(uint32_t* length, const char* filename);
char *read_spv(const char *filename, size_t *psize);

void vkeFindPhy (VkEngine* e, VkPhysicalDeviceType phyType);

VkFence vkeCreateFence (VkDevice dev);
VkSemaphore vkeCreateSemaphore (VkDevice dev);
VkCommandBuffer vkeCreateCmdBuff (VkEngine* e, VkCommandPool cmdPool, uint32_t buffCount);

bool vkeCheckPhyPropBlitSource (VkEngine *e);

#endif
