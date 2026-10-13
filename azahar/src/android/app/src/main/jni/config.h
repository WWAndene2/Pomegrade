// Copyright 2014-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/settings.h"

class INIReader;

class Config {
private:
    std::unique_ptr<INIReader> android_config;
    std::string android_config_loc;
    // Pomegrade: the game's own settings (GameSettings/<program ID>.ini), read before config.ini
    std::unique_ptr<INIReader> game_config;

    bool LoadINI(const std::string& default_contents = "", bool retry = true);
    void ReadValues();

public:
    /// Reads config.ini; with a program ID, the values that game's settings file sets replace it
    explicit Config(u64 program_id = 0);
    ~Config();

    void Reload();

private:
    /// The reader that holds the key: the game's settings when they set it, config.ini otherwise
    const INIReader& ReaderFor(const std::string& group, const std::string& key) const;

    /**
     * Applies a value read from the android_config to a Setting.
     *
     * @param group The name of the INI group
     * @param setting The yuzu setting to modify
     */

    template <typename Type, bool ranged>
    void ReadSetting(const std::string& group, Settings::Setting<Type, ranged>& setting);
};
