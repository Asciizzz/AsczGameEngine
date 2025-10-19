#pragma once

// Soon to be deprecated - use TinyMaterial instead

#include <string>

#include <glm/glm.hpp>
#include "TinyData/TinyMaterial.hpp"

struct TinyRMaterial {
    std::string name; // Material name from source data
    glm::uvec4 texIndices = glm::uvec4(0); // Albedo, Normal, Reserved, Reserved (remapped registry indices)

    // Constructor from TinyMaterial - converts local texture indices to registry indices
    TinyRMaterial() = default;
    TinyRMaterial(const TinyMaterial& material) : name(material.name) {
        // Note: texture indices will be remapped during scene loading
        // material.localAlbTexture and material.localNrmlTexture are converted to registry indices
    }

    void setAlbTexIndex(uint32_t index) { texIndices.x = index; }
    void setNrmlTexIndex(uint32_t index) { texIndices.y = index; }
};
