#include "HeadlessBattleRunner.h"

#include <algorithm>

namespace KysChess
{

struct HeadlessBattleDigestUnit
{
    int id{};
    int realRoleId{};
    int team{};
    bool alive{};
    Battle::BattleUnitVitals vitals;
    int shield{};
    int invincible{};
    Battle::BattleUnitStats stats;
    int star{};
    int chessInstanceId{};
    Battle::BattleStatusEffectState status;
};

ChessSha256 HeadlessBattleRunner::digest(const HeadlessBattleResult& result)
{
    std::vector<HeadlessBattleDigestUnit> units;
    units.reserve(result.finalRuntime.units.size());
    for (const auto& record : result.finalRuntime.units.all())
    {
        const auto& unit = record.core;
        units.push_back({
            unit.id,
            unit.realRoleId,
            unit.team,
            unit.alive,
            unit.vitals,
            unit.shield,
            unit.invincible,
            unit.stats,
            unit.star,
            unit.chessInstanceId,
            record.status.effects,
        });
    }
    std::ranges::sort(units, {}, &HeadlessBattleDigestUnit::id);
    return chessBeveSha256(
        "KYS_CHESS_BATTLE",
        result.digestEvents,
        result.summary.outcome,
        result.summary.endFrame,
        units,
        result.report.stats());
}

HeadlessBattleResult HeadlessBattleRunner::run(Battle::BattleRuntimeSessionCreationInput input)
{
    auto creation = Battle::BattleRuntimeSession::createInitialized(std::move(input));
    BattleReportCollector collector;
    collector.consumeInitialization(creation.initialization, creation.session);

    HeadlessBattleResult result;
    result.initialization = creation.initialization;
    Battle::BattlePresentationFrame frame;
    while (!creation.session.runtime().result.ended)
    {
        frame = creation.session.runFrame(std::move(frame));
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
