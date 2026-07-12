#include "BattleStatsView.h"

#include "BattleLogPresenter.h"
#include "ChessGameContent.h"
#include "Engine.h"
#include "Font.h"
#include "TextureManager.h"

#include <algorithm>
#include <cassert>
#include <format>

BattleStatsView::~BattleStatsView()
{
    clearBackground();
}

void BattleStatsView::clearBackground()
{
    if (background_)
    {
        Engine::destroyTexture(background_);
        background_ = nullptr;
    }
}

void BattleStatsView::setupPostBattle(
    const BattlePostBattleSummary& summary,
    const BattleReport& report,
    const KysChess::ChessGameContent& content)
{
    full_window_ = 1;
    summary_ = summary;
    report_ = &report;
    battleResult_ = summary.battleResult;
    totalFrames_ = report.battleEndFrame();
    clearBackground();
    background_ = Engine::getInstance()->cloneTexture(Engine::getInstance()->getMainTexture());
    battleLogShown_ = false;
    battleLogOpenFrame_ = -1;
    pointerReleaseArmed_ = false;
    keyboardReleaseArmed_ = false;
    gamepadReleaseArmed_ = false;
    allies_.clear();
    enemies_.clear();

    const auto makeEntry = [&](const BattlePostBattleUnitSummary& unit) {
        RoleEntry entry;
        entry.identity = unit.identity;
        entry.displayName = unit.identity.name;
        entry.team = unit.identity.team;
        entry.cancelDmg = unit.cancelDmg;
        if (const auto found = report.stats().find(unit.identity.battleId); found != report.stats().end())
        {
            const auto& stats = found->second;
            entry.damageDealt = stats.damageDealt;
            entry.damageTaken = stats.damageTaken;
            entry.kills = stats.kills;
            const int lastActiveFrame = stats.lastActiveFrame > 0
                ? stats.lastActiveFrame
                : totalFrames_;
            if (entry.damageDealt > 0 && lastActiveFrame > 0)
            {
                entry.dps = entry.damageDealt * 60 / lastActiveFrame;
            }
            for (const auto& [skillId, damage] : stats.damagePerSkillId)
            {
                const auto* magic = content.magic(skillId);
                const auto name = magic ? magic->Name : std::format("武學{}", skillId);
                if (damage > entry.skill1Dmg)
                {
                    entry.skill2 = std::move(entry.skill1);
                    entry.skill2Dmg = entry.skill1Dmg;
                    entry.skill1 = name;
                    entry.skill1Dmg = damage;
                }
                else if (damage > entry.skill2Dmg)
                {
                    entry.skill2 = name;
                    entry.skill2Dmg = damage;
                }
            }
        }
        return entry;
    };

    for (const auto& unit : summary.allies)
    {
        allies_.push_back(makeEntry(unit));
    }
    for (const auto& unit : summary.enemies)
    {
        enemies_.push_back(makeEntry(unit));
    }
    const auto byDamage = [](const RoleEntry& lhs, const RoleEntry& rhs) {
        return lhs.damageDealt > rhs.damageDealt;
    };
    std::ranges::sort(allies_, byDamage);
    std::ranges::sort(enemies_, byDamage);
}

void BattleStatsView::queueBattleLogOpen()
{
    if (!battleLogShown_ && battleLogOpenFrame_ < 0)
    {
        battleLogOpenFrame_ = current_frame_ + 1;
    }
}

void BattleStatsView::showBattleLog()
{
    assert(report_);
    Engine::getInstance()->showBattleLogOverlay(
        BattleLogPresenter().present(summary_, *report_));
}

void BattleStatsView::drawTeamTable(
    const std::vector<RoleEntry>& team,
    int x,
    int y,
    const std::string& title)
{
    auto* font = Font::getInstance();
    const Color white{255, 255, 255, 255};
    const Color gray{180, 180, 180, 255};
    constexpr int kFontSize = 20;
    constexpr int kRowHeight = 44;
    constexpr int kNameX = 46;
    constexpr int kDamageX = 130;
    constexpr int kDpsX = 200;
    constexpr int kTakenX = 250;
    constexpr int kKillsX = 310;
    constexpr int kCancelX = 340;
    constexpr int kSkillX = 400;

    font->draw(title, 24, x, y, {255, 215, 0, 255});
    y += 30;
    font->draw("名稱", kFontSize, x + kNameX, y, gray);
    font->draw("輸出", kFontSize, x + kDamageX, y, gray);
    font->draw("DPS", kFontSize, x + kDpsX, y, gray);
    font->draw("承傷", kFontSize, x + kTakenX, y, gray);
    font->draw("殺", kFontSize, x + kKillsX, y, gray);
    font->draw("抵消", kFontSize, x + kCancelX, y, gray);
    font->draw("技能", kFontSize - 2, x + kSkillX, y, gray);
    y += 26;

    for (const auto& entry : team)
    {
        if (auto* texture = TextureManager::getInstance()->getTexture("head", entry.identity.headId))
        {
            TextureManager::getInstance()->renderTexture(
                texture,
                x,
                y - 2,
                TextureManager::RenderInfo{white, 255, 0.22, 0.22});
        }
        font->draw(
            entry.displayName,
            kFontSize,
            x + kNameX,
            y,
            entry.team == 0 ? Color{100, 255, 100, 255} : Color{255, 100, 100, 255});
        font->draw(std::to_string(entry.damageDealt), kFontSize, x + kDamageX, y, white);
        font->draw(std::to_string(entry.dps), kFontSize, x + kDpsX, y, {255, 200, 50, 255});
        font->draw(std::to_string(entry.damageTaken), kFontSize, x + kTakenX, y, white);
        font->draw(std::to_string(entry.kills), kFontSize, x + kKillsX, y, white);
        if (entry.cancelDmg > 0)
        {
            font->draw(std::to_string(entry.cancelDmg), kFontSize, x + kCancelX, y, {200, 200, 255, 255});
        }
        if (!entry.skill1.empty())
        {
            font->draw(
                std::format("{}({})", entry.skill1, entry.skill1Dmg),
                kFontSize - 4,
                x + kSkillX,
                y,
                gray);
        }
        if (!entry.skill2.empty())
        {
            font->draw(
                std::format("{}({})", entry.skill2, entry.skill2Dmg),
                kFontSize - 4,
                x + kSkillX,
                y + 20,
                gray);
        }
        y += kRowHeight;
    }
}

