#ifndef VKUTILS_H_
#define VKUTILS_H_

#ifndef VULKAN_H_
#include <vulkan/vulkan.h>
#endif /* VULKAN_H_ */

#include <vector>
#include <assert.h>

namespace VkUtils
{
    static VkPhysicalDevice FindBestPhysicalDevice(const VkInstance instance)
    {
        VkResult err = VK_SUCCESS;

        uint32_t count = 0;
        err = vkEnumeratePhysicalDevices(instance, &count, VK_NULL_HANDLE);
        assert(!err);

        std::vector<VkPhysicalDevice> gpuList(count);
        err = vkEnumeratePhysicalDevices(instance, &count, std::data(gpuList));
        assert(!err);

        VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
        int bestScore = -1;

        for (const VkPhysicalDevice& device : gpuList) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);

            int score = 0;

            switch (properties.deviceType) {
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score += 1000; break;
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score += 500; break;
                default:;
            }

            if (score > bestScore) {
                bestScore = score;
                bestDevice = device;
            }
        }

        return bestDevice;
    }

    static uint32_t FindQueueFamilyIndex(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
    {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, VK_NULL_HANDLE);

        std::vector<VkQueueFamilyProperties> queueFamilies(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, std::data(queueFamilies));

        for (uint32_t i = 0; i < std::size(queueFamilies); i++) {
            VkBool32 isSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &isSupport);

            VkQueueFamilyProperties& queueFamily = queueFamilies[i];
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT && isSupport)
                return i;
        }

        return UINT32_MAX;
    }

    static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
    {
        VkSurfaceFormatKHR chosenSurfaceFormat = {};

        if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
            chosenSurfaceFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
            chosenSurfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        } else {
            chosenSurfaceFormat = formats[0];
        }

        return chosenSurfaceFormat;
    }

}

#endif /* VKUTILS_H_ */
