#pragma once

#include "Az3D/Az3D.hpp"

namespace AzGame {
    class World {
    public:
        World(Az3D::ResourceManager* resManager, const AzVulk::Device* vkDevice)
        : resourceManager(resManager), vkDevice(vkDevice) {

            // Initialized global palette material
            Az3D::Material globalPaletteMaterial;
            globalPaletteMaterial.setShadingParams(true, 1, 0.5f, 0.0f);
            globalPaletteMaterial.setAlbedoTextureIndex(
                resourceManager->addTexture("GlobalPalette", "Assets/Platformer/Palette.png")
            );

            materialIndex = resourceManager->addMaterial("GlobalPalette", globalPaletteMaterial);

            // Initialized mesh in the mesh manager
            for (const auto& mesh : platformerMeshes) {
                std::string fullName = "Platformer/" + mesh.first;
                std::string fullPath = "Assets/Platformer/" + mesh.second;

                // Short hand name -----------------------------------Full name and Full path
                platformerMeshIndices[mesh.first] = resourceManager->addMesh(fullName, fullPath, true);
            }

            // Initialized world model group
            // worldModelGroup = Az3D::ModelGroup("World", vkDevice);
        }

        Az3D::ResourceManager* resourceManager;
        const AzVulk::Device* vkDevice;

    // World element!
        size_t materialIndex = SIZE_MAX;
        UnorderedMap<std::string, size_t> platformerMeshIndices;

        std::vector<std::pair<std::string, std::string>> platformerMeshes = {
            {"Ground_x2", "ground_grass_2.obj"},
            {"Ground_x4", "ground_grass_4.obj"},
            {"Ground_x8", "ground_grass_8.obj"},

            {"Water_x2", "water_2.obj"},
            {"Water_x4", "water_4.obj"},

            {"Tree_1", "Tree_1.obj"},
            {"Tree_2", "Tree_2.obj"},

            {"TrailCurve_1", "trail_dirt_curved_1.obj"},
            {"TrailCurve_2", "trail_dirt_curved_2.obj"},
            {"TrailEnd_1", "trail_dirt_end_1.obj"},
            {"TrailEnd_2", "trail_dirt_end_2.obj"},

            {"Fence_x1", "fence_1.obj"},
            {"Fence_x2", "fence_2.obj"},
            {"Fence_x4", "fence_4.obj"},

            {"Flower", "flower.obj"}
        };

        // Az3D::ModelGroup worldModelGroup;

        // Unique cool things to do in here
        void placePlatformGrid(std::string name, glm::vec3 pos) {
            auto it = platformerMeshIndices.find(name);
            if (it == platformerMeshIndices.end()) return;

            size_t meshIndex = it->second;

            // Place the mesh in the world model group

            // Convert to grid world by snapping it
            float gx = glm::floor(pos.x);
            float gy = glm::floor(pos.y);
            float gz = glm::floor(pos.z);

            Az3D::Transform trform;
            trform.pos = glm::vec3(gx, gy, gz);

            Az3D::InstanceStatic data;
            data.modelMatrix = trform.getMat4();
            data.properties.x = static_cast<int>(materialIndex);

            // worldModelGroup.addInstance(meshIndex, materialIndex, data);
        }
    };
}