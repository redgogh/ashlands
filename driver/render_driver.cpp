#define VK_NO_PROTOTYPES
#define VMA_IMPLEMENTATION

#include "render_driver.h"

#include <stdio.h>
#include "vkutils.h"
#include "utils/ioutils.h"

#define VK_VERSION_1_3_216

#define VK_CHECK_ERROR(err) \
    if (err != VK_SUCCESS) \
        return err;

/* volk 全局只初始化一次 */
static bool volkInitialized = false;

struct Texture2D_T {
    VkImage vkImage = VK_NULL_HANDLE;
    VkImageView vkImageView = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VmaAllocationInfo allocationInfo = {};
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct Buffer_T {
    VkBuffer vkBuffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
    VkDeviceSize size = 0;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_UNKNOWN;
    VmaAllocationInfo allocationInfo;
};

struct Pipeline_T {
    VkPipeline vkPipeline = VK_NULL_HANDLE;
    VkPipelineLayout vkPipelineLayout = VK_NULL_HANDLE;
    VkPipelineBindPoint vkBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

RenderDriver::RenderDriver()
{
    VkResult err;

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
    vkDeviceWaitIdle(device);

    _DestroyFence(submitFence);
    _DestroySyncObjects();
    vkDestroyCommandPool(device, commandPool, VK_NULL_HANDLE);
    // vkDestroySwapchainKHR(device, swapchain, VK_NULL_HANDLE);
    _DestroySwapchain();
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, VK_NULL_HANDLE);
    vkDestroySurfaceKHR(instance, surface, VK_NULL_HANDLE);
    vkDestroyInstance(instance, VK_NULL_HANDLE);
}

VkResult RenderDriver::Initialize(VkSurfaceKHR surface)
{
    VkResult err;

    this->surface = surface;

    err = _CreateDevice();
    VK_CHECK_ERROR(err);

    err = _CreateSwapchain(VK_NULL_HANDLE);
    VK_CHECK_ERROR(err);

    err = _CreateCommandPool();
    VK_CHECK_ERROR(err);

    err = _CreateMemoryAllocator();
    VK_CHECK_ERROR(err);

    err = _InitSyncObjects();
    VK_CHECK_ERROR(err);

    err = _CreateFence(&submitFence);
    VK_CHECK_ERROR(err);

    return err;
}

VkResult RenderDriver::CreateBuffer(const size_t size, VkBufferUsageFlags usage, Buffer *pBuffer)
{
    VkResult err;

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = _GuessMemoryUsage(usage);

    *pBuffer = (Buffer_T*) malloc(sizeof(Buffer_T));

    err = vmaCreateBuffer(allocator,
                          &bufferCreateInfo,
                          &allocationCreateInfo,
                          &(*pBuffer)->vkBuffer,
                          &(*pBuffer)->allocation,
                          &(*pBuffer)->allocationInfo);
    VK_CHECK_ERROR(err);

    (*pBuffer)->usage = usage;
    (*pBuffer)->size = size;
    (*pBuffer)->memoryUsage = allocationCreateInfo.usage;

    return err;
}

void RenderDriver::DestroyBuffer(Buffer buffer)
{
    vmaDestroyBuffer(allocator, buffer->vkBuffer, buffer->allocation);
    free(buffer);
}

VkResult RenderDriver::CreateTexture2D(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, Texture2D *pTexture2D)
{
    VkResult err;

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent.width = w;
    imageCreateInfo.extent.height = h;
    imageCreateInfo.extent.depth = 1.0f;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage = (usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocationInfo = {};
    err = vmaCreateImage(allocator, &imageCreateInfo, &allocationCreateInfo, &image, &allocation, &allocationInfo);
    VK_CHECK_ERROR(err);

    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    err = vkCreateImageView(device, &imageViewCreateInfo, VK_NULL_HANDLE, &imageView);
    VK_CHECK_ERROR(err);

    *pTexture2D = (Texture2D_T *) malloc(sizeof(Texture2D_T));

    (*pTexture2D)->vkImage = image;
    (*pTexture2D)->vkImageView = imageView;
    (*pTexture2D)->allocation = allocation;
    (*pTexture2D)->allocationInfo = allocationInfo;
    (*pTexture2D)->width = w;
    (*pTexture2D)->height = h;
    (*pTexture2D)->format = format;

    return err;
}

void RenderDriver::DestroyTexture2D(Texture2D Texture2D)
{
    vmaDestroyImage(allocator, Texture2D->vkImage, Texture2D->allocation);
    vkDestroyImageView(device, Texture2D->vkImageView, VK_NULL_HANDLE);
    free(Texture2D);
}

VkResult RenderDriver::CreatePipeline(const char *shaderName, Pipeline* pPipeline)
{
    VkResult err;

    /* VkPipelineLayoutCreateInfo */
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    err = vkCreatePipelineLayout(device, &pipelineLayoutInfo, VK_NULL_HANDLE, &pipelineLayout);
    VK_CHECK_ERROR(err);

    /* shader module */
    VkShaderModule vertexShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragmentShaderModule = VK_NULL_HANDLE;

    err = _CreateShaderModule(shaderName, "vert", &vertexShaderModule);
    VK_CHECK_ERROR(err);

    err = _CreateShaderModule(shaderName, "frag", &fragmentShaderModule);
    VK_CHECK_ERROR(err);

    /* VkPipelineShaderStageCreateInfo */
    VkPipelineShaderStageCreateInfo shaderStagesCreateInfo[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexShaderModule,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShaderModule,
            .pName = "main",
        }
    };

    /* VkVertexInputAttributeDescription */
    VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 },
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 2 },
    };

    VkVertexInputBindingDescription vertexInputBindingDescriptions[] = {
        { 0, sizeof(float) * 6, VK_VERTEX_INPUT_RATE_VERTEX }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = ARRAY_SIZE(vertexInputBindingDescriptions);
    vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescriptions[0];
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = ARRAY_SIZE(vertexInputAttributeDescriptions);
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = &vertexInputAttributeDescriptions[0];

    /* VkPipelineInputAssemblyStateCreateInfo */
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    /* VkPipelineViewportStateCreateInfo */
    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.scissorCount = 1;

    /* VkPipelineRasterizationStateCreateInfo */
    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;                   // 超出深度范围裁剪而不是 clamp
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;            // 不丢弃几何体
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;            // 填充多边形方式点、线、面
    rasterizationStateCreateInfo.lineWidth = 1.0f;                              // 线宽
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;              // 背面剔除，可改 NONE 或 FRONT
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;              // 前向面定义
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;                    // 不使用深度偏移
    rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
    rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

    /* VkPipelineMultisampleStateCreateInfo */
    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;                  // 关闭样本着色
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;    // 每像素采样数，1 = 关闭 MSAA
    multisampleStateCreateInfo.minSampleShading = 1.0f;                         // 如果开启 sampleShading，最小采样比例
    multisampleStateCreateInfo.pSampleMask = VK_NULL_HANDLE;                    // 默认全开
    multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;                // alpha to coverage 禁用
    multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;                     // alphaToOne 禁用

    /* VkPipelineColorBlendStateCreateInfo */
    VkPipelineColorBlendAttachmentState colorBlendAttachmentStage = {};
    colorBlendAttachmentStage.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentStage.blendEnable = VK_FALSE;                           // 关闭混合

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;                         // 不使用逻辑操作
    colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;                       // 无效，因为逻辑操作关闭
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentStage;
    colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

    /* VkPipelineDynamicStateCreateInfo[] */
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.dynamicStateCount = ARRAY_SIZE(dynamicStates);
    dynamicStateCreateInfo.pDynamicStates = &dynamicStates[0];

    /* dynamic rendering */
    VkPipelineRenderingCreateInfo pipelineRenderingInfo = {};
    pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &surfaceFormat.format;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = &pipelineRenderingInfo;
    pipelineCreateInfo.stageCount = std::size(shaderStagesCreateInfo);
    pipelineCreateInfo.pStages = shaderStagesCreateInfo;
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pTessellationState = VK_NULL_HANDLE;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = VK_NULL_HANDLE;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
    pipelineCreateInfo.layout = pipelineLayout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    err = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, VK_NULL_HANDLE, &pipeline);
    VK_CHECK_ERROR(err);

    vkDestroyShaderModule(device, vertexShaderModule, VK_NULL_HANDLE);
    vkDestroyShaderModule(device, fragmentShaderModule, VK_NULL_HANDLE);

    Pipeline ret = (Pipeline) malloc(sizeof(Pipeline_T));
    ret->vkPipeline = pipeline;
    ret->vkPipelineLayout = pipelineLayout;
    *pPipeline = ret;

    return err;
}

