#include "TinySystem/Application.hpp"

#include "TinyEngine/TinyLoader.hpp"

#include <iostream>
#include <random>
#include <filesystem>
#include <string>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

#ifdef NDEBUG // Remember to set this to false
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = true;
#endif

using namespace TinyVK;

Application::Application(const char* title, uint32_t width, uint32_t height)
    : appTitle(title), appWidth(width), appHeight(height) {

    initComponents();
    featuresTestingGround();
}

Application::~Application() {
    if (imguiWrapper) {
        imguiWrapper->cleanup();
    }
    cleanup();
}

void Application::run() {
    mainLoop();

    printf("Application exited successfully. See you next time!\n");
}

void Application::initComponents() {

    windowManager = MakeUnique<TinyWindow>(appTitle, appWidth, appHeight);
    fpsManager = MakeUnique<TinyChrono>();

    auto extensions = windowManager->getRequiredVulkanExtensions();
    instanceVK = MakeUnique<Instance>(extensions, enableValidationLayers);
    instanceVK->createSurface(windowManager->window);

    deviceVK = MakeUnique<Device>(instanceVK->instance, instanceVK->surface);

    // So we dont have to write these things over and over again
    VkDevice device = deviceVK->device;
    VkPhysicalDevice pDevice = deviceVK->pDevice;

    // Create renderer (which now manages depth manager, swap chain and render passes)
    renderer = MakeUnique<Renderer>(
        deviceVK.get(),
        instanceVK->surface,
        windowManager->window,
        Application::MAX_FRAMES_IN_FLIGHT
    );

    project = MakeUnique<TinyProject>(deviceVK.get());

    // Initialize selected node to root
    selectedSceneNodeHandle = project->getRootNodeHandle();

    float aspectRatio = static_cast<float>(appWidth) / static_cast<float>(appHeight);
    project->getCamera()->setAspectRatio(aspectRatio);

// PLAYGROUND FROM HERE

    // Load all models from Assets directory recursively
    loadAllAssetsRecursively("Assets");

    auto glbLayout = project->getGlbDescSetLayout();
    auto matLayout = VK_NULL_HANDLE; // Placeholder until we have a material UBO
    auto texLayout = VK_NULL_HANDLE; // Placeholder until we have a texture UBO

    pipelineManager = MakeUnique<PipelineManager>();
    pipelineManager->loadPipelinesFromJson("Config/pipelines.json");

    // Initialize all pipelines with the manager using named layouts
    UnorderedMap<std::string, VkDescriptorSetLayout> namedLayouts = {
        {"global", glbLayout}
    };
    
    // Create named vertex inputs
    UnorderedMap<std::string, VertexInputVK> vertexInputVKs;
    
    // None - no vertex input (for fullscreen quads, etc.)
    vertexInputVKs["None"] = VertexInputVK();

    auto vstaticLayout = TinyVertexStatic::getLayout();
    auto vstaticBind = vstaticLayout.getBindingDescription();
    auto vstaticAttrs = vstaticLayout.getAttributeDescriptions();

    auto vriggedLayout = TinyVertexRig::getLayout();
    auto vriggedBind = vriggedLayout.getBindingDescription();
    auto vriggedAttrs = vriggedLayout.getAttributeDescriptions();

    vertexInputVKs["Test"] = VertexInputVK()
        .setBindings({ vriggedBind })
        .setAttributes({ vriggedAttrs });
    
    // Use offscreen render pass for pipeline creation
    VkRenderPass offscreenRenderPass = renderer->getOffscreenRenderPass();
    PIPELINE_INIT(pipelineManager.get(), device, offscreenRenderPass, namedLayouts, vertexInputVKs);

    // Load post-process effects from JSON configuration
    renderer->loadPostProcessEffectsFromJson("Config/postprocess.json");

    // Initialize ImGui - do this after renderer is fully set up
    imguiWrapper = MakeUnique<TinyImGui>();
    
    // ImGui now creates its own render pass using swapchain and depth info
    bool imguiInitSuccess = imguiWrapper->init(
        windowManager->window,
        instanceVK->instance,
        deviceVK.get(),
        renderer->getSwapChain(),
        renderer->getDepthManager()
    );
    
    if (imguiInitSuccess) {
        renderer->setupImGuiRenderTargets(imguiWrapper.get());

        setupImGuiWindows(*fpsManager, *project->getCamera(), true, 0.0f);
    } else {
        std::cerr << "Failed to initialize ImGui!" << std::endl;
    }
}

void Application::featuresTestingGround() {}

bool Application::checkWindowResize() {
    if (!windowManager->resizedFlag && !renderer->isResizeNeeded()) return false;

    windowManager->resizedFlag = false;
    renderer->setResizeHandled();

    int newWidth, newHeight;
    SDL_GetWindowSize(windowManager->window, &newWidth, &newHeight);

    project->getCamera()->updateAspectRatio(newWidth, newHeight);

    // Handle window resize in renderer (now handles depth resources internally)
    renderer->handleWindowResize(windowManager->window);

    // Update ImGui render pass after renderer recreates render passes  
    if (imguiWrapper) {
        imguiWrapper->updateRenderPass(renderer->getSwapChain(), renderer->getDepthManager());
        // Set up ImGui render targets with updated framebuffers
        renderer->setupImGuiRenderTargets(imguiWrapper.get());
    }

    // Recreate all pipelines with offscreen render pass for post-processing
    VkRenderPass offscreenRenderPass = renderer->getOffscreenRenderPass();
    pipelineManager->recreateAllPipelines(offscreenRenderPass);

    return true;
}


