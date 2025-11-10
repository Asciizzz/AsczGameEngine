#pragma once

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "tinyVk/Render/RenderPass.hpp"
#include "tinyVk/Render/RenderTarget.hpp"
#include "tinyVk/Render/Swapchain.hpp"
#include "tinyVk/Render/DepthImage.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include <SDL2/SDL.h>
#include <functional>
#include <unordered_map>
#include <string>
#include <vector>

class tinyImGui {
public:
    // ===========================
    // THEME SYSTEM
    // ===========================
    struct Theme {
        // Window colors (matching old theme)
        ImVec4 windowBg = ImVec4(0.00f, 0.00f, 0.00f, 0.65f);
        ImVec4 childBg = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        ImVec4 border = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
        
        // Title bar (matching old theme)
        ImVec4 titleBg = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        ImVec4 titleBgActive = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        ImVec4 titleBgCollapsed = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        
        // Text
        ImVec4 text = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        ImVec4 textDisabled = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        
        // Buttons - default style (matching old blue button)
        ImVec4 button = ImVec4(0.30f, 0.50f, 0.80f, 1.00f);
        ImVec4 buttonHovered = ImVec4(0.40f, 0.60f, 0.90f, 1.00f);
        ImVec4 buttonActive = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
        
        // Buttons - primary (accent)
        ImVec4 buttonPrimary = ImVec4(0.30f, 0.50f, 0.80f, 1.00f);
        ImVec4 buttonPrimaryHovered = ImVec4(0.40f, 0.60f, 0.90f, 1.00f);
        ImVec4 buttonPrimaryActive = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
        
        // Buttons - success
        ImVec4 buttonSuccess = ImVec4(0.30f, 0.70f, 0.40f, 1.00f);
        ImVec4 buttonSuccessHovered = ImVec4(0.40f, 0.80f, 0.50f, 1.00f);
        ImVec4 buttonSuccessActive = ImVec4(0.50f, 0.90f, 0.60f, 1.00f);
        
        // Buttons - danger
        ImVec4 buttonDanger = ImVec4(0.80f, 0.30f, 0.30f, 1.00f);
        ImVec4 buttonDangerHovered = ImVec4(0.90f, 0.40f, 0.40f, 1.00f);
        ImVec4 buttonDangerActive = ImVec4(1.00f, 0.50f, 0.50f, 1.00f);
        
        // Buttons - warning
        ImVec4 buttonWarning = ImVec4(0.90f, 0.70f, 0.30f, 1.00f);
        ImVec4 buttonWarningHovered = ImVec4(1.00f, 0.80f, 0.40f, 1.00f);
        ImVec4 buttonWarningActive = ImVec4(1.00f, 0.90f, 0.50f, 1.00f);
        
        // Headers & Tree nodes
        ImVec4 header = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
        ImVec4 headerHovered = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
        ImVec4 headerActive = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);
        
        // Component backgrounds
        ImVec4 componentBg = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
        ImVec4 componentBorder = ImVec4(0.25f, 0.30f, 0.35f, 1.00f);
        
        // Special highlights
        ImVec4 selected = ImVec4(0.35f, 0.50f, 0.75f, 0.80f);
        ImVec4 dragDrop = ImVec4(0.40f, 0.60f, 0.90f, 0.90f);
        
        // Scrollbar
        ImVec4 scrollbarBg = ImVec4(0.10f, 0.10f, 0.10f, 0.50f);
        ImVec4 scrollbarGrab = ImVec4(0.40f, 0.40f, 0.40f, 0.80f);
        ImVec4 scrollbarGrabHovered = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        ImVec4 scrollbarGrabActive = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        
        // Frame/Input fields
        ImVec4 frameBg = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
        ImVec4 frameBgHovered = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
        ImVec4 frameBgActive = ImVec4(0.24f, 0.24f, 0.26f, 1.00f);
        
        // Sizes
        float scrollbarSize = 8.0f;
        float scrollbarRounding = 0.0f;
        float frameRounding = 0.0f;
        float windowRounding = 0.0f;
        float childRounding = 0.0f;
        float buttonRounding = 0.0f;
        
