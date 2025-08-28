#ifndef RENDER_DRIVER_H_
#define RENDER_DRIVER_H_

#define ENABLE_VOLK_LOADER

#ifdef ENABLE_VOLK_LOADER
#include <volk/volk.h>
#endif /* ENABLE_VOLK_LOADER */

#include <vma/vk_mem_alloc.h>
#include <capybara/typedefs.h>

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
    VkResult CreateCommandBuffer(VkCommandBuffer* pCommandBuffer);
    void DestroyCommandBuffer(VkCommandBuffer commandBuffer);
    void DestroyCommandBuffers(uint32_t count, VkCommandBuffer* pCommandBuffers);

    void BeginCommandBuffer(VkCommandBuffer commandBuffer);
    void EndCommandBuffer(VkCommandBuffer commandBuffer);
    void CmdTextureMemoryBarrier(VkCommandBuffer commandBuffer, Texture2D texture, VkImageLayout newLayout);
    void CmdBeginRendering(VkCommandBuffer commandBuffer);
    void CmdEndRendering(VkCommandBuffer commandBuffer);
    void CmdBindPipeline(VkCommandBuffer commandBuffer, Pipeline pipeline);
    void CmdBindVertexBuffer(VkCommandBuffer commandBuffer, Buffer buffer, VkDeviceSize offset);
    void CmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t count, Buffer *pBuffers, VkDeviceSize *pOffsets);
    void CmdPushConstants(VkCommandBuffer commandBuffer, Pipeline pipeline, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* data);
    void CmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount);
    void SubmitQueue(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence fence);
    void SubmitAndPresentFrame(VkCommandBuffer commandBuffer);

    void AcquiredNextFrame(VkCommandBuffer* pCommandBuffer);
    void RebuildSwapchain();
    void ReadBuffer(Buffer buffer, size_t size, void* data);
    void WriteBuffer(Buffer buffer, size_t size, void* data);
    void CopyBuffer(Buffer srcBuffer, uint64_t srcOffset, Buffer dstBuffer, uint64_t dstOffset, uint64_t size);
    void WriteTexture2D(Texture2D texture, uint64_t size, void* pixels);
    void DeviceWaitIdle();

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
    VkResult _CreateFence(VkFence* pFence);
    VkResult _CreateSemaphore(VkSemaphore* pSemaphore);

    void _DestroySwapchain();
    void _DestroyFence(VkFence fence);
    void _DestroySemaphore(VkSemaphore semaphore);

    VkResult _InitSyncObjects();

    void _DestroySyncObjects();

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
    VkFence submitFence = VK_NULL_HANDLE;

    // Vulkan swapchain resources
    uint32_t minImageCount = 0;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent2D = {};
    uint32_t imageIndex = 0;
    std::vector<VkSemaphore> renderFinishedSemaphores;

    // Sync objects
    uint32_t flightIndex = 0;
    uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkCommandBuffer> frameCommandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkFence> inFlightFences;

    uint32_t queueFamilyIndex = UINT32_MAX;
    VkSurfaceFormatKHR surfaceFormat = {};
    VkPhysicalDeviceProperties physicalDeviceProperties = {};
};

#endif /* RENDER_DRIVER_H_ */
