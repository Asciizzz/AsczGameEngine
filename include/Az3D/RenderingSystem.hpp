#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Az3D {
    
    // Immutable data shared by many instances
    struct ModelResource {
        size_t meshIndex;
        size_t materialIndex;
    };
    
    // Dynamic, per-frame object data
    struct ModelInstance {
        glm::mat4 modelMatrix;
        size_t modelResourceIndex; // Index into modelResources
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
        void addInstance(const glm::mat4& modelMatrix, size_t modelResourceIndex);
        void addInstances(const std::vector<glm::mat4>& modelMatrices, size_t modelResourceIndex);
        
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
