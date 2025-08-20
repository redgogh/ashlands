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

    size_t bufferSize = sizeof(vertices);

    Buffer srcBuffer;
    driver->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &srcBuffer);
    driver->WriteBuffer(srcBuffer, bufferSize, vertices);

    Buffer dstBuffer;
    driver->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &dstBuffer);
    driver->CopyBuffer(srcBuffer, 0, dstBuffer, 0, bufferSize);

    driver->DestroyBuffer(srcBuffer);

    float tmp[2];
    driver->ReadBuffer(dstBuffer, sizeof(float) * 2, tmp);
    printf("x: %f, y: %f\n", tmp[0], tmp[1]);
    driver->DestroyBuffer(dstBuffer);

    glfwDestroyWindow(hwindow);
    glfwTerminate();

    return 0;
}