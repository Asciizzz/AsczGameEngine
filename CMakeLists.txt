cmake_minimum_required(VERSION 3.15)
project(AsczGame)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)


set(SDL2_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include/SDL2")
set(SDL2_LIBRARY "$ENV{VULKAN_SDK}/Lib/SDL2.lib")

file(GLOB_RECURSE SRC_FILES
    src/AzGame/WindowManager.cpp
    src/AzGame/VulkanInstance.cpp
    src/AzGame/VulkanDevice.cpp
    src/AzGame/SwapChain.cpp
    src/AzGame/ShaderManager.cpp
    src/AzGame/Buffer.cpp
    src/AzGame/GraphicsPipeline.cpp
    src/AzGame/DescriptorManager.cpp
    src/AzGame/TextureManager.cpp
    src/AzGame/DepthManager.cpp
    src/AzGame/Renderer.cpp
    src/AzGame/Application.cpp

    src/AzModel/Material.cpp
    src/AzModel/Mesh.cpp
    src/AzModel/Model3D.cpp

    main.cpp
)

add_executable(AsczGame ${SRC_FILES})

target_include_directories(AsczGame PRIVATE
    ${SDL2_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include/glm

    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/AzGame
    ${CMAKE_SOURCE_DIR}/include/AzModel
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
