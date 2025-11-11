#include "tinySystem/tinyUI.hpp"
#include <cstdarg>

namespace tinyUI {

// Static member initialization
IUIBackend* UI::s_backend = nullptr;
UI::Theme UI::s_theme;
bool UI::s_initialized = false;

// ============================================================================
// Lifecycle Management
// ============================================================================

void UI::Init(IUIBackend* backend, void* windowHandle) {
    if (s_initialized) {
        return;
    }
    
    s_backend = backend;
    
    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    
    // Apply default dark theme
    ImGui::StyleColorsDark();
    s_theme.Apply();
    
    // Initialize backend
    if (s_backend) {
        BackendInitInfo info;
        info.windowHandle = windowHandle;
        s_backend->init(info);
    }
    
    s_initialized = true;
}

void UI::Shutdown() {
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

void UI::NewFrame() {
    if (!s_initialized || !s_backend) {
        return;
    }
    
    s_backend->newFrame();
    ImGui::NewFrame();
}

void UI::Render() {
    if (!s_initialized || !s_backend) {
        return;
    }
    
    ImGui::Render();
    s_backend->renderDrawData(ImGui::GetDrawData());
}

// ============================================================================
// Theme System
// ============================================================================

void UI::Theme::Apply() const {
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

void UI::SetTheme(const Theme& theme) {
    s_theme = theme;
    s_theme.Apply();
}

UI::Theme& UI::GetTheme() {
    return s_theme;
}

// ============================================================================
// Window Management
// ============================================================================

bool UI::Begin(const char* name, bool* p_open, int flags) {
    return ImGui::Begin(name, p_open, flags);
}

void UI::End() {
    ImGui::End();
}

// ============================================================================
// Style Helpers
// ============================================================================

void UI::PushButtonStyle(ButtonStyle style) {
    switch (style) {
        case ButtonStyle::Primary:
            ImGui::PushStyleColor(ImGuiCol_Button, s_theme.buttonPrimary);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s_theme.buttonPrimaryHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, s_theme.buttonPrimaryActive);
            break;
        case ButtonStyle::Success:
            ImGui::PushStyleColor(ImGuiCol_Button, s_theme.buttonSuccess);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s_theme.buttonSuccessHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, s_theme.buttonSuccessActive);
            break;
        case ButtonStyle::Danger:
            ImGui::PushStyleColor(ImGuiCol_Button, s_theme.buttonDanger);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s_theme.buttonDangerHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, s_theme.buttonDangerActive);
            break;
        case ButtonStyle::Warning:
            ImGui::PushStyleColor(ImGuiCol_Button, s_theme.buttonWarning);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s_theme.buttonWarningHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, s_theme.buttonWarningActive);
            break;
        case ButtonStyle::Default:
        default:
            ImGui::PushStyleColor(ImGuiCol_Button, s_theme.button);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s_theme.buttonHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, s_theme.buttonActive);
            break;
    }
}

void UI::PopButtonStyle() {
    ImGui::PopStyleColor(3);
}

// ============================================================================
// Common Widgets
// ============================================================================

bool UI::Button(const char* label, const ImVec2& size) {
    return ImGui::Button(label, size);
}

bool UI::StyledButton(const char* label, ButtonStyle style, const ImVec2& size) {
    PushButtonStyle(style);
    bool result = ImGui::Button(label, size);
    PopButtonStyle();
    return result;
}

void UI::Text(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

void UI::TextColored(const ImVec4& color, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ImGui::TextColoredV(color, fmt, args);
    va_end(args);
}

void UI::Separator(const char* label) {
    if (label) {
        ImGui::SeparatorText(label);
    } else {
        ImGui::Separator();
    }
}

void UI::SameLine() {
    ImGui::SameLine();
}

void UI::Spacing() {
    ImGui::Spacing();
}

bool UI::TreeNode(const char* label) {
    return ImGui::TreeNode(label);
}

void UI::TreePop() {
    ImGui::TreePop();
}

bool UI::CollapsingHeader(const char* label, int flags) {
    return ImGui::CollapsingHeader(label, flags);
}

bool UI::InputText(const char* label, char* buf, size_t bufSize, int flags) {
    return ImGui::InputText(label, buf, bufSize, flags);
}

bool UI::InputFloat(const char* label, float* v, float step, float step_fast) {
    return ImGui::InputFloat(label, v, step, step_fast);
}

bool UI::InputInt(const char* label, int* v, int step, int step_fast) {
    return ImGui::InputInt(label, v, step, step_fast);
}

bool UI::DragFloat(const char* label, float* v, float speed, float min, float max) {
    return ImGui::DragFloat(label, v, speed, min, max);
}

bool UI::DragFloat3(const char* label, float v[3], float speed) {
    return ImGui::DragFloat3(label, v, speed);
}

bool UI::Checkbox(const char* label, bool* v) {
    return ImGui::Checkbox(label, v);
}

void UI::ShowDemoWindow(bool* p_open) {
    ImGui::ShowDemoWindow(p_open);
}

} // namespace tinyUI