void RenderDriver::DestroyPipeline(Pipeline pipeline)
{
    vkDestroyPipeline(device, pipeline->vkPipeline, VK_NULL_HANDLE);
    vkDestroyPipelineLayout(device, pipeline->vkPipelineLayout, VK_NULL_HANDLE);
    free(pipeline);
}

VkResult RenderDriver::CreateCommandBuffer(VkCommandBuffer *pCommandBuffer)
{
    VkResult err;

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    commandBufferAllocateInfo.commandPool = commandPool;

    err = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, pCommandBuffer);
    VK_CHECK_ERROR(err);

    return err;
}

void RenderDriver::DestroyCommandBuffer(VkCommandBuffer commandBuffer)
{
    DestroyCommandBuffers(1, &commandBuffer);
}

void RenderDriver::DestroyCommandBuffers(uint32_t count, VkCommandBuffer* pCommandBuffers)
{
    vkFreeCommandBuffers(device, commandPool, count, pCommandBuffers);
}

void RenderDriver::BeginCommandBuffer(VkCommandBuffer commandBuffer)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
}

void RenderDriver::EndCommandBuffer(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);
}

void RenderDriver::CmdTextureMemoryBarrier(VkCommandBuffer commandBuffer, Texture2D texture, VkImageLayout newLayout)
{
    VkImageLayout oldLayout = texture->layout;

    VkAccessFlags srcAccessMask = 0;
    VkAccessFlags dstAccessMask = 0;
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        srcAccessMask = 0;
        dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        goto DO_MEMORY_IAMGE_BARRIER_TAG;
    }

    if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        goto DO_MEMORY_IAMGE_BARRIER_TAG;
    }

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        srcAccessMask = 0;
        dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        goto DO_MEMORY_IAMGE_BARRIER_TAG;
    }

    printf("[vulkan] error - unsupported image layout transition!");
    return;

