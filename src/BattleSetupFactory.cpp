#include "BattleSetupFactory.h"
#include "ChessBattleMapCatalog.h"
#include "ChessPvp.h"
#include "battle/BattleFacing.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>

namespace KysChess
{
namespace
{

int actProperty(const ChessRoleDefinition& role, int magicType)
{
    switch (magicType)
    {
    case 0: return role.Medicine;
    case 1: return role.Fist;
    case 2: return role.Sword;
    case 3: return role.Knife;
    default: return role.Unusual;
    }
}

Battle::BattleActionSkillSeed skillSeed(
    const ChessRoleDefinition& role,
    const ChessMagicDefinition* magic,
    int power)
{
    Battle::BattleActionSkillSeed result;
    if (!magic)
    {
        return result;
    }
    result.id = magic->ID;
    result.name = magic->Name;
    result.soundId = magic->SoundID;
    result.hurtType = magic->HurtType;
    result.attackAreaType = magic->AttackAreaType;
    result.magicType = magic->MagicType;
    result.visualEffectId = magic->EffectID;
    result.selectDistance = magic->SelectDistance;
    result.actProperty = actProperty(role, magic->MagicType);
    result.magicPower = power;
    return result;
}

int faceTowardsNearestOpponent(
    const PreparedChessBattleUnit& source,
    const std::vector<PreparedChessBattleUnit>& formation)
{
    int faceTowards = source.team == 0 ? Towards_RightDown : Towards_LeftUp;
    int nearestDistance = std::numeric_limits<int>::max();
    for (const auto& candidate : formation)
    {
        if (candidate.team == source.team)
        {
            continue;
        }
        const int distance = std::abs(source.x - candidate.x) + std::abs(source.y - candidate.y);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            faceTowards = Battle::faceTowardsFromGridDelta(
                candidate.x - source.x,
                candidate.y - source.y,
                faceTowards);
        }
    }
    return faceTowards;
}

void assignFormation(
    std::vector<PreparedChessBattleUnit>& units,
    const ChessBattleMapDefinition* map,
    PreparedChessBattleLayout layout)
{
    const auto* catalogMap = map ? ChessBattleMapCatalog::find(map->id) : nullptr;
    int allyIndex = 0;
    int enemyIndex = 0;
    for (auto& unit : units)
    {
        auto& index = unit.team == 0 ? allyIndex : enemyIndex;
        const int positionIndex = unit.formationSlot >= 0 ? unit.formationSlot : index;
        if (layout == PreparedChessBattleLayout::PvpArena)
        {
            assert(positionIndex >= 0 && positionIndex < kChessFormationSlotCount);
            const auto& positions = unit.team == 0
                ? ChessPvpMapLayout::localFormation()
                : ChessPvpMapLayout::opponentFormation();
            unit.x = positions[positionIndex].x;
            unit.y = positions[positionIndex].y;
        }
        else if (map)
        {
            if (unit.team == 0 && catalogMap)
            {
                assert(positionIndex < static_cast<int>(catalogMap->teammatePositions.size()));
                unit.x = catalogMap->teammatePositions[positionIndex].x;
                unit.y = catalogMap->teammatePositions[positionIndex].y;
            }
            else
            {
                const auto& xs = unit.team == 0 ? map->teammateX : map->enemyX;
                const auto& ys = unit.team == 0 ? map->teammateY : map->enemyY;
                assert(positionIndex < static_cast<int>(xs.size()));
                unit.x = xs[positionIndex];
                unit.y = ys[positionIndex];
            }
        }
        else
        {
            unit.x = unit.team == 0 ? 20 + positionIndex : 40 - positionIndex;
            unit.y = 30 + positionIndex;
        }
        ++index;
    }
}

const ChessBattleMapDefinition* battleMapDefinition(
    const PreparedChessBattle& prepared,
    const ChessGameContent& content)
{
    assert(prepared.chosenMapId >= 0 || content.battleMaps().empty()
        || (prepared.kind == PreparedChessBattleKind::Standalone && prepared.mapCandidates.empty()));
    const auto found = content.battleMaps().find(prepared.chosenMapId);
    return found == content.battleMaps().end() ? nullptr : &found->second;
}

void appendDefinitions(
    Battle::BattleRuntimeSetupSeed& setup,
    const ChessGameContent& content)
{
    for (const auto& combo : content.combos())
    {
        Battle::BattleSetupComboDefinition definition;
        definition.id = combo.id;
        definition.name = combo.name;
        definition.memberRoleIds = combo.memberRoleIds;
        definition.isAntiCombo = combo.isAntiCombo;
        definition.starSynergyBonus = combo.starSynergyBonus;
        for (const auto& threshold : combo.thresholds)
        {
            definition.thresholds.push_back({
                threshold.count,
                threshold.rules,
                sumFightWinGrowth(threshold.managementRules),
            });
        }
        setup.comboDefinitions.push_back(std::move(definition));
    }
    for (const auto& equipment : content.equipment())
    {
        setup.equipmentDefinitions.push_back({
            equipment.itemId,
            equipment.equipType,
            countsAsComboNames(equipment.managementRules),
            equipment.rules,
        });
    }
    for (const auto& synergy : content.equipmentSynergies())
    {
        setup.equipmentSynergies.push_back({
            synergy.roleIds,
            synergy.equipmentId,
            countsAsComboNames(synergy.managementRules),
            synergy.rules,
        });
    }
    for (const auto& neigong : content.neigong())
    {
        setup.neigongDefinitions.push_back({
            neigong.magicId,
            neigong.rules,
        });
    }
    setup.magicEffectDefinitions = content.magicEffects();
}

}

