#include "vkvg_surface_internal.h"
#include "vkvg_device_internal.h"

VkvgSurface vkvg_surface_create(VkvgDevice dev, int32_t width, uint32_t height){
    VkvgSurface surf = (vkvg_surface*)calloc(1,sizeof(vkvg_surface));

    surf->dev = dev;
    surf->width = width;
    surf->height = height;

    vkh_image_create(dev,FB_COLOR_FORMAT,width,height,VK_IMAGE_TILING_LINEAR,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT ,
                                     VK_IMAGE_LAYOUT_PREINITIALIZED,&surf->img);
    vkh_image_create(dev,VK_FORMAT_S8_UINT,width,height,VK_IMAGE_TILING_OPTIMAL,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT ,
                                     VK_IMAGE_LAYOUT_PREINITIALIZED,&surf->stencil);

    vkh_image_ms_create(dev,FB_COLOR_FORMAT,VKVG_SAMPLES,width,height,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ,
                                     VK_IMAGE_LAYOUT_PREINITIALIZED,&surf->imgMS);
    vkh_image_ms_create(dev,VK_FORMAT_S8_UINT,VKVG_SAMPLES,width,height,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ,
                                     VK_IMAGE_LAYOUT_PREINITIALIZED,&surf->stencilMS);

    vkh_image_create_descriptor(&surf->img, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);
    vkh_image_create_descriptor(&surf->imgMS, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);
    vkh_image_create_descriptor(&surf->stencil, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_STENCIL_BIT, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);
    vkh_image_create_descriptor(&surf->stencilMS, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_STENCIL_BIT, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);

    VkImageView attachments[] = {
        surf->imgMS.pDescriptor->imageView, surf->img.pDescriptor->imageView,
        surf->stencilMS.pDescriptor->imageView, surf->stencil.pDescriptor->imageView
    };
    VkFramebufferCreateInfo frameBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                      .renderPass = dev->renderPass,
                                                      .attachmentCount = 4,
                                                      .pAttachments = attachments,
                                                      .width = width,
                                                      .height = height,
                                                      .layers = 1 };
    VK_CHECK_RESULT(vkCreateFramebuffer(dev->vkDev, &frameBufferCreateInfo, NULL, &surf->fb));
    return surf;
}

void vkvg_surface_destroy(VkvgSurface surf)
{
    vkDestroyFramebuffer(surf->dev->vkDev, surf->fb, NULL);
    vkh_image_destroy(&surf->img);
    vkh_image_destroy(&surf->imgMS);
    free(surf);
}

VkImage vkvg_surface_get_vk_image(VkvgSurface surf)
{
    return surf->img.image;
}
