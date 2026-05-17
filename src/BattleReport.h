#pragma once

#include "battle/BattlePresentation.h"
#include "BattlePostBattleSummary.h"

#include <map>
#include <string>
#include <vector>

struct BattleReportUnitStats
{
    int damageDealt = 0;
    int damageTaken = 0;
    int kills = 0;
    std::map<std::string, int> damagePerSkill;
    int firstDamageFrame = 0;
    int lastActiveFrame = 0;
};

enum class BattleReportEventType
{
    Damage,
    Heal,
    Status,
    Kill,
    Death,
    BattleEnd
};

struct BattleReportEvent
{
    BattleReportEventType type = BattleReportEventType::Damage;
    int frame = 0;
    int sourceId = -1;
    int targetId = -1;
    int sourceTeam = -1;
    int targetTeam = -1;
    int value = 0;
    KysChess::Battle::BattleLogCategory category = KysChess::Battle::BattleLogCategory::Status;
    KysChess::Battle::BattleLogPerspective perspective = KysChess::Battle::BattleLogPerspective::Targeted;
    std::string sourceName;
    std::string targetName;
    std::string skillName;
    std::vector<KysChess::Battle::BattleLogTextSegment> segments;
};

class BattleReport
{
public:
    const std::map<int, BattleReportUnitStats>& stats() const { return stats_; }
    const std::vector<BattleReportEvent>& events() const { return events_; }
    int battleEndFrame() const { return battleEndFrame_; }
    int battleResult() const { return battleResult_; }
    int cancelDamageForUnit(int unitId) const;

private:
    friend class BattleReportBuilder;

    std::map<int, BattleReportUnitStats> stats_;
    std::map<int, int> cancelDamageByUnit_;
    std::vector<BattleReportEvent> events_;
    int battleEndFrame_ = 0;
    int battleResult_ = -1;
};

class BattleReportBuilder
{
public:
    void recordDamage(
        const BattleUnitIdentity* attacker,
        const BattleUnitIdentity* defender,
        int damage,
        const std::string& skillName,
        int frame,
        std::vector<KysChess::Battle::BattleLogTextSegment> segments = {});
    void recordHeal(
        const BattleUnitIdentity* source,
        const BattleUnitIdentity* target,
        int amount,
        std::vector<KysChess::Battle::BattleLogTextSegment> segments,
        int frame);
    void recordStatus(
        const BattleUnitIdentity* source,
        const BattleUnitIdentity* target,
        KysChess::Battle::BattleLogCategory category,
        KysChess::Battle::BattleLogPerspective perspective,
        std::vector<KysChess::Battle::BattleLogTextSegment> segments,
        int frame);
    void recordKill(const BattleUnitIdentity* killer, const BattleUnitIdentity* victim, int frame);
    void recordDeath(const BattleUnitIdentity* unit, int frame);
    void recordProjectileCancel(int unitId, int damage);
    void recordBattleEnd(int frame, int battleResult);

    const BattleReport& report() const { return report_; }

private:
    BattleReport report_;
};
