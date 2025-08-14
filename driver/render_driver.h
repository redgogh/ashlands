#ifndef RENDER_DRIVER_H_
#define RENDER_DRIVER_H_

#define ENABLE_VOLK_LOADER

#ifdef ENABLE_VOLK_LOADER
#include <volk/volk.h>
#endif /* ENABLE_VOLK_LOADER */

#include <assert.h>

class RenderDriver
{
public:
    RenderDriver();
   ~RenderDriver();

    VkResult Initialize(VkSurfaceKHR surface);

    VkInstance GetInstance() const { return instance; }
    VkQueue GetGraphicsQueue() const { return queue; }
    VkQueue GetPresentQueue() const { return queue; }

private:
    VkResult _InitializeDevice();
    VkResult _InitializeSwapchain();

private:
    VkPhysicalDeviceProperties physicalDeviceProperties = {};

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

};

#endif /* RENDER_DRIVER_H_ */
