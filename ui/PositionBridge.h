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

    // Listener head orientation (radians, for GL rendering)
    float listenerYaw   = 0.0f;
    float listenerPitch = 0.0f;
    float listenerRoll  = 0.0f;

    // Audible sphere boundary radius (for GL rendering)
    float sphereRadius = 1.732f;

    // Walker listener position (for GL rendering)
    float listenerPosX = 0.0f;
    float listenerPosY = 0.0f;
    float listenerPosZ = 0.0f;
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

// ---------------------------------------------------------------------------
// Foreign (linked-instance) source visualization types
// ---------------------------------------------------------------------------

static constexpr int kMaxLinkedSources = 8;

struct ForeignSourceSnapshot {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float distance    = 1.0f;
    float stereoWidth = 0.0f;
    float lNodeX = 0.0f, lNodeY = 0.0f, lNodeZ = 0.0f;
    float rNodeX = 0.0f, rNodeY = 0.0f, rNodeZ = 0.0f;
    int   colorIndex  = 0;
    char  name[32]    = {};  // null-terminated instance name for GL labels
};

// Lock-free double-buffer: audio thread → message thread source position export.
// One writer (audio thread), one reader (message thread timer).
class SourceExportBuffer {
public:
    void write(float x, float y, float z, float distance, float stereoWidth,
               float lx, float ly, float lz, float rx, float ry, float rz) {
        const int idx = 1 - writeIdx_.load(std::memory_order_relaxed);
        auto& b = buf_[idx];
        b.x = x;  b.y = y;  b.z = z;
        b.distance = distance;
        b.stereoWidth = stereoWidth;
        b.lNodeX = lx;  b.lNodeY = ly;  b.lNodeZ = lz;
        b.rNodeX = rx;  b.rNodeY = ry;  b.rNodeZ = rz;
        writeIdx_.store(idx, std::memory_order_release);
    }

    [[nodiscard]] ForeignSourceSnapshot read() const {
        return buf_[writeIdx_.load(std::memory_order_acquire)];
    }

private:
    ForeignSourceSnapshot buf_[2];
    std::atomic<int> writeIdx_{0};
};

// Lock-free double-buffer: message thread → GL thread foreign source payload.
// One writer (timer callback), one reader (renderOpenGL).
class ForeignSourceBridge {
public:
    struct Payload {
        ForeignSourceSnapshot sources[kMaxLinkedSources];
        int count = 0;
    };

    void write(const Payload& p) {
        const int idx = 1 - writeIdx_.load(std::memory_order_relaxed);
        buf_[idx] = p;
        writeIdx_.store(idx, std::memory_order_release);
    }

    [[nodiscard]] Payload read() const {
        return buf_[writeIdx_.load(std::memory_order_acquire)];
    }

private:
    Payload buf_[2];
    std::atomic<int> writeIdx_{0};
};

} // namespace xyzpan
