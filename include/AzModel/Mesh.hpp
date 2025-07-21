#pragma once

#include "AzGame/Buffer.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

namespace AzModel {
    class Material;

    /**
     * @brief Face material assignment structure
     * 
     * This structure maps ranges of indices to specific materials,
     * allowing different parts of the mesh to use different textures.
     */
    struct FaceMaterial {
        uint32_t startIndex;    // Starting index in the index buffer
        uint32_t indexCount;    // Number of indices for this material
        uint32_t materialId;    // ID of the material to use
    };

    /**
     * @brief 3D Mesh class that contains vertices, indices, and material assignments
     * 
     * This class represents a complete 3D mesh with its geometry data and
     * material assignments. It can handle multiple materials per mesh by
     * mapping index ranges to specific materials.
     */
    class Mesh {
    public:
        Mesh(const AzGame::VulkanDevice& device);
        ~Mesh();

        // Delete copy constructor and assignment operator
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        /**
         * @brief Set the mesh geometry data
         * @param vertices Vector of vertex data
         * @param indices Vector of index data
         */
        void setGeometry(const std::vector<AzGame::Vertex>& vertices, 
                        const std::vector<uint16_t>& indices);

        /**
         * @brief Add a material to this mesh
         * @param material Shared pointer to the material
         * @return Material ID that can be used in face assignments
         */
        uint32_t addMaterial(std::shared_ptr<Material> material);

        /**
         * @brief Assign a material to a range of indices (faces)
         * @param startIndex Starting index in the index buffer
         * @param indexCount Number of indices to assign this material to
         * @param materialId ID of the material (from addMaterial)
         */
        void assignMaterial(uint32_t startIndex, uint32_t indexCount, uint32_t materialId);

        /**
         * @brief Get the vertex buffer for rendering
         * @return VkBuffer handle
         */
        VkBuffer getVertexBuffer() const { return buffer->getVertexBuffer(); }

        /**
         * @brief Get the index buffer for rendering
         * @return VkBuffer handle
         */
        VkBuffer getIndexBuffer() const { return buffer->getIndexBuffer(); }

        /**
         * @brief Get the total number of indices in this mesh
         * @return Total index count
         */
        uint32_t getTotalIndexCount() const { return buffer->getIndexCount(); }

        /**
         * @brief Get all face material assignments
         * @return Vector of face material assignments
         */
        const std::vector<FaceMaterial>& getFaceMaterials() const { return faceMaterials; }

        /**
         * @brief Get a material by its ID
         * @param materialId Material ID
         * @return Shared pointer to the material, or nullptr if not found
         */
        std::shared_ptr<Material> getMaterial(uint32_t materialId) const;

        /**
         * @brief Get all materials used by this mesh
         * @return Map of material ID to material pointer
         */
        const std::unordered_map<uint32_t, std::shared_ptr<Material>>& getMaterials() const { 
            return materials; 
        }

        /**
         * @brief Check if the mesh has valid geometry data
         * @return true if mesh has vertices and indices
         */
        bool isValid() const { return geometryLoaded; }

        /**
         * @brief Get the name/identifier of this mesh
         * @return Mesh name
         */
        const std::string& getName() const { return meshName; }

        /**
         * @brief Set the name/identifier of this mesh
         * @param name New mesh name
         */
        void setName(const std::string& name) { meshName = name; }

    private:
        std::unique_ptr<AzGame::Buffer> buffer;
        std::unordered_map<uint32_t, std::shared_ptr<Material>> materials;
        std::vector<FaceMaterial> faceMaterials;
        
        uint32_t nextMaterialId = 0;
        bool geometryLoaded = false;
        std::string meshName = "DefaultMesh";

        void createBuffers();
    };
}