DO_MEMORY_IAMGE_BARRIER_TAG:
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture->vkImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    vkCmdPipelineBarrier(commandBuffer,
                         srcStageMask,
                         dstStageMask,
                         0,
                         0, VK_NULL_HANDLE,
                         0, VK_NULL_HANDLE,
                         1, &barrier);

    texture->layout = newLayout;
}

void RenderDriver::CmdBeginRendering(VkCommandBuffer commandBuffer)
{
    vkAcquireNextImageKHR(device, swapchain, UINT32_MAX, imageAvailableSemaphores[flightIndex], VK_NULL_HANDLE, &imageIndex);

    VkRenderingAttachmentInfo colorRenderingAttachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchainImageViews[imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .color = { 0.5f, 0.5f, 0.5f, 1.0f }
        }
    };

    VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = swapchainExtent2D
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorRenderingAttachment
    };

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
}

void RenderDriver::CmdEndRendering(VkCommandBuffer commandBuffer)
{
    vkCmdEndRendering(commandBuffer);

    VkImageMemoryBarrier imageMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = swapchainImages[imageIndex],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         0,
                         0, VK_NULL_HANDLE,
                         0, VK_NULL_HANDLE,
                         1, &imageMemoryBarrier);
}

void RenderDriver::CmdBindPipeline(VkCommandBuffer commandBuffer, Pipeline pipeline)
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->vkPipeline);

    VkViewport viewport = {
        .x = 0,
        .y = 0,
        .width = static_cast<float>(swapchainExtent2D.width),
        .height = static_cast<float>(swapchainExtent2D.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = swapchainExtent2D,
    };

    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void RenderDriver::CmdBindVertexBuffer(VkCommandBuffer commandBuffer, Buffer buffer, VkDeviceSize offset)
{
    CmdBindVertexBuffers(commandBuffer, 1, &buffer, &offset);
}

void RenderDriver::CmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t count, Buffer *pBuffers, VkDeviceSize *pOffsets)
{
    std::vector<VkBuffer> buffers(count);

    for (uint32_t i = 0; i < count; i++)
        buffers[i] = pBuffers[i]->vkBuffer;

    vkCmdBindVertexBuffers(commandBuffer, 0, count, std::data(buffers), pOffsets);
}

