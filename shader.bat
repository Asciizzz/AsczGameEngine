@echo off
glslc Shaders/hello.vert -o Shaders/hello.vert.spv

glslc Shaders/hello.frag -o Shaders/hello.frag.spv
glslc Shaders/hello1.frag -o Shaders/hello1.frag.spv
pause