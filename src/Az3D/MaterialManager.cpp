#include "Az3D/MaterialManager.hpp"
#include <iostream>

namespace Az3D {
    
    size_t MaterialManager::addMaterial(std::shared_ptr<Material> material) {
        if (!material) {
            std::cerr << "Cannot add null material" << std::endl;
            return SIZE_MAX; // Invalid index
        }
        
        materials.push_back(material);
        return materials.size() - 1;
    }

    bool MaterialManager::removeMaterial(size_t index) {
        if (index >= materials.size()) {
            return false;
        }
        
        // Mark as deleted (don't shrink vector to preserve indices)
        materials[index] = nullptr;
        return true;
    }

    bool MaterialManager::hasMaterial(size_t index) const {
        return index < materials.size() && materials[index] != nullptr;
    }

    Material* MaterialManager::getMaterial(size_t index) const {
        if (index < materials.size() && materials[index]) {
            return materials[index].get();
        }
        
        return getDefaultMaterial();
    }

    size_t MaterialManager::createMaterial(const std::string& materialName) {
        std::string name = materialName.empty() ? "Material_" + std::to_string(materials.size()) : materialName;
        auto material = std::make_shared<Material>(name);
        
        // Ensure default material exists at index 0
        if (materials.empty()) {
            createDefaultMaterial();
        }
        
        return addMaterial(material);
    }

    Material* MaterialManager::getDefaultMaterial() const {
        if (materials.empty() || !materials[0]) {
            const_cast<MaterialManager*>(this)->createDefaultMaterial();
        }
        return materials[0].get();
    }

    void MaterialManager::createDefaultMaterial() {
        auto defaultMat = std::make_shared<Material>("DefaultMaterial");
        
        // Set default material properties
        defaultMat->getProperties().albedoColor = glm::vec3(0.8f, 0.8f, 0.8f);
        defaultMat->getProperties().roughness = 0.5f;
        defaultMat->getProperties().metallic = 0.0f;
        defaultMat->getProperties().specular = 0.5f;
        
        if (materials.empty()) {
            materials.push_back(defaultMat);
        } else {
            materials[0] = defaultMat;
        }
    }
    
} // namespace Az3D
