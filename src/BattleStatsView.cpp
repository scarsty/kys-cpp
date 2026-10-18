#include "BattleStatsView.h"

#include "BattleLogPresenter.h"
#include "ChessPresentationHelpers.h"
#include "ChessUiStyle.h"

#include <algorithm>
#include <cassert>
#include <format>

using namespace KysChess;
using namespace KysChess::ChessUiStyle;

namespace
{
// 固定 1280×720 畫布上的共用欄界；兩隊使用相同的一行式表格。
constexpr std::array<int, 12> columns{0, 112, 140, 204, 252, 316, 352, 416, 514, 730, 853, 976};
constexpr int teamHeaderHeight = 20;
constexpr int summaryWidth = 240;

int textWidth(const std::string& text, int size)
{
    return Font::getTextDrawSize(text) * size / 2;
}

void fittedText(const std::string& text, int size, int x, int y, int width, Color color, bool rightAligned = false)
{
    const int units = Font::getTextDrawSize(text);
    if (units == 0) return;
    size = std::min(size, width * 2 / units);
    Font::getInstance()->draw(text, size, rightAligned ? x + width - textWidth(text, size) : x, y, color);
}

std::string synergyProgress(const ChessComboMetadata& combo)
{
    assert(!combo.thresholds.empty());
    const int threshold = combo.nextThresholdIndex >= 0 ? combo.nextThresholdIndex : static_cast<int>(combo.thresholds.size()) - 1;
    return std::format("{}/{}", combo.effectiveCount, combo.thresholds[threshold].requiredCount);
}
}

BattleStatsView::~BattleStatsView()
{
    if (background_) Engine::destroyTexture(background_);
}

void BattleStatsView::setupPostBattle(
    const BattlePostBattleSummary& summary,
    const BattleReport& report,
    const ChessGameContent& content,
    const PreparedChessBattle& prepared,
    const std::array<std::optional<ChessTalentId>, 2>& talents,
    std::string context)
{
    full_window_ = 1;
    summary_ = summary;
    report_ = &report;
    content_ = &content;
    context_ = std::move(context);
    teams_ = buildChessBattleResultTeams(summary, report, prepared, content, talents);
    if (background_) Engine::destroyTexture(background_);
    background_ = Engine::getInstance()->cloneTexture(Engine::getInstance()->getMainTexture());
    battleLogShown_ = false;
    battleLogOpenFrame_ = -1;
    focused_ = 0;
    onPointerInputReset();
}

SDL_Rect BattleStatsView::actionRect(int action) const
{
    auto* engine = Engine::getInstance();
    return {engine->getUIWidth() - 232 + action * 116, engine->getUIHeight() - 48, 100, 34};
}

int BattleStatsView::hit(SDL_FPoint position) const
{
    for (int action = 0; action < 2; ++action)
    {
        const auto r = actionRect(action);
        if (position.x >= r.x && position.x < r.x + r.w && position.y >= r.y && position.y < r.y + r.h)
            return action;
    }
    return -1;
}

void BattleStatsView::activate(int action)
{
    if (battleLogShown_ || battleLogOpenFrame_ >= 0) return;
    if (action == 0) battleLogOpenFrame_ = current_frame_ + 1;
    else if (action == 1) setExit(true);
}

