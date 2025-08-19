#include <memory>
#include "driver/render_driver.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <unistd.h>

#ifdef WIN32
#include <direct.h>
#endif

struct Vertex
{
    float pos[2];
    float color[3];
};

Vertex vertices[] = {
    {0.0f, -0.5f}, {1.0f, 0.0f, 0.0f},
    {0.5f, 0.5f}, {0.0f, 1.0f, 0.0f},
    {-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f},
};

int main()
{
#ifdef WIN32
    char _cwd[PATH_MAX];
    system("chcp 65001");
    getcwd(_cwd, sizeof(_cwd));
    _chdir("../shaders");
    system("spvc.bat");
    _chdir(_cwd);
#endif

#ifdef __APPLE__
    char _cwd[PATH_MAX];
    getcwd(_cwd, sizeof(_cwd));
    chdir("../shaders");
    system("./spvc");
    chdir(_cwd);
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


    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    driver->CreateCommandBuffer(&commandBuffer);
    driver->BeginCommandBuffer(commandBuffer);
    driver->EndCommandBuffer(commandBuffer);
    driver->DestroyCommandBuffer(commandBuffer);

    Pipeline pipeline = VK_NULL_HANDLE;
    driver->CreatePipeline("universal", &pipeline);

    while (!glfwWindowShouldClose(hwindow)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(hwindow);
    glfwTerminate();

    return 0;
}