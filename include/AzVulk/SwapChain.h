#pragma once

#include <AzVulk/Device.h>

namespace AzVulk {

class SwapChain {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    SwapChain(Device& device);
    void init(); void cleanup();

    // Not copyable or movable
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;
    SwapChain(SwapChain&&) = delete;
    SwapChain& operator=(SwapChain&&) = delete;

    void drawFrame(
        VkPipeline graphicsPipeline,
        VkBuffer vertexBuffer, const std::vector<Vertex>& vertices,
        VkBuffer indexBuffer, const std::vector<uint32_t>& indices
    );

    // Getters
    VkSwapchainKHR getSwapChain() const { return swapChain; }
    const std::vector<VkImage>& getSwapChainImages() const { return swapChainImages; }
    VkFormat getSwapChainImageFormat() const { return swapChainImageFormat; }
    VkExtent2D getSwapChainExtent() const { return swapChainExtent; }
    const std::vector<VkImageView>& getSwapChainImageViews() const { return swapChainImageViews; }
    const std::vector<VkFramebuffer>& getSwapChainFramebuffers() const { return swapChainFramebuffers; }
    VkRenderPass getRenderPass() const { return renderPass; }
    const std::vector<VkCommandBuffer>& getCommandBuffers() const { return commandBuffers; }
    const std::vector<VkSemaphore>& getImageAvailableSemaphores() const { return imageAvailableSemaphores; }
    const std::vector<VkSemaphore>& getRenderFinishedSemaphores() const { return renderFinishedSemaphores; }
    const std::vector<VkFence>& getInFlightFences() const { return inFlightFences; }
    uint32_t getCurrentFrame() const { return currentFrame; }
    bool isFramebufferResized() const { return framebufferResized; }

private:
    Device& device;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass;

    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    bool framebufferResized = false;

    void cleanupSwapChain();
    void recreateSwapChain();

    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();

    void createCommandBuffers();
    void createSyncObjects();

    void recordCommandBuffer(
        VkCommandBuffer commandBuffer, VkPipeline graphicsPipeline,
        VkBuffer vertexBuffer, const std::vector<Vertex>& vertices,
        VkBuffer indexBuffer, const std::vector<uint32_t>& indices,
        uint32_t imageIndex
    );
};

} // namespace AzVulk