#include "HeadlessBattleRunner.h"

#include "ChessCanonicalEncoding.h"

#include <algorithm>

namespace KysChess
{
ChessSha256 HeadlessBattleRunner::digest(const HeadlessBattleResult& result)
{
    ChessCanonicalWriter writer("BTL1");
    writer.writeCollectionSize(result.digestEvents.size());
    for (const auto& event : result.digestEvents)
    {
        writer.writeU16(static_cast<std::uint16_t>(event.type));
        writer.writeI32(event.frame);
        writer.writeI32(event.sourceUnitId);
        writer.writeI32(event.targetUnitId);
        writer.writeI32(event.amount);
        writer.writeI32(event.stableEffectId);
        writer.writeI32(event.skillId);
        writer.writeI32(static_cast<int>(event.statusId));
        writer.writeI32(static_cast<int>(event.resourceId));
        writer.writeI32(event.relatedAttackId);
    }
    writer.writeU16(static_cast<std::uint16_t>(result.summary.outcome));
    writer.writeI32(result.summary.endFrame);

    std::vector<const Battle::BattleRuntimeUnitRecord*> units;
    for (const auto& record : result.finalRuntime.units.all())
    {
        units.push_back(&record);
    }
    std::ranges::sort(units, {}, [](const auto* record) { return record->id(); });
    writer.writeCollectionSize(units.size());
    for (const auto* record : units)
    {
        const auto& unit = record->core;
        const auto& status = record->status.effects;
        writer.writeI32(unit.id);
        writer.writeI32(unit.realRoleId);
        writer.writeI32(unit.team);
        writer.writeBool(unit.alive);
        writer.writeI32(unit.vitals.hp);
        writer.writeI32(unit.vitals.maxHp);
        writer.writeI32(unit.vitals.mp);
        writer.writeI32(unit.vitals.maxMp);
        writer.writeI32(unit.shield);
        writer.writeI32(unit.invincible);
        writer.writeI32(unit.stats.attack);
        writer.writeI32(unit.stats.defence);
        writer.writeI32(unit.stats.speed);
        writer.writeI32(unit.star);
        writer.writeI32(unit.chessInstanceId);
        writer.writeI32(status.poisonTimer);
        writer.writeI32(status.poisonTickPct);
        writer.writeI32(status.poisonSourceId);
        writer.writeI32(status.bleedStacks);
        writer.writeI32(status.bleedTimer);
        writer.writeI32(status.bleedSourceId);
        writer.writeI32(status.frozenTimer);
        writer.writeI32(status.frozenMaxTimer);
        writer.writeI32(status.freezeReductionPct);
        writer.writeI32(status.shieldFreezeResPct);
        writer.writeI32(status.controlImmunityFrames);
        writer.writeI32(status.mpBlockTimer);
        writer.writeI32(status.damageImmunityAfterFrames);
        writer.writeI32(status.damageImmunityDuration);
        writer.writeI32(status.damageImmunityTimer);
        writer.writeCollectionSize(status.tempAttackBuffs.size());
        for (const auto& buff : status.tempAttackBuffs)
        {
            writer.writeI32(buff.attackBonus);
            writer.writeI32(buff.remainingFrames);
        }
        writer.writeCollectionSize(status.damageReduceDebuffs.size());
        for (const auto& debuff : status.damageReduceDebuffs)
        {
            writer.writeI32(debuff.remainingFrames);
            writer.writeI32(debuff.pct);
        }
    }

    writer.writeCollectionSize(result.report.stats().size());
    for (const auto& [unitId, stats] : result.report.stats())
    {
        writer.writeI32(unitId);
        writer.writeI32(stats.damageDealt);
        writer.writeI32(stats.damageTaken);
        writer.writeI32(stats.kills);
        writer.writeI32(stats.firstDamageFrame);
        writer.writeI32(stats.lastActiveFrame);
        writer.writeCollectionSize(stats.damagePerSkillId.size());
        for (const auto& [skillId, damage] : stats.damagePerSkillId)
        {
            writer.writeI32(skillId);
            writer.writeI32(damage);
        }
    }
    return chessSha256(writer.bytes());
}

HeadlessBattleResult HeadlessBattleRunner::run(Battle::BattleRuntimeSessionCreationInput input)
{
    auto creation = Battle::BattleRuntimeSession::createInitialized(std::move(input));
    BattleReportCollector collector;
    collector.consumeInitialization(creation.initialization, creation.session);

    HeadlessBattleResult result;
    result.initialization = creation.initialization;
    while (!creation.session.runtime().result.ended)
    {
        auto frame = creation.session.runFrame();
        collector.consumeFrame(frame, creation.session);
        auto projected = Battle::battleDigestEvents(frame);
        result.digestEvents.insert(result.digestEvents.end(), projected.begin(), projected.end());
    }
    result.report = collector.report();
    result.summary = BattleSummaryBuilder::build(creation.session, result.report);
    result.finalRuntime = creation.session.runtime();
    result.digest = HeadlessBattleRunner::digest(result);
    return result;
}

}
