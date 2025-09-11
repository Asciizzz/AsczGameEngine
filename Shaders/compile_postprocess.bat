@echo off
echo Compiling PostProcess shaders...

:: Check if glslc is available
where glslc >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: glslc not found. Please make sure Vulkan SDK is installed and in PATH.
    pause
    exit /b 1
)

:: Create output directory if it doesn't exist
if not exist "%~dp0PostProcess" mkdir "%~dp0PostProcess"

:: Compile compute shaders
echo Compiling tonemap.comp...
glslc "%~dp0PostProcess\tonemap.comp" -o "%~dp0PostProcess\tonemap.comp.spv"
if %errorlevel% neq 0 (
    echo Error compiling tonemap.comp
    pause
    exit /b 1
)

echo Compiling blur.comp...
glslc "%~dp0PostProcess\blur.comp" -o "%~dp0PostProcess\blur.comp.spv"
if %errorlevel% neq 0 (
    echo Error compiling blur.comp
    pause
    exit /b 1
)

echo Compiling final_composite.comp...
glslc "%~dp0PostProcess\final_composite.comp" -o "%~dp0PostProcess\final_composite.comp.spv"
if %errorlevel% neq 0 (
    echo Error compiling final_composite.comp
    pause
    exit /b 1
)

echo Compiling final_blit.comp...
glslc "%~dp0PostProcess\final_blit.comp" -o "%~dp0PostProcess\final_blit.comp.spv"
if %errorlevel% neq 0 (
    echo Error compiling final_blit.comp
    pause
    exit /b 1
)

echo Compiling final_blit.vert...
glslc "%~dp0PostProcess\final_blit.vert" -o "%~dp0PostProcess\final_blit.vert.spv"
if %errorlevel% neq 0 (
    echo Error compiling final_blit.vert
    pause
    exit /b 1
)

echo Compiling final_blit.frag...
glslc "%~dp0PostProcess\final_blit.frag" -o "%~dp0PostProcess\final_blit.frag.spv"
if %errorlevel% neq 0 (
    echo Error compiling final_blit.frag
    pause
    exit /b 1
)

echo All PostProcess shaders compiled successfully!
pause
