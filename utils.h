#ifndef VKE_UTILS_H
#define VKE_UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>

#include <vulkan/vulkan.h>
// Used for validating return values of Vulkan API calls.

#define VK_CHECK_RESULT(f) 																				\
{																										\
    VkResult res = (f);																					\
    if (res != VK_SUCCESS)																				\
    {																									\
        printf("Fatal : VkResult is %d in %s at line %d\n", res,  __FILE__, __LINE__); \
        assert(res == VK_SUCCESS);																		\
    }																									\
}

bool memory_type_from_properties(VkPhysicalDeviceMemoryProperties* memory_properties, uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex);
char *read_spv(const char *filename, size_t *psize);
uint32_t* readFile(uint32_t* length, const char* filename);

void dumpLayerExts ();
#endif
