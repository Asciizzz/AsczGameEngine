# Adding a New Material Property

### Step 1: Add to Material Struct
Edit `include/Az3D/Material.hpp`:

```cpp
struct Material {
    size_t diffTxtr = 0;
    glm::vec4 prop1 = glm::vec4(0.0f);
    glm::vec4 prop2 = glm::vec4(1.0f); // NEW: Add your new property
};
```

### Step 2: Add to MaterialUBO
Edit `include/AzVulk/Buffer.hpp`:

```cpp
struct MaterialUBO {
    alignas(16) glm::vec4 prop1;
    alignas(16) glm::vec4 prop2; // NEW: Must match Material::prop2
};
```

**Important:** Always use `alignas(16)` for proper GPU memory alignment!

### Step 3: Update Shader
Edit `Shaders/Rasterize/raster.frag` (and any other shaders using materials):

```glsl
layout(binding = 2) uniform MaterialUBO {
    vec4 prop1;
    vec4 prop2; // NEW: Add to shader
} material;

void main() {
    // Now you can use material.prop2 in your shader
    vec4 myProperty = material.prop2;
    // ...
}
```

### Step 4: Update Buffer Material Update Logic
Find where materials are updated in the buffer (typically in `Buffer.cpp`). You'll need to ensure the new property is copied from `Material` to `MaterialUBO`.

Look for code like:
```cpp
void Buffer::updateMaterialUniformBuffer(size_t materialIndex, const Az3D::Material& material) {
    MaterialUBO ubo{};
    ubo.prop1 = material.prop1;
    ubo.prop2 = material.prop2; // NEW: Add this line
    
    // Copy to buffer...
}
```

### Step 5: Recompile Shaders
After modifying shader files, recompile them:
```bash
# Navigate to your project directory
cd path/to/AsczGameEngine

# Run the shader compilation script
./compile_shader.bat  # Windows
# or
./compile_shader.sh   # Linux/Mac
```

## Property Usage Examples

### Example 1: PBR Material Properties
```cpp
// In your material setup code:
Az3D::Material pbrMaterial;
pbrMaterial.diffTxtr = diffuseTextureIndex;
pbrMaterial.prop1 = glm::vec4(
    roughness,    // x: Surface roughness (0.0 - 1.0)
    metallic,     // y: Metallic factor (0.0 - 1.0)
    emissive,     // z: Emissive strength
    alpha         // w: Alpha/transparency
);
```

### Example 2: Animation Properties
```cpp
// In your material setup code:
Az3D::Material animMaterial;
animMaterial.prop2 = glm::vec4(
    time,         // x: Animation time offset
    speed,        // y: Animation speed multiplier
    amplitude,    // z: Animation amplitude
    frequency     // w: Animation frequency
);
```

### Example 3: Using in Shaders
```glsl
void main() {
    // Access PBR properties
    float roughness = material.prop1.x;
    float metallic = material.prop1.y;
    float emissive = material.prop1.z;
    float alpha = material.prop1.w;
    
    // Access animation properties
    float time = material.prop2.x;
    float speed = material.prop2.y;
    
    // Use in calculations...
    vec3 finalColor = calculatePBR(roughness, metallic, emissive);
    finalColor += animateColor(time, speed);
    
    outColor = vec4(finalColor, alpha);
}
```

## Vulkan Memory Alignment Rules

### Why Use `alignas(16)` and `glm::vec4`?

1. **GPU Memory Alignment**: GPUs require specific memory alignment for optimal performance
2. **Vulkan Specification**: Uniform buffer objects must follow std140 layout rules
3. **Cross-Platform Compatibility**: Ensures consistent behavior across different GPUs

### Alignment Rules Summary:
- `float` → 4 bytes
- `glm::vec2` → 8 bytes
- `glm::vec3` → 12 bytes (but takes 16 bytes due to alignment)
- `glm::vec4` → 16 bytes ✅ (perfectly aligned)
- Arrays → Each element aligned to 16 bytes

