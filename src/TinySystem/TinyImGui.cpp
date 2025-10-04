#include "TinySystem/TinyImGui.hpp"
#include "TinyVK/System/CmdBuffer.hpp"
#include <stdexcept>
#include <iostream>
#include <algorithm>

bool TinyImGui::init(SDL_Window* window, VkInstance instance, const TinyVK::Device* deviceVK, 
                     const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager) {
    if (m_initialized) {
        std::cerr << "TinyImGui: Already initialized!" << std::endl;
        return false;
    }

    this->deviceVK = deviceVK;
    
    // Create our own render pass for ImGui overlay  
    createRenderPass(swapchain, depthManager);
    // Note: render targets will be created later when we have access to framebuffers

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

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
    init_info.DescriptorPool = descPool;
    init_info.MinImageCount = swapchain->getImageCount();
    init_info.ImageCount = swapchain->getImageCount();
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    
    // Set up the pipeline info for the main viewport
    init_info.PipelineInfoMain.RenderPass = renderPass->get();
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    // Font upload is now handled automatically by ImGui

    m_initialized = true;
    return true;
}

void TinyImGui::cleanup() {
    if (!m_initialized) return;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // Clear windows
    m_windows.clear();
    descPool.destroy(); // You don't really need to destroy the pool explicitly

    m_initialized = false;
    deviceVK = nullptr;
}

void TinyImGui::newFrame() {
    if (!m_initialized) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void TinyImGui::addWindow(const std::string& name, std::function<void()> draw, bool* p_open) {
    m_windows.emplace_back(name, draw, p_open);
}

void TinyImGui::removeWindow(const std::string& name) {
    m_windows.erase(
        std::remove_if(m_windows.begin(), m_windows.end(),
            [&name](const Window& w) { return w.name == name; }),
        m_windows.end()
    );
}

void TinyImGui::clearWindows() {
    m_windows.clear();
}

void TinyImGui::render(VkCommandBuffer commandBuffer) {
    if (!m_initialized) return;

    // Render all registered windows
    for (auto& window : m_windows) {
        if (window.p_open) {
            // Window has open/close control
            if (*window.p_open) {
                ImGui::Begin(window.name.c_str(), window.p_open);
                if (window.draw) window.draw();
                ImGui::End();
            }
        } else {
            // Window is always open
            ImGui::Begin(window.name.c_str());
            if (window.draw) window.draw();
            ImGui::End();
        }
    }

    // Render ImGui
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void TinyImGui::processEvent(const SDL_Event* event) {
    if (!m_initialized) return;
    ImGui_ImplSDL2_ProcessEvent(event);
}

void TinyImGui::showDemoWindow(bool* p_open) {
    if (!m_initialized) return;
    ImGui::ShowDemoWindow(p_open);
}

void TinyImGui::createDescriptorPool() {
    descPool.create(deviceVK->device,
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 16 },                    // Font atlas + custom textures
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32 },     // Most commonly used by ImGui
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16 },              // Additional image sampling
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8 },              // Transform matrices, etc.
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 },              // Rarely used by ImGui
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 4 },      // Dynamic uniforms
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 4 }       // Dynamic storage (rare)
        },
        64 // Much more reasonable than 11,000!
    );
}

void TinyImGui::updateRenderPass(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager) {
    if (!m_initialized) return;

    // Wait for device to be idle
    vkDeviceWaitIdle(deviceVK->device);

    // Shutdown current Vulkan backend (but keep SDL2 backend)
    ImGui_ImplVulkan_Shutdown();

    // Recreate our render pass with new swapchain format  
    createRenderPass(swapchain, depthManager);
    // Note: render targets will be recreated later when we have access to framebuffers

    // You don't need to recreate the descriptor pool every time
    // // Recreate descriptor pool (old one may be invalid)
    // createDescriptorPool();

    // Reinitialize Vulkan backend with new render pass
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = VK_NULL_HANDLE; // Not needed for reinit
    init_info.PhysicalDevice = deviceVK->pDevice;
    init_info.Device = deviceVK->device;
    init_info.QueueFamily = deviceVK->queueFamilyIndices.graphicsFamily.value();
    init_info.Queue = deviceVK->graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descPool; // implicit use
    init_info.MinImageCount = swapchain->getImageCount();
    init_info.ImageCount = swapchain->getImageCount();
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    
    // Set up the pipeline info with our own render pass
    init_info.PipelineInfoMain.RenderPass = renderPass->get();
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);
}

VkRenderPass TinyImGui::getRenderPass() const {
    return renderPass ? renderPass->get() : VK_NULL_HANDLE;
}

TinyVK::RenderTarget* TinyImGui::getRenderTarget(uint32_t imageIndex) {
    return (imageIndex < renderTargets.size()) ? &renderTargets[imageIndex] : nullptr;
}

void TinyImGui::renderToTarget(uint32_t imageIndex, VkCommandBuffer cmd, VkFramebuffer framebuffer) {
    if (imageIndex < renderTargets.size()) {
        // Update render target with the correct framebuffer
        renderTargets[imageIndex].withFrameBuffer(framebuffer);

        renderTargets[imageIndex].beginRenderPass(cmd);
        render(cmd);
        renderTargets[imageIndex].endRenderPass(cmd);
    }
}

void TinyImGui::createRenderPass(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager) {
    auto renderPassConfig = TinyVK::RenderPassConfig::imguiOverlay(
        swapchain->getImageFormat(),
        depthManager->getDepthFormat()
    );
    renderPass = MakeUnique<TinyVK::RenderPass>(deviceVK->device, renderPassConfig);
}

void TinyImGui::updateRenderTargets(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager, const std::vector<VkFramebuffer>& framebuffers) {
    if (!renderPass) return;
    
    renderTargets.clear();
    VkExtent2D extent = swapchain->getExtent();
    
    // Create ImGui render targets for each swapchain image
    for (uint32_t i = 0; i < swapchain->getImageCount(); ++i) {
        VkFramebuffer framebuffer = (i < framebuffers.size()) ? framebuffers[i] : VK_NULL_HANDLE;
        TinyVK::RenderTarget imguiTarget(renderPass->get(), framebuffer, extent);
        
        // Add swapchain image attachment (no clear needed for overlay)
        VkClearValue colorClear{};
        colorClear.color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Transparent
        imguiTarget.addAttachment(swapchain->getImage(i), swapchain->getImageView(i), colorClear);
        
        // Add depth attachment 
        VkClearValue depthClear{};
        depthClear.depthStencil = {1.0f, 0};
        imguiTarget.addAttachment(depthManager->getDepthImage(), depthManager->getDepthImageView(), depthClear);
        
        renderTargets.push_back(imguiTarget);
    }
}

void TinyImGui::createRenderTargets(const TinyVK::Swapchain* swapchain, const TinyVK::DepthManager* depthManager, const std::vector<VkFramebuffer>& framebuffers) {
    updateRenderTargets(swapchain, depthManager, framebuffers);
}