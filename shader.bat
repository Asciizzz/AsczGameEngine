@echo off

REM Create directories if they don't exist
if not exist "build\Shaders\Rasterize" mkdir "build\Shaders\Rasterize"
if not exist "build\Shaders\Billboard" mkdir "build\Shaders\Billboard"

REM Compile and copy rasterize shaders
glslc Shaders/Rasterize/raster.vert -o build/Shaders/Rasterize/raster.vert.spv
glslc Shaders/Rasterize/raster.frag -o build/Shaders/Rasterize/raster.frag.spv
glslc Shaders/Rasterize/raster1.frag -o build/Shaders/Rasterize/raster1.frag.spv

REM Compile and copy billboard shaders
glslc Shaders/Billboard/billboard.vert -o build/Shaders/Billboard/billboard.vert.spv
glslc Shaders/Billboard/billboard.frag -o build/Shaders/Billboard/billboard.frag.spv

pause