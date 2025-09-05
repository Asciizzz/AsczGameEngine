Let me give you some suggestion, instead of a void createMeshBuffers, do size_t createMeshBuffer that will push back the buffer, as well as returning the indices, this will be the data we need for the TinyModelPtr. The general flow would be:

TinyModelVK:
    Vector submesh indices
    Vector material indices (of same size with submesh indices)
    Skeleton index (if any, for now ignore)

Iterate every TinyModels:
    Create a TinyModelVK, add to the list

    Create a temporary textureVK index vector

    Iterate every TinyTextures:
        Create a TextureVK from the TinyTexture
        Push to the global TextureVK vector, get the global index
        Add global index to temporary textureVK index vector

    Create a temporary materialVK index vector

    Iterate every TinyMaterials:
        Create a MaterialVK from the TinyMaterial
        Map the texture index from the temporary textureVK index vector (The mapping process happen for MaterialVK, not TinyMaterial)

        Push to the global MaterialVK vector, get the global index
        Add global index to temporary materialVK index vector

    Iterate every TinySubmeshes:
        Create the vertex/index buffer
        Push to the global vertex/index buffer vector, get the global index

        Add global index to the TinyModelVK submesh index vector
        Add the temporary materialVK index[the mesh -> material index] to the TinyModelVK material index vector
