# AsczGame Engine - Refactored Architecture

## Overview

The Vulkan application has been completely refactored from a monolithic class into a well-structured, scalable game engine architecture. The code is now organized into meaningful classes with clear responsibilities.

## Architecture

### Core Components

#### 1. **Application** (`src/AzGame/Application.cpp`)
- Main application class that orchestrates all engine components
- Handles the main loop and high-level initialization
- Manages component lifetimes and interactions

#### 2. **WindowManager** (`src/AzGame/WindowManager.cpp`)
- Manages SDL2 window creation and events
- Handles window resizing and input events
- Provides Vulkan surface extension requirements

#### 3. **VulkanInstance** (`src/AzGame/VulkanInstance.cpp`)
- Manages Vulkan instance creation and debug layers
- Handles validation layer setup and debug callbacks
- Encapsulates instance-level Vulkan operations

#### 4. **VulkanDevice** (`src/AzGame/VulkanDevice.cpp`)
- Manages physical and logical device selection
- Handles queue family discovery and creation
- Provides device-level Vulkan operations

#### 5. **SwapChain** (`src/AzGame/SwapChain.cpp`)
- Manages swap chain creation and recreation
- Handles surface format and present mode selection
- Creates and manages framebuffers

#### 6. **GraphicsPipeline** (`src/AzGame/GraphicsPipeline.cpp`)
- Creates and manages the Vulkan graphics pipeline
- Handles render pass creation
- Manages descriptor set layouts and pipeline layouts

#### 7. **ShaderManager** (`src/AzGame/ShaderManager.cpp`)
- Handles shader loading and compilation
- Manages shader module lifecycles
- Provides utility functions for shader operations

#### 8. **Buffer** (`src/AzGame/Buffer.cpp`)
- Manages Vulkan buffer creation (vertex, index, uniform)
- Handles memory allocation and mapping
- Provides buffer utility functions

#### 9. **Renderer** (`src/AzGame/Renderer.cpp`)
- Manages command buffer recording and submission
- Handles the rendering loop and synchronization
- Manages frame-in-flight synchronization objects

## Key Benefits

### 1. **Separation of Concerns**
Each class has a single, well-defined responsibility, making the code easier to understand and maintain.

### 2. **Modularity**
Components can be easily modified, extended, or replaced without affecting other parts of the system.

### 3. **Scalability**
The architecture supports easy addition of new features like:
- Multiple render passes
- Different pipeline types
- Advanced buffer management
- Scene management
- Asset loading systems

### 4. **Memory Safety**
Using RAII (Resource Acquisition Is Initialization) principles with proper destructors ensures automatic cleanup.

### 5. **Testability**
Individual components can be unit tested in isolation.

## Usage

The new architecture is much simpler to use:

```cpp
#include "AzGame/Application.hpp"

int main() {
    try {
        AzGame::Application app("My Game", 800, 600);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

## File Structure

```
include/AzGame/           # Header files
├── Application.hpp
├── WindowManager.hpp
├── VulkanInstance.hpp
├── VulkanDevice.hpp
├── SwapChain.hpp
├── GraphicsPipeline.hpp
├── ShaderManager.hpp
├── Buffer.hpp
└── Renderer.hpp

src/AzGame/              # Implementation files
├── Application.cpp
├── WindowManager.cpp
├── VulkanInstance.cpp
├── VulkanDevice.cpp
├── SwapChain.cpp
├── GraphicsPipeline.cpp
├── ShaderManager.cpp
├── Buffer.cpp
└── Renderer.cpp
```

## Future Enhancements

This architecture provides a solid foundation for adding:

1. **Scene Graph System** - For managing 3D objects and transformations
2. **Asset Management** - For loading models, textures, and other resources
3. **Input System** - For handling keyboard, mouse, and gamepad input
4. **Audio System** - For sound effects and music
5. **Physics Integration** - For realistic simulations
6. **Scripting Support** - For game logic and behavior
7. **GUI System** - For user interfaces
8. **Networking** - For multiplayer capabilities

## Dependencies

- **SDL2** - Cross-platform window and input management
- **Vulkan** - Low-level graphics API
- **GLM** - Mathematics library for graphics

The engine maintains compatibility with the existing shader files (`Shaders/hello.vert.spv` and `Shaders/hello.frag.spv`) and build system.
