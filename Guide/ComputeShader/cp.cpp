#include <vulkan/vulkan.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <cassert>

VkInstance instance;
VkDevice device;
VkPhysicalDevice physicalDevice;
VkQueue computeQueue;
uint32_t computeQueueFamilyIndex;

VkShaderModule createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
    return shaderModule;
}

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

int main() {
    // (1) Initialize Vulkan (instance, physical device, queue)
    // Skipped: instance & device creation, pick compute queue
    // You'd normally wrap this in proper Vulkan init code.

    // (2) Create buffer
    const int dataCount = 4;
    uint32_t inputData[dataCount] = {1, 2, 3, 4};
    VkBuffer buffer;
    VkDeviceMemory memory;

    // (You'd allocate a buffer with usage flags for storage buffer and map the memory)
    // Skipped here for brevity — requires helper functions

    // (3) Descriptor Set Layout
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = 0;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &layoutBinding;

    VkDescriptorSetLayout descriptorSetLayout;
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

    // (4) Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

    // (5) Shader Module
    auto shaderCode = readFile("test.spv");
    VkShaderModule computeShader = createShaderModule(shaderCode);

    // (6) Pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = computeShader;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = pipelineLayout;

    VkPipeline computePipeline;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);

    // (7) Command Buffer
    // - Record bind pipeline
    // - Bind descriptor sets
    // - Dispatch compute
    // (Skipped actual command pool/buffer allocation)

    // (8) Submit + Wait
    // - vkQueueSubmit + vkQueueWaitIdle

    // (9) Read back buffer
    // - Map memory and print data
    // Skipped for brevity

    // Clean up (in actual code)
    vkDestroyPipeline(device, computePipeline, nullptr);
    vkDestroyShaderModule(device, computeShader, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    std::cout << "Finished compute shader execution!\n";
}
