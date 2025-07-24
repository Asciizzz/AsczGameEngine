#pragma once

#include "Az3D/Mesh.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace Az3D {
    
    // MeshManager - manages all meshes in the Az3D system
    class MeshManager {
    public:
        MeshManager() = default;
        ~MeshManager() = default;

        // Delete copy constructor and assignment operator
        MeshManager(const MeshManager&) = delete;
        MeshManager& operator=(const MeshManager&) = delete;

        // Mesh management
        bool addMesh(const std::string& meshId, std::shared_ptr<Mesh> mesh);
        bool removeMesh(const std::string& meshId);
        bool hasMesh(const std::string& meshId) const;
        Mesh* getMesh(const std::string& meshId) const;
        
        // Load mesh
        Mesh* loadMesh(const std::string& meshId, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
        Mesh* loadMeshFromOBJ(const std::string& meshId, const std::string& filePath);
        

        // Create simple meshes
        Mesh* createQuadMesh(const std::string& meshId);
        Mesh* createCubeMesh(const std::string& meshId);
        
        // Statistics
        size_t getMeshCount() const { return meshes.size(); }
        std::vector<std::string> getMeshIds() const;

    private:
        // Mesh storage
        std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes;
    };
    
} // namespace Az3D
