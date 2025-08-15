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

VkResult RenderDriver::CreatePipeline(const char *shaderName)
{
    VkResult err = VK_SUCCESS;

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
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3 },
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

    /* VkPipelineRasterizationStateCreateInfo */
    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;                   // 超出深度范围裁剪而不是 clamp
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;            // 不丢弃几何体
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;            // 填充多边形方式点、线、面
    rasterizationStateCreateInfo.lineWidth = 1.0f;                              // 线宽
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;              // 背面剔除，可改 NONE 或 FRONT
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;   // 前向面定义
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
    colorBlendAttachmentStage.blendEnable = VK_FALSE; // 关闭混合

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

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = std::size(shaderStagesCreateInfo);
    pipelineCreateInfo.pStages = shaderStagesCreateInfo;
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pTessellationState = VK_NULL_HANDLE;
    pipelineCreateInfo.pViewportState = VK_NULL_HANDLE;
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

    return err;
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

VkResult RenderDriver::_CreateShaderModule(const char* shaderName, const char* stage, VkShaderModule* pShaderModule)
{
    size_t size;
    VkResult err = VK_SUCCESS;

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


void RenderDriver::_DestroySwapchain()
{
    for (uint32_t i = 0; i < imageCount; i++)
        vkDestroyImageView(device, swapchainImageViews[i], VK_NULL_HANDLE);
    swapchainImages.clear();
    swapchainImageViews.clear();
    vkDestroySwapchainKHR(device, swapchain, VK_NULL_HANDLE);
}
