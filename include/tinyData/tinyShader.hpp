#pragma once

#include "tinyVk/Pipeline/PLineRaster.hpp"

struct tinyShader {
    tinyShader() noexcept = default;

    tinyShader(const tinyShader&) = delete;
    tinyShader& operator=(const tinyShader&) = delete;

    tinyShader(tinyShader&&) noexcept = default;
    tinyShader& operator=(tinyShader&&) noexcept = default;

    tinyVk::PLineRaster& rasPipeline() noexcept { return rasPipeline_; }

private:
    tinyVk::PLineRaster rasPipeline_;
};