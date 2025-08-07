@echo off
echo Compiling grass wind compute shader...

cd /d "%~dp0"
glslc grass_wind.comp -o grass_wind.comp.spv

if %errorlevel% equ 0 (
    echo Compute shader compiled successfully!
) else (
    echo Failed to compile compute shader!
    pause
)
