// ImGui Integration Example for AsczGameEngine
// Add this to your Application.hpp and Application.cpp

// In Application.hpp, add this include and member:
#include "AzCore/ImGuiWrapper.hpp"

class Application {
    // ... existing members ...
    
    // Add this member:
    UniquePtr<ImGuiWrapper> imguiWrapper;
    
    // Add these demo variables:
    bool showDemoWindow = true;
    bool showDebugWindow = true;
    float clearColor[3] = {0.0f, 0.0f, 0.0f};
};

// In Application.cpp, integrate like this:

// 1. Initialize ImGui after renderer is created (in initComponents):
void Application::initComponents() {
    // ... existing initialization code ...
    
    // After renderer is created:
    imguiWrapper = MakeUnique<ImGuiWrapper>();
    imguiWrapper->init(
        windowManager->window,
        vkInstance->instance,       // Pass the VkInstance
        deviceVK.get(),
        renderer->getRenderPass(),  // You need to expose this method
        renderer->getImageCount()   // You need to expose this method
    );
}

// 2. Handle events in mainLoop:
void Application::mainLoop() {
    // ... existing code ...
    
    while (!winManager.shouldCloseFlag) {
        // ... existing update code ...
        
        // Process SDL events for ImGui
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            imguiWrapper->processEvent(&event);
            
            // Handle your own events here too
            if (event.type == SDL_QUIT) {
                winManager.shouldCloseFlag = true;
            }
        }
        
        // Start new ImGui frame
        imguiWrapper->newFrame();
        
        // Create your ImGui windows here:
        if (showDemoWindow) {
            imguiWrapper->showDemoWindow(&showDemoWindow);
        }
        
        if (showDebugWindow) {
            ImGui::Begin("Debug Window", &showDebugWindow);
            
            ImGui::Text("FPS: %.1f", fpsRef.currentFPS);
            ImGui::Text("Frame Time: %.2f ms", fpsRef.frameTimeMs);
            
            ImGui::Separator();
            ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", 
                       camRef.pos.x, camRef.pos.y, camRef.pos.z);
            
            ImGui::ColorEdit3("Clear Color", clearColor);
            
            if (ImGui::Button("Reset Camera")) {
                camRef.pos = glm::vec3(0.0f, 0.0f, 0.0f);
            }
            
            ImGui::End();
        }
        
        // ... existing rendering code ...
        
        uint32_t imageIndex = rendererRef.beginFrame();
        if (imageIndex != UINT32_MAX) {
            // ... existing rendering ...
            
            // Render ImGui (add this before endFrame)
            VkCommandBuffer cmdBuffer = rendererRef.getCurrentCommandBuffer();
            imguiWrapper->render(cmdBuffer);
            
            rendererRef.endFrame(imageIndex);
        }
    }
}

// 3. Cleanup in destructor:
Application::~Application() {
    if (imguiWrapper) {
        imguiWrapper->cleanup();
    }
    cleanup();
}

// Example ImGui Features You Can Use:

void createDebugUI() {
    // Basic window
    ImGui::Begin("My Window");
    ImGui::Text("Hello, World!");
    
    // Sliders and inputs
    static float f = 0.0f;
    ImGui::SliderFloat("Float Slider", &f, 0.0f, 1.0f);
    
    // Buttons
    if (ImGui::Button("Click Me!")) {
        // Do something
    }
    
    // Checkboxes
    static bool enabled = true;
    ImGui::Checkbox("Enable Feature", &enabled);
    
    // Color picker
    static float color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    ImGui::ColorEdit4("Color", color);
    
    // Tree nodes for organization
    if (ImGui::TreeNode("Advanced Settings")) {
        ImGui::Text("More options here...");
        ImGui::TreePop();
    }
    
    // Input fields
    static char text[256] = "Hello";
    ImGui::InputText("Text Input", text, sizeof(text));
    
    ImGui::End();
}