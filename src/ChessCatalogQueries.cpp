#include "ChessCatalogQueries.h"

#include "BattleStarStats.h"
#include "ChessBattleMapCatalog.h"
#include "ChessEffectDescription.h"
#include "ChessRewardRules.h"
#include "battle/BattleInitialization.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <map>
#include <ranges>
#include <utility>

namespace KysChess
{
namespace
{

const EquipmentDef& requireEquipment(const ChessGameContent& content, int itemId)
{
    const auto found = std::ranges::find(content.equipment(), itemId, &EquipmentDef::itemId);
    assert(found != content.equipment().end());
    return *found;
}

RenderedEffectDescription magicEffects(
    const ChessGameContent& content,
    int magicId,
    EffectDescriptionStyle descriptionStyle)
{
    const auto definition = std::ranges::find(
        content.magicEffects(),
        magicId,
        &ChessMagicEffectDefinition::magicId);
    if (definition == content.magicEffects().end()) return {};
    const auto& document = definition->effects;
    return describeGameplayEffects(document, descriptionStyle);
}

bool hasDescriptionRows(const RenderedEffectDescription& description)
{
    return !description.sections.empty();
}

std::string joinedRoleNames(
    const ChessGameContent& content,
    std::span<const int> roleIds)
{
    assert(!roleIds.empty());
    std::string result;
    for (std::size_t index = 0; index < roleIds.size(); ++index)
    {
        const auto* role = content.role(roleIds[index]);
        assert(role);
        if (index > 0) result += "、";
        result += role->Name;
    }
    return result;
}

std::string magicGeometry(const ChessMagicDefinition& magic)
{
    switch (magic.AttackAreaType)
    {
    case 0:
        return std::format("單體；可選擇距離 {} 格內的一個目標", magic.SelectDistance);
    case 1:
        return std::format("直線；沿選定方向延伸 {} 格", magic.SelectDistance);
    case 2:
        return std::format("十字；以施法者為中心向四方向各延伸 {} 格", magic.SelectDistance);
    case 3:
        return std::format(
            "範圍；可選擇距離 {} 格內的中心，影響中心周圍方形半徑 {} 格",
            magic.SelectDistance,
            magic.AttackDistance);
    default:
        return "特殊範圍";
    }
}

std::string magicShape(const ChessMagicDefinition& magic)
{
    switch (magic.AttackAreaType)
    {
    case 0: return "single";
    case 1: return "line";
    case 2: return "cross";
    case 3: return "area";
    default: return "special";
    }
}

}  // namespace

std::string chessBattleMapDisplayName(const ChessGameContent& content, int mapId)
{
    if (const auto curated = ChessBattleMapCatalog::displayName(mapId); !curated.empty())
    {
        return std::string(curated);
    }
    const auto found = content.battleMaps().find(mapId);
    return found != content.battleMaps().end() && !found->second.name.empty()
        ? found->second.name
        : std::format("戰場 {}", mapId);
}

std::string chessItemDisplayName(const ChessGameContent& content, int itemId)
{
    if (itemId < 0)
    {
        return {};
    }
    const auto* item = content.item(itemId);
    assert(item);
    return item->name;
}

ChessCalculatedStats chessBaseRoleStats(const ChessRoleDefinition& role)
{
    return {
        role.MaxHP,
        role.MaxMP,
        role.Attack,
        role.Defence,
        role.Speed,
        role.Fist,
        role.Sword,
        role.Knife,
        role.Unusual,
        role.HiddenWeapon,
    };
}

ChessCalculatedStats chessRoleStats(
    const ChessRoleDefinition& role,
    const BalanceConfig& balance,
    int star,
    int fightsWon,
    int amplifiedGrowthPercent)
{
    const auto stats = computeStarBoostedStats(
        {
            role.MaxHP,
            role.Attack,
            role.Defence,
            role.Speed,
            role.Fist,
            role.Sword,
            role.Knife,
            role.Unusual,
            role.HiddenWeapon,
        },
        {
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
        },
        star,
        fightsWon, 0, 0, 0, amplifiedGrowthPercent);
    return {
        stats.hp,
        role.MaxMP,
        stats.atk,
        stats.def,
        stats.spd,
        stats.fist,
        stats.sword,
        stats.knife,
        stats.unusual,
        stats.hidden,
    };
}

ChessCalculatedStats chessPieceStats(
    const ChessGameContent& content,
    const ChessSessionPiece& piece,
    int amplifiedGrowthPercent)
{
    const auto* role = content.role(piece.roleId);
    assert(role);
    return chessRoleStats(*role, content.balance(), piece.star, piece.fightsWon, amplifiedGrowthPercent);
}

ChessCalculatedStats chessPreparedUnitBaselineStats(
    const ChessGameContent& content,
    const PreparedChessBattleUnit& unit)
{
    const auto* role = content.role(unit.roleId);
    assert(role);
    return chessRoleStats(*role, content.balance(), unit.star, unit.fightsWon, unit.amplifiedGrowthPercent);
}

ChessCalculatedStats chessInitializedCombatStats(
    const Battle::BattleInitializationRoleDelta& initialized)
{
    return {
        initialized.vitals.maxHp,
        initialized.vitals.maxMp,
        initialized.stats.attack,
        initialized.stats.defence,
        initialized.stats.speed,
        initialized.fist,
        initialized.sword,
        initialized.knife,
        initialized.unusual,
        initialized.hiddenWeapon,
    };
}

ChessCalculatedStats chessStatDelta(
    const ChessCalculatedStats& initialized,
    const ChessCalculatedStats& baseline)
{
    return {
        initialized.maxHp - baseline.maxHp,
        initialized.maxMp - baseline.maxMp,
        initialized.attack - baseline.attack,
        initialized.defence - baseline.defence,
        initialized.speed - baseline.speed,
        initialized.fist - baseline.fist,
        initialized.sword - baseline.sword,
        initialized.knife - baseline.knife,
        initialized.unusual - baseline.unusual,
        initialized.hiddenWeapon - baseline.hiddenWeapon,
    };
}

ChessAbilityMetadata chessAbilityMetadata(
    const ChessGameContent& content,
    const ChessMagicDefinition& magic,
    std::vector<ChessAbilityStarPower> powerByStar,
    EffectDescriptionStyle descriptionStyle)
{
    auto effects = magicEffects(content, magic.ID, descriptionStyle);
    const bool hasConfiguredEffects = hasDescriptionRows(effects);
    return {
        magic.ID,
        magic.Name,
        std::move(powerByStar),
        magic.NeedMP,
        magicShape(magic),
        magic.SelectDistance,
        magic.AttackDistance,
        magicGeometry(magic),
        std::move(effects),
        hasConfiguredEffects
            ? std::string{}
            : "沒有額外配置的特殊效果；仍會依威力、角色屬性與範圍造成基礎武學效果",
    };
}

std::vector<ChessAbilityMetadata> chessAbilitiesForRoleStar(
    const ChessGameContent& content,
    const ChessRoleDefinition& role,
    int star,
    EffectDescriptionStyle descriptionStyle)
{
    std::vector<ChessAbilityMetadata> result;
    for (const auto& [magic, power] : chessRoleMagicsForStar(content, role, star))
    {
        result.push_back(chessAbilityMetadata(
            content,
            *magic,
            {{star, power}},
            descriptionStyle));
    }
    return result;
}

ChessRoleMetadata chessRoleMetadata(
    const ChessGameContent& content,
    int roleId,
    EffectDescriptionStyle descriptionStyle)
{
    const auto* role = content.role(roleId);
    assert(role);
    ChessRoleMetadata result;
    result.roleId = roleId;
    result.name = role->Name;
    result.cost = role->Cost;
    result.baseStats = chessBaseRoleStats(*role);
    std::map<int, std::size_t> abilityIndexes;
    for (int slot = 0; slot < ROLE_MAGIC_COUNT; ++slot)
    {
        const int magicId = role->MagicID[slot];
        if (magicId <= 0)
        {
            continue;
        }
        const int star = slot / SLOTS_PER_STAR + 1;
        if (const auto found = abilityIndexes.find(magicId); found != abilityIndexes.end())
        {
            auto& powerByStar = result.abilities[found->second].powerByStar;
            const auto existing = std::ranges::find(
                powerByStar,
                star,
                &ChessAbilityStarPower::star);
            if (existing == powerByStar.end())
            {
                powerByStar.push_back({star, role->MagicPower[slot]});
            }
            else
            {
                existing->power = std::max(existing->power, role->MagicPower[slot]);
            }
            continue;
        }
        const auto* magic = content.magic(magicId);
        assert(magic);
        abilityIndexes.emplace(magicId, result.abilities.size());
        result.abilities.push_back(chessAbilityMetadata(
            content,
            *magic,
            {{star, role->MagicPower[slot]}},
            descriptionStyle));
    }
    for (const auto& combo : content.combos())
    {
        if (std::ranges::contains(combo.memberRoleIds, roleId))
        {
            result.combos.push_back(combo.name);
        }
    }
    return result;
}

ChessEquipmentMetadata chessEquipmentMetadata(
    const ChessGameContent& content,
    int itemId,
    EffectDescriptionStyle descriptionStyle)
{
    const auto& definition = requireEquipment(content, itemId);
    const auto* item = content.item(itemId);
    assert(item);
    ChessEquipmentMetadata result;
    result.itemId = itemId;
    result.name = item->name;
    result.tier = definition.tier;
    result.equipType = definition.equipType;
    const auto& equipmentDocument = definition.effects;
    result.specialEffects = describeGameplayEffects(equipmentDocument, descriptionStyle);
    result.countsAsCombos = countsAsComboNames(definition.managementRules);
    if (!result.countsAsCombos.empty())
    {
        result.comboCountingNote = "讓裝備者視為該羈絆的一名成員；同一角色在同一羈絆只計一次，裝在原成員身上不會額外加點";
    }
    for (const auto& synergy : content.equipmentSynergies())
    {
        if (synergy.equipmentId != itemId)
        {
            continue;
        }
        ChessEquipmentCharacterBonusMetadata bonus;
        for (const int roleId : synergy.roleIds)
        {
            const auto* role = content.role(roleId);
            assert(role);
            bonus.roles.push_back(role->Name);
        }
        bonus.countsAsCombos = countsAsComboNames(synergy.managementRules);
        const auto& synergyDocument = synergy.effects;
        bonus.effects = describeGameplayEffects(synergyDocument, descriptionStyle);
        result.characterBonuses.push_back(std::move(bonus));
    }
    return result;
}

ChessComboMetadata chessComboMetadata(
    const ChessGameContent& content,
    const ComboDef& definition,
    int physicalCount,
    int effectiveCount,
    int activeThresholdIndex,
    int nextThresholdIndex,
    const std::vector<ResolvedChessComboContribution>& contributions,
    EffectDescriptionStyle descriptionStyle)
{
    ChessComboMetadata result;
    result.comboId = definition.id;
    result.name = definition.name;
    result.physicalCount = physicalCount;
    result.effectiveCount = effectiveCount;
    result.activeThresholdIndex = activeThresholdIndex;
    result.nextThresholdIndex = nextThresholdIndex;
    result.active = activeThresholdIndex >= 0;
    ChessComboProgress progress;
    progress.physicalCount = physicalCount;
    progress.effectiveCount = effectiveCount;
    progress.activeThresholdIndex = activeThresholdIndex;
    progress.nextThresholdIndex = nextThresholdIndex;
    progress.active = result.active;
    progress.isAntiCombo = definition.isAntiCombo;
    progress.starSynergyBonus = definition.starSynergyBonus;
    if (!definition.thresholds.empty())
    {
        progress.displayTargetCount = definition.isAntiCombo
            ? definition.thresholds.front().count
            : nextThresholdIndex >= 0
                ? definition.thresholds[nextThresholdIndex].count
                : definition.thresholds.back().count;
    }
    result.progressCount = formatChessComboProgressCount(progress);
    result.countExplanation = "physical_count 是不重複角色數；effective_count 再加上此羈絆允許的星級加點；裝備只讓非成員取得成員資格，不會讓同一角色重複計點";
    for (const int roleId : definition.memberRoleIds)
    {
        const auto* role = content.role(roleId);
        assert(role);
        result.members.push_back(role->Name);
    }
    for (int index = 0; index < static_cast<int>(definition.thresholds.size()); ++index)
    {
        const auto& threshold = definition.thresholds[index];
        ChessComboThresholdMetadata metadata;
        metadata.requiredCount = threshold.count;
        metadata.name = threshold.name;
        metadata.active = index <= activeThresholdIndex;
        metadata.effects = describeGameplayEffectsAndManagementRules(
            threshold.effects,
            threshold.managementRules,
            descriptionStyle);
        result.thresholds.push_back(std::move(metadata));
    }
    for (const auto& contribution : contributions)
    {
        const auto* role = content.role(contribution.roleId);
        assert(role);
        ChessComboContributionMetadata metadata;
        metadata.roleId = contribution.roleId;
        metadata.roleName = role->Name;
        metadata.unitIds = contribution.unitIds;
        metadata.countedStar = contribution.countedStar;
        metadata.physicalPoints = contribution.physicalPoints;
        metadata.starBonusPoints = contribution.starBonusPoints;
        metadata.effectivePoints = contribution.physicalPoints + contribution.starBonusPoints;
        metadata.naturalMember = contribution.naturalMember;
        for (const int equipmentItemId : contribution.equipmentItemIds)
        {
            metadata.equipmentSources.push_back({
                equipmentItemId,
                chessItemDisplayName(content, equipmentItemId),
            });
        }
        if (contribution.naturalMember && !contribution.equipmentItemIds.empty())
        {
            metadata.explanation = "角色本身已屬於羈絆；裝備亦可計作此羈絆，但不重複加點";
        }
        else if (!contribution.equipmentItemIds.empty())
        {
            metadata.explanation = "角色由裝備取得此羈絆的成員資格";
        }
        else
        {
            metadata.explanation = "角色本身屬於此羈絆";
        }
        if (contribution.starBonusPoints > 0)
        {
            metadata.explanation += std::format(
                "；{}★另提供 {} 點星級加成",
                contribution.countedStar,
                contribution.starBonusPoints);
        }
        result.contributions.push_back(std::move(metadata));
    }
    return result;
}

ChessChallengeMetadata chessChallengeMetadata(
    const ChessGameContent& content,
    const BalanceConfig::ChallengeDef& challenge)
{
    ChessChallengeMetadata result;
    result.name = challenge.name;
    result.description = challenge.description;
    const int equippedWeapons = static_cast<int>(std::ranges::count_if(
        challenge.enemies,
        [](const auto& enemy) { return enemy.weaponId >= 0; }));
    const int equippedArmor = static_cast<int>(std::ranges::count_if(
        challenge.enemies,
        [](const auto& enemy) { return enemy.armorId >= 0; }));
    std::string starSummary = "無敵人";
    if (!challenge.enemies.empty())
    {
        const auto [minimumStar, maximumStar] = std::ranges::minmax_element(
            challenge.enemies,
            {},
            &BalanceConfig::ChallengeEnemy::star);
        starSummary = minimumStar->star == maximumStar->star
            ? std::format("全 {}★", minimumStar->star)
            : std::format("{}★至 {}★", minimumStar->star, maximumStar->star);
    }
    result.summaryDescription = std::format(
        "{}；{} 名敵人；{}；武器 {}/{}、防具 {}/{}；獎勵擇一：",
        challenge.description,
        challenge.enemies.size(),
        starSummary,
        equippedWeapons,
        challenge.enemies.size(),
        equippedArmor,
        challenge.enemies.size());
    for (const auto& enemy : challenge.enemies)
    {
        const auto* role = content.role(enemy.roleId);
        assert(role);
        ChessChallengeEnemyMetadata metadata;
        metadata.roleId = enemy.roleId;
        metadata.name = role->Name;
        metadata.star = enemy.star;
        if (enemy.weaponId >= 0)
        {
            metadata.weapon = chessEquipmentMetadata(content, enemy.weaponId);
        }
        if (enemy.armorId >= 0)
        {
            metadata.armor = chessEquipmentMetadata(content, enemy.armorId);
        }
        result.enemies.push_back(std::move(metadata));
    }
    for (std::size_t index = 0; index < challenge.rewards.size(); ++index)
    {
        auto description = chessChallengeRewardDescription(content, challenge.rewards[index]);
        result.rewards.push_back(description);
        if (index > 0)
        {
            result.summaryDescription += "、";
        }
        result.summaryDescription += description;
    }
    return result;
}

const char* chessEffectCatalogSourceLabel(ChessEffectCatalogSource source)
{
    switch (source)
    {
    case ChessEffectCatalogSource::Magic: return "絕招";
    case ChessEffectCatalogSource::Equipment: return "裝備";
    case ChessEffectCatalogSource::EquipmentSynergy: return "專屬";
    case ChessEffectCatalogSource::Neigong: return "內功";
    case ChessEffectCatalogSource::ComboThreshold: return "羈絆";
    }
    std::unreachable();
}

std::vector<ChessEffectCatalogEntry> chessEffectCatalog(
    const ChessGameContent& content,
    EffectDescriptionStyle descriptionStyle)
{
    std::vector<ChessEffectCatalogEntry> result;
    const auto appendRules = [&result, descriptionStyle](
        ChessEffectCatalogSource source,
        const std::string& sourceName,
        const std::string& sourceContext,
        std::span<const GameplayEffect> definitions,
        std::span<const ChessNonBattleRule> managementRules = {})
    {
        for (std::size_t index = 0; index < definitions.size(); ++index)
        {
            const auto document = std::span<const GameplayEffect>{&definitions[index], 1};
            result.push_back({
                .source = source,
                .sourceName = sourceName,
                .sourceContext = sourceContext,
                .ruleOrdinal = index + 1,
                .sourceRuleCount = definitions.size() + managementRules.size(),
                .effects = describeGameplayEffects(document, descriptionStyle),
            });
        }
        for (std::size_t index = 0; index < managementRules.size(); ++index)
        {
            RenderedEffectDescription effects;
            appendStandaloneDescriptionRow(effects,
                chessNonBattleRuleDescription(managementRules[index],
                    descriptionStyle == EffectDescriptionStyle::Compact));
            result.push_back({
                .source = source,
                .sourceName = sourceName,
                .sourceContext = sourceContext,
                .ruleOrdinal = definitions.size() + index + 1,
                .sourceRuleCount = definitions.size() + managementRules.size(),
                .effects = std::move(effects),
            });
        }
    };

    for (const auto& definition : content.magicEffects())
    {
        appendRules(
            ChessEffectCatalogSource::Magic,
            definition.name,
            std::format("武功 ID {}", definition.magicId),
            definition.effects);
    }

    for (const auto& definition : content.equipment())
    {
        appendRules(
            ChessEffectCatalogSource::Equipment,
            chessItemDisplayName(content, definition.itemId),
            std::format("{} · {} · 裝備 ID {}",
                chessRewardTierLabel(definition.tier),
                chessEquipmentTypeName(definition.equipType),
                definition.itemId),
            definition.effects,
            definition.managementRules);
    }

    for (const auto& synergy : content.equipmentSynergies())
    {
        const auto& equipment = requireEquipment(content, synergy.equipmentId);
        const auto roles = joinedRoleNames(content, synergy.roleIds);
        appendRules(
            ChessEffectCatalogSource::EquipmentSynergy,
            // 目錄名只取裝備名：成員角色會隨配置增長，交由 sourceContext 呈現，
            // 以免一覽選單的欄寬跟著角色數爆掉。
            chessItemDisplayName(content, synergy.equipmentId),
            std::format("{}專屬 · {} · {} · 裝備 ID {}",
                roles,
                chessRewardTierLabel(equipment.tier),
                chessEquipmentTypeName(equipment.equipType),
                synergy.equipmentId),
            synergy.effects,
            synergy.managementRules);
    }

    for (const auto& definition : content.neigong())
    {
        appendRules(
            ChessEffectCatalogSource::Neigong,
            definition.name,
            std::format("{} · 武功 ID {}",
                chessRewardTierLabel(definition.tier),
                definition.magicId),
            definition.effects);
    }

    for (const auto& combo : content.combos())
    {
        for (const auto& threshold : combo.thresholds)
        {
            appendRules(
                ChessEffectCatalogSource::ComboThreshold,
                std::format("{} · {}", combo.name, threshold.name),
                std::format("{}人門檻 · 羈絆 ID {}", threshold.count, combo.id),
                threshold.effects,
                threshold.managementRules);
        }
    }
    return result;
}

}  // namespace KysChess
