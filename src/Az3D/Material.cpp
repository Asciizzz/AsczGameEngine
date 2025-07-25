#include "Az3D/Material.hpp"

namespace Az3D {
    
    size_t MaterialManager::addMaterial(const Material& material) {
        materials.push_back(std::make_shared<Material>(material));
        return materials.size() - 1;
    }

} // namespace Az3D
