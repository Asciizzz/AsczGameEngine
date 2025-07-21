#pragma once

#include "Mesh.hpp"
#include <vector>
#include <memory>
#include <string>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace AzModel {
    /**
     * @brief 3D Model class that contains multiple meshes and transformation data
     * 
     * This class represents a complete 3D model which can consist of multiple
     * meshes, each with their own materials. It also handles the model's
     * transformation matrix and provides utilities for creating common shapes.
     */
    class Model3D {
    public:
        Model3D();
        ~Model3D() = default;

        // Copy and move constructors/assignments are allowed for models
        Model3D(const Model3D&) = default;
        Model3D& operator=(const Model3D&) = default;
        Model3D(Model3D&&) = default;
        Model3D& operator=(Model3D&&) = default;

        /**
         * @brief Add a mesh to this model
         * @param mesh Shared pointer to the mesh
         */
        void addMesh(std::shared_ptr<Mesh> mesh);

        /**
         * @brief Get all meshes in this model
         * @return Vector of mesh pointers
         */
        const std::vector<std::shared_ptr<Mesh>>& getMeshes() const { return meshes; }

        /**
         * @brief Set the model's transformation matrix
         * @param transform New transformation matrix
         */
        void setTransform(const glm::mat4& transform) { modelMatrix = transform; }

        /**
         * @brief Get the model's transformation matrix
         * @return Current transformation matrix
         */
        const glm::mat4& getTransform() const { return modelMatrix; }

        /**
         * @brief Set the model's position
         * @param position New position vector
         */
        void setPosition(const glm::vec3& position);

        /**
         * @brief Set the model's rotation (in radians)
         * @param rotation New rotation vector (x, y, z rotations)
         */
        void setRotation(const glm::vec3& rotation);

        /**
         * @brief Set the model's scale
         * @param scale New scale vector
         */
        void setScale(const glm::vec3& scale);

        /**
         * @brief Get the model's position
         * @return Current position vector
         */
        const glm::vec3& getPosition() const { return position; }

        /**
         * @brief Get the model's rotation
         * @return Current rotation vector
         */
        const glm::vec3& getRotation() const { return rotation; }

        /**
         * @brief Get the model's scale
         * @return Current scale vector
         */
        const glm::vec3& getScale() const { return scale; }

        /**
         * @brief Update the transformation matrix based on position, rotation, and scale
         */
        void updateTransform();

        /**
         * @brief Get the name/identifier of this model
         * @return Model name
         */
        const std::string& getName() const { return modelName; }

        /**
         * @brief Set the name/identifier of this model
         * @param name New model name
         */
        void setName(const std::string& name) { modelName = name; }

        // Static factory methods for creating common shapes
        
        /**
         * @brief Create a cube model with optional texture
         * @param device Vulkan device reference
         * @param commandPool Command pool for texture operations
         * @param size Size of the cube (default 1.0)
         * @param texturePath Optional texture path (creates checkerboard if empty)
         * @return Shared pointer to the created cube model
         */
        static std::shared_ptr<Model3D> createCube( const AzGame::VulkanDevice& device, 
                                                    VkCommandPool commandPool,
                                                    float size = 1.0f, 
                                                    const std::string& texturePath = "");

        /**
         * @brief Create a plane model with optional texture
         * @param device Vulkan device reference
         * @param commandPool Command pool for texture operations
         * @param width Width of the plane (default 1.0)
         * @param height Height of the plane (default 1.0)
         * @param texturePath Optional texture path (creates checkerboard if empty)
         * @return Shared pointer to the created plane model
         */
        static std::shared_ptr<Model3D> createPlane(const AzGame::VulkanDevice& device, 
                                                    VkCommandPool commandPool,
                                                    float width = 1.0f, 
                                                    float height = 1.0f,
                                                    const std::string& texturePath = "");

        /**
         * @brief Create a quad model (2 triangles) with optional texture
         * @param device Vulkan device reference
         * @param commandPool Command pool for texture operations
         * @param size Size of the quad (default 1.0)
         * @param texturePath Optional texture path (creates checkerboard if empty)
         * @return Shared pointer to the created quad model
         */
        static std::shared_ptr<Model3D> createQuad( const AzGame::VulkanDevice& device, 
                                                    VkCommandPool commandPool,
                                                    float size = 1.0f,
                                                    const std::string& texturePath = "");

    private:
        std::vector<std::shared_ptr<Mesh>> meshes;
        glm::mat4 modelMatrix;
        
        // Transform components
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f}; // In radians
        glm::vec3 scale{1.0f};

        std::string modelName = "DefaultModel";
    };
}
