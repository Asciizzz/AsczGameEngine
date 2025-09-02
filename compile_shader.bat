@echo off

REM Static mesh rasterization shaders
glslc Shaders/Rasterize/StaticMesh.vert -o Shaders/Rasterize/StaticMesh.vert.spv
glslc Shaders/Rasterize/StaticMesh.frag -o Shaders/Rasterize/StaticMesh.frag.spv

REM Skinning shaders
glslc Shaders/Rasterize/RigMesh.vert -o Shaders/Rasterize/RigMesh.vert.spv
glslc Shaders/Rasterize/RigMesh.frag -o Shaders/Rasterize/RigMesh.frag.spv

REM Inefficient but really cool sky shaders
glslc Shaders/Sky/sky.vert -o Shaders/Sky/sky.vert.spv
glslc Shaders/Sky/sky.frag -o Shaders/Sky/sky.frag.spv

REM Compute shaders
glslc Shaders/Compute/test.comp -o Shaders/Compute/test.comp.spv
glslc Shaders/Compute/grass.comp -o Shaders/Compute/grass.comp.spv

echo Compiling completed!