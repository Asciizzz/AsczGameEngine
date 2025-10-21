#include "TinyApp/TinyApp.hpp"

#include <iostream>

#ifdef NDEBUG // Remember to set this to false
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = true;
#endif

using namespace TinyVK;

TinyApp::TinyApp(const char* title, uint32_t width, uint32_t height)
    : appTitle(title), appWidth(width), appHeight(height) {

    initComponents();
}

TinyApp::~TinyApp() {
    if (imguiWrapper) {
        imguiWrapper->cleanup();
    }
    cleanup();
}

void TinyApp::cleanup() {
    // No clean up needed for now
}

void TinyApp::run() {
    mainLoop();

    printf("TinyApp exited successfully. See you next time!\n");
}

void TinyApp::initComponents() {

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
        TinyApp::MAX_FRAMES_IN_FLIGHT
    );

    project = MakeUnique<TinyProject>(deviceVK.get());
    
    // Initialize the active scene handle (moved from TinyProject for better separation)
    activeSceneHandle = project->getInitialSceneHandle();

    // Initialize selected handle to root scene node (already done in TinyProject constructor)
    // project->selectSceneNode(project->getRootNodeHandle());

    float aspectRatio = static_cast<float>(appWidth) / static_cast<float>(appHeight);
    project->getCamera()->setAspectRatio(aspectRatio);

// PLAYGROUND FROM HERE

    pipelineManager = MakeUnique<PipelineManager>();
    pipelineManager->loadPipelinesFromJson("Config/pipelines.json");

    // Initialize all pipelines with the manager using named layouts
    UnorderedMap<std::string, VkDescriptorSetLayout> namedLayouts = {
        {"global", project->getGlbDescSetLayout()},
        {"skin", project->getSkinDescSetLayout()},
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

    vertexInputVKs["TestRigged"] = VertexInputVK()
        .setBindings({ vriggedBind })
        .setAttributes({ vriggedAttrs });

    vertexInputVKs["TestStatic"] = VertexInputVK()
        .setBindings({ vstaticBind })
        .setAttributes({ vstaticAttrs });

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

    windowManager->maximizeWindow();
    checkWindowResize();
}

bool TinyApp::checkWindowResize() {
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


void TinyApp::mainLoop() {
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

        imguiWrapper->newFrame();

        project->updateGlobal(rendererRef.getCurrentFrame());

        uint32_t imageIndex = rendererRef.beginFrame();
        if (imageIndex != UINT32_MAX) {
            // Update global UBO buffer from frame index
            uint32_t currentFrameIndex = rendererRef.getCurrentFrame();

            rendererRef.drawSky(project.get(), PIPELINE_INSTANCE(pipelineManager.get(), "Sky"));

            rendererRef.drawScene(project.get(), getActiveScene(), PIPELINE_INSTANCE(pipelineManager.get(), "TestRigged"),
                                                PIPELINE_INSTANCE(pipelineManager.get(), "TestStatic"));

            // End frame with ImGui rendering integrated
            rendererRef.endFrame(imageIndex, imguiWrapper.get());

            // Process any pending deletions after the frame is complete
            // to avoid deleting resources in used by Vulkan
            processPendingDeletions();
        }

        // Clean window title - FPS info now in ImGui
        static bool titleSet = false;
        if (!titleSet) {
            SDL_SetWindowTitle(winManager.window, appTitle);
            titleSet = true;
        }
    }

    vkDeviceWaitIdle(deviceVK->device);
}