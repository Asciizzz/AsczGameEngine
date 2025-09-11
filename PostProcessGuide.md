# Post-Processing Guide

## Overview
The AsczGame engine now features a comprehensive compute-based post-processing system with ping-pong buffer architecture, depth buffer access, and built-in effects including FXAA anti-aliasing.

## Architecture

### Core Components
- **PostProcess Class**: Main post-processing manager
- **Ping-Pong Buffers**: Dual render targets for multi-pass effects
- **Compute Pipelines**: GPU compute shaders for efficient processing
- **Depth Buffer Integration**: Access to scene depth for advanced effects

### Pipeline Flow
```
Scene Render → Offscreen Buffer → Post-Process Chain → Final Blit → Swapchain
```

## Quick Start

### Adding a New Effect
```cpp
// In Application.cpp initialization
renderer->addPostProcessEffect("effect_name", "Shaders/PostProcess/effect.comp.spv");
```

### Effect Order
Effects are processed in the order they're added:
```cpp
renderer->addPostProcessEffect("effect1", "Shaders/PostProcess/effect1.comp.spv");
renderer->addPostProcessEffect("effect2", "Shaders/PostProcess/effect2.comp.spv");
renderer->addPostProcessEffect("effect3", "Shaders/PostProcess/effect3.comp.spv");
```

## Creating Custom Effects

### 1. Shader Structure
Create a compute shader in `Shaders/PostProcess/`:

```glsl
#version 450
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform sampler2D inputImage;
layout(binding = 1, rgba8) uniform image2D outputImage;
layout(binding = 2) uniform sampler2D depthTexture; // Optional

void main() {
    ivec2 texCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imgSize = imageSize(outputImage);
    
    if (texCoord.x >= imgSize.x || texCoord.y >= imgSize.y) {
        return;
    }
    
    vec2 uv = (vec2(texCoord) + 0.5) / vec2(imgSize);
    
    // Sample input
    vec3 color = texture(inputImage, uv).rgb;
    
    // Your effect processing here
    // ...
    
    // Optional: Use depth for edge detection, etc.
    float depth = texture(depthTexture, uv).r;
    
    // Write output
    imageStore(outputImage, texCoord, vec4(color, 1.0));
}
```

### 2. Compile Shader
```bash
glslc Shaders/PostProcess/your_effect.comp -o Shaders/PostProcess/your_effect.comp.spv
```

### 3. Add to Engine
```cpp
renderer->addPostProcessEffect("your_effect", "Shaders/PostProcess/your_effect.comp.spv");
```

## Available Bindings

### Descriptor Set Layout
- **Binding 0**: `sampler2D inputImage` - Input color texture
- **Binding 1**: `image2D outputImage` - Output storage image (rgba8)
- **Binding 2**: `sampler2D depthTexture` - Scene depth buffer

### Image Formats
- **Color Buffers**: `VK_FORMAT_R8G8B8A8_UNORM` (compute-compatible)
- **Depth Buffer**: Managed by DepthManager (typically `VK_FORMAT_D32_SFLOAT`)

## Built-in Effects

### FXAA (Fast Approximate Anti-Aliasing)
```glsl
// Reduces aliasing artifacts using luminance-based edge detection
// Enhanced with depth buffer edge detection for improved quality
renderer->addPostProcessEffect("fxaa", "Shaders/PostProcess/fxaa.comp.spv");
```

### Tone Mapping
```glsl
// Reinhard tone mapping for HDR to LDR conversion
vec3 tonemap(vec3 color) {
    return color / (color + vec3(1.0));
}
```

### Blur
```glsl
// 3x3 Gaussian blur kernel
// Useful for bloom, depth of field, or general softening
```

## Performance Tips

### Workgroup Sizes
- Use 16x16 workgroups for optimal GPU utilization
- Ensure proper bounds checking for non-multiple dimensions

### Memory Access Patterns
- Prefer texture sampling over direct image loads for input
- Use storage images for output to avoid unnecessary barriers

### Pipeline Barriers
The system automatically handles:
- Color attachment → Compute shader transitions
- Depth attachment → Compute shader transitions  
- Compute → Compute barriers between passes
- Final blit to swapchain

## Advanced Techniques

### Depth-Based Effects
```glsl
// Edge detection using depth discontinuities
float getDepthEdge(sampler2D depthTex, vec2 uv, vec2 texelSize) {
    float depth = texture(depthTex, uv).r;
    float depthN = texture(depthTex, uv + vec2(0.0, -texelSize.y)).r;
    float depthS = texture(depthTex, uv + vec2(0.0, texelSize.y)).r;
    float depthW = texture(depthTex, uv + vec2(-texelSize.x, 0.0)).r;
    float depthE = texture(depthTex, uv + vec2(texelSize.x, 0.0)).r;
    
    return abs(depth - depthN) + abs(depth - depthS) + 
           abs(depth - depthW) + abs(depth - depthE);
}
```

### Multi-Pass Effects
Effects are automatically chained with ping-pong buffers:
- Effect 1: A → B
- Effect 2: B → A  
- Effect 3: A → B
- Final result in A (even count) or B (odd count)

### Custom Sampling
```glsl
// Use different sampling modes for different effects
vec3 color = texture(inputImage, uv).rgb;           // Linear filtering
vec3 color = texelFetch(inputImage, ivec2(uv * textureSize(inputImage, 0)), 0).rgb; // Nearest/exact
```

## Debugging

### Validation Layers
The system provides clean validation with no errors when properly implemented:
- No format compatibility issues
- Proper pipeline barriers
- Correct layout transitions

### Common Issues
1. **Shader compilation errors**: Check GLSL syntax and binding declarations
2. **Black screen**: Verify effect order and ensure at least one effect processes the input
3. **Performance drops**: Check workgroup sizes and avoid expensive operations in inner loops

## Future Extensions

The architecture supports:
- **Temporal effects** (by storing previous frame data)
- **Compute-based particle systems**
- **Screen-space reflections using depth buffer**
- **Advanced post-processing (SSAO, SSR, temporal anti-aliasing)**

## Integration Notes

The post-processing system integrates seamlessly with:
- **Vulkan 1.4.321.0** compute pipelines
- **DepthManager** for depth buffer access
- **SwapChain** management and resize handling
- **Pipeline recreation** during window changes

For more advanced effects or custom integrations, refer to the PostProcess.hpp/cpp implementation and the existing shader examples in `Shaders/PostProcess/`.
