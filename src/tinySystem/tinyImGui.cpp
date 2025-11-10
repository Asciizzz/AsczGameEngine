#include "tinySystem/tinyImGui.hpp"
#include "tinyVk/System/CmdBuffer.hpp"
#include <stdexcept>
#include <iostream>
#include <algorithm>

using namespace tinyVk;

// ===========================
// THEME IMPLEMENTATION
// ===========================

void tinyImGui::Theme::apply() const {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Colors
    style.Colors[ImGuiCol_Text] = text;
    style.Colors[ImGuiCol_TextDisabled] = textDisabled;
    style.Colors[ImGuiCol_WindowBg] = windowBg;
    style.Colors[ImGuiCol_ChildBg] = childBg;
    style.Colors[ImGuiCol_Border] = border;
    
    style.Colors[ImGuiCol_TitleBg] = titleBg;
    style.Colors[ImGuiCol_TitleBgActive] = titleBgActive;
    style.Colors[ImGuiCol_TitleBgCollapsed] = titleBgCollapsed;
    
    style.Colors[ImGuiCol_Button] = button;
    style.Colors[ImGuiCol_ButtonHovered] = buttonHovered;
    style.Colors[ImGuiCol_ButtonActive] = buttonActive;
    
    style.Colors[ImGuiCol_Header] = header;
    style.Colors[ImGuiCol_HeaderHovered] = headerHovered;
    style.Colors[ImGuiCol_HeaderActive] = headerActive;
    
    style.Colors[ImGuiCol_FrameBg] = frameBg;
    style.Colors[ImGuiCol_FrameBgHovered] = frameBgHovered;
    style.Colors[ImGuiCol_FrameBgActive] = frameBgActive;
    
    style.Colors[ImGuiCol_ScrollbarBg] = scrollbarBg;
    style.Colors[ImGuiCol_ScrollbarGrab] = scrollbarGrab;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = scrollbarGrabHovered;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = scrollbarGrabActive;
    
    // Sizes & Rounding
    style.ScrollbarSize = scrollbarSize;
    style.ScrollbarRounding = scrollbarRounding;
    style.FrameRounding = frameRounding;
    style.WindowRounding = windowRounding;
    style.ChildRounding = childRounding;
    style.GrabRounding = buttonRounding;
    style.WindowBorderSize = 1.0f;
}

// ===========================
// INITIALIZATION & CORE
// ===========================

bool tinyImGui::init(SDL_Window* window, VkInstance instance, const tinyVk::Device* deviceVk, 
                     const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage) {
    if (m_initialized) {
        std::cerr << "tinyImGui: Already initialized!" << std::endl;
        return false;
    }

    this->deviceVk = deviceVk;
    
    // Create render pass
    createRenderPass(swapchain, depthImage);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    // Apply default theme
    ImGui::StyleColorsDark();
    theme.apply();

    // Create descriptor pool
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
    init_info.PipelineInfoMain.RenderPass = renderPass->get();
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    m_initialized = true;
    return true;
}

void tinyImGui::cleanup() {
    if (!m_initialized) return;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    windows.clear();
    descPool.destroy();

    m_initialized = false;
    deviceVk = nullptr;
}

void tinyImGui::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void tinyImGui::render(VkCommandBuffer commandBuffer) {
    // Render all windows
    for (auto& window : windows) {
        if (window.p_open && !(*window.p_open)) continue;
        
        if (ImGui::Begin(window.name.c_str(), window.p_open, window.flags)) {
            window.draw();
        }
        ImGui::End();
    }
    
    ImGui::Render();
}

void tinyImGui::processEvent(const SDL_Event* event) {
    ImGui_ImplSDL2_ProcessEvent(event);
}

// ===========================
// WINDOW MANAGEMENT
// ===========================

void tinyImGui::addWindow(const std::string& name, std::function<void()> draw, bool* p_open, int flags) {
    windows.emplace_back(name, draw, p_open, flags);
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

// ===========================
// COMPONENT SYSTEM - BUTTONS
// ===========================

void tinyImGui::pushButtonStyle(ButtonStyle style) {
    switch (style) {
        case ButtonStyle::Primary:
            ImGui::PushStyleColor(ImGuiCol_Button, theme.buttonPrimary);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.buttonPrimaryHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme.buttonPrimaryActive);
            break;
        case ButtonStyle::Success:
            ImGui::PushStyleColor(ImGuiCol_Button, theme.buttonSuccess);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.buttonSuccessHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme.buttonSuccessActive);
            break;
        case ButtonStyle::Danger:
            ImGui::PushStyleColor(ImGuiCol_Button, theme.buttonDanger);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.buttonDangerHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme.buttonDangerActive);
            break;
        case ButtonStyle::Warning:
            ImGui::PushStyleColor(ImGuiCol_Button, theme.buttonWarning);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.buttonWarningHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme.buttonWarningActive);
            break;
        case ButtonStyle::Default:
        default:
            ImGui::PushStyleColor(ImGuiCol_Button, theme.button);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.buttonHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme.buttonActive);
            break;
    }
}

void tinyImGui::popButtonStyle() {
    ImGui::PopStyleColor(3);
}

