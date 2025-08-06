#include "Az3D/Material.hpp"
#include <iostream>

namespace Az3D {
    
    size_t MaterialManager::addMaterial(const Material& material) {
        materials.push_back(std::make_shared<Material>(material));

        return count++;
    }

} // namespace Az3D
