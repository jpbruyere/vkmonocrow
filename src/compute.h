#ifndef VK_COMPUTE_H
#define VK_COMPUTE_H

#include "vke.h"

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

//void computeMandelbrot(VkEngine* e);
void cleanup(VkEngine* e, VkComputePipeline* cp);
void runCommandBuffer(VkEngine* e, VkComputePipeline* cp);
VkComputePipeline createMandleBrot (VkEngine* e);

#endif
