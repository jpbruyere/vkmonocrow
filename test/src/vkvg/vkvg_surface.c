#include "vkvg_surface.h"

void vkvg_surface_create(vkvg_device* dev, int32_t width, uint32_t height, vkvg_surface * surf){

    surf->dev = dev;
    surf->width = width;
    surf->height = height;

    vkh_image_create(dev,FB_COLOR_FORMAT,width,height,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT ,
                                     VK_IMAGE_LAYOUT_PREINITIALIZED,&surf->img);

    vkh_image_ms_create(dev,FB_COLOR_FORMAT,VKVG_SAMPLES,width,height,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT ,
                                     VK_IMAGE_LAYOUT_PREINITIALIZED,&surf->imgMS);

    vkh_image_create_descriptor(&surf->img, VK_IMAGE_VIEW_TYPE_2D);
    vkh_image_create_descriptor(&surf->imgMS, VK_IMAGE_VIEW_TYPE_2D);

    VkImageView attachments[] = {surf->imgMS.pDescriptor->imageView, surf->img.pDescriptor->imageView };
    VkFramebufferCreateInfo frameBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                      .renderPass = dev->renderPass,
                                                      .attachmentCount = 2,
                                                      .pAttachments = attachments,
                                                      .width = width,
                                                      .height = height,
                                                      .layers = 1 };
    VK_CHECK_RESULT(vkCreateFramebuffer(dev->vkDev, &frameBufferCreateInfo, NULL, &surf->fb));
}

void vkvg_surface_destroy(vkvg_surface* surf)
{
    vkDestroyFramebuffer(surf->dev->vkDev, surf->fb, NULL);
    vkh_image_destroy(&surf->img);
    vkh_image_destroy(&surf->imgMS);
}
