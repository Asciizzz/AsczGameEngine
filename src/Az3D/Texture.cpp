#include "Az3D/Texture.hpp"
#include <utility>

namespace Az3D {
    
    Texture::Texture(const std::string& id, const std::string& filePath) 
        : id(id), filePath(filePath) {
    }
    
    Texture::Texture(Texture&& other) noexcept 
        : id(std::move(other.id)), filePath(std::move(other.filePath)), data(std::move(other.data)) {
    }
    
    Texture& Texture::operator=(Texture&& other) noexcept {
        if (this != &other) {
            id = std::move(other.id);
            filePath = std::move(other.filePath);
            data = std::move(other.data);
        }
        return *this;
    }
    
} // namespace Az3D
