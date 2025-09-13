@echo off

REM Static mesh rasterization shaders
glslc Shaders/raw/Rasterize/StaticMesh.vert -o Shaders/bin/Rasterize/StaticMesh.vert.spv
glslc Shaders/raw/Rasterize/StaticMesh.frag -o Shaders/bin/Rasterize/StaticMesh.frag.spv

REM Skinning shaders
glslc Shaders/raw/Rasterize/RigMesh.vert -o Shaders/bin/Rasterize/RigMesh.vert.spv
glslc Shaders/raw/Rasterize/RigMesh.frag -o Shaders/bin/Rasterize/RigMesh.frag.spv

REM Inefficient but really cool sky shaders
glslc Shaders/raw/Sky/sky.vert -o Shaders/bin/Sky/sky.vert.spv
glslc Shaders/raw/Sky/sky.frag -o Shaders/bin/Sky/sky.frag.spv

REM Single instance shader
glslc Shaders/raw/Rasterize/StaticSingle.vert -o Shaders/bin/Rasterize/StaticSingle.vert.spv

REM Post-process shaders
glslc Shaders/raw/PostProcess/tonemap.comp -o Shaders/bin/PostProcess/tonemap.comp.spv
glslc Shaders/raw/PostProcess/fxaa.comp -o Shaders/bin/PostProcess/fxaa.comp.spv
glslc Shaders/raw/PostProcess/fun.comp -o Shaders/bin/PostProcess/fun.comp.spv

echo Compiling completed!