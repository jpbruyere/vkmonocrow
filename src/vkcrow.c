#include "vkcrow.h"
#include "vkeBuffer.h"
#include "vkhelpers.h"
#include "crow.h"


VkeBuffer* crowBuff = NULL;

void vkcrow_cmd_copy_create(VkCommandBuffer cmd, VkImage bltDstImage, uint32_t width, uint32_t height){
    vkh_cmd_begin(cmd,VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

    set_image_layout(cmd, bltDstImage, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy bufferCopyRegion = { .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
                                           .imageExtent = {width,height,1}};

    vkCmdCopyBufferToImage(cmd, crowBuff->buffer, bltDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

    set_image_layout(cmd, bltDstImage, VK_IMAGE_ASPECT_COLOR_BIT,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    VK_CHECK_RESULT(vkEndCommandBuffer(cmd));
}
void vkcrow_cmd_copy_submit(VkQueue queue, VkCommandBuffer *pCmdBuff, VkSemaphore* pWaitSemaphore, VkSemaphore* pSignalSemaphore){
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                 .commandBufferCount = 1,
                                 .signalSemaphoreCount = 1,
                                 .pSignalSemaphores = pSignalSemaphore,
                                 .waitSemaphoreCount = 1,
                                 .pWaitSemaphores = pWaitSemaphore,
                                 .pWaitDstStageMask = &dstStageMask,
                                 .pCommandBuffers = pCmdBuff};
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submit_info, NULL));
}
void vkcrow_resize(VkDevice dev, VkPhysicalDeviceMemoryProperties mem_properties, uint32_t width, uint32_t height){
    crow_lock_update_mutex();
    crow_evt_enqueue(crow_evt_create_int32(CROW_RESIZE,width,height));
    vkcrow_terminate();
    crowBuff = NULL;
    crowBuff = vke_buffer_create(dev, mem_properties, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, width * height * 4);
    VK_CHECK_RESULT(vke_buffer_map(crowBuff));
    crow_release_update_mutex();
}
void vkcrow_buffer_update (){
    crow_lock_update_mutex();
    if (dirtyLength>0){
        if (dirtyLength+dirtyOffset>crowBuff->size)
            dirtyLength = crowBuff->size - dirtyOffset;
        memcpy(crowBuff->mapped + dirtyOffset, crowBmp + dirtyOffset, dirtyLength);
        dirtyLength = dirtyOffset = 0;
    }
    crow_release_update_mutex();
}
void vkcrow_start(){
    crow_init();
}
void vkcrow_terminate(){
    if (crowBuff){
        vke_buffer_unmap(crowBuff);
        vke_buffer_destroy(crowBuff);
    }
}
void vkcrow_mouse_move(uint32_t x, uint32_t y){
    crow_evt_enqueue(crow_evt_create_int32(CROW_MOUSE_MOVE,x,y));
}
void vkcrow_mouse_down(uint32_t but){
    crow_evt_enqueue(crow_evt_create_int32(CROW_MOUSE_DOWN,but,0));
}
void vkcrow_mouse_up(uint32_t but){
    crow_evt_enqueue(crow_evt_create_int32(CROW_MOUSE_UP,but,0));
}
void vkcrow_key_press(uint32_t c){
    crow_evt_enqueue(crow_evt_create_int32(CROW_KEY_PRESS,c,0));
}
void vkcrow_key_down(uint32_t key){
    crow_evt_enqueue(crow_evt_create_int32(CROW_KEY_DOWN,key,0));
}
void vkcrow_key_up(uint32_t key){
    crow_evt_enqueue(crow_evt_create_int32(CROW_KEY_UP,key,0));
}
void vkcrow_load (const char* path) {
    crow_evt_enqueue(crow_evt_create_pChar(CROW_LOAD,path,0));
}

