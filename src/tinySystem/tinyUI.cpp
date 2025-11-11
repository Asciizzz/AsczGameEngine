#include "tinySystem/tinyUI.hpp"
#include <cstdarg>
#include <imgui_internal.h>

using namespace tinyUI;

// Static member initialization
IUIBackend* Exec::s_backend = nullptr;
Theme Exec::s_theme;
bool Exec::s_initialized = false;

// ============================================================================
// Lifecycle Management
// ============================================================================

void Exec::Init(IUIBackend* backend, void* windowHandle) {
    if (s_initialized) return;

    s_backend = backend;
    
    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    
    // Apply default theme
    ApplyTheme();

    // Initialize backend
    if (s_backend) {
        BackendInitInfo info;
        info.windowHandle = windowHandle;
        s_backend->init(info);
    }
    
    s_initialized = true;
}

void Exec::Shutdown() {
    if (!s_initialized) {
        return;
    }
    
    if (s_backend) {
        s_backend->shutdown();
        delete s_backend;
        s_backend = nullptr;
    }
    
    ImGui::DestroyContext();
    s_initialized = false;
}

void Exec::NewFrame() {
    if (!s_initialized || !s_backend) {
        return;
    }
    
    s_backend->newFrame();
    ImGui::NewFrame();
}

void Exec::Render() {
    if (!s_initialized || !s_backend) return;
    
    ImGui::Render();
    s_backend->renderDrawData(ImGui::GetDrawData());
}

void Exec::SetTheme(const Theme& theme) {
    s_theme = theme;
}

void Exec::ApplyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    const Theme& theme = s_theme;

    // Colors
    style.Colors[ImGuiCol_Text] = theme.text;
    style.Colors[ImGuiCol_TextDisabled] = theme.textDisabled;
    style.Colors[ImGuiCol_WindowBg] = theme.windowBg;
    style.Colors[ImGuiCol_ChildBg] = theme.childBg;
    style.Colors[ImGuiCol_Border] = theme.border;
    
    style.Colors[ImGuiCol_TitleBg] = theme.titleBg;
    style.Colors[ImGuiCol_TitleBgActive] = theme.titleBgActive;
    style.Colors[ImGuiCol_TitleBgCollapsed] = theme.titleBgCollapsed;
    
    style.Colors[ImGuiCol_Button] = theme.button;
    style.Colors[ImGuiCol_ButtonHovered] = theme.buttonHovered;
    style.Colors[ImGuiCol_ButtonActive] = theme.buttonActive;
    
    style.Colors[ImGuiCol_Header] = theme.header;
    style.Colors[ImGuiCol_HeaderHovered] = theme.headerHovered;
    style.Colors[ImGuiCol_HeaderActive] = theme.headerActive;
    
    style.Colors[ImGuiCol_FrameBg] = theme.frameBg;
    style.Colors[ImGuiCol_FrameBgHovered] = theme.frameBgHovered;
    style.Colors[ImGuiCol_FrameBgActive] = theme.frameBgActive;
    
    style.Colors[ImGuiCol_ScrollbarBg] = theme.scrollbarBg;
    style.Colors[ImGuiCol_ScrollbarGrab] = theme.scrollbarGrab;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = theme.scrollbarGrabHovered;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = theme.scrollbarGrabActive;
    
    // Sizes & Rounding
    style.ScrollbarSize = theme.scrollbarSize;
    style.ScrollbarRounding = theme.scrollbarRounding;
    style.FrameRounding = theme.frameRounding;
    style.WindowRounding = theme.windowRounding;
    style.ChildRounding = theme.childRounding;
    style.GrabRounding = theme.buttonRounding;
    style.WindowBorderSize = 1.0f;
}

Theme& Exec::GetTheme() {
    return s_theme;
}

// ============================================================================
// Window Management
// ============================================================================

bool Exec::Begin(const char* name, bool* p_open, int flags) {
    return ImGui::Begin(name, p_open, flags);
}

void Exec::End() {
    ImGui::End();
}