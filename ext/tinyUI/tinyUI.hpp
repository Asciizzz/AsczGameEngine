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

namespace detail {
    inline IBackend* backend = nullptr;

    inline void applyDefaultTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        
        // Colors
        style.Colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.65f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.40f, 0.80f, 1.00f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.40f, 0.60f, 0.90f, 1.00f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.30f, 0.55f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.40f, 0.55f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.45f, 0.45f, 0.50f, 0.55f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.26f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.50f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.40f, 0.40f, 0.40f, 0.80f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

        // Sizes and rounding
        style.ScrollbarSize = 8.0f;
        style.ScrollbarRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.GrabRounding = 0.0f;
        style.WindowRounding = 0.0f;
        style.WindowBorderSize = 1.0f;

        ImGui::GetIO().FontGlobalScale = 1.4f;
    }
}

inline ImGuiStyle& Style() {
    return ImGui::GetStyle();
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

    detail::applyDefaultTheme();
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
