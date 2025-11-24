#pragma once

#include <glm/glm.hpp>

struct alignas(16) tinyMaterial {
    glm::vec4 base = glm::vec4(1.0f);
    glm::ivec4 tex1 = glm::ivec4(-1); // Albedo, specular, normal, occlusion
    glm::ivec4 tex2 = glm::ivec4(-1); // Metallic, roughness, emission, SHADER

    int& alb() noexcept { return tex1.x; }
    int& spec() noexcept { return tex1.y; }
    int& nrml() noexcept { return tex1.z; }
    int& oclu() noexcept { return tex1.w; }
    int& metal() noexcept { return tex2.x; }
    int& rough() noexcept { return tex2.y; }
    int& emiss() noexcept { return tex2.z; }
    int& shader() noexcept { return tex2.w; }
};