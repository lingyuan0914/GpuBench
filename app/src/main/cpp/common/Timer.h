#pragma once

#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>

namespace GpuBench {

class FrameTimer {
public:
    void beginFrame() {
        frameStart_ = std::chrono::high_resolution_clock::now();
    }

    void endFrame() {
        frameEnd_ = std::chrono::high_resolution_clock::now();
        float ms = std::chrono::duration<float, std::milli>(frameEnd_ - frameStart_).count();
        frameTimes_.push_back(ms);
    }

    void reset() {
        frameTimes_.clear();
    }

    float getAverageMs() const {
        if (frameTimes_.empty()) return 0.0f;
        float sum = std::accumulate(frameTimes_.begin(), frameTimes_.end(), 0.0f);
        return sum / frameTimes_.size();
    }

    float getAverageFps() const {
        float ms = getAverageMs();
        return ms > 0.0f ? 1000.0f / ms : 0.0f;
    }

    float getMinMs() const {
        if (frameTimes_.empty()) return 0.0f;
        return *std::min_element(frameTimes_.begin(), frameTimes_.end());
    }

    float getMaxMs() const {
        if (frameTimes_.empty()) return 0.0f;
        return *std::max_element(frameTimes_.begin(), frameTimes_.end());
    }

    float getMedianMs() const {
        if (frameTimes_.empty()) return 0.0f;
        std::vector<float> sorted = frameTimes_;
        std::sort(sorted.begin(), sorted.end());
        size_t n = sorted.size();
        if (n % 2 == 0) {
            return (sorted[n/2 - 1] + sorted[n/2]) / 2.0f;
        }
        return sorted[n/2];
    }

    float getPercentile95() const {
        if (frameTimes_.empty()) return 0.0f;
        std::vector<float> sorted = frameTimes_;
        std::sort(sorted.begin(), sorted.end());
        size_t idx = static_cast<size_t>(sorted.size() * 0.95f);
        if (idx >= sorted.size()) idx = sorted.size() - 1;
        return sorted[idx];
    }

    float getStdDev() const {
        if (frameTimes_.size() < 2) return 0.0f;
        float mean = getAverageMs();
        float variance = 0.0f;
        for (float t : frameTimes_) {
            float diff = t - mean;
            variance += diff * diff;
        }
        variance /= frameTimes_.size();
        return std::sqrt(variance);
    }

    size_t getFrameCount() const { return frameTimes_.size(); }

private:
    std::chrono::high_resolution_clock::time_point frameStart_, frameEnd_;
    std::vector<float> frameTimes_;
};

} // namespace GpuBench
