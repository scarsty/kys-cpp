#include "SystemSettings.h"

#include "Audio.h"
#include "Font.h"
#include "GameUtil.h"
#include "SQLite3Wrapper.h"
#include "filefunc.h"

#include <format>
#include <glaze/json.hpp>

namespace
{

constexpr int kSystemSettingsSchemaVersion = 1;
constexpr auto kSystemSettingsWriteOptions = glz::opts{.prettify = true};
constexpr auto kSystemSettingsReadOptions = glz::opts{.error_on_unknown_keys = false};

}

std::string SystemSettings::filename()
{
#ifdef __EMSCRIPTEN__
    return "/persist/setting.db";
#else
    return std::format("{}save/setting.db", GameUtil::PATH());
#endif
}

void SystemSettings::clamp(SystemSettingsData& data)
{
    data.musicVolume = GameUtil::limit(data.musicVolume, 0, 100);
    data.soundVolume = GameUtil::limit(data.soundVolume, 0, 100);
    data.battleSpeed = GameUtil::limit(data.battleSpeed, 0, 2);
}

void SystemSettings::save(SQLite3Wrapper& db, const SystemSettingsData& data)
{
    std::string payload;
    if (const auto writeResult = glz::write<kSystemSettingsWriteOptions>(data, payload); writeResult)
    {
        return;
    }

    db.execute("drop table if exists system_settings_store");
    db.execute("create table system_settings_store(schema_version int not null, payload text not null)");

    auto stmt = db.prepare("insert into system_settings_store(schema_version, payload) values(?, ?)");
    if (!stmt.isValid())
    {
        return;
    }
    if (!stmt.bind(1, kSystemSettingsSchemaVersion) || !stmt.bind(2, payload.c_str()))
    {
        return;
    }
    stmt.step();
}

void SystemSettings::load(SQLite3Wrapper& db, SystemSettingsData& data)
{
    data = {};

    auto stmt = db.query("select schema_version, payload from system_settings_store limit 1");
    if (!stmt.isValid() || !stmt.step())
    {
        return;
    }

    if (stmt.getColumnInt(0) != kSystemSettingsSchemaVersion)
    {
        return;
    }

    const char* payload = stmt.getColumnText(1);
    if (!payload)
    {
        return;
    }

    if (const auto readResult = glz::read<kSystemSettingsReadOptions>(data, std::string(payload)); readResult)
    {
        data = {};
    }
}

void SystemSettings::applyRuntime() const
{
    auto* audio = Audio::getInstance();
    audio->setVolume(data_.musicVolume);
    audio->setVolumeWav(data_.soundVolume);

    auto* font = Font::getInstance();
    if (font->isSimplified() != data_.simplifiedChinese)
    {
        font->setSimplified(data_.simplifiedChinese ? 1 : 0);
        font->clearBuffer();
    }
    else
    {
        font->setSimplified(data_.simplifiedChinese ? 1 : 0);
    }
}

void SystemSettings::load()
{
    data_ = {};

    std::string path = filename();
    filefunc::makePath(filefunc::getParentPath(path));

    SQLite3Wrapper db;
    if (db.open(path))
    {
        load(db, data_);
    }

    clamp(data_);
    applyRuntime();
}

void SystemSettings::save() const
{
    std::string path = filename();
    filefunc::makePath(filefunc::getParentPath(path));

    SQLite3Wrapper db;
    if (!db.open(path))
    {
        return;
    }

    save(db, data_);
}

void SystemSettings::update(const SystemSettingsData& data, bool persist)
{
    data_ = data;
    clamp(data_);
    applyRuntime();
    if (persist)
    {
        save();
    }
}

void SystemSettings::setPositionSwapEnabled(bool value)
{
    if (data_.positionSwapEnabled == value)
    {
        return;
    }

    auto updated = data_;
    updated.positionSwapEnabled = value;
    update(updated);
}