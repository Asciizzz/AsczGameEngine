#pragma once

#include "Az3D/Material.hpp"
#include <memory>
#include <vector>

namespace Az3D {
    
    // MaterialManager - manages materials using index-based access
    class MaterialManager {
    public:
        MaterialManager() = default;
        ~MaterialManager() = default;

        // Delete copy constructor and assignment operator
        MaterialManager(const MaterialManager&) = delete;
        MaterialManager& operator=(const MaterialManager&) = delete;

        // Index-based material management
        size_t addMaterial(std::shared_ptr<Material> material);
        bool removeMaterial(size_t index);
        bool hasMaterial(size_t index) const;
        Material* getMaterial(size_t index) const;
        
        // Create material and return index
        size_t createMaterial(const std::string& materialName = "");
        
        // Get default material (always at index 0)
        Material* getDefaultMaterial() const;
        size_t getDefaultMaterialIndex() const { return 0; }
        
        // Direct access to all materials
        const std::vector<std::shared_ptr<Material>>& getAllMaterials() const { return materials; }
        
        // Statistics
        size_t getMaterialCount() const { return materials.size(); }

        // Material storage - index-based
        std::vector<std::shared_ptr<Material>> materials;
        
        // Initialize default material
        void createDefaultMaterial();
    };
    
} // namespace Az3D