bool tinyImGui::button(const char* label, ButtonStyle style, ImVec2 size) {
    pushButtonStyle(style);
    bool result = ImGui::Button(label, size);
    popButtonStyle();
    return result;
}

bool tinyImGui::iconButton(const char* icon, const char* tooltip, ButtonStyle style) {
    pushButtonStyle(style);
    bool result = ImGui::SmallButton(icon);
    popButtonStyle();
    
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    
    return result;
}

// ===========================
// COMPONENT CONTAINER
// ===========================

bool tinyImGui::componentHeader(const char* label, bool showRemoveButton, std::function<void()> onRemove) {
    ImGui::PushStyleColor(ImGuiCol_Header, theme.componentBg);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme.headerHovered);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, theme.headerActive);
    
    bool isOpen = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
    
    if (showRemoveButton && onRemove) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
        if (iconButton("X", "Remove component", ButtonStyle::Danger)) {
            onRemove();
        }
    }
    
    ImGui::PopStyleColor(3);
    return isOpen;
}

void tinyImGui::componentBody(std::function<void()> content) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.componentBg);
    ImGui::PushStyleColor(ImGuiCol_Border, theme.componentBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme.childRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    
    if (ImGui::BeginChild("ComponentBody", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders)) {
        ImGui::Indent(8.0f);
        content();
        ImGui::Unindent(8.0f);
        ImGui::Spacing();
    }
    ImGui::EndChild();
    
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::Spacing();
    ImGui::Spacing();
}

// ===========================
// HANDLE FIELD
// ===========================

bool tinyImGui::handleField(const char* id, HandleFieldConfig& config) {
    bool modified = false;
    
    ImVec4 buttonColor = config.hasValue ? theme.buttonPrimary : theme.button;
    ImVec4 hoveredColor = config.hasValue ? theme.buttonPrimaryHovered : theme.buttonHovered;
    ImVec4 activeColor = config.hasValue ? theme.buttonPrimaryActive : theme.buttonActive;
    
    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
    
    std::string label = config.displayText + id;
    if (ImGui::Button(label.c_str(), config.size)) {
        if (config.hasValue && config.onClear) {
            config.onClear();
            modified = true;
        }
    }
    
    ImGui::PopStyleColor(3);
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", config.tooltip.c_str());
    }
    
    if (ImGui::BeginDragDropTarget()) {
        if (config.onDragDrop && config.onDragDrop()) {
            modified = true;
        }
        ImGui::EndDragDropTarget();
    }
    
    return modified;
}

// ===========================
// TREE NODE
// ===========================

bool tinyImGui::treeNode(const char* label, TreeNodeConfig& config) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    
    if (config.isLeaf) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (config.isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    // Apply custom background if provided
    if (config.customBgColor.w > 0.0f) {
        ImGui::PushStyleColor(ImGuiCol_Header, config.customBgColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, config.customBgColor);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Header, theme.header);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme.headerHovered);
    }
    
    if (config.forceOpen) {
        ImGui::SetNextItemOpen(true);
    }
    
    bool nodeOpen = ImGui::TreeNodeEx(label, flags);
    
    ImGui::PopStyleColor(2);
    
    // Handle drag source
    if (config.dragSource && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        if (config.dragSource()) {
            // Drag started successfully
        }
        ImGui::EndDragDropSource();
    }
    
    // Handle drag target
    if (config.dragTarget && ImGui::BeginDragDropTarget()) {
        if (config.dragTarget()) {
            // Drop accepted
        }
        ImGui::EndDragDropTarget();
    }
    
    // Handle selection
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (config.onLeftClick) {
            config.onLeftClick();
        }
    }
    
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        if (config.onRightClick) {
            config.onRightClick();
        }
    }
    
    // Context menu
    if (config.contextMenu && ImGui::BeginPopupContextItem()) {
        config.contextMenu();
        ImGui::EndPopup();
    }
    
    return nodeOpen && !config.isLeaf;
}

void tinyImGui::treeNodeEnd() {
    ImGui::TreePop();
}

// ===========================
// SCROLLABLE AREA
// ===========================

void tinyImGui::beginScrollArea(const char* id, ImVec2 size, bool alwaysShowScrollbar) {
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, theme.scrollbarSize);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, theme.scrollbarRounding);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, theme.scrollbarBg);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, theme.scrollbarGrab);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, theme.scrollbarGrabHovered);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, theme.scrollbarGrabActive);
    
    int flags = alwaysShowScrollbar ? ImGuiWindowFlags_AlwaysVerticalScrollbar : 0;
    ImGui::BeginChild(id, size, true, flags);
}

