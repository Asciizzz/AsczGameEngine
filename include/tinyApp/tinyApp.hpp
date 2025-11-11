#pragma once

#include "tinySystem/tinyChrono.hpp"
#include "tinySystem/tinyWindow.hpp"
#include "tinySystem/tinyImGui.hpp"

#include "tinyVk/System/Device.hpp"
#include "tinyVk/System/Instance.hpp"

#include "tinyVk/Render/Swapchain.hpp"
#include "tinyVk/Render/RenderPass.hpp"
#include "tinyVk/Render/DepthImage.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include "tinyVk/Render/Renderer.hpp"
#include "tinyVk/Pipeline/Pipeline_include.hpp"

#include "tinyEngine/tinyProject.hpp"
#include <unordered_set>
#include <vector>
#include <filesystem>

// Unified selection handle for both scene nodes and file system nodes
struct SelectHandle {
    enum class Type { Scene, File };
    
    tinyHandle handle;
    Type type;
    
    SelectHandle() : handle(), type(Type::Scene) {}
    SelectHandle(tinyHandle h, Type t) : handle(h), type(t) {}
    
    bool valid() const { return handle.valid(); }
    bool isScene() const { return type == Type::Scene; }
    bool isFile() const { return type == Type::File; }
    
    void clear() { handle = tinyHandle(); }
    
    bool operator==(const SelectHandle& other) const {
        return handle == other.handle && type == other.type;
    }
    bool operator!=(const SelectHandle& other) const {
        return !(*this == other);
    }
};

// File dialog state
struct FileDialog {
    bool isOpen = false;
    bool justOpened = false;  // Flag to track when we need to call ImGui::OpenPopup
    bool shouldClose = false; // Flag to handle delayed closing
    std::filesystem::path currentPath;
    std::vector<std::filesystem::directory_entry> currentFiles;
    std::string selectedFile;
    tinyHandle targetFolder;
    
    void open(const std::filesystem::path& startPath, tinyHandle folder);
    void close();
    void update();
    void refreshFileList();
    bool isModelFile(const std::filesystem::path& path);
};

struct LoadScriptDialog {
    bool isOpen = false;
    bool justOpened = false;
    bool shouldClose = false;
    std::filesystem::path currentPath;
    std::vector<std::filesystem::directory_entry> currentFiles;
    std::string selectedFile;
    tinyHandle targetFolder;
    
    void open(const std::filesystem::path& startPath, tinyHandle folder);
    void close();
    void update();
    void refreshFileList();
    bool isLuaFile(const std::filesystem::path& path);
};

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
    UniquePtr<tinyVk::Device> deviceVk;

    UniquePtr<tinyVk::Renderer> renderer;

    UniquePtr<tinyVk::PipelineManager> pipelineManager;

    UniquePtr<tinyProject> project; // New gigachad system

    // Window metadata
    const char* appTitle;
    uint32_t appWidth;
    uint32_t appHeight;

    // Functions that actually do things
    void initComponents();
    void mainLoop();

    bool checkWindowResize();
};
