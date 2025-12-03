@echo off

REM Inefficient but really cool sky shaders
glslc Shaders/raw/Sky/sky.vert -o Shaders/bin/Sky/sky.vert.spv
glslc Shaders/raw/Sky/sky.frag -o Shaders/bin/Sky/sky.frag.spv

REM Tests
glslc Shaders/raw/Test/Test.vert -o Shaders/bin/Test/Test.vert.spv
glslc Shaders/raw/Test/Test.frag -o Shaders/bin/Test/Test.frag.spv

echo Compiling completed!