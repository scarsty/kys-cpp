#include "ImGuiLayer.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "Audio.h"
#include "Font.h"
#include "SDL3/SDL.h"
#include "GameUtil.h"
#include <algorithm>
#include <string>
#include <string_view>

namespace
{
ImVec4 colorU8(int r, int g, int b, int a = 255)
{
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

float clampf(float v, float lo, float hi)
{
    return (std::max)(lo, (std::min)(v, hi));
}

bool isGuardedOverlayEventType(Uint32 type)
{
    switch (type)
    {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_TEXT_EDITING:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        return true;
    default:
        return false;
    }
}

std::string localizeImGuiText(std::string_view text)
{
    return Font::getInstance()->localize(std::string(text));
}

std::string localizeImGuiLabel(std::string_view label)
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

void textLocalized(std::string_view text)
{
    auto localized = localizeImGuiText(text);
    ImGui::TextUnformatted(localized.c_str());
}

bool checkboxLocalized(std::string_view label, bool* value)
{
    auto localized = localizeImGuiLabel(label);
    return ImGui::Checkbox(localized.c_str(), value);
}

bool radioButtonLocalized(std::string_view label, bool active)
{
    auto localized = localizeImGuiLabel(label);
    return ImGui::RadioButton(localized.c_str(), active);
}

bool buttonLocalized(std::string_view label, const ImVec2& size = ImVec2(0.0f, 0.0f))
{
    auto localized = localizeImGuiLabel(label);
    return ImGui::Button(localized.c_str(), size);
}

bool selectableLocalized(std::string_view label, bool selected = false)
{
    auto localized = localizeImGuiText(label);
    return ImGui::Selectable(localized.c_str(), selected);
}

bool beginComboLocalized(const char* label, std::string_view preview)
{
    auto localized = localizeImGuiText(preview);
    return ImGui::BeginCombo(label, localized.c_str());
}
}

void ImGuiLayer::init(SDL_Window* window, SDL_Renderer* renderer)
{
    if (initialized_ || window == nullptr || renderer == nullptr)
    {
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;
    std::string chinese_font = GameUtil::PATH() + "font/chinese.ttf";
    if (!io.Fonts->AddFontFromFileTTF(chinese_font.c_str(), 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull()))
    {
        io.Fonts->AddFontDefault();
    }

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    initialized_ = true;
}

void ImGuiLayer::shutdown()
{
    if (!initialized_)
    {
        return;
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    initialized_ = false;
    visible_ = false;
}

bool ImGuiLayer::processEvent(const SDL_Event& event)
{
    if (!initialized_)
    {
        return false;
    }

    if (((battle_log_.open && battle_log_input_guard_frames_ > 0)
            || (system_menu_.open && system_menu_input_guard_frames_ > 0))
        && isGuardedOverlayEventType(event.type))
    {
        return true;
    }

    ImGui_ImplSDL3_ProcessEvent(&event);

    if (event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_F1)
    {
        visible_ = !visible_;
        return true;
    }

    return wantsCaptureEvent(event);
}

void ImGuiLayer::render(SDL_Window* window, SDL_Renderer* renderer, int main_texture_w, int main_texture_h, const char* renderer_name)
{
    if (!initialized_ || (!visible_ && !battle_log_.open && !system_menu_.open))
    {
        return;
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    int window_w = 0;
    int window_h = 0;
    SDL_GetWindowSize(window, &window_w, &window_h);

    if (visible_)
    {
        ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420.0f, 180.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Debug Overlay", &visible_))
        {
            ImGui::Text("Press F1 to toggle this overlay.");
            ImGui::Separator();
            ImGui::Text("Renderer: %s", renderer_name ? renderer_name : "unknown");
            ImGui::Text("Window size: %d x %d", window_w, window_h);
            ImGui::Text("Main texture: %d x %d", main_texture_w, main_texture_h);
            ImGui::Text("Frame time: %.3f ms (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::Checkbox("Show ImGui metrics window", &show_metrics_window_);
        }
        ImGui::End();
    }

    renderBattleLogWindow();
    renderBattleSystemMenuWindow();

    if (show_metrics_window_)
    {
        ImGui::ShowMetricsWindow(&show_metrics_window_);
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

bool ImGuiLayer::wantsCaptureEvent(const SDL_Event& event) const
{
    if (!initialized_ || (!visible_ && !battle_log_.open && !system_menu_.open))
    {
        return false;
    }

    const ImGuiIO& io = ImGui::GetIO();
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
        return battle_log_.open || system_menu_.open || io.WantCaptureMouse;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_TEXT_EDITING:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        return battle_log_.open || system_menu_.open || io.WantCaptureKeyboard || io.WantTextInput;
    default:
        return false;
    }
}

void ImGuiLayer::showBattleLog(const BattleLogData& data)
{
    battle_log_ = data;
    battle_log_.open = true;
    battle_log_input_guard_frames_ = 10;
    battle_log_hover_guard_ = true;
    battle_log_child_flip_ ^= 1;
    battle_log_ally_filter_id_ = -1;
    battle_log_enemy_filter_id_ = -1;
}

void ImGuiLayer::hideBattleLog()
{
    battle_log_.open = false;
    battle_log_input_guard_frames_ = 0;
    battle_log_hover_guard_ = false;
}

bool ImGuiLayer::isBattleLogOpen() const
{
    return battle_log_.open;
}

void ImGuiLayer::showBattleSystemMenu(const BattleSystemMenuData& data)
{
    system_menu_ = data;
    system_menu_.musicVolume = GameUtil::limit(system_menu_.musicVolume, 0, 100);
    system_menu_.soundVolume = GameUtil::limit(system_menu_.soundVolume, 0, 100);
    system_menu_.battleSpeed = GameUtil::limit(system_menu_.battleSpeed, 0, 2);
    system_menu_.open = true;
    system_menu_input_guard_frames_ = 8;
    system_menu_hover_guard_ = true;
}

void ImGuiLayer::hideBattleSystemMenu()
{
    system_menu_.open = false;
    system_menu_input_guard_frames_ = 0;
    system_menu_hover_guard_ = false;
}

bool ImGuiLayer::isBattleSystemMenuOpen() const
{
    return system_menu_.open;
}

BattleSystemMenuData ImGuiLayer::getBattleSystemMenuData() const
{
    return system_menu_;
}

void ImGuiLayer::renderBattleLogWindow()
{
    if (!battle_log_.open)
    {
        return;
    }

    const ImVec4 panel_bg = colorU8(34, 38, 30, 164);
    const ImVec4 panel_border = colorU8(192, 172, 118, 190);
    const ImVec4 title_gold = colorU8(238, 221, 112);
    const ImVec4 text_main = colorU8(248, 245, 232);
    const ImVec4 text_muted = colorU8(202, 198, 180);
    const ImVec4 ally_color = colorU8(170, 232, 166);
    const ImVec4 enemy_color = colorU8(245, 165, 155);
    const ImVec4 neutral_line = colorU8(230, 224, 205);
    const ImVec4 system_line = colorU8(241, 220, 118);
    const ImVec4 skill_color = colorU8(125, 205, 245);
    const ImVec4 damage_color = colorU8(255, 196, 96);
    const ImVec4 chip_bg = colorU8(92, 96, 59, 150);
    const ImVec4 child_bg = colorU8(30, 34, 27, 88);
    const ImVec4 accent_line = colorU8(166, 152, 82, 90);
    const ImVec4 dim_bg = colorU8(8, 10, 8, 42);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 vp_pos = viewport ? viewport->Pos : ImVec2(0.0f, 0.0f);
    ImVec2 vp_size = viewport ? viewport->Size : ImVec2(1280.0f, 720.0f);
    const ImGuiIO& io = ImGui::GetIO();
    if (battle_log_hover_guard_ && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
    {
        battle_log_hover_guard_ = false;
    }
    float body_scale = clampf(vp_size.y / 740.0f, 1.24f, 2.10f);
    float title_scale = clampf(body_scale * 1.34f, 1.52f, 2.48f);
    float chip_scale = clampf(body_scale * 1.18f, 1.34f, 2.18f);
    float small_scale = clampf(body_scale * 1.04f, 1.16f, 1.78f);
    float filter_scale = clampf(body_scale * 1.14f, 1.34f, 2.20f);
    float scrollbar_size = clampf(vp_size.y / 18.0f, 26.0f, 42.0f);
    ImVec2 panel_size((std::max)(980.0f, vp_size.x * 0.90f), (std::max)(620.0f, vp_size.y * 0.86f));
    panel_size.x = (std::min)(panel_size.x, vp_size.x - 36.0f);
    panel_size.y = (std::min)(panel_size.y, vp_size.y - 30.0f);
    ImVec2 panel_pos(vp_pos.x + (vp_size.x - panel_size.x) * 0.5f, vp_pos.y + (vp_size.y - panel_size.y) * 0.5f);

    ImGui::GetForegroundDrawList()->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y), ImGui::ColorConvertFloat4ToU32(dim_bg));

    ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26.0f, 22.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, panel_bg);
    ImGui::PushStyleColor(ImGuiCol_Border, panel_border);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, child_bg);
    ImGui::PushStyleColor(ImGuiCol_Separator, accent_line);
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, colorU8(255, 255, 255, 10));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, colorU8(255, 255, 255, 18));

    if (ImGui::Begin("battle_log_window", &battle_log_.open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus))
    {
        bool allow_close = battle_log_input_guard_frames_ <= 0;
        bool suppress_hover = battle_log_hover_guard_ || !allow_close;
        if (suppress_hover)
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, chip_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, chip_bg);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_Header));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_Header));
        }
        auto matchesFilter = [&](const BattleLogLine& entry) {
            if (entry.sourceId < 0 && entry.targetId < 0)
            {
                return true;
            }

            auto matchesTeamFilter = [&](int filterId, int team) {
                if (filterId < 0)
                {
                    return true;
                }
                return (entry.sourceTeam == team && entry.sourceId == filterId)
                    || (entry.targetTeam == team && entry.targetId == filterId);
            };

            return matchesTeamFilter(battle_log_ally_filter_id_, 0)
                && matchesTeamFilter(battle_log_enemy_filter_id_, 1);
        };

        auto visibleEntryCount = [&]() {
            int count = 0;
            for (const auto& entry : battle_log_.entries)
            {
                if (matchesFilter(entry))
                {
                    ++count;
                }
            }
            return count;
        };

        ImGui::SetWindowFontScale(title_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        auto title = localizeImGuiText(battle_log_.title.empty() ? std::string_view("本次戰鬥日誌") : std::string_view(battle_log_.title));
        ImGui::TextUnformatted(title.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::SetWindowFontScale(body_scale);

        std::vector<std::pair<std::string, std::string>> chips = {
            {"戰鬥結果", battle_log_.resultText},
            {"總幀數", std::to_string(battle_log_.totalFrames)},
            {"顯示條數", std::format("{} / {}", visibleEntryCount(), (int)battle_log_.entries.size())}
        };
        if (battle_log_.omittedEntries > 0)
        {
            chips.push_back({"已省略", std::to_string(battle_log_.omittedEntries)});
        }

        auto drawChip = [&](const std::string& label, const std::string& value)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
            ImGui::SetWindowFontScale(small_scale);
            auto localizedLabel = localizeImGuiText(label);
            ImGui::TextUnformatted(localizedLabel.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::SetWindowFontScale(chip_scale);
            ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
            auto localizedValue = localizeImGuiText(value);
            ImGui::TextUnformatted(localizedValue.c_str());
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(body_scale);
        };

        ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, colorU8(188, 170, 110, 120));
        ImGui::PushStyleColor(ImGuiCol_TableBorderLight, colorU8(188, 170, 110, 70));
        if (ImGui::BeginTable("battle_log_chips", (int)chips.size(), ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame))
        {
            for (int i = 0; i < (int)chips.size(); ++i)
            {
                ImGui::TableNextColumn();
                drawChip(chips[i].first, chips[i].second);
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleColor(2);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto drawFilterCombo = [&](const char* label, const char* allLabel, const std::vector<BattleLogRoleRow>& rows, int& selectedId)
        {
            std::string preview = allLabel;
            for (const auto& row : rows)
            {
                if (row.id == selectedId)
                {
                    preview = row.name;
                    break;
                }
            }

            ImGui::SetWindowFontScale(filter_scale);
            if (beginComboLocalized(label, preview))
            {
                ImGui::SetWindowFontScale(filter_scale);
                bool allSelected = selectedId < 0;
                if (selectableLocalized(allLabel, allSelected))
                {
                    selectedId = -1;
                }
                if (allSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
                for (const auto& row : rows)
                {
                    bool selected = selectedId == row.id;
                    ImGui::PushID(row.id);
                    if (selectableLocalized(row.name, selected))
                    {
                        selectedId = row.id;
                    }
                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::SetWindowFontScale(body_scale);
        };

        ImGui::SetWindowFontScale(filter_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
        textLocalized("我方篩選");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        drawFilterCombo("##ally_filter", "全部我方", battle_log_.allies, battle_log_ally_filter_id_);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
        textLocalized("敵方篩選");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        drawFilterCombo("##enemy_filter", "全部敵方", battle_log_.enemies, battle_log_enemy_filter_id_);
        ImGui::SameLine();
        if (buttonLocalized("重設篩選") && allow_close)
        {
            battle_log_ally_filter_id_ = -1;
            battle_log_enemy_filter_id_ = -1;
        }
        ImGui::SetWindowFontScale(body_scale);

        ImGui::Spacing();
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, scrollbar_size);
        const char* child_id = battle_log_child_flip_ ? "battle_log_entries_b" : "battle_log_entries_a";
        ImGui::BeginChild(child_id, ImVec2(0.0f, ImGui::GetContentRegionAvail().y - 60.0f), true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
        ImGui::SetWindowFontScale(body_scale);
        auto colorForField = [&](BattleLogFieldTone tone, BattleLogTone line_tone) -> ImVec4
        {
            switch (tone)
            {
            case BattleLogFieldTone::AllyName:
                return ally_color;
            case BattleLogFieldTone::EnemyName:
                return enemy_color;
            case BattleLogFieldTone::SkillName:
                return skill_color;
            case BattleLogFieldTone::DamageValue:
                return damage_color;
            case BattleLogFieldTone::SystemAccent:
                return system_line;
            default:
                break;
            }
            if (line_tone == BattleLogTone::Ally) return neutral_line;
            if (line_tone == BattleLogTone::Enemy) return neutral_line;
            if (line_tone == BattleLogTone::System) return system_line;
            return neutral_line;
        };
        bool drewAnyEntry = false;
        for (const auto& entry : battle_log_.entries)
        {
            if (!matchesFilter(entry))
            {
                continue;
            }

            drewAnyEntry = true;
            ImVec4 line_color = neutral_line;
            if (entry.tone == BattleLogTone::Ally) line_color = ally_color;
            if (entry.tone == BattleLogTone::Enemy) line_color = enemy_color;
            if (entry.tone == BattleLogTone::System) line_color = system_line;
            ImGui::Bullet();
            if (!entry.segments.empty())
            {
                ImGui::SameLine();
                for (int i = 0; i < (int)entry.segments.size(); ++i)
                {
                    const auto& seg = entry.segments[i];
                    ImGui::PushStyleColor(ImGuiCol_Text, colorForField(seg.tone, entry.tone));
                    auto localizedSegment = localizeImGuiText(seg.text);
                    ImGui::TextUnformatted(localizedSegment.c_str());
                    ImGui::PopStyleColor();
                    if (i + 1 < (int)entry.segments.size())
                    {
                        ImGui::SameLine(0.0f, 0.0f);
                    }
                }
            }
            else
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, line_color);
                auto localizedEntry = localizeImGuiText(entry.text);
                ImGui::TextWrapped("%s", localizedEntry.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::Spacing();
        }

        if (!drewAnyEntry)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
            textLocalized("當前篩選下沒有符合的記錄。");
            ImGui::PopStyleColor();
        }

        if (allow_close
            && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
            && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)
            && !ImGui::IsAnyItemActive())
        {
            ImGui::SetScrollY(ImGui::GetScrollY() - ImGui::GetIO().MouseDelta.y);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetWindowFontScale(small_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
        textLocalized("點擊「繼續」關閉日誌");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        float button_w = 225.0f;
        float button_x = ImGui::GetWindowContentRegionMax().x - button_w;
        float button_h = ImGui::GetFrameHeight() * 1.5f;
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), button_x));
        ImGui::PushStyleColor(ImGuiCol_Button, chip_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, allow_close ? colorU8(67, 78, 39, 230) : chip_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, allow_close ? colorU8(88, 98, 50, 230) : chip_bg);
        ImGui::SetWindowFontScale(clampf(body_scale * 1.14f, 1.28f, 2.35f));
        if (buttonLocalized("繼續", ImVec2(button_w, button_h)) && allow_close)
        {
            battle_log_.open = false;
        }
        ImGui::PopStyleColor(3);
        ImGui::SetWindowFontScale(1.0f);
        if (suppress_hover)
        {
            ImGui::PopStyleColor(6);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);

    if (battle_log_input_guard_frames_ > 0)
    {
        battle_log_input_guard_frames_--;
    }
}

void ImGuiLayer::renderBattleSystemMenuWindow()
{
    if (!system_menu_.open)
    {
        return;
    }

    system_menu_.musicVolume = GameUtil::limit(system_menu_.musicVolume, 0, 100);
    system_menu_.soundVolume = GameUtil::limit(system_menu_.soundVolume, 0, 100);
    system_menu_.battleSpeed = GameUtil::limit(system_menu_.battleSpeed, 0, 2);

    const ImVec4 panel_bg = colorU8(34, 38, 30, 164);
    const ImVec4 panel_border = colorU8(192, 172, 118, 190);
    const ImVec4 title_gold = colorU8(238, 221, 112);
    const ImVec4 accent_line = colorU8(166, 152, 82, 90);
    const ImVec4 chip_bg = colorU8(92, 96, 59, 150);
    const ImVec4 child_bg = colorU8(30, 34, 27, 88);
    const ImVec4 dim_bg = colorU8(8, 10, 8, 42);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 vp_pos = viewport ? viewport->Pos : ImVec2(0.0f, 0.0f);
    ImVec2 vp_size = viewport ? viewport->Size : ImVec2(1280.0f, 720.0f);
    const ImGuiIO& io = ImGui::GetIO();
    if (system_menu_hover_guard_ && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
    {
        system_menu_hover_guard_ = false;
    }

    float ui_scale = clampf((std::min)(vp_size.x / 1280.0f, vp_size.y / 720.0f), 1.0f, 2.15f);
    float body_scale = clampf(ui_scale * 1.08f, 1.10f, 2.05f);
    float title_scale = clampf(body_scale * 1.30f, 1.40f, 2.66f);
    float section_scale = clampf(body_scale * 1.10f, 1.18f, 2.18f);
    float label_scale = clampf(body_scale * 0.98f, 1.02f, 1.70f);
    float small_scale = clampf(body_scale * 0.92f, 0.98f, 1.48f);
    float window_rounding = clampf(14.0f * ui_scale, 14.0f, 28.0f);
    float window_padding_x = clampf(20.0f * ui_scale, 20.0f, 44.0f);
    float window_padding_y = clampf(16.0f * ui_scale, 16.0f, 34.0f);
    float item_spacing_x = clampf(10.0f * ui_scale, 10.0f, 24.0f);
    float item_spacing_y = clampf(8.0f * ui_scale, 8.0f, 20.0f);
    float frame_padding_x = clampf(8.0f * ui_scale, 8.0f, 18.0f);
    float frame_padding_y = clampf(6.0f * ui_scale, 6.0f, 14.0f);
    float cell_padding_x = clampf(6.0f * ui_scale, 6.0f, 16.0f);
    float cell_padding_y = clampf(4.0f * ui_scale, 4.0f, 10.0f);
    float child_rounding = clampf(10.0f * ui_scale, 10.0f, 22.0f);
    float frame_rounding = clampf(4.0f * ui_scale, 4.0f, 10.0f);
    float grab_min_size = clampf(12.0f * ui_scale, 12.0f, 24.0f);
    ImVec2 panel_size((std::max)(980.0f, vp_size.x * 0.80f), (std::max)(640.0f, vp_size.y * 0.90f));
    panel_size.x = (std::min)(panel_size.x, vp_size.x - 20.0f);
    panel_size.y = (std::min)(panel_size.y, vp_size.y - 12.0f);
    ImVec2 panel_pos(vp_pos.x + (vp_size.x - panel_size.x) * 0.5f, vp_pos.y + (vp_size.y - panel_size.y) * 0.5f);
    bool suppress_hover = system_menu_hover_guard_ || system_menu_input_guard_frames_ > 0;

    if (system_menu_input_guard_frames_ <= 0 && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        system_menu_.open = false;
    }

    ImGui::GetForegroundDrawList()->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y), ImGui::ColorConvertFloat4ToU32(dim_bg));

    ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, window_rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(window_padding_x, window_padding_y));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(item_spacing_x, item_spacing_y));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(frame_padding_x, frame_padding_y));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(cell_padding_x, cell_padding_y));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, child_rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, frame_rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, grab_min_size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, panel_bg);
    ImGui::PushStyleColor(ImGuiCol_Border, panel_border);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, child_bg);
    ImGui::PushStyleColor(ImGuiCol_Separator, accent_line);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, colorU8(49, 55, 43, 200));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, colorU8(60, 67, 52, 220));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, colorU8(70, 77, 58, 230));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, title_gold);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, title_gold);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, colorU8(255, 236, 140));
    auto drawSectionTitle = [&](const char* title)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        ImGui::SetWindowFontScale(section_scale);
        auto localizedTitle = localizeImGuiText(title);
        ImGui::TextUnformatted(localizedTitle.c_str());
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(body_scale);
    };
    auto drawCheckboxRow = [&](const char* label, const char* id, bool* value)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SetWindowFontScale(label_scale);
        textLocalized(label);
        ImGui::TableNextColumn();
        ImGui::SetWindowFontScale(body_scale);
        float checkboxX = ImGui::GetCursorPosX() + (std::max)(0.0f, ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight());
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), checkboxX));
        ImGui::Checkbox(id, value);
    };
    auto drawRadioGroupRow = [&](const char* label, auto&& drawValue)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SetWindowFontScale(label_scale);
        textLocalized(label);
        ImGui::TableNextColumn();
        ImGui::SetWindowFontScale(body_scale);
        drawValue();
    };
    auto drawVolumeSlider = [&](const char* label, const char* id, int* value, auto&& applyValue)
    {
        std::string valueText = std::to_string(*value) + "%";
        ImGui::PushID(id);
        if (ImGui::BeginTable("volume_header", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX))
        {
            const float valueColumnWidth = ImGui::CalcTextSize("100%").x + clampf(10.0f * ui_scale, 10.0f, 20.0f);
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, valueColumnWidth);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::SetWindowFontScale(label_scale);
            ImGui::AlignTextToFramePadding();
            textLocalized(label);

            ImGui::TableNextColumn();
            ImGui::SetWindowFontScale(small_scale);
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
            float valueX = ImGui::GetCursorPosX() + (std::max)(0.0f, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(valueText.c_str()).x);
            ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), valueX));
            ImGui::TextUnformatted(valueText.c_str());
            ImGui::PopStyleColor();
            ImGui::EndTable();
        }

        ImGui::SetWindowFontScale(body_scale);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt(id, value, 0, 100, " "))
        {
            applyValue();
        }
        ImGui::PopID();
    };

    if (ImGui::Begin("##battle_system_menu", &system_menu_.open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        if (suppress_hover)
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, chip_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, chip_bg);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_Header));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_Header));
        }

        ImGui::SetWindowFontScale(title_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        textLocalized("系統設定");
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(body_scale);

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, clampf(6.0f * ui_scale, 6.0f, 14.0f)));

        const float footer_gap = clampf(8.0f * ui_scale, 8.0f, 18.0f);
        const float body_h = (std::max)(380.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() - footer_gap);
        const float body_w = ImGui::GetContentRegionAvail().x;
        const bool split_top = body_w >= 860.0f;
        const float column_gap = clampf(16.0f * ui_scale, 12.0f, 24.0f);
        const float section_gap = clampf(10.0f * ui_scale, 8.0f, 18.0f);
        const float top_card_w = split_top ? (body_w - column_gap) * 0.5f : body_w;
        const float min_bottom_h = split_top ? clampf(150.0f * ui_scale, 150.0f, 250.0f) : clampf(140.0f * ui_scale, 140.0f, 220.0f);
        const float radio_gap = clampf(14.0f * ui_scale, 12.0f, 28.0f);
        const float toggle_column_w = clampf(46.0f * ui_scale, 46.0f, 90.0f);
        const float display_label_w = clampf(150.0f * ui_scale, 150.0f, 250.0f);
        const float button_w = clampf(150.0f * ui_scale, 150.0f, 260.0f);

        float top_card_h = 0.0f;
        if (split_top)
        {
            top_card_h = clampf(body_h * 0.44f, clampf(190.0f * ui_scale, 190.0f, 320.0f), body_h - min_bottom_h - section_gap);
        }
        else
        {
            top_card_h = (body_h - min_bottom_h - section_gap * 2.0f) * 0.5f;
            top_card_h = clampf(top_card_h, clampf(120.0f * ui_scale, 120.0f, 180.0f), clampf(180.0f * ui_scale, 180.0f, 240.0f));
        }

        ImGui::BeginChild("system_settings_body", ImVec2(0.0f, body_h), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::BeginChild("system_settings_battle", ImVec2(top_card_w, top_card_h), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        drawSectionTitle("戰鬥");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, clampf(4.0f * ui_scale, 4.0f, 10.0f)));
        if (ImGui::BeginTable("system_settings_battle_rows", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 2.6f);
            ImGui::TableSetupColumn("toggle", ImGuiTableColumnFlags_WidthFixed, toggle_column_w);
            drawCheckboxRow("啟動佈陣", "##position_swap_enabled", &system_menu_.positionSwapEnabled);
            drawCheckboxRow("手動鏡頭（未實裝）", "##manual_camera_enabled", &system_menu_.manualCamera);
            drawCheckboxRow("顯示戰鬥日誌", "##show_battle_log_enabled", &system_menu_.showBattleLog);
            ImGui::EndTable();
        }
        ImGui::EndChild();

        if (split_top)
        {
            ImGui::SameLine(0.0f, column_gap);
        }
        else
        {
            ImGui::Dummy(ImVec2(0.0f, section_gap));
        }

        ImGui::BeginChild("system_settings_audio", ImVec2(0.0f, top_card_h), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        drawSectionTitle("音訊");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, clampf(4.0f * ui_scale, 4.0f, 10.0f)));
        if (ImGui::BeginTable("system_settings_audio_controls", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX))
        {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            drawVolumeSlider("音樂", "##music_volume", &system_menu_.musicVolume, [&]() {
                Audio::getInstance()->setVolume(system_menu_.musicVolume);
            });

            ImGui::TableNextColumn();
            drawVolumeSlider("音效", "##sound_volume", &system_menu_.soundVolume, [&]() {
                Audio::getInstance()->setVolumeWav(system_menu_.soundVolume);
            });

            ImGui::EndTable();
        }
        ImGui::EndChild();

        ImGui::Dummy(ImVec2(0.0f, section_gap));

        float bottom_card_h = (std::max)(0.0f, ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("system_settings_display", ImVec2(0.0f, bottom_card_h), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        drawSectionTitle("顯示與速度");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, clampf(4.0f * ui_scale, 4.0f, 10.0f)));
        if (ImGui::BeginTable("system_settings_display_rows", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, display_label_w);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

            drawRadioGroupRow("戰鬥速度", [&]() {
                if (radioButtonLocalized("2x##battle_speed_2x", system_menu_.battleSpeed == 0))
                {
                    system_menu_.battleSpeed = 0;
                }
                ImGui::SameLine(0.0f, radio_gap);
                if (radioButtonLocalized("1x##battle_speed_1x", system_menu_.battleSpeed == 1))
                {
                    system_menu_.battleSpeed = 1;
                }
                ImGui::SameLine(0.0f, radio_gap);
                if (radioButtonLocalized("0.5x##battle_speed_half", system_menu_.battleSpeed == 2))
                {
                    system_menu_.battleSpeed = 2;
                }
            });

            drawRadioGroupRow("文字", [&]() {
                if (radioButtonLocalized("繁體##font_lang_traditional", !system_menu_.simplifiedChinese))
                {
                    system_menu_.simplifiedChinese = false;
                    Font::getInstance()->setSimplified(0);
                    Font::getInstance()->clearBuffer();
                }
                ImGui::SameLine(0.0f, radio_gap);
                if (radioButtonLocalized("簡體##font_lang_simplified", system_menu_.simplifiedChinese))
                {
                    system_menu_.simplifiedChinese = true;
                    Font::getInstance()->setSimplified(1);
                    Font::getInstance()->clearBuffer();
                }
            });

            ImGui::EndTable();
        }
        ImGui::EndChild();
        ImGui::EndChild();

        ImGui::Dummy(ImVec2(0.0f, footer_gap));
        float button_x = ImGui::GetWindowContentRegionMax().x - button_w;
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), button_x));
        ImGui::PushStyleColor(ImGuiCol_Button, chip_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colorU8(67, 78, 39, 230));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, colorU8(88, 98, 50, 230));
        ImGui::SetWindowFontScale(body_scale);
        if (buttonLocalized("完成", ImVec2(button_w, 0.0f)))
        {
            system_menu_.open = false;
        }
        ImGui::PopStyleColor(3);
        ImGui::SetWindowFontScale(1.0f);
        if (suppress_hover)
        {
            ImGui::PopStyleColor(6);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(10);
    ImGui::PopStyleVar(9);

    if (!system_menu_.open)
    {
        system_menu_input_guard_frames_ = 0;
        system_menu_hover_guard_ = false;
    }
    else if (system_menu_input_guard_frames_ > 0)
    {
        system_menu_input_guard_frames_--;
    }
}