void Application::mainLoop() {
    SDL_SetRelativeMouseMode(SDL_TRUE); // Start with mouse locked

    // Create references to avoid arrow spam
    auto& winManager = *windowManager;
    auto& fpsRef = *fpsManager;

    auto& camRef = *project->getCamera();

    auto& rendererRef = *renderer;

    while (!winManager.shouldCloseFlag) {
        // Update FPS manager for timing
        fpsRef.update();
        
        // Handle SDL events for both window manager and ImGui
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Let ImGui process the event first
            imguiWrapper->processEvent(&event);
            
            // Then handle our own events
            switch (event.type) {
                case SDL_QUIT:
                    winManager.shouldCloseFlag = true;
                    break;
                case SDL_WINDOWEVENT:
                    if (SDL_GetWindowFromID(event.window.windowID) == winManager.window) {
                        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                            winManager.resizedFlag = true;
                        } else if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                            winManager.shouldCloseFlag = true;
                        }
                    }
                    break;
            }
        }

        float dTime = fpsRef.deltaTime;

        static float cam_dist = 1.5f;
        static glm::vec3 camPos = camRef.pos;

        // Check if window was resized or renderer needs to be updated
        checkWindowResize();

        const Uint8* k_state = SDL_GetKeyboardState(nullptr);
        if (k_state[SDL_SCANCODE_ESCAPE]) {
            winManager.shouldCloseFlag = true;
            break;
        }

        // Toggle fullscreen with F11 key
        static bool fullscreenPressed = false;
        static SDL_Scancode fullscreenKey = SDL_SCANCODE_F11;
        if (k_state[fullscreenKey] && !fullscreenPressed) {
            winManager.toggleFullscreen();

            fullscreenPressed = true;
        } else if (!k_state[fullscreenKey]) {
            fullscreenPressed = false;
        }
        
        // Toggle mouse lock with F1 key
        static bool mouseFocusPressed = false;
        static bool mouseFocus = true;
        static SDL_Scancode mouseFocusKey = SDL_SCANCODE_Q;
        if (k_state[mouseFocusKey] && !mouseFocusPressed) {
            mouseFocus = !mouseFocus;
            if (mouseFocus) {
                SDL_SetRelativeMouseMode(SDL_TRUE);
                SDL_WarpMouseInWindow(winManager.window, 0, 0);
            } else {
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            mouseFocusPressed = true;
        } else if (!k_state[mouseFocusKey]) {
            mouseFocusPressed = false;
        }

        // Handle mouse look when focused
        if (mouseFocus) {
            int mouseX, mouseY;
            SDL_GetRelativeMouseState(&mouseX, &mouseY);

            float sensitivity = 0.02f;
            float yawDelta = -mouseX * sensitivity;  // Inverted for correct quaternion rotation
            float pitchDelta = -mouseY * sensitivity;

            camRef.rotate(pitchDelta, yawDelta, 0.0f);
        }
    
// ======== PLAYGROUND HERE! ========

        // Press

        // Camera movement controls
        bool fast = k_state[SDL_SCANCODE_LSHIFT] && !k_state[SDL_SCANCODE_LCTRL];
        bool slow = k_state[SDL_SCANCODE_LCTRL] && !k_state[SDL_SCANCODE_LSHIFT];
        float p_speed = (fast ? 26.0f : (slow ? 0.5f : 8.0f)) * dTime;

        if (k_state[SDL_SCANCODE_W]) camPos += camRef.forward * p_speed;
        if (k_state[SDL_SCANCODE_S]) camPos -= camRef.forward * p_speed;
        if (k_state[SDL_SCANCODE_A]) camPos -= camRef.right * p_speed;
        if (k_state[SDL_SCANCODE_D]) camPos += camRef.right * p_speed;

        camRef.pos = camPos;

// =================================

        project->runPlayground(dTime);

        imguiWrapper->newFrame();

        project->getGlobal()->update(camRef, rendererRef.getCurrentFrame());

        uint32_t imageIndex = rendererRef.beginFrame();
        if (imageIndex != UINT32_MAX) {
            // Update global UBO buffer from frame index
            uint32_t currentFrameIndex = rendererRef.getCurrentFrame();

            rendererRef.drawSky(project.get(), PIPELINE_INSTANCE(pipelineManager.get(), "Sky"));

            rendererRef.drawScene(project.get(), PIPELINE_INSTANCE(pipelineManager.get(), "Test"));

            // End frame with ImGui rendering integrated
            rendererRef.endFrame(imageIndex, imguiWrapper.get());
            
            // Process any pending deletions after the frame is complete
            processPendingDeletions();
        };

        // Clean window title - FPS info now in ImGui
        static bool titleSet = false;
        if (!titleSet) {
            SDL_SetWindowTitle(winManager.window, appTitle);
            titleSet = true;
        }
    }

    vkDeviceWaitIdle(deviceVK->device);
}

void Application::setupImGuiWindows(const TinyChrono& fpsManager, const TinyCamera& camera, bool mouseFocus, float deltaTime) {
    // Clear any existing windows first
    imguiWrapper->clearWindows();
    
    // Main Editor Window - full size, no scroll bars
    const TinyRegistry& registry = project->registryRef();
    TinyFS& fs = project->filesystem();

    imguiWrapper->addWindow("Editor", [this, &camera, &registry, &fs]() {
        // Static variable to store splitter position (persists between frames)
        static float splitterPos = 0.5f; // Start with 50% for hierarchy, 50% for file explorer
        
        // Get full available space
        float totalHeight = ImGui::GetContentRegionAvail().y;
        
        // Calculate section heights based on splitter position
        float hierarchyHeight = totalHeight * splitterPos;
        float explorerHeight = totalHeight * (1.0f - splitterPos);
        
        // =============================================================================
        // HIERARCHY SECTION (TOP)
        // =============================================================================
        
        // Active Scene Name Header
        TinyRScene* activeScene = project->getActiveScene();
        if (activeScene) {
            ImGui::Text("Active Scene: %s", activeScene->name.c_str());
            
            // Show full scene info on hover
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Scene: %s", activeScene->name.c_str());
                ImGui::Text("Total Nodes: %u", activeScene->nodes.count());
                ImGui::Text("Handle: %u_v%u", project->getActiveSceneHandle().index, project->getActiveSceneHandle().version);
                ImGui::EndTooltip();
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "No Active Scene");
        }
        
        ImGui::Separator();
        
        // Hierarchy Tree with background
        ImGui::BeginChild("Hierarchy", ImVec2(0, hierarchyHeight - 30), ImGuiChildFlags_Borders);
        
        // Clear filesystem selection when clicking in node tree area
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
            selectedFNodeHandle = TinyHandle();
        }
        
        if (activeScene && activeScene->nodes.count() > 0) {
            project->renderSelectableNodeTreeImGui(project->getRootNodeHandle(), selectedSceneNodeHandle);
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No active scene");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drag scenes here to create instances");
        }
        ImGui::EndChild();
        
        // =============================================================================
        // HORIZONTAL SPLITTER
        // =============================================================================
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.7f, 0.7f, 0.8f));
        
        ImGui::Button("##HorizontalSplitter", ImVec2(-1, 4));
        
        if (ImGui::IsItemActive()) {
            float mouseDelta = ImGui::GetIO().MouseDelta.y;
            splitterPos += mouseDelta / totalHeight;
            splitterPos = std::max(0.2f, std::min(0.8f, splitterPos)); // Clamp between 20% and 80%
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        
        ImGui::PopStyleColor(3);
        
        // =============================================================================
        // FILE EXPLORER SECTION (BOTTOM)
        // =============================================================================
        
        ImGui::Text("File Explorer");
        ImGui::Separator();
        
        // File Explorer with background
        ImGui::BeginChild("FileExplorer", ImVec2(0, explorerHeight - 30), ImGuiChildFlags_Borders);
        
        // Clear scene node selection when clicking in file tree area (but not on items)
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
            selectedSceneNodeHandle = TinyHandle();
        }
        
        renderSceneFolderTree(fs, fs.rootHandle());
        ImGui::EndChild();
        
    });  // End Editor window
    
    // Debug Panel Window (smaller, optional)
    imguiWrapper->addWindow("Debug Panel", [this, &fpsManager, &camera, mouseFocus, deltaTime]() {
        // FPS and Performance (update display only every second)
        static auto lastFPSUpdate = std::chrono::steady_clock::now();
        static float displayFPS = 0.0f;
        static float displayFrameTime = 0.0f;
        static float displayAvgFPS = 0.0f;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFPSUpdate);
        
        if (elapsed.count() >= 1000) { // Update every 1000ms (1 second)
            displayFPS = fpsManager.currentFPS;
            displayFrameTime = fpsManager.frameTimeMs;
            displayAvgFPS = fpsManager.getAverageFPS();
            lastFPSUpdate = now;
        }
        
        ImGui::Text("Performance");
        ImGui::Separator();
        ImGui::Text("FPS: %.1f (%.2f ms)", displayFPS, displayFrameTime);
        ImGui::Text("Avg FPS: %.1f", displayAvgFPS);
        ImGui::Text("Delta Time: %.4f s", deltaTime);
        
        ImGui::Spacing();
        
        // Camera Information
        ImGui::Text("Camera");
        ImGui::Separator();
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", camera.pos.x, camera.pos.y, camera.pos.z);
        ImGui::Text("Forward: (%.2f, %.2f, %.2f)", camera.forward.x, camera.forward.y, camera.forward.z);
        ImGui::Text("Right: (%.2f, %.2f, %.2f)", camera.right.x, camera.right.y, camera.right.z);
        ImGui::Text("Up: (%.2f, %.2f, %.2f)", camera.up.x, camera.up.y, camera.up.z);
        ImGui::Text("Yaw: %.2f° | Pitch: %.2f° | Roll: %.2f°", 
                   camera.getYaw(true) * 57.2958f, // Convert radians to degrees
                   camera.getPitch(true) * 57.2958f,
                   camera.getRoll() * 57.2958f);

        ImGui::Spacing();
        
        // Window controls
        ImGui::Text("Windows");
        ImGui::Separator();
        ImGui::Checkbox("Show Inspector", &showInspectorWindow);
        ImGui::Checkbox("Show Editor Settings", &showEditorSettingsWindow);
    }, &showDebugWindow);
    
    // Inspector Window
    imguiWrapper->addWindow("Inspector", [this]() {
        renderInspectorWindow();
    }, &showInspectorWindow);
    
    // Editor Settings Window
    imguiWrapper->addWindow("Editor Settings", [this]() {
        ImGui::Text("UI & Display");
        ImGui::Separator();
        
        // Font scaling for better readability
        static float fontScale = 1.0f;
        ImGui::Text("Font Scale");
        if (ImGui::SliderFloat("##FontScale", &fontScale, 0.5f, 3.0f, "%.1fx")) {
            ImGui::GetIO().FontGlobalScale = fontScale;
        }

        ImGui::Spacing();
        
        // Quick preset buttons
        ImGui::Text("Presets:");
        if (ImGui::Button("Small##FontPreset")) {
            fontScale = 0.8f;
            ImGui::GetIO().FontGlobalScale = fontScale;
        }
        ImGui::SameLine();
        if (ImGui::Button("Normal##FontPreset")) {
            fontScale = 1.0f;
            ImGui::GetIO().FontGlobalScale = fontScale;
        }
        ImGui::SameLine();
        if (ImGui::Button("Large##FontPreset")) {
            fontScale = 1.5f;
            ImGui::GetIO().FontGlobalScale = fontScale;
        }
        ImGui::SameLine();
        if (ImGui::Button("XL##FontPreset")) {
            fontScale = 2.0f;
            ImGui::GetIO().FontGlobalScale = fontScale;
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Future settings placeholder
        ImGui::TextDisabled("More settings will be added here...");
        ImGui::TextDisabled("• Theme selection");
        ImGui::TextDisabled("• Window layout presets"); 
        ImGui::TextDisabled("• Performance options");
        ImGui::TextDisabled("• Keybind customization");
    }, &showEditorSettingsWindow);
}