### ❌ Don't Do This:
```cpp
struct MaterialUBO {
    float roughness;  // Poor alignment
    float metallic;   // Wastes memory
    float emissive;   // GPU performance hit
};
```

### ✅ Do This Instead:
```cpp
struct MaterialUBO {
    alignas(16) glm::vec4 pbrProps; // roughness, metallic, emissive, alpha
};
```

## Common Pitfalls and Solutions

### Problem 1: Shader Compilation Errors
**Error**: `'material' : undeclared identifier`
**Solution**: Make sure the uniform block name matches exactly between C++ and GLSL

### Problem 2: Incorrect Values in Shader
**Error**: Getting wrong values or garbage data
**Solution**: Check that:
- MaterialUBO struct matches Material struct
- Proper `alignas(16)` is used
- Buffer update code copies all properties
- Shader accesses correct components (.x, .y, .z, .w)

### Problem 3: GPU Memory Issues
**Error**: Rendering artifacts or crashes
**Solution**: Always use `alignas(16)` and `glm::vec4` for uniform buffer members

## Advanced Tips

### Tip 1: Naming Convention
Use descriptive names for your vec4 components:
```glsl
// Instead of material.prop1.x, use descriptive names:
#define ROUGHNESS material.prop1.x
#define METALLIC material.prop1.y
#define EMISSIVE material.prop1.z
#define ALPHA material.prop1.w
```

### Tip 2: Multiple Materials
For complex materials, you can pack multiple properties efficiently:
```cpp
material.prop1 = glm::vec4(roughness, metallic, emissive, alpha);
material.prop2 = glm::vec4(normalStrength, parallaxHeight, detailTiling, detailStrength);
material.prop3 = glm::vec4(animTime, animSpeed, windStrength, windDirection);
```

### Tip 3: Conditional Compilation
Use preprocessor directives in shaders for optional features:
```glsl
#ifdef ENABLE_PBR
    float roughness = material.prop1.x;
    float metallic = material.prop1.y;
    // PBR calculations...
#endif
```

## Critical Memory Alignment Warning ⚠️

**EXTREMELY IMPORTANT**: When modifying the Material struct, always follow this memory alignment rule:

- **All `glm::vec4` members MUST be declared first**
- **All `size_t` and other scalar members MUST be declared after vec4 members**

Example of CORRECT ordering:
```cpp
struct Material {
    glm::vec4 prop1;     // MUST be first for 16-byte alignment
    glm::vec4 prop2;     // Additional vec4s can follow
    size_t diffTxtr;     // size_t comes AFTER vec4s
    size_t normalTxtr;   // Additional scalars after vec4s
};
```

Example of INCORRECT ordering (will cause memory corruption):
```cpp
struct Material {
    size_t diffTxtr;     // WRONG! This breaks vec4 alignment
    glm::vec4 prop1;     // Will be corrupted due to misalignment
};
```

**Why this matters**: GLM vector types require 16-byte alignment for SIMD operations. If a `size_t` (8 bytes on 64-bit) is placed before a `glm::vec4`, it breaks the alignment and causes severe memory corruption where vector components get corrupted values like `-5.78737e-24`.

**Testing alignment**: After any struct changes, verify in your shader that vec4 components contain expected values, not garbage data.

## Testing Your Changes

1. **Compile**: Make sure your code compiles without errors
2. **Shader Compilation**: Verify shaders compile successfully
3. **Runtime Testing**: Test with different material values
4. **Visual Validation**: Ensure the material properties affect rendering as expected
5. **Performance**: Monitor frame rate to ensure no performance regression

## File Locations Summary

- **Material Definition**: `include/Az3D/Material.hpp`
- **Buffer Definition**: `include/AzVulk/Buffer.hpp` 
- **Shader Files**: `Shaders/Rasterize/raster.frag` (and others)
- **Buffer Logic**: `src/AzVulk/Buffer.cpp`
- **Material Creation**: `src/AzCore/Application.cpp` (or your app code)

Remember: The key to success is keeping the C++ structs and GLSL uniform blocks synchronized!
