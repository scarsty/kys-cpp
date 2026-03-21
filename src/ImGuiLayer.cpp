#include "ImGuiLayer.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "Audio.h"
#include "Font.h"
#include "SDL3/SDL.h"
#include "GameUtil.h"
#include <algorithm>

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

    if (battle_log_.open && battle_log_input_guard_frames_ > 0)
    {
        switch (event.type)
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
            break;
        }
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
}

void ImGuiLayer::hideBattleSystemMenu()
{
    system_menu_.open = false;
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
        ImGui::TextUnformatted(battle_log_.title.empty() ? "本次戰鬥日誌" : battle_log_.title.c_str());
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
            ImGui::TextUnformatted(label.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::SetWindowFontScale(chip_scale);
            ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
            ImGui::TextUnformatted(value.c_str());
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
            if (ImGui::BeginCombo(label, preview.c_str()))
            {
                ImGui::SetWindowFontScale(filter_scale);
                bool allSelected = selectedId < 0;
                if (ImGui::Selectable(allLabel, allSelected))
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
                    if (ImGui::Selectable(row.name.c_str(), selected))
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
        ImGui::TextUnformatted("我方篩選");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        drawFilterCombo("##ally_filter", "全部我方", battle_log_.allies, battle_log_ally_filter_id_);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
        ImGui::TextUnformatted("敵方篩選");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        drawFilterCombo("##enemy_filter", "全部敵方", battle_log_.enemies, battle_log_enemy_filter_id_);
        ImGui::SameLine();
        if (ImGui::Button("重設篩選") && allow_close)
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
                    ImGui::TextUnformatted(seg.text.c_str());
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
                ImGui::TextWrapped("%s", entry.text.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::Spacing();
        }

        if (!drewAnyEntry)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
            ImGui::TextUnformatted("當前篩選下沒有符合的記錄。");
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
        ImGui::TextUnformatted("點擊「繼續」關閉日誌");
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
        if (ImGui::Button("繼續", ImVec2(button_w, button_h)) && allow_close)
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
    float body_scale = clampf(vp_size.y / 740.0f, 1.24f, 2.10f);
    float title_scale = clampf(body_scale * 1.34f, 1.52f, 2.48f);
    float section_scale = clampf(body_scale * 1.14f, 1.28f, 2.02f);
    ImVec2 panel_size((std::max)(860.0f, vp_size.x * 0.68f), (std::max)(560.0f, vp_size.y * 0.72f));
    panel_size.x = (std::min)(panel_size.x, vp_size.x - 42.0f);
    panel_size.y = (std::min)(panel_size.y, vp_size.y - 34.0f);
    ImVec2 panel_pos(vp_pos.x + (vp_size.x - panel_size.x) * 0.5f, vp_pos.y + (vp_size.y - panel_size.y) * 0.5f);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        system_menu_.open = false;
    }

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
    ImGui::PushStyleColor(ImGuiCol_FrameBg, colorU8(49, 55, 43, 200));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, colorU8(60, 67, 52, 220));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, colorU8(70, 77, 58, 230));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, title_gold);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, title_gold);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, colorU8(255, 236, 140));

    auto* game = GameUtil::getInstance();
    auto persistGameInt = [&](const char* section, const char* key, int value)
    {
        game->setKey(section, key, std::to_string(value));
    };
    auto drawSectionTitle = [&](const char* title)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        ImGui::SetWindowFontScale(section_scale);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(body_scale);
    };

    if (ImGui::Begin("##battle_system_menu", &system_menu_.open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::SetWindowFontScale(title_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        ImGui::TextUnformatted("戰鬥設定");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float section_h = ImGui::GetContentRegionAvail().y - 68.0f;
        ImGui::BeginChild("battle_system_content", ImVec2(0.0f, section_h), true);

        drawSectionTitle("戰前");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("啟動佈陣##position_swap", &system_menu_.positionSwapEnabled);
        ImGui::Spacing();

        if (ImGui::Checkbox("手動鏡頭（未實裝）##manual_camera", &system_menu_.manualCamera))
        {
            persistGameInt("game", "manual_battle_camera", system_menu_.manualCamera ? 1 : 0);
        }
        ImGui::Spacing();

        if (ImGui::Checkbox("顯示戰鬥日誌##show_battle_log", &system_menu_.showBattleLog))
        {
            persistGameInt("game", "show_battle_log", system_menu_.showBattleLog ? 1 : 0);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        drawSectionTitle("音量");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextUnformatted("音樂");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt("##music_volume", &system_menu_.musicVolume, 0, 100, "%d%%"))
        {
            Audio::getInstance()->setVolume(system_menu_.musicVolume);
            persistGameInt("music", "volume", system_menu_.musicVolume);
        }
        ImGui::Spacing();

        ImGui::TextUnformatted("音效");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt("##sound_volume", &system_menu_.soundVolume, 0, 100, "%d%%"))
        {
            Audio::getInstance()->setVolumeWav(system_menu_.soundVolume);
            persistGameInt("music", "volumewav", system_menu_.soundVolume);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        drawSectionTitle("顯示與速度");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextUnformatted("戰鬥速度");
        if (ImGui::RadioButton("2x##battle_speed", system_menu_.battleSpeed == 0))
        {
            system_menu_.battleSpeed = 0;
            persistGameInt("game", "battle_speed", system_menu_.battleSpeed);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("1x##battle_speed", system_menu_.battleSpeed == 1))
        {
            system_menu_.battleSpeed = 1;
            persistGameInt("game", "battle_speed", system_menu_.battleSpeed);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("0.5x##battle_speed", system_menu_.battleSpeed == 2))
        {
            system_menu_.battleSpeed = 2;
            persistGameInt("game", "battle_speed", system_menu_.battleSpeed);
        }
        ImGui::Spacing();

        ImGui::TextUnformatted("文字");
        if (ImGui::RadioButton("繁體##font_lang", !system_menu_.simplifiedChinese))
        {
            system_menu_.simplifiedChinese = false;
            Font::getInstance()->setSimplified(0);
            Font::getInstance()->clearBuffer();
            persistGameInt("game", "simplified_chinese", 0);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("簡體##font_lang", system_menu_.simplifiedChinese))
        {
            system_menu_.simplifiedChinese = true;
            Font::getInstance()->setSimplified(1);
            Font::getInstance()->clearBuffer();
            persistGameInt("game", "simplified_chinese", 1);
        }

        ImGui::Spacing();
        ImGui::EndChild();

        ImGui::Spacing();
        float button_w = 150.0f;
        float button_x = ImGui::GetWindowContentRegionMax().x - button_w;
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), button_x));
        ImGui::PushStyleColor(ImGuiCol_Button, chip_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colorU8(67, 78, 39, 230));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, colorU8(88, 98, 50, 230));
        ImGui::SetWindowFontScale(body_scale);
        if (ImGui::Button("完成", ImVec2(button_w, 0.0f)))
        {
            system_menu_.open = false;
        }
        ImGui::PopStyleColor(3);
        ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::End();
    ImGui::PopStyleColor(10);
    ImGui::PopStyleVar(3);
}
