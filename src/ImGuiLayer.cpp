#include "ImGuiLayer.h"

#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_sdl3.h"
#include "../third_party/imgui/backends/imgui_impl_sdlrenderer3.h"

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
    if (!initialized_ || (!visible_ && !battle_log_.open))
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

    if (show_demo_window_)
    {
        ImGui::ShowDemoWindow(&show_demo_window_);
    }
    if (show_metrics_window_)
    {
        ImGui::ShowMetricsWindow(&show_metrics_window_);
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

bool ImGuiLayer::wantsCaptureEvent(const SDL_Event& event) const
{
    if (!initialized_ || (!visible_ && !battle_log_.open))
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
        return battle_log_.open || io.WantCaptureMouse;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_TEXT_EDITING:
        return battle_log_.open || io.WantCaptureKeyboard || io.WantTextInput;
    default:
        return false;
    }
}

void ImGuiLayer::showBattleLog(const BattleLogData& data)
{
    battle_log_ = data;
    battle_log_.open = true;
    battle_log_input_guard_frames_ = 10;
}

void ImGuiLayer::hideBattleLog()
{
    battle_log_.open = false;
    battle_log_input_guard_frames_ = 0;
}

bool ImGuiLayer::isBattleLogOpen() const
{
    return battle_log_.open;
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
    float body_scale = clampf(vp_size.y / 740.0f, 1.24f, 2.10f);
    float title_scale = clampf(body_scale * 1.34f, 1.52f, 2.48f);
    float chip_scale = clampf(body_scale * 1.18f, 1.34f, 2.18f);
    float small_scale = clampf(body_scale * 1.04f, 1.16f, 1.78f);
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

    if (ImGui::Begin("##battle_log", &battle_log_.open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        bool allow_close = battle_log_input_guard_frames_ <= 0;
        ImGui::SetWindowFontScale(title_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        ImGui::TextUnformatted(battle_log_.title.empty() ? "本次战斗日志" : battle_log_.title.c_str());
        ImGui::SameLine();
        ImGui::TextUnformatted("  •  戰鬥記錄");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::SetWindowFontScale(body_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
        ImGui::TextUnformatted("记录本场自动战斗中的关键出手、伤害与击杀。");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        std::vector<std::pair<std::string, std::string>> chips = {
            {"战斗结果", battle_log_.resultText},
            {"总帧数", std::to_string(battle_log_.totalFrames)},
            {"记录条数", std::to_string((int)battle_log_.entries.size())}
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

        auto drawTeamTable = [&](const char* table_id, const char* label, const std::vector<BattleLogRoleRow>& rows, const ImVec4& name_color)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
            if (ImGui::BeginTable(table_id, 5, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("角色");
                ImGui::TableSetupColumn("输出");
                ImGui::TableSetupColumn("承伤");
                ImGui::TableSetupColumn("击杀");
                ImGui::TableSetupColumn("存活");
                ImGui::TableHeadersRow();
                for (const auto& row : rows)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::PushStyleColor(ImGuiCol_Text, name_color);
                    ImGui::Text("%s", row.name.c_str());
                    ImGui::PopStyleColor();
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", row.damageDealt);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", row.damageTaken);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", row.kills);
                    ImGui::TableNextColumn();
                    if (row.dead)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, enemy_color);
                        ImGui::TextUnformatted("阵亡");
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        ImGui::Text("%d/%d", (std::max)(row.hpRemaining, 0), (std::max)(row.maxHp, 0));
                    }
                }
                ImGui::EndTable();
            }
        };

        float avail_w = ImGui::GetContentRegionAvail().x;
        float left_w = avail_w * 0.66f;
        float right_w = avail_w - left_w - 16.0f;
        float section_h = ImGui::GetContentRegionAvail().y - 68.0f;

        ImGui::BeginChild("battle_log_left", ImVec2(left_w, section_h), true);
        ImGui::SetWindowFontScale(chip_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        ImGui::TextUnformatted("戰鬥記錄");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::BeginChild("battle_log_entries", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
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
        for (const auto& entry : battle_log_.entries)
        {
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
        ImGui::EndChild();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("battle_log_right", ImVec2(right_w, section_h), true);
        ImGui::SetWindowFontScale(chip_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        ImGui::TextUnformatted("本場概覽");
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(body_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
        ImGui::TextUnformatted("右侧集中查看双方表现。");
        ImGui::PopStyleColor();
        ImGui::Separator();

        int ally_alive = 0;
        int enemy_alive = 0;
        int ally_damage = 0;
        int enemy_damage = 0;
        for (const auto& row : battle_log_.allies)
        {
            ally_alive += row.dead ? 0 : 1;
            ally_damage += row.damageDealt;
        }
        for (const auto& row : battle_log_.enemies)
        {
            enemy_alive += row.dead ? 0 : 1;
            enemy_damage += row.damageDealt;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ally_color);
        ImGui::Text("我方存活: %d / %d", ally_alive, (int)battle_log_.allies.size());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, enemy_color);
        ImGui::Text("敌方存活: %d / %d", enemy_alive, (int)battle_log_.enemies.size());
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, text_main);
        ImGui::Text("我方总输出: %d", ally_damage);
        ImGui::Text("敌方总输出: %d", enemy_damage);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        drawTeamTable("battle_log_allies", "我方统计", battle_log_.allies, ally_color);
        ImGui::Spacing();
        drawTeamTable("battle_log_enemies", "敌方统计", battle_log_.enemies, enemy_color);
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetWindowFontScale(small_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
        ImGui::TextUnformatted("点击“继续”关闭日志");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        float button_w = 150.0f;
        float button_x = ImGui::GetWindowContentRegionMax().x - button_w;
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), button_x));
        ImGui::PushStyleColor(ImGuiCol_Button, chip_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colorU8(67, 78, 39, 230));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, colorU8(88, 98, 50, 230));
        ImGui::SetWindowFontScale(body_scale);
        if (!allow_close)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("继续", ImVec2(button_w, 0.0f)) && allow_close)
        {
            battle_log_.open = false;
        }
        if (!allow_close)
        {
            ImGui::EndDisabled();
        }
        ImGui::PopStyleColor(3);
        ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::End();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);

    if (battle_log_input_guard_frames_ > 0)
    {
        battle_log_input_guard_frames_--;
    }
}
