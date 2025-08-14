#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <string>

#include "AzVulk/Buffer.hpp"

// Forward declarations for Vulkan types
struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;

namespace Az3D {
    
    // Immutable data shared by many instances
    struct ModelResource {
        size_t meshIndex;
        size_t materialIndex;

        bool operator==(const ModelResource& other) const {
            return meshIndex == other.meshIndex && materialIndex == other.materialIndex;
        }
    };

    struct ModelResourceHash {
        std::size_t operator()(const ModelResource& res) const {
            return std::hash<size_t>{}(res.meshIndex) ^ (std::hash<size_t>{}(res.materialIndex) << 1);
        }
    };

    
    // Dynamic, per-frame object data
    struct ModelInstance {
        ModelInstance() = default;

        struct GPUData {
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            glm::vec4 multColor = glm::vec4(1.0f);
        } data;

        size_t modelResourceIndex = 0;

        // Dynamic mesh mapping indices for direct mesh map updates
        size_t meshIndex = SIZE_MAX;        // Which mesh this instance belongs to
        size_t instanceIndex = SIZE_MAX;    // This instance's index in the modelInstances array
        
        // Convenience getters/setters
        glm::mat4& modelMatrix() { return data.modelMatrix; }
        const glm::mat4& modelMatrix() const { return data.modelMatrix; }
        glm::vec4& multColor() { return data.multColor; }
        const glm::vec4& multColor() const { return data.multColor; }

        // Vulkan-specific methods for vertex input
        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions();
    };

    // TODO: Completely rework the entire MeshMappingData structure.
    // turn it into ModelMapping instead

    // Mesh mapping structure to organize instance data per mesh
    struct MeshMappingData {
        std::vector<size_t> instanceIndices;           // instance indices for this mesh
        std::vector<size_t> updateIndices;             // update queue indices for this mesh
        std::vector<bool> instanceActive;              // active state for each instance
        uint32_t prevInstanceCount = 0;                // previous instance count for this mesh
        UnorderedMap<size_t, size_t> instanceToBufferPos; // instance index -> buffer position mapping
        
        void clear() {
            instanceIndices.clear();
            updateIndices.clear();
            instanceActive.clear();
            prevInstanceCount = 0;
            instanceToBufferPos.clear();
        }
    };

    // Model group for separate renderer
    struct ModelGroup {
        ModelGroup() = default;
        ModelGroup(const std::string& name) : name(name) {}

        std::string name = "Default";

        size_t modelResourceCount = 0;
        std::vector<ModelResource> modelResources;
        UnorderedMap<std::string, size_t> modelResourceNameToIndex;

        size_t modelInstanceCount = 0;
        std::vector<ModelInstance> modelInstances;
        
        // Organized mesh mapping data - now per mesh index
        UnorderedMap<size_t, MeshMappingData> meshMapping;

        size_t addModelResource(const std::string& name, size_t meshIndex, size_t materialIndex);
        size_t getModelResourceIndex(const std::string& name) const;

        void clearInstances();
        void addInstance(const ModelInstance& instance);
        void addInstances(const std::vector<ModelInstance>& instances);

        void copyFrom(const ModelGroup& other);

        void buildMeshMapping();
        
        // Explicit update tracking system
        void queueUpdate(size_t instanceIndex);
        void queueUpdate(const ModelInstance& instance);
        void queueUpdates(const std::vector<ModelInstance>& instances);
        void clearUpdateQueue();
    };

    // Global model management system that manages all model resources and instances
    class ModelManager {
    public:
        ModelManager() = default;
        ~ModelManager() = default;

    // Model Grupo

        size_t groupCount = 0;
        UnorderedMap<std::string, ModelGroup> groups;

        ModelGroup& getGroup(const std::string& groupName) { return groups[groupName]; }

        void addGroup(const std::string& groupName);
        void addGroups(const std::vector<std::string>& groupNames);
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
