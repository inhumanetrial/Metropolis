#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <vector>

#include "core/VulkanContext.h"
#include "core/Swapchain.h"
#include "renderer/ComputePass.h"
#include "renderer/camera.h"

const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;

class Engine {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    std::unique_ptr<VulkanContext> context;
    std::unique_ptr<Swapchain> swapchain;
    std::unique_ptr<ComputePass> computepass;

    Camera camera; 
    float deltaTime = 0.0f; 
    float lastFrame = 0.0f;
    float frameCount = 1.0f;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Metropolis", nullptr, nullptr);
        if (!window) throw std::runtime_error("Failed to create GLFW window");

        // Set up Input
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetWindowUserPointer(window, this); 

        glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
            auto engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(win));
            if (engine->camera.firstMouse) {
                engine->camera.lastMouseX = (float)xpos;
                engine->camera.lastMouseY = (float)ypos;
                engine->camera.firstMouse = false;
            }
            float xoffset = (float)xpos - engine->camera.lastMouseX;
            float yoffset = engine->camera.lastMouseY - (float)ypos; 
            engine->camera.lastMouseX = (float)xpos;
            engine->camera.lastMouseY = (float)ypos;
            engine->camera.processMouseMovement(xoffset, yoffset);
            engine->frameCount = 1.0f;
        });
    }

    void initVulkan() {
        context = std::make_unique<VulkanContext>(window);
        swapchain = std::make_unique<Swapchain>(context.get(), window);
        computepass = std::make_unique<ComputePass>(context.get(), swapchain.get());
    }

    void mainLoop() {
        lastFrame = (float)glfwGetTime();
        // float frameCount = 1.0f;
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            float currentFrame = (float)glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            updateCamera(deltaTime);
            drawFrame();
        }
        vkDeviceWaitIdle(context->getDevice());
    }

    void updateCamera(float dt) {
        float moveSpeed = 5.0f * dt;
        bool cameraMoved = false;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.position += camera.forward * moveSpeed; cameraMoved = true;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.position -= camera.forward * moveSpeed; cameraMoved = true;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.position -= camera.right * moveSpeed; cameraMoved = true;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.position += camera.right * moveSpeed; cameraMoved = true;

        if (cameraMoved) {
            frameCount = 1.0f;
        }

        CameraUBO ubo{};
        ubo.position = glm::vec4(camera.position, 1.0f);
        ubo.forward  = glm::vec4(camera.forward, 0.0f);
        ubo.right    = glm::vec4(camera.right, 0.0f);
        ubo.up       = glm::vec4(camera.up, 0.0f);

        computepass->updateCamera(ubo);
    }

    void drawFrame() {
        VkFence inFlightFence = swapchain->getInFlightFence();
        vkWaitForFences(context->getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        
        uint32_t imageIndex;
        if (!swapchain->acquireNextImage(&imageIndex)) return;

        vkResetFences(context->getDevice(), 1, &inFlightFence);

        VkCommandBuffer cmdBuffer = context->beginCommandBuffer();
        computepass->dispatch(cmdBuffer, WIDTH, HEIGHT, frameCount);
        frameCount +=1.0f;
        computepass->copyToSwapchain(cmdBuffer, imageIndex);
        vkEndCommandBuffer(cmdBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = { swapchain->getImageAvailableSemaphore() }; 
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;
        VkSemaphore signalSemaphores[] = { swapchain->getRenderFinishedSemaphore() };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(context->getComputeQueue(), 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        swapchain->present(imageIndex);
        vkFreeCommandBuffers(context->getDevice(), context->getCommandPool(), 1, &cmdBuffer);
    }

    void cleanup() {
        computepass.reset();
        swapchain.reset();
        context.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    Engine app;
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}