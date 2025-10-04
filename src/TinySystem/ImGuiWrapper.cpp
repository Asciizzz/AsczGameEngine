#include "TinySystem/ImGuiWrapper.hpp"
#include "TinyVK/System/CmdBuffer.hpp"
#include <stdexcept>
#include <iostream>

bool ImGuiWrapper::init(SDL_Window* window, VkInstance instance, const TinyVK::Device* deviceVK, 
                       const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager) {
    if (m_initialized) {
        std::cerr << "ImGuiWrapper: Already initialized!" << std::endl;
        return false;
    }

    this->deviceVK = deviceVK;
    
    // Create our own render pass for ImGui overlay
    createRenderPass(swapchain, depthManager);

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
    init_info.Device = deviceVK->device;
    init_info.QueueFamily = deviceVK->queueFamilyIndices.graphicsFamily.value();
    init_info.Queue = deviceVK->graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_descriptorPool;
    init_info.MinImageCount = swapchain->getImageCount();
    init_info.ImageCount = swapchain->getImageCount();
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    
    // Set up the pipeline info for the main viewport
    init_info.PipelineInfoMain.RenderPass = m_renderPass->get();
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
    deviceVK = nullptr;
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

    if (vkCreateDescriptorPool(deviceVK->device, &pool_info, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }
}

void ImGuiWrapper::destroyDescriptorPool() {
    if (m_descriptorPool != VK_NULL_HANDLE && deviceVK != nullptr) {
        vkDestroyDescriptorPool(deviceVK->device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
}

void ImGuiWrapper::updateRenderPass(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager) {
    if (!m_initialized) return;

    // Wait for device to be idle
    vkDeviceWaitIdle(deviceVK->device);

    // Shutdown current Vulkan backend (but keep SDL2 backend)
    ImGui_ImplVulkan_Shutdown();

    // Recreate our render pass with new swapchain format
    createRenderPass(swapchain, depthManager);

    // Recreate descriptor pool (old one may be invalid)
    destroyDescriptorPool();
    createDescriptorPool();

    // Reinitialize Vulkan backend with new render pass
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = VK_NULL_HANDLE; // Not needed for reinit
    init_info.PhysicalDevice = deviceVK->pDevice;
    init_info.Device = deviceVK->device;
    init_info.QueueFamily = deviceVK->queueFamilyIndices.graphicsFamily.value();
    init_info.Queue = deviceVK->graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_descriptorPool;
    init_info.MinImageCount = swapchain->getImageCount();
    init_info.ImageCount = swapchain->getImageCount();
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    
    // Set up the pipeline info with our own render pass
    init_info.PipelineInfoMain.RenderPass = m_renderPass->get();
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);
}

VkRenderPass ImGuiWrapper::getRenderPass() const {
    return m_renderPass ? m_renderPass->get() : VK_NULL_HANDLE;
}

void ImGuiWrapper::createRenderPass(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager) {
    auto renderPassConfig = TinyVK::RenderPassConfig::imguiOverlay(
        swapchain->getImageFormat(),
        depthManager->getDepthFormat()
    );
    m_renderPass = MakeUnique<TinyVK::RenderPass>(deviceVK->device, renderPassConfig);
}