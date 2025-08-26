// Draw the scene using a graphics pipeline and a group of models

Get material, texture, and mesh managers
Get global descriptor set for current frame

Bind graphics pipeline

For each model in the group:
    Get instance count
    If no instances, skip

    Update instance buffer data

    Decode model hash to get mesh and material indices

    Get descriptor sets for material and texture

    Bind global, material, and texture descriptor sets

    Get mesh data and index count

    If no indices or invalid instance buffer, skip

    Bind vertex and instance buffers
    Bind index buffer

    Draw indexed geometry for all instances