void RenderDriver::CmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount)
{
    vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
}

void RenderDriver::SubmitQueue(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence fence)
{
    VkResult err;

    if (fence != VK_NULL_HANDLE)
        vkResetFences(device, 1, &fence);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    if (waitSemaphore != VK_NULL_HANDLE) {
        VkPipelineStageFlags pipelineStageFlags[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitSemaphore;
        submitInfo.pWaitDstStageMask = pipelineStageFlags;
    }

    if (signalSemaphore != VK_NULL_HANDLE) {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalSemaphore;
    }

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    err = vkQueueSubmit(queue, 1, &submitInfo, fence);
    assert(!err);
}

void RenderDriver::SubmitAndPresentFrame(VkCommandBuffer commandBuffer)
{
    VkResult err;

    SubmitQueue(commandBuffer, imageAvailableSemaphores[flightIndex], renderFinishedSemaphores[imageIndex], inFlightFences[flightIndex]);

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphores[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex,
    };

    err = vkQueuePresentKHR(queue, &presentInfo);
    assert(!err);
}

void RenderDriver::CopyBuffer(Buffer srcBuffer, uint64_t srcOffset, Buffer dstBuffer, uint64_t dstOffset, uint64_t size)
{
    VkCommandBuffer commandBuffer;
    CreateCommandBuffer(&commandBuffer);
    BeginCommandBuffer(commandBuffer);

    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;

    vkCmdCopyBuffer(commandBuffer, srcBuffer->vkBuffer, dstBuffer->vkBuffer, 1, &copyRegion);

    EndCommandBuffer(commandBuffer);
    SubmitQueue(commandBuffer, VK_NULL_HANDLE, VK_NULL_HANDLE, submitFence);
    vkWaitForFences(device, 1, &submitFence, VK_TRUE, UINT32_MAX);
}

void RenderDriver::WriteTexture2D(Texture2D texture, uint64_t size, void *pixels)
{
    Buffer stagingBuffer;
    CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stagingBuffer);
    WriteBuffer(stagingBuffer, size, pixels);

    VkCommandBuffer commandBuffer;
    CreateCommandBuffer(&commandBuffer);
    BeginCommandBuffer(commandBuffer);

    CmdTextureMemoryBarrier(commandBuffer, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { texture->width, texture->height, 1 }
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        stagingBuffer->vkBuffer,
        texture->vkImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copyRegion);

    EndCommandBuffer(commandBuffer);
    SubmitQueue(commandBuffer, VK_NULL_HANDLE, VK_NULL_HANDLE, submitFence);
    vkWaitForFences(device, 1, &submitFence, VK_TRUE, UINT32_MAX);

    DestroyBuffer(stagingBuffer);
}