void BattleStatsView::draw()
{
    auto* engine = Engine::getInstance();
    const int width = engine->getUIWidth();
    const int height = engine->getUIHeight();
    if (background_)
    {
        engine->renderTexture(background_, 0, 0, width, height);
    }
    if (battleLogShown_ || battleLogOpenFrame_ >= 0)
    {
        engine->fillColor({0, 0, 0, 120}, 0, 0, width, height);
        return;
    }

    engine->fillColor({0, 0, 0, 200}, 0, 0, width, height);
    auto* font = Font::getInstance();
    const auto centeredX = [&](const std::string& text, int fontSize) {
        return width / 2 - fontSize * Font::getTextDrawSize(text) / 4;
    };

    font->draw(std::format("戰鬥幀數 {}", totalFrames_), 18, 20, 10, {180, 180, 180, 255});
    const std::string title = battleResult_ == 0 ? "戰鬥勝利" : "戰鬥失敗";
    font->draw(
        title,
        28,
        centeredX(title, 28),
        10,
        battleResult_ == 0 ? Color{100, 255, 100, 255} : Color{255, 100, 100, 255});

    const int halfWidth = width / 2;
    drawTeamTable(allies_, 10, 45, "我方戰績");
    drawTeamTable(enemies_, halfWidth + 10, 45, "敵方戰績");

    const std::string footer = battleLogShown_
        ? "關閉戰鬥日誌後繼續"
        : "按任意鍵查看戰鬥日誌";
    font->draw(footer, 20, centeredX(footer, 20), height - 35, {200, 200, 200, 255});
}

void BattleStatsView::dealEvent(EngineEvent& event)
{
    if (battleLogShown_ || battleLogOpenFrame_ >= 0)
    {
        return;
    }
    if (event.type == EVENT_KEY_DOWN && (event.key.key == K_RETURN || event.key.key == K_SPACE))
    {
        keyboardReleaseArmed_ = true;
        return;
    }
    if (event.type == EVENT_KEY_UP && (event.key.key == K_RETURN || event.key.key == K_SPACE))
    {
        if (keyboardReleaseArmed_)
        {
            keyboardReleaseArmed_ = false;
            queueBattleLogOpen();
        }
        return;
    }
    if (event.type == EVENT_GAMEPAD_BUTTON_DOWN && event.gbutton.button == GAMEPAD_BUTTON_SOUTH)
    {
        gamepadReleaseArmed_ = true;
        return;
    }
    if (event.type == EVENT_GAMEPAD_BUTTON_UP && event.gbutton.button == GAMEPAD_BUTTON_SOUTH)
    {
        if (gamepadReleaseArmed_)
        {
            gamepadReleaseArmed_ = false;
            queueBattleLogOpen();
        }
    }
}

RunNode::PointerResult BattleStatsView::onPointerEvent(const PointerEvent& event)
{
    if (event.button != SDL_BUTTON_LEFT)
    {
        return RunNode::onPointerEvent(event);
    }
    if (event.phase == PointerPhase::ButtonDown)
    {
        if (!event.insidePresent)
        {
            return PointerResult::Ignored;
        }
        pointerReleaseArmed_ = true;
        return PointerResult::Captured;
    }
    if (event.phase == PointerPhase::ButtonUp)
    {
        if (pointerReleaseArmed_ && !battleLogShown_ && battleLogOpenFrame_ < 0)
        {
            queueBattleLogOpen();
        }
        pointerReleaseArmed_ = false;
        return PointerResult::Handled;
    }
    if (event.phase == PointerPhase::Cancel)
    {
        pointerReleaseArmed_ = false;
    }
    return PointerResult::Handled;
}

void BattleStatsView::backRun()
{
    if (!battleLogShown_ && battleLogOpenFrame_ >= 0 && current_frame_ >= battleLogOpenFrame_)
    {
        battleLogShown_ = true;
        battleLogOpenFrame_ = -1;
        showBattleLog();
    }
    if (battleLogShown_ && !Engine::getInstance()->isBattleLogOverlayOpen())
    {
        setExit(true);
    }
}
