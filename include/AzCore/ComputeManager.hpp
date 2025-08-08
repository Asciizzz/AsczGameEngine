#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <glm/glm.hpp>

namespace AzVulk {
    class Device;
}

namespace AzCore {

    // Generic compute shader data structure - matches vertex shader layout
    struct ComputeParticle {
        glm::vec3 position;    // location = 0
        glm::vec3 velocity;    // location = 1
        glm::vec3 acceleration; // location = 2
        float life;            // location = 3
        float age;             // location = 4
        float size;            // location = 5
        glm::vec4 color;       // location = 6
    };

    // Uniform buffer for particle compute shader
    struct ParticleComputeUBO {
        glm::vec4 emitterPos;        // xyz: position, w: emission rate
        glm::vec4 forces;           // xyz: gravity/wind, w: damping
        glm::vec4 lifeSpan;         // x: min life, y: max life, z: deltaTime, w: maxParticles
        glm::vec4 velocityRange;    // xy: min velocity, zw: max velocity
        glm::vec4 sizeRange;        // xy: min size, zw: max size
        glm::vec4 colorStart;       // starting color
        glm::vec4 colorEnd;         // ending color
    };

    class ComputeManager {
    public:
        ComputeManager(const AzVulk::Device& device, VkCommandPool commandPool);
        ~ComputeManager();

        ComputeManager(const ComputeManager&) = delete;
        ComputeManager& operator=(const ComputeManager&) = delete;

        // Initialize particle compute shader
        bool initializeParticleSystem(uint32_t maxParticles);
        
        // Update particle system
        void updateParticles(const ParticleComputeUBO& params);
        
        // Get particle data for rendering
        const std::vector<ComputeParticle>& getParticles() const { return particleData; }
        VkBuffer getParticleBuffer() const { return particleBuffer; }
        uint32_t getParticleCount() const { return maxParticleCount; }
        
        // Generic compute shader execution
        bool createComputePipeline(const std::string& shaderPath, VkPipeline& pipeline, 
                                  VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout);
        
        void dispatch(VkPipeline pipeline, VkPipelineLayout pipelineLayout, 
                     VkDescriptorSet descriptorSet, uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1);

    private:
        const AzVulk::Device& vulkanDevice;
        VkCommandPool commandPool;
        
        // Particle system specific
        VkBuffer particleBuffer = VK_NULL_HANDLE;
        VkDeviceMemory particleBufferMemory = VK_NULL_HANDLE;
        VkBuffer uniformBuffer = VK_NULL_HANDLE;
        VkDeviceMemory uniformBufferMemory = VK_NULL_HANDLE;
        void* uniformBufferMapped = nullptr;
        
        VkPipeline particleComputePipeline = VK_NULL_HANDLE;
        VkPipelineLayout particleComputePipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout particleDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool particleDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet particleDescriptorSet = VK_NULL_HANDLE;
        
        uint32_t maxParticleCount = 0;
        std::vector<ComputeParticle> particleData;
        
        // Command buffer for compute operations
        VkCommandBuffer computeCommandBuffer = VK_NULL_HANDLE;
        
        // Helper functions
        std::vector<char> readShaderFile(const std::string& filename);
        VkShaderModule createShaderModule(const std::vector<char>& code);
        void createBuffers(uint32_t particleCount);
        void createDescriptors();
        void cleanup();
    };

} // namespace AzCore
