#pragma once

#include "Az3D/Mesh.hpp"
#include <memory>
#include <vector>

namespace Az3D {
    
    // MeshManager - manages meshes using index-based access
    class MeshManager {
    public:
        MeshManager() = default;
        ~MeshManager() = default;

        // Delete copy constructor and assignment operator
        MeshManager(const MeshManager&) = delete;
        MeshManager& operator=(const MeshManager&) = delete;

        // Index-based mesh management
        size_t addMesh(std::shared_ptr<Mesh> mesh);
        bool removeMesh(size_t index);
        bool hasMesh(size_t index) const;
        Mesh* getMesh(size_t index) const;
        
        // Load mesh and return index
        size_t loadMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
        size_t loadMeshFromOBJ(const std::string& filePath);
        
        // Create simple meshes and return index
        size_t createQuadMesh();
        size_t createCubeMesh();
        
        // Direct access to all meshes
        const std::vector<std::shared_ptr<Mesh>>& getAllMeshes() const { return meshes; }
        
        // Statistics
        size_t getMeshCount() const { return meshes.size(); }

        // Index-based mesh storage
        std::vector<std::shared_ptr<Mesh>> meshes;
    };
    
} // namespace Az3D
