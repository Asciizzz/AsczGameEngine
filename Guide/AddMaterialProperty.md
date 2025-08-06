# Adding a New Material Property

### Step 1: Add to Material Struct
In `include/Az3D/Material.hpp`:

```cpp
struct Material {
    size_t diffTxtr = 0;
    glm::vec4 prop1 = glm::vec4(0.0f);
    glm::vec4 prop2 = glm::vec4(1.0f); // NEW
};
```

### Step 2: Add to MaterialUBO
In `include/AzVulk/Buffer.hpp`:

```cpp
struct MaterialUBO {
    alignas(16) glm::vec4 prop1;
    alignas(16) glm::vec4 prop2; // NEW (must match)
};
```

**Important:** Always use `alignas(16)` for proper GPU memory alignment!

### Step 3: Update Shader
In `Shaders/Rasterize/raster.frag`:

```glsl
layout(binding = 2) uniform MaterialUBO {
    vec4 prop1;
    vec4 prop2; // NEW
} material;

void main() {
    // Enjoy your new property
    vec4 myProperty = material.prop2;
    // ...
}
```

### Step 4: Update Buffer Material Update Logic
You'll need to ensure the new property is copied from `Material` to `MaterialUBO`.

In `buffer.cpp`, look for:

```cpp
void Buffer::updateMaterialUniformBuffer(size_t materialIndex, const Az3D::Material& material) {
    MaterialUBO ubo{};
    ubo.prop1 = material.prop1;
    ubo.prop2 = material.prop2; // NEW

    // ...
}
```

### Step 5: Recompile Shaders

```bash
# Run straight from root:
./compile_shader.bat  # Windows
# or (not yet implemented, sorry)
./compile_shader.sh   # Linux/Mac
```

## Vulkan Memory Alignment Rules

### Why Use `alignas(16)` and `glm::vec4`?

1. **GPU Memory Alignment**: GPUs require specific memory alignment for optimal performance
2. **Vulkan Specification**: Uniform buffer objects must follow std140 layout rules
3. **Cross-Platform Compatibility**: Ensures consistent behavior across different GPUs

### Best Practice:

Use `alignas(16) glm::vec4` for all properties, even when you don't need all 4 components. This automatically aligns your data to 16 bytes, which is GPU-friendly

## Critical Memory Alignment Warning

**EXTREMELY IMPORTANT**: When modifying the Material struct, always follow this memory alignment rule:

- **All `glm::vec4` shader input members MUST be declared first**

- **All `size_t` and other scalar members MUST be declared after vec4 members**

Correct Ordering:
```cpp
struct Material {
    glm::vec4 prop1; // MUST be first for 16-byte alignment
    glm::vec4 prop2; // Additional vec4s can follow
    size_t diffTxtr; // size_t comes AFTER vec4s
};
```

Incorrect Ordering (corrupt memory):
```cpp
struct Material {
    size_t diffTxtr; // Wrong! This breaks vec4 alignment
    glm::vec4 prop1; // Will be corrupted due to misalignment
};
```