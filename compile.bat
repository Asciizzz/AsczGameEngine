@echo off
glslc Shaders/hello.vert -o Shaders/hello.vert.spv
glslc Shaders/hello.frag -o Shaders/hello.frag.spv

glslc Shaders/hi.vert -o Shaders/hi.vert.spv
glslc Shaders/hi.frag -o Shaders/hi.frag.spv
pause