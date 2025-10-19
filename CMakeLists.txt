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

# Set SDL2 library path based on target architecture
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # 64-bit
    set(SDL2_LIBRARY "$ENV{VULKAN_SDK}/Lib/SDL2.lib")
else()
    # 32-bit
    set(SDL2_LIBRARY "$ENV{VULKAN_SDK}/Lib32/SDL2.lib")
endif()

file(GLOB_RECURSE SRC_FILES
    src/TinyData/TinyVertex.cpp
    src/TinyData/TinyMesh.cpp
    src/TinyData/TinyMaterial.cpp
    src/TinyData/TinyTexture.cpp
    src/TinyData/TinySkeleton.cpp
    src/TinyData/TinyAnimation.cpp
    src/TinyData/TinyNode.cpp
    src/TinyData/TinyCamera.cpp
    
    src/TinyEngine/TinyRData.cpp
    src/TinyEngine/TinyGlobal.cpp
    src/TinyEngine/TinyProject.cpp
    src/TinyEngine/TinyLoader.cpp
    # src/TinyEngine/TinyPlayback.cpp deprecated

    # ImGui source files
    include/.ext/imgui/imgui.cpp
    include/.ext/imgui/imgui_demo.cpp
    include/.ext/imgui/imgui_draw.cpp
    include/.ext/imgui/imgui_tables.cpp
    include/.ext/imgui/imgui_widgets.cpp
    include/.ext/imgui/backends/imgui_impl_sdl2.cpp
    include/.ext/imgui/backends/imgui_impl_vulkan.cpp

    # Lua source files (excluding lua.c and luac.c which contain main functions)
    include/.ext/luacpp/lapi.c
    include/.ext/luacpp/lauxlib.c
    include/.ext/luacpp/lbaselib.c
    include/.ext/luacpp/lcode.c
    include/.ext/luacpp/lcorolib.c
    include/.ext/luacpp/lctype.c
    include/.ext/luacpp/ldblib.c
    include/.ext/luacpp/ldebug.c
    include/.ext/luacpp/ldo.c
    include/.ext/luacpp/ldump.c
    include/.ext/luacpp/lfunc.c
    include/.ext/luacpp/lgc.c
    include/.ext/luacpp/linit.c
    include/.ext/luacpp/liolib.c
    include/.ext/luacpp/llex.c
    include/.ext/luacpp/lmathlib.c
    include/.ext/luacpp/lmem.c
    include/.ext/luacpp/loadlib.c
    include/.ext/luacpp/lobject.c
    include/.ext/luacpp/lopcodes.c
    include/.ext/luacpp/loslib.c
    include/.ext/luacpp/lparser.c
    include/.ext/luacpp/lstate.c
    include/.ext/luacpp/lstring.c
    include/.ext/luacpp/lstrlib.c
    include/.ext/luacpp/ltable.c
    include/.ext/luacpp/ltablib.c
    include/.ext/luacpp/ltm.c
    include/.ext/luacpp/lundump.c
    include/.ext/luacpp/lutf8lib.c
    include/.ext/luacpp/lvm.c
    include/.ext/luacpp/lzio.c

    src/TinySystem/TinyChrono.cpp
    src/TinySystem/TinyWindow.cpp
    src/TinySystem/Application.cpp
    src/TinySystem/TinyImGui.cpp

    src/TinyVK/System/Device.cpp
    src/TinyVK/System/Instance.cpp
    src/TinyVK/System/CmdBuffer.cpp

    src/TinyVK/Resource/DataBuffer.cpp
    src/TinyVK/Resource/Descriptor.cpp
    src/TinyVK/Resource/TextureVK.cpp

    src/TinyVK/Pipeline/Pipeline_core.cpp
    src/TinyVK/Pipeline/Pipeline_raster.cpp
    src/TinyVK/Pipeline/Pipeline_compute.cpp
    src/TinyVK/Pipeline/Pipeline_manager.cpp

    src/TinyVK/Render/FrameBuffer.cpp
    src/TinyVK/Render/DepthImage.cpp
    src/TinyVK/Render/Swapchain.cpp
    src/TinyVK/Render/RenderPass.cpp
    src/TinyVK/Render/RenderTarget.cpp
    src/TinyVK/Render/PostProcess.cpp
    src/TinyVK/Render/Renderer.cpp

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
    ${CMAKE_SOURCE_DIR}/include

    ${CMAKE_SOURCE_DIR}/include/.ext/glm
    ${CMAKE_SOURCE_DIR}/include/.ext/json
    ${CMAKE_SOURCE_DIR}/include/.ext/tiny3d
    ${CMAKE_SOURCE_DIR}/include/.ext/Helpers
    ${CMAKE_SOURCE_DIR}/include/.ext/imgui
    ${CMAKE_SOURCE_DIR}/include/.ext/luacpp

    ${CMAKE_SOURCE_DIR}/include/TinySystem
    ${CMAKE_SOURCE_DIR}/include/TinyVK
    ${CMAKE_SOURCE_DIR}/include/TinyExt
    ${CMAKE_SOURCE_DIR}/include/TinyData
    ${CMAKE_SOURCE_DIR}/include/TinyEngine
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
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit
        find_file(SDL2_DLL 
            NAMES "SDL2.dll"
            PATHS 
                "$ENV{VULKAN_SDK}/Bin"
                "$ENV{VULKAN_SDK}/Lib"
                "${SDL2_INCLUDE_DIR}/../Bin"
                "${SDL2_INCLUDE_DIR}/../Lib"
            NO_DEFAULT_PATH
        )
    else()
        # 32-bit
        find_file(SDL2_DLL 
            NAMES "SDL2.dll"
            PATHS 
                "$ENV{VULKAN_SDK}/Bin32"
                "$ENV{VULKAN_SDK}/Lib32"
                "${SDL2_INCLUDE_DIR}/../Bin32"
                "${SDL2_INCLUDE_DIR}/../Lib32"
            NO_DEFAULT_PATH
        )
    endif()
    
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
