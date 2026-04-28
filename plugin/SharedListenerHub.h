#pragma once
#include <juce_core/juce_core.h>
#include "PositionBridge.h"
#include <vector>
#include <algorithm>
#include <atomic>
#include <functional>
#include <cstring>

// Process-wide singleton shared across all XYZPan plugin instances in the same
// DAW host process via juce::SharedResourcePointer<SharedListenerHub>.
// Manages linked listener orientation synchronization.
//
// Thread safety: all methods acquire spinLock_. Each Listener carries an atomic
// alive flag that is cleared *before* removeLinkedInstance() so that any
// concurrent iteration that already holds the lock will skip the dying instance
// rather than calling a virtual method on a partially-destroyed object.
class SharedListenerHub {
public:
    struct Listener {
        virtual ~Listener() = default;
        virtual void listenerOrientationChanged(float yaw, float pitch, float roll,
                                                 bool headFollows) = 0;
        virtual void listenerPositionChanged(float /*x*/, float /*y*/, float /*z*/) {}
        virtual xyzpan::SourceExportBuffer* getSourceExportBuffer() { return nullptr; }

        // Returns the AudioProcessor that owns this listener (for remote control).
        virtual juce::AudioProcessor* getProcessor() { return nullptr; }

        // User-visible instance name (e.g. DAW track name or manual rename).
        virtual juce::String getInstanceName() const { return {}; }

        // Called when this instance gains or loses pilot status.
        virtual void pilotStatusChanged(bool /*isPilot*/) {}

        // Returns this instance's source shape enum value (for cross-instance rendering).
        virtual int getSourceShape() const { return 0; }

        // Whether this instance's audible radius sphere should be visible.
        virtual bool getShowSphere() const { return true; }

        // Safety flag — cleared by the instance before destruction begins.
        // The hub checks this under the spinlock before dispatching to skip
        // any instance whose destructor is in progress on another thread.
        std::atomic<bool> hubAlive_{true};
    };

    SharedListenerHub() = default;

    void addLinkedInstance(Listener* l) {
        l->hubAlive_.store(true, std::memory_order_release);
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        if (std::find(linked_.begin(), linked_.end(), l) == linked_.end())
            linked_.push_back(l);
    }

    // Two-phase removal: the caller must set l->hubAlive_ = false BEFORE calling
    // this (see prepareToRemove). The flag prevents any concurrent broadcast/
    // getLinkedSources from calling virtual methods on a dying object. The
    // removal itself then erases the pointer under the lock.
    void removeLinkedInstance(Listener* l) {
        Listener* newPilot = nullptr;
        {
            const juce::SpinLock::ScopedLockType lock(spinLock_);
            linked_.erase(std::remove(linked_.begin(), linked_.end(), l), linked_.end());
            if (pilot_ == l) {
                pilot_ = linked_.empty() ? nullptr : linked_.front();
                newPilot = pilot_;
            }
        }
        if (newPilot && newPilot->hubAlive_.load(std::memory_order_acquire))
            newPilot->pilotStatusChanged(true);
    }

    // Convenience: atomically mark dead + remove + fire removal callbacks.
    // This is the preferred API — call as the VERY FIRST line of the destructor.
    // Callbacks are fired OUTSIDE the spinlock so they can safely do
    // heavyweight work (SliderAttachment teardown, APVTS operations, etc.).
    void detachInstance(Listener* l) {
        l->hubAlive_.store(false, std::memory_order_release);

        // Snapshot callbacks and remove from linked list under the lock
        std::vector<std::function<void(Listener*)>> cbSnapshot;
        Listener* newPilot = nullptr;
        {
            const juce::SpinLock::ScopedLockType lock(spinLock_);
            linked_.erase(std::remove(linked_.begin(), linked_.end(), l), linked_.end());
            if (pilot_ == l) {
                pilot_ = linked_.empty() ? nullptr : linked_.front();
                newPilot = pilot_;
            }
            cbSnapshot.reserve(removalCallbacks_.size());
            for (auto& cb : removalCallbacks_)
                cbSnapshot.push_back(cb.fn);
        }

        // Fire callbacks outside the lock — safe for expensive operations
        for (auto& fn : cbSnapshot)
            fn(l);

        // Notify auto-promoted pilot after removal callbacks
        if (newPilot && newPilot->hubAlive_.load(std::memory_order_acquire))
            newPilot->pilotStatusChanged(true);
    }

