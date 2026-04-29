#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>

struct ConfigSyncThread;

class SessionConfig {
public:
    SessionConfig();
    ~SessionConfig();

    bool hasCached() const;

    using SyncCallback = std::function<void(bool success, const juce::String& message)>;
    void syncAsync(const juce::String& token, SyncCallback onResult);

    bool isReady() const { return ready_.load(std::memory_order_acquire); }

private:
    friend struct ConfigSyncThread;

    std::atomic<bool> ready_{false};
    std::unique_ptr<juce::Thread> syncThread_;

    static juce::File configPath();
    static juce::String envSalt();
    static juce::String digest(const juce::String& input);
    bool verify() const;
    void persist(const juce::String& token, const juce::String& tag);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionConfig)
};
