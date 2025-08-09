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
        
        // Dynamic mesh mapping indices for direct mesh map updates
        size_t meshIndex = SIZE_MAX;        // Which mesh this instance belongs to
        size_t instanceIndex = SIZE_MAX;    // This instance's index in the modelInstances array
        
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

    // Mesh mapping structure to organize the three vectors
    struct MeshMappingData {
        std::unordered_map<size_t, std::vector<size_t>> toInstanceIndices;     // mesh index -> instance indices
        std::unordered_map<size_t, std::vector<size_t>> toUpdateIndices;       // mesh index -> update queue indices  
        std::unordered_map<size_t, uint32_t> toPrevInstanceCount;              // mesh index -> previous instance count
        
        void clear() {
            toInstanceIndices.clear();
            toUpdateIndices.clear();
            toPrevInstanceCount.clear();
        }
    };

    // Model group for separate renderer
    struct ModelGroup {
        ModelGroup() = default;
        ModelGroup(const std::string& name) : name(name) {}

        std::string name = "Default";

        size_t modelResourceCount = 0;
        std::vector<ModelResource> modelResources;
        std::unordered_map<std::string, size_t> modelResourceNameToIndex;

        size_t modelInstanceCount = 0;
        std::vector<ModelInstance> modelInstances;
        
        // Organized mesh mapping data
        MeshMappingData meshMapping;

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

        bool hasUpdates() const; // Costly, avoid using this

        void printDebug() const {
            printf("ModelGroup '%s': %zu resources, %zu instances\n", name.c_str(), modelResourceCount, modelInstanceCount);
        }
    };

    // Global model management system that manages all model resources and instances
    class ModelManager {
    public:
        ModelManager() = default;
        ~ModelManager() = default;

    // Model Grupo

        size_t groupCount = 0;
        std::unordered_map<std::string, ModelGroup> groups;

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
