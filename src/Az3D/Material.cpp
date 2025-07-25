#include "Az3D/Material.hpp"

namespace Az3D {
    
    size_t MaterialManager::addMaterial(Material* material) {
        if (!material) return 0; // Default material index

        materials.push_back(std::shared_ptr<Material>(material));
        return materials.size() - 1;
    }

} // namespace Az3D