void tinyImGui::endScrollArea() {
    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

// ===========================
// INPUT FIELDS
// ===========================

bool tinyImGui::inputText(const char* label, char* buf, size_t bufSize, ImGuiInputTextFlags flags) {
    ImGui::SetNextItemWidth(-1);
    return ImGui::InputText(label, buf, bufSize, flags);
}

bool tinyImGui::inputFloat(const char* label, float* v, float step, float step_fast) {
    ImGui::SetNextItemWidth(-1);
    return ImGui::InputFloat(label, v, step, step_fast);
}

bool tinyImGui::inputFloat3(const char* label, float v[3]) {
    ImGui::SetNextItemWidth(-1);
    return ImGui::InputFloat3(label, v);
}

bool tinyImGui::inputInt(const char* label, int* v, int step, int step_fast) {
    ImGui::SetNextItemWidth(-1);
    return ImGui::InputInt(label, v, step, step_fast);
}

// ===========================
// COMBO BOX
// ===========================

bool tinyImGui::combo(const char* label, int* current_item, const char* const items[], int items_count) {
    ImGui::SetNextItemWidth(-1);
    return ImGui::Combo(label, current_item, items, items_count);
}

bool tinyImGui::combo(const char* label, int* current_item, const std::vector<std::string>& items) {
    ImGui::SetNextItemWidth(-1);
    return ImGui::Combo(label, current_item, 
        [](void* data, int idx, const char** out_text) {
            auto& vec = *static_cast<std::vector<std::string>*>(data);
            if (idx >= 0 && idx < static_cast<int>(vec.size())) {
                *out_text = vec[idx].c_str();
                return true;
            }
            return false;
        }, 
        const_cast<void*>(static_cast<const void*>(&items)), 
        static_cast<int>(items.size())
    );
}

// ===========================
// SEPARATOR & TOOLTIP
// ===========================

void tinyImGui::separator(const char* label) {
    if (label) {
        ImGui::SeparatorText(label);
    } else {
        ImGui::Separator();
    }
}

void tinyImGui::tooltip(const char* text) {
    ImGui::SetTooltip("%s", text);
}

void tinyImGui::tooltipOnHover(const char* text) {
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", text);
    }
}

void tinyImGui::showDemoWindow(bool* p_open) {
    ImGui::ShowDemoWindow(p_open);
}

// ===========================
// RENDER PASS MANAGEMENT
// ===========================

void tinyImGui::createDescriptorPool() {
    descPool.create(deviceVk->device, {
        { DescType::CombinedImageSampler, 1000 }
    }, 1000);
}

void tinyImGui::createRenderPass(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage) {
    auto renderPassConfig = tinyVk::RenderPassConfig::imguiOverlay(
        swapchain->getImageFormat(),
        depthImage->getFormat()
    );
    renderPass = MakeUnique<tinyVk::RenderPass>(deviceVk->device, renderPassConfig);
}

void tinyImGui::createRenderTargets(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage, 
                                   const std::vector<VkFramebuffer>& framebuffers) {
    renderTargets.clear();
    renderTargets.reserve(swapchain->getImageCount());
    
    for (uint32_t i = 0; i < swapchain->getImageCount(); ++i) {
        VkFramebuffer fb = (i < framebuffers.size()) ? framebuffers[i] : VK_NULL_HANDLE;
        renderTargets.emplace_back(renderPass->get(), fb, swapchain->getExtent());
    }
}

void tinyImGui::updateRenderPass(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage) {
    if (!m_initialized) return;
    
    vkDeviceWaitIdle(deviceVk->device);
    ImGui_ImplVulkan_Shutdown();
    
    createRenderPass(swapchain, depthImage);
    
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = VK_NULL_HANDLE;
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
    init_info.PipelineInfoMain.RenderPass = renderPass->get();
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);
}

VkRenderPass tinyImGui::getRenderPass() const {
    return renderPass ? renderPass->get() : VK_NULL_HANDLE;
}

tinyVk::RenderTarget* tinyImGui::getRenderTarget(uint32_t imageIndex) {
    if (imageIndex >= renderTargets.size()) return nullptr;
    return &renderTargets[imageIndex];
}

void tinyImGui::renderToTarget(uint32_t imageIndex, VkCommandBuffer cmd, VkFramebuffer framebuffer) {
    auto* target = getRenderTarget(imageIndex);
    if (!target) return;
    
    target->withFrameBuffer(framebuffer);
    
    target->beginRenderPass(cmd);
    
    // Render all windows first (this calls ImGui::Render() internally)
    render(cmd);
    
    // Then render the ImGui draw data to the command buffer
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    
    target->endRenderPass(cmd);
}

void tinyImGui::updateRenderTargets(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage, 
                                   const std::vector<VkFramebuffer>& framebuffers) {
    if (!renderPass) return;
    
    renderTargets.clear();
    VkExtent2D extent = swapchain->getExtent();
    
    for (uint32_t i = 0; i < swapchain->getImageCount(); ++i) {
        VkFramebuffer framebuffer = (i < framebuffers.size()) ? framebuffers[i] : VK_NULL_HANDLE;
        tinyVk::RenderTarget imguiTarget(renderPass->get(), framebuffer, extent);
        
        VkClearValue colorClear{};
        colorClear.color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        imguiTarget.addAttachment(swapchain->getImage(i), swapchain->getImageView(i), colorClear);
        
        VkClearValue depthClear{};
        depthClear.depthStencil = {1.0f, 0};
        imguiTarget.addAttachment(depthImage->getImage(), depthImage->getView(), depthClear);

        renderTargets.push_back(imguiTarget);
    }
}