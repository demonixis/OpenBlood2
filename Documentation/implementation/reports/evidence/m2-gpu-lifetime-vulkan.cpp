// Standalone diagnostic: system Vulkan loader only; no SDL or game linkage.
#include <vulkan/vulkan.h>
#include <cstdio>
int main()
{
    VkInstance instance{};
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create.pApplicationInfo = &app;
    if (vkCreateInstance(&create, nullptr, &instance) != VK_SUCCESS)
        return 1;
    unsigned count = 8;
    VkPhysicalDevice devices[8]{};
    auto result = vkEnumeratePhysicalDevices(instance, &count, devices);
    if (result == VK_SUCCESS && count)
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(devices[0], &properties);
        std::printf("Vulkan device: %s\n", properties.deviceName);
    }
    vkDestroyInstance(instance, nullptr);
    std::puts("Vulkan instance teardown complete");
    return result == VK_SUCCESS && count ? 0 : 1;
}
