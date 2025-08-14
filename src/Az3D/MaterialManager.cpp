#include "Az3D/MaterialManager.hpp"

namespace Az3D {

    size_t MaterialManager::addMaterial(const Material& material) {
        materials.push_back(MakeShared<Material>(material));

        return materials.size() - 1;
    }

    void MaterialManager::createBufferDatas(VkDevice device, VkPhysicalDevice physicalDevice) {
        using namespace AzVulk;

        materialBufferDatas.resize(materials.size());

        for (size_t i = 0; i < materials.size(); ++i) {
            auto& bufferData = materialBufferDatas[i];
            bufferData.initVulkan(device, physicalDevice);

            bufferData.createBuffer(
                1, sizeof(MaterialUBO), BufferData::Uniform,
                BufferData::HostVisible | BufferData::HostCoherent
            );

            MaterialUBO materialUBO(materials[i]->prop1);
            bufferData.uploadData(&materialUBO);
        }
    }

} // namespace Az3D
