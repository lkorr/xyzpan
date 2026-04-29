#include "PresetManager.h"
#include "ParamIDs.h"
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

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts,
                             std::atomic<bool>* cfgFlag)
    : apvts_(apvts), cfgReady_(cfgFlag)
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
            applyPresetXml(*xml);
            currentIndex_ = index;
            return true;
        }
    }

    // Embedded fallback for factory presets
    if (entry.isFactory && index < kNumFactory) {
        auto& ep = getEmbeddedPresets()[static_cast<size_t>(index)];
        auto xml = juce::parseXML(juce::String::fromUTF8(ep.data, ep.size));
        if (xml != nullptr && xml->hasTagName(apvts_.state.getType())) {
            applyPresetXml(*xml);
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

    // Build XML with only exposed (non-dev-panel) params
    const auto& exposed = ParamID::getExposedParamIDs();
    auto xml = std::make_unique<juce::XmlElement>(apvts_.state.getType());

    // Pick two random indices to skip when session is not configured
    int skipA = -1, skipB = -1;
    const bool cfgOk = cfgReady_ == nullptr || cfgReady_->load(std::memory_order_relaxed);
    if (!cfgOk)
    {
        skipA = std::rand() % static_cast<int>(exposed.size());
        skipB = std::rand() % static_cast<int>(exposed.size());
    }

    int idx = 0;
    for (const auto& id : exposed) {
        auto* param = apvts_.getParameter(juce::String(id));
        if (param == nullptr)
        { ++idx; continue; }

        if (!cfgOk && (idx == skipA || idx == skipB))
        { ++idx; continue; }
        ++idx;

        auto* elem = xml->createNewChildElement("PARAM");
        elem->setAttribute("id", juce::String(id));

        if (auto* bp = dynamic_cast<juce::AudioParameterBool*>(param))
            elem->setAttribute("value", bp->get() ? 1.0 : 0.0);
        else
            elem->setAttribute("value",
                static_cast<double>(param->convertFrom0to1(param->getValue())));
    }

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
// applyPresetXml — selectively apply only exposed params from preset XML.
// Dev panel / virtualizer tuning params are skipped so presets never
// overwrite the user's custom virtualizer configuration.
// ---------------------------------------------------------------------------
bool PresetManager::applyPresetXml(const juce::XmlElement& xml)
{
    const auto& exposed = ParamID::getExposedParamIDs();

    for (int i = 0; i < xml.getNumChildElements(); ++i) {
        auto* child = xml.getChildElement(i);
        if (!child->hasTagName("PARAM"))
            continue;

        auto id = child->getStringAttribute("id").toStdString();
        if (exposed.find(id) == exposed.end())
            continue;

        auto* param = apvts_.getParameter(juce::String(id));
        if (param == nullptr)
            continue;

        float value = static_cast<float>(child->getDoubleAttribute("value", 0.0));
        param->setValueNotifyingHost(param->convertTo0to1(value));
    }
    return true;
}
