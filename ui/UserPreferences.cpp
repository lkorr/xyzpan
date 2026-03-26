#include "UserPreferences.h"
#include <juce_core/juce_core.h>

namespace xyzpan {

static juce::File getPrefsFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("XYZPan");
    dir.createDirectory();
    return dir.getChildFile("preferences.json");
}

UserPreferences::UserPreferences()
{
    load();
}

void UserPreferences::setThemeIndex(int index)
{
    themeIndex_ = juce::jlimit(0, kNumThemes - 1, index);
    save();
}

void UserPreferences::setAvatarParams(const AvatarParams& params)
{
    avatar_ = params;
    save();
}

void UserPreferences::load()
{
    auto file = getPrefsFile();
    if (!file.existsAsFile())
        return;

    auto text = file.loadFileAsString();
    auto parsed = juce::JSON::parse(text);
    if (parsed.isVoid())
        return;

    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return;

    themeIndex_ = juce::jlimit(0, kNumThemes - 1,
                                static_cast<int>(obj->getProperty("themeIndex")));

    if (auto* av = obj->getProperty("avatar").getDynamicObject()) {
        auto readFloat = [&](const char* key, float fallback) -> float {
            auto v = av->getProperty(key);
            return v.isVoid() ? fallback : static_cast<float>(static_cast<double>(v));
        };
        avatar_.headElongation = readFloat("headElongation", 1.0f);
        avatar_.eyeSize        = readFloat("eyeSize",        1.0f);
        avatar_.eyeSpacing     = readFloat("eyeSpacing",     1.0f);
        avatar_.eyeHeight      = readFloat("eyeHeight",      0.5f);
        avatar_.earSize        = readFloat("earSize",        1.0f);
        avatar_.earOffset      = readFloat("earOffset",      1.0f);
        avatar_.headSize       = readFloat("headSize",       1.0f);
        avatar_.pupilSize      = readFloat("pupilSize",      0.35f);
        avatar_.earRotation    = readFloat("earRotation",    0.0f);
        avatar_.googlyGravity  = readFloat("googlyGravity",  0.0f);
        avatar_.googlySpring   = readFloat("googlySpring",   1.0f);
        auto readInt = [&](const char* key, int fallback) -> int {
            auto v = av->getProperty(key);
            return v.isVoid() ? fallback : static_cast<int>(v);
        };
        avatar_.eyeType = readInt("eyeType", xyzpan::kEyeNone);
        // Migrate legacy format: eyesEnabled bool + old 0-based eyeType → new enum
        auto eyesVal = av->getProperty("eyesEnabled");
        if (!eyesVal.isVoid()) {
            if (!static_cast<bool>(eyesVal))
                avatar_.eyeType = xyzpan::kEyeNone;
            else
                avatar_.eyeType += 1;  // shift old 0-based (Normal=0) to new (Normal=1)
        }
        avatar_.earType = readInt("earType", xyzpan::kEarDefault);
        avatar_.hatSize    = readFloat("hatSize", 1.0f);
        avatar_.hatType = readInt("hatType", xyzpan::kHatNone);

        avatar_.noseSize   = readFloat("noseSize", 1.0f);
        avatar_.noseType   = readInt("noseType", xyzpan::kNoseCone);

        avatar_.headColor  = static_cast<uint32_t>(readInt("headColor",  0));
        // Read noseColor, falling back to old arrowColor key for backward compat
        auto noseVal = readInt("noseColor", 0);
        if (noseVal == 0) noseVal = readInt("arrowColor", 0);
        avatar_.noseColor  = static_cast<uint32_t>(noseVal);
        avatar_.hatColor   = static_cast<uint32_t>(readInt("hatColor",   0));
        avatar_.eyeColor   = static_cast<uint32_t>(readInt("eyeColor",   0));
    }
}

void UserPreferences::save() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty("themeIndex", themeIndex_);

    auto* av = new juce::DynamicObject();
    av->setProperty("headElongation", static_cast<double>(avatar_.headElongation));
    av->setProperty("eyeSize",        static_cast<double>(avatar_.eyeSize));
    av->setProperty("eyeSpacing",     static_cast<double>(avatar_.eyeSpacing));
    av->setProperty("eyeHeight",      static_cast<double>(avatar_.eyeHeight));
    av->setProperty("earSize",        static_cast<double>(avatar_.earSize));
    av->setProperty("earOffset",      static_cast<double>(avatar_.earOffset));
    av->setProperty("headSize",       static_cast<double>(avatar_.headSize));
    av->setProperty("pupilSize",     static_cast<double>(avatar_.pupilSize));
    av->setProperty("earRotation",   static_cast<double>(avatar_.earRotation));
    av->setProperty("googlyGravity", static_cast<double>(avatar_.googlyGravity));
    av->setProperty("googlySpring",  static_cast<double>(avatar_.googlySpring));
    av->setProperty("eyeType",        avatar_.eyeType);
    av->setProperty("earType",        avatar_.earType);
    av->setProperty("hatSize",       static_cast<double>(avatar_.hatSize));
    av->setProperty("hatType",        avatar_.hatType);
    av->setProperty("noseSize",      static_cast<double>(avatar_.noseSize));
    av->setProperty("noseType",       avatar_.noseType);
    av->setProperty("headColor",      static_cast<int>(avatar_.headColor));
    av->setProperty("noseColor",      static_cast<int>(avatar_.noseColor));
    av->setProperty("hatColor",      static_cast<int>(avatar_.hatColor));
    av->setProperty("eyeColor",      static_cast<int>(avatar_.eyeColor));
    root->setProperty("avatar", juce::var(av));

    auto file = getPrefsFile();
    file.replaceWithText(juce::JSON::toString(juce::var(root)));
}

} // namespace xyzpan
