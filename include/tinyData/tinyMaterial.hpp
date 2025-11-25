#pragma once

#include <glm/glm.hpp>

struct tinyMaterial {
    struct alignas(16) Data {
        glm::vec4 base = glm::vec4(1.0f);
        glm::ivec4 tex1 = glm::ivec4(-1); // Albedo, specular, normal, occlusion
        glm::ivec4 tex2 = glm::ivec4(-1); // Metallic, roughness, emission,
    } data;

    tinyHandle shader;

    int& alb() noexcept { return data.tex1.x; }
    int& spec() noexcept { return data.tex1.y; }
    int& nrml() noexcept { return data.tex1.z; }
    int& oclu() noexcept { return data.tex1.w; }
    int& metal() noexcept { return data.tex2.x; }
    int& rough() noexcept { return data.tex2.y; }
    int& emiss() noexcept { return data.tex2.z; }
};