void Application::loadAllAssetsRecursively(const std::string& assetsPath) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(assetsPath) || !fs::is_directory(assetsPath)) {
        std::cerr << "Assets directory not found: " << assetsPath << std::endl;
        return;
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(assetsPath)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                std::string extension = entry.path().extension().string();
                
                // Convert to lowercase for case-insensitive comparison
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == ".glb" || extension == ".gltf") {
                    try {
                        TinyModel model = TinyLoader::loadModel(filePath, false);
                        TinyHandle sceneHandle = project->addSceneFromModel(model);
                        // Note: addSceneFromModel already creates the proper folder structure in TinyFS
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to load model " << filePath << ": " << e.what() << std::endl;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning assets directory: " << e.what() << std::endl;
    }
}

void Application::renderSceneFolderTree(TinyFS& fs, TinyHandle folderHandle, int depth) {
    const TinyFNode* folder = fs.getFNodes().get(folderHandle);
    if (!folder) return;
    
    // Skip root node display
    if (folderHandle != fs.rootHandle()) {
        ImGui::PushID(folderHandle.index);
        
        // Check if this folder is selected
        bool isFolderSelected = (selectedFNodeHandle == folderHandle);
        
        // Use Selectable for consistent highlighting with files
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // White text
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f)); // Hover background
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Gray selection background
        
        bool nodeOpen = ImGui::TreeNodeEx(folder->name.c_str(), 
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
            (isFolderSelected ? ImGuiTreeNodeFlags_Selected : 0));
        
        ImGui::PopStyleColor(3);
        
        // Handle folder selection
        if (ImGui::IsItemClicked()) {
            selectedFNodeHandle = folderHandle;
            // Keep scene node selection when selecting files/folders
        }
        
        // Drag source for folders
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("FOLDER_HANDLE", &folderHandle, sizeof(TinyHandle));
            ImGui::Text("Moving folder: %s", folder->name.c_str());
            ImGui::EndDragDropSource();
        }
        
        // Drag drop target for folders and files
        if (ImGui::BeginDragDropTarget()) {
            // Accept folders being dropped
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FOLDER_HANDLE")) {
                TinyHandle draggedFolder = *(const TinyHandle*)payload->Data;
                if (draggedFolder != folderHandle) { // Can't drop folder on itself
                    fs.moveFNode(draggedFolder, folderHandle);
                }
            }
            
            // Accept files being dropped  
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                TinyHandle draggedFile = *(const TinyHandle*)payload->Data;
                fs.moveFNode(draggedFile, folderHandle);
            }
            
            ImGui::EndDragDropTarget();
        }
        
        if (nodeOpen) {
            // Render children with proper indentation
            for (TinyHandle childHandle : folder->children) {
                const TinyFNode* child = fs.getFNodes().get(childHandle);
                if (!child || child->isHidden()) continue; // Skip invalid or hidden nodes

                if (child->type == TinyFNode::Type::Folder) {
                    renderSceneFolderTree(fs, childHandle, depth + 1);
                } else if (child->type == TinyFNode::Type::File) {
                    // Add indentation for files within folders
                    ImGui::Indent();
                    
                    ImGui::PushID(childHandle.index);
                    
                    bool isSelected = (selectedFNodeHandle == childHandle);
                    
                    if (child->tHandle.isType<TinyRScene>()) {
                        // This is a scene file - get the actual scene data from registry
                        TinyRScene* scene = static_cast<TinyRScene*>(fs.registryRef().get(child->tHandle));
                        if (scene) {
                            std::string sceneName = child->name;
                            
                            // Set colors for scene files: gray text, gray selection background
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Gray text
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f)); // Subtle hover background
                            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Gray selection background
                            
                            if (ImGui::Selectable(sceneName.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                                // Single click to select file
                                selectedFNodeHandle = childHandle;
                                // Keep scene node selection when selecting files
                                
                                if (ImGui::IsMouseDoubleClicked(0) && selectedSceneNodeHandle.valid()) {
                                    // Double-click to place scene at selected node
                                    TinyHandle sceneRegistryHandle = child->tHandle.handle;
                                    project->addSceneInstance(sceneRegistryHandle, selectedSceneNodeHandle);
                                    if (TinyRScene* scene = project->getActiveScene()) scene->updateGlbTransform();
                                }
                            }
                            
                            // Override text color to white when hovered
                            if (ImGui::IsItemHovered()) {
                                ImGui::SameLine();
                                ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::CalcTextSize(sceneName.c_str()).x - ImGui::GetStyle().ItemSpacing.x);
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // White on hover
                                ImGui::Text("%s", sceneName.c_str());
                                ImGui::PopStyleColor();
                            }
                            
                            ImGui::PopStyleColor(3); // Pop all three colors
                            
                            // Drag source for scene files - use FILE_HANDLE but scene targets will handle instantiation
                            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                                // Use FILE_HANDLE as the primary payload since it works for both operations
                                ImGui::SetDragDropPayload("FILE_HANDLE", &childHandle, sizeof(TinyHandle));
                                ImGui::Text("Scene: %s", child->name.c_str());
                                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drop on folders to move");
                                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drop on nodes to instantiate");
                                ImGui::EndDragDropSource();
                            }
                            
                            // Context menu for scene files
                            if (ImGui::BeginPopupContextItem()) {
                                // Get scene handle from TypeHandle
                                TinyHandle sceneRegistryHandle = child->tHandle.handle;
                                bool isCurrentlyActive = (project->getActiveSceneHandle() == sceneRegistryHandle);
                                
                                if (isCurrentlyActive) {
                                    ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "Active Scene");
                                } else {
                                    if (ImGui::MenuItem("Make Active Scene")) {
                                        if (project->setActiveScene(sceneRegistryHandle)) {
                                            selectedSceneNodeHandle = project->getRootNodeHandle(); // Reset node selection
                                        }
                                    }
                                }
                                
                                ImGui::Separator();
                                
                                if (ImGui::MenuItem("Duplicate Scene")) {
                                    // TODO: Implement scene duplication
                                }
                                
                                if (child->cfg.deletable && ImGui::MenuItem("Delete")) {
                                    // TODO: Add deletion confirmation and logic
                                }
                                
                                ImGui::EndPopup();
                            }
                            
                            // Tooltip with scene info
                            if (ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::Text("Scene: %s", scene->name.c_str());
                                ImGui::Text("Nodes: %u", scene->nodes.count());
                                ImGui::EndTooltip();
                            }
                        }
                    } else {
                        // Other file types - display with hover effect but not interactive
                        std::string fileName = child->name;
                        
                        // Set colors for other files: gray text, gray selection background
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Gray text
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.3f)); // Subtle hover background
                        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Gray selection background
                        
                        ImGui::Selectable(fileName.c_str(), isSelected, ImGuiSelectableFlags_None);
                        
                        // Handle file selection
                        if (ImGui::IsItemClicked()) {
                            selectedFNodeHandle = childHandle;
                            // Keep scene node selection when selecting files
                        }
                        
                        ImGui::PopStyleColor(3); // Pop all three colors
                        
                        // Drag source for other files
                        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                            ImGui::SetDragDropPayload("FILE_HANDLE", &childHandle, sizeof(TinyHandle));
                            ImGui::Text("Moving file: %s", child->name.c_str());
                            ImGui::EndDragDropSource();
                        }
                        
                        // Tooltip with file info
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            // Show file type based on TypeHandle
                            if (child->tHandle.isType<TinyRTexture>()) {
                                ImGui::Text("Type: Texture");
                            } else if (child->tHandle.isType<TinyRMaterial>()) {
                                ImGui::Text("Type: Material");
                            } else if (child->tHandle.isType<TinyRMesh>()) {
                                ImGui::Text("Type: Mesh");
                            } else if (child->tHandle.isType<TinyRSkeleton>()) {
                                ImGui::Text("Type: Skeleton");
                            } else {
                                ImGui::Text("Type: Asset");
                            }
                            ImGui::EndTooltip();
                        }
                    }
                    
                    ImGui::PopID();
                    ImGui::Unindent();
                }
            }
            ImGui::TreePop();
        }
        
        ImGui::PopID();
    } else {
        // Root folder - just render children without tree node but with consistent styling
        for (TinyHandle childHandle : folder->children) {
            const TinyFNode* child = fs.getFNodes().get(childHandle);
            if (!child || child->isHidden()) continue; // Skip invalid or hidden nodes

            if (child->type == TinyFNode::Type::Folder) {
                renderSceneFolderTree(fs, childHandle, depth);
            } else if (child->type == TinyFNode::Type::File) {
                ImGui::PushID(childHandle.index);
                
                bool isSelected = (selectedFNodeHandle == childHandle);
                
                if (child->tHandle.isType<TinyRScene>()) {
                    // Root-level scene files
                    TinyRScene* scene = static_cast<TinyRScene*>(fs.registryRef().get(child->tHandle));
                    if (scene) {
                        std::string sceneName = child->name;
                        
                        // Set colors for scene files: gray text, gray selection background
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Gray text
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f)); // Subtle hover background
                        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Gray selection background
                        
                        if (ImGui::Selectable(sceneName.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                            // Single click to select file
                            selectedFNodeHandle = childHandle;
                            // Keep scene node selection when selecting files
                            
                            if (ImGui::IsMouseDoubleClicked(0) && selectedSceneNodeHandle.valid()) {
                                // Double-click placement - use registry handle from TypeHandle
                                TinyHandle sceneRegistryHandle = child->tHandle.handle;
                                project->addSceneInstance(sceneRegistryHandle, selectedSceneNodeHandle);
                                if (TinyRScene* scene = project->getActiveScene()) scene->updateGlbTransform();
                            }
                        }
                        
                        ImGui::PopStyleColor(3); // Pop all three colors
                        
                        // Drag source for root scene files - use FILE_HANDLE but scene targets will handle instantiation
                        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                            // Use FILE_HANDLE as the primary payload since it works for both operations
                            ImGui::SetDragDropPayload("FILE_HANDLE", &childHandle, sizeof(TinyHandle));
                            ImGui::Text("Scene: %s", child->name.c_str());
                            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drop on folders to move");
                            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drop on nodes to instantiate");
                            ImGui::EndDragDropSource();
                        }
                    }
                } else {
                    // Other root-level files
                    std::string fileName = child->name;
                    
                    // Set colors for other files: gray text, gray selection background
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Gray text
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.3f)); // Subtle hover background
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Gray selection background
                    
                    ImGui::Selectable(fileName.c_str(), isSelected, ImGuiSelectableFlags_None);
                    
                    // Handle file selection
                    if (ImGui::IsItemClicked()) {
                        selectedFNodeHandle = childHandle;
                        // Keep scene node selection when selecting files
                    }
                    
                    ImGui::PopStyleColor(3); // Pop all three colors
                    
                    // Drag source for other root files
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("FILE_HANDLE", &childHandle, sizeof(TinyHandle));
                        ImGui::Text("Moving file: %s", child->name.c_str());
                        ImGui::EndDragDropSource();
                    }
                }
                
                ImGui::PopID();
            }
        }
        
        // Add drag-drop target to root folder (invisible area at bottom)
        ImGui::InvisibleButton("RootDropTarget", ImVec2(-1, 20));
        if (ImGui::BeginDragDropTarget()) {
            // Accept folders being dropped to root
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FOLDER_HANDLE")) {
                TinyHandle draggedFolder = *(const TinyHandle*)payload->Data;
                fs.moveFNode(draggedFolder, fs.rootHandle());
            }
            
            // Accept files being dropped to root
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                TinyHandle draggedFile = *(const TinyHandle*)payload->Data;
                fs.moveFNode(draggedFile, fs.rootHandle());
            }
            
            ImGui::EndDragDropTarget();
        }
    }
}

