#include <memory>
#include "driver/render_driver.h"
#define GLFW_INCLUDE_VULKAN
#include <dlfcn.h>
#include <GLFW/glfw3.h>

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* hwindow =
        glfwCreateWindow(800, 600, "AshLands", nullptr, nullptr);

    if (hwindow == nullptr)
        throw std::runtime_error("Failed to create GLFW window");

    void* handle = VK_NULL_HANDLE;
    handle = dlopen("libvulkan.1.4.321.dylib", RTLD_LAZY | RTLD_LOCAL);

    const std::unique_ptr<RenderDriver> driver = std::make_unique<RenderDriver>();
    int r = glfwVulkanSupported();
    if (!r)
        throw std::runtime_error("Failed to create GLFW vulkan support");

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    glfwCreateWindowSurface(driver->GetInstance(), hwindow, VK_NULL_HANDLE, &surface);
    assert(surface != VK_NULL_HANDLE);
    driver->Initialize(VK_NULL_HANDLE);

    while (!glfwWindowShouldClose(hwindow)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(hwindow);
    glfwTerminate();

    return 0;
}