# ImGui Integration Guide for AsczGameEngine

## 🎯 How ImGui Works

ImGui (Immediate Mode GUI) is a lightweight, immediate-mode graphical user interface library. Unlike traditional retained-mode GUIs, ImGui doesn't store widget state between frames - you recreate the UI every frame.

## 🔗 Integration Points

### 1. **SDL2 Backend** (`imgui_impl_sdl2`)
- **Purpose**: Handles input (mouse, keyboard, gamepad)
- **What it does**: 
  - Processes SDL events and converts them to ImGui input
  - Manages window focus, mouse cursors, clipboard
  - Handles keyboard/gamepad navigation

### 2. **Vulkan Backend** (`imgui_impl_vulkan`)
- **Purpose**: Renders ImGui draw commands using Vulkan
- **What it does**:
  - Creates Vulkan resources (buffers, textures, pipelines)
  - Converts ImGui draw data to Vulkan draw calls
  - Manages font textures and descriptor sets

## 🏗️ Architecture

```
Your Application
     ↓
┌─────────────────┐    ┌──────────────────┐
│   SDL Events    │───→│  ImGui SDL Back  │
│ (Mouse/Keyboard)│    │  end Processes   │
└─────────────────┘    └──────────────────┘
                              ↓
┌─────────────────┐    ┌──────────────────┐
│   ImGui::*()    │───→│   ImGui Core     │
│ (Your UI Code)  │    │  Generates Draw  │
└─────────────────┘    └──────────────────┘
                              ↓
┌─────────────────┐    ┌──────────────────┐
│  Vulkan Render  │←───│ ImGui Vulkan     │
│   Commands      │    │    Backend       │
└─────────────────┘    └──────────────────┘
```

## 🚀 Quick Start Integration

### Step 1: Add to Application.hpp
```cpp
#include "AzCore/ImGuiWrapper.hpp"

class Application {
    UniquePtr<ImGuiWrapper> imguiWrapper;
    bool showDemoWindow = true;
};
```

### Step 2: Initialize (in initComponents)
```cpp
// After renderer initialization:
imguiWrapper = MakeUnique<ImGuiWrapper>();
imguiWrapper->init(windowManager->window, deviceVK.get(), 
                  renderer->getRenderPass(), renderer->getImageCount());
```

### Step 3: Update Main Loop
```cpp
void Application::mainLoop() {
    while (!winManager.shouldCloseFlag) {
        // 1. Process events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            imguiWrapper->processEvent(&event);
            // Handle your events...
        }
        
        // 2. Start ImGui frame
        imguiWrapper->newFrame();
        
        // 3. Create your UI
        if (showDemoWindow) {
            ImGui::Begin("Debug Panel");
            ImGui::Text("FPS: %.1f", fpsManager->currentFPS);
            ImGui::End();
        }
        
        // 4. Render scene
        uint32_t imageIndex = renderer->beginFrame();
        if (imageIndex != UINT32_MAX) {
            // Your scene rendering...
            
            // 5. Render ImGui
            imguiWrapper->render(renderer->getCurrentCommandBuffer());
            
            renderer->endFrame(imageIndex);
        }
    }
}
```

## 🎨 Common UI Patterns

### Debug Information Panel
```cpp
ImGui::Begin("Debug Info");
ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 
           1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
ImGui::Text("Camera: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
ImGui::End();
```

### Scene Controls
```cpp
ImGui::Begin("Scene Controls");
ImGui::SliderFloat("Camera Speed", &cameraSpeed, 0.1f, 10.0f);
ImGui::ColorEdit3("Background", clearColor);
if (ImGui::Button("Reset Scene")) {
    // Reset logic
}
ImGui::End();
```

### Performance Monitor
```cpp
ImGui::Begin("Performance");
ImGui::PlotLines("Frame Times", frameTimes, frameTimeCount);
ImGui::Text("GPU Memory: %zu MB", gpuMemoryUsed / (1024*1024));
ImGui::End();
```

## ⚙️ Configuration Options

### Styling
```cpp
// Dark theme (default)
ImGui::StyleColorsDark();

// Light theme
ImGui::StyleColorsLight();

// Custom colors
ImGuiStyle& style = ImGui::GetStyle();
style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.9f);
```

### Features
```cpp
ImGuiIO& io = ImGui::GetIO();
io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Keyboard navigation
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Docking windows
io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Multi-viewport
```

## 🚨 Important Notes

### Input Handling
- ImGui processes input FIRST, then your application
- Use `ImGui::GetIO().WantCaptureMouse` to check if ImGui wants mouse input
- Use `ImGui::GetIO().WantCaptureKeyboard` for keyboard input

### Rendering Order
- ImGui renders LAST (after your scene)
- ImGui uses alpha blending, so it appears on top
- Make sure to call `imguiWrapper->render()` in the correct render pass

### Performance
- ImGui is very lightweight (~1-2ms per frame for complex UIs)
- Uses immediate mode - no retained state between frames
- Efficient vertex/index buffer usage

## 🛠️ Troubleshooting

### Common Issues:
1. **ImGui not rendering**: Check render pass compatibility
2. **Input not working**: Ensure `processEvent()` is called
3. **Fonts blurry**: Check DPI scaling settings
4. **Crashes on cleanup**: Make sure to call `cleanup()` before Vulkan cleanup

### Debug Tips:
- Use `ImGui::ShowDemoWindow()` to test basic functionality
- Check Vulkan validation layers for ImGui-related errors
- Verify descriptor pool has enough space for ImGui resources

## 🎉 What You Can Build

With ImGui integrated, you can create:
- **Debug panels** for real-time variable editing
- **Scene inspectors** for object properties
- **Performance profilers** with graphs and metrics
- **Asset browsers** for loading models/textures
- **Shader editors** with live recompilation
- **Game settings** panels
- **Console output** windows

ImGui is perfect for development tools and debug interfaces!