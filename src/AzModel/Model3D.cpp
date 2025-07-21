#include "AzModel/Model3D.hpp"
#include "AzModel/Material.hpp"
#include "AzGame/VulkanDevice.hpp"

namespace AzModel {
    Model3D::Model3D() : modelMatrix(1.0f) {
        updateTransform();
    }

    void Model3D::addMesh(std::shared_ptr<Mesh> mesh) {
        if (mesh) {
            meshes.push_back(mesh);
        }
    }

    void Model3D::setPosition(const glm::vec3& newPosition) {
        position = newPosition;
        updateTransform();
    }

    void Model3D::setRotation(const glm::vec3& newRotation) {
        rotation = newRotation;
        updateTransform();
    }

    void Model3D::setScale(const glm::vec3& newScale) {
        scale = newScale;
        updateTransform();
    }

    void Model3D::updateTransform() {
        modelMatrix = glm::mat4(1.0f);
        
        // Apply transformations in TRS order: Translation * Rotation * Scale
        modelMatrix = glm::translate(modelMatrix, position);
        
        // Apply rotations in order: Z, Y, X (yaw, pitch, roll)
        modelMatrix = glm::rotate(modelMatrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        modelMatrix = glm::rotate(modelMatrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        
        modelMatrix = glm::scale(modelMatrix, scale);
    }

    std::shared_ptr<Model3D> Model3D::createCube(const AzGame::VulkanDevice& device, 
                                                VkCommandPool commandPool,
                                                float size, 
                                                const std::string& texturePath) {
        auto model = std::make_shared<Model3D>();
        model->setName("Cube");

        auto mesh = std::make_shared<Mesh>(device);
        mesh->setName("CubeMesh");

        float halfSize = size * 0.5f;

        // Cube vertices (24 vertices for proper texture mapping on each face)
        std::vector<AzGame::Vertex> vertices = {
            // Front face
            {{-halfSize, -halfSize,  halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{ halfSize, -halfSize,  halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{ halfSize,  halfSize,  halfSize}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-halfSize,  halfSize,  halfSize}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

            // Back face
            {{ halfSize, -halfSize, -halfSize}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{-halfSize, -halfSize, -halfSize}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
            {{-halfSize,  halfSize, -halfSize}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}},
            {{ halfSize,  halfSize, -halfSize}, {0.8f, 0.3f, 0.2f}, {0.0f, 1.0f}},

            // Left face
            {{-halfSize, -halfSize, -halfSize}, {0.2f, 0.8f, 0.3f}, {0.0f, 0.0f}},
            {{-halfSize, -halfSize,  halfSize}, {0.3f, 0.2f, 0.8f}, {1.0f, 0.0f}},
            {{-halfSize,  halfSize,  halfSize}, {0.8f, 0.8f, 0.2f}, {1.0f, 1.0f}},
            {{-halfSize,  halfSize, -halfSize}, {0.2f, 0.8f, 0.8f}, {0.0f, 1.0f}},

            // Right face
            {{ halfSize, -halfSize,  halfSize}, {0.8f, 0.2f, 0.8f}, {0.0f, 0.0f}},
            {{ halfSize, -halfSize, -halfSize}, {0.4f, 0.6f, 0.2f}, {1.0f, 0.0f}},
            {{ halfSize,  halfSize, -halfSize}, {0.6f, 0.4f, 0.8f}, {1.0f, 1.0f}},
            {{ halfSize,  halfSize,  halfSize}, {0.2f, 0.6f, 0.4f}, {0.0f, 1.0f}},

            // Top face
            {{-halfSize,  halfSize,  halfSize}, {0.9f, 0.1f, 0.1f}, {0.0f, 0.0f}},
            {{ halfSize,  halfSize,  halfSize}, {0.1f, 0.9f, 0.1f}, {1.0f, 0.0f}},
            {{ halfSize,  halfSize, -halfSize}, {0.1f, 0.1f, 0.9f}, {1.0f, 1.0f}},
            {{-halfSize,  halfSize, -halfSize}, {0.9f, 0.9f, 0.1f}, {0.0f, 1.0f}},

            // Bottom face
            {{-halfSize, -halfSize, -halfSize}, {0.9f, 0.1f, 0.9f}, {0.0f, 0.0f}},
            {{ halfSize, -halfSize, -halfSize}, {0.1f, 0.9f, 0.9f}, {1.0f, 0.0f}},
            {{ halfSize, -halfSize,  halfSize}, {0.5f, 0.5f, 0.1f}, {1.0f, 1.0f}},
            {{-halfSize, -halfSize,  halfSize}, {0.1f, 0.5f, 0.5f}, {0.0f, 1.0f}}
        };

        // Cube indices (36 indices for 12 triangles, 2 per face)
        std::vector<uint16_t> indices = {
            // Front face
            0, 1, 2, 2, 3, 0,
            // Back face
            4, 5, 6, 6, 7, 4,
            // Left face
            8, 9, 10, 10, 11, 8,
            // Right face
            12, 13, 14, 14, 15, 12,
            // Top face
            16, 17, 18, 18, 19, 16,
            // Bottom face
            20, 21, 22, 22, 23, 20
        };

        mesh->setGeometry(vertices, indices);

        // Create material
        auto material = std::make_shared<Material>(device, commandPool);
        material->setName("CubeMaterial");
        
        if (!texturePath.empty()) {
            if (!material->loadTexture(texturePath)) {
                // If texture loading fails, fallback is automatically created
            }
        } else {
            material->createCheckerboardTexture();
        }

        uint32_t materialId = mesh->addMaterial(material);
        mesh->assignMaterial(0, static_cast<uint32_t>(indices.size()), materialId);

        model->addMesh(mesh);
        return model;
    }

    std::shared_ptr<Model3D> Model3D::createPlane(const AzGame::VulkanDevice& device, 
                                                 VkCommandPool commandPool,
                                                 float width, 
                                                 float height,
                                                 const std::string& texturePath) {
        auto model = std::make_shared<Model3D>();
        model->setName("Plane");

        auto mesh = std::make_shared<Mesh>(device);
        mesh->setName("PlaneMesh");

        float halfWidth = width * 0.5f;
        float halfHeight = height * 0.5f;

        // Plane vertices (horizontal plane in XZ)
        std::vector<AzGame::Vertex> vertices = {
            {{-halfWidth, 0.0f, -halfHeight}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{ halfWidth, 0.0f, -halfHeight}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{ halfWidth, 0.0f,  halfHeight}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-halfWidth, 0.0f,  halfHeight}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        };

        std::vector<uint16_t> indices = {
            0, 1, 2, 2, 3, 0
        };

        mesh->setGeometry(vertices, indices);

        // Create material
        auto material = std::make_shared<Material>(device, commandPool);
        material->setName("PlaneMaterial");
        
        if (!texturePath.empty()) {
            if (!material->loadTexture(texturePath)) {
                // If texture loading fails, fallback is automatically created
            }
        } else {
            material->createCheckerboardTexture();
        }

        uint32_t materialId = mesh->addMaterial(material);
        mesh->assignMaterial(0, static_cast<uint32_t>(indices.size()), materialId);

        model->addMesh(mesh);
        return model;
    }

    std::shared_ptr<Model3D> Model3D::createQuad(const AzGame::VulkanDevice& device, 
                                                VkCommandPool commandPool,
                                                float size,
                                                const std::string& texturePath) {
        auto model = std::make_shared<Model3D>();
        model->setName("Quad");

        auto mesh = std::make_shared<Mesh>(device);
        mesh->setName("QuadMesh");

        float halfSize = size * 0.5f;

        // Quad vertices (facing forward along Z-axis)
        std::vector<AzGame::Vertex> vertices = {
            {{-halfSize, -halfSize, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{ halfSize, -halfSize, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{ halfSize,  halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-halfSize,  halfSize, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        };

        std::vector<uint16_t> indices = {
            0, 1, 2, 2, 3, 0
        };

        mesh->setGeometry(vertices, indices);

        // Create material
        auto material = std::make_shared<Material>(device, commandPool);
        material->setName("QuadMaterial");
        
        if (!texturePath.empty()) {
            if (!material->loadTexture(texturePath)) {
                // If texture loading fails, fallback is automatically created
            }
        } else {
            material->createCheckerboardTexture();
        }

        uint32_t materialId = mesh->addMaterial(material);
        mesh->assignMaterial(0, static_cast<uint32_t>(indices.size()), materialId);

        model->addMesh(mesh);
        return model;
    }
}
