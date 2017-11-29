#include "vkvg.h"
#include "vkvg_internal.h"

void _setupRenderPass(vkvg_device* dev);
void _setupPipelines(vkvg_device* dev);

void vkvg_device_create(VkDevice vkdev, VkQueue queue, uint32_t qFam, VkPhysicalDeviceMemoryProperties memprops, vkvg_device* dev)
{
    dev->vkDev = vkdev;
    dev->phyMemProps = memprops;

    dev->queue = queue;
    dev->cmdPool = vkh_cmd_pool_create (dev->vkDev, qFam, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    _setupRenderPass (dev);
    _setupPipelines (dev);
}

void vkvg_device_destroy(vkvg_device* dev)
{
    vkDestroyPipeline (dev->vkDev, dev->pipeline, NULL);
    vkDestroyPipelineLayout(dev->vkDev, dev->pipelineLayout, NULL);
    vkDestroyRenderPass (dev->vkDev, dev->renderPass, NULL);
    vkDestroyCommandPool (dev->vkDev, dev->cmdPool, NULL);
}

void _setupRenderPass(vkvg_device* dev)
{
    VkAttachmentDescription attachment = {
                    .format = FB_COLOR_FORMAT,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference colorReference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpassDescription = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                        .colorAttachmentCount = 1,									// Subpass uses one color attachment
                        .pColorAttachments = &colorReference };						// Reference to the color attachment in slot 0
    VkSubpassDependency dep0 = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT };
    VkSubpassDependency dep1 = {
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT };

    VkSubpassDependency dependencies[] = {dep0,dep1};
    VkRenderPassCreateInfo renderPassInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &attachment,
                .subpassCount = 1,
                .pSubpasses = &subpassDescription,
                .dependencyCount = 2,
                .pDependencies = dependencies };

    VK_CHECK_RESULT(vkCreateRenderPass(dev->vkDev, &renderPassInfo, NULL, &dev->renderPass));
}

void _setupPipelines(vkvg_device* dev)
{
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .renderPass = dev->renderPass };

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

    VkPipelineColorBlendAttachmentState blendAttachmentState =
    { .colorWriteMask = 0xf, .blendEnable = VK_FALSE };

    VkPipelineColorBlendStateCreateInfo colorBlendState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &blendAttachmentState };

    VkDynamicState dynamicStateEnables[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = 2,
                .pDynamicStates = dynamicStateEnables };

    VkPipelineViewportStateCreateInfo viewportState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1, .scissorCount = 1 };

    VkPipelineMultisampleStateCreateInfo multisampleState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                              .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
    VkVertexInputBindingDescription vertexInputBinding = { .binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };

    VkVertexInputAttributeDescription vertexInputAttributs[3];
    vertexInputAttributs[0].binding = 0;//pos
    vertexInputAttributs[0].location = 0;
    vertexInputAttributs[0].format = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributs[0].offset = 0;
    vertexInputAttributs[1].binding = 0;//color
    vertexInputAttributs[1].location = 1;
    vertexInputAttributs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertexInputAttributs[1].offset = sizeof(vec2);
    vertexInputAttributs[2].binding = 0;//uv
    vertexInputAttributs[2].location = 2;
    vertexInputAttributs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributs[2].offset = sizeof(vec2) + sizeof(vec4);

    VkPipelineVertexInputStateCreateInfo vertexInputState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                              .vertexBindingDescriptionCount = 1,
                                                              .pVertexBindingDescriptions = &vertexInputBinding,
                                                              .vertexAttributeDescriptionCount = 3,
                                                              .pVertexAttributeDescriptions = vertexInputAttributs };
    VkPipelineShaderStageCreateInfo vertStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vkh_load_module(dev->vkDev, "shaders/triangle.vert.spv"),
        .pName = "main",
    };
    VkPipelineShaderStageCreateInfo fragStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = vkh_load_module(dev->vkDev, "shaders/triangle.frag.spv"),
        .pName = "main",
    };
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage,fragStage};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                      .setLayoutCount = 0,
                                                      .pushConstantRangeCount = 0 };
    VK_CHECK_RESULT(vkCreatePipelineLayout(dev->vkDev, &pipelineLayoutInfo, NULL, &dev->pipelineLayout));
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.layout = dev->pipelineLayout;

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vkDev, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &dev->pipeline));

    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vkDev, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &dev->pipelineWired));

    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vkDev, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &dev->pipelineLineList));

    vkDestroyShaderModule(dev->vkDev, shaderStages[0].module, NULL);
    vkDestroyShaderModule(dev->vkDev, shaderStages[1].module, NULL);
}
