#include "Az3D/ModelManager.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>

namespace Az3D {

// Model Group

    size_t ModelGroup::addModelResource(const std::string& name, size_t meshIndex, size_t materialIndex) {
        modelResources.emplace_back(ModelResource{meshIndex, materialIndex});
        modelResourceNameToIndex[name] = modelResourceCount;
        return modelResourceCount++;
    }

    size_t ModelGroup::getModelResourceIndex(const std::string& name) const {
        auto it = modelResourceNameToIndex.find(name);
        return it != modelResourceNameToIndex.end() ? it->second : SIZE_MAX;
    }

    void ModelGroup::clearInstances() {
        modelInstances.clear();
        meshMapping.clear();
        modelInstanceCount = 0;
    }

    void ModelGroup::addInstance(const ModelInstance& instance) {
        size_t newInstanceIndex = modelInstances.size();
        modelInstances.push_back(instance);
        auto& addedInstance = modelInstances.back();
        
        // Set up mesh mapping indices
        const auto& resource = modelResources[instance.modelResourceIndex];
        addedInstance.meshIndex = resource.meshIndex;
        addedInstance.instanceIndex = newInstanceIndex;
        
        // Add instance index to mesh mapping
        meshMapping.toInstanceIndices[addedInstance.meshIndex].push_back(newInstanceIndex);
        
        modelInstanceCount++;
    }

    void ModelGroup::addInstances(const std::vector<ModelInstance>& instances) {
        size_t startIndex = modelInstances.size();
        this->modelInstances.reserve(startIndex + instances.size());
        this->modelInstances.insert(this->modelInstances.end(), instances.begin(), instances.end());

        // Set up mesh mapping indices for all added instances
        for (size_t i = 0; i < instances.size(); ++i) {
            size_t instanceIndex = startIndex + i;
            auto& addedInstance = modelInstances[instanceIndex];
            const auto& resource = modelResources[instances[i].modelResourceIndex];
            
            addedInstance.meshIndex = resource.meshIndex;
            addedInstance.instanceIndex = instanceIndex;
            
            // Add instance index to mesh mapping
            meshMapping.toInstanceIndices[addedInstance.meshIndex].push_back(instanceIndex);
        }

        modelInstanceCount += instances.size();
    }

    void ModelGroup::buildMeshMapping() {
        // Clear the existing mapping
        meshMapping.toInstanceIndices.clear();
        meshMapping.toInstanceToBufferPos.clear();

        // Rebuild mapping with instance indices
        for (size_t i = 0; i < modelInstances.size(); ++i) {
            auto& instance = modelInstances[i];

            const auto& resource = modelResources[instance.modelResourceIndex];
            size_t meshIndex = resource.meshIndex;
            
            // Set the indices in the instance
            instance.meshIndex = meshIndex;
            instance.instanceIndex = i;
            
            // Add instance index to mesh mapping
            meshMapping.toInstanceIndices[meshIndex].push_back(i);
        }
        
        // Build buffer position mapping for efficient lookups
        for (const auto& [meshIndex, instanceIndices] : meshMapping.toInstanceIndices) {
            auto& bufferPosMap = meshMapping.toInstanceToBufferPos[meshIndex];
            for (size_t bufferPos = 0; bufferPos < instanceIndices.size(); ++bufferPos) {
                size_t instanceIndex = instanceIndices[bufferPos];
                bufferPosMap[instanceIndex] = bufferPos;
            }
        }
    }

    void ModelGroup::queueUpdate(size_t instanceIndex) {
        size_t meshIndex = modelInstances[instanceIndex].meshIndex;
        meshMapping.toUpdateIndices[meshIndex].push_back(instanceIndex);
    }
    
    void ModelGroup::queueUpdate(const ModelInstance& instance) {
        queueUpdate(instance.instanceIndex);
    }
    
    void ModelGroup::queueUpdates(const std::vector<ModelInstance>& instances) {
        for (const auto& instance : instances) queueUpdate(instance);
    }

