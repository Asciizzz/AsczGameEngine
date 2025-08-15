@echo off

REM Rasterization shaders
glslc Shaders/Rasterize/raster.vert -o Shaders/Rasterize/raster.vert.spv
glslc Shaders/Rasterize/raster.frag -o Shaders/Rasterize/raster.frag.spv

REM Ineffcient but really cool sky shaders
glslc Shaders/Sky/sky.vert -o Shaders/Sky/sky.vert.spv
glslc Shaders/Sky/sky.frag -o Shaders/Sky/sky.frag.spv

REM Compute shaders
glslc Shaders/Compute/times2.comp -o Shaders/Compute/times2.comp.spv

echo Compiling completed!