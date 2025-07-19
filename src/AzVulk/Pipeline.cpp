#include "AzVulk/Pipeline.h"

#include <fstream>

namespace AzVulk {

Pipeline::Pipeline(const char *vertfile, const char *fragfile) {
    createGraphicsPipeline(vertfile, fragfile);
}

std::vector<char> Pipeline::readFile(const char *path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed to open file: " + std::string(path));

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

void Pipeline::createGraphicsPipeline(const char *vertfile, const char *fragfile) {
    auto vertShaderCode = readFile(vertfile);
    auto fragShaderCode = readFile(fragfile);

    printf("Vertex shader code size: %zu bytes\n", vertShaderCode.size());
    printf("Fragment shader code size: %zu bytes\n", fragShaderCode.size());

    // Will create in the future
}

} // namespace AzVulk