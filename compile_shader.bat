@echo off

glslc Shaders/Rasterize/raster.vert -o Shaders/Rasterize/raster.vert.spv
glslc Shaders/Rasterize/raster.frag -o Shaders/Rasterize/raster.frag.spv
glslc Shaders/Rasterize/raster1.frag -o Shaders/Rasterize/raster1.frag.spv
