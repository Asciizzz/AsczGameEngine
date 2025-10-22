@echo off

REM Inefficient but really cool sky shaders
glslc Shaders/raw/Sky/sky.vert -o Shaders/bin/Sky/sky.vert.spv
glslc Shaders/raw/Sky/sky.frag -o Shaders/bin/Sky/sky.frag.spv

REM Single instance shader
glslc Shaders/raw/Rasterize/TestStatic.vert -o Shaders/bin/Rasterize/TestStatic.vert.spv
glslc Shaders/raw/Rasterize/TestRigged.vert -o Shaders/bin/Rasterize/TestRigged.vert.spv
glslc Shaders/raw/Rasterize/TestSingle.frag -o Shaders/bin/Rasterize/TestSingle.frag.spv

REM Post-process shaders
glslc Shaders/raw/PostProcess/tonemap.comp -o Shaders/bin/PostProcess/tonemap.comp.spv
glslc Shaders/raw/PostProcess/fxaa.comp -o Shaders/bin/PostProcess/fxaa.comp.spv
glslc Shaders/raw/PostProcess/fun.comp -o Shaders/bin/PostProcess/fun.comp.spv

echo Compiling completed!