void RenderDriver::DeviceWaitIdle()
{
    vkDeviceWaitIdle(device);
}

void RenderDriver::AcquiredNextFrame(VkCommandBuffer* pCommandBuffer)
{
    flightIndex = (flightIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    *pCommandBuffer = frameCommandBuffers[flightIndex];

    vkWaitForFences(device, 1, &inFlightFences[flightIndex], VK_TRUE, UINT32_MAX);
    vkResetFences(device, 1, &inFlightFences[flightIndex]);

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    VkExtent2D currentExtent2D = capabilities.currentExtent;

    if (currentExtent2D.width != swapchainExtent2D.width || currentExtent2D.height != swapchainExtent2D.height)
        RebuildSwapchain();
}

void RenderDriver::RebuildSwapchain()
{
    _CreateSwapchain(swapchain);
}

void RenderDriver::ReadBuffer(Buffer buffer, size_t size, void *data)
{
    void* src;
    vmaMapMemory(allocator, buffer->allocation, &src);
    memcpy(data, src, size);
    vmaUnmapMemory(allocator, buffer->allocation);
}

void RenderDriver::WriteBuffer(Buffer buffer, size_t size, void *data)
{
    if (buffer->memoryUsage == VMA_MEMORY_USAGE_GPU_ONLY) {
        Buffer stagingBuffer;
        CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stagingBuffer);
        WriteBuffer(stagingBuffer, size, data);
        CopyBuffer(stagingBuffer, 0, buffer, 0, size);
        DestroyBuffer(stagingBuffer);
        return;
    }

    /* buffer->memoryUsage != VMA_MEMORY_USAGE_GPU_ONLY */
    void* dst;
    vmaMapMemory(allocator, buffer->allocation, &dst);
    memcpy(dst, data, size);
    vmaUnmapMemory(allocator, buffer->allocation);
}

VkResult RenderDriver::_CreateInstance()
{
    VkResult err;

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "capybara";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.pEngineName = "capybara";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_3;

    const std::vector<const char*> layers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
    #if defined(_WIN32)
        "VK_KHR_win32_surface",
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
    VkResult err;

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
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
#ifdef __APPLE__
        "VK_KHR_portability_subset"
#endif
    };

    /* dynamic rendering */
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature = {};
    dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicRenderingFeature.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &dynamicRenderingFeature;
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

VkResult RenderDriver::_CreateMemoryAllocator()
{
    VkResult err;

    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

    err = vmaCreateAllocator(&allocatorCreateInfo, &allocator);
    VK_CHECK_ERROR(err);

    return err;
}

VkResult RenderDriver::_CreateSwapchain(VkSwapchainKHR oldSwapchain)
{
    VkResult err;

    if (oldSwapchain != VK_NULL_HANDLE)
        DeviceWaitIdle();

    VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
    VK_CHECK_ERROR(err);

    minImageCount = surfaceCapabilities.minImageCount + 1;
    if (minImageCount > surfaceCapabilities.maxImageCount)
        minImageCount = surfaceCapabilities.maxImageCount;

    swapchainExtent2D = surfaceCapabilities.currentExtent;

    uint32_t formatCount = 0;
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, VK_NULL_HANDLE);
    VK_CHECK_ERROR(err);

    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, std::data(surfaceFormats));
    VK_CHECK_ERROR(err);

    surfaceFormat = VkUtils::ChooseSwapSurfaceFormat(surfaceFormats);

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.minImageCount = minImageCount;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = swapchainExtent2D;
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
    err = vkGetSwapchainImagesKHR(device, swapchain, &minImageCount, nullptr);
    VK_CHECK_ERROR(err);

    swapchainImages.resize(minImageCount);
    swapchainImageViews.resize(minImageCount);
    renderFinishedSemaphores.resize(minImageCount);
    
    err = vkGetSwapchainImagesKHR(device, swapchain, &minImageCount, std::data(swapchainImages));
    VK_CHECK_ERROR(err);

    for (uint32_t i = 0; i < minImageCount; i++) {
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

        err = _CreateSemaphore(&renderFinishedSemaphores[i]);
        VK_CHECK_ERROR(err);
    }

    return err;
}

