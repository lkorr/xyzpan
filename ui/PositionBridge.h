#pragma once
#include <atomic>

namespace xyzpan {

// Snapshot of source position for audio-to-GL data transfer.
// Written by audio thread after engine.process(); read by GL thread in renderOpenGL().
struct SourcePositionSnapshot {
    float x        = 0.0f;
    float y        = 1.0f;  // default: front (Y-forward convention)
    float z        = 0.0f;
    float distance = 1.0f;

    // Stereo source node positions (for GL rendering of L/R nodes)
    float lNodeX = 0.0f, lNodeY = 0.0f, lNodeZ = 0.0f;
    float rNodeX = 0.0f, rNodeY = 0.0f, rNodeZ = 0.0f;
    float stereoWidth = 0.0f;

    // Audible sphere boundary radius (for GL rendering)
    float sphereRadius = 1.732f;
};

// Lock-free double-buffer for audio->GL position transfer (UI-07).
// No mutex — safe for one writer (audio thread) and one reader (GL thread).
class PositionBridge {
public:
    // Called from audio thread after engine.process()
    void write(const SourcePositionSnapshot& pos) {
        const int idx = 1 - writeIdx_.load(std::memory_order_relaxed);
        buf_[idx] = pos;
        writeIdx_.store(idx, std::memory_order_release);
    }

    // Called from GL thread in renderOpenGL()
    [[nodiscard]] SourcePositionSnapshot read() const {
        return buf_[writeIdx_.load(std::memory_order_acquire)];
    }

private:
    SourcePositionSnapshot buf_[2];
    std::atomic<int> writeIdx_{0};
};

} // namespace xyzpan
