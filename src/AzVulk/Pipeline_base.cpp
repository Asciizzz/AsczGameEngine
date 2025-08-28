// BasePipeline.cpp
#include "AzVulk/Pipeline_base.hpp"
#include <fstream>
#include <stdexcept>

using namespace AzVulk;

std::vector<char> BasePipeline::readFile(const std::string& path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f.is_open()) throw std::runtime_error("failed to open file: " + path);
    size_t size = (size_t)f.tellg();
    std::vector<char> buf(size);
    f.seekg(0);
    f.read(buf.data(), size);
    return buf;
}

VkShaderModule BasePipeline::createModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule m{};
    if (vkCreateShaderModule(lDevice, &ci, nullptr, &m) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module");
    return m;
}
