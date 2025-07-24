#pragma once

#include "Az3D/Material.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace Az3D {
    
    // MaterialManager - manages all materials in the Az3D system
    class MaterialManager {
    public:
        MaterialManager() = default;
        ~MaterialManager() = default;

        // Delete copy constructor and assignment operator
        MaterialManager(const MaterialManager&) = delete;
        MaterialManager& operator=(const MaterialManager&) = delete;

        // Material management
        bool addMaterial(const std::string& materialId, std::shared_ptr<Material> material);
        bool removeMaterial(const std::string& materialId);
        bool hasMaterial(const std::string& materialId) const;
        Material* getMaterial(const std::string& materialId) const;
        
        // Create material with ID
        Material* createMaterial(const std::string& materialId, const std::string& materialName = "");
        
        // Get default material for fallback
        Material* getDefaultMaterial() const;
        
        // Statistics
        size_t getMaterialCount() const { return materials.size(); }
        std::vector<std::string> getMaterialIds() const;

    private:
        // Material storage
        std::unordered_map<std::string, std::shared_ptr<Material>> materials;
        std::shared_ptr<Material> defaultMaterial;
        
        // Initialize default material
        void createDefaultMaterial();
    };
    
} // namespace Az3D