void Application::renderInspectorWindow() {
    // Inspector Window: Node Inspector (top) + File Inspector (bottom) with splitter
    // Use exact same technique as Editor window to avoid scrollbars
    
    static float splitterPos = 0.6f; // Node inspector gets 60% by default
    
    // Get full available space (same as Editor window)
    float totalHeight = ImGui::GetContentRegionAvail().y;
    
    // Calculate section heights based on splitter position (same as Editor window)
    float nodeInspectorHeight = totalHeight * splitterPos;
    float fileInspectorHeight = totalHeight * (1.0f - splitterPos);
    
    // NODE INSPECTOR (TOP) - Always exists
    ImGui::Text("Node Inspector");
    ImGui::Separator();
    ImGui::BeginChild("NodeInspectorPane", ImVec2(0, nodeInspectorHeight - 30), ImGuiChildFlags_Borders);
    renderNodeInspector();
    ImGui::EndChild();
    
    // HORIZONTAL SPLITTER (exact same as Editor window)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.7f, 0.7f, 0.8f));
    
    ImGui::Button("##InspectorSplitter", ImVec2(-1, 4));
    
    if (ImGui::IsItemActive()) {
        float mouseDelta = ImGui::GetIO().MouseDelta.y;
        splitterPos += mouseDelta / totalHeight;
        splitterPos = std::max(0.2f, std::min(0.8f, splitterPos)); // Clamp between 20% and 80%
    }
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    
    ImGui::PopStyleColor(3);
    
    // FILE INSPECTOR (BOTTOM) - Always exists
    ImGui::Text("File Inspector");
    ImGui::Separator();
    ImGui::BeginChild("FileInspectorPane", ImVec2(0, fileInspectorHeight - 30), ImGuiChildFlags_Borders);
    renderFileSystemInspector();
    ImGui::EndChild();
}

