cmake_minimum_required(VERSION 3.15)
project(AsczGame)

set(CMAKE_CXX_STANDARD 17)

# Configurable distribution name - change this to rename your game!
set(DIST_NAME "AsczGame" CACHE STRING "Name for the distribution folder and executable")

find_package(Vulkan REQUIRED)

set(SDL2_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include/SDL2")
set(SDL2_LIBRARY "$ENV{VULKAN_SDK}/Lib/SDL2.lib")

file(GLOB_RECURSE SRC_FILES
    src/Az3D/Camera.cpp
    src/Az3D/Texture.cpp
    src/Az3D/Mesh.cpp
    src/Az3D/Mesh_BVH.cpp
    src/Az3D/Material.cpp
    src/Az3D/Light.cpp
    src/Az3D/Billboard.cpp
    src/Az3D/ResourceManager.cpp
    src/Az3D/RenderSystem.cpp

    src/AzVulk/Instance.cpp
    src/AzVulk/Device.cpp
    src/AzVulk/SwapChain.cpp
    src/AzVulk/ShaderManager.cpp
    src/AzVulk/Buffer.cpp
    src/AzVulk/RasterPipeline.cpp
    src/AzVulk/DescriptorManager.cpp
    src/AzVulk/DepthManager.cpp
    src/AzVulk/MSAAManager.cpp
    src/AzVulk/Renderer.cpp

    src/AzCore/WindowManager.cpp
    src/AzCore/FpsManager.cpp
    src/AzCore/Application.cpp

    main.cpp
)

add_executable(AsczGame ${SRC_FILES})

target_include_directories(AsczGame PRIVATE
    ${SDL2_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include/glm

    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/AzBeta
    ${CMAKE_SOURCE_DIR}/include/AzCore
    ${CMAKE_SOURCE_DIR}/include/AzVulk
    ${CMAKE_SOURCE_DIR}/include/Az3D
)


target_link_libraries(AsczGame PRIVATE Vulkan::Vulkan ${SDL2_LIBRARY})

target_compile_options(AsczGame PRIVATE 
    $<$<CONFIG:Release>:-O3>
)

# Static linking for better portability (reduces dependency on system libraries)
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(AsczGame PRIVATE
        /INCREMENTAL:NO
        /SUBSYSTEM:CONSOLE
    )
    # Link runtime statically to reduce dependencies
    set_property(TARGET AsczGame PROPERTY 
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()


# Copy executable to project root after build
add_custom_command(TARGET AsczGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:AsczGame> ${CMAKE_SOURCE_DIR}
)

# Copy Shaders folder to output directory for both Debug and Release
add_custom_command(TARGET AsczGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Shaders $<TARGET_FILE_DIR:AsczGame>/Shaders
    COMMENT "Copying Shaders folder to output directory\n"
)

# Copy Assets folder to output directory for both Debug and Release  
add_custom_command(TARGET AsczGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Assets $<TARGET_FILE_DIR:AsczGame>/Assets
    COMMENT "Copying Assets folder to output directory\n"
)

# For Release builds, create a standalone distribution folder
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET AsczGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/${DIST_NAME}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:AsczGame> ${CMAKE_SOURCE_DIR}/${DIST_NAME}/
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders ${CMAKE_SOURCE_DIR}/${DIST_NAME}/Shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Assets ${CMAKE_SOURCE_DIR}/${DIST_NAME}/Assets
        COMMENT "Creating standalone distribution in ${DIST_NAME} folder\n"
    )
    
    # Copy Visual C++ Redistributables and SDL2.dll if available (for portability)
    if(WIN32)
        # Copy SDL2.dll from Vulkan SDK
        find_file(SDL2_DLL 
            NAMES "SDL2.dll"
            PATHS 
                "$ENV{VULKAN_SDK}/Bin"
                "$ENV{VULKAN_SDK}/Lib"
                "$ENV{VULKAN_SDK}/Bin32"
                "${SDL2_INCLUDE_DIR}/../Lib"
                "${SDL2_INCLUDE_DIR}/../Bin"
            NO_DEFAULT_PATH
        )
        
        if(SDL2_DLL)
            add_custom_command(TARGET AsczGame POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SDL2_DLL} ${CMAKE_SOURCE_DIR}/${DIST_NAME}/
                COMMENT "Copying SDL2.dll to distribution"
            )
        else()
            message(WARNING "SDL2.dll not found - distribution may not work on target machine\n")
        endif()
        
        # Try to find and copy common redistributables
        find_file(VCRUNTIME_DLL 
            NAMES "vcruntime140.dll" "vcruntime140_1.dll" "msvcp140.dll"
            PATHS 
                "$ENV{SystemRoot}/System32"
                "$ENV{SystemRoot}/SysWOW64"
                "$ENV{VCToolsRedistDir}/x64/Microsoft.VC143.CRT"
                "$ENV{VCToolsRedistDir}/x86/Microsoft.VC143.CRT"
            NO_DEFAULT_PATH
        )
        
        if(VCRUNTIME_DLL)
            add_custom_command(TARGET AsczGame POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${VCRUNTIME_DLL} ${CMAKE_SOURCE_DIR}/${DIST_NAME}/
                COMMENT "Copying Visual C++ runtime to distribution\n"
            )
        endif()
    endif()
endif()

# Optional: Treat all warnings as errors (recommended for engine dev)
# target_compile_options(AsczGame PRIVATE /W4 /WX)  # For MSVC
