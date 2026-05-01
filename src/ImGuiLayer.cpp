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
    ImGui::GetStyle().FontSizeBase = 18.0f;

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

    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT)
    {
        if (system_menu_.open && system_menu_input_guard_frames_ <= 0)
        {
            hideBattleSystemMenu();
            return true;
        }
        if (battle_log_.open && battle_log_input_guard_frames_ <= 0)
        {
            hideBattleLog();
            return true;
        }
        if (changelog_.open && changelog_input_guard_frames_ <= 0)
        {
            hideChangelog();
            return true;
        }
    }

    if (((battle_log_.open && battle_log_input_guard_frames_ > 0)
            || (system_menu_.open && system_menu_input_guard_frames_ > 0)
            || (changelog_.open && changelog_input_guard_frames_ > 0))
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
    if (!initialized_)
    {
        return;
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    int window_w = 0;
    int window_h = 0;
    SDL_GetWindowSize(window, &window_w, &window_h);
    ImGui::GetStyle().FontScaleMain = clampf(window_h / 720.0f, 1.24f, 3.0f);

    if (!visible_ && !battle_log_.open && !system_menu_.open && !changelog_.open && !show_metrics_window_)
    {
        ImGui::EndFrame();
        return;
    }

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
    renderChangelogWindow();

    if (show_metrics_window_)
    {
        ImGui::ShowMetricsWindow(&show_metrics_window_);
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

bool ImGuiLayer::wantsCaptureEvent(const SDL_Event& event) const
{
    if (!initialized_ || (!visible_ && !battle_log_.open && !system_menu_.open && !changelog_.open))
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
        return battle_log_.open || system_menu_.open || changelog_.open || io.WantCaptureMouse;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_TEXT_EDITING:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        return battle_log_.open || system_menu_.open || changelog_.open || io.WantCaptureKeyboard || io.WantTextInput;
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
    battle_log_dragging_ = false;
    battle_log_child_flip_ ^= 1;
    battle_log_ally_filter_id_ = -1;
    battle_log_enemy_filter_id_ = -1;
}

void ImGuiLayer::hideBattleLog()
{
    battle_log_.open = false;
    battle_log_input_guard_frames_ = 0;
    battle_log_hover_guard_ = false;
    battle_log_dragging_ = false;
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

void ImGuiLayer::showChangelog(const ChangelogData& data)
{
    changelog_ = data;
    changelog_.open = true;
    changelog_input_guard_frames_ = 8;
    changelog_hover_guard_ = true;
    changelog_dragging_ = false;
}

void ImGuiLayer::hideChangelog()
{
    changelog_.open = false;
    changelog_input_guard_frames_ = 0;
    changelog_hover_guard_ = false;
    changelog_dragging_ = false;
}

bool ImGuiLayer::isChangelogOpen() const
{
    return changelog_.open;
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
    const float fs = ImGui::GetFontSize();
    const float fsBase = ImGui::GetStyle().FontSizeBase;
    ImVec2 panel_size((std::max)(fs * 36.0f, vp_size.x * 0.90f), (std::max)(fs * 23.0f, vp_size.y * 0.86f));
    panel_size.x = (std::min)(panel_size.x, vp_size.x - fs * 1.3f);
    panel_size.y = (std::min)(panel_size.y, vp_size.y - fs * 1.1f);
    ImVec2 panel_pos(vp_pos.x + (vp_size.x - panel_size.x) * 0.5f, vp_pos.y + (vp_size.y - panel_size.y) * 0.5f);

    ImGui::GetForegroundDrawList()->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y), ImGui::ColorConvertFloat4ToU32(dim_bg));

    ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, fs * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(fs * 1.0f, fs * 0.8f));
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

        ImGui::PushFont(nullptr, fsBase * 1.34f);
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        auto title = localizeImGuiText(battle_log_.title.empty() ? std::string_view("本次戰鬥日誌") : std::string_view(battle_log_.title));
        ImGui::TextUnformatted(title.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::PopFont();

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
            ImGui::PushFont(nullptr, fsBase * 1.04f);
            auto localizedLabel = localizeImGuiText(label);
            ImGui::TextUnformatted(localizedLabel.c_str());
            ImGui::PopFont();
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, fs * 0.3f);
            ImGui::PushFont(nullptr, fsBase * 1.18f);
            ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
            auto localizedValue = localizeImGuiText(value);
            ImGui::TextUnformatted(localizedValue.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
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

            if (beginComboLocalized(label, preview))
            {
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
        };

        ImGui::PushFont(nullptr, fsBase * 1.14f);
        ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
        textLocalized("我方篩選");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(fs * 8.0f);
        drawFilterCombo("##ally_filter", "全部我方", battle_log_.allies, battle_log_ally_filter_id_);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
        textLocalized("敵方篩選");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(fs * 8.0f);
        drawFilterCombo("##enemy_filter", "全部敵方", battle_log_.enemies, battle_log_enemy_filter_id_);
        ImGui::SameLine();
        if (buttonLocalized("重設篩選") && allow_close)
        {
            battle_log_ally_filter_id_ = -1;
            battle_log_enemy_filter_id_ = -1;
        }
        ImGui::PopFont();

        ImGui::Spacing();
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, fs * 2.0f);
        const char* child_id = battle_log_child_flip_ ? "battle_log_entries_b" : "battle_log_entries_a";
        ImVec2 child_pos = ImGui::GetCursorScreenPos();
        ImVec2 child_size = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2.0f);
        if (child_size.x < 1.0f) child_size.x = 1.0f;
        if (child_size.y < 1.0f) child_size.y = 1.0f;
        ImGui::BeginChild(child_id, child_size, true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
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

        const ImGuiIO& drag_io = ImGui::GetIO();
        bool mouse_in_child = drag_io.MousePos.x >= child_pos.x
            && drag_io.MousePos.x < child_pos.x + child_size.x
            && drag_io.MousePos.y >= child_pos.y
            && drag_io.MousePos.y < child_pos.y + child_size.y;
        if (allow_close && !battle_log_dragging_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouse_in_child)
        {
            battle_log_dragging_ = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            battle_log_dragging_ = false;
        }
        if (battle_log_dragging_)
        {
            ImGui::SetScrollY(ImGui::GetScrollY() - drag_io.MouseDelta.y);
            ImGui::SetScrollX(ImGui::GetScrollX() - drag_io.MouseDelta.x);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushFont(nullptr, fsBase * 1.04f);
        ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
        textLocalized("點擊「繼續」關閉日誌");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        float button_w = fs * 8.5f;
        float button_x = ImGui::GetWindowContentRegionMax().x - button_w;
        float button_h = ImGui::GetFrameHeight() * 1.5f;
        ImGui::PopFont();
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), button_x));
        ImGui::PushStyleColor(ImGuiCol_Button, chip_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, allow_close ? colorU8(67, 78, 39, 230) : chip_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, allow_close ? colorU8(88, 98, 50, 230) : chip_bg);
        ImGui::PushFont(nullptr, fsBase * 1.14f);
        if (buttonLocalized("繼續", ImVec2(button_w, button_h)) && allow_close)
        {
            battle_log_.open = false;
            battle_log_dragging_ = false;
        }
        ImGui::PopFont();
        ImGui::PopStyleColor(3);
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
    if (!battle_log_.open)
    {
        battle_log_dragging_ = false;
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
    const ImVec4 text_main = colorU8(244, 240, 224);
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

    const float fs = ImGui::GetFontSize();
    const float fsBase = ImGui::GetStyle().FontSizeBase;
    float row_gap = fs * 0.7f;
    float label_column_w = fs * 15.0f;
    float button_w = fs * 10.0f;
    float footer_gap = fs * 0.6f;
    ImVec2 panel_size((std::max)(fs * 32.0f, vp_size.x * 0.64f), (std::max)(fs * 21.0f, vp_size.y * 0.84f));
    panel_size.x = (std::min)(panel_size.x, vp_size.x - fs * 0.7f);
    panel_size.y = (std::min)(panel_size.y, vp_size.y - fs * 0.45f);
    ImVec2 panel_pos(vp_pos.x + (vp_size.x - panel_size.x) * 0.5f, vp_pos.y + (vp_size.y - panel_size.y) * 0.5f);
    bool suppress_hover = system_menu_hover_guard_ || system_menu_input_guard_frames_ > 0;

    if (system_menu_input_guard_frames_ <= 0 && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        system_menu_.open = false;
    }

    ImGui::GetForegroundDrawList()->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y), ImGui::ColorConvertFloat4ToU32(dim_bg));

    ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, fs * 0.8f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(fs * 1.2f, fs * 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(fs * 0.6f, fs * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(fs * 0.5f, fs * 0.35f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(fs * 0.35f, fs * 0.25f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, fs * 0.6f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, fs * 0.25f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, fs * 0.7f);
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

        ImGui::PushFont(nullptr, fsBase * 1.16f);
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        textLocalized("系統設定");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, row_gap));

        const float button_h = ImGui::GetFrameHeight() * 1.45f;
        const float footer_reserve_h = button_h + footer_gap * 2.0f + ImGui::GetFrameHeightWithSpacing();
        const float body_h = (std::max)(0.0f, ImGui::GetContentRegionAvail().y - footer_reserve_h);
        ImGui::BeginChild("system_settings_body", ImVec2(0.0f, body_h), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const ImGuiStyle& style = ImGui::GetStyle();

        auto drawSettingRow = [&](const char* label, auto&& drawControl)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, text_main);
            ImGui::AlignTextToFramePadding();
            textLocalized(label);
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            drawControl(ImGui::GetContentRegionAvail().x);
        };

        auto drawCenteredCheckbox = [&](const char* id, bool* value, float control_width)
        {
            ImGui::Checkbox(id, value);
        };

        auto drawVolumeSlider = [&](const char* id, int* value, float control_width, auto&& applyValue)
        {
            float sliderWidth = (std::min)(fs * 12.0f, control_width * 0.60f);
            ImGui::SetNextItemWidth(sliderWidth);
            if (ImGui::SliderInt(id, value, 0, 100, " "))
            {
                applyValue();
            }
        };

        auto radioButtonWidth = [&](const char* label)
        {
            std::string localized = localizeImGuiLabel(label);
            return ImGui::CalcTextSize(localized.c_str()).x + ImGui::GetFrameHeight() + style.ItemInnerSpacing.x;
        };

        auto drawBattleSpeedRow = [&](float control_width)
        {
            const float radio_gap = fs * 0.8f;
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
        };

        auto drawLanguageRow = [&](float control_width)
        {
            const float radio_gap = fs * 0.8f;
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
        };

        if (ImGui::BeginTable("system_settings_rows", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, label_column_w);
            ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch, 1.0f);

            drawSettingRow("啟動佈陣", [&](float control_width) { drawCenteredCheckbox("##position_swap_enabled", &system_menu_.positionSwapEnabled, control_width); });
            drawSettingRow("手動鏡頭", [&](float control_width) { drawCenteredCheckbox("##manual_camera_enabled", &system_menu_.manualCamera, control_width); });
            drawSettingRow("顯示戰鬥日誌", [&](float control_width) { drawCenteredCheckbox("##show_battle_log_enabled", &system_menu_.showBattleLog, control_width); });
            drawSettingRow("音樂", [&](float control_width) { drawVolumeSlider("##music_volume", &system_menu_.musicVolume, control_width, [&]() { Audio::getInstance()->setVolume(system_menu_.musicVolume); }); });
            drawSettingRow("音效", [&](float control_width) { drawVolumeSlider("##sound_volume", &system_menu_.soundVolume, control_width, [&]() { Audio::getInstance()->setVolumeWav(system_menu_.soundVolume); }); });
            drawSettingRow("戰鬥速度", [&](float control_width) { drawBattleSpeedRow(control_width); });
            drawSettingRow("文字", [&](float control_width) { drawLanguageRow(control_width); });

            ImGui::EndTable();
        }

        ImGui::EndChild();

        ImGui::Dummy(ImVec2(0.0f, footer_gap * 0.75f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, footer_gap * 0.75f));

        float button_x = ImGui::GetWindowContentRegionMax().x - button_w;
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), button_x));
        ImGui::PushStyleColor(ImGuiCol_Button, chip_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colorU8(67, 78, 39, 230));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, colorU8(88, 98, 50, 230));
        if (buttonLocalized("完成", ImVec2(button_w, button_h)))
        {
            system_menu_.open = false;
        }
        ImGui::PopStyleColor(3);
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

