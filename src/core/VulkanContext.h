#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>

// Helper struct
struct QueueFamilyIndices {
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return computeFamily.has_value() && presentFamily.has_value();
    }
};

class VulkanContext {
public:
    VulkanContext(GLFWwindow* window);
    ~VulkanContext();

    VkDevice getDevice() const { return device; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkSurfaceKHR getSurface() const { return surface; }
    VkQueue getComputeQueue() const { return computeQueue; }
    VkQueue getPresentQueue() const { return presentQueue; }
    
    VkCommandBuffer beginSingleTimeCommands();
    VkCommandBuffer beginCommandBuffer();

    VkCommandPool getCommandPool() const { return commandPool; }
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

private:
    GLFWwindow* window;

    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    
    VkQueue computeQueue;
    VkQueue presentQueue;
    
    VkCommandPool commandPool;

    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();

    // Helpers
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
};