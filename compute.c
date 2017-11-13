#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>

#include "compute.h"
//#include "utils.h"

void createBuffer(VkEngine* e, VkComputePipeline* cp) {
    VkBufferCreateInfo bufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                            .size = cp->bufferSize,
                                            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    VK_CHECK_RESULT(vkCreateBuffer(e->dev, &bufferCreateInfo, NULL, &cp->buffer));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(e->dev, cp->buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                          .allocationSize = memoryRequirements.size };

    assert(memory_type_from_properties(e, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                            &allocateInfo.memoryTypeIndex));

    VK_CHECK_RESULT(vkAllocateMemory(e->dev, &allocateInfo, NULL, &cp->bufferMemory));
    VK_CHECK_RESULT(vkBindBufferMemory(e->dev, cp->buffer, cp->bufferMemory, 0));
}

void createDescriptorSetLayout(VkEngine* e, VkComputePipeline* cp) {
    //layout(std140, binding = 0) buffer buf
    VkDescriptorSetLayoutBinding dsLayoutBinding = { .binding = 0,
                                                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                    .descriptorCount = 1,
                                                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT };
    VkDescriptorSetLayoutCreateInfo dsLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                          .bindingCount = 1,
                                                          .pBindings = &dsLayoutBinding };
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(e->dev, &dsLayoutCreateInfo, NULL, &cp->descriptorSetLayout));
}

void createDescriptorSet(VkEngine* e, VkComputePipeline* cp) {
    VkDescriptorPoolSize descriptorPoolSize = { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                .descriptorCount = 1 };
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                            .maxSets = 1,
                                                            .poolSizeCount = 1,
                                                            .pPoolSizes = &descriptorPoolSize };
    VK_CHECK_RESULT(vkCreateDescriptorPool(e->dev, &descriptorPoolCreateInfo, NULL, &cp->descriptorPool));

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                              .descriptorPool = cp->descriptorPool,
                                                              .descriptorSetCount = 1,
                                                              .pSetLayouts = &cp->descriptorSetLayout };
    VK_CHECK_RESULT(vkAllocateDescriptorSets(e->dev, &descriptorSetAllocateInfo, &cp->descriptorSet));

    // Specify the buffer to bind to the descriptor.
    VkDescriptorBufferInfo descriptorBufferInfo = { .buffer = cp->buffer,
                                                    .offset = 0,
                                                    .range = cp->bufferSize };
    VkWriteDescriptorSet writeDescriptorSet = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                .dstSet = cp->descriptorSet,
                                                .dstBinding = 0,
                                                .descriptorCount = 1,
                                                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                .pBufferInfo = &descriptorBufferInfo };
    vkUpdateDescriptorSets(e->dev, 1, &writeDescriptorSet, 0, NULL);
}


void createComputePipeline(VkEngine* e, VkComputePipeline* cp) {
    size_t filelength;
    char* pCode = read_spv("shaders/shader.comp.spv", &filelength);
    VkShaderModuleCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                            .pCode = pCode,
                                            .codeSize = filelength};

    VK_CHECK_RESULT(vkCreateShaderModule(e->dev, &createInfo, NULL, &cp->computeShaderModule));
    free (pCode);

    VkPipelineShaderStageCreateInfo shaderStageCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                              .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                                                              .module = cp->computeShaderModule,
                                                              .pName = "main" };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                            .setLayoutCount = 1,
                                                            .pSetLayouts = &cp->descriptorSetLayout };
    VK_CHECK_RESULT(vkCreatePipelineLayout(e->dev, &pipelineLayoutCreateInfo, NULL, &cp->pipelineLayout));

    VkComputePipelineCreateInfo pipelineCreateInfo = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                       .stage = shaderStageCreateInfo,
                                                       .layout = cp->pipelineLayout };
    VK_CHECK_RESULT(vkCreateComputePipelines(e->dev, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &cp->pipeline));
}

void createCommandBuffer(VkEngine* e, VkComputePipeline* cp) {
    cp->commandBuffer = vkeCreateCmdBuff(e,e->computer.cmdPool,1);

    VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                           .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK_RESULT(vkBeginCommandBuffer(cp->commandBuffer, &beginInfo)); // start recording commands.

    vkCmdBindPipeline(cp->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cp->pipeline);
    vkCmdBindDescriptorSets(cp->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cp->pipelineLayout, 0, 1, &cp->descriptorSet, 0, NULL);
    vkCmdDispatch(cp->commandBuffer, (uint32_t)ceil(cp->width / (float)cp->work_size),
                  (uint32_t)ceil(cp->height/ (float)cp->work_size), 1);

    VK_CHECK_RESULT(vkEndCommandBuffer(cp->commandBuffer)); // end recording commands.
}

void runCommandBuffer(VkEngine* e, VkComputePipeline* cp) {
    VkSubmitInfo submitInfo = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                .commandBufferCount = 1,
                                .pCommandBuffers = &cp->commandBuffer};
    VkFence fence = vkeCreateFence(e->dev);
    VK_CHECK_RESULT(vkQueueSubmit(e->computer.queue, 1, &submitInfo, fence));
    VK_CHECK_RESULT(vkWaitForFences(e->dev, 1, &fence, VK_TRUE, 100000000000));
    vkDestroyFence(e->dev, fence, NULL);
}

void cleanup(VkEngine* e, VkComputePipeline* cp) {
    vkFreeMemory(e->dev, cp->bufferMemory, NULL);
    vkDestroyBuffer(e->dev, cp->buffer, NULL);
    vkDestroyShaderModule(e->dev, cp->computeShaderModule, NULL);
    vkDestroyDescriptorPool(e->dev, cp->descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(e->dev, cp->descriptorSetLayout, NULL);
    vkDestroyPipelineLayout(e->dev, cp->pipelineLayout, NULL);
    vkDestroyPipeline(e->dev, cp->pipeline, NULL);
}

VkComputePipeline createMandleBrot (VkEngine* e) {
    VkComputePipeline cp = { .width = 3200,
                             .height = 2400,
                             .work_size = 32 };

    cp.bufferSize = 4 * sizeof(float) * cp.width * cp.height;

    createBuffer(e, &cp);
    createDescriptorSetLayout(e, &cp);
    createDescriptorSet(e, &cp);
    createComputePipeline(e, &cp);
    createCommandBuffer(e, &cp);
    return cp;
}
/*void computeMandelbrot(VkEngine* e) {
    runCommandBuffer(e, &cp);
    cleanup(e, &cp);
}*/
