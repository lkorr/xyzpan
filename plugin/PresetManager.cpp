#include "PresetManager.h"
#include "FactoryPresetData.h"

// ---------------------------------------------------------------------------
// Embedded factory preset data — maps display names to binary data generated
// by juce_add_binary_data from presets/factory/*.xml files.
// ---------------------------------------------------------------------------
const std::array<PresetManager::EmbeddedPreset, PresetManager::kNumFactory>&
PresetManager::getEmbeddedPresets()
{
    // JUCE binary data naming: file "Default.xml" → Default_xml, size Default_xmlSize
    static const std::array<EmbeddedPreset, kNumFactory> presets = {{
        { "Default",      "Default_xml",      FactoryPresetData::Default_xml,      FactoryPresetData::Default_xmlSize },
        { "Orbit XY",     "Orbit_XY_xml",     FactoryPresetData::Orbit_XY_xml,     FactoryPresetData::Orbit_XY_xmlSize },
        { "Slow Drift",   "Slow_Drift_xml",   FactoryPresetData::Slow_Drift_xml,   FactoryPresetData::Slow_Drift_xmlSize },
        { "Behind You",   "Behind_You_xml",   FactoryPresetData::Behind_You_xml,   FactoryPresetData::Behind_You_xmlSize },
        { "Fly Around",   "Fly_Around_xml",   FactoryPresetData::Fly_Around_xml,   FactoryPresetData::Fly_Around_xmlSize },
        { "Overhead",     "Overhead_xml",     FactoryPresetData::Overhead_xml,     FactoryPresetData::Overhead_xmlSize },
        { "Near Whisper", "Near_Whisper_xml", FactoryPresetData::Near_Whisper_xml, FactoryPresetData::Near_Whisper_xmlSize },
    }};
    return presets;
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts)
    : apvts_(apvts)
{
}

// ---------------------------------------------------------------------------
// Platform preset directories (Steinberg VST3 spec)
// ---------------------------------------------------------------------------
juce::File PresetManager::getFactoryPresetDir()
{
#if JUCE_WINDOWS
    // C:\ProgramData\VST3 Presets\XYZAudio\XYZPan
    return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
               .getChildFile("VST3 Presets")
               .getChildFile("XYZAudio")
               .getChildFile("XYZPan");
#elif JUCE_MAC
    return juce::File("/Library/Audio/Presets/XYZAudio/XYZPan");
#else
    return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
               .getChildFile("VST3 Presets/XYZAudio/XYZPan");
#endif
}

juce::File PresetManager::getUserPresetDir()
{
#if JUCE_WINDOWS
    // C:\Users\<user>\Documents\VST3 Presets\XYZAudio\XYZPan
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
               .getChildFile("VST3 Presets")
               .getChildFile("XYZAudio")
               .getChildFile("XYZPan");
#elif JUCE_MAC
    return juce::File("~/Library/Audio/Presets/XYZAudio/XYZPan");
#else
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
               .getChildFile("VST3 Presets/XYZAudio/XYZPan");
#endif
}

// ---------------------------------------------------------------------------
// ensureFactoryPresetsOnDisk — extract embedded presets if missing
// ---------------------------------------------------------------------------
void PresetManager::ensureFactoryPresetsOnDisk()
{
    auto dir = getFactoryPresetDir();

    // Try to create the directory. If it fails (no admin rights on
    // ProgramData), silently fall back to embedded data.
    if (!dir.isDirectory())
        dir.createDirectory();

    if (!dir.isDirectory())
        return;  // Can't write — embedded fallback will be used

    auto& embedded = getEmbeddedPresets();
    for (auto& ep : embedded) {
        auto file = dir.getChildFile(juce::String(ep.displayName) + ".xml");
        if (!file.existsAsFile())
            file.replaceWithData(ep.data, static_cast<size_t>(ep.size));
    }
}

