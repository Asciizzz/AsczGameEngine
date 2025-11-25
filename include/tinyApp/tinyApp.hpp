#pragma once

#include "tinySystem/tinyChrono.hpp"
#include "tinySystem/tinyWindow.hpp"

#include "tinyUI/tinyUI.hpp"
#include "tinyUI/tinyUI_Vulkan.hpp"

#include "tinyVk/System/Device.hpp"
#include "tinyVk/System/Instance.hpp"

#include "tinyVk/Render/Swapchain.hpp"
#include "tinyVk/Render/RenderPass.hpp"
#include "tinyVk/Render/DepthImage.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include "tinyVk/Render/Renderer.hpp"
#include "tinyVk/Pipeline/Pipeline_raster.hpp"

#include "tinyEngine/tinyProject.hpp"
#include <unordered_set>
#include <vector>
#include <filesystem>

#include <fstream>
#include <sstream>

class tinyApp {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    tinyApp(const char* title = "Vulkan tinyApp", uint32_t width = 800, uint32_t height = 600);
    ~tinyApp();

    // Modern C++ says no sharing
    tinyApp(const tinyApp&) = delete;
    tinyApp& operator=(const tinyApp&) = delete;

    void run();

private:
    UniquePtr<tinyWindow> windowManager;
    UniquePtr<tinyChrono> fpsManager;

    UniquePtr<tinyVk::Instance> instanceVk;
    UniquePtr<tinyVk::Device> dvk;

    UniquePtr<tinyVk::Renderer> renderer;

    UniquePtr<tinyVk::PipelineRaster> pipelineSky;
    UniquePtr<tinyVk::PipelineRaster> pipelineTest;

    UniquePtr<tinyProject> project; // New gigachad system

    rtScene* curScene = nullptr;

    // tinyUI
    tinyUI::Backend_Vulkan* UIBackend = nullptr;
    tinyVk::SamplerVk uiSampler; // Helpful


    // Window metadata
    const char* appTitle;
    uint32_t appWidth;
    uint32_t appHeight;

    // Functions that actually do things
    void initComponents();

    void mainLoop();

    void updateActiveScene();

    bool checkWindowResize();
    
    // UI rendering - implemented in tinyApp_imgui.cpp
    void initUI();
    void renderUI();
};