void BattleStatsView::drawTeam(int side, SDL_Rect frame)
{
    auto* engine = Engine::getInstance();
    auto* font = Font::getInstance();
    const auto& team = teams_[side];
    const int x = frame.x + 10, width = frame.w - 20;
    const Color sideColor = side == 0 ? positive : negative;
    panel(frame.x, frame.y, frame.w, frame.h);
    font->draw(side == 0 ? "我方" : "敵方", 18, x, frame.y + 1, sideColor);
    const auto totals = std::format("存活 {}/{}   總輸出 {}", team.survivors, team.units.size(), team.damageDealt);
    const int totalsX = width - textWidth(totals, 14);
    font->draw(totals, 14, x + totalsX, frame.y + 3, ink);
    const int count = static_cast<int>(team.units.size());
    const int rowHeight = count == 0 ? 20 : std::min(28, (frame.h - teamHeaderHeight - 6) / count);
    int y = frame.y + teamHeaderHeight;
    int row{};
    for (const auto& entry : team.units)
    {
        const auto& unit = entry.unit;
        if (row++ % 2 == 0) engine->fillColor(quietFill, x, y, width, rowHeight - 1);
        engine->fillColor(line, x, y + rowHeight - 1, width, 1);
        const int textY = y + (rowHeight - 16) / 2;
        const int smallTextY = y + (rowHeight - 14) / 2;
        asset("head", unit.identity.headId, {x, y + (rowHeight - 18) / 2, 18, 18}, false,
            unit.dead ? Color{160, 160, 160, 255} : Color{255, 255, 255, 255});
        const auto* role = content_->role(unit.identity.realRoleId);
        assert(role);
        fittedText(unit.identity.name, 16, x + 22, textY, columns[1] - 30, chessPieceTierColor(role->Cost));
        font->draw(std::format("{}★", unit.star), 14, x + columns[1], smallTextY, gold);
        const std::array<int, 5> values{entry.damageDealt, entry.dps, entry.damageTaken, entry.kills, unit.cancelDmg};
        const std::array<Color, 5> colors{gold, ink, context, sideColor, context};
        for (int col = 0; col < 5; ++col)
        {
            fittedText(std::to_string(values[col]), 16, x + columns[col + 2], textY,
                columns[col + 3] - columns[col + 2] - 10, colors[col], true);
        }
        const std::string hp = unit.dead ? "退場" : std::format("{}/{}", unit.hpRemaining, unit.maxHpRemaining);
        fittedText(hp, 16, x + columns[7], textY, columns[8] - columns[7] - 12, unit.dead ? muted : positive, true);

        // 每件裝備預留六個中文字，武學傷害緊接名稱，避免過大的閱讀間距。
        constexpr int equipmentWidth = (columns[9] - columns[8]) / 2;
        for (int slot = 0; slot < 2; ++slot)
        {
            const int itemId = slot == 0 ? unit.weaponId : unit.armorId;
            const int itemX = x + columns[8] + slot * equipmentWidth;
            if (itemId < 0)
            {
                font->draw("—", 14, itemX, smallTextY, muted);
                continue;
            }
            asset("item", itemId, {itemX, smallTextY, 14, 14});
            fittedText(slot == 0 ? entry.weaponName : entry.armorName, 14,
                itemX + 18, smallTextY, equipmentWidth - 24, slot == 0 ? gold : context);
        }
        for (int skill = 0; skill < 2; ++skill)
        {
            if (entry.skillNames[skill].empty()) continue;
            const int skillX = x + columns[9 + skill];
            const int skillWidth = columns[10 + skill] - columns[9 + skill];
            const auto damage = std::to_string(entry.skillDamage[skill]);
            const int nameWidth = skillWidth - textWidth(damage, 14) - 14;
            const int nameSize = std::min(14, nameWidth * 2 / Font::getTextDrawSize(entry.skillNames[skill]));
            font->draw(entry.skillNames[skill], nameSize, skillX, smallTextY, muted);
            font->draw(damage, 14, skillX + textWidth(entry.skillNames[skill], nameSize) + 6, smallTextY, ink);
        }
        y += rowHeight;
    }

}

