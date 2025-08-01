cmake_minimum_required(VERSION 3.15)
project(AsczGame)

set(CMAKE_CXX_STANDARD 17)

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


# Copy executable to project root after build
add_custom_command(TARGET AsczGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:AsczGame> ${CMAKE_SOURCE_DIR}
)

# Optional: Treat all warnings as errors (recommended for engine dev)
# target_compile_options(AsczGame PRIVATE /W4 /WX)  # For MSVC
