#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <array>

class PresetManager {
public:
    struct PresetEntry {
        juce::String name;
        juce::File   file;       // empty for embedded-only fallback
        bool         isFactory;
    };

    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);

    // Try to write embedded factory presets to disk (silent on failure)
    void ensureFactoryPresetsOnDisk();

    // Rescan factory + user dirs and rebuild the preset list
    void refreshPresetList();

    const std::vector<PresetEntry>& getPresets() const { return presetList_; }
    int getNumPresets() const { return static_cast<int>(presetList_.size()); }
    int getNumFactoryPresets() const { return kNumFactory; }

    // Load preset by index into APVTS. Returns true on success.
    bool loadPreset(int index);

    // Save current state as user preset. Returns true on success.
    bool saveUserPreset(const juce::String& name);

    // Delete a user preset by index. Returns false if factory or invalid.
    bool deleteUserPreset(int index);

    // Platform-correct preset directories
    static juce::File getFactoryPresetDir();
    static juce::File getUserPresetDir();

    int getCurrentIndex() const { return currentIndex_; }
    void setCurrentIndex(int i) { currentIndex_ = i; }

    // Get factory preset name by index (for DAW program API)
    juce::String getFactoryPresetName(int index) const;

private:
    juce::AudioProcessorValueTreeState& apvts_;
    std::vector<PresetEntry> presetList_;
    int currentIndex_ = 0;

    // Embedded factory preset data
    struct EmbeddedPreset {
        const char* displayName;
        const char* binaryName;  // identifier in FactoryPresetData namespace
        const char* data;
        int         size;
    };

    static constexpr int kNumFactory = 7;
    static const std::array<EmbeddedPreset, kNumFactory>& getEmbeddedPresets();

    // Load XML string into APVTS
    bool applyXml(const juce::String& xmlText);
};
