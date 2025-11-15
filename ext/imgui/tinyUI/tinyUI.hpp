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
// Just implement IBackend for your graphics API and you're ready to go!
//
// Supported backends (implement as needed):
//   - Vulkan (see Backend_Vulkan below)
//   - OpenGL
//   - DirectX 11/12
//   - Metal
//   - WebGPU
//   - whatever you can dream of!
// ============================================================================

namespace tinyUI {

struct BackendInitInfo {
    void* windowHandle = nullptr;  // SDL_Window*, GLFWwindow*, etc.
    void* deviceHandle = nullptr;  // VkDevice, ID3D11Device*, etc.
    void* extraData = nullptr;     // Additional platform-specific data
};

// Abstract backend interface - implement this for each graphics API
struct IBackend {
    virtual ~IBackend() = default;
    
    virtual void init(const BackendInitInfo& info) = 0;
    virtual void newFrame() = 0;
    virtual void renderDrawData(ImDrawData* drawData) = 0;
    virtual void shutdown() = 0;
    virtual void onResize(uint32_t width, uint32_t height) { (void)width; (void)height; }
    virtual const char* getName() const = 0;
};

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
    ImVec4 button = ImVec4(0.20f, 0.40f, 0.80f, 1.00f);
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
    ImVec4 header = ImVec4(0.25f, 0.25f, 0.30f, 0.55f);
    ImVec4 headerHovered = ImVec4(0.35f, 0.35f, 0.40f, 0.55f);
    ImVec4 headerActive = ImVec4(0.45f, 0.45f, 0.50f, 0.55f);
    
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
    float childRounding = 0.0f;
    float buttonRounding = 0.0f;
    float windowRounding = 0.0f;
    float windowBorderSize = 1.0f;

    float fontScale = 1.4f;

    void apply() {
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
        style.ChildRounding = childRounding;
        style.GrabRounding = buttonRounding;
        style.WindowRounding = windowRounding;
        style.WindowBorderSize = windowBorderSize;

        ImGui::GetIO().FontGlobalScale = fontScale;
    }
};

// ========================================
// Main Execution Interface
// ========================================

class Instance {
public:
    ~Instance() { Shutdown(); }

    bool Init(IBackend* backend, void* windowHandle = nullptr) {
        if (!backend) return false;
        Shutdown();

        backend_ = backend;


        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls (pretty useless)
        io.ConfigWindowsMoveFromTitleBarOnly = true;

        BackendInitInfo info;
        info.windowHandle = windowHandle;
        backend_->init(info);

        theme_.apply();

        return true;
    }

    void Shutdown() {
        if (backend_ == nullptr) return;

        backend_->shutdown();
        delete backend_;
        backend_ = nullptr;

        ImGui::DestroyContext();
    }

    void NewFrame() {
        if (!backend_) return;

        backend_->newFrame();
        ImGui::NewFrame();
    }

    void Render() {
        if (!backend_) return;

        ImGui::Render();
        backend_->renderDrawData(ImGui::GetDrawData());
    }

    // These functions soon to be put into templates
    bool Begin(const char* name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0) {
        bool result = ImGui::Begin(name, p_open, flags);

        // Clamp window to viewport bounds
        if (result) {
            ImVec2 pos = ImGui::GetWindowPos();
            ImVec2 size = ImGui::GetWindowSize();
            ImGuiViewport* vp = ImGui::GetMainViewport();
            ImVec2 vp_min = vp->Pos;
            ImVec2 vp_max = ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);

            pos.x = std::max(vp_min.x, std::min(pos.x, vp_max.x - size.x));
            pos.y = std::max(vp_min.y, std::min(pos.y, vp_max.y - size.y));

            size.x = std::min(size.x, vp_max.x - pos.x);
            size.y = std::min(size.y, vp_max.y - pos.y);

            ImGui::SetWindowPos(pos);
            ImGui::SetWindowSize(size);
        }

        return result;
    }

    void End() {
        ImGui::End();
    }

    IBackend* backend() { return backend_; }
    Theme& theme() { return theme_; }

private:
    IBackend* backend_ = nullptr;
    Theme theme_;

    // Reusable template storage
    // std::unordered_map<std::string, ITemplate*> templates_;
};

} // namespace tinyUI