void Application::renderNodeInspector() {
    TinyRScene* activeScene = project->getActiveScene();
    if (!activeScene) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No active scene");
        return;
    }

    if (!selectedSceneNodeHandle.valid()) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "No node selected");
        ImGui::Text("Select a scene node from the hierarchy.");
        return;
    }

    const TinyRNode* selectedNode = activeScene->nodes.get(selectedSceneNodeHandle);
    if (!selectedNode) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid node selection");
        selectedSceneNodeHandle = project->getRootNodeHandle();
        return;
    }

    bool isRootNode = (selectedSceneNodeHandle == project->getRootNodeHandle());

    // GENERAL INFO SECTION (non-scrollable)
    ImGui::Text("Node: %s", selectedNode->name.c_str());
    ImGui::Text("Handle: %u_v%u", selectedSceneNodeHandle.index, selectedSceneNodeHandle.version);
    
    // Show parent and children count
    if (selectedNode->parentHandle.valid()) {
        const TinyRNode* parentNode = activeScene->nodes.get(selectedNode->parentHandle);
        if (parentNode) {
            ImGui::Text("Parent: %s", parentNode->name.c_str());
        }
    } else {
        ImGui::Text("Parent: None (Root)");
    }
    ImGui::Text("Children: %zu", selectedNode->childrenHandles.size());
    
    ImGui::Separator();
    
    // SCROLLABLE COMPONENTS SECTION 
    float remainingHeight = ImGui::GetContentRegionAvail().y - 35; // Reserve space for add component dropdown
    ImGui::BeginChild("ComponentsScrollable", ImVec2(0, remainingHeight), false);
    
    // Transform component (dynamic sizing, no remove button)
    ImGui::Separator();
    ImGui::Text("Transform");
    ImGui::Indent();
    renderTransformInspector(selectedNode, isRootNode);
    ImGui::Unindent();
    ImGui::Separator();
    
    ImGui::Spacing();
    
    // Other components
    renderComponentsInspector(selectedNode);
    
    ImGui::EndChild();
    
    // ADD COMPONENT DROPDOWN at bottom
    renderAddComponentDropdown(selectedNode);
}

void Application::renderAddComponentDropdown(const TinyRNode* selectedNode) {
    if (!selectedNode) return;
    
    TinyRNode* mutableNode = const_cast<TinyRNode*>(selectedNode);
    
    // Check which components are missing
    std::vector<std::pair<std::string, TinyNode::Types>> missingComponents;
    
    if (!selectedNode->hasComponent<TinyNode::MeshRender>()) {
        missingComponents.push_back({"Mesh Renderer", TinyNode::Types::MeshRender});
    }
    if (!selectedNode->hasComponent<TinyNode::Skeleton>()) {
        missingComponents.push_back({"Skeleton", TinyNode::Types::Skeleton});
    }
    if (!selectedNode->hasComponent<TinyNode::BoneAttach>()) {
        missingComponents.push_back({"Bone Attachment", TinyNode::Types::BoneAttach});
    }
    
    // Only show dropdown if there are missing components
    if (!missingComponents.empty()) {
        ImGui::Text("Add Component:");
        ImGui::SameLine();
        
        // Set specific width for combo to prevent overflow
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10); // -10 for padding
        if (ImGui::BeginCombo("##AddComponent", "Select...")) {
            for (const auto& comp : missingComponents) {
                if (ImGui::Selectable(comp.first.c_str())) {
                    // Add the selected component
                    switch (comp.second) {
                        case TinyNode::Types::MeshRender:
                            mutableNode->add(TinyNode::MeshRender{});
                            break;
                        case TinyNode::Types::Skeleton:
                            mutableNode->add(TinyNode::Skeleton{});
                            break;
                        case TinyNode::Types::BoneAttach:
                            mutableNode->add(TinyNode::BoneAttach{});
                            break;
                    }
                }
            }
            ImGui::EndCombo();
        }
    }
}

void Application::queueForDeletion(TinyHandle handle) {
    pendingDeletions.push_back(handle);
}

void Application::processPendingDeletions() {
    if (pendingDeletions.empty()) return;
    
    TinyFS& fs = project->filesystem();
    
    for (TinyHandle handle : pendingDeletions) {
        fs.removeFNode(handle);
    }
    
    pendingDeletions.clear();
}

void Application::cleanup() {}

// ===================================================================
// INSPECTOR HELPER FUNCTIONS - Component Creation/Modification System
// ===================================================================

