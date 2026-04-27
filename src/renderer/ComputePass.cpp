#include "ComputePass.h"
#include "../core/VulkanContext.h"
#include "../core/Swapchain.h"
#include <fstream>
#include <stdexcept>
#include <array>

ComputePass::ComputePass(VulkanContext* context, Swapchain* swapchain)
    : context(context), swapchain(swapchain) {
    createStorageImage();
    createDescriptorSetLayout();
    createCameraBuffer();
    createSceneBuffer();
    createComputePipeline();
    createDescriptorPoolAndSets();

    // Initialize the  storage image 
    VkCommandBuffer cmd = context->beginSingleTimeCommands();
    transitionImageLayout(cmd, storageImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    transitionImageLayout(cmd, accumImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    context->endSingleTimeCommands(cmd);
}

ComputePass::~ComputePass() {
    VkDevice device = context->getDevice();
    vkDestroyPipeline(device, computePipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyImageView(device, storageImageView, nullptr);
    vkDestroyImage(device, storageImage, nullptr);
    vkFreeMemory(device, storageImageMemory, nullptr);
    vkDestroyImageView(device, accumImageView, nullptr);
    vkDestroyImage(device, accumImage, nullptr);
    vkFreeMemory(device, accumImageMemory, nullptr);
    vkDestroyBuffer(device, cameraBuffer, nullptr);
    vkFreeMemory(device, cameraBufferMemory, nullptr);
    vkDestroyBuffer(device, sceneBuffer, nullptr);
    vkFreeMemory(device, sceneBufferMemory, nullptr);
}

    // Dispatch shader into thread groups
void ComputePass::dispatch(VkCommandBuffer cmdBuffer, uint32_t width, uint32_t height, float frameCount) {
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &frameCount);  
    vkCmdDispatch(cmdBuffer, (width + 15) / 16, (height + 15) / 16, 1);
}

void ComputePass::copyToSwapchain(VkCommandBuffer cmdBuffer, uint32_t swapchainImageIndex) {
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(context->getDevice(), swapchain->getSwapchain(), &imageCount, nullptr);
    
    std::vector<VkImage> swapchainImages(imageCount);
    vkGetSwapchainImagesKHR(context->getDevice(), swapchain->getSwapchain(), &imageCount, swapchainImages.data());
    
    VkImage targetImage = swapchainImages[swapchainImageIndex];

    transitionImageLayout(cmdBuffer, storageImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transitionImageLayout(cmdBuffer, targetImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.extent = { swapchain->getExtent().width, swapchain->getExtent().height, 1 };

    vkCmdCopyImage(cmdBuffer, 
        storageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion);

    transitionImageLayout(cmdBuffer, storageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    transitionImageLayout(cmdBuffer, targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void ComputePass::createStorageImage() {
    VkDevice device = context->getDevice();
    VkExtent2D extent = swapchain->getExtent();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = storageFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &storageImage) != VK_SUCCESS) throw std::runtime_error("Failed to create storage image!");

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, storageImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &storageImageMemory) != VK_SUCCESS) throw std::runtime_error("Failed to allocate image memory!");
    vkBindImageMemory(device, storageImage, storageImageMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = storageImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = storageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &storageImageView) != VK_SUCCESS) throw std::runtime_error("Failed to create texture image view!");

// accumulation image
VkImageCreateInfo accumImageInfo{};
accumImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
accumImageInfo.imageType = VK_IMAGE_TYPE_2D;
accumImageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT; 
accumImageInfo.extent.width = extent.width;
accumImageInfo.extent.height = extent.height;
accumImageInfo.extent.depth = 1;
accumImageInfo.mipLevels = 1;
accumImageInfo.arrayLayers = 1;
accumImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
accumImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
accumImageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT; 
if (vkCreateImage(context->getDevice(), &accumImageInfo, nullptr, &accumImage) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create accumulation image!");
}

VkMemoryRequirements accumMemReqs;
vkGetImageMemoryRequirements(context->getDevice(), accumImage, &accumMemReqs);

VkMemoryAllocateInfo accumAllocInfo{};
accumAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
accumAllocInfo.allocationSize = accumMemReqs.size;
accumAllocInfo.memoryTypeIndex = findMemoryType(accumMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

vkAllocateMemory(context->getDevice(), &accumAllocInfo, nullptr, &accumImageMemory);
vkBindImageMemory(context->getDevice(), accumImage, accumImageMemory, 0);

VkImageViewCreateInfo accumViewInfo{};
accumViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
accumViewInfo.image = accumImage;
accumViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
accumViewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
accumViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
accumViewInfo.subresourceRange.baseMipLevel = 0;
accumViewInfo.subresourceRange.levelCount = 1;
accumViewInfo.subresourceRange.baseArrayLayer = 0;
accumViewInfo.subresourceRange.layerCount = 1;

vkCreateImageView(context->getDevice(), &accumViewInfo, nullptr, &accumImageView);
}

void ComputePass::createDescriptorSetLayout() {
    std::vector<VkDescriptorSetLayoutBinding> bindings(4);

    // Binding 0: Storage Image
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Camera Uniform Buffer
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = nullptr;
    // Binding 2: sStorage Buffer
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; 
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    // Binding 3: Accum image
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data(); 

    if (vkCreateDescriptorSetLayout(context->getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void ComputePass::createComputePipeline() {
    auto shaderCode = readFile("shaders/shader.spv");
    VkShaderModule shaderModule = createShaderModule(shaderCode);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(float);

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    if (vkCreatePipelineLayout(context->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage = shaderStageInfo;

    if (vkCreateComputePipelines(context->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline!");
    }

    vkDestroyShaderModule(context->getDevice(), shaderModule, nullptr);
}

void ComputePass::createDescriptorPoolAndSets() {
    // reserve memory
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[0].descriptorCount = 2;

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 1;

    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(context->getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
    // allocate memory
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    if (vkAllocateDescriptorSets(context->getDevice(), &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    // update
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = storageImageView;

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = cameraBuffer; 
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(CameraUBO);

    VkDescriptorBufferInfo sceneBufferInfo{};
    sceneBufferInfo.buffer = sceneBuffer; 
    sceneBufferInfo.offset = 0;
    sceneBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo accumInfo{};
    accumInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    accumInfo.imageView = accumImageView;
    accumInfo.sampler = VK_NULL_HANDLE;

    std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

    // Ticket 1: The Image 
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &imageInfo;

    // Ticket 2: Camera Buffer
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &bufferInfo; 

    // 2. Ticket 3: Scene SSBO
    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].pNext = nullptr;
    descriptorWrites[2].dstSet = descriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pImageInfo = nullptr;
    descriptorWrites[2].pBufferInfo = &sceneBufferInfo; 
    descriptorWrites[2].pTexelBufferView = nullptr;

    // Ticket 4: accumimage
    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = descriptorSet;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pImageInfo = &accumInfo;

    vkUpdateDescriptorSets(context->getDevice(), 4, descriptorWrites.data(), 0, nullptr);
}

// Utility Functions

std::vector<char> ComputePass::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed to open file: " + filename);
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule ComputePass::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(context->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }
    return shaderModule;
}

uint32_t ComputePass::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context->getPhysicalDevice(), &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

void ComputePass::transitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(cmdBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}
void ComputePass::createCameraBuffer() {
    VkDeviceSize bufferSize = sizeof(CameraUBO);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(context->getDevice(), &bufferInfo, nullptr, &cameraBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(context->getDevice(), cameraBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    // CPU writes directly to HOST_VISIBLE and HOST_COHERENT
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &cameraBufferMemory);
    vkBindBufferMemory(context->getDevice(), cameraBuffer, cameraBufferMemory, 0);

    vkMapMemory(context->getDevice(), cameraBufferMemory, 0, sizeof(CameraUBO), 0, &cameraBufferMapped);
}
void ComputePass::updateCamera(const CameraUBO& ubo) {
    if (cameraBufferMapped != nullptr) {
        memcpy(cameraBufferMapped, &ubo, sizeof(CameraUBO));
    }
}
void ComputePass::createSceneBuffer() {
    std::vector<Object> scene;
   
// x, y, z, radius | r, g, b, padding

scene.push_back({
        glm::vec4(0.0f, 101.0f, -5.0f, 100.0f), 
        glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(0.0f), 
        glm::vec4(0.1f, 0.8f, 0.2f, 0.0f) // Type 0.0f
    });
    // Sphere 1: Blue, left
scene.push_back({
        glm::vec4(0.0f, 101.0f, -5.0f, 100.0f), 
        glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(0.0f), 
        glm::vec4(0.1f, 0.8f, 0.2f, 0.0f) // Type 0.0f
    });

    // 1. Blue Sphere (Left)
    scene.push_back({
        glm::vec4(-2.5f, 0.0f, -5.0f, 1.0f), // Center & Radius
        glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(0.0f), 
        glm::vec4(0.1f, 0.2f, 0.8f, 0.0f) // Type 0.0f
    });

    // 2. Red Cuboid / Box (Center)
    scene.push_back({
        glm::vec4(0.0f, 0.0f, -5.0f, 0.0f), // Center
        glm::vec4(0.8f, 1.2f, 0.8f, 0.0f),  // Half-Extents (Width, Height, Depth)
        glm::vec4(0.0f), glm::vec4(0.0f),   
        glm::vec4(0.8f, 0.1f, 0.1f, 1.0f) // Type 1.0f
    });

    // 3. Yellow Tetrahedron (Right)
    scene.push_back({
        glm::vec4( 2.5f, -1.0f, -5.0f, 0.0f), // Top Vertex
        glm::vec4( 1.5f,  1.0f, -4.0f, 0.0f), // Base Left
        glm::vec4( 3.5f,  1.0f, -4.0f, 0.0f), // Base Right
        glm::vec4( 2.5f,  1.0f, -6.0f, 0.0f), 
        glm::vec4(0.8f, 0.8f, 0.1f, 2.0f)
    });


    VkDeviceSize bufferSize = sizeof(Object) * scene.size();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; 
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(context->getDevice(), &bufferInfo, nullptr, &sceneBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create scene buffer!");
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(context->getDevice(), sceneBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;

    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &sceneBufferMemory);
    vkBindBufferMemory(context->getDevice(), sceneBuffer, sceneBufferMemory, 0);

    vkMapMemory(context->getDevice(), sceneBufferMemory, 0, bufferSize, 0, &sceneBufferMapped);
    memcpy(sceneBufferMapped, scene.data(), (size_t)bufferSize);
    
    // Note: Since the scene is static for now, we don't need to unmap it or update it every frame.
}