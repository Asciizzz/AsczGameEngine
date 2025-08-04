@echo off

glslc Shaders/Rasterize/raster.vert -o Shaders/Rasterize/raster.vert.spv
glslc Shaders/Rasterize/raster.frag -o Shaders/Rasterize/raster.frag.spv
glslc Shaders/Rasterize/raster1.frag -o Shaders/Rasterize/raster1.frag.spv

REM Transparency shaders for OIT
glslc Shaders/Rasterize/transparent.vert -o Shaders/Rasterize/transparent.vert.spv
glslc Shaders/Rasterize/transparent.frag -o Shaders/Rasterize/transparent.frag.spv

REM Compositing shaders for OIT
glslc Shaders/Rasterize/composite.vert -o Shaders/Rasterize/composite.vert.spv
glslc Shaders/Rasterize/composite.frag -o Shaders/Rasterize/composite.frag.spv
