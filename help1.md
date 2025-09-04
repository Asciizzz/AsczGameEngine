### 1. Keep descriptions tied to the vertex format

```cpp
struct VertexLayout {
    uint32_t stride;
    std::vector<VertexAttribute> attributes;

    // Utility to generate Vulkan descriptions once
    VkVertexInputBindingDescription getBindingDescription() const {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = stride;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() const {
        std::vector<VkVertexInputAttributeDescription> descs;
        for (auto& attr : attributes) {
            VkVertexInputAttributeDescription d{};
            d.binding = 0;
            d.location = attr.location;
            d.format = attr.format;
            d.offset = attr.offset;
            descs.push_back(d);
        }
        return descs;
    }
};
```

Now your `StaticVertex::getLayout()` and `RigVertex::getLayout()` provide layouts that pipelines can use.

---

### 2. Mesh only holds raw data

```cpp
struct Mesh {
    std::vector<uint8_t> vertexData;
    std::vector<uint32_t> indices;
    VertexLayout layout;

    size_t vertexCount() const { return vertexData.size() / layout.stride; }

    template<typename VertexT>
    static Mesh create(const std::vector<VertexT>& verts,
                             const std::vector<uint32_t>& idx) 
    {
        Mesh mesh;
        mesh.layout = VertexT::getLayout();
        mesh.indices = idx;
        mesh.vertexData.resize(verts.size() * sizeof(VertexT));
        std::memcpy(mesh.vertexData.data(), verts.data(), mesh.vertexData.size());
        return mesh;
    }
};
```

Notice: no Vulkan calls here. Mesh is pure resource data.

---

### 3. Pipeline creation (once per vertex type)

When you decide to render static vs rigged meshes, you choose layouts **at pipeline creation**:

```cpp
// For static pipeline
VertexLayout staticLayout = StaticVertex::getLayout();
auto bindingDesc = staticLayout.getBindingDescription();
auto attrDescs  = staticLayout.getAttributeDescriptions();

// For rigged pipeline
VertexLayout rigLayout = RigVertex::getLayout();
auto rigBindingDesc = rigLayout.getBindingDescription();
auto rigAttrDescs   = rigLayout.getAttributeDescriptions();

// These get baked into VkPipelineVertexInputStateCreateInfo
```