    void ModelGroup::clearUpdateQueue() {
        meshMapping.toUpdateIndices.clear();
    }

    bool ModelGroup::hasSequentialInstances(size_t meshIndex) const {
        auto it = meshMapping.toInstanceIndices.find(meshIndex);
        if (it == meshMapping.toInstanceIndices.end()) return false;
        
        const auto& instanceIndices = it->second;
        if (instanceIndices.empty()) return false;
        
        // Check if instances are sequential: [0, 1, 2, 3, ...] or [n, n+1, n+2, ...]
        for (size_t i = 1; i < instanceIndices.size(); ++i) {
            if (instanceIndices[i] != instanceIndices[i-1] + 1) {
                return false;
            }
        }
        return true;
    }

    bool ModelGroup::hasUpdates() const {
        for (const auto& [meshIndex, updateIndices] : meshMapping.toUpdateIndices) {
            if (!updateIndices.empty()) return true;
        }
        return false;
    }

    void ModelGroup::copyFrom(const ModelGroup& other) {
        modelResourceCount = other.modelResourceCount;
        modelResources = other.modelResources;
        modelResourceNameToIndex = other.modelResourceNameToIndex;

        modelInstanceCount = other.modelInstanceCount;
        modelInstances = other.modelInstances;
        meshMapping = other.meshMapping;
    }


// Ease of use function straight from model manager

    void ModelManager::addGroup(const std::string& groupName) {
        groups[groupName] = ModelGroup{};
        groupCount++;
    }
    void ModelManager::addGroups(const std::vector<std::string>& groupNames) {
        for (const auto& groupName : groupNames) {
            groups[groupName] = ModelGroup{groupName};
        }
        groupCount += groupNames.size();
    }
    void ModelManager::addGroup(const std::string& groupName, const std::vector<ModelInstance>& instances) {
        groups[groupName] = ModelGroup{};
        groups[groupName].addInstances(instances);
        groupCount++;
    }
    void ModelManager::addGroup(const std::string& groupName, const ModelGroup& group) {
        groups[groupName] = group;
        groupCount++;
    }

    void ModelManager::clearAllInstances() {
        for (auto& group : groups) {
            group.second.clearInstances();
        }
    }
    void ModelManager::clearInstances(const std::string& groupName) {
        groups[groupName].clearInstances();
    }

    void ModelManager::addInstance(const std::string& groupName, const ModelInstance& instance) {
        auto& group = groups[groupName];
        group.addInstance(instance);
    }
    void ModelManager::addInstances(const std::string& groupName, const std::vector<ModelInstance>& instances) {
        auto& group = groups[groupName];
        group.addInstances(instances);
    }

    void ModelManager::deleteGroup(const std::string& groupName) {
        groups.erase(groupName);
        groupCount--;
    }
    void ModelManager::deleteAllGroups() {
        groups.clear();
        groupCount = 0;
    }




    // Vulkan-specific methods for ModelInstance
    VkVertexInputBindingDescription ModelInstance::getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 1; // Binding 1 for instance data
        bindingDescription.stride = sizeof(InstanceVertexData); // Only GPU data
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        return bindingDescription;
    }

    std::array<VkVertexInputAttributeDescription, 5> ModelInstance::getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

        // Model matrix is 4x4, so we need 4 attribute locations (3, 4, 5, 6)
        // Each vec4 takes one attribute location
        for (int i = 0; i < 4; i++) {
            attributeDescriptions[i].binding = 1;
            attributeDescriptions[i].location = 3 + i; // Locations 3, 4, 5, 6
            attributeDescriptions[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[i].offset = offsetof(InstanceVertexData, modelMatrix) + sizeof(glm::vec4) * i;
        }

        // Instance color multiplier vec4 (location 7) - directly after modelMatrix in InstanceVertexData
        attributeDescriptions[4].binding = 1;
        attributeDescriptions[4].location = 7;
        attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[4].offset = offsetof(InstanceVertexData, multColor);

        return attributeDescriptions;
    }

}
