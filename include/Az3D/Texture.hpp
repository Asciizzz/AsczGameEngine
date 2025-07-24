#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>

namespace Az3D {
    
    // Texture resource data - contains Vulkan objects
    struct TextureData {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        uint32_t mipLevels = 1;
        uint32_t width = 0;
        uint32_t height = 0;
        
        bool isValid() const {
            return image != VK_NULL_HANDLE && memory != VK_NULL_HANDLE && 
                   view != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE;
        }
    };
    
    // Texture class - represents a single texture with metadata
    class Texture {
    public:
        Texture(const std::string& id, const std::string& filePath = "");
        ~Texture() = default;
        
        // Delete copy constructor and assignment operator
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;
        
        // Move constructor and assignment
        Texture(Texture&& other) noexcept;
        Texture& operator=(Texture&& other) noexcept;
        
        // Texture identification
        const std::string& getId() const { return id; }
        const std::string& getFilePath() const { return filePath; }
        void setFilePath(const std::string& path) { filePath = path; }
        
        // Texture properties
        uint32_t getWidth() const { return data ? data->width : 0; }
        uint32_t getHeight() const { return data ? data->height : 0; }
        uint32_t getMipLevels() const { return data ? data->mipLevels : 0; }
        
        // Resource access
        const TextureData* getData() const { return data.get(); }
        void setData(std::unique_ptr<TextureData> textureData) { data = std::move(textureData); }
        
        // State checks
        bool isLoaded() const { return data && data->isValid(); }
        bool isEmpty() const { return !data; }
        
    private:
        std::string id;
        std::string filePath;
        std::unique_ptr<TextureData> data;
    };
    
} // namespace Az3D
