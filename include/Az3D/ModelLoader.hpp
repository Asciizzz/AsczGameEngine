#pragma once

#include <Az3D/MeshStatic.hpp>
#include <Az3D/MeshSkinned.hpp>

namespace Az3D {

    class ModelLoader {
    public:
        static MeshStatic loadStaticModel(const std::string& filePath);
        static MeshSkinned loadSkinnedModel(const std::string& filePath);
    };

} // namespace Az3D
