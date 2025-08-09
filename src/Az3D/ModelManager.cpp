#include "Az3D/ModelManager.hpp"
#include <vulkan/vulkan.h>

namespace Az3D {

    size_t ModelManager::addModelResource(size_t meshIndex, size_t materialIndex) {
        modelResources.emplace_back(ModelResource{meshIndex, materialIndex});
        return modelResources.size() - 1;
    }

    size_t ModelManager::addModelResource(std::string name, size_t meshIndex, size_t materialIndex) {
        size_t index = addModelResource(meshIndex, materialIndex);
        modelResourceNameToIndex[name] = index;
        return index;
    }

    size_t ModelManager::getModelResourceIndex(std::string name) const {
        auto it = modelResourceNameToIndex.find(name);
        return it != modelResourceNameToIndex.end() ? it->second : SIZE_MAX;
    }

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
        meshIndexToModelInstances.clear();
        meshIndexToModelInstances.rehash(0);
        modelInstanceCount = 0;
    }

    void ModelGroup::addInstance(const ModelInstance& instance) {
        modelInstances.push_back(instance);
        modelInstanceCount++;

        const auto& resource = modelResources[instance.modelResourceIndex];

        size_t meshIndex = resource.meshIndex;
        meshIndexToModelInstances[meshIndex].push_back(&instance);
    }

    void ModelGroup::addInstances(const std::vector<ModelInstance>& instances) {
        this->modelInstances.reserve(this->modelInstances.size() + instances.size());
        this->modelInstances.insert(this->modelInstances.end(), instances.begin(), instances.end());

        modelInstanceCount += instances.size();

        for (const auto& instance : instances) {
            if (instance.modelResourceIndex >= modelResourceCount) continue;

            const auto& resource = modelResources[instance.modelResourceIndex];
            size_t meshIndex = resource.meshIndex;

            meshIndexToModelInstances[meshIndex].push_back(&instance);
        }
    }

    void ModelGroup::copyFrom(const ModelGroup& other) {
        modelResourceCount = other.modelResourceCount;
        modelResources = other.modelResources;
        modelResourceNameToIndex = other.modelResourceNameToIndex;

        modelInstanceCount = other.modelInstanceCount;
        modelInstances = other.modelInstances;
        meshIndexToModelInstances = other.meshIndexToModelInstances;
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
