#ifndef RENDER_DRIVER_H_
#define RENDER_DRIVER_H_

#define ENABLE_VOLK_LOADER

#ifdef ENABLE_VOLK_LOADER
#include <volk/volk.h>
#endif /* ENABLE_VOLK_LOADER */

#include <vma/vk_mem_alloc.h>
#include <ashlands/typedefs.h>

// std
#include <assert.h>
#include <vector>

typedef struct Texture2D_T *Texture2D;
typedef struct Buffer_T *Buffer;
typedef struct Pipeline_T *Pipeline;

class RenderDriver
{
public:
    RenderDriver();
   ~RenderDriver();

    VkResult Initialize(VkSurfaceKHR surface);

    VkResult CreateBuffer(size_t size, VkBufferUsageFlags usage, Buffer *pBuffer);
    void DestroyBuffer(Buffer buffer);
    VkResult CreateTexture2D(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, Texture2D *pTexture2D);
    void DestroyTexture2D(Texture2D Texture2D);
    VkResult CreatePipeline(const char *shaderName, Pipeline* pPipeline);
    void DestroyPipeline(Pipeline pipeline);

    void RebuildSwapchain();
    void ReadBuffer(Buffer buffer, void* data, size_t size);
    void WriteBuffer(Buffer buffer, void* data, size_t size);

    VkInstance GetInstance() const { return instance; }
    VkQueue GetGraphicsQueue() const { return queue; }
    VkQueue GetPresentQueue() const { return queue; }

private:
    VkResult _CreateInstance();
    VkResult _CreateDevice();
    VkResult _CreateMemoryAllocator();
    VkResult _CreateSwapchain(VkSwapchainKHR oldSwapchain);
    VkResult _CreateCommandPool();
    VkResult _CreateShaderModule(const char* shaderName, const char* stage, VkShaderModule* pShaderModule);

    void _DestroySwapchain();

    static VmaMemoryUsage _GuessMemoryUsage(VkBufferUsageFlags usage);

    // Vulkan handles
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
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
