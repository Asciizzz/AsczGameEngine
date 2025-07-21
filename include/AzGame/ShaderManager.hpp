#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace AzGame {
    class ShaderManager {
    public:
        ShaderManager(VkDevice device);
        ~ShaderManager();

        // Delete copy constructor and assignment operator
        ShaderManager(const ShaderManager&) = delete;
        ShaderManager& operator=(const ShaderManager&) = delete;

        VkShaderModule createShaderModule(const std::string& filename);
        void destroyShaderModule(VkShaderModule shaderModule);

        static std::vector<char> readFile(const std::string& filename);

    private:
        VkDevice device;
        std::vector<VkShaderModule> shaderModules;
    };
}
