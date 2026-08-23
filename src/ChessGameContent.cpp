#include "ChessGameContent.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace KysChess
{
namespace
{

auto roleContentView(const ChessRoleDefinition& role)
{
    std::array<int, ROLE_MAGIC_COUNT> magicIds{};
    std::array<int, ROLE_MAGIC_COUNT> magicPower{};
    std::ranges::copy(role.MagicID, magicIds.begin());
    std::ranges::copy(role.MagicPower, magicPower.begin());
    return std::tuple{
        role.ID,
        role.HeadID,
        role.Cost,
        role.Sexual,
        role.MaxHP,
        role.MPType,
        role.MaxMP,
        role.Attack,
        role.Speed,
        role.Defence,
        role.Medicine,
        role.UsePoison,
        role.Detoxification,
        role.AntiPoison,
        role.Fist,
        role.Sword,
        role.Knife,
        role.Unusual,
        role.HiddenWeapon,
        role.Knowledge,
        role.Morality,
        role.AttackWithPoison,
        role.AttackTwice,
        role.Fame,
        role.IQ,
        std::move(magicIds),
        std::move(magicPower),
        role.Name,
        role.Nick,
    };
}

auto magicContentView(const ChessMagicDefinition& magic)
{
    return std::tuple{
        magic.ID,
        magic.SoundID,
        magic.MagicType,
        magic.EffectID,
        magic.HurtType,
        magic.AttackAreaType,
        magic.NeedMP,
        magic.WithPoison,
        magic.SelectDistance,
        magic.AttackDistance,
        magic.AddMP,
        magic.HurtMP,
        magic.Name,
    };
}

auto roleContentViews(const std::map<int, ChessRoleDefinition>& roles)
{
    using View = decltype(roleContentView(std::declval<const ChessRoleDefinition&>()));
    std::vector<std::pair<int, View>> result;
    result.reserve(roles.size());
    for (const auto& [id, role] : roles)
    {
        result.emplace_back(id, roleContentView(role));
    }
    return result;
}

auto magicContentViews(const std::map<int, ChessMagicDefinition>& magics)
{
    using View = decltype(magicContentView(std::declval<const ChessMagicDefinition&>()));
    std::vector<std::pair<int, View>> result;
    result.reserve(magics.size());
    for (const auto& [id, magic] : magics)
    {
        result.emplace_back(id, magicContentView(magic));
    }
    return result;
}

using RuleContentView = std::tuple<
    std::uint64_t,
    int,
    int,
    int,
    int,
    int,
    int,
    int,
    std::string,
    std::string>;
using NonBattleRuleContentView = std::tuple<int, std::string, int, int, int>;

std::vector<RuleContentView> effectRuleContentViews(
    const std::vector<EffectRule>& rules)
{
    std::vector<RuleContentView> result;
    result.reserve(rules.size());
    for (const auto& rule : rules)
    {
        result.emplace_back(
            rule.id.value,
            static_cast<int>(rule.event),
            static_cast<int>(rule.observation),
            static_cast<int>(rule.castMatch),
            static_cast<int>(rule.selector.kind),
            rule.chancePct,
            rule.maxActivations,
            rule.sharedCooldownFrames,
            effectDescription(rule, EffectDescriptionStyle::Full),
            effectDescription(rule, EffectDescriptionStyle::Compact));
    }
    return result;
}

NonBattleRuleContentView nonBattleRuleContentView(const ChessNonBattleRule& rule)
{
    return std::visit(
        [](const auto& typed) -> NonBattleRuleContentView
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, CountsAsComboRule>)
            {
                return {
                    static_cast<int>(ChessNonBattleRuleKind::CountsAsCombo),
                    typed.comboName,
                    0,
                    0,
                    0,
                };
            }
            else if constexpr (std::is_same_v<T, VictoryGoldRule>)
            {
                return {
                    static_cast<int>(ChessNonBattleRuleKind::VictoryGold),
                    {},
                    typed.perHighestSurvivorStar,
                    0,
                    0,
                };
            }
            else if constexpr (std::is_same_v<T, FreeShopRefreshRule>)
            {
                return {
                    static_cast<int>(ChessNonBattleRuleKind::FreeShopRefresh),
                    {},
                    0,
                    0,
                    0,
                };
            }
            else if constexpr (std::is_same_v<T, BattleMapChoiceRule>)
            {
                return {
                    static_cast<int>(ChessNonBattleRuleKind::BattleMapChoice),
                    {},
                    0,
                    0,
                    0,
                };
            }
            else
            {
                static_assert(std::is_same_v<T, FightWinGrowthRule>);
                return {
                    static_cast<int>(ChessNonBattleRuleKind::FightWinGrowth),
                    {},
                    typed.maxHp,
                    typed.attack,
                    typed.defence,
                };
            }
        },
        rule);
}

std::vector<NonBattleRuleContentView> nonBattleRuleContentViews(
    const std::vector<ChessNonBattleRule>& rules)
{
    std::vector<NonBattleRuleContentView> result;
    result.reserve(rules.size());
    std::ranges::transform(rules, std::back_inserter(result), nonBattleRuleContentView);
    return result;
}

