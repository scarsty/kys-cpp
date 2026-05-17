#include "BattleLogImGuiView.h"

#include "BattleLogPresenter.h"
#include "ImGuiShared.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace KysImGui;

void BattleLogImGuiView::render(BattleLogWindowState& state) const
{
    if (!state.model.open)
    {
        return;
    }

    const ImVec4 panel_bg = colorU8(34, 38, 30, 164);
    const ImVec4 panel_border = colorU8(192, 172, 118, 190);
    const ImVec4 title_gold = colorU8(238, 221, 112);
    const ImVec4 text_muted = colorU8(202, 198, 180);
    const ImVec4 ally_color = colorU8(170, 232, 166);
    const ImVec4 enemy_color = colorU8(245, 165, 155);
    const ImVec4 neutral_line = colorU8(230, 224, 205);
    const ImVec4 system_line = colorU8(241, 220, 118);
    const ImVec4 skill_color = colorU8(125, 205, 245);
    const ImVec4 damage_color = colorU8(255, 196, 96);
    const ImVec4 duration_color = colorU8(190, 166, 255);
    const ImVec4 frame_color = colorU8(180, 205, 220);
    const ImVec4 formula_color = colorU8(255, 220, 135);
    const ImVec4 projectile_color = colorU8(155, 218, 255);
    const ImVec4 chip_bg = colorU8(92, 96, 59, 150);
    const ImVec4 child_bg = colorU8(30, 34, 27, 88);
    const ImVec4 accent_line = colorU8(166, 152, 82, 90);
    const ImVec4 dim_bg = colorU8(8, 10, 8, 42);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 vp_pos = viewport ? viewport->Pos : ImVec2(0.0f, 0.0f);
    ImVec2 vp_size = viewport ? viewport->Size : ImVec2(1280.0f, 720.0f);
    const ImGuiIO& io = ImGui::GetIO();
    if (state.hoverGuard && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
    {
        state.hoverGuard = false;
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

    if (ImGui::Begin("battle_log_window", &state.model.open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus))
    {
        bool allow_close = state.inputGuardFrames <= 0;
        bool suppress_hover = state.hoverGuard || !allow_close;
        if (suppress_hover)
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, chip_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, chip_bg);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_Header));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_Header));
        }

        ImGui::PushFont(nullptr, fsBase * 1.34f);
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        auto title = localizeImGuiText(state.model.title.empty() ? std::string_view("本場戰鬥日誌") : std::string_view(state.model.title));
        ImGui::TextUnformatted(title.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::PopFont();

        std::vector<std::pair<std::string, std::string>> chips = {
            { "戰鬥結果", state.model.resultText },
            { "總幀數", std::to_string(state.model.totalFrames) },
            { "顯示條數", std::format("{} / {}", countVisibleBattleLogEntries(state.model, state.filter), static_cast<int>(state.model.entries.size())) }
        };
        if (state.model.omittedEntries > 0)
        {
            chips.push_back({ "已省略", std::to_string(state.model.omittedEntries) });
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
        if (ImGui::BeginTable("battle_log_chips", static_cast<int>(chips.size()), ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame))
        {
            for (int i = 0; i < static_cast<int>(chips.size()); ++i)
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

        auto drawFilterCombo = [&](const char* label, const char* allLabel, const std::vector<BattleLogRoleFilterRow>& rows, int& selectedId)
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

        auto drawCategoryFilterCombo = [&]()
        {
            static constexpr std::array categories = {
                BattleLogEntryCategory::Any,
                BattleLogEntryCategory::Damage,
                BattleLogEntryCategory::Heal,
                BattleLogEntryCategory::Status,
                BattleLogEntryCategory::Cast,
                BattleLogEntryCategory::ProjectileCancel,
                BattleLogEntryCategory::Lifecycle,
                BattleLogEntryCategory::BattleEnd,
            };

            if (beginComboLocalized("##category_filter", battleLogCategoryFilterLabel(state.filter.category)))
            {
                for (BattleLogEntryCategory category : categories)
                {
                    const bool selected = state.filter.category == category;
                    if (selectableLocalized(battleLogCategoryFilterLabel(category), selected))
                    {
                        state.filter.category = category;
                    }
                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
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
        drawFilterCombo("##ally_filter", "全部我方", state.model.allies, state.filter.allyUnitId);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
        textLocalized("敵方篩選");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(fs * 8.0f);
        drawFilterCombo("##enemy_filter", "全部敵方", state.model.enemies, state.filter.enemyUnitId);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, text_muted);
        textLocalized("類型篩選");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(fs * 7.5f);
        drawCategoryFilterCombo();
        ImGui::SameLine();
        if (buttonLocalized("重設篩選") && allow_close)
        {
            state.filter = {};
        }
        ImGui::PopFont();

        ImGui::Spacing();
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, fs * 2.0f);
        const char* child_id = state.childFlip ? "battle_log_entries_b" : "battle_log_entries_a";
        ImVec2 child_pos = ImGui::GetCursorScreenPos();
        ImVec2 child_size = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2.0f);
        if (child_size.x < 1.0f) child_size.x = 1.0f;
        if (child_size.y < 1.0f) child_size.y = 1.0f;
        ImGui::BeginChild(child_id, child_size, true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
        auto colorForField = [&](KysChess::Battle::BattleLogTextTone tone, BattleLogEntryTone line_tone) -> ImVec4
        {
            switch (tone)
            {
            case KysChess::Battle::BattleLogTextTone::AllyName:
                return ally_color;
            case KysChess::Battle::BattleLogTextTone::EnemyName:
                return enemy_color;
            case KysChess::Battle::BattleLogTextTone::SkillName:
                return skill_color;
            case KysChess::Battle::BattleLogTextTone::DamageValue:
                return damage_color;
            case KysChess::Battle::BattleLogTextTone::HealValue:
                return ally_color;
            case KysChess::Battle::BattleLogTextTone::ShieldValue:
                return duration_color;
            case KysChess::Battle::BattleLogTextTone::ResourceValue:
                return projectile_color;
            case KysChess::Battle::BattleLogTextTone::SystemAccent:
                return system_line;
            case KysChess::Battle::BattleLogTextTone::DurationValue:
                return duration_color;
            case KysChess::Battle::BattleLogTextTone::FrameValue:
                return frame_color;
            case KysChess::Battle::BattleLogTextTone::FormulaValue:
                return formula_color;
            case KysChess::Battle::BattleLogTextTone::ProjectileId:
                return projectile_color;
            case KysChess::Battle::BattleLogTextTone::Positive:
                return ally_color;
            case KysChess::Battle::BattleLogTextTone::Negative:
                return enemy_color;
            default:
                break;
            }
            if (line_tone == BattleLogEntryTone::Ally) return neutral_line;
            if (line_tone == BattleLogEntryTone::Enemy) return neutral_line;
            if (line_tone == BattleLogEntryTone::System) return system_line;
            return neutral_line;
        };
        bool drewAnyEntry = false;
        for (const auto& entry : state.model.entries)
        {
            if (!battleLogEntryMatchesFilter(entry, state.filter))
            {
                continue;
            }

            drewAnyEntry = true;
            ImVec4 line_color = neutral_line;
            if (entry.tone == BattleLogEntryTone::Ally) line_color = ally_color;
            if (entry.tone == BattleLogEntryTone::Enemy) line_color = enemy_color;
            if (entry.tone == BattleLogEntryTone::System) line_color = system_line;
            ImGui::Bullet();
            if (!entry.segments.empty())
            {
                ImGui::SameLine();
                for (int i = 0; i < static_cast<int>(entry.segments.size()); ++i)
                {
                    const auto& seg = entry.segments[i];
                    ImGui::PushStyleColor(ImGuiCol_Text, colorForField(seg.tone, entry.tone));
                    auto localizedSegment = localizeImGuiText(seg.text);
                    ImGui::TextUnformatted(localizedSegment.c_str());
                    ImGui::PopStyleColor();
                    if (i + 1 < static_cast<int>(entry.segments.size()))
                    {
                        ImGui::SameLine(0.0f, 0.0f);
                    }
                }
            }
            else
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, line_color);
                auto localizedEntry = localizeImGuiText(entry.plainText());
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
        if (allow_close && !state.dragging && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouse_in_child)
        {
            state.dragging = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            state.dragging = false;
        }
        if (state.dragging)
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
            state.model.open = false;
            state.dragging = false;
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

    if (state.inputGuardFrames > 0)
    {
        state.inputGuardFrames--;
    }
    if (!state.model.open)
    {
        state.dragging = false;
    }
}
