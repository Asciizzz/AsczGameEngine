@echo off

REM Rasterization shaders
glslc Shaders/Rasterize/raster.vert -o Shaders/Rasterize/raster.vert.spv
glslc Shaders/Rasterize/raster.frag -o Shaders/Rasterize/raster.frag.spv

glslc Shaders/Sky/sky.vert -o Shaders/Sky/sky.vert.spv
glslc Shaders/Sky/sky.frag -o Shaders/Sky/sky.frag.spv

echo Compiling completed!