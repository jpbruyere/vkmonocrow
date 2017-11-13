#include "vke.h"

//void computeMandelbrot(VkEngine* e);
void cleanup(VkEngine* e, VkComputePipeline* cp);
void runCommandBuffer(VkEngine* e, VkComputePipeline* cp);
VkComputePipeline createMandleBrot (VkEngine* e);
