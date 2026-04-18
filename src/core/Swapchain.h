#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

// Forward declaration to avoid circular includes
class VulkanContext;

class Swapchain {
public:
    Swapchain(VulkanContext* context, GLFWwindow* window);
    ~Swapchain();

    // The two critical loop functions we used in main.cpp
    bool acquireNextImage(uint32_t* imageIndex);
    void present(uint32_t imageIndex);

    // Getters for the Compute Pass to use later
    VkFormat getFormat() const { return swapChainImageFormat; }
    VkExtent2D getExtent() const { return swapChainExtent; }
    VkSwapchainKHR getSwapchain() const { return swapChain; }
    VkFence getInFlightFence() const { return inFlightFence; }
    VkSemaphore getImageAvailableSemaphore() const { return imageAvailableSemaphore; }
    VkSemaphore getRenderFinishedSemaphore() const { return renderFinishedSemaphore; }

private:
    VulkanContext* context;
    GLFWwindow* window;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    // Synchronization primitives
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

    void createSwapchain();
    void createImageViews();
    void createSyncObjects();
};