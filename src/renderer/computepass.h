#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "camera.h"
#include "../scene/scene.h"
#include <string>
#include <vector>

class VulkanContext;
class Swapchain;

class ComputePass {
public:
    ComputePass(VulkanContext* context, Swapchain* swapchain);
    ~ComputePass();

    // The functions called every frame in main.cpp
    void dispatch(VkCommandBuffer cmdBuffer, uint32_t width, uint32_t height);
    void copyToSwapchain(VkCommandBuffer cmdBuffer, uint32_t swapchainImageIndex);
    void updateCamera(const CameraUBO& ubo);

private:
    VulkanContext* context;
    Swapchain* swapchain;

    // Pipeline and layout
    VkPipelineLayout pipelineLayout;
    VkPipeline computePipeline;

    // Descriptors (How we pass the image to the shader)
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    // The Storage Texture the shader actually writes to
    VkImage storageImage;
    VkDeviceMemory storageImageMemory;
    VkImageView storageImageView;
    VkFormat storageFormat = VK_FORMAT_R8G8B8A8_UNORM;

    // Setup functions
    void createStorageImage();
    void createDescriptorSetLayout();
    void createComputePipeline();
    void createDescriptorPoolAndSets();

    // Helpers
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void transitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
// camera 
    glm::vec3 camPos = glm::vec3(0.0f, 0.0f, 5.0f);
    glm::vec3 camForward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 camRight = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 camUp = glm::vec3(0.0f, 1.0f, 0.0f);

    VkBuffer cameraBuffer;
VkDeviceMemory cameraBufferMemory;
void* cameraBufferMapped; // Pointer for persistent mapping
    VkBuffer sceneBuffer;
    VkDeviceMemory sceneBufferMemory;
    void* sceneBufferMapped;
    
    void createSceneBuffer();

void createCameraBuffer();
void updateDescriptorSets();

};