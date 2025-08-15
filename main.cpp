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

    const std::unique_ptr<RenderDriver> driver = std::make_unique<RenderDriver>();

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult err = glfwCreateWindowSurface(driver->GetInstance(), hwindow, VK_NULL_HANDLE, &surface);
    assert(!err);
    driver->Initialize(surface);

    driver->CreatePipeline("universal");

    while (!glfwWindowShouldClose(hwindow)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(hwindow);
    glfwTerminate();

    return 0;
}