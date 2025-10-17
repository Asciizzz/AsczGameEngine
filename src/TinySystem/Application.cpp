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
    
    // Debug Panel Window
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
        ImGui::Checkbox("Show Scene Window", &showSceneWindow);
        ImGui::Checkbox("Show Inspector", &showInspectorWindow);
        ImGui::Checkbox("Show Editor Settings", &showEditorSettingsWindow);
    }, &showDebugWindow);
    
    // Scene Manager Window
    const TinyRegistry& registry = project->registryRef();
    TinyFS& fs = project->filesystem();

    imguiWrapper->addWindow("Scene Manager", [this, &camera, &registry, &fs]() {
        // Runtime Node Hierarchy section (now on top)
        ImGui::Text("Runtime Node Hierarchy");
        ImGui::Separator();
        
        // Use 60% of window for node hierarchy (was bottom, now top)
        float hierarchyHeight = ImGui::GetContentRegionAvail().y * 0.6f;
        ImGui::BeginChild("NodeTree", ImVec2(0, hierarchyHeight), true);
        
        // Clear filesystem selection when clicking in node tree area
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
            selectedFNodeHandle = TinyHandle();
        }
        
        TinyRScene* activeScene = project->getActiveScene();
        if (activeScene && activeScene->nodes.count() > 0) {
            project->renderSelectableNodeTreeImGui(project->getRootNodeHandle(), selectedSceneNodeHandle);
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No active scene");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drag scenes here to create instances");
        }
        ImGui::EndChild();
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Scene Library section (now on bottom)
        ImGui::Text("Scene Library");
        
        // Render folder tree with drag-drop - use remaining space
        float libraryHeight = ImGui::GetContentRegionAvail().y - 150; // Leave space for stats and buttons
        ImGui::BeginChild("SceneLibrary", ImVec2(0, libraryHeight), true);
        
        // Clear filesystem node selection when clicking in folder tree area (but not on items)
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
            selectedSceneNodeHandle = TinyHandle();
            selectedFNodeHandle = TinyHandle();
        }
        
        renderSceneFolderTree(fs, fs.rootHandle());
        ImGui::EndChild();
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Scene Library and Runtime Info
        ImGui::Text("Project Statistics");
        
        // Count scenes in library
        uint32_t totalScenes = fs.registryRef().count<TinyRScene>();
        ImGui::Text("Loaded Scenes: %u", totalScenes);
        
        // Show active scene info
        if (activeScene) {
            ImGui::Text("Active Scene: %s", activeScene->name.c_str());
            ImGui::Text("Active Scene Nodes: %u", activeScene->nodes.count());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "No Active Scene");
            ImGui::Text("Active Scene Nodes: 0");
        }

        ImGui::Spacing();
        
        // Debug buttons
        if (ImGui::Button("Debug Tree")) {
            project->debugPrintHierarchyTree();
        }
        ImGui::SameLine();
        if (ImGui::Button("Debug Flat")) {
            project->debugPrintHierarchyFlat();
        }
    }, &showSceneWindow);
    
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
            selectedSceneNodeHandle = TinyHandle(); // Clear scene node selection
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
                                selectedSceneNodeHandle = TinyHandle(); // Clear scene node selection
                                
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
                            selectedSceneNodeHandle = TinyHandle(); // Clear scene node selection
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
                            selectedSceneNodeHandle = TinyHandle(); // Clear scene node selection
                            
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
                        selectedSceneNodeHandle = TinyHandle(); // Clear scene node selection
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
    // Determine what to inspect based on selection
    if (selectedFNodeHandle.valid()) {
        // Filesystem Node Inspector (Files and Folders)
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
                // Directly modify the node name
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
                        // Gray out button if already active
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                        ImGui::Button("Active Scene", ImVec2(-1, 30));
                        ImGui::PopStyleColor(4);
                    } else {
                        // Normal green button for non-active scenes
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                        
                        if (ImGui::Button("Make Active", ImVec2(-1, 30))) {
                            if (project->setActiveScene(sceneRegistryHandle)) {
                                // Successfully switched active scene
                                // Optionally show a status message or do nothing
                            }
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
            // Folder-specific information
            ImGui::Text("Children: %zu", selectedFNode->children.size());
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Actions
        ImGui::Text("Actions:");
        
        if (!selectedFNode->isFile()) {
            // Folder actions
            if (ImGui::Button("Create Subfolder", ImVec2(-1, 30))) {
                static int folderCounter = 1;
                std::string newFolderName = "New Folder " + std::to_string(folderCounter++);
                fs.addFolder(selectedFNodeHandle, newFolderName);
            }
            ImGui::Spacing();
        }
        
        // Delete button (with different logic for folders vs files)
        bool canDelete = true;
        std::string deleteButtonText = selectedFNode->isFile() ? "Delete File" : "Delete Folder";
        
        if (selectedFNode->isFile()) {
            // For files, check if it's deletable
            canDelete = selectedFNode->isDeletable();
        } else {
            // For folders, check if empty and deletable
            canDelete = selectedFNode->children.empty() && selectedFNode->isDeletable();
            if (selectedFNodeHandle == fs.rootHandle()) {
                canDelete = false; // Never delete root
            }
        }
        
        // Only show delete button if item is deletable
        if (selectedFNode->isDeletable() && selectedFNodeHandle != fs.rootHandle()) {
            if (!canDelete && !selectedFNode->isFile()) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.3f, 0.3f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            }
            
            if (ImGui::Button(deleteButtonText.c_str(), ImVec2(-1, 30))) {
                if (canDelete || selectedFNode->isFile()) {
                    if (selectedFNode->isFile()) {
                        ImGui::OpenPopup("ConfirmFileDelete");
                    } else {
                        queueForDeletion(selectedFNodeHandle);
                        selectedFNodeHandle = TinyHandle();
                    }
                } else {
                    ImGui::OpenPopup("ConfirmFolderDelete");
                }
            }
            
            if (!canDelete && !selectedFNode->isFile()) {
                ImGui::PopStyleColor(1);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Folder contains items. Click to delete anyway.");
                }
            } else {
                ImGui::PopStyleColor(3);
            }
        }
        // If not deletable, show a grayed out indicator instead
        else if (!selectedFNode->isDeletable()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            
            ImGui::Button("Protected", ImVec2(-1, 30));
            
            ImGui::PopStyleColor(4);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("This item is protected and cannot be deleted");
            }
        }
        
        // Confirmation popups
        if (ImGui::BeginPopupModal("ConfirmFileDelete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete '%s'?", selectedFNode->name.c_str());
            ImGui::Text("This action cannot be undone.");
            ImGui::Separator();
            
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                queueForDeletion(selectedFNodeHandle);
                selectedFNodeHandle = TinyHandle();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginPopupModal("ConfirmFolderDelete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("This folder contains %zu items.", selectedFNode->children.size());
            ImGui::Text("Are you sure you want to delete it and all its contents?");
            ImGui::Separator();
            
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                queueForDeletion(selectedFNodeHandle);
                selectedFNodeHandle = TinyHandle();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        // Additional information
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text(selectedFNode->isFile() ? "File Information:" : "Folder Information:");
        
        // Parent info
        if (selectedFNode->parent.valid()) {
            const TinyFNode* parentNode = fs.getFNodes().get(selectedFNode->parent);
            if (parentNode) {
                ImGui::Text("Parent: %s", parentNode->name.c_str());
            }
        } else {
            ImGui::Text("Parent: Root");
        }
        
        // Special actions for scene files
        if (selectedFNode->isFile() && selectedFNode->tHandle.isType<TinyRScene>() && selectedSceneNodeHandle.valid()) {
            ImGui::Spacing();
            ImGui::Text("Scene Actions:");
            if (ImGui::Button("Instantiate at Selected Node", ImVec2(-1, 25))) {
                TinyHandle sceneRegistryHandle = selectedFNode->tHandle.handle;
                project->addSceneInstance(sceneRegistryHandle, selectedSceneNodeHandle);
                if (TinyRScene* scene = project->getActiveScene()) scene->updateGlbTransform();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Add this scene as a child of the selected runtime node");
            }
        }
        
        return;
    }
    
    if (!selectedSceneNodeHandle.valid()) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Nothing selected");
        ImGui::Text("Select a scene node, folder, or file to inspect it.");
        return;
    }

    TinyRScene* activeScene = project->getActiveScene();
    if (!activeScene) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No active scene");
        return;
    }
    
    const TinyRNode* selectedNode = activeScene->nodes.get(selectedSceneNodeHandle);
    if (!selectedNode) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid node selection");
        selectedSceneNodeHandle = project->getRootNodeHandle(); // Reset to root
        return;
    }

    bool isRootNode = (selectedSceneNodeHandle == project->getRootNodeHandle());

    // Node Header Info
    ImGui::Text("Node: %s", selectedNode->name.c_str());
    ImGui::Text("Handle: %u_v%u", selectedSceneNodeHandle.index, selectedSceneNodeHandle.version);
    // Determine node type based on components
    std::string nodeType = "Transform Node";
    if (selectedNode->hasType(TinyNode::Types::MeshRender)) {
        nodeType = "Mesh Renderer";
    } else if (selectedNode->hasType(TinyNode::Types::Skeleton)) {
        nodeType = "Skeleton";
    } else if (selectedNode->hasType(TinyNode::Types::BoneAttach)) {
        nodeType = "Bone Attachment";
    }
    
    ImGui::Text("Type: %s", nodeType.c_str());
    
    ImGui::Separator();

    // Transform section
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen) && !isRootNode) {
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
        
        // Convert quaternion to Euler angles (in degrees) - but handle gimbal lock
        rotation = glm::degrees(glm::eulerAngles(rotationQuat));
        
        // Normalize angles to [-180, 180] range to avoid discontinuities
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
        glm::quat originalQuaternion = rotationQuat;
        
        // Translation controls (Godot-style drag)
        ImGui::Text("Position");
        ImGui::DragFloat3("##Position", &translation.x, 0.01f, -1000.0f, 1000.0f, "%.3f");
        ImGui::SameLine();
        if (ImGui::Button("To Cam")) {
            translation = project->getCamera()->pos;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Set position to camera location");
        }
        
        // Rotation controls with gimbal lock mitigation
        ImGui::Text("Rotation (degrees)");
        
        // Individual axis rotation sliders with ±180 degree range
        bool rotationChanged = false;
        
        ImGui::Text("Pitch (X)");
        float pitchDegrees = rotation.x;
        if (ImGui::DragFloat("##RotX", &pitchDegrees, 0.5f, -89.0f, 89.0f, "%.1f°")) {
            rotation.x = pitchDegrees;
            rotationChanged = true;
        }
        
        ImGui::Text("Yaw (Y)"); 
        float yawDegrees = rotation.y;
        if (ImGui::DragFloat("##RotY", &yawDegrees, 0.5f, -180.0f, 180.0f, "%.1f°")) {
            rotation.y = yawDegrees;
            rotationChanged = true;
        }
        
        ImGui::Text("Roll (Z)");
        float rollDegrees = rotation.z;
        if (ImGui::DragFloat("##RotZ", &rollDegrees, 0.5f, -180.0f, 180.0f, "%.1f°")) {
            rotation.z = rollDegrees;
            rotationChanged = true;
        }
        
        // Alternative: Direct quaternion manipulation buttons for gimbal-lock-free rotation
        ImGui::Spacing();
        ImGui::Text("Quick Rotations (Gimbal-Lock Free):");
        
        if (ImGui::Button("Rot +90° Y")) {
            rotationQuat = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0)) * rotationQuat;
            rotationChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Rot -90° Y")) {
            rotationQuat = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0, 1, 0)) * rotationQuat;
            rotationChanged = true;
        }
        
        if (ImGui::Button("Rot +90° X")) {
            rotationQuat = glm::angleAxis(glm::radians(90.0f), glm::vec3(1, 0, 0)) * rotationQuat;
            rotationChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Rot -90° X")) {
            rotationQuat = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1, 0, 0)) * rotationQuat;
            rotationChanged = true;
        }
        
        // Scale controls with safety limits
        ImGui::Text("Scale");
        ImGui::DragFloat3("##Scale", &scale.x, 0.01f, MIN_SCALE, 10.0f, "%.3f");
        
        // Enforce minimum scale values in real-time
        scale.x = std::max(MIN_SCALE, scale.x);
        scale.y = std::max(MIN_SCALE, scale.y);
        scale.z = std::max(MIN_SCALE, scale.z);
        
        // Uniform scale button
        ImGui::SameLine();
        if (ImGui::Button("Uniform")) {
            float avgScale = (scale.x + scale.y + scale.z) / 3.0f;
            scale = glm::vec3(avgScale);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Set all scale components to their average");
        }
        
        // Reset buttons
        ImGui::Spacing();
        if (ImGui::Button("Reset Position")) {
            translation = glm::vec3(0.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Rotation")) {
            rotation = glm::vec3(0.0f);
            rotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
            rotationChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Scale")) {
            scale = glm::vec3(1.0f);
        }
        
        // Apply changes if any values changed
        if (translation != originalTranslation || rotation != originalRotation || scale != originalScale || rotationChanged) {
            // Validate inputs before applying
            if (isValidVec3(translation) && isValidVec3(scale)) {
                glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
                glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
                glm::mat4 R;
                
                // Use updated quaternion if we used direct quaternion manipulation, otherwise convert from Euler
                if (rotationChanged && (rotationQuat != originalQuaternion)) {
                    // Direct quaternion manipulation was used
                    R = glm::mat4_cast(rotationQuat);
                } else if (rotation != originalRotation) {
                    // Euler angle sliders were used - convert to quaternion
                    glm::quat qX = glm::angleAxis(glm::radians(rotation.x), glm::vec3(1, 0, 0));
                    glm::quat qY = glm::angleAxis(glm::radians(rotation.y), glm::vec3(0, 1, 0));
                    glm::quat qZ = glm::angleAxis(glm::radians(rotation.z), glm::vec3(0, 0, 1));
                    rotationQuat = qY * qX * qZ; // YXZ order to minimize gimbal lock
                    R = glm::mat4_cast(rotationQuat);
                } else {
                    // No rotation change
                    R = glm::mat4_cast(rotationQuat);
                }
                
                glm::mat4 newTransform = T * R * S;
                
                // Validate the resulting matrix
                bool matrixValid = true;
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        if (!std::isfinite(newTransform[i][j])) {
                            matrixValid = false;
                            break;
                        }
                    }
                    if (!matrixValid) break;
                }
                
                if (matrixValid) {
                    // Apply the new transform to the node
                    TinyRNode* mutableNode = const_cast<TinyRNode*>(selectedNode);
                    mutableNode->localTransform = newTransform;
                    
                    // Update global transforms
                    if (TinyRScene* scene = project->getActiveScene()) scene->updateGlbTransform();
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid transform - changes ignored");
                }
            }
        }
    }

    // Additional Info section
    if (ImGui::CollapsingHeader("Additional Info")) {
        ImGui::Text("Local Transform Matrix:");
        const glm::mat4& matrix = selectedNode->localTransform;
        ImGui::Text("[ %6.3f %6.3f %6.3f %6.3f ]", matrix[0][0], matrix[1][0], matrix[2][0], matrix[3][0]);
        ImGui::Text("[ %6.3f %6.3f %6.3f %6.3f ]", matrix[0][1], matrix[1][1], matrix[2][1], matrix[3][1]);
        ImGui::Text("[ %6.3f %6.3f %6.3f %6.3f ]", matrix[0][2], matrix[1][2], matrix[2][2], matrix[3][2]);
        ImGui::Text("[ %6.3f %6.3f %6.3f %6.3f ]", matrix[0][3], matrix[1][3], matrix[2][3], matrix[3][3]);
        
        // Parent info
        if (selectedNode->parentHandle.valid()) {
            const TinyRNode* parentNode = activeScene->nodes.get(selectedNode->parentHandle);
            if (parentNode) {
                ImGui::Text("Parent: %s (%u_v%u)", parentNode->name.c_str(), 
                           selectedNode->parentHandle.index, selectedNode->parentHandle.version);
            }
        } else {
            ImGui::Text("Parent: None (Root Node)");
        }
        
        ImGui::Text("Child Count: %zu", selectedNode->childrenHandles.size());
        
        // Component info
        ImGui::Text("Components:");
        if (selectedNode->hasType(TinyNode::Types::MeshRender)) {
            ImGui::BulletText("Mesh Renderer");
        }
        if (selectedNode->hasType(TinyNode::Types::Skeleton)) {
            ImGui::BulletText("Skeleton");
        }
        if (selectedNode->hasType(TinyNode::Types::BoneAttach)) {
            ImGui::BulletText("Bone Attachment");
        }
        if (selectedNode->types == TinyNode::toMask(TinyNode::Types::Node)) {
            ImGui::BulletText("Transform Only");
        }
    }

    ImGui::Separator();

    // Delete button (only show if not root node)
    if (!isRootNode) {
        if (ImGui::Button("Delete Node", ImVec2(-1, 30))) {
            if (activeScene->deleteNodeRecursive(selectedSceneNodeHandle)) {
                // Successfully deleted, reset selection to root
                selectedSceneNodeHandle = project->getRootNodeHandle();
                // Update global transforms after deletion
                if (TinyRScene* scene = project->getActiveScene()) scene->updateGlbTransform();
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Delete this node and all its children");
        }
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Root node cannot be deleted");
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