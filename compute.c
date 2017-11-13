#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>

#include "compute.h"
//#include "utils.h"

void createBuffer(VkEngine* e, VkComputePipeline* cp) {
    // Create an image, map it, and write some values to the image
    VkImageCreateInfo image_info = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                     .imageType = VK_IMAGE_TYPE_2D,
                                     .tiling = VK_IMAGE_TILING_LINEAR,
                                     .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                     .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                     .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                     .format = VK_FORMAT_R8G8B8A8_UNORM,
                                     .extent = {cp->width,cp->height,1},
                                     .mipLevels = 1,
                                     .arrayLayers = 1,
                                     .samples = VK_SAMPLE_COUNT_1_BIT };
    VK_CHECK_RESULT(vkCreateImage(e->dev, &image_info, NULL, &cp->outImg));

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(e->dev, cp->outImg, &memReq);
    VkMemoryAllocateInfo memAllocInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                          .allocationSize = memReq.size };
    assert(memory_type_from_properties(e, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            &memAllocInfo.memoryTypeIndex));
    VK_CHECK_RESULT(vkAllocateMemory(e->dev, &memAllocInfo, NULL, &cp->bufferMemory));
    VK_CHECK_RESULT(vkBindImageMemory(e->dev, cp->outImg, cp->bufferMemory, 0));

    VkCommandBuffer cmdBuff = vkeCreateCmdBuff(e, e->computer.cmdPool, 1);
    vkeBeginCmd (cmdBuff);
    // Intend to blit from this image, set the layout accordingly
    set_image_layout(cmdBuff, cp->outImg, VK_IMAGE_ASPECT_COLOR_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuff));

    VkFence cmdFence = vkeCreateFence(e->dev);
    vkeSubmitCmd (e->computer.queue, &cmdBuff, cmdFence);
    vkWaitForFences(e->dev, 1, &cmdFence, VK_TRUE, FENCE_TIMEOUT);

    vkFreeCommandBuffers (e->dev, e->computer.cmdPool, 1, &cmdBuff);
    vkDestroyFence(e->dev, cmdFence, NULL);

    VkImageViewCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                         .image = cp->outImg,
                                         .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                         .format = VK_FORMAT_R8G8B8A8_UNORM,
                                         .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
    assert (vkCreateImageView(e->dev, &createInfo, NULL, &cp->view) == VK_SUCCESS);

    VkSamplerCreateInfo sampler = { .magFilter = VK_FILTER_LINEAR,
                                    .minFilter = VK_FILTER_LINEAR,
                                    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                    .mipLodBias = 0.0f,
                                    .maxAnisotropy = 1.0f,
                                    .compareOp = VK_COMPARE_OP_NEVER,
                                    .minLod = 0.0f,
                                    .maxLod = 0.0f,
                                    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE };
    VK_CHECK_RESULT(vkCreateSampler(e->dev, &sampler, NULL, &cp->sampler));
}

void createDescriptorSetLayout(VkEngine* e, VkComputePipeline* cp) {
    //layout(std140, binding = 0) buffer buf
    VkDescriptorSetLayoutBinding dsLayoutBinding = { .binding = 0,
                                                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                    .descriptorCount = 1,
                                                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT };
    VkDescriptorSetLayoutCreateInfo dsLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                          .bindingCount = 1,
                                                          .pBindings = &dsLayoutBinding };
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(e->dev, &dsLayoutCreateInfo, NULL, &cp->descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                            .setLayoutCount = 1,
                                                            .pSetLayouts = &cp->descriptorSetLayout };
    VK_CHECK_RESULT(vkCreatePipelineLayout(e->dev, &pipelineLayoutCreateInfo, NULL, &cp->pipelineLayout));
}

void createDescriptorSet(VkEngine* e, VkComputePipeline* cp) {
    VkDescriptorPoolSize descriptorPoolSize = { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
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
    VkDescriptorImageInfo descImgInfo = { .imageView = cp->view,
                                          .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                                          .sampler = cp->sampler };

    VkWriteDescriptorSet writeDescriptorSet = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                .dstSet = cp->descriptorSet,
                                                .dstBinding = 0,
                                                .descriptorCount = 1,
                                                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                .pImageInfo = &descImgInfo };
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

    /*set_image_layout(cp->commandBuffer, cp->outImg, VK_IMAGE_ASPECT_COLOR_BIT,
                     VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);*/

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
    vkDestroySampler(e->dev, cp->sampler, NULL);
    vkDestroyImageView(e->dev, cp->view, NULL);
    vkDestroyImage(e->dev, cp->outImg, NULL);
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
