#include "Az3D/MeshManager.hpp"
#include <iostream>

namespace Az3D {
    
    size_t MeshManager::addMesh(std::shared_ptr<Mesh> mesh) {
        if (!mesh) {
            std::cerr << "Cannot add null mesh" << std::endl;
            return SIZE_MAX; // Invalid index
        }
        
        meshes.push_back(mesh);
        return meshes.size() - 1;
    }

    bool MeshManager::removeMesh(size_t index) {
        if (index >= meshes.size()) {
            return false;
        }
        
        // Mark as deleted (don't shrink vector to preserve indices)
        meshes[index] = nullptr;
        return true;
    }

    bool MeshManager::hasMesh(size_t index) const {
        return index < meshes.size() && meshes[index] != nullptr;
    }

    Mesh* MeshManager::getMesh(size_t index) const {
        if (index < meshes.size() && meshes[index]) {
            return meshes[index].get();
        }
        
        std::cerr << "Error: Mesh at index " << index << " not found!" << std::endl;
        return nullptr;
    }

    size_t MeshManager::loadMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
        auto mesh = std::make_shared<Mesh>(std::move(vertices), std::move(indices));
        return addMesh(mesh);
    }
    
    size_t MeshManager::loadMeshFromOBJ(const std::string& filePath) {
        try {
            auto mesh = Mesh::loadFromOBJ(filePath);
            if (mesh && !mesh->isEmpty()) {
                return addMesh(mesh);
            } else {
                std::cerr << "Failed to load mesh from '" << filePath << "' - mesh is empty or invalid" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to load mesh from '" << filePath << "': " << e.what() << std::endl;
        }
        return SIZE_MAX; // Invalid index
    }

    size_t MeshManager::createQuadMesh() {
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
        return addMesh(mesh);
    }

    size_t MeshManager::createCubeMesh() {
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
        return addMesh(mesh);
    }
    
} // namespace Az3D
