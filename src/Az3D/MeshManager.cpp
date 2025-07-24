#include "Az3D/MeshManager.hpp"
#include <iostream>

namespace Az3D {
    
    bool MeshManager::addMesh(const std::string& meshId, std::shared_ptr<Mesh> mesh) {
        if (!mesh) {
            std::cerr << "Cannot add null mesh with ID '" << meshId << "'" << std::endl;
            return false;
        }
        
        meshes[meshId] = mesh;
        return true;
    }

    bool MeshManager::removeMesh(const std::string& meshId) {
        auto it = meshes.find(meshId);
        if (it != meshes.end()) {
            meshes.erase(it);
            return true;
        }
        return false;
    }

    bool MeshManager::hasMesh(const std::string& meshId) const {
        return meshes.find(meshId) != meshes.end();
    }

    Mesh* MeshManager::getMesh(const std::string& meshId) const {
        auto it = meshes.find(meshId);
        if (it != meshes.end()) {
            return it->second.get();
        }
        
        std::cerr << "Error: Mesh '" << meshId << "' not found!" << std::endl;
        return nullptr;
    }

    Mesh* MeshManager::loadMeshFromOBJ(const std::string& meshId, const std::string& filePath) {
        try {
            auto mesh = Mesh::loadFromOBJ(filePath);
            if (mesh && !mesh->isEmpty()) {
                if (addMesh(meshId, mesh)) {
                    return mesh.get();
                }
            } else {
                std::cerr << "Failed to load mesh from '" << filePath << "' - mesh is empty or invalid" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to load mesh '" << meshId << "' from '" << filePath << "': " << e.what() << std::endl;
        }
        return nullptr;
    }

    Mesh* MeshManager::createQuadMesh(const std::string& meshId) {
        // Create a simple quad mesh (2 triangles)
        std::vector<Vertex> vertices = {
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // Bottom-left
            {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // Bottom-right
            {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // Top-right
            {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}  // Top-left
        };
        
        std::vector<uint32_t> indices = {
            0, 1, 2,  // First triangle
            2, 3, 0   // Second triangle
        };
        
        auto mesh = std::make_shared<Mesh>(std::move(vertices), std::move(indices));
        if (addMesh(meshId, mesh)) {
            return mesh.get();
        }
        return nullptr;
    }

    Mesh* MeshManager::createCubeMesh(const std::string& meshId) {
        // Create a simple cube mesh
        std::vector<Vertex> vertices = {
            // Front face
            {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
            {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            
            // Back face
            {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
            {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
            {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
            {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        };
        
        std::vector<uint32_t> indices = {
            // Front face
            0, 1, 2,  2, 3, 0,
            // Back face
            4, 6, 5,  6, 4, 7,
            // Left face
            4, 0, 3,  3, 7, 4,
            // Right face
            1, 5, 6,  6, 2, 1,
            // Top face
            3, 2, 6,  6, 7, 3,
            // Bottom face
            4, 5, 1,  1, 0, 4
        };
        
        auto mesh = std::make_shared<Mesh>(std::move(vertices), std::move(indices));
        if (addMesh(meshId, mesh)) {
            return mesh.get();
        }
        return nullptr;
    }

    std::vector<std::string> MeshManager::getMeshIds() const {
        std::vector<std::string> ids;
        for (const auto& pair : meshes) {
            ids.push_back(pair.first);
        }
        return ids;
    }
    
} // namespace Az3D
