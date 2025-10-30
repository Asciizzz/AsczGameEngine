#include "tinySystem/tinyImGui.hpp"
#include "tinyVk/System/CmdBuffer.hpp"
#include <stdexcept>
#include <iostream>
#include <algorithm>

using namespace tinyVk;

bool tinyImGui::init(SDL_Window* window, VkInstance instance, const tinyVk::Device* deviceVk, 
                     const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage) {
    if (m_initialized) {
        std::cerr << "tinyImGui: Already initialized!" << std::endl;
        return false;
    }

    this->deviceVk = deviceVk;
    
    // Create our own render pass for ImGui overlay  
    createRenderPass(swapchain, depthImage);
    // Note: render targets will be created later when we have access to framebuffers

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.25f);
    style.Colors[ImGuiCol_FrameBg]  = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    style.Colors[ImGuiCol_Header]   = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_Button]   = ImVec4(0.30f, 0.50f, 0.80f, 1.00f);

    // default font scale
    style.FontScaleMain = 1.2f;

    // Create descriptor pool for ImGui
    createDescriptorPool();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(window);
    
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = instance;
    init_info.PhysicalDevice = deviceVk->pDevice;
    init_info.Device = deviceVk->device;
    init_info.QueueFamily = deviceVk->queueFamilyIndices.graphicsFamily.value();
    init_info.Queue = deviceVk->graphicsQueue;
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

void tinyImGui::cleanup() {
    if (!m_initialized) return;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // Clear windows
    windows.clear();
    descPool.destroy(); // You don't really need to destroy the pool explicitly

    m_initialized = false;
    deviceVk = nullptr;
}

void tinyImGui::newFrame() {
    if (!m_initialized) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void tinyImGui::addWindow(const std::string& name, std::function<void()> draw, bool* p_open) {
    windows.emplace_back(name, draw, p_open);
}

void tinyImGui::removeWindow(const std::string& name) {
    windows.erase(
        std::remove_if(windows.begin(), windows.end(),
            [&name](const Window& w) { return w.name == name; }),
        windows.end()
    );
}

void tinyImGui::clearWindows() {
    windows.clear();
}

void tinyImGui::render(VkCommandBuffer commandBuffer) {
    if (!m_initialized) return;

    // Render all registered windows
    for (auto& window : windows) {
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

void tinyImGui::processEvent(const SDL_Event* event) {
    if (!m_initialized) return;
    ImGui_ImplSDL2_ProcessEvent(event);
}

void tinyImGui::showDemoWindow(bool* p_open) {
    if (!m_initialized) return;
    ImGui::ShowDemoWindow(p_open);
}

void tinyImGui::createDescriptorPool() {
    descPool.create(deviceVk->device,
        {
            { DescType::Sampler, 16 },                    // Font atlas + custom textures
            { DescType::CombinedImageSampler, 32 },     // Most commonly used by ImGui
            { DescType::SampledImage, 16 },              // Additional image sampling
            { DescType::UniformBuffer, 8 },              // Transform matrices, etc.
            { DescType::StorageBuffer, 4 },              // Rarely used by ImGui
            { DescType::UniformBufferDynamic, 4 },      // Dynamic uniforms
            { DescType::StorageBufferDynamic, 4 }       // Dynamic storage (rare)
        },
        64 // Much more reasonable than 11,000!
    );
}

void tinyImGui::updateRenderPass(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage) {
    if (!m_initialized) return;

    // Wait for device to be idle
    vkDeviceWaitIdle(deviceVk->device);

    // Shutdown current Vulkan backend (but keep SDL2 backend)
    ImGui_ImplVulkan_Shutdown();

    // Recreate our render pass with new swapchain format  
    createRenderPass(swapchain, depthImage);
    // Note: render targets will be recreated later when we have access to framebuffers

    // You don't need to recreate the descriptor pool every time
    // // Recreate descriptor pool (old one may be invalid)
    // createDescriptorPool();

    // Reinitialize Vulkan backend with new render pass
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = VK_NULL_HANDLE; // Not needed for reinit
    init_info.PhysicalDevice = deviceVk->pDevice;
    init_info.Device = deviceVk->device;
    init_info.QueueFamily = deviceVk->queueFamilyIndices.graphicsFamily.value();
    init_info.Queue = deviceVk->graphicsQueue;
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

VkRenderPass tinyImGui::getRenderPass() const {
    return renderPass ? renderPass->get() : VK_NULL_HANDLE;
}

tinyVk::RenderTarget* tinyImGui::getRenderTarget(uint32_t imageIndex) {
    return (imageIndex < renderTargets.size()) ? &renderTargets[imageIndex] : nullptr;
}

void tinyImGui::renderToTarget(uint32_t imageIndex, VkCommandBuffer cmd, VkFramebuffer framebuffer) {
    if (imageIndex < renderTargets.size()) {
        // Update render target with the correct framebuffer
        renderTargets[imageIndex].withFrameBuffer(framebuffer);

        renderTargets[imageIndex].beginRenderPass(cmd);
        render(cmd);
        renderTargets[imageIndex].endRenderPass(cmd);
    }
}

void tinyImGui::createRenderPass(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage) {
    auto renderPassConfig = tinyVk::RenderPassConfig::imguiOverlay(
        swapchain->getImageFormat(),
        depthImage->getFormat()
    );
    renderPass = MakeUnique<tinyVk::RenderPass>(deviceVk->device, renderPassConfig);
}

void tinyImGui::updateRenderTargets(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage, const std::vector<VkFramebuffer>& framebuffers) {
    if (!renderPass) return;
    
    renderTargets.clear();
    VkExtent2D extent = swapchain->getExtent();
    
    // Create ImGui render targets for each swapchain image
    for (uint32_t i = 0; i < swapchain->getImageCount(); ++i) {
        VkFramebuffer framebuffer = (i < framebuffers.size()) ? framebuffers[i] : VK_NULL_HANDLE;
        tinyVk::RenderTarget imguiTarget(renderPass->get(), framebuffer, extent);
        
        // Add swapchain image attachment (no clear needed for overlay)
        VkClearValue colorClear{};
        colorClear.color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Transparent
        imguiTarget.addAttachment(swapchain->getImage(i), swapchain->getImageView(i), colorClear);
        
        // Add depth attachment 
        VkClearValue depthClear{};
        depthClear.depthStencil = {1.0f, 0};
        imguiTarget.addAttachment(depthImage->getImage(), depthImage->getView(), depthClear);

        renderTargets.push_back(imguiTarget);
    }
}

void tinyImGui::createRenderTargets(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage, const std::vector<VkFramebuffer>& framebuffers) {
    updateRenderTargets(swapchain, depthImage, framebuffers);
}