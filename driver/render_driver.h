#ifndef RENDER_DRIVER_H_
#define RENDER_DRIVER_H_

#define ENABLE_VOLK_LOADER

#ifdef ENABLE_VOLK_LOADER
#include <volk/volk.h>
#endif /* ENABLE_VOLK_LOADER */

#include <ashlands/typedefs.h>

// std
#include <assert.h>
#include <vector>

typedef struct Pipeline_T *Pipeline;

class RenderDriver
{
public:
    RenderDriver();
   ~RenderDriver();

    VkResult Initialize(VkSurfaceKHR surface);

    void RebuildSwapchain();

    VkResult CreatePipeline(const char *shaderName, Pipeline* pPipeline);

    void DestroyPipeline(Pipeline pipeline);

    VkInstance GetInstance() const { return instance; }
    VkQueue GetGraphicsQueue() const { return queue; }
    VkQueue GetPresentQueue() const { return queue; }

private:
    VkResult _CreateInstance();
    VkResult _CreateDevice();
    VkResult _CreateSwapchain(VkSwapchainKHR oldSwapchain);
    VkResult _CreateCommandPool();
    VkResult _CreateShaderModule(const char* shaderName, const char* stage, VkShaderModule* pShaderModule);

    void _DestroySwapchain();

    // Vulkan handles
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;

    // Vulkan swapchain resources
    uint32_t imageCount = 0;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    uint32_t queueFamilyIndex = UINT32_MAX;
    VkSurfaceFormatKHR surfaceFormat = {};
    VkPhysicalDeviceProperties physicalDeviceProperties = {};
};

#endif /* RENDER_DRIVER_H_ */
