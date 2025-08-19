#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <glm/glm.hpp>

#include "AzVulk/Buffer.hpp"
#include "AzVulk/DescriptorSets.hpp"

namespace Az3D {

    struct MaterialUBO {
        alignas(16) glm::vec4 prop1;

        MaterialUBO() : prop1(1.0f, 0.0f, 0.0f, 0.0f) {}
        MaterialUBO(const glm::vec4& p1) : prop1(p1) {}
    };

    struct Material {
        // Generic material properties using vec4 for alignment and flexibility
        // Put this FIRST to ensure proper alignment
        glm::vec4 prop1 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // <bool shading>, <int toonLevel>, <float normalBlend>, <float discardThreshold>
        
        // Custom material properties exclusive to AsczGameEngine
        size_t diffTxtr = 0; // Albedo/Diffuse map

        static Material fastTemplate(float a, float b, float c, float d, size_t diffTexture) {
            Material mat;
            mat.prop1 = glm::vec4(a, b, c, d);
            mat.diffTxtr = diffTexture;
            return mat;
        }
    };


    // MaterialManager - manages materials using index-based access
    class MaterialManager {
    public:
        MaterialManager(const AzVulk::Device* vkDevice);

        MaterialManager(const MaterialManager&) = delete;
        MaterialManager& operator=(const MaterialManager&) = delete;

        const AzVulk::Device* vkDevice;

        size_t addMaterial(const Material& material);

        SharedPtrVec<Material> materials;

        std::vector<AzVulk::BufferData> gpuBufferDatas;
        void createGPUBufferDatas();

        AzVulk::DynamicDescriptor dynamicDescriptor;
        void createDescriptorSets(uint32_t maxFramesInFlight);
        VkDescriptorSet getDescriptorSet(uint32_t materialIndex, uint32_t frameIndex, uint32_t maxFramesInFlight) const {
            return dynamicDescriptor.getSet(materialIndex * maxFramesInFlight + frameIndex);
        }


        void uploadToGPU(uint32_t maxFramesInFlight);
    };
    
} // namespace Az3D