void BattleStatsView::drawTeamSummary(int side, SDL_Rect frame)
{
    auto* font = Font::getInstance();
    const auto& team = teams_[side];
    const int x = frame.x + 12, width = frame.w - 24;
    panel(frame.x, frame.y, frame.w, frame.h);
    font->draw("天賦", 14, x, frame.y + 10, muted);
    font->draw(team.talent ? chessTalentName(*team.talent) : "無", 18,
        x + 42, frame.y + 7, team.talent ? gold : muted);
    font->draw("內功", 14, x, frame.y + 36, muted);
    int y = frame.y + 54;
    for (const auto& row : wrapLines(team.neigongNames.empty() ? "無" : team.neigongNames, width * 2 / 14))
    {
        font->draw(row, 14, x, y, context);
        y += 18;
    }
    y += 8;
    font->draw("羈絆", 14, x, y, muted);
    y += 20;
    if (team.synergies.empty())
    {
        font->draw("無", 14, x, y, muted);
        return;
    }

    // 已啟動項目較多時改為多欄圖示與門檻，所有羈絆仍留在同一畫面。
    const int count = static_cast<int>(team.synergies.size());
    const int availableRows = (frame.y + frame.h - 8 - y) / 20;
    assert(availableRows > 0);
    const int columnCount = (count + availableRows - 1) / availableRows;
    const int rowsPerColumn = (count + columnCount - 1) / columnCount;
    const int columnWidth = width / columnCount;
    for (int index = 0; index < count; ++index)
    {
        const auto& combo = team.synergies[index];
        const int sx = x + index / rowsPerColumn * columnWidth;
        const int sy = y + index % rowsPerColumn * 20;
        const auto def = std::ranges::find(content_->combos(), combo.comboId, &ComboDef::id);
        assert(def != content_->combos().end());
        asset("chess-synergies", def->iconId, {sx, sy, 16, 16}, false, gold);
        const auto progress = synergyProgress(combo);
        if (columnCount == 1)
        {
            fittedText(combo.name, 14, sx + 24, sy, columnWidth - textWidth(progress, 14) - 36, gold);
            font->draw(progress, 14, sx + columnWidth - textWidth(progress, 14), sy, gold);
        }
        else
            font->draw(progress, 14, sx + 24, sy, gold);
    }
}

void BattleStatsView::draw()
{
    auto* engine = Engine::getInstance();
    auto* font = Font::getInstance();
    const int width = engine->getUIWidth(), height = engine->getUIHeight();
    if (background_) engine->renderTexture(background_, 0, 0, width, height);
    engine->fillColor(canvasScrim, 0, 0, width, height);
    panel(16, 8, width - 32, 32);
    const bool timeout = summary_.outcome == Battle::BattleOutcome::Timeout;
    const auto title = timeout ? "戰鬥超時 · 我方判負" : summary_.battleResult == 0 ? "戰鬥勝利" : "戰鬥失敗";
    font->draw(title, 22, 28, 12, summary_.battleResult == 0 ? positive : negative);
    fittedText(context_, 16, timeout ? 290 : 176, 16, width - (timeout ? 480 : 366), muted);
    const auto frames = std::format("{} 幀", report_->battleEndFrame());
    font->draw(frames, 16, width - 28 - textWidth(frames, 16), 16, gold);
    const int tableWidth = width - 44 - summaryWidth;
    const int summaryX = width - 16 - summaryWidth;
    panel(16, 42, tableWidth, 18);
    panel(summaryX, 42, summaryWidth, 18);
    font->draw("隊伍概覽", 14, summaryX + 12, 44, muted);
    const std::array<std::string, 11> headings{"棋子", "星", "輸出", "DPS", "承傷", "擊殺", "抵消", "剩餘生命", "裝備", "武學 · 傷害", "武學 · 傷害"};
    for (std::size_t col = 0; col < headings.size(); ++col)
        fittedText(headings[col], 14, 26 + columns[col], 44, columns[col + 1] - columns[col] - 10, muted, col >= 2 && col <= 7);
    const int teamHeight = (height - 120) / 2;
    for (int side = 0; side < 2; ++side)
    {
        const int y = 62 + side * (teamHeight + 6);
        drawTeam(side, {16, y, tableWidth, teamHeight});
        drawTeamSummary(side, {summaryX, y, summaryWidth, teamHeight});
    }
    font->draw("戰後總覽", 18, 64, height - 43, gold);
    font->draw("依輸出排序 · 羈絆為開戰陣容", 14, 168, height - 40, muted);
    const int hovered = hit(PointerInput::instance().logicalPointerUiPosition());
    for (int action = 0; action < 2; ++action)
    {
        const auto rect = actionRect(action);
        const ControlState state{.hovered = hovered == action, .focused = focused_ == action, .primary = action == 1};
        controlSurface(rect, state);
        asset("chess-preparation", iconId(action == 0 ? Icon::Book : Icon::Right),
            {rect.x + 12, rect.y + 7, 20, 20}, false, controlText(state));
        font->draw(action == 0 ? "日誌" : "繼續", 18, rect.x + 40, rect.y + 6, controlText(state));
    }
}

