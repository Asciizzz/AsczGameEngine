#pragma once

#include <chrono>
#include <deque>

namespace AzCore {
    class FpsManager {
    public:
        FpsManager();

        // Call this once per frame
        void update();

        // Get current FPS
        float getFPS() const { return currentFPS; }
        
        // Get frame time in milliseconds
        float getFrameTimeMs() const { return frameTimeMs; }
        
        // Get delta time in seconds (time since last frame)
        float getDeltaTime() const { return deltaTime; }
        
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

    private:
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
        
        void updateFrameTimeHistory(float frameTime);
        void limitFrameRate();
    };
}
