#pragma once

// Soon to be deprecated - use tinyMaterial instead

#include <string>

#include <glm/glm.hpp>
#include "tinyData/tinyMaterial.hpp"

struct tinyRMaterial {
    std::string name; // Material name from source data
    glm::uvec4 texIndices = glm::uvec4(0); // Albedo, Normal, Reserved, Reserved (remapped registry indices)

    // Constructor from tinyMaterial - converts local texture indices to registry indices
    tinyRMaterial() noexcept = default;
    tinyRMaterial(const tinyMaterial& material) : name(material.name) {
        // Note: texture indices will be remapped during scene loading
        // material.localAlbTexture and material.localNrmlTexture are converted to registry indices
    }

    void setAlbTexIndex(uint32_t index) { texIndices.x = index; }
    void setNrmlTexIndex(uint32_t index) { texIndices.y = index; }
};
