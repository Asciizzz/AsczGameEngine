#include "tinyApp/tinyApp.hpp"

#include "tinyEngine/TinyLoader.hpp"

#ifdef NDEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = true;
#endif

using namespace tinyVk;

tinyApp::tinyApp(const char* title, uint32_t width, uint32_t height)
    : appTitle(title), appWidth(width), appHeight(height) {

    initComponents();
}

tinyApp::~tinyApp() {
    vkDeviceWaitIdle(dvk->device);
    tinyUI::Shutdown();

    printf("tinyApp exited successfully. See you next time!\n");
}

void tinyApp::run() {
    mainLoop();
}

void tinyApp::initComponents() {
    windowManager = MakeUnique<tinyWindow>(appTitle, appWidth, appHeight);
    fpsManager = MakeUnique<tinyChrono>();

    auto extensions = windowManager->getRequiredVulkanExtensions();
    instanceVk = MakeUnique<Instance>(extensions, enableValidationLayers);
    instanceVk->createSurface(windowManager->window);

    dvk = MakeUnique<Device>(instanceVk->instance, instanceVk->surface);

    VkDevice device = dvk->device;
    VkPhysicalDevice pDevice = dvk->pDevice;

    // Create renderer (which now manages depth manager, swap chain and render passes)
    renderer = MakeUnique<Renderer>(
        dvk.get(),
        instanceVk->surface,
        windowManager->window,
        tinyApp::MAX_FRAMES_IN_FLIGHT
    );

    project = MakeUnique<tinyProject>(dvk.get());

    float aspectRatio = static_cast<float>(appWidth) / static_cast<float>(appHeight);
    project->camera()->setAspectRatio(aspectRatio);

// PLAYGROUND FROM HERE

    const SceneRes& sharedRes = project->sharedRes();
    VkRenderPass renderPass = renderer->getMainRenderPass();

    // ===== Pipeline 1: Sky =====
    RasterCfg skyCfg;
    skyCfg.renderPass = renderPass;
    skyCfg.setLayouts = { project->descSLayout_Global() };
    skyCfg.withShaders("Shaders/bin/Sky/sky.vert.spv", "Shaders/bin/Sky/sky.frag.spv")
        .withCulling(CullMode::None)
        .withDepthTest(false, VK_COMPARE_OP_LESS)
        .withDepthWrite(false);
    // No vertex input needed for sky (fullscreen quad)
    
    pipelineSky = MakeUnique<PipelineRaster>(device, skyCfg);

    // ===== Pipeline 4: Test =====

    RasterCfg testCfg;
    testCfg.renderPass = renderPass;
    testCfg
        .withDescriptorLayouts({
            project->descSLayout_Global(), // Set 0
            project->drawable().vrtxExtLayout(), // Set 1
            project->drawable().matDescLayout(), // Set 2
            project->drawable().texDescLayout(), // Set 3
            project->drawable().skinDescLayout(), // Set 4
            project->drawable().mrphWsDescLayout(), // Set 5
        })
        .withPushConstants(ShaderStage::VertexAndFragment, 0, 48) // 3 x uvec4
        .withShaders("Shaders/bin/Test/Test.vert.spv", "Shaders/bin/Test/Test.frag.spv")
        .withVertexInput(
            tinyDrawable::bindingDesc(),
            tinyDrawable::attributeDescs()
        );

    pipelineTest = MakeUnique<PipelineRaster>(device, testCfg);
    // ===== Initialize UI System =====
    initUI();

    windowManager->maximizeWindow();
    checkWindowResize();
}

bool tinyApp::checkWindowResize() {
    if (!windowManager->resizedFlag && !renderer->isResizeNeeded()) return false;

    windowManager->resizedFlag = false;
    renderer->setResizeHandled();

    int newWidth, newHeight;
    SDL_GetWindowSize(windowManager->window, &newWidth, &newHeight);

    project->camera()->setAspectRatio(
        static_cast<float>(newWidth) / static_cast<float>(newHeight)
    );

    // Handle window resize in renderer (now handles depth resources internally)
    renderer->handleWindowResize(windowManager->window);

    // Recreate all three pipelines with offscreen render pass for post-processing
    VkRenderPass renderPass = renderer->getMainRenderPass();

    // Update render pass for all pipeline configs
    pipelineSky->cfg.renderPass = renderPass;
    pipelineTest->cfg.renderPass = renderPass;

    // Recreate pipelines
    pipelineSky->recreate();
    pipelineTest->recreate();

    // Update UI render pass
    UIBackend->updateRenderPass(renderPass);
    UIBackend->rebuildIfNeeded();

    return true;
}