void Application::renderFileSystemInspector() {
    TinyFS& fs = project->filesystem();
    const TinyFNode* selectedFNode = fs.getFNodes().get(selectedFNodeHandle);
    if (!selectedFNode) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid filesystem node selection");
        selectedFNodeHandle = TinyHandle(); // Clear invalid selection
        return;
    }
    
    // Header based on type
    if (selectedFNode->isFile()) {
        ImGui::Text("File Inspector");
    } else {
        ImGui::Text("Folder Inspector");
    }
    ImGui::Separator();
    
    // Name editing
    static char nameBuffer[256];
    static bool nameEdit = false;
    
    if (!nameEdit) {
        ImGui::Text("Name: %s", selectedFNode->name.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Rename")) {
            nameEdit = true;
            strcpy_s(nameBuffer, sizeof(nameBuffer), selectedFNode->name.c_str());
        }
    } else {
        ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            TinyFNode* mutableNode = const_cast<TinyFNode*>(fs.getFNodes().get(selectedFNodeHandle));
            if (mutableNode) {
                mutableNode->name = std::string(nameBuffer);
            }
            nameEdit = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            nameEdit = false;
        }
    }
    
    ImGui::Text("Handle: %u_v%u", selectedFNodeHandle.index, selectedFNodeHandle.version);
    
    if (selectedFNode->isFile()) {
        // File-specific information
        std::string fileType = "Unknown";
        if (selectedFNode->tHandle.isType<TinyRScene>()) {
            fileType = "Scene";
            TinyRScene* scene = static_cast<TinyRScene*>(fs.registryRef().get(selectedFNode->tHandle));
            if (scene) {
                ImGui::Text("Scene Nodes: %u", scene->nodes.count());
                
                // Make Active Scene button
                ImGui::Spacing();
                TinyHandle sceneRegistryHandle = selectedFNode->tHandle.handle;
                bool isActiveScene = (project->getActiveSceneHandle() == sceneRegistryHandle);
                
                if (isActiveScene) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::Button("Active Scene", ImVec2(-1, 30));
                    ImGui::PopStyleColor(4);
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                    
                    if (ImGui::Button("Make Active", ImVec2(-1, 30))) {
                        project->setActiveScene(sceneRegistryHandle);
                    }
                    ImGui::PopStyleColor(3);
                }
            }
        } else if (selectedFNode->tHandle.isType<TinyRTexture>()) {
            fileType = "Texture";
        } else if (selectedFNode->tHandle.isType<TinyRMaterial>()) {
            fileType = "Material";
        } else if (selectedFNode->tHandle.isType<TinyRMesh>()) {
            fileType = "Mesh";
        } else if (selectedFNode->tHandle.isType<TinyRSkeleton>()) {
            fileType = "Skeleton";
        }
        
        ImGui::Text("Type: %s", fileType.c_str());
        if (selectedFNode->tHandle.valid()) {
            ImGui::Text("Registry Handle: %u_v%u", selectedFNode->tHandle.handle.index, selectedFNode->tHandle.handle.version);
        }
    } else {
        ImGui::Text("Children: %zu", selectedFNode->children.size());
    }
}

void Application::renderTransformInspector(const TinyRNode* selectedNode, bool isRootNode) {
    // Simple transform section without dropdown - just the controls
    if (!isRootNode) {
        // Get the current transform
        glm::mat4 localTransform = selectedNode->localTransform;
        
        // Extract translation, rotation, and scale from the matrix
        glm::vec3 translation, rotation, scale;
        glm::quat rotationQuat;
        glm::vec3 skew;
        glm::vec4 perspective;
        
        // Check if decomposition is valid
        bool validDecomposition = glm::decompose(localTransform, scale, rotationQuat, translation, skew, perspective);
        
        if (!validDecomposition) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Warning: Invalid transform matrix detected!");
            if (ImGui::Button("Reset Transform")) {
                TinyRNode* mutableNode = const_cast<TinyRNode*>(selectedNode);
                mutableNode->localTransform = glm::mat4(1.0f);
                if (TinyRScene* scene = project->getActiveScene()) scene->updateGlbTransform();
            }
            return;
        }
        
        // Validate extracted values for NaN/infinity
        auto isValidVec3 = [](const glm::vec3& v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };
        
        if (!isValidVec3(translation) || !isValidVec3(scale)) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Warning: NaN/Infinite values detected!");
            if (ImGui::Button("Reset Transform")) {
                TinyRNode* mutableNode = const_cast<TinyRNode*>(selectedNode);
                mutableNode->localTransform = glm::mat4(1.0f);
                if (TinyRScene* scene = project->getActiveScene()) scene->updateGlbTransform();
            }
            return;
        }
        
        // Clamp scale to prevent zero/negative values
        const float MIN_SCALE = 0.001f;
        scale.x = std::max(MIN_SCALE, std::abs(scale.x));
        scale.y = std::max(MIN_SCALE, std::abs(scale.y));
        scale.z = std::max(MIN_SCALE, std::abs(scale.z));
        
        // Convert quaternion to Euler angles (in degrees)
        rotation = glm::degrees(glm::eulerAngles(rotationQuat));
        
        // Normalize angles to [-180, 180] range
        auto normalizeAngle = [](float angle) {
            while (angle > 180.0f) angle -= 360.0f;
            while (angle < -180.0f) angle += 360.0f;
            return angle;
        };
        
        rotation.x = normalizeAngle(rotation.x);
        rotation.y = normalizeAngle(rotation.y);
        rotation.z = normalizeAngle(rotation.z);
        
        // Store original values to detect changes
        glm::vec3 originalTranslation = translation;
        glm::vec3 originalRotation = rotation;
        glm::vec3 originalScale = scale;
        
        // Translation controls
        ImGui::Text("Position");
        ImGui::DragFloat3("##Position", &translation.x, 0.01f, -1000.0f, 1000.0f, "%.3f");
        ImGui::SameLine();
        if (ImGui::Button("To Cam")) {
            translation = project->getCamera()->pos;
        }
        
        // Rotation controls
        ImGui::Text("Rotation (degrees)");
        ImGui::DragFloat3("##Rotation", &rotation.x, 0.5f, -180.0f, 180.0f, "%.1f°");
        
        // Scale controls
        ImGui::Text("Scale");
        ImGui::DragFloat3("##Scale", &scale.x, 0.01f, MIN_SCALE, 10.0f, "%.3f");
        ImGui::SameLine();
        if (ImGui::Button("Uniform")) {
            float avgScale = (scale.x + scale.y + scale.z) / 3.0f;
            scale = glm::vec3(avgScale);
        }
        
        // Apply changes if any values changed
        if (translation != originalTranslation || rotation != originalRotation || scale != originalScale) {
            if (isValidVec3(translation) && isValidVec3(scale)) {
                // Reconstruct matrix from components
                glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), translation);
                glm::mat4 rotationMatrix = glm::eulerAngleXYZ(glm::radians(rotation.x), glm::radians(rotation.y), glm::radians(rotation.z));
                glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);
                
                glm::mat4 newTransform = translationMatrix * rotationMatrix * scaleMatrix;
                
                TinyRNode* mutableNode = const_cast<TinyRNode*>(selectedNode);
                mutableNode->localTransform = newTransform;
                if (TinyRScene* scene = project->getActiveScene()) scene->updateGlbTransform();
            }
        }
    }
}

void Application::renderComponentsInspector(const TinyRNode* selectedNode) {
    TinyRNode* mutableNode = const_cast<TinyRNode*>(selectedNode);
    
    // Render existing components in the specified order: Mesh -> Bone -> Skeleton
    // 1. Mesh Renderer Component (first priority)
    if (selectedNode->hasComponent<TinyNode::MeshRender>()) {
        renderMeshRenderComponent(mutableNode);
        ImGui::Spacing();
    }
    
    // 2. Bone Attach Component (second priority)  
    if (selectedNode->hasComponent<TinyNode::BoneAttach>()) {
        renderBoneAttachComponent(mutableNode);
        ImGui::Spacing();
    }
    
    // 3. Skeleton Component (third priority)
    if (selectedNode->hasComponent<TinyNode::Skeleton>()) {
        renderSkeletonComponent(mutableNode);
        ImGui::Spacing();
    }
}

