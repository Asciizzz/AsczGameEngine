#define SDL_MAIN_HANDLED

#include <SDL2/SDL_Keyboard.h>

#include <AzVulk/Pipeline.h>
#include <AzVulk/Window.h>
#include <AzVulk/SwapChain.h>

#include <memory>
#include <iostream>
#include <vector>


int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    constexpr int WIDTH = 1600;
    constexpr int HEIGHT = 800;

    AzVulk::Window vk_window(WIDTH, HEIGHT, "AzVulk Window");
    AzVulk::Device vk_device(vk_window);
    AzVulk::SwapChain vk_swapchain(vk_device, vk_window.getExtent());

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0; // No descriptor sets for now
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0; // No push constants for now
    pipelineLayoutInfo.pPushConstantRanges = nullptr;
    if (vkCreatePipelineLayout(vk_device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    // Create graphics pipeline
    AzVulk::PipelineCfgInfo pipelineCfg(
        vk_swapchain.width(), vk_swapchain.height()
    );
    pipelineCfg.renderPass = vk_swapchain.getRenderPass();
    pipelineCfg.pipelineLayout = pipelineLayout;

    std::unique_ptr<AzVulk::Pipeline> vk_pipeline =
    std::make_unique<AzVulk::Pipeline>(
        vk_device,
        "Shaders/hello.vert.spv",
        "Shaders/hello.frag.spv",
        pipelineCfg
    );

    // // Create command buffers
    // commandBuffers.resize(vk_swapchain.imageCount());
    // VkCommandBufferAllocateInfo allocInfo{};
    // allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    // allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    // allocInfo.commandPool = vk_device.getCommandPool();
    // allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    

    int q_count = 0;
    bool q_down = false;

    bool running = true;
    while (running) {
        // Poll for events
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                // Handle quit event
                printf("Quit event received. Exiting...\n");
                return 0;
            }
        }

        // Press Q to print a message
        const Uint8* state = SDL_GetKeyboardState(NULL);
        if (state[SDL_SCANCODE_Q] && !q_down) {
            q_count++;
            printf("Q key pressed %d times.\n", q_count);
            q_down = true;
        }
        if (!state[SDL_SCANCODE_Q]) {
            q_down = false;
        }
    }

    return 0;
}