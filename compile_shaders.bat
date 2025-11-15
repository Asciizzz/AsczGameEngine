@echo off

REM Inefficient but really cool sky shaders
glslc Shaders/raw/Sky/sky.vert -o Shaders/bin/Sky/sky.vert.spv
glslc Shaders/raw/Sky/sky.frag -o Shaders/bin/Sky/sky.frag.spv

REM Single instance shader
glslc Shaders/raw/Rasterize/TestStatic.vert -o Shaders/bin/Rasterize/TestStatic.vert.spv
glslc Shaders/raw/Rasterize/TestRigged.vert -o Shaders/bin/Rasterize/TestRigged.vert.spv
glslc Shaders/raw/Rasterize/TestSingle.frag -o Shaders/bin/Rasterize/TestSingle.frag.spv


glslc Shaders/raw/Rasterize/MeshOnly.vert -o Shaders/bin/Rasterize/MeshOnly.vert.spv
glslc Shaders/raw/Rasterize/MeshOnly.frag -o Shaders/bin/Rasterize/MeshOnly.frag.spv

echo Compiling completed!