#pragma once

#include "imgui.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tinyUI {

// -----------------------------
// Backend
// -----------------------------
struct BackendInit {
    void* windowHandle = nullptr;
    void* deviceHandle = nullptr;
    void* extraData = nullptr;
};

struct IBackend {
    virtual ~IBackend() = default;
    virtual void init(const BackendInit& info) = 0;
    virtual void newFrame() = 0;
    virtual void renderDrawData(ImDrawData* drawData) = 0;
    virtual void shutdown() = 0;
    virtual void onResize(uint32_t width, uint32_t height) { (void)width; (void)height; }
    virtual const char* getName() const = 0;
};

struct ThemeStruct {
    ImVec4 windowBg = ImVec4(0.00f, 0.00f, 0.00f, 0.65f);
    ImVec4 childBg = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    ImVec4 border = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
    ImVec4 titleBg = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    ImVec4 titleBgActive = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    ImVec4 titleBgCollapsed = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    ImVec4 text = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    ImVec4 textDisabled = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    ImVec4 button = ImVec4(0.20f, 0.40f, 0.80f, 1.00f);
    ImVec4 buttonHovered = ImVec4(0.40f, 0.60f, 0.90f, 1.00f);
    ImVec4 buttonActive = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
    ImVec4 header = ImVec4(0.25f, 0.25f, 0.30f, 0.55f);
    ImVec4 headerHovered = ImVec4(0.35f, 0.35f, 0.40f, 0.55f);
    ImVec4 headerActive = ImVec4(0.45f, 0.45f, 0.50f, 0.55f);
    ImVec4 frameBg = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    ImVec4 frameBgHovered = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    ImVec4 frameBgActive = ImVec4(0.24f, 0.24f, 0.26f, 1.00f);
    ImVec4 scrollbarBg = ImVec4(0.10f, 0.10f, 0.10f, 0.50f);
    ImVec4 scrollbarGrab = ImVec4(0.40f, 0.40f, 0.40f, 0.80f);
    ImVec4 scrollbarGrabHovered = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    ImVec4 scrollbarGrabActive = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

    float scrollbarSize = 8.0f;
    float scrollbarRounding = 0.0f;
    float frameRounding = 0.0f;
    float childRounding = 0.0f;
    float buttonRounding = 0.0f;
    float windowRounding = 0.0f;
    float windowBorderSize = 1.0f;
    float fontScale = 1.4f;

    inline void apply() const {
        ImGuiStyle& style = ImGui::GetStyle();
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

namespace detail {
    inline IBackend* backend = nullptr;
    inline ThemeStruct Theme;
}

inline ThemeStruct& Theme() {
    return detail::Theme;
}


inline bool Init(IBackend* b, void* windowHandle = nullptr) {
    if (!b) return false;
    detail::backend = b;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    BackendInit info;
    info.windowHandle = windowHandle;
    detail::backend->init(info);

    detail::Theme.apply();
    return true;
}

inline void Shutdown() {
    if (!detail::backend) return;
    detail::backend->shutdown();
    detail::backend = nullptr;
    ImGui::DestroyContext();
}

inline void NewFrame() {
    if (!detail::backend) return;
    detail::backend->newFrame();
    ImGui::NewFrame();
}

inline void Render() {
    if (!detail::backend) return;
    ImGui::Render();
    detail::backend->renderDrawData(ImGui::GetDrawData());
}

inline bool Begin(const char* name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0) {
    bool result = ImGui::Begin(name, p_open, flags);

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

inline void End() { ImGui::End(); }

} // namespace tinyUI
