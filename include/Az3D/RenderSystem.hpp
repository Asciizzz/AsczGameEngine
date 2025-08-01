#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <vector>
#include <unordered_map>
#include <array>
#include <glm/glm.hpp>

// Forward declarations for Vulkan types
struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;

namespace Az3D {
    
    // Immutable data shared by many instances
    struct ModelResource {
        size_t meshIndex;
        size_t materialIndex;
    };
    
    // GPU vertex data structure - must be tightly packed for vertex attributes
    struct InstanceVertexData {
        glm::mat4 modelMatrix;     // 64 bytes - locations 3,4,5,6
        glm::vec4 multColor;       // 16 bytes - location 7
    };
    
    // Dynamic, per-frame object data
    struct ModelInstance {
        InstanceVertexData vertexData;
        size_t modelResourceIndex; // Index into modelResources
        
        // Default constructor
        ModelInstance() {
            vertexData.modelMatrix = glm::mat4(1.0f);
            vertexData.multColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            modelResourceIndex = 0;
        }
        
        // Convenience getters/setters
        glm::mat4& modelMatrix() { return vertexData.modelMatrix; }
        const glm::mat4& modelMatrix() const { return vertexData.modelMatrix; }
        glm::vec4& multColor() { return vertexData.multColor; }
        const glm::vec4& multColor() const { return vertexData.multColor; }
        
        // Vulkan-specific methods for vertex input
        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions();
    };

    // Global rendering system that manages all model resources and instances
    class RenderSystem {
    public:
        RenderSystem() = default;
        ~RenderSystem() = default;

        // Resource management
        size_t addModelResource(size_t meshIndex, size_t materialIndex);
        const ModelResource& getModelResource(size_t index) const;
        
        // Instance management
        void clearInstances();
        void addInstance(const ModelInstance& instance);
        void addInstances(const std::vector<ModelInstance>& instances);

        // Batch processing for rendering
        std::unordered_map<size_t, std::vector<const ModelInstance*>> groupInstancesByMesh() const;
        
        // Getters
        const std::vector<ModelResource>& getModelResources() const { return modelResources; }
        const std::vector<ModelInstance>& getModelInstances() const { return modelInstances; }
        
    private:
        std::vector<ModelResource> modelResources;
        std::vector<ModelInstance> modelInstances;
    };

} // namespace Az3D