    void broadcastOrientation(Listener* sender, float yaw, float pitch, float roll,
                               bool headFollows) {
        constexpr int kMaxBroadcast = 16;
        Listener* targets[kMaxBroadcast];
        int count = 0;
        {
            const juce::SpinLock::ScopedLockType lock(spinLock_);
            cachedYaw_   = yaw;
            cachedPitch_ = pitch;
            cachedRoll_  = roll;
            cachedHeadFollows_ = headFollows;
            hasCache_ = true;

            for (auto* l : linked_)
                if (l != sender && l->hubAlive_.load(std::memory_order_acquire)
                    && count < kMaxBroadcast)
                    targets[count++] = l;
        }
        // Update shared atomics (lock-free, audio-thread readable)
        sharedYaw.store(yaw, std::memory_order_release);
        sharedPitch.store(pitch, std::memory_order_release);
        sharedRoll.store(roll, std::memory_order_release);

        // Dispatch outside the lock — setValue may do APVTS listener work
        for (int i = 0; i < count; ++i)
            if (targets[i]->hubAlive_.load(std::memory_order_acquire))
                targets[i]->listenerOrientationChanged(yaw, pitch, roll, headFollows);
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
        constexpr int kMaxBroadcast = 16;
        Listener* targets[kMaxBroadcast];
        int count = 0;
        {
            const juce::SpinLock::ScopedLockType lock(spinLock_);
            cachedPosX_ = x;
            cachedPosY_ = y;
            cachedPosZ_ = z;
            hasPosCache_ = true;

            for (auto* l : linked_)
                if (l != sender && l->hubAlive_.load(std::memory_order_acquire)
                    && count < kMaxBroadcast)
                    targets[count++] = l;
        }
        sharedWalkerX.store(x, std::memory_order_release);
        sharedWalkerY.store(y, std::memory_order_release);
        sharedWalkerZ.store(z, std::memory_order_release);

        for (int i = 0; i < count; ++i)
            if (targets[i]->hubAlive_.load(std::memory_order_acquire))
                targets[i]->listenerPositionChanged(x, y, z);
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

    // Returns pointers to all linked instances. Returns count written.
    int getLinkedInstances(Listener** out, int maxOut) const {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        int count = 0;
        for (auto* l : linked_) {
            if (count >= maxOut) break;
            out[count++] = l;
        }
        return count;
    }

    // Returns the index of a listener in the linked list, or -1 if not found.
    int getLinkedIndex(const Listener* l) const {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        for (int i = 0; i < static_cast<int>(linked_.size()); ++i)
            if (linked_[i] == l) return i;
        return -1;
    }

    // --- Pilot management ---
    // Only the pilot instance can broadcast listener orientation/position changes.
    // Mutual exclusion: claiming pilot revokes it from the previous holder.

    void claimPilot(Listener* l) {
        Listener* oldPilot = nullptr;
        {
            const juce::SpinLock::ScopedLockType lock(spinLock_);
            if (std::find(linked_.begin(), linked_.end(), l) == linked_.end())
                return;
            oldPilot = pilot_;
            pilot_ = l;
        }
        // Notify outside lock to avoid deadlocks from APVTS operations
        if (oldPilot && oldPilot != l && oldPilot->hubAlive_.load(std::memory_order_acquire))
            oldPilot->pilotStatusChanged(false);
        if (l->hubAlive_.load(std::memory_order_acquire))
            l->pilotStatusChanged(true);
    }

    void releasePilot(Listener* l) {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        if (pilot_ == l)
            pilot_ = nullptr;
    }

    bool isPilot(const Listener* l) const {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        return pilot_ == l;
    }

    juce::String getPilotName() const {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        if (pilot_ && pilot_->hubAlive_.load(std::memory_order_acquire))
            return pilot_->getInstanceName();
        return {};
    }

    // Register a callback invoked (under lock, on message thread) when any
    // instance is removed. Editors use this to detach remote attachments
    // before the processor is destroyed.
    void addRemovalCallback(void* owner, std::function<void(Listener*)> cb) {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        removalCallbacks_.push_back({owner, std::move(cb)});
    }

    void removeRemovalCallback(void* owner) {
        const juce::SpinLock::ScopedLockType lock(spinLock_);
        removalCallbacks_.erase(
            std::remove_if(removalCallbacks_.begin(), removalCallbacks_.end(),
                           [owner](const RemovalCB& cb) { return cb.owner == owner; }),
            removalCallbacks_.end());
    }

    // Collect source positions from all linked instances except caller.
    // Returns number of sources written.
    // Lock is held only to snapshot pointers; virtual calls happen outside.
    int getLinkedSources(const Listener* caller,
                         xyzpan::ForeignSourceSnapshot* out, int maxOut) {
        constexpr int kMaxSnapshot = 128;
        Listener* snapshot[kMaxSnapshot];
        bool isPilotFlags[kMaxSnapshot];
        int snapshotCount = 0;
        {
            const juce::SpinLock::ScopedLockType lock(spinLock_);
            for (auto* l : linked_) {
                if (l == caller) continue;
                if (!l->hubAlive_.load(std::memory_order_acquire)) continue;
                if (snapshotCount >= kMaxSnapshot) break;
                isPilotFlags[snapshotCount] = (l == pilot_);
                snapshot[snapshotCount++] = l;
            }
        }
        // Iterate outside lock — virtual calls + string copies are safe because
        // hubAlive_ is checked again and detachInstance() clears it before destruction.
        int count = 0;
        int colorIdx = 0;
        for (int i = 0; i < snapshotCount && count < maxOut; ++i) {
            auto* l = snapshot[i];
            if (!l->hubAlive_.load(std::memory_order_acquire)) continue;
            auto* buf = l->getSourceExportBuffer();
            if (buf == nullptr) continue;
            out[count] = buf->read();
            out[count].colorIndex = colorIdx++;
            out[count].sourceShape = l->getSourceShape();
            out[count].showSphere = l->getShowSphere();
            out[count].isPilot = isPilotFlags[i];
            auto nameStr = l->getInstanceName();
            if (nameStr.isEmpty())
                nameStr = "Source " + juce::String(colorIdx);
            auto nameUtf8 = nameStr.toUTF8();
            const size_t len = std::min<size_t>(nameUtf8.sizeInBytes(),
                                                 sizeof(out[count].name) - 1);
            std::memcpy(out[count].name, nameUtf8.getAddress(), len);
            out[count].name[len] = '\0';
            ++count;
        }
        return count;
    }

    // Lock-free shared values — audio threads read these directly when linked.
    // Written by broadcast methods (message thread) with release semantics.
    std::atomic<float> sharedYaw{0.0f};
    std::atomic<float> sharedPitch{0.0f};
    std::atomic<float> sharedRoll{0.0f};
    std::atomic<float> sharedWalkerX{0.0f};
    std::atomic<float> sharedWalkerY{0.0f};
    std::atomic<float> sharedWalkerZ{0.0f};

    // Monotonic counter — bumped whenever any instance persists user preferences
    // (theme / scene / avatar). Other instances poll this and reload on increment.
    std::atomic<uint32_t> sharedPreferencesVersion{0};
    void bumpPreferencesVersion() {
        sharedPreferencesVersion.fetch_add(1, std::memory_order_release);
    }

private:
    struct RemovalCB {
        void* owner;
        std::function<void(Listener*)> fn;
    };

    mutable juce::SpinLock spinLock_;
    std::vector<Listener*> linked_;
    std::vector<RemovalCB> removalCallbacks_;

    bool  hasCache_ = false;
    float cachedYaw_   = 0.0f;
    float cachedPitch_ = 0.0f;
    float cachedRoll_  = 0.0f;
    bool  cachedHeadFollows_ = false;

    bool  hasPosCache_ = false;
    float cachedPosX_  = 0.0f;
    float cachedPosY_  = 0.0f;
    float cachedPosZ_  = 0.0f;

    Listener* pilot_ = nullptr;  // current pilot, or nullptr if none
};
