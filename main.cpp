#include <memory>
#include "driver/render_driver.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <unistd.h>

#ifdef WIN32
#include <direct.h>
#endif

#include <stb/stb_image.h>

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
        glfwCreateWindow(800, 600, "capybara", nullptr, nullptr);

    if (hwindow == nullptr)
        throw std::runtime_error("Failed to create GLFW window");

    const std::unique_ptr<RenderDriver> driver = std::make_unique<RenderDriver>();

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult err = glfwCreateWindowSurface(driver->GetInstance(), hwindow, VK_NULL_HANDLE, &surface);
    assert(!err);
    driver->Initialize(surface);

    stbi_uc* pixels;
    int w, h, channels;
    char path[] = "/Users/redgogh/Desktop/Snipaste_2025-08-21_17-19-47.png";
    pixels = stbi_load(path, &w, &h, &channels, STBI_rgb_alpha);

    Texture2D texture;
    driver->CreateTexture2D(w, h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &texture);
    driver->WriteTexture2D(texture, w * h * 4, pixels);

    stbi_image_free(pixels);
    driver->DestroyTexture2D(texture);

    glfwDestroyWindow(hwindow);
    glfwTerminate();

    return 0;
}