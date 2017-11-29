#ifndef VKH_IMAGE_H
#define VKH_IMAGE_H

#include "vkhelpers.h"
#include "vkh_device.h"

typedef struct vkh_image_t {
	vkh_device* pDev;
	uint32_t width, height;
	VkFormat format;
	VkImageLayout layout;
	VkImage image;
	VkDeviceMemory memory;
	VkDescriptorImageInfo* pDescriptor;
	VkSampleCountFlagBits samples;
}vkh_image;

void vkh_image_create(vkh_device* pDev, VkFormat format, uint32_t width, uint32_t height,
						   VkMemoryPropertyFlags memprops,
						   VkImageUsageFlags usage, VkImageLayout layout, vkh_image *img);
void vkh_image_ms_create (vkh_device *pDev,
						   VkFormat format, VkSampleCountFlagBits num_samples, uint32_t width, uint32_t height,
						   VkMemoryPropertyFlags memprops,
						   VkImageUsageFlags usage, VkImageLayout layout, vkh_image *img);
void vkh_image_create_descriptor(vkh_image* img);
void vkh_image_destroy(vkh_image* img);

#endif
