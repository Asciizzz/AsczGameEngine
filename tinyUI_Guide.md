# tinyUI - Renderer-Agnostic ImGui Abstraction

## Overview

`tinyUI` is a **completely renderer-agnostic** UI system built on top of Dear ImGui. It can work with any graphics API (Vulkan, OpenGL, DirectX, Metal, WebGPU, etc.) and is designed to be portable across projects.

## Architecture (3 Layers)

```
┌─────────────────────────────────────────┐
│      tinyUI::UI (Core API)              │  ← Your code uses this
│  - EditProperty<T>(label, value)        │
│  - Begin/End windows                    │
│  - Button, Text, TreeNode, etc.         │
└─────────────────────────────────────────┘
                   ↓
┌─────────────────────────────────────────┐
│      IUIBackend (Interface)             │  ← Backend abstraction
│  - init(), newFrame(), render()         │
└─────────────────────────────────────────┘
                   ↓
┌─────────────────────────────────────────┐
│   UIBackend_Vulkan (Implementation)     │  ← Graphics API specific
│   UIBackend_OpenGL                      │
│   UIBackend_DX11, etc.                  │
└─────────────────────────────────────────┘
```

## Files

- **`tinyUI.hpp`** - Core UI system (100% renderer-agnostic)
- **`tinyUI.cpp`** - Implementation of core system
- **`tinyUI_Vulkan.hpp`** - Vulkan backend (ONLY include if using Vulkan)

## Basic Usage

### 1. Include the headers

```cpp
#include "tinySystem/tinyUI.hpp"           // Always needed
#include "tinySystem/tinyUI_Vulkan.hpp"    // Only if using Vulkan
```

### 2. Initialize with your backend

```cpp
// Setup Vulkan backend data
tinyUI::VulkanBackendData vkData;
vkData.instance = vulkanInstance;
vkData.physicalDevice = physicalDevice;
vkData.device = device;
vkData.queueFamily = graphicsQueueFamily;
vkData.queue = graphicsQueue;
vkData.renderPass = imguiRenderPass;
vkData.minImageCount = 2;
vkData.imageCount = swapchainImageCount;

// Create Vulkan backend
auto* backend = new tinyUI::UIBackend_Vulkan(vkData);

// Initialize tinyUI
tinyUI::UI::Init(backend, sdlWindow);
```

### 3. Main loop

```cpp
while (running) {
    // Process events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        backend->processEvent(&event);  // Let ImGui handle events
        // ... your event handling
    }
    
    // Start new UI frame
    tinyUI::UI::NewFrame();
    
    // Draw your UI
    if (tinyUI::UI::Begin("My Window")) {
        tinyUI::UI::Text("Hello, World!");
        
        // Magic templated property editors!
        float myFloat = 5.0f;
        tinyUI::UI::EditProperty("Float", myFloat);
        
        glm::vec3 position(0, 0, 0);
        tinyUI::UI::EditProperty("Position", position);
        
        tinyUI::UI::End();
    }
    
    // Render
    // ... your rendering code ...
    
    // Set command buffer for ImGui rendering
    backend->setCommandBuffer(currentCommandBuffer);
    
    // Render UI
    tinyUI::UI::Render();
}

// Cleanup
tinyUI::UI::Shutdown();
```

## Advanced Features

### Templated Property Editors

The `EditProperty<T>()` function automatically creates the right widget for any type:

```cpp
int myInt = 42;
tinyUI::UI::EditProperty("Integer", myInt);

float myFloat = 3.14f;
tinyUI::UI::EditProperty("Float", myFloat);

bool myBool = true;
tinyUI::UI::EditProperty("Checkbox", myBool);

std::string myString = "Hello";
tinyUI::UI::EditProperty("Text", myString);

glm::vec3 color(1.0f, 0.5f, 0.2f);
tinyUI::UI::EditProperty("Color", color);
```

### Custom Theme

```cpp
tinyUI::UI::Theme myTheme;
myTheme.windowBg = ImVec4(0.1f, 0.1f, 0.15f, 1.0f);
myTheme.button = ImVec4(0.2f, 0.6f, 0.8f, 1.0f);
myTheme.frameRounding = 4.0f;

tinyUI::UI::SetTheme(myTheme);
```

### Window Resize Handling

```cpp
// When window resizes
backend->updateRenderPass(newRenderPass);
backend->rebuildIfNeeded();
```

## How to Port to Other Renderers

### Example: OpenGL Backend

```cpp
class UIBackend_OpenGL : public tinyUI::IUIBackend {
public:
    void init(const tinyUI::BackendInitInfo& info) override {
        auto* window = static_cast<GLFWwindow*>(info.windowHandle);
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }
    
    void newFrame() override {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }
    
    void renderDrawData(ImDrawData* drawData) override {
        ImGui_ImplOpenGL3_RenderDrawData(drawData);
    }
    
    void shutdown() override {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
    }
    
    const char* getName() const override {
        return "OpenGL";
    }
};
```

Then use it:

```cpp
auto* backend = new tinyUI::UIBackend_OpenGL();
tinyUI::UI::Init(backend, glfwWindow);
```

## Integration with Current AsczGameEngine

In your `tinyApp.hpp`:

```cpp
#include "tinySystem/tinyUI.hpp"
#include "tinySystem/tinyUI_Vulkan.hpp"

class tinyApp {
private:
    tinyUI::UIBackend_Vulkan* uiBackend = nullptr;
    // ... rest of your members
};
```

In `tinyApp_basic.cpp`:

```cpp
void tinyApp::initComponents() {
    // ... your existing initialization ...
    
    // Setup UI backend
    tinyUI::VulkanBackendData vkData;
    vkData.instance = instanceVk->instance;
    vkData.physicalDevice = deviceVk->pDevice;
    vkData.device = deviceVk->device;
    vkData.queueFamily = deviceVk->queueFamilyIndices.graphicsFamily.value();
    vkData.queue = deviceVk->graphicsQueue;
    vkData.renderPass = renderer->getOffscreenRenderPass();
    vkData.minImageCount = 2;
    vkData.imageCount = renderer->getSwapchain()->getImageCount();
    
    uiBackend = new tinyUI::UIBackend_Vulkan(vkData);
    tinyUI::UI::Init(uiBackend, windowManager->window);
}

void tinyApp::mainLoop() {
    while (!winManager.shouldCloseFlag) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            uiBackend->processEvent(&event);
            // ... your event handling
        }
        
        tinyUI::UI::NewFrame();
        
        // Your UI code here
        if (tinyUI::UI::Begin("Debug Info")) {
            tinyUI::UI::Text("FPS: %.1f", fpsRef.fps);
            tinyUI::UI::EditProperty("Camera Position", camRef.pos);
            tinyUI::UI::End();
        }
        
        // ... rendering ...
        
        uiBackend->setCommandBuffer(currentCommandBuffer);
        tinyUI::UI::Render();
    }
    
    tinyUI::UI::Shutdown();
}
```

## Benefits

✅ **100% Renderer Agnostic** - No Vulkan dependencies in core  
✅ **Single File Include** - Just include `tinyUI.hpp`  
✅ **Portable** - Use in any future project  
✅ **Type-Safe** - Templated property editors  
✅ **Extensible** - Easy to add new backends  
✅ **Clean API** - Simple, intuitive interface  

## Future Backends You Can Add

- `UIBackend_OpenGL.hpp`
- `UIBackend_DX11.hpp`
- `UIBackend_DX12.hpp`
- `UIBackend_Metal.hpp`
- `UIBackend_WebGPU.hpp`

Each backend is just one file that implements `IUIBackend`!
