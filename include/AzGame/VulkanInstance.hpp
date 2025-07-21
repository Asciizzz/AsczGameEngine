#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

namespace AzGame {
    class VulkanInstance {
    public:
        VulkanInstance(const std::vector<const char*>& requiredExtensions, bool enableValidation = false);
        ~VulkanInstance();

        // Delete copy constructor and assignment operator
        VulkanInstance(const VulkanInstance&) = delete;
        VulkanInstance& operator=(const VulkanInstance&) = delete;

        VkInstance getInstance() const { return instance; }
        VkDebugUtilsMessengerEXT getDebugMessenger() const { return debugMessenger; }

    private:
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        bool validationLayersEnabled;

        void createInstance(const std::vector<const char*>& requiredExtensions);
        void setupDebugMessenger();
        bool checkValidationLayerSupport();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData);

        static const std::vector<const char*> validationLayers;
    };
}
