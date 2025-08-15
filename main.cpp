#include <memory>
#include "driver/render_driver.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifdef WIN32
#include <direct.h>
#endif

int main()
{
#ifdef WIN32
    char _cwd[512];
    system("chcp 65001");
    getcwd(_cwd, sizeof(_cwd));
    _chdir("../shaders");
    system("spvc.bat");
    _chdir(_cwd);
#endif

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