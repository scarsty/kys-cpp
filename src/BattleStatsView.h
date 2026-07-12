#pragma once

#include "BattlePostBattleSummary.h"
#include "BattleReport.h"
#include "RunNode.h"

#include <string>
#include <vector>

namespace KysChess
{
class ChessGameContent;
}

class BattleStatsView : public RunNode
{
public:
    ~BattleStatsView() override;

    void setupPostBattle(
        const BattlePostBattleSummary& summary,
        const BattleReport& report,
        const KysChess::ChessGameContent& content);

    void draw() override;
    void dealEvent(EngineEvent& event) override;
    PointerResult onPointerEvent(const PointerEvent& event) override;
    void backRun() override;

private:
    struct RoleEntry
    {
        BattleUnitIdentity identity;
        std::string displayName;
        int team{};
        int damageDealt{};
        int damageTaken{};
        int kills{};
        int dps{};
        int cancelDmg{};
        std::string skill1;
        std::string skill2;
        int skill1Dmg{};
        int skill2Dmg{};
    };

    void clearBackground();
    void queueBattleLogOpen();
    void showBattleLog();
    void drawTeamTable(
        const std::vector<RoleEntry>& team,
        int x,
        int y,
        const std::string& title);

    BattlePostBattleSummary summary_;
    const BattleReport* report_{};
    std::vector<RoleEntry> allies_;
    std::vector<RoleEntry> enemies_;
    Texture* background_{};
    int battleResult_ = -1;
    int totalFrames_{};
    int battleLogOpenFrame_ = -1;
    bool battleLogShown_ = false;
    bool pointerReleaseArmed_ = false;
    bool keyboardReleaseArmed_ = false;
    bool gamepadReleaseArmed_ = false;
};
