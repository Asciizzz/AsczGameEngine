#include "Az3D/RenderingSystem.hpp"

namespace Az3D {

    size_t RenderSystem::addModelResource(size_t meshIndex, size_t materialIndex) {
        modelResources.emplace_back(ModelResource{meshIndex, materialIndex});
        return modelResources.size() - 1;
    }

    const ModelResource& RenderSystem::getModelResource(size_t index) const {
        return modelResources[index];
    }

    void RenderSystem::clearInstances() {
        modelInstances.clear();
    }

    void RenderSystem::addInstance(const glm::mat4& modelMatrix, size_t modelResourceIndex) {
        modelInstances.emplace_back(ModelInstance{modelMatrix, modelResourceIndex});
    }

    void RenderSystem::addInstances(const std::vector<glm::mat4>& modelMatrices, size_t modelResourceIndex) {
        for (const auto& matrix : modelMatrices) {
            addInstance(matrix, modelResourceIndex);
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

}
