#include "AzCore/FpsManager.hpp"
#include <algorithm>
#include <thread>

namespace AzCore {

FpsManager::FpsManager() 
    : lastFrameTime(Clock::now()), startTime(Clock::now()),
        currentFPS(0.0f), frameTimeMs(0.0f), deltaTime(0.0f),
        targetFPS(0.0f), vsyncEnabled(false) {
}

void FpsManager::update() {
    auto currentTime = Clock::now();
    
    // Calculate delta time in seconds
    auto duration = currentTime - lastFrameTime;
    deltaTime = std::chrono::duration<float>(duration).count();
    
    // Calculate frame time in milliseconds
    frameTimeMs = deltaTime * 1000.0f;
    
    // Calculate FPS (avoid division by zero)
    if (deltaTime > 0.0f) {
        currentFPS = 1.0f / deltaTime;
    }
    
    // Update frame time history for averaging
    updateFrameTimeHistory(frameTimeMs);
    
    // Apply frame rate limiting if needed
    if (!vsyncEnabled && targetFPS > 0.0f) {
        limitFrameRate();
    }
    
    lastFrameTime = currentTime;
}

float FpsManager::getAverageFPS() const {
    if (frameTimeHistory.empty()) {
        return 0.0f;
    }
    
    float totalFrameTime = 0.0f;
    for (float frameTime : frameTimeHistory) {
        totalFrameTime += frameTime;
    }
    
    float averageFrameTime = totalFrameTime / frameTimeHistory.size();
    return averageFrameTime > 0.0f ? 1000.0f / averageFrameTime : 0.0f;
}

float FpsManager::getMinFrameTime() const {
    if (frameTimeHistory.empty()) {
        return 0.0f;
    }
    return *std::min_element(frameTimeHistory.begin(), frameTimeHistory.end());
}

float FpsManager::getMaxFrameTime() const {
    if (frameTimeHistory.empty()) {
        return 0.0f;
    }
    return *std::max_element(frameTimeHistory.begin(), frameTimeHistory.end());
}

void FpsManager::reset() {
    lastFrameTime = Clock::now();
    startTime = Clock::now();
    currentFPS = 0.0f;
    frameTimeMs = 0.0f;
    deltaTime = 0.0f;
    frameTimeHistory.clear();
}

void FpsManager::updateFrameTimeHistory(float frameTime) {
    frameTimeHistory.push_back(frameTime);
    
    // Keep only the last SAMPLE_COUNT frames
    if (frameTimeHistory.size() > SAMPLE_COUNT) {
        frameTimeHistory.pop_front();
    }
}

void FpsManager::limitFrameRate() {
    if (targetFPS <= 0.0f) {
        return;
    }
    
    float targetFrameTime = 1.0f / targetFPS;
    auto targetDuration = std::chrono::duration<float>(targetFrameTime);
    auto targetTimePoint = lastFrameTime + std::chrono::duration_cast<Clock::duration>(targetDuration);
    
    auto currentTime = Clock::now();
    if (currentTime < targetTimePoint) {
        std::this_thread::sleep_until(targetTimePoint);
    }
}

}
