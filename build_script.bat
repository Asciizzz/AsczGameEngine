@echo off
setlocal enabledelayedexpansion

:: Configurable distribution name - change this to rename your distribution
set "DIST_NAME=AsczGame"

echo ========================================
echo   %DIST_NAME% Portable Distribution Builder
echo ========================================
echo.

:: Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo Error: CMakeLists.txt not found. Please run this from the project root.
    exit /b 1
)

:: Create build directory
if not exist "build" mkdir build
cd build

echo Configuring CMake for Release build...
cmake -DCMAKE_BUILD_TYPE=Release -DDIST_NAME="%DIST_NAME%" .. 
if errorlevel 1 (
    echo Error: CMake configuration failed!
    cd ..
    exit /b 1
)

echo.
echo Building project...
cmake --build . --config Release
if errorlevel 1 (
    echo Error: Build failed!
    cd ..
    exit /b 1
)

cd ..

:: Verify distribution was created by CMake (updated for new naming)
set "RELEASE_DIST_NAME=%DIST_NAME%_Release"
if not exist "%RELEASE_DIST_NAME%" (
    echo Error: Distribution folder '%RELEASE_DIST_NAME%' not created by CMake!
    exit /b 1
)

if not exist "%RELEASE_DIST_NAME%\%DIST_NAME%_release.exe" (
    echo Error: %DIST_NAME%_release.exe not found in distribution!
    exit /b 1
)

:: Additional manual DLL copying for better compatibility
echo.
echo Searching for additional dependencies...

:: Search for SDL2.dll if not already copied
if not exist "%RELEASE_DIST_NAME%\SDL2.dll" (
    echo Searching for SDL2.dll...
    if defined VULKAN_SDK (
        if exist "%VULKAN_SDK%\Bin\SDL2.dll" (
            echo Found SDL2.dll in Vulkan SDK
            copy "%VULKAN_SDK%\Bin\SDL2.dll" "%RELEASE_DIST_NAME%\" >nul 2>&1
        )
    )
)

:: Search for additional Visual C++ runtime DLLs
set "RUNTIME_FOUND=0"
for %%d in (vcruntime140.dll vcruntime140_1.dll msvcp140.dll) do (
    if not exist "%RELEASE_DIST_NAME%\%%d" (
        if exist "%SystemRoot%\System32\%%d" (
            echo Found %%d in System32
            copy "%SystemRoot%\System32\%%d" "%RELEASE_DIST_NAME%\" >nul 2>&1
            set "RUNTIME_FOUND=1"
        )
    ) else (
        set "RUNTIME_FOUND=1"
    )
)

:: Check for OpenMP DLL (common dependency)
if not exist "%RELEASE_DIST_NAME%\vcomp140.dll" (
    if exist "%SystemRoot%\System32\vcomp140.dll" (
        echo Found OpenMP runtime
        copy "%SystemRoot%\System32\vcomp140.dll" "%RELEASE_DIST_NAME%\" >nul 2>&1
    )
)

:: Create launch script for dependency checking (updated for new executable name)
echo Creating launch script...
echo @echo off > "%RELEASE_DIST_NAME%\launch.bat"
echo echo Checking system compatibility... >> "%RELEASE_DIST_NAME%\launch.bat"
echo echo. >> "%RELEASE_DIST_NAME%\launch.bat"
echo. >> "%RELEASE_DIST_NAME%\launch.bat"
echo :: Check for Vulkan runtime using multiple methods >> "%RELEASE_DIST_NAME%\launch.bat"
echo set "VULKAN_OK=0" >> "%RELEASE_DIST_NAME%\launch.bat"
echo. >> "%RELEASE_DIST_NAME%\launch.bat"
echo :: Method 1: Check for vulkan-1.dll in System32 >> "%RELEASE_DIST_NAME%\launch.bat"
echo if exist "%%SystemRoot%%\System32\vulkan-1.dll" ^( >> "%RELEASE_DIST_NAME%\launch.bat"
echo     echo Vulkan runtime DLL found >> "%RELEASE_DIST_NAME%\launch.bat"
echo     set "VULKAN_OK=1" >> "%RELEASE_DIST_NAME%\launch.bat"
echo ^) >> "%RELEASE_DIST_NAME%\launch.bat"
echo. >> "%RELEASE_DIST_NAME%\launch.bat"
echo :: Method 2: Check for any Vulkan registry entries >> "%RELEASE_DIST_NAME%\launch.bat"
echo reg query "HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\Vulkan" ^>nul 2^>^&1 >> "%RELEASE_DIST_NAME%\launch.bat"
echo if not errorlevel 1 ^( >> "%RELEASE_DIST_NAME%\launch.bat"
echo     echo Vulkan registry entries found >> "%RELEASE_DIST_NAME%\launch.bat"
echo     set "VULKAN_OK=1" >> "%RELEASE_DIST_NAME%\launch.bat"
echo ^) >> "%RELEASE_DIST_NAME%\launch.bat"
echo. >> "%RELEASE_DIST_NAME%\launch.bat"
echo :: Method 3: Check for LoaderICDDrivers ^(strict check^) >> "%RELEASE_DIST_NAME%\launch.bat"
echo reg query "HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\Vulkan\LoaderICDDrivers" ^>nul 2^>^&1 >> "%RELEASE_DIST_NAME%\launch.bat"
echo if not errorlevel 1 ^( >> "%RELEASE_DIST_NAME%\launch.bat"
echo     echo Vulkan ICD drivers registered >> "%RELEASE_DIST_NAME%\launch.bat"
echo     set "VULKAN_OK=1" >> "%RELEASE_DIST_NAME%\launch.bat"
echo ^) >> "%RELEASE_DIST_NAME%\launch.bat"
echo. >> "%RELEASE_DIST_NAME%\launch.bat"
echo if "%%VULKAN_OK%%"=="0" ^( >> "%RELEASE_DIST_NAME%\launch.bat"
echo     echo Warning: Vulkan runtime detection uncertain >> "%RELEASE_DIST_NAME%\launch.bat"
echo     echo If the game fails to start, please update your graphics drivers >> "%RELEASE_DIST_NAME%\launch.bat"
echo     echo. >> "%RELEASE_DIST_NAME%\launch.bat"
echo     echo Attempting to start anyway... >> "%RELEASE_DIST_NAME%\launch.bat"
echo     timeout /t 3 ^>nul >> "%RELEASE_DIST_NAME%\launch.bat"
echo ^) else ^( >> "%RELEASE_DIST_NAME%\launch.bat"
echo     echo Vulkan runtime detected >> "%RELEASE_DIST_NAME%\launch.bat"
echo ^) >> "%RELEASE_DIST_NAME%\launch.bat"
echo. >> "%RELEASE_DIST_NAME%\launch.bat"
echo echo Starting %DIST_NAME%... >> "%RELEASE_DIST_NAME%\launch.bat"
echo echo. >> "%RELEASE_DIST_NAME%\launch.bat"
echo %DIST_NAME%_release.exe >> "%RELEASE_DIST_NAME%\launch.bat"
echo if errorlevel 1 pause >> "%RELEASE_DIST_NAME%\launch.bat"

