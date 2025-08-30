@echo off

REM Static mesh rasterization shaders
glslc Shaders/Rasterize/MeshStatic.vert -o Shaders/Rasterize/MeshStatic.vert.spv
glslc Shaders/Rasterize/MeshStatic.frag -o Shaders/Rasterize/MeshStatic.frag.spv

REM Skinning shaders
glslc Shaders/Rasterize/MeshSkinned.vert -o Shaders/Rasterize/MeshSkinned.vert.spv
glslc Shaders/Rasterize/MeshSkinned.frag -o Shaders/Rasterize/MeshSkinned.frag.spv

REM Inefficient but really cool sky shaders
glslc Shaders/Sky/sky.vert -o Shaders/Sky/sky.vert.spv
glslc Shaders/Sky/sky.frag -o Shaders/Sky/sky.frag.spv

REM Compute shaders
glslc Shaders/Compute/test.comp -o Shaders/Compute/test.comp.spv
glslc Shaders/Compute/grass.comp -o Shaders/Compute/grass.comp.spv

echo Compiling completed!