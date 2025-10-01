#pragma once

#include <chrono>
#include <deque>

class FpsManager {
public:
    FpsManager();

    // Call this once per frame
    void update();

    // Get average FPS over the sample window
    float getAverageFPS() const;
    
    // Get frame time statistics
    float getMinFrameTime() const;
    float getMaxFrameTime() const;
    
    // FPS limiting
    void setTargetFPS(float target) { targetFPS = target; }
    void enableVSync(bool enabled) { vsyncEnabled = enabled; }
    
    // Reset statistics
    void reset();

    
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    
    TimePoint lastFrameTime;
    TimePoint startTime;
    
    float currentFPS;
    float frameTimeMs;
    float deltaTime;
    float targetFPS;
    bool vsyncEnabled;
    
    // Frame time history for averaging
    static constexpr size_t SAMPLE_COUNT = 60;
    std::deque<float> frameTimeHistory;
    
    // Helper methods 
    void updateFrameTimeHistory(float frameTime);
    void limitFrameRate();
};
