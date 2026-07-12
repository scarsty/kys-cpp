#pragma once

#include "battle/BattlePresentation.h"

#include <map>
#include <string>
#include <vector>

namespace KysChess::Battle
{
struct BattleRuntimeUnit;
}

struct BattleReportUnitStats
{
    int damageDealt = 0;
    int damageTaken = 0;
    int kills = 0;
    std::map<int, int> damagePerSkillId;
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
    int skillId = -1;
    KysChess::Battle::BattleStatusSemanticId statusId = KysChess::Battle::BattleStatusSemanticId::None;
    KysChess::Battle::BattleResourceSemanticId resourceId = KysChess::Battle::BattleResourceSemanticId::None;
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
        const KysChess::Battle::BattleRuntimeUnit* attacker,
        const KysChess::Battle::BattleRuntimeUnit* defender,
        int damage,
        const std::string& skillName,
        int frame,
        std::vector<KysChess::Battle::BattleLogTextSegment> segments = {},
        int skillId = -1);
    void recordHeal(
        const KysChess::Battle::BattleRuntimeUnit* source,
        const KysChess::Battle::BattleRuntimeUnit* target,
        int amount,
        std::vector<KysChess::Battle::BattleLogTextSegment> segments,
        int frame,
        KysChess::Battle::BattleResourceSemanticId resourceId = KysChess::Battle::BattleResourceSemanticId::HitPoints);
    void recordStatus(
        const KysChess::Battle::BattleRuntimeUnit* source,
        const KysChess::Battle::BattleRuntimeUnit* target,
        KysChess::Battle::BattleLogCategory category,
        KysChess::Battle::BattleLogPerspective perspective,
        std::vector<KysChess::Battle::BattleLogTextSegment> segments,
        int frame,
        KysChess::Battle::BattleStatusSemanticId statusId = KysChess::Battle::BattleStatusSemanticId::None,
        KysChess::Battle::BattleResourceSemanticId resourceId = KysChess::Battle::BattleResourceSemanticId::None);
    void recordKill(const KysChess::Battle::BattleRuntimeUnit* killer, const KysChess::Battle::BattleRuntimeUnit* victim, int frame);
    void recordDeath(const KysChess::Battle::BattleRuntimeUnit* unit, int frame);
    void recordProjectileCancel(int unitId, int damage);
    void recordBattleEnd(int frame, int battleResult);

    const BattleReport& report() const { return report_; }

private:
    BattleReport report_;
};
