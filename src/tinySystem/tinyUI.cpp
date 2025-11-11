#include "tinySystem/tinyUI.hpp"
#include <cstdarg>
#include <algorithm>
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
    if (!s_initialized || !s_backend) return;
    
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

    // Colors
    style.Colors[ImGuiCol_Text] = s_theme.text;
    style.Colors[ImGuiCol_TextDisabled] = s_theme.textDisabled;
    style.Colors[ImGuiCol_WindowBg] = s_theme.windowBg;
    style.Colors[ImGuiCol_ChildBg] = s_theme.childBg;
    style.Colors[ImGuiCol_Border] = s_theme.border;
    
    style.Colors[ImGuiCol_TitleBg] = s_theme.titleBg;
    style.Colors[ImGuiCol_TitleBgActive] = s_theme.titleBgActive;
    style.Colors[ImGuiCol_TitleBgCollapsed] = s_theme.titleBgCollapsed;
    
    style.Colors[ImGuiCol_Button] = s_theme.button;
    style.Colors[ImGuiCol_ButtonHovered] = s_theme.buttonHovered;
    style.Colors[ImGuiCol_ButtonActive] = s_theme.buttonActive;
    
    style.Colors[ImGuiCol_Header] = s_theme.header;
    style.Colors[ImGuiCol_HeaderHovered] = s_theme.headerHovered;
    style.Colors[ImGuiCol_HeaderActive] = s_theme.headerActive;
    
    style.Colors[ImGuiCol_FrameBg] = s_theme.frameBg;
    style.Colors[ImGuiCol_FrameBgHovered] = s_theme.frameBgHovered;
    style.Colors[ImGuiCol_FrameBgActive] = s_theme.frameBgActive;
    
    style.Colors[ImGuiCol_ScrollbarBg] = s_theme.scrollbarBg;
    style.Colors[ImGuiCol_ScrollbarGrab] = s_theme.scrollbarGrab;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = s_theme.scrollbarGrabHovered;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = s_theme.scrollbarGrabActive;
    
    // Sizes & Rounding
    style.ScrollbarSize = s_theme.scrollbarSize;
    style.ScrollbarRounding = s_theme.scrollbarRounding;
    style.FrameRounding = s_theme.frameRounding;
    style.ChildRounding = s_theme.childRounding;
    style.GrabRounding = s_theme.buttonRounding;
    style.WindowRounding = s_theme.windowRounding;
    style.WindowBorderSize = s_theme.windowBorderSize;

    ImGui::GetIO().FontGlobalScale = s_theme.fontScale;
}

Theme& Exec::GetTheme() {
    return s_theme;
}

// ============================================================================
// Window Management
// ============================================================================

bool Exec::Begin(const char* name, bool* p_open, int flags) {
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

void Exec::End() {
    ImGui::End();
}