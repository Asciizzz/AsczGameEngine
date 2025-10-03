#pragma once

class TinyRenderer {
public:
    TinyRenderer() = default;
    ~TinyRenderer() = default;

    // Delete copy semantics
    TinyRenderer(const TinyRenderer&) = delete;
    TinyRenderer& operator=(const TinyRenderer&) = delete;

};