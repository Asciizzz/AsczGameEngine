#pragma once

/**
 * @brief AzModel - 3D Model and Material System
 * 
 * This namespace contains classes for managing 3D models, meshes, and materials
 * in the AsczGameEngine. It provides a modular system for handling geometry,
 * textures, and material assignments.
 * 
 * Key Components:
 * - Material: Manages texture resources and material properties
 * - Mesh: Contains geometry data (vertices/indices) and material assignments
 * - Model3D: High-level container for meshes with transformation support
 * 
 * Usage Example:
 * ```cpp
 * // Create a cube with a custom texture
 * auto model = AzModel::Model3D::createCube(device, commandPool, 2.0f, "textures/wood.jpg");
 * model->setPosition({0.0f, 1.0f, 0.0f});
 * model->setRotation({0.0f, glm::radians(45.0f), 0.0f});
 * 
 * // Create a custom model
 * auto customModel = std::make_shared<AzModel::Model3D>();
 * auto mesh = std::make_shared<AzModel::Mesh>(device);
 * mesh->setGeometry(vertices, indices);
 * 
 * auto material = std::make_shared<AzModel::Material>(device, commandPool);
 * material->loadTexture("textures/metal.jpg");
 * uint32_t matId = mesh->addMaterial(material);
 * mesh->assignMaterial(0, indices.size(), matId);
 * 
 * customModel->addMesh(mesh);
 * ```
 */

#include "Material.hpp"
#include "Mesh.hpp"
#include "Model3D.hpp"
