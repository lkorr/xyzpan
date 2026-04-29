#include <catch2/catch_test_macros.hpp>
#include "SessionConfig.h"
#include "PluginProcessor.h"

// Helper: get the config file path (mirrors SessionConfig::configPath)
static juce::File getTestConfigFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("XYZPan")
               .getChildFile("license.dat");
}

struct ConfigGuard {
    juce::File configFile = getTestConfigFile();
    juce::File backup;
    bool hadExisting = false;

    ConfigGuard() {
        if (configFile.existsAsFile()) {
            backup = configFile.getSiblingFile("session.dat.test_backup");
            configFile.copyFileTo(backup);
            hadExisting = true;
        }
        configFile.deleteFile();
    }

    ~ConfigGuard() {
        configFile.deleteFile();
        if (hadExisting && backup.existsAsFile()) {
            backup.moveFileTo(configFile);
        }
    }
};

// Helper: compute salt + hash matching SessionConfig internals
static juce::String testSalt()
{
    return juce::String::toHexString(
        (juce::SystemStats::getComputerName() + "|" + juce::SystemStats::getLogonName()).hashCode64());
}

static void writeValidConfig(const juce::File& file)
{
    juce::String key = "TESTKEY1-TESTKEY2-TESTKEY3-TESTKEY4";
    auto hash = juce::String::toHexString((key + testSalt()).hashCode64());

    auto obj = new juce::DynamicObject();
    obj->setProperty("k", key);
    obj->setProperty("h", hash);
    obj->setProperty("t", "test@example.com");
    obj->setProperty("ts", juce::Time::getCurrentTime().toISO8601(true));

    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::JSON::toString(juce::var(obj)));
}

// ---- Validation tests ----

TEST_CASE("No config file -> not ready", "[session]") {
    ConfigGuard guard;
    SessionConfig cfg;
    REQUIRE_FALSE(cfg.isReady());
    REQUIRE_FALSE(cfg.hasCached());
}

TEST_CASE("Valid config file -> ready", "[session]") {
    ConfigGuard guard;
    writeValidConfig(getTestConfigFile());

    SessionConfig cfg;
    REQUIRE(cfg.isReady());
    REQUIRE(cfg.hasCached());
}

TEST_CASE("Corrupted config file -> not ready", "[session]") {
    ConfigGuard guard;
    auto file = getTestConfigFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText("not valid json {{{");

    SessionConfig cfg;
    REQUIRE_FALSE(cfg.isReady());
}

TEST_CASE("Config with wrong hash -> not ready", "[session]") {
    ConfigGuard guard;
    auto obj = new juce::DynamicObject();
    obj->setProperty("k", "TESTKEY1-TESTKEY2-TESTKEY3-TESTKEY4");
    obj->setProperty("h", "deadbeef12345678");
    obj->setProperty("t", "test@example.com");

    auto file = getTestConfigFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::JSON::toString(juce::var(obj)));

    SessionConfig cfg;
    REQUIRE_FALSE(cfg.isReady());
}

TEST_CASE("Config with empty key -> not ready", "[session]") {
    ConfigGuard guard;
    auto obj = new juce::DynamicObject();
    obj->setProperty("k", "");
    obj->setProperty("h", "");

    auto file = getTestConfigFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::JSON::toString(juce::var(obj)));

    SessionConfig cfg;
    REQUIRE_FALSE(cfg.isReady());
}

TEST_CASE("Config missing fields -> not ready", "[session]") {
    ConfigGuard guard;
    auto obj = new juce::DynamicObject();
    obj->setProperty("k", "TESTKEY1-TESTKEY2-TESTKEY3-TESTKEY4");

    auto file = getTestConfigFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::JSON::toString(juce::var(obj)));

    SessionConfig cfg;
    REQUIRE_FALSE(cfg.isReady());
}

// ---- Audio degradation tests ----

TEST_CASE("Unconfigured processor inserts silence gaps", "[session][demo]") {
    ConfigGuard guard;

    XYZPanProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    bool foundSilence = false;
    bool foundAudio = false;

    for (int block = 0; block < 750; ++block) {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample(ch, i, 0.5f);

        proc.processBlock(buffer, midi);

        for (int i = 0; i < 512; ++i) {
            float L = buffer.getSample(0, i);
            float R = buffer.getSample(1, i);
            if (L == 0.0f && R == 0.0f) foundSilence = true;
            if (L != 0.0f || R != 0.0f) foundAudio = true;
        }

        if (foundSilence && foundAudio) break;
    }

    REQUIRE(foundSilence);
    REQUIRE(foundAudio);
}

TEST_CASE("Configured processor has no silence gaps", "[session]") {
    ConfigGuard guard;
    writeValidConfig(getTestConfigFile());

    XYZPanProcessor proc;
    REQUIRE(proc.sessionCfg_.isReady());

    proc.prepareToPlay(44100.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    int silentSamples = 0;
    int totalSamples = 0;

    for (int block = 0; block < 860; ++block) {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample(ch, i, 0.5f);

        proc.processBlock(buffer, midi);

        for (int i = 0; i < 512; ++i) {
            float L = buffer.getSample(0, i);
            float R = buffer.getSample(1, i);
            if (L == 0.0f && R == 0.0f) silentSamples++;
            totalSamples++;
        }
    }

    float silenceRatio = static_cast<float>(silentSamples) / static_cast<float>(totalSamples);
    REQUIRE(silenceRatio < 0.001f);
}