int BattleSetupFactory::requiredFormationSlots(
    const PreparedChessBattle& prepared,
    int team)
{
    assert(team == 0 || team == 1);
    int sequentialIndex{};
    int requiredSlots{};
    for (const auto& unit : prepared.units)
    {
        if (unit.team != team)
        {
            continue;
        }
        const int positionIndex = unit.formationSlot >= 0
            ? unit.formationSlot
            : sequentialIndex;
        requiredSlots = std::max(requiredSlots, positionIndex + 1);
        ++sequentialIndex;
    }
    return requiredSlots;
}

void BattleSetupFactory::populateBaseFormation(
    PreparedChessBattle& prepared,
    const ChessGameContent& content)
{
    assignFormation(
        prepared.units,
        battleMapDefinition(prepared, content),
        prepared.layout);
}

std::vector<PreparedChessBattleUnit> BattleSetupFactory::resolvePreparedFormation(
    const PreparedChessBattle& prepared,
    const ChessGameContent& content)
{
    auto formation = prepared.units;
    assignFormation(
        formation,
        battleMapDefinition(prepared, content),
        prepared.layout);
    for (const auto& [firstId, secondId] : prepared.formationSwaps)
    {
        auto first = std::ranges::find(formation, firstId, &PreparedChessBattleUnit::unitId);
        auto second = std::ranges::find(formation, secondId, &PreparedChessBattleUnit::unitId);
        assert(first != formation.end() && second != formation.end());
        std::swap(first->x, second->x);
        std::swap(first->y, second->y);
    }
    return formation;
}

