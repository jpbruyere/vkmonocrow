#ifndef VKE_BUFFER_H
#define VKE_BUFFER_H

#include <vulkan/vulkan.h>

typedef struct VkeBuffer_t {
    VkDevice device;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDescriptorBufferInfo descriptor;
    VkDeviceSize size;
    VkDeviceSize alignment;

    VkBufferUsageFlags usageFlags;
    VkMemoryPropertyFlags memoryPropertyFlags;

    void* mapped;
}VkeBuffer;

VkeBuffer* vke_buffer_create(VkDevice dev, VkPhysicalDeviceMemoryProperties memory_properties, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size);
void vke_buffer_destroy(VkeBuffer* buff);
VkResult vke_buffer_map(VkeBuffer* buff);
void vke_buffer_unmap(VkeBuffer* buff);
VkResult vke_buffer_bind(VkeBuffer* buff);

#endif
