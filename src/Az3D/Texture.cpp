#include "Az3D/Texture.hpp"
#include "Helpers/stb_image.h"

namespace Az3D {

void Texture::free() {
    if (data) {
        if (isStbiAllocated) {
            stbi_image_free(data);
        } else {
            delete[] data;
        }
        data = nullptr;
    }
}

} // namespace Az3D