void Application::renderMeshRenderComponent(TinyRNode* selectedNode) {
    // Dynamic component - no fixed container, just content with separator
    ImGui::Separator();
    
    // Header with remove button
    ImGui::Text("Mesh Renderer");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Remove##MeshRender", ImVec2(65, 0))) {
        selectedNode->remove<TinyNode::MeshRender>();
        ImGui::PopStyleColor(3);
        return;
    }
    ImGui::PopStyleColor(3);
    
    ImGui::Separator();
    
    TinyNode::MeshRender* meshComp = selectedNode->get<TinyNode::MeshRender>();
    if (meshComp) {
        ImGui::Spacing();
        
        // Mesh Handle field with enhanced drag-drop support
        ImGui::Text("Mesh Resource:");
        bool meshModified = renderEnhancedHandleField("##MeshHandle", meshComp->meshHandle, "Mesh", 
            "Drag a mesh file from the File Explorer", 
            "Select mesh resource for rendering");
        
        // Show mesh information if valid
        if (meshComp->meshHandle.valid()) {
            const TinyRegistry& registry = project->registryRef();
            const TinyRMesh* mesh = registry.get<TinyRMesh>(meshComp->meshHandle);
            if (mesh) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "✓ %s", mesh->name.c_str());
            } else {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "✗ Invalid");
            }
        }
        
        ImGui::Spacing();
        
        // Skeleton Node Handle field with enhanced drag-drop support  
        ImGui::Text("Skeleton Node:");
        bool skeleModified = renderEnhancedHandleField("##SkeletonNodeHandle", meshComp->skeleNodeHandle, "SkeletonNode",
            "Drag a skeleton node from the Hierarchy",
            "Select skeleton node for bone animation");
        
        // Show skeleton node information if valid
        if (meshComp->skeleNodeHandle.valid()) {
            TinyRScene* activeScene = project->getActiveScene();
            if (activeScene) {
                const TinyRNode* skeleNode = activeScene->nodes.get(meshComp->skeleNodeHandle);
                if (skeleNode && skeleNode->hasComponent<TinyNode::Skeleton>()) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "✓ %s", skeleNode->name.c_str());
                } else {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "✗ Invalid/No Skeleton");
                }
            }
        }
        
        ImGui::Spacing();
    }
    
    ImGui::Separator();
}

void Application::renderSkeletonComponent(TinyRNode* selectedNode) {
    // Dynamic component - no fixed container
    ImGui::Separator();
    
    // Header with remove button
    ImGui::Text("Skeleton");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 65);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("Remove##Skeleton", ImVec2(60, 0))) {
        selectedNode->remove<TinyNode::Skeleton>();
        ImGui::PopStyleColor(1);
        return;
    }
    ImGui::PopStyleColor(1);
    ImGui::Separator();
        
    TinyNode::Skeleton* skeleComp = selectedNode->get<TinyNode::Skeleton>();
    if (skeleComp) {
        ImGui::Spacing();
        
        // Skeleton Handle field
        ImGui::Text("Skeleton Resource:");
        bool skeleModified = renderEnhancedHandleField("##SkeletonHandle", skeleComp->skeleHandle, "Skeleton",
            "Drag a skeleton file from the File Explorer",
            "Select skeleton resource for bone data");
        
        // Show skeleton information if valid
        if (skeleComp->skeleHandle.valid()) {
            const TinyRegistry& registry = project->registryRef();
            const TinyRSkeleton* skeleton = registry.get<TinyRSkeleton>(skeleComp->skeleHandle);
            if (skeleton) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "✓");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Skeleton Resource");
                    ImGui::Text("Bones: %zu", skeleton->bones.size());
                    ImGui::EndTooltip();
                }
            } else {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "✗ Invalid");
            }
        }
        
        ImGui::Spacing();
        
        // Show bone transform count (read-only)
        ImGui::Text("Active Bone Transforms: %zu", skeleComp->boneTransformsFinal.size());
        
        // Component status
        ImGui::Separator();
        ImGui::Text("Status:");
        ImGui::SameLine();
        
        if (skeleComp->skeleHandle.valid()) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Skeleton loaded and ready");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Missing skeleton resource");
        }
        
        ImGui::Spacing();
    }
    
    ImGui::Separator();
}

void Application::renderBoneAttachComponent(TinyRNode* selectedNode) {
    // Dynamic component - no fixed container
    ImGui::Separator();
    
    // Header with remove button
    ImGui::Text("Bone Attachment");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 65);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("Remove##BoneAttach", ImVec2(60, 0))) {
        selectedNode->remove<TinyNode::BoneAttach>();
        ImGui::PopStyleColor(1);
        return;
    }
    ImGui::PopStyleColor(1);
    ImGui::Separator();
        
        TinyNode::BoneAttach* boneComp = selectedNode->get<TinyNode::BoneAttach>();
        if (boneComp) {
            ImGui::Spacing();
            
            // Skeleton Node Handle field with enhanced drag-drop support
            ImGui::Text("Skeleton Node:");
            bool skeleModified = renderEnhancedHandleField("##SkeletonNodeHandle", boneComp->skeleNodeHandle, "SkeletonNode",
                "Drag a skeleton node from the Hierarchy",
                "Select skeleton node to attach to");
            
            // Show skeleton node information if valid
            if (boneComp->skeleNodeHandle.valid()) {
                TinyRScene* activeScene = project->getActiveScene();
                if (activeScene) {
                    const TinyRNode* skeleNode = activeScene->nodes.get(boneComp->skeleNodeHandle);
                    if (skeleNode && skeleNode->hasComponent<TinyNode::Skeleton>()) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "✓");
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::Text("Skeleton Node: %s", skeleNode->name.c_str());
                            const TinyNode::Skeleton* skelComp = skeleNode->get<TinyNode::Skeleton>();
                            if (skelComp) {
                                ImGui::Text("Available Bones: %zu", skelComp->boneTransformsFinal.size());
                            }
                            ImGui::EndTooltip();
                        }
                    } else {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "✗ Invalid/No Skeleton");
                    }
                }
            }
            
            ImGui::Spacing();
            
            // Bone Index field with validation
            ImGui::Text("Bone Index:");
            int boneIndex = static_cast<int>(boneComp->boneIndex);
            
            // Determine max bone count for validation
            int maxBoneIndex = 255; // Default max
            if (boneComp->skeleNodeHandle.valid()) {
                TinyRScene* activeScene = project->getActiveScene();
                if (activeScene) {
                    const TinyRNode* skeleNode = activeScene->nodes.get(boneComp->skeleNodeHandle);
                    if (skeleNode && skeleNode->hasComponent<TinyNode::Skeleton>()) {
                        const TinyNode::Skeleton* skelComp = skeleNode->get<TinyNode::Skeleton>();
                        if (skelComp && !skelComp->boneTransformsFinal.empty()) {
                            maxBoneIndex = static_cast<int>(skelComp->boneTransformsFinal.size() - 1);
                        }
                    }
                }
            }
            
            if (ImGui::DragInt("##BoneIndex", &boneIndex, 1.0f, 0, maxBoneIndex)) {
                boneComp->boneIndex = static_cast<uint32_t>(boneIndex);
            }
            
            // Show validation status
            ImGui::SameLine();
            if (boneIndex <= maxBoneIndex && boneComp->skeleNodeHandle.valid()) {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "✓");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Valid bone index (%d/%d)", boneIndex, maxBoneIndex);
                }
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "✗");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Invalid bone index (max: %d)", maxBoneIndex);
                }
            }
            
            ImGui::Spacing();
            
            // Component status
            ImGui::Separator();
            ImGui::Text("Status:");
            ImGui::SameLine();
            
            bool hasValidSkeleton = boneComp->skeleNodeHandle.valid();
            bool hasValidBoneIndex = boneIndex <= maxBoneIndex;
            
            if (hasValidSkeleton && hasValidBoneIndex) {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Ready for bone attachment");
            } else if (hasValidSkeleton) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Invalid bone index");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Missing skeleton node");
            }
            
            ImGui::Spacing();
        }
        
    ImGui::Separator();
}

