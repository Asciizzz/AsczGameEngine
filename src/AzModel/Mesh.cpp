#include "AzModel/Mesh.hpp"
#include "AzModel/Material.hpp"
#include "AzGame/VulkanDevice.hpp"
#include <stdexcept>

namespace AzModel {
    Mesh::Mesh(const AzGame::VulkanDevice& device) {
        buffer = std::make_unique<AzGame::Buffer>(device);
    }

    Mesh::~Mesh() = default;

    void Mesh::setGeometry(const std::vector<AzGame::Vertex>& vertices, 
                          const std::vector<uint16_t>& indices) {
        if (vertices.empty() || indices.empty()) {
            throw std::invalid_argument("Vertices and indices cannot be empty");
        }

        buffer->createVertexBuffer(vertices);
        buffer->createIndexBuffer(indices);
        geometryLoaded = true;
    }

    uint32_t Mesh::addMaterial(std::shared_ptr<Material> material) {
        if (!material) {
            throw std::invalid_argument("Material cannot be null");
        }

        uint32_t materialId = nextMaterialId++;
        materials[materialId] = material;
        return materialId;
    }

    void Mesh::assignMaterial(uint32_t startIndex, uint32_t indexCount, uint32_t materialId) {
        // Validate that the material exists
        if (materials.find(materialId) == materials.end()) {
            throw std::invalid_argument("Material ID not found in this mesh");
        }

        // Validate index range
        if (!geometryLoaded) {
            throw std::runtime_error("Geometry must be loaded before assigning materials");
        }

        if (startIndex + indexCount > buffer->getIndexCount()) {
            throw std::invalid_argument("Index range exceeds buffer size");
        }

        // Add the face material assignment
        FaceMaterial faceMaterial;
        faceMaterial.startIndex = startIndex;
        faceMaterial.indexCount = indexCount;
        faceMaterial.materialId = materialId;

        faceMaterials.push_back(faceMaterial);
    }

    std::shared_ptr<Material> Mesh::getMaterial(uint32_t materialId) const {
        auto it = materials.find(materialId);
        if (it != materials.end()) {
            return it->second;
        }
        return nullptr;
    }
}
