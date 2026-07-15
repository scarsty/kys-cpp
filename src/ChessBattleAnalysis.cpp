#include "ChessBattleAnalysis.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <format>
#include <map>
#include <ranges>
#include <string_view>

namespace KysChess
{
namespace
{

std::string battleEventText(const BattleReportEvent& event)
{
    std::string result = event.skillName;
    for (const auto& segment : event.segments)
    {
        result += segment.text;
    }
    return result.empty() ? "未標記效果" : result;
}

bool containsText(std::string_view text, std::string_view needle)
{
    return text.find(needle) != std::string_view::npos;
}

int eventDurationFrames(const BattleReportEvent& event)
{
    for (const auto& segment : event.segments)
    {
        if (segment.tone != Battle::BattleLogTextTone::DurationValue)
        {
            continue;
        }
        int value{};
        const auto parsed = std::from_chars(
            segment.text.data(),
            segment.text.data() + segment.text.size(),
            value);
        if (parsed.ec == std::errc{})
        {
            return value;
        }
    }
    return 0;
}

struct UnitCombatAggregate
{
    int healingDone{};
    int magicPointsRestored{};
    int magicPointsDrained{};
    int magicPointsDrainEvents{};
    int blocks{};
    int dodges{};
    int poisonPayloadEvents{};
    int poisonApplicationEvents{};
    int poisonTicks{};
    int poisonDamage{};
    int bleedApplications{};
    int hitstunApplications{};
    int hitstunDurationFrames{};
    int stunApplications{};
    int stunDurationFrames{};
    int knockbackApplications{};
    int magicBlockApplications{};
    int magicBlockDurationFrames{};
    int cooldownManipulations{};
    int cooldownManipulationFrames{};
    int invulnerabilityTriggers{};
    int deathPreventionTriggers{};
    ChessBattleDamageBreakdown damage;
    std::map<std::string, int> nonSkillDamageSources;
    std::map<std::string, int> nonSkillDamageSourceCounts;
};

void addDamageCategory(
    UnitCombatAggregate& aggregate,
    const BattleReportEvent& event,
    const ChessGameContent& content)
{
    const auto text = battleEventText(event);
    if (containsText(text, "中毒"))
    {
        ++aggregate.poisonTicks;
        aggregate.poisonDamage += event.value;
    }
    if (event.skillId >= 0)
    {
        aggregate.damage.skill += event.value;
        return;
    }
    aggregate.nonSkillDamageSources[text] += event.value;
    ++aggregate.nonSkillDamageSourceCounts[text];
    if (containsText(text, "中毒") || containsText(text, "流血"))
    {
        aggregate.damage.status += event.value;
        return;
    }
    if (containsText(text, "殉爆")
        || std::ranges::any_of(content.combos(), [&](const auto& combo) {
            return containsText(text, combo.name);
        }))
    {
        aggregate.damage.combo += event.value;
        return;
    }
    if (std::ranges::any_of(content.equipment(), [&](const auto& equipment) {
            const auto* item = content.item(equipment.itemId);
            return item && containsText(text, item->name);
        }))
    {
        aggregate.damage.equipment += event.value;
        return;
    }
    if (text == "未標記效果" || containsText(text, "普通攻擊"))
    {
        aggregate.damage.basicAttack += event.value;
        return;
    }
    aggregate.damage.other += event.value;
}

std::string battleEffectType(const BattleReportEvent& event)
{
    const auto label = battleEventText(event);
    if (event.category == Battle::BattleLogCategory::Cast) return "ability_cast";
    if (event.category == Battle::BattleLogCategory::ProjectileCancel) return "projectile_cancelled";
    if (event.type == BattleReportEventType::Heal
        && event.resourceId == Battle::BattleResourceSemanticId::MagicPoints)
    {
        return "magic_points_restored";
    }
    switch (event.statusId)
    {
    case Battle::BattleStatusSemanticId::Hitstun: return "hitstun_applied";
    case Battle::BattleStatusSemanticId::Stun: return "stun_applied";
    case Battle::BattleStatusSemanticId::Poison: return "poison_applied";
    case Battle::BattleStatusSemanticId::Bleed: return "bleed_applied";
    case Battle::BattleStatusSemanticId::DamageReduceDebuff: return "damage_reduction_applied";
    case Battle::BattleStatusSemanticId::MpBlocked: return "magic_block_applied";
    case Battle::BattleStatusSemanticId::BlockedByInvincible: return "blocked_by_invulnerability";
    case Battle::BattleStatusSemanticId::BlockedByFirstHit: return "blocked_by_first_hit";
    case Battle::BattleStatusSemanticId::DeathPrevented: return "death_prevented";
    case Battle::BattleStatusSemanticId::ExecuteTriggered: return "execute_triggered";
    case Battle::BattleStatusSemanticId::Knockback: return "knockback_applied";
    case Battle::BattleStatusSemanticId::EnemyTopDebuff: return "enemy_top_debuff_changed";
    case Battle::BattleStatusSemanticId::MagicPointsDrained: return "magic_points_drained";
    case Battle::BattleStatusSemanticId::PoisonPayload: return "poison_payload";
    case Battle::BattleStatusSemanticId::None: break;
    }
    if (event.resourceId == Battle::BattleResourceSemanticId::Cooldown) return "cooldown_changed";
    if (containsText(label, "閃避")) return "dodge";
    if (containsText(label, "格擋")) return "block";
    if (containsText(label, "無敵")) return "invulnerability";
    return "status_effect";
}

ChessBattleEffectActivation battleEffectActivation(const BattleReportEvent& event)
{
    ChessBattleEffectActivation result;
    result.type = battleEffectType(event);
    result.description = battleEventText(event);
    result.frame = event.frame;
    if (event.sourceId >= 0) result.sourceUnitId = event.sourceId;
    if (!event.sourceName.empty()) result.sourceName = event.sourceName;
    if (event.sourceTeam >= 0) result.sourceTeam = event.sourceTeam;
    if (!event.sourceKind.empty()) result.sourceKind = event.sourceKind;
    if (event.targetId >= 0)
    {
        result.targetUnitId = event.targetId;
        result.targetName = event.targetName;
        result.targetTeam = event.targetTeam;
    }
    if (event.category == Battle::BattleLogCategory::Cast)
    {
        if (event.skillId >= 0) result.abilityId = event.skillId;
        if (!event.skillName.empty()) result.abilityName = event.skillName;
    }
    if (event.statusId == Battle::BattleStatusSemanticId::PoisonPayload)
    {
        result.poisonPercent = event.value;
        result.scheduledTicks = event.secondaryValue;
    }
    else if (event.statusId == Battle::BattleStatusSemanticId::Poison)
    {
        result.poisonPercent = event.value;
    }
    if (event.category == Battle::BattleLogCategory::ProjectileCancel)
    {
        result.sourceProjectileId = event.effectId;
        result.opposingProjectileId = event.secondaryEffectId;
        result.sourceValueBefore = event.value;
        result.opposingValueBefore = event.secondaryValue;
        result.cancelledPotentialDamage = std::min(event.value, event.secondaryValue);
        result.sourceValueAfter = std::max(0, event.value - event.secondaryValue);
        result.opposingValueAfter = std::max(0, event.secondaryValue - event.value);
        return result;
    }
    if (event.statusId == Battle::BattleStatusSemanticId::EnemyTopDebuff)
    {
        result.previousValue = event.previousValue;
        result.newValue = event.newValue;
        result.delta = event.value;
        return result;
    }
    if (event.value != 0) result.value = event.value;
    const int durationFrames = event.statusId == Battle::BattleStatusSemanticId::Knockback
        ? event.secondaryValue
        : eventDurationFrames(event);
    if (durationFrames > 0) result.durationFrames = durationFrames;
    return result;
}

void addCombatEffect(UnitCombatAggregate& aggregate, const BattleReportEvent& event)
{
    const auto label = battleEventText(event);
    if (event.statusId == Battle::BattleStatusSemanticId::Poison) ++aggregate.poisonApplicationEvents;
    if (event.statusId == Battle::BattleStatusSemanticId::PoisonPayload) ++aggregate.poisonPayloadEvents;
    if (event.statusId == Battle::BattleStatusSemanticId::MagicPointsDrained)
    {
        aggregate.magicPointsDrained += event.value;
        ++aggregate.magicPointsDrainEvents;
    }
    if (event.statusId == Battle::BattleStatusSemanticId::Bleed) ++aggregate.bleedApplications;
    if (event.statusId == Battle::BattleStatusSemanticId::Hitstun)
    {
        ++aggregate.hitstunApplications;
        aggregate.hitstunDurationFrames += eventDurationFrames(event);
    }
    if (event.statusId == Battle::BattleStatusSemanticId::Stun)
    {
        ++aggregate.stunApplications;
        aggregate.stunDurationFrames += eventDurationFrames(event);
    }
    if (event.statusId == Battle::BattleStatusSemanticId::Knockback) ++aggregate.knockbackApplications;
    if (event.statusId == Battle::BattleStatusSemanticId::MpBlocked)
    {
        ++aggregate.magicBlockApplications;
        aggregate.magicBlockDurationFrames += eventDurationFrames(event);
    }
    if (event.resourceId == Battle::BattleResourceSemanticId::Cooldown)
    {
        ++aggregate.cooldownManipulations;
        aggregate.cooldownManipulationFrames += event.value;
    }
    if (event.statusId == Battle::BattleStatusSemanticId::DeathPrevented
        || containsText(label, "死亡庇護")
        || containsText(label, "免死"))
    {
        ++aggregate.deathPreventionTriggers;
    }
    if (event.statusId == Battle::BattleStatusSemanticId::BlockedByFirstHit
        || containsText(label, "格擋"))
    {
        ++aggregate.blocks;
    }
    if (containsText(label, "閃避")) ++aggregate.dodges;
    if (event.statusId == Battle::BattleStatusSemanticId::BlockedByInvincible
        || event.resourceId == Battle::BattleResourceSemanticId::Invincibility
        || containsText(label, "無敵"))
    {
        ++aggregate.invulnerabilityTriggers;
    }
}

}  // namespace

std::string chessBattleTeamName(int team)
{
    return team == 0 ? "我方" : "敵方";
}

std::string chessBattleOutcomeDescription(Battle::BattleOutcome outcome)
{
    switch (outcome)
    {
    case Battle::BattleOutcome::InProgress: return "尚未完成";
    case Battle::BattleOutcome::PlayerVictory: return "我方勝利";
    case Battle::BattleOutcome::PlayerDefeat: return "我方戰敗";
    case Battle::BattleOutcome::Timeout: return "超過戰鬥時間上限";
    }
    std::unreachable();
}

ChessBattleResultAnalysis analyzeChessBattleResult(
    const ChessGameContent& content,
    const PreparedChessBattle& prepared,
    const HeadlessBattleResult& battle)
{
    ChessBattleResultAnalysis result;
    result.outcome = battle.summary.outcome;
    result.outcomeDescription = chessBattleOutcomeDescription(battle.summary.outcome);
    result.endFrame = battle.summary.endFrame;
    result.digest = battle.digest;
    for (const auto& survivor : battle.summary.survivors)
    {
        const auto* role = content.role(survivor.roleId);
        assert(role);
        result.survivors.push_back({
            survivor.unitId,
            survivor.chessInstanceId,
            survivor.roleId,
            role->Name,
            survivor.team,
            survivor.hp,
            survivor.mp,
            survivor.summoned,
        });
    }

    std::map<int, UnitCombatAggregate> combatByUnit;
    for (const auto& event : battle.report.events())
    {
        if (event.type == BattleReportEventType::Damage && event.sourceId >= 0)
        {
            addDamageCategory(combatByUnit[event.sourceId], event, content);
        }
        else if (event.type == BattleReportEventType::Heal && event.sourceId >= 0)
        {
            if (event.resourceId == Battle::BattleResourceSemanticId::MagicPoints)
            {
                result.effectActivations.push_back(battleEffectActivation(event));
                combatByUnit[event.sourceId].magicPointsRestored += event.value;
            }
            else if (event.resourceId == Battle::BattleResourceSemanticId::HitPoints)
            {
                combatByUnit[event.sourceId].healingDone += event.value;
            }
        }
        else if (event.type == BattleReportEventType::Status)
        {
            result.effectActivations.push_back(battleEffectActivation(event));
            const int unitId = event.sourceId >= 0 ? event.sourceId : event.targetId;
            if (unitId >= 0)
            {
                addCombatEffect(combatByUnit[unitId], event);
            }
        }
    }

    int allyDamage{};
    int enemyDamage{};
    const PreparedChessBattleUnit* topUnit = nullptr;
    int topDamage = -1;
    for (const auto& unit : prepared.units)
    {
        const auto* role = content.role(unit.roleId);
        assert(role);
        ChessBattleUnitAnalysis stats;
        stats.unitId = unit.unitId;
        stats.roleId = unit.roleId;
        stats.name = role->Name;
        stats.team = unit.team;
        stats.star = unit.star;
        const auto baseline = chessPreparedUnitBaselineStats(content, unit);
        const auto initialized = std::ranges::find(
            battle.initialization.roleDeltas,
            unit.unitId,
            &Battle::BattleInitializationRoleDelta::unitId);
        assert(initialized != battle.initialization.roleDeltas.end());
        stats.initialCombatStats = chessInitializedCombatStats(*initialized);
        stats.initialStatDeltaFromSpecialEffects = chessStatDelta(
            stats.initialCombatStats,
            baseline);
        if (const auto debuff = std::ranges::find(
                battle.initialization.enemyTopDebuffs,
                unit.unitId,
                &Battle::BattleInitializationEnemyTopDebuffDelta::unitId);
            debuff != battle.initialization.enemyTopDebuffs.end())
        {
            stats.enemyAttackDebuff = debuff->attackDelta;
            stats.enemyDefenceDebuff = debuff->defenceDelta;
        }
        if (const auto found = battle.report.stats().find(unit.unitId);
            found != battle.report.stats().end())
        {
            stats.damageDealt = found->second.damageDealt;
            stats.damageTaken = found->second.damageTaken;
            stats.kills = found->second.kills;
            for (const auto& [skillId, damage] : found->second.damagePerSkillId)
            {
                const auto* magic = content.magic(skillId);
                stats.skillDamage.push_back({
                    skillId,
                    magic ? magic->Name : "一般攻擊或效果",
                    damage,
                });
            }
        }
        stats.projectilePotentialDamageCancelled =
            battle.report.projectilePotentialDamageCancelledForUnit(unit.unitId);
        stats.projectileCancellations =
            battle.report.projectileCancellationCountForUnit(unit.unitId);
        const UnitCombatAggregate emptyAggregate;
        const auto aggregateIt = combatByUnit.find(unit.unitId);
        const auto& aggregate = aggregateIt != combatByUnit.end()
            ? aggregateIt->second
            : emptyAggregate;
        stats.healingDone = aggregate.healingDone;
        stats.magicPointsRestored = aggregate.magicPointsRestored;
        stats.magicPointsDrained = aggregate.magicPointsDrained;
        stats.magicPointsDrainEvents = aggregate.magicPointsDrainEvents;
        stats.blocks = aggregate.blocks;
        stats.dodges = aggregate.dodges;
        stats.poisonPayloadEvents = aggregate.poisonPayloadEvents;
        stats.poisonApplicationEvents = aggregate.poisonApplicationEvents;
        stats.poisonTicks = aggregate.poisonTicks;
        stats.poisonDamage = aggregate.poisonDamage;
        stats.bleedApplications = aggregate.bleedApplications;
        stats.hitstunApplications = aggregate.hitstunApplications;
        stats.hitstunDurationFrames = aggregate.hitstunDurationFrames;
        stats.stunApplications = aggregate.stunApplications;
        stats.stunDurationFrames = aggregate.stunDurationFrames;
        stats.knockbackApplications = aggregate.knockbackApplications;
        stats.magicBlockApplications = aggregate.magicBlockApplications;
        stats.magicBlockDurationFrames = aggregate.magicBlockDurationFrames;
        stats.cooldownManipulations = aggregate.cooldownManipulations;
        stats.cooldownManipulationFrames = aggregate.cooldownManipulationFrames;
        stats.invulnerabilityTriggers = aggregate.invulnerabilityTriggers;
        stats.deathPreventionTriggers = aggregate.deathPreventionTriggers;
        stats.damageBreakdown = aggregate.damage;
        for (const auto& [source, damage] : aggregate.nonSkillDamageSources)
        {
            stats.nonSkillDamageSources.push_back({-1, source, damage});
        }

        const auto appendSourceImportantEffect = [&result, &unit, role](
                                                   std::string type,
                                                   int count,
                                                   int value,
                                                   std::string description)
        {
            if (count <= 0)
            {
                return;
            }
            ChessBattleImportantEffect effect;
            effect.type = std::move(type);
            effect.sourceUnitId = unit.unitId;
            effect.sourceName = role->Name;
            effect.sourceTeam = unit.team;
            effect.count = count;
            if (value != 0) effect.value = value;
            effect.description = std::move(description);
            result.importantEffects.push_back(std::move(effect));
        };
        if (stats.enemyAttackDebuff != 0 || stats.enemyDefenceDebuff != 0)
        {
            ChessBattleImportantEffect effect;
            effect.type = "opening_enemy_debuff";
            effect.sourceTeam = 1 - unit.team;
            effect.sourceKind = "combo";
            effect.sourceName = "陰險";
            effect.targetUnitId = unit.unitId;
            effect.targetName = role->Name;
            effect.targetTeam = unit.team;
            effect.attackDelta = stats.enemyAttackDebuff;
            effect.defenceDelta = stats.enemyDefenceDebuff;
            effect.description = std::format(
                "陰險使[{}] {}開戰攻擊{:+}、防禦{:+}",
                chessBattleTeamName(unit.team),
                role->Name,
                stats.enemyAttackDebuff,
                stats.enemyDefenceDebuff);
            result.importantEffects.push_back(std::move(effect));
        }
        appendSourceImportantEffect(
            "projectile_cancellation",
            stats.projectileCancellations,
            stats.projectilePotentialDamageCancelled,
            std::format(
                "[{}] {}抵消 {} 點潛在彈道傷害（{} 次碰撞）",
                chessBattleTeamName(unit.team),
                role->Name,
                stats.projectilePotentialDamageCancelled,
                stats.projectileCancellations));
        appendSourceImportantEffect(
            "magic_points_drained",
            stats.magicPointsDrainEvents,
            stats.magicPointsDrained,
            std::format(
                "[{}] {}吸取 {} MP，實際回復 {} MP",
                chessBattleTeamName(unit.team),
                role->Name,
                stats.magicPointsDrained,
                stats.magicPointsRestored));
        appendSourceImportantEffect(
            "poison_activity",
            stats.poisonPayloadEvents,
            0,
            std::format(
                "[{}] {}送出 {} 次施毒效果，成功套用或強化 {} 次，觸發 {} 次毒傷，共 {} 點",
                chessBattleTeamName(unit.team),
                role->Name,
                stats.poisonPayloadEvents,
                stats.poisonApplicationEvents,
                stats.poisonTicks,
                stats.poisonDamage));
        appendSourceImportantEffect(
            "bleed_applied",
            stats.bleedApplications,
            0,
            std::format(
                "[{}] {}施加流血 {} 次",
                chessBattleTeamName(unit.team),
                role->Name,
                stats.bleedApplications));
        appendSourceImportantEffect(
            "stun_applied",
            stats.stunApplications,
            stats.stunDurationFrames,
            std::format(
                "[{}] {}施加眩暈 {} 次，共 {} 幀",
                chessBattleTeamName(unit.team),
                role->Name,
                stats.stunApplications,
                stats.stunDurationFrames));
        appendSourceImportantEffect(
            "death_prevented",
            stats.deathPreventionTriggers,
            0,
            std::format(
                "[{}] {}觸發免死 {} 次",
                chessBattleTeamName(unit.team),
                role->Name,
                stats.deathPreventionTriggers));
        for (const auto& [source, damage] : aggregate.nonSkillDamageSources)
        {
            if (containsText(source, "殉爆"))
            {
                appendSourceImportantEffect(
                    "death_explosion_damage",
                    aggregate.nonSkillDamageSourceCounts.at(source),
                    damage,
                    std::format(
                        "[{}] {}的殉爆觸發 {} 次，共造成 {} 點傷害",
                        chessBattleTeamName(unit.team),
                        role->Name,
                        aggregate.nonSkillDamageSourceCounts.at(source),
                        damage));
            }
        }
        if (unit.team == 0) allyDamage += stats.damageDealt;
        else enemyDamage += stats.damageDealt;
        if (stats.damageDealt > topDamage)
        {
            topDamage = stats.damageDealt;
            topUnit = &unit;
        }
        result.unitStats.push_back(std::move(stats));
    }

    for (const auto& event : battle.report.events())
    {
        if (event.type == BattleReportEventType::Kill)
        {
            result.keyEvents.push_back({
                event.frame,
                std::format(
                    "[{}] {} 擊倒 [{}] {}",
                    chessBattleTeamName(event.sourceTeam),
                    event.sourceName,
                    chessBattleTeamName(event.targetTeam),
                    event.targetName),
            });
        }
    }
    const int allySurvivors = static_cast<int>(std::ranges::count_if(
        battle.summary.survivors,
        [](const auto& survivor) { return survivor.team == 0 && !survivor.summoned; }));
    const int enemySurvivors = static_cast<int>(std::ranges::count_if(
        battle.summary.survivors,
        [](const auto& survivor) { return survivor.team == 1 && !survivor.summoned; }));
    const int allySummonedSurvivors = static_cast<int>(std::ranges::count_if(
        battle.summary.survivors,
        [](const auto& survivor) { return survivor.team == 0 && survivor.summoned; }));
    const int enemySummonedSurvivors = static_cast<int>(std::ranges::count_if(
        battle.summary.survivors,
        [](const auto& survivor) { return survivor.team == 1 && survivor.summoned; }));
    const int allyCount = static_cast<int>(std::ranges::count(
        prepared.units,
        0,
        &PreparedChessBattleUnit::team));
    const int enemyCount = static_cast<int>(prepared.units.size()) - allyCount;
    result.summary = std::format(
        "{}；我方存活 {}/{}，敵方存活 {}/{}；總傷害我方 {}、敵方 {}",
        result.outcomeDescription,
        allySurvivors,
        allyCount,
        enemySurvivors,
        enemyCount,
        allyDamage,
        enemyDamage);
    if (allySummonedSurvivors > 0 || enemySummonedSurvivors > 0)
    {
        result.summary += std::format(
            "；另有召喚單位存活我方 {}、敵方 {}",
            allySummonedSurvivors,
            enemySummonedSurvivors);
    }
    if (topUnit)
    {
        const auto* role = content.role(topUnit->roleId);
        assert(role);
        result.summary += std::format(
            "；全場最高輸出為[{}] {}的 {} 點",
            chessBattleTeamName(topUnit->team),
            role->Name,
            topDamage);
    }
    return result;
}

}  // namespace KysChess