VkResult RenderDriver::_CreateCommandPool()
{
    VkResult err;

    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
                                  | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    err = vkCreateCommandPool(device, &commandPoolCreateInfo, VK_NULL_HANDLE, &commandPool);
    VK_CHECK_ERROR(err);

    return err;
}

VkResult RenderDriver::_CreateShaderModule(const char* shaderName, const char* stage, VkShaderModule* pShaderModule)
{
    size_t size;
    VkResult err;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s.%s.spv", shaderName, stage);

    char *buf = io_read_bytecode(path, &size);

    printf("[vulkan] load shader module %s, code size=%ld\n", path, size);

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = size;
    shaderModuleCreateInfo.pCode = reinterpret_cast<uint32_t*>(buf);

    err = vkCreateShaderModule(device, &shaderModuleCreateInfo, VK_NULL_HANDLE, pShaderModule);
    io_free_buf(buf);
    VK_CHECK_ERROR(err);

    return err;
}

VkResult RenderDriver::_CreateFence(VkFence *pFence)
{
    VkResult err;

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    err = vkCreateFence(device, &fenceCreateInfo, VK_NULL_HANDLE, pFence);
    VK_CHECK_ERROR(err);

    return err;
}

VkResult RenderDriver::_CreateSemaphore(VkSemaphore *pSemaphore)
{
    VkResult err;

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    err = vkCreateSemaphore(device, &semaphoreCreateInfo, VK_NULL_HANDLE, pSemaphore);
    VK_CHECK_ERROR(err);

    return err;
}

void RenderDriver::_DestroySwapchain()
{
    for (uint32_t i = 0; i < minImageCount; i++) {
        vkDestroyImageView(device, swapchainImageViews[i], VK_NULL_HANDLE);
        _DestroySemaphore(renderFinishedSemaphores[i]);
    }

    swapchainImages.clear();
    swapchainImageViews.clear();
    vkDestroySwapchainKHR(device, swapchain, VK_NULL_HANDLE);
}

void RenderDriver::_DestroyFence(VkFence fence)
{
    vkDestroyFence(device, fence, VK_NULL_HANDLE);
}

void RenderDriver::_DestroySemaphore(VkSemaphore semaphore)
{
    vkDestroySemaphore(device, semaphore, VK_NULL_HANDLE);
}

VkResult RenderDriver::_InitSyncObjects()
{
    VkResult err;

    frameCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        err = CreateCommandBuffer(&frameCommandBuffers[i]);
        VK_CHECK_ERROR(err);

        err = _CreateFence(&inFlightFences[i]);
        VK_CHECK_ERROR(err);

        err = _CreateSemaphore(&imageAvailableSemaphores[i]);
        VK_CHECK_ERROR(err);
    }

    return err;
}

void RenderDriver::_DestroySyncObjects()
{
    DestroyCommandBuffers(MAX_FRAMES_IN_FLIGHT, std::data(frameCommandBuffers));

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        _DestroyFence(inFlightFences[i]);
        _DestroySemaphore(imageAvailableSemaphores[i]);
    }
}

VmaMemoryUsage RenderDriver::_GuessMemoryUsage(VkBufferUsageFlags usage)
{
    if ((usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) && (usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT))
        return VMA_MEMORY_USAGE_GPU_ONLY;

    if (usage & (VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
        return VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
        | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
        | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
        return VMA_MEMORY_USAGE_GPU_ONLY;

    // 默认：CPU -> GPU
    return VMA_MEMORY_USAGE_CPU_TO_GPU;
}