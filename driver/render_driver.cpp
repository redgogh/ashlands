#include "render_driver.h"

#include <stdio.h>
#include "vkutils.h"

#define VK_VERSION_1_3_216

#define VK_CHECK_ERROR(err) \
    if (err != VK_SUCCESS) \
        return err;

/* volk 全局只初始化一次 */
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

    err = _CreateInstance();
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
    vkDestroyCommandPool(device, commandPool, VK_NULL_HANDLE);
    // vkDestroySwapchainKHR(device, swapchain, VK_NULL_HANDLE);
    _DestroySwapchain();
    vkDestroyDevice(device, VK_NULL_HANDLE);
    vkDestroyInstance(instance, VK_NULL_HANDLE);
}

VkResult RenderDriver::Initialize(VkSurfaceKHR surface)
{
    VkResult err = VK_SUCCESS;

    this->surface = surface;

    err = _CreateDevice();
    VK_CHECK_ERROR(err);

    err = _CreateSwapchain(VK_NULL_HANDLE);
    VK_CHECK_ERROR(err);

    err = _CreateCommandPool();
    VK_CHECK_ERROR(err);

    return err;
}

void RenderDriver::RebuildSwapchain()
{
    _CreateSwapchain(swapchain);
}


VkResult RenderDriver::_CreateInstance()
{
    VkResult err = VK_SUCCESS;

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
    VK_CHECK_ERROR(err);

    return err;
}

VkResult RenderDriver::_CreateDevice()
{
    VkResult err = VK_SUCCESS;

    physicalDevice = VkUtils::PickBestPhysicalDevice(instance);
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
    printf("[vulkan] found best physical device: %s\n", physicalDeviceProperties.deviceName);

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

    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

TAG_DEVICE_Create_END:
    return err;
}

VkResult RenderDriver::_CreateSwapchain(VkSwapchainKHR oldSwapchain)
{
    VkResult err = VK_SUCCESS;

    VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
    VK_CHECK_ERROR(err);

    imageCount = surfaceCapabilities.minImageCount + 1;
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
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR tmpSwapchain = VK_NULL_HANDLE;
    err = vkCreateSwapchainKHR(device, &swapchainCreateInfo, VK_NULL_HANDLE, &tmpSwapchain);
    VK_CHECK_ERROR(err);

    if (oldSwapchain != VK_NULL_HANDLE)
        _DestroySwapchain();

    swapchain = tmpSwapchain;

    /* Create swapchain resources */
    err = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    VK_CHECK_ERROR(err);

    swapchainImages.resize(imageCount);
    swapchainImageViews.resize(imageCount);

    err = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, std::data(swapchainImages));
    VK_CHECK_ERROR(err);

    for (uint32_t i = 0; i < imageCount; i++) {
        VkImage swapchainImage = swapchainImages[i];

        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = swapchainImage;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = surfaceFormat.format;
        imageViewCreateInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        err = vkCreateImageView(device, &imageViewCreateInfo, VK_NULL_HANDLE, &swapchainImageViews[i]);
        VK_CHECK_ERROR(err);
    }

    return err;
}

VkResult RenderDriver::_CreateCommandPool()
{
    VkResult err = VK_SUCCESS;

    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
                                  | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    err = vkCreateCommandPool(device, &commandPoolCreateInfo, VK_NULL_HANDLE, &commandPool);
    VK_CHECK_ERROR(err);

    return err;
}

void RenderDriver::_DestroySwapchain()
{
    for (uint32_t i = 0; i < imageCount; i++)
        vkDestroyImageView(device, swapchainImageViews[i], VK_NULL_HANDLE);
    swapchainImages.clear();
    swapchainImageViews.clear();
    vkDestroySwapchainKHR(device, swapchain, VK_NULL_HANDLE);
}
