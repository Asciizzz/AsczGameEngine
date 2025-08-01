@echo off
setlocal enabledelayedexpansion

:: Configurable distribution name - change this to rename your game!
set "DIST_NAME=AsczGame"

echo ========================================
echo   %DIST_NAME% Portable Distribution Builder
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
cmake -DCMAKE_BUILD_TYPE=Release -DDIST_NAME="%DIST_NAME%" .. || (
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
    copy "DISTRIBUTION_README.md" "%DIST_NAME%\README.txt" >nul
)

:: Manually copy SDL2.dll if CMake didn't find it
if not exist "%DIST_NAME%\SDL2.dll" (
    echo Searching for SDL2.dll...
    for %%p in (
        "%VULKAN_SDK%\Bin\SDL2.dll"
        "%VULKAN_SDK%\Lib\SDL2.dll"
        "%VULKAN_SDK%\Bin32\SDL2.dll"
    ) do (
        if exist "%%p" (
            echo Found SDL2.dll at: %%p
            copy "%%p" "%DIST_NAME%\" >nul 2>&1
            goto :sdl2_found
        )
    )
    echo Warning: SDL2.dll not found! Distribution may not work.
    :sdl2_found
)

:: Try to find and copy common runtime DLLs
echo Searching for Visual C++ runtime DLLs...
for %%d in (
    "vcruntime140.dll"
    "vcruntime140_1.dll" 
    "msvcp140.dll"
) do (
    for %%p in (
        "%SystemRoot%\System32"
        "%SystemRoot%\SysWOW64"
    ) do (
        if exist "%%p\%%d" (
            echo Found: %%d
            copy "%%p\%%d" "%DIST_NAME%\" >nul 2>&1
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
echo @echo off > "%DIST_NAME%\launch.bat"
echo echo Checking system compatibility... >> "%DIST_NAME%\launch.bat"
echo echo. >> "%DIST_NAME%\launch.bat"
echo :: Check for Vulkan >> "%DIST_NAME%\launch.bat"
echo reg query "HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\Vulkan\LoaderICDDrivers" ^>nul 2^>^&1 >> "%DIST_NAME%\launch.bat"
echo if %%errorlevel%% neq 0 ( >> "%DIST_NAME%\launch.bat"
echo     echo Error: Vulkan runtime not found! >> "%DIST_NAME%\launch.bat"
echo     echo Please install Vulkan runtime or update your graphics drivers. >> "%DIST_NAME%\launch.bat"
echo     echo. >> "%DIST_NAME%\launch.bat"
echo     pause >> "%DIST_NAME%\launch.bat"
echo     exit /b 1 >> "%DIST_NAME%\launch.bat"
echo ^) >> "%DIST_NAME%\launch.bat"
echo echo Vulkan runtime detected >> "%DIST_NAME%\launch.bat"
echo echo Starting %DIST_NAME%... >> "%DIST_NAME%\launch.bat"
echo echo. >> "%DIST_NAME%\launch.bat"
echo AsczGame.exe >> "%DIST_NAME%\launch.bat"
echo if %%errorlevel%% neq 0 pause >> "%DIST_NAME%\launch.bat"

echo.
echo ========================================
echo   Build Complete!
echo ========================================
echo.
echo Distribution created in: %DIST_NAME%\
echo.
echo Contents:
if exist "%DIST_NAME%\AsczGame.exe" echo AsczGame.exe
if exist "%DIST_NAME%\Shaders" echo Shaders folder
if exist "%DIST_NAME%\Assets" echo Assets folder  
if exist "%DIST_NAME%\README.txt" echo README.txt
if exist "%DIST_NAME%\launch.bat" echo launch.bat (dependency checker)
if exist "%DIST_NAME%\SDL2.dll" (
    echo SDL2.dll (FOUND - distribution should work!)
) else (
    echo SDL2.dll (MISSING - distribution will NOT work!)
    echo Please copy SDL2.dll manually from %%VULKAN_SDK%%\Bin\
)
echo.
echo Runtime DLLs found and copied:
dir "%DIST_NAME%\*.dll" 2>nul | find ".dll" || echo   (none found - target machine may need Visual C++ Redistributables)
echo.
echo To test on another machine:
echo 1. Copy entire '%DIST_NAME%' folder
echo 2. Run 'launch.bat' for dependency checking
echo 3. Or run 'AsczGame.exe' directly
echo.
echo To change the distribution name, edit DIST_NAME variable at the top of this script.
echo.
pause
