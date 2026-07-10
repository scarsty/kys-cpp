#pragma once

#include "BattlePostBattleSummary.h"
#include "BattleReport.h"
#include "Chess.h"
#include "ChessCombo.h"
#include "ChessManager.h"
#include "ChessRoleSave.h"
#include "RunNode.h"
#include <deque>
#include <string>
#include <vector>

class BattleStatsView : public RunNode
{
public:
    BattleStatsView(KysChess::ChessRoleSave& roleSave, KysChess::ChessManager& chessManager)
        : roleSave_(roleSave), chessManager_(chessManager)
    {
    }

    ~BattleStatsView() override;

    struct RoleEntry
    {
        BattleUnitIdentity identity;
        std::string displayName;
        int star = 1;
        int chessInstanceId = -1;
        int battleId = -1;
        int team = 0;
        int hp = 0, atk = 0, def = 0, spd = 0;
        int weaponId = -1, armorId = -1;
        std::string skillNames;
        int damageDealt = 0, damageTaken = 0, kills = 0, dps = 0, cancelDmg = 0;
        std::string skill1, skill2;
        int skill1Dmg = 0, skill2Dmg = 0;
        int hpRemaining = 0;
        int maxHpRemaining = 0;
        bool dead = false;
        std::vector<int> skillSoundIds;
        std::vector<int> skillEffectIds;
    };

    struct ComboEntry
    {
        std::string name;
        std::string thresholdName;
        int memberCount = 0;          // effective (star-augmented) count
        int physicalMemberCount = 0;  // distinct heroes actually on field
        int displayTargetCount = 0;
        bool starSynergyBonus = false;
        bool isAntiCombo = false;
    };

    void setupPreBattle(
        const std::vector<KysChess::Chess>& allies,
        const std::vector<int>& enemyIds,
        const std::vector<int>& enemyStars,
        const std::vector<KysChess::ActiveCombo>& allyCombos,
        const std::vector<KysChess::ActiveCombo>& enemyCombos,
        int battleId,
        int musicId,
        const std::vector<int>& enemyWeapons = {},
        const std::vector<int>& enemyArmors = {},
        const std::vector<int>& allyWeapons = {},
        const std::vector<int>& allyArmors = {});

    void setupPostBattle(
        const BattlePostBattleSummary& summary,
        const BattleReport& report);

    void draw() override;
    void dealEvent(EngineEvent& e) override;
    PointerResult onPointerEvent(const PointerEvent& event) override;
    void backRun() override;

private:
    KysChess::ChessRoleSave& roleSave_;
    KysChess::ChessManager& chessManager_;
    bool isPreBattle_ = true;
    int battleResult_ = 0;
    bool assetsPreloaded_ = false;
    bool loadingTextRendered_ = false;
    int musicId_ = -1;
    int battleId_ = -1;
    bool postBattleLogShown_ = false;
    int postBattleLogOpenFrame_ = -1;
    bool postBattlePointerReleaseArmed_ = false;
    bool postBattleKeyboardReleaseArmed_ = false;
    bool postBattleGamepadReleaseArmed_ = false;
    int battleLogTotalFrames_ = 0;
    const BattleReport* battleReport_ = nullptr;
    Texture* postBattleBackground_ = nullptr;

    std::vector<RoleEntry> allies_, enemies_;
    std::vector<ComboEntry> allyCombos_, enemyCombos_;

    void clearPostBattleBackground();
    void queuePostBattleLogOpen();
    void showPostBattleLog();
    void drawTeamTable(const std::vector<RoleEntry>& team, int x, int y, int w, const std::string& title, bool showPostBattle);
    void drawComboList(const std::vector<ComboEntry>& combos, int x, int y, int w);
};
