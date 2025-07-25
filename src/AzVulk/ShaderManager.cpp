#include "AzVulk/ShaderManager.hpp"
#include <fstream>
#include <stdexcept>
#include <algorithm>

namespace AzVulk {
    ShaderManager::ShaderManager(VkDevice device) : device(device) {}

    ShaderManager::~ShaderManager() {
        for (auto shaderModule : shaderModules) {
            vkDestroyShaderModule(device, shaderModule, nullptr);
        }
    }

    VkShaderModule ShaderManager::createShaderModule(const char* filename) {
        auto code = readFile(filename);

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        shaderModules.push_back(shaderModule);
        return shaderModule;
    }

    void ShaderManager::destroyShaderModule(VkShaderModule shaderModule) {
        auto it = std::find(shaderModules.begin(), shaderModules.end(), shaderModule);
        if (it != shaderModules.end()) {
            vkDestroyShaderModule(device, shaderModule, nullptr);
            shaderModules.erase(it);
        }
    }

    std::vector<char> ShaderManager::readFile(const char* filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + std::string(filename));
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }
}