auto comboContentViews(const std::vector<ComboDef>& definitions)
{
    using ThresholdView = std::tuple<
        int,
        std::string,
        std::vector<RuleContentView>,
        std::vector<NonBattleRuleContentView>>;
    using DefinitionView = std::tuple<
        int,
        std::string,
        std::vector<int>,
        std::vector<ThresholdView>,
        bool,
        bool>;
    std::vector<DefinitionView> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        std::vector<ThresholdView> thresholds;
        thresholds.reserve(definition.thresholds.size());
        for (const auto& threshold : definition.thresholds)
        {
            thresholds.emplace_back(
                threshold.count,
                threshold.name,
                effectRuleContentViews(threshold.rules),
                nonBattleRuleContentViews(threshold.managementRules));
        }
        result.emplace_back(
            definition.id,
            definition.name,
            definition.memberRoleIds,
            std::move(thresholds),
            definition.isAntiCombo,
            definition.starSynergyBonus);
    }
    return result;
}

auto equipmentContentViews(const std::vector<EquipmentDef>& definitions)
{
    using DefinitionView = std::tuple<
        int,
        int,
        int,
        std::vector<RuleContentView>,
        std::vector<NonBattleRuleContentView>>;
    std::vector<DefinitionView> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        result.emplace_back(
            definition.itemId,
            definition.tier,
            definition.equipType,
            effectRuleContentViews(definition.rules),
            nonBattleRuleContentViews(definition.managementRules));
    }
    return result;
}

auto equipmentSynergyContentViews(
    const std::vector<EquipmentSynergyDef>& definitions)
{
    using DefinitionView = std::tuple<
        std::vector<int>,
        int,
        std::vector<RuleContentView>,
        std::vector<NonBattleRuleContentView>>;
    std::vector<DefinitionView> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        result.emplace_back(
            definition.roleIds,
            definition.equipmentId,
            effectRuleContentViews(definition.rules),
            nonBattleRuleContentViews(definition.managementRules));
    }
    return result;
}

auto neigongContentViews(const std::vector<NeigongDef>& definitions)
{
    using DefinitionView = std::tuple<
        int,
        int,
        int,
        std::string,
        std::vector<RuleContentView>>;
    std::vector<DefinitionView> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        result.emplace_back(
            definition.itemId,
            definition.magicId,
            definition.tier,
            definition.name,
            effectRuleContentViews(definition.rules));
    }
    return result;
}

auto magicEffectContentViews(const std::vector<ChessMagicEffectDefinition>& definitions)
{
    using DefinitionView = std::tuple<
        int,
        std::string,
        std::string,
        bool,
        std::vector<RuleContentView>>;
    std::vector<DefinitionView> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        if (!definition.enabled)
        {
            continue;
        }
        result.emplace_back(
            definition.magicId,
            definition.name,
            definition.purpose,
            definition.enabled,
            effectRuleContentViews(definition.rules));
    }
    return result;
}

ChessSha256 chessContentFingerprint(const ChessGameContentData& data)
{
    const auto roles = roleContentViews(data.roles);
    const auto magics = magicContentViews(data.magics);
    const auto combos = comboContentViews(data.combos);
    const auto equipment = equipmentContentViews(data.equipment);
    const auto equipmentSynergies = equipmentSynergyContentViews(
        data.equipmentSynergies);
    const auto neigong = neigongContentViews(data.neigong);
    const auto magicEffects = magicEffectContentViews(data.magicEffects);
    return chessBeveSha256(
        "KYS_CHESS_CONTENT",
        data.difficulty,
        data.balance,
        roles,
        magics,
        data.items,
        data.poolRoleIds,
        combos,
        equipment,
        equipmentSynergies,
        data.neigongConfig,
        neigong,
        magicEffects,
        data.battleMaps,
        data.battlefields);
}

}

ChessGameContent::ChessGameContent(ChessGameContentData data, std::string gameVersion)
    : data_(std::make_shared<const ChessGameContentData>(std::move(data))),
      contentFingerprint_(chessContentFingerprint(*data_)),
      gameVersion_(std::move(gameVersion))
{
}

std::shared_ptr<const ChessGameContent> ChessGameContent::withGameVersion(
    std::string gameVersion) const
{
    auto result = std::make_shared<ChessGameContent>(*this);
    result->gameVersion_ = std::move(gameVersion);
    return result;
}

const ChessRoleDefinition* ChessGameContent::role(int roleId) const
{
    const auto found = data_->roles.find(roleId);
    return found == data_->roles.end() ? nullptr : &found->second;
}

const ChessMagicDefinition* ChessGameContent::magic(int magicId) const
{
    const auto found = data_->magics.find(magicId);
    return found == data_->magics.end() ? nullptr : &found->second;
}

const ChessItemDefinition* ChessGameContent::item(int itemId) const
{
    const auto found = data_->items.find(itemId);
    return found == data_->items.end() ? nullptr : &found->second;
}

std::vector<std::pair<const ChessMagicDefinition*, int>> chessRoleMagicsForStar(
    const ChessGameContent& content,
    const ChessRoleDefinition& role,
    int star)
{
    std::vector<std::pair<const ChessMagicDefinition*, int>> result;
    for (int index = RoleSave::getMagicSlotStart(star);
         index < RoleSave::getMagicSlotEnd(star);
         ++index)
    {
        if (const auto* magic = content.magic(role.MagicID[index]))
        {
            result.emplace_back(magic, role.MagicPower[index]);
        }
    }
    std::ranges::sort(result, [](const auto& lhs, const auto& rhs) {
        return std::tuple{lhs.second, lhs.first->ID}
            < std::tuple{rhs.second, rhs.first->ID};
    });
    return result;
}

}
