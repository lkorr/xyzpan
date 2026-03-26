#pragma once
#include "ColorTheme.h"
#include "AvatarParams.h"

namespace xyzpan {

// Persists global user preferences (theme + avatar) to disk as JSON.
// File location: <userAppDataDir>/XYZPan/preferences.json
class UserPreferences {
public:
    UserPreferences();

    int themeIndex() const { return themeIndex_; }
    const ColorTheme& activeTheme() const { return getThemeEntry(themeIndex_).theme; }
    const AvatarParams& avatarParams() const { return avatar_; }

    void setThemeIndex(int index);
    void setAvatarParams(const AvatarParams& params);

private:
    void load();
    void save() const;

    int          themeIndex_ = 0;
    AvatarParams avatar_;
};

} // namespace xyzpan