void tinyApp::mainLoop() {
    SDL_SetRelativeMouseMode(SDL_TRUE); // Start with mouse locked

    // Create references to avoid arrow spam
    auto& winManager = *windowManager;
    auto& fpsRef = *fpsManager;

    auto& camRef = *project->camera();

    auto& rendererRef = *renderer;

    while (!winManager.shouldCloseFlag) {
        // Update FPS manager for timing
        fpsRef.update();
        
        // Handle SDL events for both window manager and ImGui
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Let UI process the event first
            UIBackend->processEvent(&event);

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

                case SDL_DROPFILE: {
                    char* droppedFile = event.drop.file;
                    std::string ext = tinyFS::pExt(droppedFile);
                    std::string name = tinyFS::pName(droppedFile); // Without extension

                    if (ext == "glb" || ext == "gltf" || ext == "obj" || ext == "fbx") {
                        tinyModel model = tinyLoader::loadModel(droppedFile);
                        project->addModel(model);
                    }

                    // if (ext == "lua") {
                    //     tinyScript script;
                    //     script.code = tinyText::readFrom(droppedFile);

                    //     project->fs().createFile(name, std::move(script));
                    // }

                    // if (ext == "txt" || ext == "cfg" || ext == "ini" || ext == "log" || ext == "md") {
                    //     tinyText text;
                    //     text.str = tinyText::readFrom(droppedFile);

                    //     project->fs().createFile(name, std::move(text));
                    // }

                    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp") {
                        tinyTexture texture = tinyLoader::loadTexture(droppedFile);
                        texture.vkCreate(dvk.get());

                        project->fs().createFile(name, std::move(texture));
                    }

                } break;
            }
        }

        float deltaTime = fpsRef.deltaTime;

        static float cam_dist = 1.5f;
        static glm::vec3 camPos = camRef.pos;

        // Check if window was resized or renderer needs to be updated
        checkWindowResize();

        const Uint8* k_state = SDL_GetKeyboardState(nullptr);

        static float escHoldTime = 0.0f;
        static bool mouseFocusPressed = false;
        static bool mouseFocus = true;
        // Hold esc for 1 second to quit, otherwise toggle mouse focus
        if (k_state[SDL_SCANCODE_ESCAPE]) {
            escHoldTime += deltaTime;
            if (escHoldTime >= 1.0f) winManager.shouldCloseFlag = true;

            if (!mouseFocusPressed) {
                mouseFocus = !mouseFocus;
                mouseFocusPressed = true;
                if (mouseFocus) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    SDL_WarpMouseInWindow(winManager.window, 0, 0);
                } else {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
            }
        } else {
            escHoldTime = 0.0f;
            mouseFocusPressed = false;
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
        float p_speed = (fast ? 26.0f : (slow ? 0.5f : 8.0f)) * deltaTime;

        if (k_state[SDL_SCANCODE_W]) camPos += camRef.forward * p_speed;
        if (k_state[SDL_SCANCODE_S]) camPos -= camRef.forward * p_speed;
        if (k_state[SDL_SCANCODE_A]) camPos -= camRef.right * p_speed;
        if (k_state[SDL_SCANCODE_D]) camPos += camRef.right * p_speed;

        camRef.pos = camPos;
        camRef.update();

// =================================

        // Start new UI frame
        tinyUI::NewFrame();
        renderUI();

        updateActiveScene();
        project->updateGlobal(rendererRef.getCurrentFrame());

        uint32_t imageIndex = rendererRef.beginFrame();
        if (imageIndex != UINT32_MAX) {
            // Update global UBO buffer from frame index

            rendererRef.drawSky(project.get(), pipelineSky.get());

            // For the time being draw test pipeline
            rendererRef.drawTest(
                project.get(), curScene,
                pipelineTest.get()
            );

            // Render UI
            VkCommandBuffer currentCmd = rendererRef.getCurrentCommandBuffer();

            UIBackend->setCommandBuffer(currentCmd);
            tinyUI::Render();

            // End frame with ImGui rendering integrated
            rendererRef.endFrame(imageIndex);
            rendererRef.processPendingRemovals(project.get(), curScene);
        }

        // Clean window title - FPS info now in ImGui
        static bool titleSet = false;
        if (!titleSet) {
            SDL_SetWindowTitle(winManager.window, appTitle);
            titleSet = true;
        }
    }

    vkDeviceWaitIdle(dvk->device);
}