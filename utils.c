#include "utils.h"

bool memory_type_from_properties(VkPhysicalDeviceMemoryProperties* memory_properties, uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex) {
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < memory_properties->memoryTypeCount; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((memory_properties->memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    // No memory types matched, return failure
    return false;
}

char *read_spv(const char *filename, size_t *psize) {
    long int size;
    size_t retval;
    void *shader_code;

#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
    filename =[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent: @(filename)].UTF8String;
#endif

    FILE *fp = fopen(filename, "rb");
    if (!fp)
        return NULL;

    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);

    fseek(fp, 0L, SEEK_SET);

    shader_code = malloc(size);
    retval = fread(shader_code, size, 1, fp);
    assert(retval == 1);

    *psize = size;

    fclose(fp);
    return shader_code;
}

// Read file into array of bytes, and cast to uint32_t*, then return.
// The data has been padded, so that it fits into an array uint32_t.
uint32_t* readFile(uint32_t* length, const char* filename) {

    FILE* fp = fopen(filename, "rb");
    if (fp == 0) {
        printf("Could not find or open file: %s\n", filename);
    }

    // get file size.
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    long filesizepadded = (long)(ceil(filesize / 4.0)) * 4;

    // read file contents.
    char *str = (char*)malloc(filesizepadded*sizeof(char));
    fread(str, filesize, sizeof(char), fp);
    fclose(fp);

    // data padding.
    for (int i = filesize; i < filesizepadded; i++)
        str[i] = 0;

    *length = filesizepadded;
    return (uint32_t *)str;
}

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
