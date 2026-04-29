#include "SessionConfig.h"
#include "xyzpan/obfuscate.h"

#define OBF(s) juce::String(static_cast<const char*>(AY_OBFUSCATE(s)))

// ---------------------------------------------------------------------------
struct ConfigSyncThread : public juce::Thread {
    juce::String token;
    SessionConfig::SyncCallback callback;
    SessionConfig* owner;

    ConfigSyncThread(const juce::String& t, SessionConfig::SyncCallback cb, SessionConfig* o)
        : juce::Thread("CfgSync"), token(t), callback(std::move(cb)), owner(o) {}

    void run() override
    {
        juce::URL url(OBF("https://api.gumroad.com/v2/licenses/verify"));
        auto body = juce::String(OBF("product_id="))
                  + juce::URL::addEscapeChars(OBF("5Nc-KzHFqkw4nG14StZThA=="), true)
                  + OBF("&license_key=") + juce::URL::addEscapeChars(token, true)
                  + OBF("&increment_uses_count=true");

        auto stream = url.withPOSTData(body)
                         .createInputStream(juce::URL::InputStreamOptions(
                             juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs(10000)
                             .withHttpRequestCmd("POST"));

        if (threadShouldExit()) return;

        if (!stream)
        {
            juce::MessageManager::callAsync([cb = callback] {
                cb(false, OBF("Could not reach server. Check your internet connection."));
            });
            return;
        }

        auto response = stream->readEntireStreamAsString();
        if (threadShouldExit()) return;

        auto json = juce::JSON::parse(response);
        bool ok = json.getProperty(OBF("success"), false);

        if (ok)
        {
            int n = static_cast<int>(json.getProperty(OBF("uses"), 0));
            // TODO: Set this to 2 or 3 before launching the VST for sale!
            if (n > 999)
            {
                juce::MessageManager::callAsync([cb = callback] {
                    cb(false, OBF("This key has reached its activation limit."));
                });
                return;
            }

            auto info = json.getProperty(OBF("purchase"), juce::var());
            auto tag = info.getProperty(OBF("email"), "").toString();

            owner->persist(token, tag);
            owner->ready_.store(true, std::memory_order_release);

            juce::MessageManager::callAsync([cb = callback] {
                cb(true, OBF("Activated successfully!"));
            });
        }
        else
        {
            auto msg = json.getProperty(OBF("message"),
                                        OBF("Invalid key.")).toString();
            juce::MessageManager::callAsync([cb = callback, msg] {
                cb(false, msg);
            });
        }
    }
};

// ---------------------------------------------------------------------------
SessionConfig::SessionConfig()
{
    ready_.store(verify(), std::memory_order_release);
}

SessionConfig::~SessionConfig()
{
    if (syncThread_)
    {
        syncThread_->stopThread(5000);
        syncThread_.reset();
    }
}

bool SessionConfig::hasCached() const
{
    return ready_.load(std::memory_order_acquire);
}

juce::File SessionConfig::configPath()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("XYZPan")
               .getChildFile("license.dat");
}

juce::String SessionConfig::envSalt()
{
    auto raw = juce::SystemStats::getComputerName()
             + "|" + juce::SystemStats::getLogonName();
    return digest(raw);
}

juce::String SessionConfig::digest(const juce::String& input)
{
    return juce::String::toHexString(input.hashCode64());
}

bool SessionConfig::verify() const
{
    auto file = configPath();
    if (!file.existsAsFile())
        return false;

    auto json = juce::JSON::parse(file.loadFileAsString());
    if (!json.isObject())
        return false;

    auto h = json.getProperty(OBF("h"), "").toString();
    auto k = json.getProperty(OBF("k"), "").toString();

    if (h.isEmpty() || k.isEmpty())
        return false;

    auto expected = digest(k + envSalt());
    return h == expected;
}

void SessionConfig::persist(const juce::String& token, const juce::String& tag)
{
    auto obj = new juce::DynamicObject();
    obj->setProperty(OBF("k"), token);
    obj->setProperty(OBF("h"), digest(token + envSalt()));
    obj->setProperty(OBF("t"), tag);
    obj->setProperty(OBF("ts"), juce::Time::getCurrentTime().toISO8601(true));

    auto file = configPath();
    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::JSON::toString(juce::var(obj)));
}

void SessionConfig::syncAsync(const juce::String& token, SyncCallback onResult)
{
    if (syncThread_)
    {
        syncThread_->stopThread(2000);
        syncThread_.reset();
    }

    syncThread_ = std::make_unique<ConfigSyncThread>(
        token.trim(), std::move(onResult), this);
    syncThread_->startThread();
}
