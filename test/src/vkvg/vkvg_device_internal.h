#ifndef VKVG_DEVICE_INTERNAL_H
#define VKVG_DEVICE_INTERNAL_H

#include "vkvg.h"

typedef struct {
    vec2    scale;
    vec2    translate;
}push_constants;

typedef struct _vkvg_device_t{
    VkDevice				vkDev;
    VkPhysicalDeviceMemoryProperties phyMemProps;
    VkRenderPass			renderPass;

    VkQueue					queue;
    VkCommandPool			cmdPool;

    VkPipeline				pipeline;
    VkPipeline				pipelineClipping;
    VkPipeline				pipeline_OP_SUB;
    VkPipeline				pipelineWired;
    VkPipeline				pipelineLineList;
    VkPipelineCache			pipelineCache;
    VkPipelineLayout		pipelineLayout;
    VkDescriptorPool		descriptorPool;
    VkDescriptorSet			descriptorSet;
    VkDescriptorSetLayout	descriptorSetLayout;

    int		hdpi,
            vdpi;

    void*	fontCache;

}vkvg_device;
#endif
