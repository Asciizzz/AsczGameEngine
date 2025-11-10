#include "tinyApp/tinyApp.hpp"

#include <iostream>

#ifdef NDEBUG // Remember to set this to false
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
    if (imguiWrapper) {
        imguiWrapper->cleanup();
    }
    cleanup();
}

void tinyApp::cleanup() {
    // No clean up needed for now
}

void tinyApp::run() {
    mainLoop();

    printf("tinyApp exited successfully. See you next time!\n");
}

void tinyApp::initComponents() {

    windowManager = MakeUnique<tinyWindow>(appTitle, appWidth, appHeight);
    fpsManager = MakeUnique<tinyChrono>();

    auto extensions = windowManager->getRequiredVulkanExtensions();
    instanceVk = MakeUnique<Instance>(extensions, enableValidationLayers);
    instanceVk->createSurface(windowManager->window);

    deviceVk = MakeUnique<Device>(instanceVk->instance, instanceVk->surface);

    // So we dont have to write these things over and over again
    VkDevice device = deviceVk->device;
    VkPhysicalDevice pDevice = deviceVk->pDevice;

    // Create renderer (which now manages depth manager, swap chain and render passes)
    renderer = MakeUnique<Renderer>(
        deviceVk.get(),
        instanceVk->surface,
        windowManager->window,
        tinyApp::MAX_FRAMES_IN_FLIGHT
    );

    project = MakeUnique<tinyProject>(deviceVk.get());

    float aspectRatio = static_cast<float>(appWidth) / static_cast<float>(appHeight);
    project->camera()->setAspectRatio(aspectRatio);

// PLAYGROUND FROM HERE

    pipelineManager = MakeUnique<PipelineManager>();
    pipelineManager->loadPipelinesFromJson("Config/pipelines.json");

    // Initialize all pipelines with the manager using named layouts

    const tinySharedRes& sharedRes = project->sharedRes();

    UnorderedMap<std::string, VkDescriptorSetLayout> namedLayouts = {
        {"global", project->descSLayout_Global()},
        {"material", sharedRes.matDescLayout()},
        {"skin", sharedRes.skinDescLayout()},
        {"morph_ds", sharedRes.mrphDsDescLayout()},
        {"morph_ws", sharedRes.mrphWsDescLayout()}
    };
    
    // Create named vertex inputs
    UnorderedMap<std::string, VertexInputVk> VertexInputVks;
    
    // None - no vertex input (for fullscreen quads, etc.)
    VertexInputVks["None"] = VertexInputVk();

    auto vstaticLayout = tinyVertex::Static::layout();
    auto vstaticBind = vstaticLayout.bindingDesc();
    auto vstaticAttrs = vstaticLayout.attributeDescs();

    auto vriggedLayout = tinyVertex::Rigged::layout();
    auto vriggedBind = vriggedLayout.bindingDesc();
    auto vriggedAttrs = vriggedLayout.attributeDescs();

    VertexInputVks["TestRigged"] = VertexInputVk()
        .setBindings({ vriggedBind })
        .setAttributes({ vriggedAttrs });

    VertexInputVks["TestStatic"] = VertexInputVk()
        .setBindings({ vstaticBind })
        .setAttributes({ vstaticAttrs });

    // Use offscreen render pass for pipeline creation
    VkRenderPass offscreenRenderPass = renderer->getOffscreenRenderPass();
    PIPELINE_INIT(pipelineManager.get(), device, offscreenRenderPass, namedLayouts, VertexInputVks);

    // Load post-process effects from JSON configuration
    renderer->loadPostProcessEffectsFromJson("Config/postprocess.json");

    // Initialize ImGui - do this after renderer is fully set up
    imguiWrapper = MakeUnique<tinyImGui>();

    // ImGui now creates its own render pass using swapchain and depth info
    bool imguiInitSuccess = imguiWrapper->init(
        windowManager->window,
        instanceVk->instance,
        deviceVk.get(),
        renderer->getSwapChain(),
        renderer->getDepthManager()
    );

    windowManager->maximizeWindow();
    checkWindowResize();
}

bool tinyApp::checkWindowResize() {
    if (!windowManager->resizedFlag && !renderer->isResizeNeeded()) return false;

    windowManager->resizedFlag = false;
    renderer->setResizeHandled();

    int newWidth, newHeight;
    SDL_GetWindowSize(windowManager->window, &newWidth, &newHeight);

    project->camera()->updateAspectRatio(newWidth, newHeight);

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

        static float escHoldTime = 0.0f;
        static bool mouseFocusPressed = false;
        static bool mouseFocus = true;
        // Hold esc for 1 second to quit, otherwise toggle mouse focus
        if (k_state[SDL_SCANCODE_ESCAPE]) {
            escHoldTime += dTime;
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
        float p_speed = (fast ? 26.0f : (slow ? 0.5f : 8.0f)) * dTime;

        if (k_state[SDL_SCANCODE_W]) camPos += camRef.forward * p_speed;
        if (k_state[SDL_SCANCODE_S]) camPos -= camRef.forward * p_speed;
        if (k_state[SDL_SCANCODE_A]) camPos -= camRef.right * p_speed;
        if (k_state[SDL_SCANCODE_D]) camPos += camRef.right * p_speed;

        camRef.pos = camPos;

// =================================

        imguiWrapper->newFrame();
        project->updateGlobal(rendererRef.getCurrentFrame());

        uint32_t currentFrameIndex = rendererRef.getCurrentFrame();

        tinySceneRT* activeScene = project->fs().rGet<tinySceneRT>(project->initialSceneHandle);

        activeScene->setFStart({ currentFrameIndex, dTime });
        activeScene->update();

        uint32_t imageIndex = rendererRef.beginFrame();
        if (imageIndex != UINT32_MAX) {
            // Update global UBO buffer from frame index

            rendererRef.drawSky(project.get(), PIPELINE_INSTANCE(pipelineManager.get(), "Sky"));

            rendererRef.drawScene(
                project.get(), activeScene,
                PIPELINE_INSTANCE(pipelineManager.get(), "TestRigged"),
                PIPELINE_INSTANCE(pipelineManager.get(), "TestStatic")
            );

            // End frame with ImGui rendering integrated
            rendererRef.endFrame(imageIndex, imguiWrapper.get());
            rendererRef.processPendingRemovals(project.get(), activeScene);
        }

        // Clean window title - FPS info now in ImGui
        static bool titleSet = false;
        if (!titleSet) {
            SDL_SetWindowTitle(winManager.window, appTitle);
            titleSet = true;
        }
    }

    vkDeviceWaitIdle(deviceVk->device);
}