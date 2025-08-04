@echo off

glslc Shaders/Rasterize/raster.vert -o Shaders/Rasterize/raster.vert.spv
glslc Shaders/Rasterize/raster.frag -o Shaders/Rasterize/raster.frag.spv
glslc Shaders/Rasterize/rasterDepth.frag -o Shaders/Rasterize/rasterDepth.frag.spv
glslc Shaders/Rasterize/rasterNormal.frag -o Shaders/Rasterize/rasterNormal.frag.spv