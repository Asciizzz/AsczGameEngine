#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <array>
#include <vector>
#include <string>
#include <unordered_map>
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

    // Model group for separate renderer
    struct ModelGroup {
        ModelGroup() = default;

        size_t modelResourceCount = 0;
        std::vector<ModelResource> modelResources;
        std::unordered_map<std::string, size_t> modelResourceNameToIndex;

        size_t modelInstanceCount = 0;
        std::vector<ModelInstance> modelInstances;
        std::unordered_map<size_t, std::vector<const ModelInstance*>> meshIndexToModelInstances;

        size_t addModelResource(const std::string& name, size_t meshIndex, size_t materialIndex);
        size_t getModelResourceIndex(const std::string& name) const;

        void clearInstances();
        void addInstance(const ModelInstance& instance);
        void addInstances(const std::vector<ModelInstance>& instances);
    };

    // Global model management system that manages all model resources and instances
    class ModelManager {
    public:
        ModelManager() = default;
        ~ModelManager() = default;

    // Resource management
        // String-to-index map for model resources
        std::unordered_map<std::string, size_t> modelResourceNameToIndex;

        // Public data members
        std::vector<ModelResource> modelResources;
        size_t addModelResource(size_t meshIndex, size_t materialIndex);
        size_t addModelResource(std::string name, size_t meshIndex, size_t materialIndex); // with name mapping

        // String-to-index getter
        size_t getModelResourceIndex(std::string name) const;

    // Instances group
        size_t groupCount = 0;
        std::unordered_map<std::string, ModelGroup> groups;

        void addGroup(const std::string& groupName);
        void addGroup(const std::string& groupName, const std::vector<ModelInstance>& instances);
        void addGroup(const std::string& groupName, const ModelGroup& group);

        void clearAllInstances();
        void clearInstances(const std::string& groupName);

        void addInstance(const std::string& groupName, const ModelInstance& instance);
        void addInstances(const std::string& groupName, const std::vector<ModelInstance>& instances);

        void deleteGroup(const std::string& groupName);
        void deleteAllGroups();
    };

} // namespace Az3D
