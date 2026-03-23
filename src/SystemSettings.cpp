#include "SystemSettings.h"

#include "Audio.h"
#include "Font.h"
#include "GameUtil.h"
#include "filefunc.h"

#include <format>
#include <glaze/json.hpp>

namespace
{

constexpr auto kSystemSettingsWriteOptions = glz::opts{.prettify = true};
constexpr auto kSystemSettingsReadOptions = glz::opts{.error_on_unknown_keys = false};
bool readSettingsJson(const std::string& path, SystemSettingsData& data)
{
    if (!filefunc::fileExist(path))
    {
        return false;
    }

    auto payload = filefunc::readFileToString(path);
    if (payload.empty())
    {
        return false;
    }

    return !glz::read<kSystemSettingsReadOptions>(data, payload);
}

bool writeSettingsJson(const std::string& path, const SystemSettingsData& data)
{
    std::string payload;
    if (const auto result = glz::write<kSystemSettingsWriteOptions>(data, payload); result)
    {
        return false;
    }

    filefunc::makePath(filefunc::getParentPath(path));
    return filefunc::writeStringToFile(payload, path) > 0;
}

}

std::string SystemSettings::filename()
{
#ifdef __EMSCRIPTEN__
    return "/persist/setting.json";
#else
    return std::format("{}save/setting.json", GameUtil::PATH());
#endif
}

void SystemSettings::clamp(SystemSettingsData& data)
{
    data.musicVolume = GameUtil::limit(data.musicVolume, 0, 100);
    data.soundVolume = GameUtil::limit(data.soundVolume, 0, 100);
    data.battleSpeed = GameUtil::limit(data.battleSpeed, 0, 2);
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

    readSettingsJson(path, data_);

    clamp(data_);
    applyRuntime();
}

void SystemSettings::save() const
{
    std::string path = filename();
    filefunc::makePath(filefunc::getParentPath(path));

    if (!writeSettingsJson(path, data_))
    {
        return;
    }
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