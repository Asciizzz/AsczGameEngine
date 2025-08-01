@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   AsczGame Portable Distribution Builder
echo ========================================
echo.

:: Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo Error: CMakeLists.txt not found. Please run this from the project root.
    pause
    exit /b 1
)

:: Create build directory
if not exist "build" mkdir build
cd build

echo Configuring CMake for Release build...
cmake -DCMAKE_BUILD_TYPE=Release .. || (
    echo Error: CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo Building project...
cmake --build . --config Release || (
    echo Error: Build failed!
    cd ..
    pause
    exit /b 1
)

cd ..

:: Additional dependency copying for maximum portability
echo.
echo Copying additional dependencies for portability...

:: Copy the distribution README
if exist "DISTRIBUTION_README.md" (
    copy "DISTRIBUTION_README.md" "AsczGame\README.txt" >nul
)

:: Try to find and copy common runtime DLLs
set "SYSTEM_DLLS="
for %%d in (
    "vcruntime140.dll"
    "vcruntime140_1.dll" 
    "msvcp140.dll"
    "api-ms-win-crt-*.dll"
) do (
    for %%p in (
        "%SystemRoot%\System32"
        "%SystemRoot%\SysWOW64"
    ) do (
        if exist "%%p\%%d" (
            echo Found: %%d
            copy "%%p\%%d" "AsczGame\" >nul 2>&1
        )
    )
)

:: Check Vulkan installation
echo.
echo Checking Vulkan installation...
if defined VULKAN_SDK (
    echo Vulkan SDK found: %VULKAN_SDK%
) else (
    echo Warning: VULKAN_SDK not set. Target machine will need Vulkan runtime.
)

:: Create a simple launcher that checks dependencies
echo @echo off > AsczGame\launch.bat
echo echo Checking system compatibility... >> AsczGame\launch.bat
echo echo. >> AsczGame\launch.bat
echo :: Check for Vulkan >> AsczGame\launch.bat
echo reg query "HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\Vulkan\LoaderICDDrivers" ^>nul 2^>^&1 >> AsczGame\launch.bat
echo if %%errorlevel%% neq 0 ( >> AsczGame\launch.bat
echo     echo Error: Vulkan runtime not found! >> AsczGame\launch.bat
echo     echo Please install Vulkan runtime or update your graphics drivers. >> AsczGame\launch.bat
echo     echo. >> AsczGame\launch.bat
echo     pause >> AsczGame\launch.bat
echo     exit /b 1 >> AsczGame\launch.bat
echo ^) >> AsczGame\launch.bat
echo echo Vulkan runtime detected >> AsczGame\launch.bat
echo echo Starting AsczGame... >> AsczGame\launch.bat
echo echo. >> AsczGame\launch.bat
echo AsczGame.exe >> AsczGame\launch.bat
echo if %%errorlevel%% neq 0 pause >> AsczGame\launch.bat

echo.
echo ========================================
echo   Build Complete!
echo ========================================
echo.
echo Distribution created in: AsczGame\
echo.
echo Contents:
if exist "AsczGame\AsczGame.exe" echo AsczGame.exe
if exist "AsczGame\Shaders" echo Shaders folder
if exist "AsczGame\Assets" echo Assets folder  
if exist "AsczGame\README.txt" echo README.txt
if exist "AsczGame\launch.bat" echo launch.bat (dependency checker)
echo.
echo Runtime DLLs found and copied:
dir "AsczGame\*.dll" 2>nul | find ".dll" || echo   (none found - target machine may need Visual C++ Redistributables)
echo.
echo To test on another machine:
echo 1. Copy entire 'dist' folder
echo 2. Run 'launch.bat' for dependency checking
echo 3. Or run 'AsczGame.exe' directly
echo.
pause
