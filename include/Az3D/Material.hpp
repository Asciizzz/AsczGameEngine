#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Az3D {

    struct Material {
        // Custom material properties exclusive to my game engine
        size_t diffTxtr = 0; // Albedo/Diffuse map
    };


    // MaterialManager - manages materials using index-based access
    class MaterialManager {
    public:
        MaterialManager() = default;
        MaterialManager(const MaterialManager&) = delete;
        MaterialManager& operator=(const MaterialManager&) = delete;

        size_t addMaterial(const Material& material);

        // Material storage - index-based
        std::vector<std::shared_ptr<Material>> materials = { 
            std::make_shared<Material>()
        };
    };
    
} // namespace Az3D
