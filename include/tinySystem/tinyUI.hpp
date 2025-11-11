#pragma once

#include "imgui.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// TINY UI - Renderer-Agnostic ImGui Abstraction Layer
// ============================================================================
// This is a self-contained, portable UI system that can work with any renderer.
// Just implement IUIBackend for your graphics API and you're ready to go!
//
// Supported backends (implement as needed):
//   - Vulkan (see UIBackend_Vulkan below)
//   - OpenGL
//   - DirectX 11/12
//   - Metal
//   - WebGPU
// ============================================================================

namespace tinyUI {

// ============================================================================
// LAYER 1: Backend Interface (Platform Bridge)
// ============================================================================

// Opaque initialization data for backends
struct BackendInitInfo {
    void* windowHandle = nullptr;  // SDL_Window*, GLFWwindow*, etc.
    void* deviceHandle = nullptr;  // VkDevice, ID3D11Device*, etc.
    void* extraData = nullptr;     // Additional platform-specific data
};

// Abstract backend interface - implement this for each graphics API
struct IUIBackend {
    virtual ~IUIBackend() = default;
    
    virtual void init(const BackendInitInfo& info) = 0;
    virtual void newFrame() = 0;
    virtual void renderDrawData(ImDrawData* drawData) = 0;
    virtual void shutdown() = 0;
    virtual void onResize(uint32_t width, uint32_t height) { (void)width; (void)height; }
    virtual const char* getName() const = 0;
};

// ============================================================================
// LAYER 2: Core UI System (Renderer-Agnostic)
// ============================================================================

// ========================================
// Theme System
// ========================================

struct Theme {
    // Window colors
    ImVec4 windowBg = ImVec4(0.00f, 0.00f, 0.00f, 0.65f);
    ImVec4 childBg = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    ImVec4 border = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
    
    // Title bar
    ImVec4 titleBg = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    ImVec4 titleBgActive = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    ImVec4 titleBgCollapsed = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    
    // Text
    ImVec4 text = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    ImVec4 textDisabled = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    
    // Buttons - default
    ImVec4 button = ImVec4(0.30f, 0.50f, 0.80f, 1.00f);
    ImVec4 buttonHovered = ImVec4(0.40f, 0.60f, 0.90f, 1.00f);
    ImVec4 buttonActive = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
    
    // Buttons - primary
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
    
    // Scrollbar
    ImVec4 scrollbarBg = ImVec4(0.10f, 0.10f, 0.10f, 0.50f);
    ImVec4 scrollbarGrab = ImVec4(0.40f, 0.40f, 0.40f, 0.80f);
    ImVec4 scrollbarGrabHovered = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    ImVec4 scrollbarGrabActive = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    
    // Frame/Input fields
    ImVec4 frameBg = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    ImVec4 frameBgHovered = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    ImVec4 frameBgActive = ImVec4(0.24f, 0.24f, 0.26f, 1.00f);
    
    // Sizes & Rounding
    float scrollbarSize = 8.0f;
    float scrollbarRounding = 0.0f;
    float frameRounding = 0.0f;
    float windowRounding = 0.0f;
    float childRounding = 0.0f;
    float buttonRounding = 0.0f;
};

// Button style variants
enum class ButtonStyle { Default, Primary, Success, Danger, Warning };

// ========================================
// Main Execution Interface
// ========================================

class Exec {
public:
    // ========================================
    // Lifecycle Management
    // ========================================
    
    static void Init(IUIBackend* backend, void* windowHandle = nullptr);
    static void Shutdown();
    static void NewFrame();
    static void Render();

    static void SetTheme(const Theme& theme);
    static void ApplyTheme();
    static Theme& GetTheme();

    // ========================================
    // Window Management
    // ========================================

    static bool Begin(const char* name, bool* p_open = nullptr, int flags = 0);
    static void End();

private:
    static IUIBackend* s_backend;
    static Theme s_theme;
    static bool s_initialized;
};

} // namespace tinyUI