#include "AzCore/ImGuiWrapper.hpp"
#include "TinyVK/CmdBuffer.hpp"
#include <stdexcept>
#include <iostream>

bool ImGuiWrapper::init(SDL_Window* window, VkInstance instance, const TinyVK::Device* deviceVK, 
                       const TinyVK::RenderPass* renderPass, uint32_t imageCount) {
    if (m_initialized) {
        std::cerr << "ImGuiWrapper: Already initialized!" << std::endl;
        return false;
    }

    m_deviceVK = deviceVK;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Create descriptor pool for ImGui
    createDescriptorPool();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(window);
    
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = instance;
    init_info.PhysicalDevice = deviceVK->pDevice;
    init_info.Device = deviceVK->lDevice;
    init_info.QueueFamily = deviceVK->queueFamilyIndices.graphicsFamily.value();
    init_info.Queue = deviceVK->graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_descriptorPool;
    init_info.MinImageCount = imageCount;
    init_info.ImageCount = imageCount;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    
    // Set up the pipeline info for the main viewport
    init_info.PipelineInfoMain.RenderPass = renderPass->renderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    // Font upload is now handled automatically by ImGui

    m_initialized = true;
    std::cout << "ImGui initialized successfully!" << std::endl;
    return true;
}

void ImGuiWrapper::cleanup() {
    if (!m_initialized) return;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    destroyDescriptorPool();
    
    m_initialized = false;
    m_deviceVK = nullptr;
}

void ImGuiWrapper::newFrame() {
    if (!m_initialized) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void ImGuiWrapper::render(VkCommandBuffer commandBuffer) {
    if (!m_initialized) return;

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void ImGuiWrapper::processEvent(const SDL_Event* event) {
    if (!m_initialized) return;
    ImGui_ImplSDL2_ProcessEvent(event);
}

void ImGuiWrapper::showDemoWindow(bool* p_open) {
    if (!m_initialized) return;
    ImGui::ShowDemoWindow(p_open);
}

void ImGuiWrapper::createDescriptorPool() {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(m_deviceVK->lDevice, &pool_info, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }
}

void ImGuiWrapper::destroyDescriptorPool() {
    if (m_descriptorPool != VK_NULL_HANDLE && m_deviceVK != nullptr) {
        vkDestroyDescriptorPool(m_deviceVK->lDevice, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
}