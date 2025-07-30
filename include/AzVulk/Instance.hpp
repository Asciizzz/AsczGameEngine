#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

namespace AzVulk {

    class Instance {
    public:
        Instance(const std::vector<const char*>& requiredExtensions, bool enableValidation = false);
        ~Instance();

        // Delete copy constructor and assignment operator
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;

        
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        bool validationLayersEnabled;

        // Helper methods (now public for direct access)
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
