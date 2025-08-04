#include "Az3D/RenderSystem.hpp"
#include "Az3D/ResourceManager.hpp"
#include <vulkan/vulkan.h>

namespace Az3D {

    size_t RenderSystem::addModelResource(size_t meshIndex, size_t materialIndex) {
        modelResources.emplace_back(ModelResource{meshIndex, materialIndex});
        return modelResources.size() - 1;
    }

    size_t RenderSystem::addModelResource(const char* name, size_t meshIndex, size_t materialIndex) {
        size_t index = addModelResource(meshIndex, materialIndex);
        modelResourceNameToIndex[name] = index;
        return index;
    }

    size_t RenderSystem::getModelResource(const char* name) const {
        auto it = modelResourceNameToIndex.find(name);
        return it != modelResourceNameToIndex.end() ? it->second : SIZE_MAX; // SIZE_MAX indicates not found
    }

    void RenderSystem::clearInstances() {
        modelInstances.clear();
    }

    void RenderSystem::addInstance(const ModelInstance& instance) {
        modelInstances.push_back(instance);
    }

    void RenderSystem::addInstances(const std::vector<ModelInstance>& instances) {
        for (const auto& instance : instances) {
            modelInstances.push_back(instance);
        }
    }

    std::unordered_map<size_t, std::vector<const ModelInstance*>> RenderSystem::groupInstancesByMesh() const {
        std::unordered_map<size_t, std::vector<const ModelInstance*>> meshToInstances;
        
        for (const auto& instance : modelInstances) {
            const auto& resource = modelResources[instance.modelResourceIndex];
            meshToInstances[resource.meshIndex].push_back(&instance);
        }
        
        return meshToInstances;
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

    std::unordered_map<size_t, std::vector<const ModelInstance*>> RenderSystem::groupOpaqueInstancesByMesh() const {
        std::unordered_map<size_t, std::vector<const ModelInstance*>> meshToInstances;
        
        for (const auto& instance : modelInstances) {
            if (!isInstanceTransparent(instance)) {
                const auto& resource = modelResources[instance.modelResourceIndex];
                meshToInstances[resource.meshIndex].push_back(&instance);
            }
        }
        
        return meshToInstances;
    }

    std::unordered_map<size_t, std::vector<const ModelInstance*>> RenderSystem::groupTransparentInstancesByMesh() const {
        std::unordered_map<size_t, std::vector<const ModelInstance*>> meshToInstances;
        
        for (const auto& instance : modelInstances) {
            if (isInstanceTransparent(instance)) {
                const auto& resource = modelResources[instance.modelResourceIndex];
                meshToInstances[resource.meshIndex].push_back(&instance);
            }
        }
        
        return meshToInstances;
    }

    bool RenderSystem::isInstanceTransparent(const ModelInstance& instance) const {
        // Check instance alpha first
        if (instance.multColor().w < 1.0f) {
            return true;
        }

        // Check if the material's texture has transparency
        if (resourceManager != nullptr) {
            const auto& resource = modelResources[instance.modelResourceIndex];
            const auto& material = *resourceManager->materialManager->materials[resource.materialIndex];
            
            if (material.diffTxtr > 0) {
                const auto& texture = resourceManager->textureManager->textures[material.diffTxtr];
                if (texture.semiTransparent) { return true; }
            }
        }

        return false; // Fully opaque
    }

    void RenderSystem::setResourceManager(const ResourceManager* resManager) {
        resourceManager = resManager;
    }

}
