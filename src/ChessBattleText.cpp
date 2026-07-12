#include "ChessBattleText.h"

#include <format>
#include <map>

namespace KysChess
{
namespace
{

std::map<int, std::string> battleTokens(const PreparedChessBattle& prepared)
{
    std::map<int, std::string> result;
    int ally = 0;
    int enemy = 0;
    for (const auto& unit : prepared.units)
    {
        result.emplace(unit.unitId, unit.team == 0
            ? std::format("A{}", ++ally)
            : std::format("E{}", ++enemy));
    }
    return result;
}

std::string token(const std::map<int, std::string>& tokens, int unitId)
{
    const auto found = tokens.find(unitId);
    return found == tokens.end() ? "--" : found->second;
}

}

std::string ChessBattleText::formatHeader(
    int fight,
    const PreparedChessBattle& prepared,
    const ChessGameContent& content)
{
    const auto map = content.battleMaps().find(prepared.chosenMapId);
    const std::string mapName = map == content.battleMaps().end() || map->second.name.empty()
        ? std::format("戰場{}", prepared.chosenMapId)
        : map->second.name;
    return std::format(
        "第 {} 戰：{}  map={}  battle_seed={}\n",
        fight,
        mapName,
        prepared.chosenMapId,
        prepared.battleSeed);
}

std::string ChessBattleText::formatEvents(
    const PreparedChessBattle& prepared,
    const std::vector<Battle::BattleDigestEvent>& events)
{
    const auto tokens = battleTokens(prepared);
    std::string text;
    for (const auto& event : events)
    {
        switch (event.type)
        {
        case Battle::BattleGameplayEventType::DamageApplied:
            text += std::format(
                "[{:>5}F] {} 對 {} 造成 {} 點傷害（效果={}）\n",
                event.frame,
                token(tokens, event.sourceUnitId),
                token(tokens, event.targetUnitId),
                event.amount,
                event.stableEffectId);
            break;
        case Battle::BattleGameplayEventType::UnitDied:
            text += std::format(
                "[{:>5}F] {} 擊倒 {}\n",
                event.frame,
                token(tokens, event.sourceUnitId),
                token(tokens, event.targetUnitId));
            break;
        case Battle::BattleGameplayEventType::BattleEnded:
            text += std::format(
                "[{:>5}F] {}\n",
                event.frame,
                event.amount == 0 ? "戰鬥勝利" : "戰鬥失敗");
            break;
        case Battle::BattleGameplayEventType::ProjectileMoved:
            break;
        default:
            text += std::format(
                "[{:>5}F] 事件={} {}→{} 數值={} 效果={}\n",
                event.frame,
                static_cast<int>(event.type),
                token(tokens, event.sourceUnitId),
                token(tokens, event.targetUnitId),
                event.amount,
                event.stableEffectId);
            break;
        }
    }
    return text;
}

std::string ChessBattleText::formatSummary(const BattleSummary& summary)
{
    std::string text = std::format(
        "戰鬥結束於第 {} 幀，結果={}\n存活單位：\n",
        summary.endFrame,
        static_cast<int>(summary.outcome));
    for (const auto& survivor : summary.survivors)
    {
        text += std::format(
            "  unit={} role={} team={} HP={} MP={}\n",
            survivor.unitId,
            survivor.roleId,
            survivor.team,
            survivor.hp,
            survivor.mp);
    }
    return text;
}

}