        void apply() const;
    };

    // ===========================
    // COMPONENT SYSTEM
    // ===========================
    
    // Button styles
    enum class ButtonStyle { Default, Primary, Success, Danger, Warning };
    
    // Tree node configuration
    struct TreeNodeConfig {
        bool isLeaf = false;
        bool isSelected = false;
        bool isHighlighted = false;
        bool forceOpen = false;
        ImVec4 customBgColor = ImVec4(0, 0, 0, 0); // Transparent = use theme
        std::function<void()> onLeftClick = nullptr;
        std::function<void()> onRightClick = nullptr;
        std::function<void()> contextMenu = nullptr;
        std::function<bool()> dragSource = nullptr; // Return true if drag started
        std::function<bool()> dragTarget = nullptr; // Return true if drop accepted
    };
    
    // Handle field configuration
    struct HandleFieldConfig {
        bool hasValue = false;
        std::string displayText = "None";
        std::string tooltip = "Drag and drop to assign";
        ImVec2 size = ImVec2(-1, 30);
        std::function<void()> onClear = nullptr;
        std::function<bool()> onDragDrop = nullptr; // Return true if drop accepted
    };

    // Window wrapper
    struct Window {
        std::string name;
        std::function<void()> draw;
        bool* p_open = nullptr;
        int flags = 0;
        
        Window(const std::string& windowName, std::function<void()> drawFunc, bool* openPtr = nullptr, int windowFlags = 0)
            : name(windowName), draw(drawFunc), p_open(openPtr), flags(windowFlags) {}
    };

    tinyImGui() noexcept = default;
    ~tinyImGui() noexcept = default;

    tinyImGui(const tinyImGui&) = delete;
    tinyImGui& operator=(const tinyImGui&) = delete;

    // ===========================
    // INITIALIZATION & CORE
    // ===========================
    bool init(SDL_Window* window, VkInstance instance, const tinyVk::Device* deviceVk, 
              const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage);
    void cleanup();
    void newFrame();
    void render(VkCommandBuffer commandBuffer);
    void processEvent(const SDL_Event* event);
    
    // ===========================
    // THEME MANAGEMENT
    // ===========================
    Theme& getTheme() { return theme; }
    const Theme& getTheme() const { return theme; }
    void setTheme(const Theme& newTheme) { theme = newTheme; theme.apply(); }
    void applyTheme() { theme.apply(); }
    
    // ===========================
    // WINDOW MANAGEMENT
    // ===========================
    void addWindow(const std::string& name, std::function<void()> draw, bool* p_open = nullptr, int flags = 0);
    void removeWindow(const std::string& name);
    void clearWindows();
    
    // ===========================
    // REUSABLE COMPONENTS
    // ===========================
    
    // Styled buttons with automatic theme application
    bool button(const char* label, ButtonStyle style = ButtonStyle::Default, ImVec2 size = ImVec2(0, 0));
    bool iconButton(const char* icon, const char* tooltip = nullptr, ButtonStyle style = ButtonStyle::Default);
    
    // Component container - styled box with header
    bool componentHeader(const char* label, bool showRemoveButton = false, 
                        std::function<void()> onRemove = nullptr);
    void componentBody(std::function<void()> content);
    
    // Handle field - drag-drop target with visual feedback
    bool handleField(const char* id, HandleFieldConfig& config);
    
    // Tree node with full customization
    bool treeNode(const char* label, TreeNodeConfig& config);
    void treeNodeEnd(); // Call after rendering children
    
    // Styled scrollable child region
    void beginScrollArea(const char* id, ImVec2 size = ImVec2(0, 0), bool alwaysShowScrollbar = true);
    void endScrollArea();
    
    // Input fields with theme styling
    bool inputText(const char* label, char* buf, size_t bufSize, ImGuiInputTextFlags flags = 0);
    bool inputFloat(const char* label, float* v, float step = 0.0f, float step_fast = 0.0f);
    bool inputFloat3(const char* label, float v[3]);
    bool inputInt(const char* label, int* v, int step = 1, int step_fast = 100);
    
    // Combo box with theme styling
    bool combo(const char* label, int* current_item, const char* const items[], int items_count);
    bool combo(const char* label, int* current_item, const std::vector<std::string>& items);
    
    // Separator with optional label
    void separator(const char* label = nullptr);
    
    // Tooltip helper
    void tooltip(const char* text);
    void tooltipOnHover(const char* text);
    
    // ===========================
    // RENDER PASS MANAGEMENT
    // ===========================
    void updateRenderPass(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage);
    VkRenderPass getRenderPass() const;
    tinyVk::RenderTarget* getRenderTarget(uint32_t imageIndex);
    void renderToTarget(uint32_t imageIndex, VkCommandBuffer cmd, VkFramebuffer framebuffer);
    void updateRenderTargets(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage, 
                            const std::vector<VkFramebuffer>& framebuffers);
    
    // Demo
    void showDemoWindow(bool* p_open = nullptr);

private:
    bool m_initialized = false;
    
    // Vulkan context
    const tinyVk::Device* deviceVk = nullptr;
    tinyVk::DescPool descPool;
    UniquePtr<tinyVk::RenderPass> renderPass;
    std::vector<tinyVk::RenderTarget> renderTargets;
    
    // Window management
    std::vector<Window> windows;
    
    // Theme
    Theme theme;
    
    // Helper methods
    void createDescriptorPool();
    void createRenderPass(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage);
    void createRenderTargets(const tinyVk::Swapchain* swapchain, const tinyVk::DepthImage* depthImage, 
                           const std::vector<VkFramebuffer>& framebuffers);
    
    // Style helpers
    void pushButtonStyle(ButtonStyle style);
    void popButtonStyle();
};