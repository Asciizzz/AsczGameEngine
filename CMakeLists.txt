cmake_minimum_required(VERSION 3.15)
project(AsczGame)

set(CMAKE_CXX_STANDARD 17)

# Configurable distribution name - change this to rename your game!
set(DIST_NAME "AsczGame" CACHE STRING "Name for the distribution folder and executable")

find_package(Vulkan REQUIRED)

# Find and enable OpenMP for parallel processing
find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found: ${OpenMP_CXX_VERSION}")
endif()

set(SDL2_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include/SDL2")
set(SDL2_LIBRARY "$ENV{VULKAN_SDK}/Lib/SDL2.lib")

file(GLOB_RECURSE SRC_FILES
    src/Az3D/Camera.cpp
    src/Az3D/GlobalUBO.cpp
    src/Az3D/VertexTypes.cpp
    # src/Az3D/StaticMesh_BVH.cpp
    src/Az3D/TinyModel.cpp
    src/Az3D/TinySkeleton.cpp
    src/Az3D/TinyAnimation.cpp
    src/Az3D/TinyLoader.cpp
    src/Az3D/StaticInstance.cpp
    src/Az3D/RigDemo.cpp

    src/Az3D/ResourceGroup.cpp

    src/AzCore/FpsManager.cpp
    src/AzCore/WindowManager.cpp
    src/AzCore/Application.cpp

    src/AzVulk/Device.cpp
    src/AzVulk/Instance.cpp
    src/AzVulk/Buffer.cpp
    src/AzVulk/Descriptor.cpp
    src/AzVulk/Swapchain.cpp
    src/AzVulk/RenderPass.cpp
    src/AzVulk/MSAAManager.cpp
    src/AzVulk/DepthManager.cpp
    src/AzVulk/Pipeline_base.cpp
    src/AzVulk/Pipeline_graphic.cpp
    src/AzVulk/Pipeline_compute.cpp
    src/AzVulk/Renderer.cpp

    src/AzGame/Grass.cpp

    main.cpp
)

add_executable(AsczGame ${SRC_FILES})

# Set different output names for Debug and Release builds
set_target_properties(AsczGame PROPERTIES
    OUTPUT_NAME_DEBUG "AsczGame_debug"
    OUTPUT_NAME_RELEASE "AsczGame_release"
    OUTPUT_NAME_RELWITHDEBINFO "AsczGame_release"
    OUTPUT_NAME_MINSIZEREL "AsczGame_release"
)

target_include_directories(AsczGame PRIVATE
    ${SDL2_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include/glm
    ${CMAKE_SOURCE_DIR}/include/Helpers

    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/AzBeta
    ${CMAKE_SOURCE_DIR}/include/AzCore
    ${CMAKE_SOURCE_DIR}/include/AzVulk
    ${CMAKE_SOURCE_DIR}/include/Az3D
    ${CMAKE_SOURCE_DIR}/include/AzGame
)


target_link_libraries(AsczGame PRIVATE Vulkan::Vulkan ${SDL2_LIBRARY} OpenMP::OpenMP_CXX)

target_compile_options(AsczGame PRIVATE 
    $<$<CONFIG:Release>:$<IF:$<CXX_COMPILER_ID:MSVC>,/O2,-O3>>
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


# Copy executable to project root after build with proper naming
add_custom_command(TARGET AsczGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:AsczGame> ${CMAKE_SOURCE_DIR}
)

# Copy Shaders folder to output directory for both Debug and Release
add_custom_command(TARGET AsczGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Shaders $<TARGET_FILE_DIR:AsczGame>/Shaders
)

# Copy Assets folder to output directory for both Debug and Release  
add_custom_command(TARGET AsczGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Assets $<TARGET_FILE_DIR:AsczGame>/Assets
)

# Create distribution folders for both Debug and Release builds using generator expressions
add_custom_command(TARGET AsczGame POST_BUILD
    COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> -E make_directory ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Debug
    COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> -E copy $<TARGET_FILE:AsczGame> ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Debug/
    COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Debug/Shaders
    COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> -E copy_directory ${CMAKE_SOURCE_DIR}/Assets ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Debug/Assets
    
    COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> -E make_directory ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Release
    COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> -E copy $<TARGET_FILE:AsczGame> ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Release/
    COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Release/Shaders
    COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> -E copy_directory ${CMAKE_SOURCE_DIR}/Assets ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Release/Assets
)

# Copy DLLs only for Release builds (for portability)
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
            COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> -E copy_if_different
            ${SDL2_DLL} ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Release/
        )
    else()
        message(WARNING "SDL2.dll not found - distribution may not work on target machine")
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
            COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> -E copy_if_different
            ${VCRUNTIME_DLL} ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Release/
        )
    endif()
endif()

# For Debug builds, create a debug distribution folder (optional)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEBUG_DIST_NAME "${DIST_NAME}_Debug")
    
    add_custom_command(TARGET AsczGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/${DEBUG_DIST_NAME}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:AsczGame> ${CMAKE_SOURCE_DIR}/${DEBUG_DIST_NAME}/
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders ${CMAKE_SOURCE_DIR}/${DEBUG_DIST_NAME}/Shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Assets ${CMAKE_SOURCE_DIR}/${DEBUG_DIST_NAME}/Assets
    )
endif()

# Optional: Treat all warnings as errors (recommended for engine dev)
# target_compile_options(AsczGame PRIVATE /W4 /WX)  # For MSVC
