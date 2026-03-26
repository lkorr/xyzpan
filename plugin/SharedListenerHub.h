#pragma once
#include <juce_core/juce_core.h>
#include "PositionBridge.h"
#include <vector>
#include <algorithm>

// Process-wide singleton shared across all XYZPan plugin instances in the same
// DAW host process via juce::SharedResourcePointer<SharedListenerHub>.
// Manages linked listener orientation synchronization.
// All methods are called on the message thread only.
class SharedListenerHub {
public:
    struct Listener {
        virtual ~Listener() = default;
        virtual void listenerOrientationChanged(float yaw, float pitch, float roll,
                                                 bool headFollows) = 0;
        virtual void listenerPositionChanged(float /*x*/, float /*y*/, float /*z*/) {}
        virtual xyzpan::SourceExportBuffer* getSourceExportBuffer() { return nullptr; }
    };

    SharedListenerHub() = default;

    void addLinkedInstance(Listener* l) {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        if (std::find(linked_.begin(), linked_.end(), l) == linked_.end())
            linked_.push_back(l);
    }

    void removeLinkedInstance(Listener* l) {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        linked_.erase(std::remove(linked_.begin(), linked_.end(), l), linked_.end());
    }

    void broadcastOrientation(Listener* sender, float yaw, float pitch, float roll,
                               bool headFollows) {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        cachedYaw_   = yaw;
        cachedPitch_ = pitch;
        cachedRoll_  = roll;
        cachedHeadFollows_ = headFollows;
        hasCache_ = true;

        for (auto* l : linked_) {
            if (l != sender)
                l->listenerOrientationChanged(yaw, pitch, roll, headFollows);
        }
    }

    bool getCachedOrientation(float& yaw, float& pitch, float& roll,
                               bool& headFollows) const {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        if (!hasCache_)
            return false;
        yaw   = cachedYaw_;
        pitch = cachedPitch_;
        roll  = cachedRoll_;
        headFollows = cachedHeadFollows_;
        return true;
    }

    void broadcastPosition(Listener* sender, float x, float y, float z) {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        cachedPosX_ = x;
        cachedPosY_ = y;
        cachedPosZ_ = z;
        hasPosCache_ = true;

        for (auto* l : linked_) {
            if (l != sender)
                l->listenerPositionChanged(x, y, z);
        }
    }

    bool getCachedPosition(float& x, float& y, float& z) const {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        if (!hasPosCache_)
            return false;
        x = cachedPosX_;
        y = cachedPosY_;
        z = cachedPosZ_;
        return true;
    }

    int getLinkedCount() const {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        return static_cast<int>(linked_.size());
    }

    // Collect source positions from all linked instances except caller.
    // Called on message thread only. Returns number of sources written.
    int getLinkedSources(const Listener* caller,
                         xyzpan::ForeignSourceSnapshot* out, int maxOut) {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        int count = 0;
        int colorIdx = 0;
        for (auto* l : linked_) {
            if (l == caller) continue;
            auto* buf = l->getSourceExportBuffer();
            if (buf == nullptr) continue;
            if (count >= maxOut) break;
            out[count] = buf->read();
            out[count].colorIndex = colorIdx++;
            ++count;
        }
        return count;
    }

private:
    mutable juce::SpinLock spinLock_;
    std::vector<Listener*> linked_;

    bool  hasCache_ = false;
    float cachedYaw_   = 0.0f;
    float cachedPitch_ = 0.0f;
    float cachedRoll_  = 0.0f;
    bool  cachedHeadFollows_ = false;

    bool  hasPosCache_ = false;
    float cachedPosX_  = 0.0f;
    float cachedPosY_  = 0.0f;
    float cachedPosZ_  = 0.0f;
};
