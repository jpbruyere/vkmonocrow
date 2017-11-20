#ifndef VKCROW_H
#define VKCROW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <vulkan/vulkan.h>


void vkcrow_cmd_copy_create(VkCommandBuffer cmd, VkImage bltDstImage, uint32_t width, uint32_t height);
void vkcrow_cmd_copy_submit(VkQueue queue, VkCommandBuffer *pCmdBuff, VkSemaphore* pWaitSemaphore, VkSemaphore* pSignalSemaphore);
void vkcrow_resize(VkDevice dev, VkPhysicalDeviceMemoryProperties mem_properties, uint32_t width, uint32_t height);
void vkcrow_start();
void vkcrow_terminate();
void vkcrow_buffer_update ();
void vkcrow_mouse_move(uint32_t x, uint32_t y);
void vkcrow_mouse_down(uint32_t but);
void vkcrow_mouse_up(uint32_t but);
void vkcrow_key_press(uint32_t c);
void vkcrow_key_down(uint32_t key);
void vkcrow_key_up(uint32_t key);
void vkcrow_load (const char *path);

#ifdef __cplusplus
}
#endif

#endif
