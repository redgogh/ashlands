#include "render_driver.h"

#include <stdio.h>
#include "vkutils.h"

#define VK_VERSION_1_3_216

#define VK_CHECK_ERROR(err) \
    if (err != VK_SUCCESS) \
        return err;

static bool volkInitialized = false;

RenderDriver::RenderDriver()
{
    VkResult err = VK_SUCCESS;

#ifdef ENABLE_VOLK_LOADER
    if (!volkInitialized) {
        err = volkInitialize();
        assert(!err);
        volkInitialized = true;
    }
#endif /* ENABLE_VOLK_LOADER */

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "AshLands";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.pEngineName = "AshLands";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_3;

    const std::vector<const char*> layers = {
#ifndef __APPLE__
        "VK_LAYER_KHRONOS_validation"
#endif
    };

    const std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
    #if defined(_WIN32)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    #elif defined(__APPLE__)
        "VK_MVK_macos_surface",
        "VK_EXT_metal_surface",
    #elif defined(__linux__)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
    #endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,

#if VK_HEADER_VERSION >= 216
        "VK_KHR_portability_enumeration",
        "VK_KHR_get_physical_device_properties2",
#endif
    };

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
#if VK_HEADER_VERSION >= 216
    instanceCreateInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(std::size(layers));
    instanceCreateInfo.ppEnabledLayerNames = std::data(layers);
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(extensions));
    instanceCreateInfo.ppEnabledExtensionNames = std::data(extensions);

    err = vkCreateInstance(&instanceCreateInfo, VK_NULL_HANDLE, &instance);
    assert(!err);

#ifdef ENABLE_VOLK_LOADER
    volkLoadInstance(instance);
#endif /* ENABLE_VOLK_LOADER */

    uint32_t version = 0;
    err = vkEnumerateInstanceVersion(&version);

    if (!err) {
        printf("[vulkan] instance version supported: %d.%d.%d\n",
            VK_VERSION_MAJOR(version),
            VK_VERSION_MINOR(version),
            VK_VERSION_PATCH(version));
    } else {
        printf("[vulkan] instance version is <= 1.0");
    }
}

RenderDriver::~RenderDriver()
{
    vkDestroySwapchainKHR(device, swapchain, VK_NULL_HANDLE);
    vkDestroyDevice(device, VK_NULL_HANDLE);
    vkDestroyInstance(instance, VK_NULL_HANDLE);
}

VkResult RenderDriver::Initialize(VkSurfaceKHR surface)
{
    VkResult err = VK_SUCCESS;

    this->surface = surface;

    err = _InitializeDevice();
    VK_CHECK_ERROR(err);

    err = _InitializeSwapchain();
    VK_CHECK_ERROR(err);

    return err;
}

VkResult RenderDriver::_InitializeDevice()
{
    VkResult err = VK_SUCCESS;

    physicalDevice = VkUtils::FindBestPhysicalDevice(instance);
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
    printf("[vulkan] found best physical device: %s\n", physicalDeviceProperties.deviceName);

    uint32_t queueFamilyIndex = 0;
    queueFamilyIndex = VkUtils::FindQueueFamilyIndex(physicalDevice, surface);
    assert(queueFamilyIndex != UINT32_MAX);

    float priorities = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &priorities;

    const std::vector<const char*> extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME
    };

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(extensions));
    deviceCreateInfo.ppEnabledExtensionNames = std::data(extensions);

    err = vkCreateDevice(physicalDevice, &deviceCreateInfo, VK_NULL_HANDLE, &device);
    VK_CHECK_ERROR(err);

TAG_DEVICE_INITIALIZE_END:
    return err;
}

VkResult RenderDriver::_InitializeSwapchain()
{
    VkResult err = VK_SUCCESS;

    VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
    VK_CHECK_ERROR(err);

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (imageCount > surfaceCapabilities.maxImageCount)
        imageCount = surfaceCapabilities.maxImageCount;

    uint32_t formatCount = 0;
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, VK_NULL_HANDLE);
    VK_CHECK_ERROR(err);

    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, std::data(surfaceFormats));
    VK_CHECK_ERROR(err);

    VkSurfaceFormatKHR surfaceFormat = VkUtils::ChooseSwapSurfaceFormat(surfaceFormats);

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + 1;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    err = vkCreateSwapchainKHR(device, &swapchainCreateInfo, VK_NULL_HANDLE, &swapchain);
    VK_CHECK_ERROR(err);

    return err;
}