void BattleStatsView::dealEvent(EngineEvent& event)
{
    if (battleLogShown_ || battleLogOpenFrame_ >= 0) return;
    if (event.type == EVENT_KEY_DOWN)
    {
        if (event.key.key == K_LEFT) focused_ = 0;
        if (event.key.key == K_RIGHT) focused_ = 1;
        if (event.key.key == SDLK_TAB) focused_ = 1 - focused_;
        if (event.key.key == K_RETURN || event.key.key == K_SPACE) keyboardReleaseArmed_ = true;
    }
    if (event.type == EVENT_KEY_UP && (event.key.key == K_RETURN || event.key.key == K_SPACE))
    {
        if (keyboardReleaseArmed_) activate(focused_);
        keyboardReleaseArmed_ = false;
    }
    if (event.type == EVENT_GAMEPAD_BUTTON_DOWN)
    {
        if (event.gbutton.button == GAMEPAD_BUTTON_DPAD_LEFT) focused_ = 0;
        if (event.gbutton.button == GAMEPAD_BUTTON_DPAD_RIGHT) focused_ = 1;
        if (event.gbutton.button == GAMEPAD_BUTTON_SOUTH) gamepadReleaseArmed_ = true;
    }
    if (event.type == EVENT_GAMEPAD_BUTTON_UP && event.gbutton.button == GAMEPAD_BUTTON_SOUTH)
    {
        if (gamepadReleaseArmed_) activate(focused_);
        gamepadReleaseArmed_ = false;
    }
}

RunNode::PointerResult BattleStatsView::onPointerEvent(const PointerEvent& event)
{
    if (battleLogShown_ || battleLogOpenFrame_ >= 0) return PointerResult::Handled;
    if (event.button != SDL_BUTTON_LEFT) return PointerResult::Handled;
    const int target = event.insidePresent ? hit(event.uiPosition) : -1;
    if (event.phase == PointerPhase::ButtonDown)
    {
        pressed_ = target;
        if (target >= 0) focused_ = target;
        return target >= 0 ? PointerResult::Captured : PointerResult::Handled;
    }
    if (event.phase == PointerPhase::ButtonUp)
    {
        if (target >= 0 && target == pressed_) activate(target);
        pressed_ = -1;
    }
    if (event.phase == PointerPhase::Cancel) pressed_ = -1;
    return PointerResult::Handled;
}

void BattleStatsView::onPointerInputReset()
{
    pressed_ = -1;
    keyboardReleaseArmed_ = false;
    gamepadReleaseArmed_ = false;
}

void BattleStatsView::backRun()
{
    auto* engine = Engine::getInstance();
    if (!battleLogShown_ && battleLogOpenFrame_ >= 0 && current_frame_ >= battleLogOpenFrame_)
    {
        battleLogShown_ = true;
        battleLogOpenFrame_ = -1;
        onPointerInputReset();
        engine->showBattleLogOverlay(BattleLogPresenter().present(summary_, *report_));
    }
    if (battleLogShown_ && !engine->isBattleLogOverlayOpen())
    {
        battleLogShown_ = false;
        onPointerInputReset();
    }
}