:: Create distribution README
echo Creating distribution documentation...
echo %DIST_NAME% - Portable Release Distribution > "%RELEASE_DIST_NAME%\README.txt"
echo ================================= >> "%RELEASE_DIST_NAME%\README.txt"
echo. >> "%RELEASE_DIST_NAME%\README.txt"
echo This is a portable release distribution of %DIST_NAME%. >> "%RELEASE_DIST_NAME%\README.txt"
echo. >> "%RELEASE_DIST_NAME%\README.txt"
echo To run: >> "%RELEASE_DIST_NAME%\README.txt"
echo 1. Run launch.bat for automatic compatibility checking >> "%RELEASE_DIST_NAME%\README.txt"
echo 2. Or run %DIST_NAME%_release.exe directly (no virus legit pubg mobile) >> "%RELEASE_DIST_NAME%\README.txt"
echo. >> "%RELEASE_DIST_NAME%\README.txt"
echo Requirements: >> "%RELEASE_DIST_NAME%\README.txt"
echo - Windows that can open and close >> "%RELEASE_DIST_NAME%\README.txt"
echo - Vulkan-compatible graphics card and drivers >> "%RELEASE_DIST_NAME%\README.txt"
echo - Visual C++ runtime ^(included^) >> "%RELEASE_DIST_NAME%\README.txt"
echo. >> "%RELEASE_DIST_NAME%\README.txt"
echo Performance Features: >> "%RELEASE_DIST_NAME%\README.txt"
echo - Not to brag but it can handle 4000 Kasane Pearto at once >> "%RELEASE_DIST_NAME%\README.txt"
echo - Oh and de_dust2 as well >> "%RELEASE_DIST_NAME%\README.txt"

:: Build summary
echo.
echo ========================================
echo   Build Complete!
echo ========================================
echo.
echo Distribution created in: %RELEASE_DIST_NAME%\
echo.
echo Contents:
if exist "%RELEASE_DIST_NAME%\%DIST_NAME%_release.exe" echo   [OK] %DIST_NAME%_release.exe
if exist "%RELEASE_DIST_NAME%\Shaders" echo   [OK] Shaders folder
if exist "%RELEASE_DIST_NAME%\Assets" echo   [OK] Assets folder
if exist "%RELEASE_DIST_NAME%\imgui.ini" echo   [OK] imgui.ini
if exist "%RELEASE_DIST_NAME%\Ascz.ico" echo   [OK] Ascz.ico
if exist "%RELEASE_DIST_NAME%\README.txt" echo   [OK] README.txt
if exist "%RELEASE_DIST_NAME%\launch.bat" echo   [OK] launch.bat

echo.
echo DLL Dependencies:
if exist "%RELEASE_DIST_NAME%\SDL2.dll" (
    echo   [OK] SDL2.dll
) else (
    echo   [MISSING] SDL2.dll - distribution may not work!
)

if "%RUNTIME_FOUND%"=="1" (
    echo   [OK] Visual C++ runtime DLLs
) else (
    echo   [WARNING] Visual C++ runtime DLLs not found
    echo            Target machine may need redistributables
)

if exist "%RELEASE_DIST_NAME%\vcomp140.dll" echo   [OK] OpenMP runtime

echo.
echo Distribution is ready for deployment!
echo To test on another machine:
echo   1. Copy entire '%RELEASE_DIST_NAME%' folder
echo   2. Run 'launch.bat' for automatic checking
echo.
