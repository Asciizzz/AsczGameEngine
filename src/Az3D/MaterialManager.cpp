#include "Az3D/MaterialManager.hpp"
#include <iostream>

namespace Az3D {
    
    bool MaterialManager::addMaterial(const std::string& materialId, std::shared_ptr<Material> material) {
        if (!material) {
            std::cerr << "Cannot add null material with ID '" << materialId << "'" << std::endl;
            return false;
        }
        
        materials[materialId] = material;
        return true;
    }

    bool MaterialManager::removeMaterial(const std::string& materialId) {
        auto it = materials.find(materialId);
        if (it != materials.end()) {
            materials.erase(it);
            return true;
        }
        return false;
    }

    bool MaterialManager::hasMaterial(const std::string& materialId) const {
        return materials.find(materialId) != materials.end();
    }

    Material* MaterialManager::getMaterial(const std::string& materialId) const {
        auto it = materials.find(materialId);
        if (it != materials.end()) {
            return it->second.get();
        }
        
        return getDefaultMaterial();
    }

    Material* MaterialManager::createMaterial(const std::string& materialId, const std::string& materialName) {
        std::string name = materialName.empty() ? materialId : materialName;
        auto material = std::make_shared<Material>(name);
        
        if (addMaterial(materialId, material)) {
            return material.get();
        }
        return nullptr;
    }

    Material* MaterialManager::getDefaultMaterial() const {
        if (!defaultMaterial) {
            const_cast<MaterialManager*>(this)->createDefaultMaterial();
        }
        return defaultMaterial.get();
    }

    std::vector<std::string> MaterialManager::getMaterialIds() const {
        std::vector<std::string> ids;
        for (const auto& pair : materials) {
            ids.push_back(pair.first);
        }
        return ids;
    }

    void MaterialManager::createDefaultMaterial() {
        defaultMaterial = std::make_shared<Material>("DefaultMaterial");
        
        // Set default material properties
        defaultMaterial->getProperties().albedoColor = glm::vec3(0.8f, 0.8f, 0.8f);
        defaultMaterial->getProperties().roughness = 0.5f;
        defaultMaterial->getProperties().metallic = 0.0f;
        defaultMaterial->getProperties().specular = 0.5f;
    }
    
} // namespace Az3D
