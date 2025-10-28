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
    src/tinyData/tinyCamera.cpp
    src/tinyData/tinyMaterial.cpp
    src/tinyData/tinyTexture.cpp
    src/tinyData/tinyVertex.cpp
    src/tinyData/tinyMesh.cpp
    src/tinyData/tinySkeleton.cpp

    src/tinyData/tinyRT_MeshRender3D.cpp
    src/tinyData/tinyRT_Skeleton3D.cpp
    src/tinyData/tinyRT_Anime3D.cpp
    src/tinyData/tinyRT_Scene.cpp
    src/tinyData/tinyRT_Node.cpp

    src/tinyEngine/tinyRData.cpp
    src/tinyEngine/tinyGlobal.cpp
    src/tinyEngine/tinyProject.cpp
    src/tinyEngine/tinyLoader.cpp
    # src/tinyEngine/tinyPlayback.cpp deprecated

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

    src/tinySystem/tinyChrono.cpp
    src/tinySystem/tinyWindow.cpp
    src/tinySystem/tinyImGui.cpp

    src/tinyApp/tinyApp_basic.cpp
    src/tinyApp/tinyApp_imgui.cpp

    src/tinyVK/System/Device.cpp
    src/tinyVK/System/Instance.cpp
    src/tinyVK/System/CmdBuffer.cpp

    src/tinyVK/Resource/DataBuffer.cpp
    src/tinyVK/Resource/Descriptor.cpp
    src/tinyVK/Resource/TextureVK.cpp

    src/tinyVK/Pipeline/Pipeline_core.cpp
    src/tinyVK/Pipeline/Pipeline_raster.cpp
    src/tinyVK/Pipeline/Pipeline_compute.cpp
    src/tinyVK/Pipeline/Pipeline_manager.cpp

    src/tinyVK/Render/FrameBuffer.cpp
    src/tinyVK/Render/DepthImage.cpp
    src/tinyVK/Render/Swapchain.cpp
    src/tinyVK/Render/RenderPass.cpp
    src/tinyVK/Render/RenderTarget.cpp
    src/tinyVK/Render/PostProcess.cpp
    src/tinyVK/Render/Renderer.cpp

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

    ${CMAKE_SOURCE_DIR}/include/tinyVK
    ${CMAKE_SOURCE_DIR}/include/tinyExt
    ${CMAKE_SOURCE_DIR}/include/tinyData
    ${CMAKE_SOURCE_DIR}/include/tinyEngine
    ${CMAKE_SOURCE_DIR}/include/tinySystem
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
