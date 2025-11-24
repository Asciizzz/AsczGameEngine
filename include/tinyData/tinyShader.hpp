#pragma once

#include "tinyVk/Pipeline/Pipeline_raster.hpp"

struct tinyShader {
    tinyShader() noexcept = default;

    tinyShader(const tinyShader&) = delete;
    tinyShader& operator=(const tinyShader&) = delete;

    tinyShader(tinyShader&&) noexcept = default;
    tinyShader& operator=(tinyShader&&) noexcept = default;

    tinyVk::PipelineRaster& rasPipeline() noexcept { return rasPipeline_; }

private:
    tinyVk::PipelineRaster rasPipeline_;
};