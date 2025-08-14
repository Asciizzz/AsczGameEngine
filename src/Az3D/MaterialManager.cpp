#include "Az3D/MaterialManager.hpp"

namespace Az3D {

    size_t MaterialManager::addMaterial(const Material& material) {
        materials.push_back(MakeShared<Material>(material));

        return count++;
    }

} // namespace Az3D
