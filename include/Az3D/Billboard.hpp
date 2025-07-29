#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>

namespace Az3D {

    struct Billboard {
        glm::vec3 position{0.0f};
        float width = 1.0f;
        float height = 1.0f;
        size_t textureIndex = 0;
        
        // UV coordinates for sprite sheets (AB1 to AB2)
        glm::vec2 uvMin{0.0f, 0.0f};  // AB1 - top-left UV
        glm::vec2 uvMax{1.0f, 1.0f};  // AB2 - bottom-right UV
        
        Billboard() = default;
        Billboard(const glm::vec3& pos, float w, float h, size_t texIndex)
            : position(pos), width(w), height(h), textureIndex(texIndex) {}
    };

    // BillboardManager - manages billboard instances using texture array like TextureManager
    class BillboardManager {
    public:
        BillboardManager() = default;
        BillboardManager(const BillboardManager&) = delete;
        BillboardManager& operator=(const BillboardManager&) = delete;

        size_t addBillboard(const Billboard& billboard);
        void updateBillboard(size_t index, const Billboard& billboard);
        void removeBillboard(size_t index);
        
        // Billboard storage - index-based access
        std::vector<Billboard> billboards;
        
        // Get all billboards using a specific texture
        std::vector<size_t> getBillboardsWithTexture(size_t textureIndex) const;
    };

} // namespace Az3D
