#include "vkeBuffer.h"
#include "utils.h"

VkeBuffer vke_buffer_create(VkDevice dev, VkPhysicalDeviceMemoryProperties* memory_properties, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size){
    VkeBuffer buff = { .device = dev };
    VkBufferCreateInfo bufCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = usage, .size = size, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VK_CHECK_RESULT(vkCreateBuffer(dev, &bufCreateInfo, NULL, &buff.buffer));

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(dev, buff.buffer, &memReq);
    VkMemoryAllocateInfo memAllocInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                          .allocationSize = memReq.size };
    assert(memory_type_from_properties(&memory_properties, memReq.memoryTypeBits,memoryPropertyFlags, &memAllocInfo.memoryTypeIndex));
    VK_CHECK_RESULT(vkAllocateMemory(dev, &memAllocInfo, NULL, &buff.memory));

    buff.alignment = memReq.alignment;
    buff.size = memAllocInfo.allocationSize;
    buff.usageFlags = usage;
    buff.memoryPropertyFlags = memoryPropertyFlags;

    VK_CHECK_RESULT(vke_buffer_bind(&buff));
    return buff;
}

void vke_buffer_destroy(VkeBuffer* buff){
    if (buff->buffer)
        vkDestroyBuffer(buff->device, buff->buffer, NULL);
    if (buff->memory)
        vkFreeMemory(buff->device, buff->memory, NULL);
}


VkResult vke_buffer_map(VkeBuffer* buff){
    return vkMapMemory(buff->device, buff->memory, 0, VK_WHOLE_SIZE, 0, &buff->mapped);
}
void vke_buffer_unmap(VkeBuffer* buff){
    if (!buff->mapped)
        return;
    vkUnmapMemory(buff->device, buff->memory);
    buff->mapped = NULL;
}

VkResult vke_buffer_bind(VkeBuffer* buff)
{
    return vkBindBufferMemory(buff->device, buff->buffer, buff->memory, 0);
}
