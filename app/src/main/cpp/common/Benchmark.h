#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace GpuBench {

// 测试结果结构
struct TestResult {
    std::string name;
    std::string api;           // "OpenGL ES" 或 "Vulkan"
    float fps;
    float frameTimeMs;
    float gpuTimeMs;
    float cpuTimeMs;
    uint64_t triangles;
    uint64_t drawCalls;
    float memoryBandwidthGBs;
    float computeGFLOPS;
};

// 测试配置
struct TestConfig {
    int width = 1080;
    int height = 2340;
    int frameCount = 300;
    int warmupFrames = 60;
    bool vsync = false;
};

// 基准测试基类
class Benchmark {
public:
    virtual ~Benchmark() = default;

    virtual bool initialize(void* surface, int width, int height) = 0;
    virtual void shutdown() = 0;

    virtual TestResult run(const std::string& testName, int frameCount) = 0;

    virtual std::vector<TestResult> runAllTests() {
        std::vector<TestResult> results;
        for (const auto& test : getTestNames()) {
            results.push_back(run(test, config_.frameCount));
        }
        return results;
    }

    virtual std::vector<std::string> getTestNames() const = 0;
    virtual std::string getApiName() const = 0;

    void setConfig(const TestConfig& config) { config_ = config; }
    TestConfig getConfig() const { return config_; }

protected:
    TestConfig config_;

    // 计时工具
    class Timer {
    public:
        void start() { start_ = std::chrono::high_resolution_clock::now(); }
        void stop() { end_ = std::chrono::high_resolution_clock::now(); }
        float elapsedMs() const {
            return std::chrono::duration<float, std::milli>(end_ - start_).count();
        }
        float elapsedUs() const {
            return std::chrono::duration<float, std::micro>(end_ - start_).count();
        }
    private:
        std::chrono::high_resolution_clock::time_point start_, end_;
    };
};

} // namespace GpuBench
