#include <catch2/catch_test_macros.hpp>
#include "xyzpan/Engine.h"
#include "xyzpan/Types.h"
#include <chrono>
#include <vector>

using namespace xyzpan;

// Performance microbenchmarks — run these in Release configuration only.
// In Debug, MSVC does not optimize and these will not meet the time budget.
// The verify command uses -C Release so these pass in CI.
// If you see failures in Debug that is expected behaviour.

TEST_CASE("Engine throughput: 128 samples at 44.1kHz under 10% CPU", "[performance]") {
    XYZPanEngine engine;
    engine.prepare(44100.0, 128);

    std::vector<float> inBuf(128, 0.5f);
    std::vector<float> outL(128), outR(128);
    const float* inputs[1] = { inBuf.data() };

    EngineParams params;
    params.x = 0.3f; params.y = 0.8f; params.z = 0.1f;
    engine.setParams(params);

    // Warm up caches and branch predictors
    for (int i = 0; i < 100; ++i)
        engine.process(inputs, 1, outL.data(), outR.data(), nullptr, nullptr, 128);

    constexpr int kTrials = 1000;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kTrials; ++i)
        engine.process(inputs, 1, outL.data(), outR.data(), nullptr, nullptr, 128);
    auto t1 = std::chrono::steady_clock::now();

    const double usPerBlock =
        std::chrono::duration<double, std::micro>(t1 - t0).count() / kTrials;

    // Budget: 128/44100 * 1e6 = 2902us. Target: <10% CPU = <290us
    INFO("throughput: " << usPerBlock << " us per 128-sample block (budget: 290 us)");
    CHECK(usPerBlock < 290.0);
}

TEST_CASE("Engine throughput: 64 samples at 44.1kHz under 10% CPU", "[performance]") {
    XYZPanEngine engine;
    engine.prepare(44100.0, 64);

    std::vector<float> inBuf(64, 0.5f);
    std::vector<float> outL(64), outR(64);
    const float* inputs[1] = { inBuf.data() };

    EngineParams params;
    params.x = 0.3f; params.y = 0.8f; params.z = 0.1f;
    engine.setParams(params);

    // Warm up
    for (int i = 0; i < 100; ++i)
        engine.process(inputs, 1, outL.data(), outR.data(), nullptr, nullptr, 64);

    constexpr int kTrials = 1000;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kTrials; ++i)
        engine.process(inputs, 1, outL.data(), outR.data(), nullptr, nullptr, 64);
    auto t1 = std::chrono::steady_clock::now();

    const double usPerBlock =
        std::chrono::duration<double, std::micro>(t1 - t0).count() / kTrials;

    // Budget: 64/44100 * 1e6 = 1451us. Target: <10% CPU = <145us
    INFO("throughput: " << usPerBlock << " us per 64-sample block (budget: 145 us)");
    CHECK(usPerBlock < 145.0);
}

TEST_CASE("Engine throughput: stereo 128 samples at 44.1kHz under 20% CPU", "[performance]") {
    XYZPanEngine engine;
    engine.prepare(44100.0, 128);

    std::vector<float> inBufL(128, 0.5f);
    std::vector<float> inBufR(128, 0.3f);
    std::vector<float> outL(128), outR(128);
    const float* inputs[2] = { inBufL.data(), inBufR.data() };

    EngineParams params;
    params.x = 0.3f; params.y = 0.8f; params.z = 0.1f;
    params.stereoWidth = 0.5f;  // enable stereo path
    engine.setParams(params);

    // Warm up
    for (int i = 0; i < 100; ++i)
        engine.process(inputs, 2, outL.data(), outR.data(), nullptr, nullptr, 128);

    constexpr int kTrials = 1000;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kTrials; ++i)
        engine.process(inputs, 2, outL.data(), outR.data(), nullptr, nullptr, 128);
    auto t1 = std::chrono::steady_clock::now();

    const double usPerBlock =
        std::chrono::duration<double, std::micro>(t1 - t0).count() / kTrials;

    // Budget for stereo: 2x mono pipeline, allow 20% CPU = 580us at 128/44.1k
    INFO("throughput: " << usPerBlock << " us per stereo 128-sample block (budget: 580 us)");
    CHECK(usPerBlock < 580.0);
}
