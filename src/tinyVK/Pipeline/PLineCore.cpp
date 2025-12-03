// PLineCore.cpp
#include "tinyVk/Pipeline/PLineCore.hpp"
#include <fstream>
#include <stdexcept>

using namespace tinyVk;

std::vector<char> PLineCore::readFile(const std::string& path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f.is_open()) throw std::runtime_error("failed to open file: " + path);
    size_t size = (size_t)f.tellg();
    std::vector<char> buf(size);
    f.seekg(0);
    f.read(buf.data(), size);
    return buf;
}

VkShaderModule PLineCore::createModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule m{};
    if (vkCreateShaderModule(device, &ci, nullptr, &m) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module");
    return m;
}

VkShaderModule PLineCore::createModuleFromPath(VkDevice device, const std::string& path) {
    auto code = readFile(path);
    return createModule(device, code);
}