#include "ChessPreparedBattleAnalysis.h"

#include "BattleSetupFactory.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <ranges>
#include <set>
#include <utility>

namespace KysChess
{
namespace
{

std::vector<ChessComboMetadata> activeTeamSynergies(
    const ChessGameContent& content,
    const std::vector<Battle::BattleSetupRosterUnit>& roster,
    const Battle::BattleRuntimeSetupSeed& setup)
{
    std::vector<ChessComboMetadata> result;
    for (const auto& resolved : Battle::resolveBattleSetupCombos(roster, setup))
    {
        if (resolved.activeThresholdIndex < 0)
        {
            continue;
        }
        const auto definition = std::ranges::find(
            content.combos(),
            resolved.id,
            &ComboDef::id);
        assert(definition != content.combos().end());
        result.push_back(chessComboMetadata(
            content,
            *definition,
            resolved.physicalMemberCount,
            resolved.effectiveMemberCount,
            resolved.activeThresholdIndex,
            resolved.nextThresholdIndex,
            resolved.contributions));
    }
    return result;
}

void appendSkillAssets(
    ChessPreparedBattleAssetIds& assets,
    std::set<int>& seenMagicIds,
    const Battle::BattleActionSkillSeed& skill)
{
    if (skill.id < 0 || !seenMagicIds.insert(skill.id).second)
    {
        return;
    }
    if (skill.soundId >= 0)
    {
        assets.attackSoundIds.push_back(skill.soundId);
    }
    if (skill.visualEffectId >= 0)
    {
        assets.effectIds.push_back(skill.visualEffectId);
    }
}

}  // namespace

ChessPreparedBattleAnalysis projectPreparedChessBattle(
    const PreparedChessBattle& prepared,
    const ChessGameContent& content)
{
    ChessPreparedBattleAnalysis result;
    result.identity.kind = prepared.kind;
    result.identity.stableBattleId = prepared.stableBattleId;
    result.identity.chosenMapId = prepared.chosenMapId;
    result.identity.battleSeed = prepared.battleSeed;
    if (prepared.chosenMapId >= 0)
    {
        result.identity.chosenMapName = chessBattleMapDisplayName(content, prepared.chosenMapId);
    }
    for (const int mapId : prepared.mapCandidates)
    {
        result.identity.mapCandidates.push_back({
            mapId,
            chessBattleMapDisplayName(content, mapId),
        });
    }
    const auto formation = prepared.chosenMapId < 0 && !prepared.mapCandidates.empty()
        ? prepared.units
        : BattleSetupFactory::resolvePreparedFormation(prepared, content);
    for (const auto& source : formation)
    {
        const auto* role = content.role(source.roleId);
        assert(role);
        ChessPreparedBattleUnitAnalysis unit;
        unit.unitId = source.unitId;
        unit.chessInstanceId = source.chessInstanceId;
        unit.roleId = source.roleId;
        unit.name = role->Name;
        unit.team = source.team;
        unit.star = source.star;
        unit.fightsWon = source.fightsWon;
        unit.x = source.x;
        unit.y = source.y;
        unit.headId = role->HeadID;
        unit.weaponItemId = source.weaponItemId;
        unit.weaponName = chessItemDisplayName(content, source.weaponItemId);
        unit.armorItemId = source.armorItemId;
        unit.armorName = chessItemDisplayName(content, source.armorItemId);
        result.units.push_back(std::move(unit));
    }
    return result;
}

ChessPreparedBattleAnalysis analyzePreparedChessBattle(
    const PreparedChessBattle& prepared,
    const ChessGameContent& content,
    const std::set<int>& obtainedNeigongIds,
    int maximumFrames)
{
    auto result = projectPreparedChessBattle(prepared, content);
    result.baselineStatsNote = "已計入星級、勝場成長與裝備基礎屬性；羈絆與裝備特殊效果另見隊伍羈絆及裝備說明";
    result.initializedStatsNote = "已計入星級、勝場成長、裝備基礎屬性、羈絆、內功與裝備特殊效果的開戰數值";
    for (std::size_t index = 0; index < prepared.units.size(); ++index)
    {
        const auto& source = prepared.units[index];
        auto& unit = result.units[index];
        const auto* role = content.role(source.roleId);
        assert(role);
        unit.baselineStats = chessPreparedUnitBaselineStats(content, source);
        unit.abilities = chessAbilitiesForRoleStar(content, *role, source.star);
    }

    if (prepared.chosenMapId < 0 && !prepared.mapCandidates.empty())
    {
        return result;
    }

    auto input = BattleSetupFactory::build(
        prepared,
        content,
        obtainedNeigongIds,
        maximumFrames);
    result.allySynergies = activeTeamSynergies(content, input.setup.allyRoster, input.setup);
    result.enemySynergies = activeTeamSynergies(content, input.setup.enemyRoster, input.setup);

    std::set<int> seenMagicIds;
    for (const auto& setupUnit : input.units)
    {
        auto unit = std::ranges::find(
            result.units,
            setupUnit.unitId,
            &ChessPreparedBattleUnitAnalysis::unitId);
        assert(unit != result.units.end());
        unit->x = setupUnit.gridX;
        unit->y = setupUnit.gridY;
        unit->skillNames = setupUnit.skillNames;
        appendSkillAssets(result.presentationAssets, seenMagicIds, setupUnit.normalSkill);
        appendSkillAssets(result.presentationAssets, seenMagicIds, setupUnit.ultimateSkill);
    }

    auto creation = Battle::BattleRuntimeSession::createInitialized(std::move(input));
    for (auto& unit : result.units)
    {
        const auto& runtimeUnit = creation.session.requireRuntimeUnit(unit.unitId);
        const auto delta = std::ranges::find(
            creation.initialization.roleDeltas,
            unit.unitId,
            &Battle::BattleInitializationRoleDelta::unitId);
        assert(delta != creation.initialization.roleDeltas.end());
        unit.name = runtimeUnit.name;
        unit.headId = runtimeUnit.headId;
        unit.skillNames = runtimeUnit.skillNames;
        unit.initializedCombatStats = chessInitializedCombatStats(*delta);
        unit.initializedStatDelta = chessStatDelta(
            *unit.initializedCombatStats,
            unit.baselineStats);
    }
    result.combatInitialized = true;
    return result;
}

}  // namespace KysChess
