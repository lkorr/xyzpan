#pragma once
#include "ColorTheme.h"
#include "AvatarParams.h"
#include "SceneParams.h"
#include <functional>

namespace xyzpan {

// Persists global user preferences (theme + avatar) to disk as JSON.
// File location: <userAppDataDir>/XYZPan/preferences.json
class UserPreferences {
public:
    UserPreferences();

    int themeIndex() const { return themeIndex_; }
    const ColorTheme& activeTheme() const { return getThemeEntry(themeIndex_).theme; }
    const AvatarParams& avatarParams() const { return avatar_; }
    const SceneParams& sceneParams() const { return scene_; }

    void setThemeIndex(int index);
    void setAvatarParams(const AvatarParams& params);
    void setSceneParams(const SceneParams& params);

    // Reload from disk — used when another instance has updated preferences.
    void reload();

    // Fired after a successful save() (message thread). Consumers use this to
    // broadcast a cross-instance invalidation signal.
    std::function<void()> onSaved;

private:
    void load();
    void save() const;

    int          themeIndex_ = 0;
    AvatarParams avatar_;
    SceneParams  scene_;
};

} // namespace xyzpan
