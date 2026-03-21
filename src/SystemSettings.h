#pragma once

#include <string>

class SQLite3Wrapper;

struct SystemSettingsData
{
    bool positionSwapEnabled = false;
    int musicVolume = 25;
    int soundVolume = 15;
    bool manualCamera = false;
    int battleSpeed = 1;
    bool simplifiedChinese = true;
    bool showBattleLog = true;
};

class SystemSettings
{
public:
    static SystemSettings* getInstance()
    {
        static SystemSettings settings;
        return &settings;
    }

    const SystemSettingsData& data() const { return data_; }
    SystemSettingsData snapshot() const { return data_; }

    void load();
    void save() const;
    void update(const SystemSettingsData& data, bool persist = true);

    bool positionSwapEnabled() const { return data_.positionSwapEnabled; }
    void setPositionSwapEnabled(bool value);

    static std::string filename();

private:
    SystemSettings() = default;

    static void clamp(SystemSettingsData& data);
    static void save(SQLite3Wrapper& db, const SystemSettingsData& data);
    static void load(SQLite3Wrapper& db, SystemSettingsData& data);
    void applyRuntime() const;

    SystemSettingsData data_;
};