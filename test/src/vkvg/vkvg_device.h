#ifndef VKVG_DEVICE_H
#define VKVG_DEVICE_H

typedef struct vkvg_device_t{
	VkDevice vkDev;
	VkPhysicalDeviceMemoryProperties phyMemProps;
	VkRenderPass renderPass;

	VkQueue queue;
	VkCommandPool cmdPool;

	VkPipeline pipeline;
	VkPipelineCache pipelineCache;
	VkPipelineLayout pipelineLayout;
}vkvg_device;

void vkvg_device_create(VkDevice vkdev, VkQueue queue, uint32_t qFam, VkPhysicalDeviceMemoryProperties memprops, vkvg_device* dev);
void vkvg_device_destroy(vkvg_device* dev);

#endif
