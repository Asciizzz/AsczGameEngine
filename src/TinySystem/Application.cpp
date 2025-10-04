#include "TinySystem/Application.hpp"

#include "TinyEngine/TinyLoader.hpp"

#include <iostream>
#include <random>

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

    float aspectRatio = static_cast<float>(appWidth) / static_cast<float>(appHeight);
    project->getCamera()->setAspectRatio(aspectRatio);

// PLAYGROUND FROM HERE

    TinyModel newModel0 = TinyLoader::loadModel("Assets/Characters/Furina.glb", false);
    TinyModel newModel1 = TinyLoader::loadModel("Assets/Characters/Furina.glb", false);
    project->addTemplateFromModel(newModel0);
    project->addTemplateFromModel(newModel1);

    project->addNodeInstance(0);

    project->printRuntimeNodeHierarchy();

// PLAYGROUND END HERE 

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
    imguiWrapper = MakeUnique<ImGuiWrapper>();
    
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

        // Press p to place
        static bool pPressed = false;
        if (k_state[SDL_SCANCODE_P] && !pPressed) {
            // Place it at the camera position
            // Rotated in the camera yaw direction

            glm::mat4 rot = glm::mat4(1.0f);
            rot = glm::rotate(rot, camRef.getYaw(true), glm::vec3(0.0f, 1.0f, 0.0f));

            glm::mat4 trans = glm::translate(glm::mat4(1.0f), camRef.pos + camRef.forward * 2.0f);

            glm::mat4 model = trans * rot;

            project->addNodeInstance(1, 0, model);
            project->updateGlobalTransforms(0);
            
            project->printRuntimeNodeHierarchy();
            pPressed = true;
        } else if (!k_state[SDL_SCANCODE_P]) {
            pPressed = false;
        }

        project->runPlayground(dTime);

        // Start ImGui frame
        imguiWrapper->newFrame();
        
        // Create ImGui UI
        createImGuiUI(fpsRef, camRef, mouseFocus, dTime);

        project->getGlobal()->update(camRef, rendererRef.getCurrentFrame());

        uint32_t imageIndex = rendererRef.beginFrame();
        if (imageIndex != UINT32_MAX) {
            // Update global UBO buffer from frame index
            uint32_t currentFrameIndex = rendererRef.getCurrentFrame();
            // glbUBOManager->updateUBO(camRef, currentFrameIndex);

            rendererRef.drawSky(project.get(), PIPELINE_INSTANCE(pipelineManager.get(), "Sky"));

            rendererRef.drawScene(project.get(), PIPELINE_INSTANCE(pipelineManager.get(), "Test"));

            // End frame with ImGui rendering integrated
            rendererRef.endFrame(imageIndex, imguiWrapper.get());
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

void Application::createImGuiUI(const TinyChrono& fpsManager, const TinyCamera& camera, bool mouseFocus, float deltaTime) {
    // Main debug window
    if (showDebugWindow) {
        ImGui::Begin("Debug Panel", &showDebugWindow);
        
        // FPS and Performance
        ImGui::Text("Performance");
        ImGui::Separator();
        ImGui::Text("FPS: %.1f (%.2f ms)", fpsManager.currentFPS, fpsManager.frameTimeMs);
        ImGui::Text("Avg FPS: %.1f", fpsManager.getAverageFPS());
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
        
        // Input Status
        ImGui::Text("Input");
        ImGui::Separator();
        ImGui::Text("Mouse Focused: %s", mouseFocus ? "Yes" : "No");
        ImGui::Text("Press F1 to toggle mouse lock");
        ImGui::Text("Press F11 for fullscreen");
        ImGui::Text("Press P to place object");
        ImGui::Text("WASD: Move | QE: Roll | R: Reset Roll");
        
        ImGui::Spacing();
        
        // Scene Controls
        ImGui::Text("Scene Controls");
        ImGui::Separator();
        
        // Node instance placement button
        if (ImGui::Button("Place Object at Camera", ImVec2(180, 30))) {
            // Same logic as the 'P' key press
            glm::mat4 rot = glm::mat4(1.0f);
            rot = glm::rotate(rot, camera.getYaw(true), glm::vec3(0.0f, 1.0f, 0.0f));

            glm::mat4 trans = glm::translate(glm::mat4(1.0f), camera.pos + camera.forward * 2.0f);
            glm::mat4 model = trans * rot;

            project->addNodeInstance(1, 0, model);
            project->updateGlobalTransforms(0);
            project->printRuntimeNodeHierarchy();
        }
        
        // Additional placement options
        ImGui::SameLine();
        if (ImGui::Button("Place at Origin", ImVec2(120, 30))) {
            glm::mat4 model = glm::mat4(1.0f); // Identity matrix (at origin)
            project->addNodeInstance(1, 0, model);
            project->updateGlobalTransforms(0);
            project->printRuntimeNodeHierarchy();
        }
        
        // Random placement button
        if (ImGui::Button("Place Random", ImVec2(120, 30))) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_real_distribution<float> posDist(-10.0f, 10.0f);
            static std::uniform_real_distribution<float> rotDist(0.0f, 6.28318f); // 0 to 2π
            
            glm::vec3 randomPos(posDist(gen), 0.0f, posDist(gen));
            glm::mat4 rot = glm::rotate(glm::mat4(1.0f), rotDist(gen), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 trans = glm::translate(glm::mat4(1.0f), randomPos);
            glm::mat4 model = trans * rot;
            
            project->addNodeInstance(1, 0, model);
            project->updateGlobalTransforms(0);
            project->printRuntimeNodeHierarchy();
        }
        
        ImGui::Spacing();
        
        // Window controls
        ImGui::Text("Windows");
        ImGui::Separator();
        ImGui::Checkbox("Show Demo Window", &showDemoWindow);
        ImGui::Checkbox("Show Scene Window", &showSceneWindow);
        ImGui::Checkbox("Show ImGui Explorer", &showImGuiExplorerWindow);
        
        ImGui::End();
    }
    
    // Scene Management Window
    if (showSceneWindow) {
        ImGui::Begin("Scene Manager", &showSceneWindow);
        
        // Object Placement Section
        ImGui::Text("Object Placement");
        ImGui::Separator();
        
        // Distance slider for placement
        static float placementDistance = 2.0f;
        ImGui::SliderFloat("Placement Distance", &placementDistance, 0.5f, 10.0f, "%.1f units");
        
        // Template selection
        static int selectedTemplate = 1;
        ImGui::InputInt("Template ID", &selectedTemplate);
        if (selectedTemplate < 0) selectedTemplate = 0;
        
        // Placement buttons with custom distance
        if (ImGui::Button("Place at Camera", ImVec2(140, 25))) {
            glm::mat4 rot = glm::rotate(glm::mat4(1.0f), camera.getYaw(true), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 trans = glm::translate(glm::mat4(1.0f), camera.pos + camera.forward * placementDistance);
            glm::mat4 model = trans * rot;
            
            project->addNodeInstance(selectedTemplate, 0, model);
            project->updateGlobalTransforms(0);
            project->printRuntimeNodeHierarchy();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Place at Origin", ImVec2(140, 25))) {
            project->addNodeInstance(selectedTemplate, 0, glm::mat4(1.0f));
            project->updateGlobalTransforms(0);
            project->printRuntimeNodeHierarchy();
        }
        
        // Manual position placement
        static float manualPos[3] = {0.0f, 0.0f, 0.0f};
        static float manualRot[3] = {0.0f, 0.0f, 0.0f};
        
        ImGui::Spacing();
        ImGui::Text("Manual Placement");
        ImGui::DragFloat3("Position", manualPos, 0.1f, -50.0f, 50.0f);
        ImGui::DragFloat3("Rotation (degrees)", manualRot, 1.0f, -180.0f, 180.0f);
        
        if (ImGui::Button("Place Manually", ImVec2(140, 25))) {
            glm::mat4 model = glm::mat4(1.0f);
            
            // Apply rotations (convert degrees to radians)
            model = glm::rotate(model, glm::radians(manualRot[1]), glm::vec3(0.0f, 1.0f, 0.0f)); // Y (yaw)
            model = glm::rotate(model, glm::radians(manualRot[0]), glm::vec3(1.0f, 0.0f, 0.0f)); // X (pitch)
            model = glm::rotate(model, glm::radians(manualRot[2]), glm::vec3(0.0f, 0.0f, 1.0f)); // Z (roll)
            
            // Apply translation
            model = glm::translate(glm::mat4(1.0f), glm::vec3(manualPos[0], manualPos[1], manualPos[2])) * model;
            
            project->addNodeInstance(selectedTemplate, 0, model);
            project->updateGlobalTransforms(0);
            project->printRuntimeNodeHierarchy();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Scene Info
        ImGui::Text("Scene Information");
        ImGui::Text("Runtime Node Count: %zu", project->getRuntimeNodes().size());
        ImGui::Text("Mesh Render Nodes: %zu", project->getRuntimeMeshRenderIndices().size());
        
        // Quick node list (first few nodes)
        ImGui::Spacing();
        ImGui::Text("Node List (first 10):");
        const auto& rtNodes = project->getRuntimeNodes();
        for (size_t i = 0; i < std::min(rtNodes.size(), size_t(10)); ++i) {
            if (rtNodes[i]) {
                // Get registry node to display name
                const auto* regNode = project->getRegistry()->get<TinyRNode>(rtNodes[i]->rHandle);
                std::string nodeName = regNode ? regNode->name : "Unknown";
                ImGui::Text("  [%zu] %s", i, nodeName.c_str());
            }
        }
        
        ImGui::End();
    }
    
    // ImGui Feature Explorer Window
    if (showImGuiExplorerWindow) {
        ImGui::Begin("ImGui Feature Explorer", &showImGuiExplorerWindow);
        
        // Tabs for different feature categories
        if (ImGui::BeginTabBar("FeatureTabs")) {
            
            // Basic Widgets Tab
            if (ImGui::BeginTabItem("Basic Widgets")) {
                static bool checkbox1 = false;
                static bool checkbox2 = true;
                ImGui::Checkbox("Checkbox 1", &checkbox1);
                ImGui::Checkbox("Checkbox 2", &checkbox2);
                
                static int radio = 0;
                ImGui::RadioButton("Radio 1", &radio, 0); ImGui::SameLine();
                ImGui::RadioButton("Radio 2", &radio, 1); ImGui::SameLine();
                ImGui::RadioButton("Radio 3", &radio, 2);
                
                static float slider1 = 0.5f;
                static int slider2 = 50;
                ImGui::SliderFloat("Float Slider", &slider1, 0.0f, 1.0f);
                ImGui::SliderInt("Int Slider", &slider2, 0, 100);
                
                static float color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
                ImGui::ColorEdit4("Color Picker", color);
                
                ImGui::EndTabItem();
            }
            
            // Input Widgets Tab
            if (ImGui::BeginTabItem("Input Widgets")) {
                static char textBuffer[256] = "Edit me!";
                ImGui::InputText("Text Input", textBuffer, sizeof(textBuffer));
                
                static float inputFloat = 3.14159f;
                static int inputInt = 42;
                ImGui::InputFloat("Input Float", &inputFloat, 0.01f, 1.0f, "%.3f");
                ImGui::InputInt("Input Int", &inputInt);
                
                static float dragFloat = 0.0f;
                static int dragInt = 0;
                ImGui::DragFloat("Drag Float", &dragFloat, 0.005f);
                ImGui::DragInt("Drag Int", &dragInt);
                
                ImGui::EndTabItem();
            }
            
            // Layout & Styling Tab
            if (ImGui::BeginTabItem("Layout & Style")) {
                ImGui::Text("Button Variations:");
                
                if (ImGui::Button("Normal Button")) {
                    // Button action
                }
                
                ImGui::SameLine();
                if (ImGui::SmallButton("Small")) {
                    // Small button action
                }
                
                if (ImGui::Button("Custom Size Button", ImVec2(200, 40))) {
                    // Custom size button action
                }
                
                ImGui::Separator();
                
                ImGui::Text("Text Variations:");
                ImGui::Text("Normal text");
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Colored text");
                ImGui::TextWrapped("This is a long text that will wrap around when it reaches the edge of the window. Very useful for descriptions and help text.");
                
                ImGui::Separator();
                
                ImGui::Text("Progress Bars:");
                static float progress = 0.0f;
                progress += 0.001f;
                if (progress > 1.0f) progress = 0.0f;
                
                ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f));
                ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), "Custom Progress");
                
                ImGui::EndTabItem();
            }
            
            // Trees & Lists Tab
            if (ImGui::BeginTabItem("Trees & Lists")) {
                ImGui::Text("Tree Nodes:");
                
                if (ImGui::TreeNode("Root Node")) {
                    if (ImGui::TreeNode("Child 1")) {
                        ImGui::Text("Leaf 1");
                        ImGui::Text("Leaf 2");
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("Child 2")) {
                        ImGui::Text("Leaf 3");
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
                
                ImGui::Separator();
                
                ImGui::Text("Selectable List:");
                static int selected = -1;
                for (int i = 0; i < 5; i++) {
                    char label[32];
                    sprintf(label, "Item %d", i);
                    if (ImGui::Selectable(label, selected == i)) {
                        selected = i;
                    }
                }
                
                ImGui::EndTabItem();
            }
            
            // Fonts Tab
            if (ImGui::BeginTabItem("Fonts & Styling")) {
                ImGui::Text("Custom Font Loading:");
                ImGui::Separator();
                
                // Font loading controls
                static float fontSize = 16.0f;
                ImGui::SliderFloat("Font Size", &fontSize, 8.0f, 48.0f, "%.0f px");
                
                // Load font buttons
                if (ImGui::Button("Load SpaceMono Regular", ImVec2(200, 25))) {
                    imguiWrapper->loadCustomFont("Assets/Fonts/SpaceMono-Regular.ttf", fontSize, "SpaceMono Regular");
                }
                
                if (ImGui::Button("Load SpaceMono Bold", ImVec2(200, 25))) {
                    imguiWrapper->loadCustomFont("Assets/Fonts/SpaceMono-Bold.ttf", fontSize, "SpaceMono Bold");
                }
                
                if (ImGui::Button("Load SpaceMono Italic", ImVec2(200, 25))) {
                    imguiWrapper->loadCustomFont("Assets/Fonts/SpaceMono-Italic.ttf", fontSize, "SpaceMono Italic");
                }
                
                if (ImGui::Button("Load SpaceMono Bold Italic", ImVec2(200, 25))) {
                    imguiWrapper->loadCustomFont("Assets/Fonts/SpaceMono-BoldItalic.ttf", fontSize, "SpaceMono Bold Italic");
                }
                
                ImGui::Spacing();
                ImGui::Separator();
                
                // Font atlas debug info
                ImGuiIO& io = ImGui::GetIO();
                ImGui::Text("Font Atlas Debug:");
                ImGui::Text("Built: %s", io.Fonts->IsBuilt() ? "Yes" : "No");
                ImGui::Text("Font count: %d", io.Fonts->Fonts.Size);
                
                // Get texture data to check if atlas is built
                unsigned char* pixels;
                int width, height;
                io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
                ImGui::Text("Texture size: %dx%d", width, height);
                ImGui::Text("Pixels: %s", pixels ? "Valid" : "NULL");
                
                ImGui::Spacing();
                ImGui::Separator();
                
                // Font selection
                ImGui::Text("Font Selection:");
                const auto& loadedFonts = imguiWrapper->getLoadedFonts();
                
                if (ImGui::Button("Reset to Default Font", ImVec2(180, 25))) {
                    imguiWrapper->resetToDefaultFont();
                }
                
                ImGui::Spacing();
                
                if (loadedFonts.empty()) {
                    ImGui::Text("No custom fonts loaded. Use buttons above to load fonts.");
                } else {
                    for (size_t i = 0; i < loadedFonts.size(); ++i) {
                        const auto& [fontName, font] = loadedFonts[i];
                        
                        if (ImGui::Button(("Set Global: " + fontName).c_str(), ImVec2(180, 25))) {
                            imguiWrapper->setGlobalFont(font);
                        }
                        
                        // Show sample text with this font
                        ImGui::SameLine();
                        ImGui::PushFont(font);
                        ImGui::Text("Sample: Hello World! 123");
                        ImGui::PopFont();
                    }
                }
                
                ImGui::Spacing();
                ImGui::Separator();
                
                // Style customization
                ImGui::Text("Style Customization:");
                ImGuiStyle& style = ImGui::GetStyle();
                
                static bool showStyleEditor = false;
                ImGui::Checkbox("Show Style Editor", &showStyleEditor);
                
                if (showStyleEditor) {
                    ImGui::Begin("Style Editor", &showStyleEditor);
                    ImGui::ShowStyleEditor();
                    ImGui::End();
                }
                
                // Quick style presets
                if (ImGui::Button("Dark Theme")) {
                    ImGui::StyleColorsDark();
                }
                ImGui::SameLine();
                if (ImGui::Button("Light Theme")) {
                    ImGui::StyleColorsLight();
                }
                ImGui::SameLine();
                if (ImGui::Button("Classic Theme")) {
                    ImGui::StyleColorsClassic();
                }
                
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        
        ImGui::End();
    }
    
    // Demo window
    if (showDemoWindow) {
        imguiWrapper->showDemoWindow(&showDemoWindow);
    }
}

void Application::cleanup() {}