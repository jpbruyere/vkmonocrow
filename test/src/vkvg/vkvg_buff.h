#ifndef VKVG_BUFF_H
#define VKVG_BUFF_H

#include <vulkan/vulkan.h>
#include "vkh_device.h"

typedef struct vkvg_buff_t {
    VkhDevice       pDev;
    VkBuffer        buffer;
    VkDeviceMemory  memory;
    VkDescriptorBufferInfo descriptor;
    VkDeviceSize    size;
    VkDeviceSize    alignment;

    VkBufferUsageFlags usageFlags;
    VkMemoryPropertyFlags memoryPropertyFlags;

    void* mapped;
}vkvg_buff;

void vkvg_buffer_create         (VkhDevice pDev, VkBufferUsageFlags usage,
                                    VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, vkvg_buff* buff);
void vkvg_buffer_destroy        (vkvg_buff* buff);
void vkvg_buffer_increase_size  (vkvg_buff *buff, uint32_t sizeAdded);
#endif
