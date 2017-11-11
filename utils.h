#ifndef VKE_UTILS_H
#define VKE_UTILS_H

#include <vulkan/vulkan.h>

void dumpLayerExts () {
    printf ("Layers:\n");
    uint32_t instance_layer_count;
    assert (vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL)==VK_SUCCESS);
    if (instance_layer_count == 0)
        return;
    VkLayerProperties vk_props[instance_layer_count];
    assert (vkEnumerateInstanceLayerProperties(&instance_layer_count, vk_props)==VK_SUCCESS);

    for (uint32_t i = 0; i < instance_layer_count; i++) {
        printf ("\t%s, %s\n", vk_props[i].layerName, vk_props[i].description);
/*        res = init_global_extension_properties(layer_props);
        if (res) return res;
        info.instance_layer_properties.push_back(layer_props);*/
    }
}
#endif