void ImGuiLayer::renderChangelogWindow()
{
    if (!changelog_.open)
    {
        return;
    }

    const ImVec4 panel_bg = colorU8(31, 34, 29, 232);
    const ImVec4 panel_border = colorU8(190, 166, 96, 205);
    const ImVec4 title_gold = colorU8(242, 222, 126);
    const ImVec4 heading_gold = colorU8(232, 205, 112);
    const ImVec4 text_main = colorU8(236, 232, 215);
    const ImVec4 text_muted = colorU8(168, 164, 145);
    const ImVec4 bullet_color = colorU8(211, 177, 92);
    const ImVec4 accent_line = colorU8(166, 145, 78, 90);
    const ImVec4 chip_bg = colorU8(91, 96, 59, 165);
    const ImVec4 child_bg = colorU8(18, 21, 18, 118);
    const ImVec4 dim_bg = colorU8(6, 7, 6, 70);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 vp_pos = viewport ? viewport->Pos : ImVec2(0.0f, 0.0f);
    ImVec2 vp_size = viewport ? viewport->Size : ImVec2(1280.0f, 720.0f);
    const ImGuiIO& io = ImGui::GetIO();
    if (changelog_hover_guard_ && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
    {
        changelog_hover_guard_ = false;
    }

    const float fs = ImGui::GetFontSize();
    const float fsBase = ImGui::GetStyle().FontSizeBase;
    const bool allow_close = changelog_input_guard_frames_ <= 0;
    if (allow_close && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        changelog_.open = false;
    }

    ImVec2 panel_size((std::max)(fs * 36.0f, vp_size.x * 0.72f), (std::max)(fs * 24.0f, vp_size.y * 0.88f));
    panel_size.x = (std::min)(panel_size.x, vp_size.x - fs * 0.8f);
    panel_size.y = (std::min)(panel_size.y, vp_size.y - fs * 0.6f);
    ImVec2 panel_pos(vp_pos.x + (vp_size.x - panel_size.x) * 0.5f, vp_pos.y + (vp_size.y - panel_size.y) * 0.5f);
    bool suppress_hover = changelog_hover_guard_ || !allow_close;

    ImGui::GetForegroundDrawList()->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y), ImGui::ColorConvertFloat4ToU32(dim_bg));

    ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, fs * 0.7f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(fs * 1.1f, fs * 0.9f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, fs * 0.45f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, fs * 0.25f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, fs * 1.4f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, panel_bg);
    ImGui::PushStyleColor(ImGuiCol_Border, panel_border);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, child_bg);
    ImGui::PushStyleColor(ImGuiCol_Separator, accent_line);

    if (ImGui::Begin("##changelog_window", &changelog_.open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        if (suppress_hover)
        {
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, chip_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, chip_bg);
        }

        ImGui::PushFont(nullptr, fsBase * 1.35f);
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        textLocalized(changelog_.title.empty() ? "更新日誌" : changelog_.title);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float button_h = ImGui::GetFrameHeight() * 1.45f;
        const float footer_h = button_h + fs * 1.15f;
        ImVec2 child_size(ImGui::GetContentRegionAvail().x, (std::max)(0.0f, ImGui::GetContentRegionAvail().y - footer_h));
        if (child_size.x < 1.0f) child_size.x = 1.0f;
        if (child_size.y < 1.0f) child_size.y = 1.0f;
        ImVec2 child_pos = ImGui::GetCursorScreenPos();
        ImGui::BeginChild("changelog_body", child_size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);

        if (!changelog_.error.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, colorU8(255, 170, 120));
            auto error = localizeImGuiText(changelog_.error);
            ImGui::TextWrapped("%s", error.c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            for (const auto& line : changelog_.lines)
            {
                if (line.text.empty())
                {
                    ImGui::Spacing();
                    continue;
                }

                if (line.headingLevel > 0)
                {
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, heading_gold);
                    ImGui::PushFont(nullptr, line.headingLevel == 1 ? fsBase * 1.22f : fsBase * 1.12f);
                    auto heading = localizeImGuiText(line.text);
                    ImGui::TextWrapped("%s", heading.c_str());
                    ImGui::PopFont();
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                    continue;
                }

                if (line.bulletIndent >= 0)
                {
                    const float indent = fs * (0.75f + line.bulletIndent * 1.25f);
                    ImGui::Indent(indent);
                    ImGui::PushStyleColor(ImGuiCol_Text, bullet_color);
                    ImGui::Bullet();
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, text_main);
                    auto bullet = localizeImGuiText(line.text);
                    ImGui::TextWrapped("%s", bullet.c_str());
                    ImGui::PopStyleColor();
                    ImGui::Unindent(indent);
                    continue;
                }

                ImGui::PushStyleColor(ImGuiCol_Text, text_main);
                auto text = localizeImGuiText(line.text);
                ImGui::TextWrapped("%s", text.c_str());
                ImGui::PopStyleColor();
            }
        }

        const ImGuiIO& drag_io = ImGui::GetIO();
        bool mouse_in_child = drag_io.MousePos.x >= child_pos.x
            && drag_io.MousePos.x < child_pos.x + child_size.x
            && drag_io.MousePos.y >= child_pos.y
            && drag_io.MousePos.y < child_pos.y + child_size.y;
        if (allow_close && !changelog_dragging_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouse_in_child)
        {
            changelog_dragging_ = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            changelog_dragging_ = false;
        }
        if (changelog_dragging_)
        {
            ImGui::SetScrollY(ImGui::GetScrollY() - drag_io.MouseDelta.y);
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float button_w = fs * 8.0f;
        float button_x = ImGui::GetWindowContentRegionMax().x - button_w;
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), button_x));
        ImGui::PushStyleColor(ImGuiCol_Button, chip_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, allow_close ? colorU8(67, 78, 39, 230) : chip_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, allow_close ? colorU8(88, 98, 50, 230) : chip_bg);
        if (buttonLocalized("返回", ImVec2(button_w, button_h)) && allow_close)
        {
            changelog_.open = false;
            changelog_dragging_ = false;
        }
        ImGui::PopStyleColor(3);

        if (suppress_hover)
        {
            ImGui::PopStyleColor(2);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(6);

    if (!changelog_.open)
    {
        changelog_input_guard_frames_ = 0;
        changelog_hover_guard_ = false;
        changelog_dragging_ = false;
    }
    else if (changelog_input_guard_frames_ > 0)
    {
        changelog_input_guard_frames_--;
    }
}
