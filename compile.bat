@echo off
glslc Shaders/hello.vert -o Shaders/hello.vert.spv
glslc Shaders/hello.frag -o Shaders/hello.frag.spv
pause