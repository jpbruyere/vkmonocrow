#include "vkh_image.h"

void vkh_image_create (vkh_device *pDev,
                           VkFormat format, uint32_t width, uint32_t height,
                           VkMemoryPropertyFlags memprops,
                           VkImageUsageFlags usage, VkImageLayout layout, vkh_image* img)
{
    img->pDev = pDev;
    img->width = width;
    img->height = height;
    img->format = format;
    img->layout = layout;
    VkImageCreateInfo image_info = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                     .imageType = VK_IMAGE_TYPE_2D,
                                     .tiling = VK_IMAGE_TILING_LINEAR,
                                     .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
                                     .usage = usage,
                                     .format = format,
                                     .extent = {width,height,1},
                                     .mipLevels = 1,
                                     .arrayLayers = 1,
                                     .samples = VK_SAMPLE_COUNT_1_BIT };

    VK_CHECK_RESULT(vkCreateImage(pDev->vkDev, &image_info, NULL, &img->image));

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(pDev->vkDev, img->image, &memReq);
    VkMemoryAllocateInfo memAllocInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                          .allocationSize = memReq.size };
    assert(memory_type_from_properties(&pDev->phyMemProps, memReq.memoryTypeBits, memprops,&memAllocInfo.memoryTypeIndex));
    VK_CHECK_RESULT(vkAllocateMemory(pDev->vkDev, &memAllocInfo, NULL, &img->memory));
    VK_CHECK_RESULT(vkBindImageMemory(pDev->vkDev, img->image, img->memory, 0));
}
void vkh_image_create_descriptor(vkh_image* img)
{
    img->pDescriptor = (VkDescriptorImageInfo*)malloc(sizeof(VkDescriptorImageInfo));
    img->pDescriptor->imageLayout = img->layout;
    VkImageViewCreateInfo viewInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                         .image = img->image,
                                         .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                         .format = img->format,
                                         .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
    VK_CHECK_RESULT(vkCreateImageView(img->pDev->vkDev, &viewInfo, NULL, &img->pDescriptor->imageView));

    VkSamplerCreateInfo samplerCreateInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                              .magFilter = VK_FILTER_LINEAR,
                                              .minFilter = VK_FILTER_LINEAR,
                                              .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR};
    VK_CHECK_RESULT(vkCreateSampler(img->pDev->vkDev, &samplerCreateInfo, NULL, &img->pDescriptor->sampler));

}
void vkh_image_destroy(vkh_image* img)
{
    if (img->pDescriptor != NULL){
        vkDestroyImageView(img->pDev->vkDev,img->pDescriptor->imageView,NULL);
        if(img->pDescriptor->sampler != VK_NULL_HANDLE)
            vkDestroySampler(img->pDev->vkDev,img->pDescriptor->sampler,NULL);
    }
    free(img->pDescriptor);
    vkDestroyImage(img->pDev->vkDev,img->image,NULL);
    vkFreeMemory(img->pDev->vkDev, img->memory, NULL);
}