// ---------------------------------------------------------------------------
// refreshPresetList — scan directories and rebuild
// ---------------------------------------------------------------------------
void PresetManager::refreshPresetList()
{
    presetList_.clear();

    auto& embedded = getEmbeddedPresets();
    auto factoryDir = getFactoryPresetDir();

    // Factory presets: prefer on-disk files in canonical order, fall back to embedded
    for (int i = 0; i < kNumFactory; ++i) {
        PresetEntry entry;
        entry.name = embedded[static_cast<size_t>(i)].displayName;
        entry.isFactory = true;

        auto onDisk = factoryDir.getChildFile(entry.name + ".xml");
        if (onDisk.existsAsFile())
            entry.file = onDisk;
        // else file remains empty → loadPreset will use embedded data

        presetList_.push_back(std::move(entry));
    }

    // User presets: scan user directory, sorted alphabetically
    auto userDir = getUserPresetDir();
    if (userDir.isDirectory()) {
        auto files = userDir.findChildFiles(juce::File::findFiles, false, "*.xml");
        files.sort();
        for (auto& f : files) {
            PresetEntry entry;
            entry.name = f.getFileNameWithoutExtension();
            entry.file = f;
            entry.isFactory = false;
            presetList_.push_back(std::move(entry));
        }
    }
}

// ---------------------------------------------------------------------------
// loadPreset
// ---------------------------------------------------------------------------
bool PresetManager::loadPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(presetList_.size()))
        return false;

    auto& entry = presetList_[static_cast<size_t>(index)];

    // Try on-disk file first
    if (entry.file.existsAsFile()) {
        auto xml = juce::parseXML(entry.file);
        if (xml != nullptr && xml->hasTagName(apvts_.state.getType())) {
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
            currentIndex_ = index;
            return true;
        }
    }

    // Embedded fallback for factory presets
    if (entry.isFactory && index < kNumFactory) {
        auto& ep = getEmbeddedPresets()[static_cast<size_t>(index)];
        if (applyXml(juce::String::fromUTF8(ep.data, ep.size))) {
            currentIndex_ = index;
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// saveUserPreset
// ---------------------------------------------------------------------------
bool PresetManager::saveUserPreset(const juce::String& name)
{
    if (name.isEmpty())
        return false;

    auto dir = getUserPresetDir();
    if (!dir.isDirectory())
        dir.createDirectory();
    if (!dir.isDirectory())
        return false;

    auto file = dir.getChildFile(name + ".xml");

    auto state = apvts_.copyState();
    auto xml = state.createXml();
    if (xml == nullptr)
        return false;

    if (!xml->writeTo(file))
        return false;

    refreshPresetList();

    // Set current index to the newly saved preset
    for (int i = 0; i < static_cast<int>(presetList_.size()); ++i) {
        if (!presetList_[static_cast<size_t>(i)].isFactory &&
            presetList_[static_cast<size_t>(i)].name == name) {
            currentIndex_ = i;
            break;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// deleteUserPreset
// ---------------------------------------------------------------------------
bool PresetManager::deleteUserPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(presetList_.size()))
        return false;

    auto& entry = presetList_[static_cast<size_t>(index)];
    if (entry.isFactory)
        return false;

    if (entry.file.existsAsFile())
        entry.file.deleteFile();

    refreshPresetList();

    if (currentIndex_ == index)
        currentIndex_ = 0;
    else if (currentIndex_ > index)
        --currentIndex_;

    return true;
}

// ---------------------------------------------------------------------------
// getFactoryPresetName
// ---------------------------------------------------------------------------
juce::String PresetManager::getFactoryPresetName(int index) const
{
    if (index >= 0 && index < kNumFactory)
        return getEmbeddedPresets()[static_cast<size_t>(index)].displayName;
    return "Unknown";
}

// ---------------------------------------------------------------------------
// applyXml — parse XML text and replace APVTS state
// ---------------------------------------------------------------------------
bool PresetManager::applyXml(const juce::String& xmlText)
{
    auto xml = juce::parseXML(xmlText);
    if (xml != nullptr && xml->hasTagName(apvts_.state.getType())) {
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
        return true;
    }
    return false;
}
