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
    # ImGui source files
    ext/imgui/imgui.cpp
    ext/imgui/imgui_demo.cpp
    ext/imgui/imgui_draw.cpp
    ext/imgui/imgui_tables.cpp
    ext/imgui/imgui_widgets.cpp
    ext/imgui/backends/imgui_impl_sdl2.cpp
    ext/imgui/backends/imgui_impl_vulkan.cpp
    ext/imgui/TextEditor.cpp

    # Lua source files (excluding lua.c and luac.c which contain main functions)
    ext/luacpp/lapi.c
    ext/luacpp/lauxlib.c
    ext/luacpp/lbaselib.c
    ext/luacpp/lcode.c
    ext/luacpp/lcorolib.c
    ext/luacpp/lctype.c
    ext/luacpp/ldblib.c
    ext/luacpp/ldebug.c
    ext/luacpp/ldo.c
    ext/luacpp/ldump.c
    ext/luacpp/lfunc.c
    ext/luacpp/lgc.c
    ext/luacpp/linit.c
    ext/luacpp/liolib.c
    ext/luacpp/llex.c
    ext/luacpp/lmathlib.c
    ext/luacpp/lmem.c
    ext/luacpp/loadlib.c
    ext/luacpp/lobject.c
    ext/luacpp/lopcodes.c
    ext/luacpp/loslib.c
    ext/luacpp/lparser.c
    ext/luacpp/lstate.c
    ext/luacpp/lstring.c
    ext/luacpp/lstrlib.c
    ext/luacpp/ltable.c
    ext/luacpp/ltablib.c
    ext/luacpp/ltm.c
    ext/luacpp/lundump.c
    ext/luacpp/lutf8lib.c
    ext/luacpp/lvm.c
    ext/luacpp/lzio.c

    # Engine specific files

    src/tinyData/tinyCamera.cpp

    src/tinyScript/tinyScript.cpp
    src/tinyData/tinySharedRes.cpp
    src/tinyData/tinyRT_MeshRender3D.cpp
    src/tinyData/tinyRT_Skeleton3D.cpp
    src/tinyData/tinyRT_Anime3D.cpp
    src/tinyData/tinyRT_Script.cpp
    src/tinyData/tinyRT_Scene.cpp
    src/tinyData/tinyRT_Node.cpp

    src/tinyEngine/tinyGlobal.cpp
    src/tinyEngine/tinyProject.cpp
    src/tinyEngine/tinyLoader.cpp

    src/tinySystem/tinyChrono.cpp
    src/tinySystem/tinyWindow.cpp
    src/tinyUI/tinyUI.cpp

    src/tinyApp/tinyApp_basic.cpp
    src/tinyApp/tinyApp_imgui.cpp

    src/tinyVk/System/Device.cpp
    src/tinyVk/System/Instance.cpp
    src/tinyVk/System/CmdBuffer.cpp

    src/tinyVk/Resource/DataBuffer.cpp
    src/tinyVk/Resource/Descriptor.cpp
    src/tinyVk/Resource/TextureVk.cpp

    src/tinyVk/Pipeline/Pipeline_core.cpp
    src/tinyVk/Pipeline/Pipeline_raster.cpp
    src/tinyVk/Pipeline/Pipeline_compute.cpp

    src/tinyVk/Render/FrameBuffer.cpp
    src/tinyVk/Render/DepthImage.cpp
    src/tinyVk/Render/Swapchain.cpp
    src/tinyVk/Render/RenderPass.cpp
    src/tinyVk/Render/RenderTarget.cpp
    src/tinyVk/Render/Renderer.cpp

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

    ${CMAKE_SOURCE_DIR}/ext
    ${CMAKE_SOURCE_DIR}/ext/glm
    ${CMAKE_SOURCE_DIR}/ext/json
    ${CMAKE_SOURCE_DIR}/ext/tiny3d
    ${CMAKE_SOURCE_DIR}/ext/Helpers
    ${CMAKE_SOURCE_DIR}/ext/imgui
    ${CMAKE_SOURCE_DIR}/ext/luacpp
    ${CMAKE_SOURCE_DIR}/ext/tinyExt

    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/tinyVk
    ${CMAKE_SOURCE_DIR}/include/tinyData
    ${CMAKE_SOURCE_DIR}/include/tinyScript
    ${CMAKE_SOURCE_DIR}/include/tinySystem
    ${CMAKE_SOURCE_DIR}/include/tinyEngine
    ${CMAKE_SOURCE_DIR}/include/tinyApp 
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


# Create distribution folders for both Debug and Release builds using generator expressions
add_custom_command(TARGET AsczGame POST_BUILD
    COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> -E make_directory ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Debug
    COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> -E copy $<TARGET_FILE:AsczGame> ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Debug/
    COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Debug/Shaders
    
    COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> -E make_directory ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Release
    COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> -E copy $<TARGET_FILE:AsczGame> ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Release/
    COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders ${CMAKE_SOURCE_DIR}/${DIST_NAME}_Release/Shaders
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
    )
endif()

# Optional: Treat all warnings as errors (recommended for engine dev)
# target_compile_options(AsczGame PRIVATE /W4 /WX)  # For MSVC
