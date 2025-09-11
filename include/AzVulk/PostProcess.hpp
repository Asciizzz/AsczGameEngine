#pragma once

#include "AzVulk/Device.hpp"
#include "AzVulk/SwapChain.hpp"
#include "AzVulk/Descriptor.hpp"
#include "AzVulk/Pipeline_core.hpp"
#include "AzVulk/CmdBuffer.hpp"

#include <vector>
#include <string>
#include <unordered_map>
#include <array>
#include <vulkan/vulkan.h>

namespace AzVulk {

/**
 * @brief Represents a single post-processing effect with its shader and descriptor layout
 */
struct PostEffect {
    const Device* vkDevice;
    std::string shaderPath;
    
    DescLayout descLayout;
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo shaderStageInfo{};

    PostEffect(const Device* vkDevice, const std::string& computeShaderPath);
    ~PostEffect();
    
    // Delete copy constructor and assignment operator
    PostEffect(const PostEffect&) = delete;
    PostEffect& operator=(const PostEffect&) = delete;

private:
    void loadShader();
    void createDescriptorLayout();

public:
    void updateDescriptorSet(VkDescriptorSet descriptorSet, VkImageView inputImage, VkImageView outputImage);
};

/**
 * @brief Manages a chain of post-processing effects using ping-pong rendering
 * 
 * The PostProcessor is designed to be agnostic about specific effects like FXAA, bloom, or fog.
 * It simply chains compute shaders together using a ping-pong pattern where each effect
 * reads from the previous effect's output and writes to its own output target.
 * 
 * Features:
 * - Dynamic effect loading from compute shader files
 * - Automatic ping-pong buffer management
 * - Per-frame descriptor set management for multi-frame rendering
 * - Reusable pipeline and descriptor infrastructure
 */
class PostProcessor {
public:
    PostProcessor(const Device* vkDevice, const SwapChain* swapChain);
    ~PostProcessor();

    // Delete copy constructor and assignment operator
    PostProcessor(const PostProcessor&) = delete;
    PostProcessor& operator=(const PostProcessor&) = delete;

    /**
     * @brief Add a post-processing effect from a compute shader file
     * @param computeShaderPath Path to the compiled compute shader (.spv file)
     */
    void addEffect(const std::string& computeShaderPath);

    /**
     * @brief Execute the entire post-processing chain
     * @param inputImage The input image to process (usually from the main render pass)
     * @param outputImage The final output target (usually swapchain image)
     * @param frameIndex Current frame index for multi-frame rendering
     */
    void execute(VkImageView inputImage, VkImageView outputImage, uint32_t frameIndex);

private:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr int MAX_EFFECTS = 16; // Maximum number of effects

    const Device* vkDevice;
    const SwapChain* swapChain;
    uint32_t currentFrameIndex;

    // Effects and their resources
    std::vector<PostEffect*> effects;
    std::vector<VkPipeline> pipelines;
    std::vector<VkPipelineLayout> pipelineLayouts;

    // Ping-pong images for intermediate results
    struct PingPongImage {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
    };
    std::vector<PingPongImage> pingPongImages;

    // Descriptor management
    DescPool descPool;
    std::unordered_map<size_t, std::array<DescSets, MAX_FRAMES_IN_FLIGHT>> effectDescriptorSets;

    // Command buffer management
    CmdBuffer cmdBuffer;

    // Internal methods
    void createPingPongImages();
    void createDescriptorPool();
    void createCommandBuffers();
    void createPipelines();
    void createPipelineForEffect(size_t effectIndex);
    void executeEffect(VkCommandBuffer cmdBuffer, size_t effectIndex, VkImageView inputImage, VkImageView outputImage);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    void cleanup();
};

}