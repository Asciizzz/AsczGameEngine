@echo off

REM Rasterization shaders
glslc Shaders/Rasterize/raster.vert -o Shaders/Rasterize/raster.vert.spv
glslc Shaders/Rasterize/raster.frag -o Shaders/Rasterize/raster.frag.spv


REM Compute shaders
glslc Shaders/GrassWind/grass_wind.comp -o Shaders/GrassWind/grass_wind.comp.spv