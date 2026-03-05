#pragma once

#include "Chess.h"
#include "ChessCombo.h"
#include "RunNode.h"
#include <deque>
#include <map>
#include <string>
#include <vector>

struct RoleBattleStats
{
    int damageDealt = 0;
    int damageTaken = 0;
    int kills = 0;
    std::map<std::string, int> damagePerSkill;
    int firstDamageFrame = 0;
    int lastActiveFrame = 0;
};

class BattleTracker
{
public:
    void recordDamage(Role* attacker, Role* defender, int damage, const std::string& skillName, int frame);
    void recordKill(Role* killer);
    void recordDeath(Role* role, int frame);
    void recordBattleEnd(int frame);
    const std::map<int, RoleBattleStats>& getStats() const { return stats_; }
    int getBattleEndFrame() const { return battleEndFrame_; }

private:
    std::map<int, RoleBattleStats> stats_;
    int battleEndFrame_ = 0;
};

class BattleStatsView : public RunNode
{
public:
    struct RoleEntry
    {
        Role* role = nullptr;
        int star = 1;
        int team = 0;
        int hp = 0, atk = 0, def = 0, spd = 0;
        std::string skillNames;
        int damageDealt = 0, damageTaken = 0, kills = 0, dps = 0, cancelDmg = 0;
        std::string skill1, skill2;
        int skill1Dmg = 0, skill2Dmg = 0;
    };

    struct ComboEntry
    {
        std::string name;
        std::string thresholdName;
        int memberCount = 0;          // effective (star-augmented) count
        int physicalMemberCount = 0;  // distinct heroes actually on field
        int totalMembers = 0;
        int thresholdCount = 0;       // count requirement of the active threshold
        bool starSynergyBonus = false;
        bool isAntiCombo = false;
    };

    void setupPreBattle(
        const std::vector<KysChess::Chess>& allies,
        const std::vector<int>& enemyIds,
        const std::vector<int>& enemyStars,
        const std::vector<KysChess::ActiveCombo>& allyCombos,
        const std::vector<KysChess::ActiveCombo>& enemyCombos,
        int musicId);

    void setupPostBattle(
        const std::deque<Role>& allyBattleCopies,
        const std::deque<Role>& enemyBattleCopies,
        const BattleTracker& tracker,
        int battleResult);

    void draw() override;
    void dealEvent(EngineEvent& e) override;

private:
    bool isPreBattle_ = true;
    int battleResult_ = 0;
    bool assetsPreloaded_ = false;
    bool loadingTextRendered_ = false;
    int musicId_ = -1;

    std::vector<RoleEntry> allies_, enemies_;
    std::vector<ComboEntry> allyCombos_, enemyCombos_;

    void drawTeamTable(const std::vector<RoleEntry>& team, int x, int y, int w, const std::string& title, bool showPostBattle);
    void drawComboList(const std::vector<ComboEntry>& combos, int x, int y, int w);
};