bool Application::renderHandleField(const char* label, TinyHandle& handle, const char* targetType, const char* tooltip) {
    bool modified = false;
    
    ImGui::Text("%s", label);
    
    // Create a drag-drop target area
    std::string fieldId = std::string("##") + label + "Field";
    std::string displayText = handle.valid() ? 
        (std::to_string(handle.index) + "_v" + std::to_string(handle.version)) : 
        "None (drag here)";
    
    // Make the field look like a drag-drop zone
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.8f, 1.0f));
    
    if (ImGui::Button((displayText + fieldId).c_str(), ImVec2(-1, 25))) {
        // Clear handle on click
        handle = TinyHandle();
        modified = true;
    }
    
    ImGui::PopStyleColor(3);
    
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    
    // Handle drag-drop
    if (ImGui::BeginDragDropTarget()) {
        // Accept different payload types based on target type
        if (strcmp(targetType, "Mesh") == 0) {
            // Accept mesh files from file explorer
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                TinyHandle fileNodeHandle = *(const TinyHandle*)payload->Data;
                const TinyFNode* fileNode = project->filesystem().getFNodes().get(fileNodeHandle);
                if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<TinyRMesh>()) {
                    handle = fileNode->tHandle.handle; // Use registry handle
                    modified = true;
                }
            }
        } else if (strcmp(targetType, "Skeleton") == 0) {
            // Accept skeleton files from file explorer
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                TinyHandle fileNodeHandle = *(const TinyHandle*)payload->Data;
                const TinyFNode* fileNode = project->filesystem().getFNodes().get(fileNodeHandle);
                if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<TinyRSkeleton>()) {
                    handle = fileNode->tHandle.handle; // Use registry handle
                    modified = true;
                }
            }
        } else if (strcmp(targetType, "SkeletonNode") == 0) {
            // Accept skeleton nodes from hierarchy
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NODE_HANDLE")) {
                TinyHandle nodeHandle = *(const TinyHandle*)payload->Data;
                TinyRScene* activeScene = project->getActiveScene();
                if (activeScene) {
                    const TinyRNode* node = activeScene->nodes.get(nodeHandle);
                    if (node && node->hasComponent<TinyNode::Skeleton>()) {
                        handle = nodeHandle; // Use node handle directly
                        modified = true;
                    }
                }
            }
        }
        
        ImGui::EndDragDropTarget();
    }
    
    return modified;
}

bool Application::renderEnhancedHandleField(const char* fieldId, TinyHandle& handle, const char* targetType, const char* dragTooltip, const char* description) {
    bool modified = false;
    
    // Create enhanced drag-drop target area with better styling
    std::string displayText;
    ImVec4 buttonColor, hoveredColor, activeColor;
    
    if (handle.valid()) {
        displayText = std::to_string(handle.index) + "_v" + std::to_string(handle.version);
        buttonColor = ImVec4(0.2f, 0.4f, 0.2f, 1.0f);   // Green tint for valid handle
        hoveredColor = ImVec4(0.3f, 0.5f, 0.3f, 1.0f);
        activeColor = ImVec4(0.1f, 0.3f, 0.1f, 1.0f);
    } else {
        displayText = "None - Drop here";
        buttonColor = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);   // Gray for empty
        hoveredColor = ImVec4(0.4f, 0.4f, 0.6f, 1.0f); // Blue tint on hover
        activeColor = ImVec4(0.2f, 0.2f, 0.4f, 1.0f);
    }
    
    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
    
    // Create the drag-drop button with clear styling
    if (ImGui::Button((displayText + fieldId).c_str(), ImVec2(-1, 30))) {
        // Clear handle on click
        if (handle.valid()) {
            handle = TinyHandle();
            modified = true;
        }
    }
    
    ImGui::PopStyleColor(3);
    
    // Show appropriate tooltip based on state
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        if (handle.valid()) {
            ImGui::Text("Click to clear");
            if (description) ImGui::Text("%s", description);
        } else {
            if (dragTooltip) ImGui::Text("%s", dragTooltip);
            if (description) ImGui::Text("%s", description);
        }
        ImGui::EndTooltip();
    }
    
    // Enhanced drag-drop handling with visual feedback
    if (ImGui::BeginDragDropTarget()) {
        ImGui::PushStyleColor(ImGuiCol_DragDropTarget, ImVec4(0.3f, 0.6f, 1.0f, 0.7f));
        
        // Accept different payload types based on target type
        if (strcmp(targetType, "Mesh") == 0) {
            // Accept mesh files from file explorer
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                TinyHandle fileNodeHandle = *(const TinyHandle*)payload->Data;
                const TinyFNode* fileNode = project->filesystem().getFNodes().get(fileNodeHandle);
                if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<TinyRMesh>()) {
                    handle = fileNode->tHandle.handle; // Use registry handle
                    modified = true;
                }
            }
        } else if (strcmp(targetType, "Skeleton") == 0) {
            // Accept skeleton files from file explorer
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                TinyHandle fileNodeHandle = *(const TinyHandle*)payload->Data;
                const TinyFNode* fileNode = project->filesystem().getFNodes().get(fileNodeHandle);
                if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<TinyRSkeleton>()) {
                    handle = fileNode->tHandle.handle; // Use registry handle
                    modified = true;
                }
            }
        } else if (strcmp(targetType, "SkeletonNode") == 0) {
            // Accept skeleton nodes from hierarchy
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NODE_HANDLE")) {
                TinyHandle nodeHandle = *(const TinyHandle*)payload->Data;
                TinyRScene* activeScene = project->getActiveScene();
                if (activeScene) {
                    const TinyRNode* node = activeScene->nodes.get(nodeHandle);
                    if (node && node->hasComponent<TinyNode::Skeleton>()) {
                        handle = nodeHandle; // Use node handle directly
                        modified = true;
                    }
                }
            }
        }
        
        ImGui::PopStyleColor();
        ImGui::EndDragDropTarget();
    }
    
    return modified;
}

void Application::renderNodeDeleteButton(const TinyRNode* selectedNode) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    
    if (ImGui::Button("Delete Node", ImVec2(-1, 30))) {
        // Add confirmation logic here if needed
        // For now, just clear the selection
        selectedSceneNodeHandle = project->getRootNodeHandle();
    }
    
    ImGui::PopStyleColor(3);
}