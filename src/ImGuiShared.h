#pragma once

#include "Font.h"

#include <imgui.h>

#include <string>
#include <string_view>

namespace KysImGui
{
inline ImVec4 colorU8(int r, int g, int b, int a = 255)
{
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

inline std::string localizeImGuiText(std::string_view text)
{
    return Font::getInstance()->localize(std::string(text));
}

inline std::string localizeImGuiLabel(std::string_view label)
{
    const size_t idPos = label.find("##");
    if (idPos == std::string_view::npos)
    {
        return localizeImGuiText(label);
    }

    std::string visible(label.substr(0, idPos));
    std::string id(label.substr(idPos));
    if (visible.empty())
    {
        return id;
    }
    return Font::getInstance()->localize(visible) + id;
}

inline void textLocalized(std::string_view text)
{
    auto localized = localizeImGuiText(text);
    ImGui::TextUnformatted(localized.c_str());
}

inline bool checkboxLocalized(std::string_view label, bool* value)
{
    auto localized = localizeImGuiLabel(label);
    return ImGui::Checkbox(localized.c_str(), value);
}

inline bool radioButtonLocalized(std::string_view label, bool active)
{
    auto localized = localizeImGuiLabel(label);
    return ImGui::RadioButton(localized.c_str(), active);
}

inline bool buttonLocalized(std::string_view label, const ImVec2& size = ImVec2(0.0f, 0.0f))
{
    auto localized = localizeImGuiLabel(label);
    return ImGui::Button(localized.c_str(), size);
}

inline bool selectableLocalized(std::string_view label, bool selected = false)
{
    auto localized = localizeImGuiText(label);
    return ImGui::Selectable(localized.c_str(), selected);
}

inline bool beginComboLocalized(const char* label, std::string_view preview)
{
    auto localized = localizeImGuiText(preview);
    return ImGui::BeginCombo(label, localized.c_str());
}
}  // namespace KysImGui