Battle::BattleRuntimeSessionCreationInput BattleSetupFactory::build(
    const PreparedChessBattle& prepared,
    const ChessGameContent& content,
    int maximumFrames)
{
    const auto* map = battleMapDefinition(prepared, content);
    const ChessBattlefieldDefinition* battlefield = nullptr;
    if (prepared.layout == PreparedChessBattleLayout::PvpArena)
    {
        if (const auto found = content.battlefields().find(ChessPvpMapLayout::BattlefieldId);
            found != content.battlefields().end())
        {
            battlefield = &found->second;
        }
    }
    else if (map)
    {
        if (const auto found = content.battlefields().find(map->battlefieldId);
            found != content.battlefields().end())
        {
            battlefield = &found->second;
        }
    }
    BattlefieldData terrain(battlefield);

    const auto formation = resolvePreparedFormation(prepared, content);

    Battle::BattleRuntimeSessionCreationInput input;
    input.randomSeed = prepared.battleSeed;
    input.rules = Battle::makeHadesBattleRuntimeRules(
        BattlefieldData::TileWidth,
        BattlefieldData::CoordinateCount);
    input.rules.maximumFrames = maximumFrames;
    input.rules.movementCollisionWorld.walkableByCell.assign(
        BattlefieldData::CoordinateCount * BattlefieldData::CoordinateCount,
        0);
    const auto& balance = content.balance();
    input.setup.talentBattleSeed = prepared.talentBattleSeed;
    input.setup.starGrowth = {
        balance.starHPMult,
        balance.starAtkMult,
        balance.starDefMult,
        balance.starMartialMult,
        balance.starSpdMult,
        balance.starFlatHP,
        balance.starFlatAtk,
        balance.starFlatDef,
        balance.fightWinGrowthHP,
        balance.fightWinGrowthAtk,
        balance.fightWinGrowthDef,
        balance.fightWinGrowthWeapon,
        balance.fightWinGrowthSpeed,
    };
    appendDefinitions(input.setup, content);
    for (int team = 0; team < 2; ++team)
    {
        input.setup.obtainedNeigongMagicIdsByTeam[team].assign(
            prepared.obtainedNeigongIdsByTeam[team].begin(),
            prepared.obtainedNeigongIdsByTeam[team].end());
    }

    for (const auto& preparedUnit : formation)
    {
        const auto* role = content.role(preparedUnit.roleId);
        assert(role);
        Battle::BattleSetupUnitInput unit;
        unit.unitId = preparedUnit.unitId;
        unit.realRoleId = preparedUnit.roleId;
        unit.name = role->Name;
        unit.headId = role->HeadID;
        unit.team = preparedUnit.team;
        unit.sourceOrder = preparedUnit.unitId;
        unit.gridX = preparedUnit.x;
        unit.gridY = preparedUnit.y;
        unit.motion.position = terrain.worldPosition(preparedUnit.x, preparedUnit.y);
        unit.motion.acceleration = {0, 0, -4};
        unit.faceTowards = faceTowardsNearestOpponent(preparedUnit, formation);
        unit.motion.facing = Battle::realTowardsFromFaceTowards(unit.faceTowards);
        unit.vitals = {role->MaxHP, role->MaxHP, 0, role->MaxMP};
        unit.stats = {role->Attack, role->Defence, role->Speed};
        unit.star = preparedUnit.star;
        unit.cost = role->Cost;
        unit.weaponId = preparedUnit.weaponItemId;
        unit.armorId = preparedUnit.armorItemId;
        unit.chessInstanceId = preparedUnit.chessInstanceId;
        unit.fightsWon = preparedUnit.fightsWon;
        unit.fist = role->Fist;
        unit.sword = role->Sword;
        unit.knife = role->Knife;
        unit.unusual = role->Unusual;
        unit.hiddenWeapon = role->HiddenWeapon;
        unit.physicalPower = 90;
        unit.actPropertiesByMagicType = {
            role->Medicine,
            role->Fist,
            role->Sword,
            role->Knife,
            role->Unusual,
        };
        const auto magics = chessRoleMagicsForStar(content, *role, preparedUnit.star);
        unit.actionPlan.emplace();
        for (const auto& magic : magics)
        {
            if (!unit.skillNames.empty())
            {
                unit.skillNames += " ";
            }
            unit.skillNames += magic.first->Name;
        }
        if (!magics.empty())
        {
            unit.actionPlan->hasEquippedSkill = true;
            unit.actionPlan->normalSkill = skillSeed(
                *role,
                magics.front().first,
                magics.front().second);
            unit.actionPlan->ultimateSkill = skillSeed(
                *role,
                magics.back().first,
                magics.back().second);
        }
        input.units.push_back(std::move(unit));
    }

    for (const auto& unit : input.units)
    {
        const auto& preparedUnit = *std::ranges::find(formation, unit.unitId, &PreparedChessBattleUnit::unitId);
        input.setup.units.push_back({
            unit.unitId,
            unit.realRoleId,
            unit.team,
            unit.star,
            unit.cost,
            unit.vitals.maxHp,
            unit.stats.attack,
            unit.stats.defence,
            unit.stats.speed,
            unit.fist,
            unit.sword,
            unit.knife,
            unit.unusual,
            unit.hiddenWeapon,
        });
        auto roster = Battle::BattleSetupRosterUnit{
            unit.unitId,
            unit.realRoleId,
            unit.team,
            unit.star,
            unit.cost,
            unit.weaponId,
            unit.armorId,
            unit.chessInstanceId,
            unit.fightsWon,
            unit.sourceOrder,
            preparedUnit.amplifiedGrowthPercent,
            preparedUnit.openingMp,
            preparedUnit.openingStrengthening,
            preparedUnit.lethalRecovery,
        };
        (unit.team == 0 ? input.setup.allyRoster : input.setup.enemyRoster).push_back(roster);
        input.setup.cloneSources.push_back({
            unit.unitId,
            unit.realRoleId,
            unit.vitals.maxHp + unit.stats.attack + unit.stats.defence,
            unit.star,
            unit.chessInstanceId,
            unit.sourceOrder,
        });
    }

    const auto appendCloneCells = [&](const auto& positions, int team) {
        for (const auto& position : positions)
        {
            const bool occupied = std::ranges::any_of(input.units, [&](const auto& unit) {
                return unit.alive
                    && unit.gridX == position.x
                    && unit.gridY == position.y;
            });
            input.setup.cloneCells.push_back({
                position.x,
                position.y,
                true,
                occupied,
                team,
            });
        }
    };
    if (prepared.layout == PreparedChessBattleLayout::PvpArena)
    {
        appendCloneCells(ChessPvpMapLayout::localAdditionalSpawns(), 0);
        appendCloneCells(ChessPvpMapLayout::opponentAdditionalSpawns(), 1);
    }
    else if (map)
    {
        const auto* catalogMap = ChessBattleMapCatalog::find(map->id);
        if (catalogMap)
        {
            appendCloneCells(catalogMap->allyClonePositions, 0);

            const int enemyCount = static_cast<int>(std::ranges::count_if(input.units, [](const auto& unit) {
                return unit.team == 1;
            }));
            std::vector<Point> enemyClonePositions;
            const int enemyEnd = std::min({
                catalogMap->enemyCapacity,
                static_cast<int>(map->enemyX.size()),
                static_cast<int>(map->enemyY.size()),
            });
            for (int index = enemyCount; index < enemyEnd; ++index)
            {
                enemyClonePositions.push_back({map->enemyX[index], map->enemyY[index]});
            }
            appendCloneCells(enemyClonePositions, 1);
        }
    }

    for (int x = 0; x < BattlefieldData::CoordinateCount; ++x)
    {
        for (int y = 0; y < BattlefieldData::CoordinateCount; ++y)
        {
            const auto position = terrain.worldPosition(x, y);
            const bool walkable = terrain.canWalk(x, y);
            input.terrainCells.push_back({position, walkable});
            input.rescueCells.push_back({x, y, walkable, false, -1, position});
            input.rules.movementCollisionWorld.walkableByCell[
                static_cast<std::size_t>(y * BattlefieldData::CoordinateCount + x)] = walkable ? 1 : 0;
        }
    }
    return input;
}

}
