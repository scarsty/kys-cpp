#pragma once

#include "ChessCatalogQueries.h"
#include "ChessGameContent.h"
#include "ChessManagementRules.h"
#include "ChessRewardRules.h"
#include "ChessScreenLayout.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace KysChess
{

struct ChessMenuColumnRow
{
    std::string prefix;
    std::string name;
    std::string middle;
    std::string right;
};

struct ChessMenuColumnMinimumWidths
{
    int name = ChessScreenLayout::kMenuNameUnits;
    int middle = ChessScreenLayout::kMenuStarUnits;
    int right = ChessScreenLayout::kMenuCostUnits;
};

struct ChessMenuPresentation
{
    int fontSize{};
    int itemsPerPage{};

    bool operator==(const ChessMenuPresentation&) const = default;
};

inline constexpr ChessMenuPresentation kChessBrowseMenuPresentation{36, 10};
inline constexpr ChessMenuPresentation kChessCompactMenuPresentation{32, 12};
inline constexpr ChessMenuPresentation kChessChallengeMenuPresentation{36, 9};
inline constexpr ChessMenuPresentation kChessPositionSwapMenuPresentation{36, 2};

inline std::string chessMenuPrefixWithSeparator(std::string prefix)
{
    if (!prefix.empty() && prefix.back() != ' ')
    {
        prefix += ' ';
    }
    return prefix;
}

inline std::string chessEquipmentAssignmentColumn(bool assigned)
{
    return assigned ? " [已裝]" : std::string{};
}

inline std::string chessRewardOptionCostColumn(const ChessRewardOption& option)
{
    return option.goldCost > 0 ? std::format("+${}", option.goldCost) : std::string{};
}

inline ChessMenuPresentation chessRewardMenuPresentation(
    const ChessPendingReward& pending,
    int displayedRows)
{
    assert(displayedRows > 0);
    if (pending.kind == ChessRewardKind::Piece
        || pending.kind == ChessRewardKind::StarUpgrade)
    {
        return kChessCompactMenuPresentation;
    }
    if (!pending.challengeName.empty())
    {
        if (pending.kind == ChessRewardKind::InternalSkill)
        {
            return {36, 16};
        }
        if (pending.kind == ChessRewardKind::Equipment)
        {
            return kChessBrowseMenuPresentation;
        }
    }
    return {36, displayedRows};
}

inline bool chessRewardShowsComboPanel(ChessRewardKind kind)
{
    return kind == ChessRewardKind::Piece;
}

inline std::string chessStarUpgradeRewardTitle(
    const ChessSessionState& state,
    const ChessPendingReward& pending)
{
    assert(pending.kind == ChessRewardKind::StarUpgrade);
    assert(!pending.options.empty());
    const auto firstPiece = state.roster.find(pending.options.front().value);
    assert(firstPiece != state.roster.end());
    const int sourceStar = firstPiece->second.star;
    const int targetStar = pending.options.front().value2;
    assert(std::ranges::all_of(pending.options, [&](const ChessRewardOption& option) {
        const auto piece = state.roster.find(option.value);
        return piece != state.roster.end()
            && piece->second.star == sourceStar
            && option.value2 == targetStar;
    }));
    return std::format("選擇升星 {}★→{}★", sourceStar, targetStar);
}

struct ChessMenuOutlineStyle
{
    Color color{};
    bool animated = false;
    int thickness = 1;
};

enum class ChessShopRowEmphasis
{
    None,
    Owned,
    Merge,
};

inline ChessShopRowEmphasis chessShopRowEmphasis(const ChessSessionState& state, int roleId)
{
    if (ChessManagementRules::wouldGrantPieceMerge(state, roleId))
    {
        return ChessShopRowEmphasis::Merge;
    }

    int matchingPieces = 0;
    bool hasOtherStarPiece = false;
    for (const auto& entry : state.roster)
    {
        const auto& piece = entry.second;
        if (piece.roleId == roleId)
        {
            ++matchingPieces;
            hasOtherStarPiece = hasOtherStarPiece || piece.star != 3;
        }
    }
    if (matchingPieces == 0 || (matchingPieces == 1 && !hasOtherStarPiece))
    {
        return ChessShopRowEmphasis::None;
    }
    return ChessShopRowEmphasis::Owned;
}

inline ChessMenuOutlineStyle chessShopRowOutlineStyle(ChessShopRowEmphasis emphasis)
{
    switch (emphasis)
    {
    case ChessShopRowEmphasis::None:
        return {};
    case ChessShopRowEmphasis::Owned:
        return {{100, 200, 255, 220}, true, 1};
    case ChessShopRowEmphasis::Merge:
        return {{255, 215, 0, 255}, true, 3};
    }
    std::unreachable();
}

struct ChessBuyExpDeploymentPreview
{
    int current{};
    std::optional<int> next;
};

inline ChessBuyExpDeploymentPreview buildChessBuyExpDeploymentPreview(
    const ChessSessionState& state,
    const ChessGameContent& content)
{
    assert(state.level < content.balance().maxLevel);
    ChessBuyExpDeploymentPreview preview;
    preview.current = ChessManagementRules::maximumDeployment(state, content);
    const int next = ChessManagementRules::maximumDeploymentAtLevel(content, state.level + 1);
    if (next > preview.current)
    {
        preview.next = next;
    }
    return preview;
}

inline std::string formatChessShopWeightLine(const BalanceConfig& balance, int level)
{
    assert(level >= 0);
    if (level >= static_cast<int>(balance.shopWeights.size()))
    {
        return "已滿級";
    }

    const auto& weights = balance.shopWeights[level];
    return std::format(
        "1費{}% 2費{}% 3費{}% 4費{}% 5費{}%",
        weights[0],
        weights[1],
        weights[2],
        weights[3],
        weights[4]);
}

struct ChessBuyExpPreviewText
{
    std::string header;
    std::string costLine;
    std::string deploymentLine;
    std::string deploymentNext;
    std::string currentWeights;
    std::string nextWeights;
};

inline ChessBuyExpPreviewText buildChessBuyExpPreviewText(
    const ChessSessionState& state,
    const ChessGameContent& content)
{
    const auto& balance = content.balance();
    const auto deployment = buildChessBuyExpDeploymentPreview(state, content);
    ChessBuyExpPreviewText text;
    text.header = std::format(
        "等級 {}    經驗 {}/{}    金幣 ${}",
        state.level + 1,
        state.experience,
        ChessManagementRules::experienceForNextLevel(state, content),
        state.money);
    text.costLine = std::format("花費 ${}  獲得 {} 經驗", balance.buyExpCost, balance.buyExpAmount);
    text.deploymentLine = std::format("出戰人數: {}", deployment.current);
    if (deployment.next)
    {
        text.deploymentNext = std::format("  → {}", *deployment.next);
    }
    text.currentWeights = "  " + formatChessShopWeightLine(balance, state.level);
    text.nextWeights = "  " + formatChessShopWeightLine(balance, state.level + 1);
    return text;
}

inline std::vector<int> buildChessPoolBrowseOrder(const ChessGameContent& content)
{
    std::vector<int> roleIds = content.poolRoleIds();
    std::stable_sort(roleIds.begin(), roleIds.end(), [&](int leftId, int rightId) {
        const auto* left = content.role(leftId);
        const auto* right = content.role(rightId);
        assert(left && right);
        return left->Cost < right->Cost;
    });
    return roleIds;
}

inline bool chessPositionSwapPreviewEnabled(bool currentEnabled, int selectedRow)
{
    assert(selectedRow >= -1 && selectedRow <= 1);
    return selectedRow >= 0 ? selectedRow == 1 : currentEnabled;
}

struct ChessBanManagementRow
{
    int roleId{};
    bool banned = false;
};

inline std::vector<ChessBanManagementRow> buildChessBanManagementRows(
    const ChessGameplayObservation& observation,
    const ChessGameContent& content)
{
    std::vector<int> roleIds = observation.seenRoles;
    std::ranges::sort(roleIds, [&](int leftId, int rightId) {
        const auto* left = content.role(leftId);
        const auto* right = content.role(rightId);
        assert(left && right);
        return left->Cost != right->Cost ? left->Cost < right->Cost : leftId < rightId;
    });

    std::vector<ChessBanManagementRow> rows;
    rows.reserve(roleIds.size());
    for (const int roleId : roleIds)
    {
        rows.push_back({roleId, std::ranges::contains(observation.bans, roleId)});
    }
    return rows;
}

inline ChessMenuOutlineStyle chessBanManagementRowOutlineStyle(bool banned)
{
    return banned
        ? ChessMenuOutlineStyle{{255, 80, 80, 255}, false, 2}
        : ChessMenuOutlineStyle{};
}

struct ChessEquipmentMenuOrder
{
    bool owned{};
    int tier{};
    int itemId{};

    bool operator==(const ChessEquipmentMenuOrder&) const = default;
};

inline bool chessEquipmentMenuOrderLess(
    const ChessEquipmentMenuOrder& left,
    const ChessEquipmentMenuOrder& right)
{
    if (left.owned != right.owned)
    {
        return left.owned;
    }
    if (left.tier != right.tier)
    {
        return left.tier < right.tier;
    }
    return left.itemId < right.itemId;
}

enum class ChessGameGuideLineTone
{
    Standard,
    Information,
    Highlight,
};

struct ChessGameGuideLine
{
    std::string text;
    ChessGameGuideLineTone tone = ChessGameGuideLineTone::Standard;
};

struct ChessGameGuideSection
{
    std::string title;
    std::vector<ChessGameGuideLine> lines;
};

inline std::vector<ChessGameGuideSection> buildChessGameGuideSections(const ChessGameContent& content)
{
    const auto& balance = content.balance();
    const auto& neigong = content.neigongConfig();

    const auto formatTierPrices = [&balance]() {
        std::string result;
        for (int tier = 0; tier < static_cast<int>(balance.tierPrices.size()); ++tier)
        {
            if (!result.empty())
            {
                result += "、";
            }
            result += std::format("{}費{}金", tier + 1, balance.tierPrices[tier]);
        }
        return result;
    };

    const auto formatEquipmentRewardLine = [&balance]() {
        if (balance.playerEquipmentRewards.empty())
        {
            return std::string("· 本難度沒有固定裝備獎勵，主要從遠征挑戰與商店取得裝備");
        }

        int firstFight = balance.playerEquipmentRewards.front().fight;
        int maxTier = balance.playerEquipmentRewards.front().maxTier;
        for (const auto& reward : balance.playerEquipmentRewards)
        {
            firstFight = std::min(firstFight, reward.fight);
            maxTier = std::max(maxTier, reward.maxTier);
        }
        return std::format(
            "· 固定裝備獎勵共{}次，最早第{}回，最高{}階",
            balance.playerEquipmentRewards.size(),
            firstFight,
            maxTier);
    };

    const auto formatLegendaryShopLine = [&balance]() {
        if (balance.legendaryShop.unlockFight <= 0)
        {
            return std::string("· 神兵商店未開放，可從獎勵與挑戰取得裝備");
        }
        return std::format(
            "· 神兵商店第{}回後開放，每件{}金",
            balance.legendaryShop.unlockFight,
            balance.legendaryShop.price);
    };

    const auto formatChallengeLine = [&balance]() {
        if (balance.challenges.empty())
        {
            return std::string("· 目前沒有遠征挑戰配置");
        }
        return std::format(
            "· 遠征挑戰共{}關，不佔主線回合，勝後獎勵各領一次",
            balance.challenges.size());
    };

    const auto formatBanUnlockLine = [&balance]() {
        if (balance.banUnlocks.empty())
        {
            return std::string("· 禁棋會把已見過的角色逐出棋池，助你收束門路");
        }

        int firstAfterFight = balance.banUnlocks.front().afterFight;
        int maxTier = balance.banUnlocks.front().maxTier;
        for (const auto& unlock : balance.banUnlocks)
        {
            firstAfterFight = std::min(firstAfterFight, unlock.afterFight);
            maxTier = std::max(maxTier, unlock.maxTier);
        }
        return std::format(
            "· 額外禁棋共{}段，最早第{}回後解鎖，最高可禁{}費",
            balance.banUnlocks.size(),
            firstAfterFight,
            maxTier);
    };

    std::vector<ChessGameGuideSection> sections{
        {
            "基本流程",
            {
                {"· 每回合先在商店招募棋子、整隊佈陣，再入局交鋒"},
                {std::format("· 戰鬥全自動進行，共{}回；每{}回有一位強敵", balance.totalFights, balance.bossInterval)},
                {std::format("· 勝利得{}經驗，強敵勝利得{}經驗", balance.battleExp, balance.bossBattleExp)},
            },
        },
        {
            "棋子與升星",
            {
                {std::format("· 商店每回合{}格可選，棋子分一至五費，越高費越難遇見", balance.shopSlotCount)},
                {std::format("· {}；星級價格按{}倍計", formatTierPrices(), balance.starCostMult)},
                {"· 三枚相同合成二星，三枚二星再成三星，升星後屬性大增"},
            },
        },
        {
            "經濟與等級",
            {
                {std::format(
                    "· 初始{}金，刷新商店{}金，購買經驗{}金換{}點",
                    balance.initialMoney,
                    balance.refreshCost,
                    balance.buyExpCost,
                    balance.buyExpAmount)},
                {std::format(
                    "· 存款按{}%生利息，上限{}金；強敵獎勵另加{}金",
                    balance.interestPercent,
                    balance.interestMax,
                    balance.bossRewardBonus)},
                {std::format(
                    "· 最高{}級，背包{}格，最低{}人出戰",
                    balance.maxLevel,
                    balance.benchSize,
                    balance.minBattleSize)},
            },
        },
        {
            "羈絆",
            {
                {"· 同門同路的棋子同時上陣，可啟動羈絆效果"},
                {"· 羈絆人數、反向羈絆、星級加成與效果皆以羈絆一覽為準"},
            },
        },
        {
            "裝備與內功",
            {
                {"· 裝備管理可將武器、防具授予棋子，補強前排或成全主力"},
                {formatEquipmentRewardLine()},
                {formatLegendaryShopLine()},
                {std::format(
                    "· 強敵戰後可選{}本內功；刷新後選擇新候選加收{}金",
                    neigong.choiceCount,
                    neigong.rerollCost)},
            },
        },
        {
            "遠征挑戰",
            {
                {formatChallengeLine()},
                {"· 獎勵可能包含金幣、棋子、升星、內功或裝備"},
            },
        },
    };

    const bool hasBanSystem = balance.banBaseCount > 0
        || balance.banCountPerLevel > 0
        || !balance.banUnlocks.empty();
    if (hasBanSystem)
    {
        sections.insert(
            sections.begin() + 3,
            {
                std::format("{}棋局", ChessBalance::difficultyDisplayNameTraditional(content.difficulty())),
                {
                    {
                        std::format(
                            "· 本難度商店每回合{}格可選，最高{}級",
                            balance.shopSlotCount,
                            balance.maxLevel),
                        ChessGameGuideLineTone::Information,
                    },
                    {
                        std::format(
                            "· 開局可禁{}名棋子；每升1級，再增{}名禁位",
                            balance.banBaseCount,
                            balance.banCountPerLevel),
                        ChessGameGuideLineTone::Highlight,
                    },
                    {formatBanUnlockLine()},
                },
            });
    }

    return sections;
}

template <typename DisplayWidth>
std::string padRightToDisplayWidth(std::string text, int targetWidth, DisplayWidth displayWidth)
{
    while (displayWidth(text) < targetWidth)
    {
        text += ' ';
    }
    return text;
}

template <typename DisplayWidth>
std::string padLeftToDisplayWidth(std::string text, int targetWidth, DisplayWidth displayWidth)
{
    while (displayWidth(text) < targetWidth)
    {
        text = ' ' + text;
    }
    return text;
}

template <typename DisplayWidth>
std::vector<std::string> buildAlignedChessMenuLabels(
    const std::vector<ChessMenuColumnRow>& rows,
    DisplayWidth displayWidth,
    ChessMenuColumnMinimumWidths minimumWidths = {})
{
    int nameWidth = minimumWidths.name;
    int middleWidth = minimumWidths.middle;
    int rightWidth = minimumWidths.right;
    for (const auto& row : rows)
    {
        nameWidth = std::max(nameWidth, displayWidth(row.prefix + row.name));
        middleWidth = std::max(middleWidth, displayWidth(row.middle));
        rightWidth = std::max(rightWidth, displayWidth(row.right));
    }

    std::vector<std::string> labels;
    labels.reserve(rows.size());
    for (const auto& row : rows)
    {
        labels.push_back(
            padRightToDisplayWidth(row.prefix + row.name, nameWidth, displayWidth)
            + padRightToDisplayWidth(row.middle, middleWidth, displayWidth)
            + padLeftToDisplayWidth(row.right, rightWidth, displayWidth));
    }
    return labels;
}

template <typename DisplayWidth>
std::vector<std::string> buildContentSizedAlignedChessMenuLabels(
    const std::vector<ChessMenuColumnRow>& rows,
    DisplayWidth displayWidth)
{
    return buildAlignedChessMenuLabels(
        rows,
        displayWidth,
        ChessMenuColumnMinimumWidths{0, 0, 0});
}

template <typename DisplayWidth>
std::vector<std::string> buildAlignedChessRewardMenuLabels(
    ChessRewardKind rewardKind,
    const std::vector<ChessMenuColumnRow>& rows,
    DisplayWidth displayWidth)
{
    if (rewardKind == ChessRewardKind::Equipment)
    {
        return buildContentSizedAlignedChessMenuLabels(rows, displayWidth);
    }
    return buildAlignedChessMenuLabels(rows, displayWidth);
}

template <typename DisplayWidth>
std::vector<std::string> buildAlignedComboCatalogLabels(
    const std::vector<std::pair<std::string, std::string>>& rows,
    DisplayWidth displayWidth)
{
    int nameWidth = 0;
    int countWidth = 0;
    for (const auto& [name, count] : rows)
    {
        nameWidth = std::max(nameWidth, displayWidth(name));
        countWidth = std::max(countWidth, displayWidth(count));
    }

    std::vector<std::string> labels;
    labels.reserve(rows.size());
    for (const auto& [name, count] : rows)
    {
        labels.push_back(std::format(
            "{} ({})",
            padRightToDisplayWidth(name, nameWidth, displayWidth),
            padRightToDisplayWidth(count, countWidth, displayWidth)));
    }
    return labels;
}

inline std::string formatChessRolePreviewMp(int maximumMp)
{
    // 管理與獎勵預覽代表剛進入戰鬥的單位，當前內力一律從零開始。
    return std::format("{:5}/{:5}", 0, maximumMp);
}

inline std::string chessStars(int star)
{
    std::string result;
    for (int index = 0; index < star; ++index)
    {
        result += "★";
    }
    return result;
}

inline std::vector<std::string> buildChessEquipmentSynergyDetailLines(
    const ChessGameContent& content,
    int equipmentId)
{
    return chessEquipmentSynergyDetailLines(content, equipmentId);
}

inline std::string chessVictoryComboBonusText(
    const ChessGameContent& content,
    const ChessSemanticEvent& goldEvent,
    int bonus)
{
    assert(goldEvent.type == ChessSemanticEventType::GoldAwarded);
    assert(bonus > 0);
    const ComboDef* source = nullptr;
    for (const auto& combo : content.combos())
    {
        if (goldEvent.stableId == std::format("combo:{}", combo.id))
        {
            source = &combo;
            break;
        }
    }
    return std::format("({}+${})", source ? source->name : "羈絆", bonus);
}

inline const char* chessChallengeCompletionLabel(bool completed)
{
    return completed ? "[已通關]" : "";
}

inline Color chessChallengeMenuRowColor(bool completed)
{
    return completed ? Color{120, 120, 120, 255} : Color{255, 200, 100, 255};
}

inline std::vector<std::string> chessChallengeBrowseNames(const ChessGameContent& content)
{
    std::vector<std::string> names;
    names.reserve(content.balance().challenges.size());
    for (const auto& challenge : content.balance().challenges)
    {
        names.push_back(challenge.name);
    }
    return names;
}

inline std::string chessChallengeEnemyDescription(
    const ChessGameContent& content,
    const BalanceConfig::ChallengeEnemy& enemy)
{
    const auto* role = content.role(enemy.roleId);
    assert(role);
    return std::format("  {} {} ({}費)", role->Name, chessStars(enemy.star), role->Cost);
}

struct ChessRewardRolePreview
{
    int roleId = -1;
    int star = 1;
    int instanceId = -1;

    bool operator==(const ChessRewardRolePreview&) const = default;
};

inline std::optional<ChessRewardRolePreview> chessRewardRolePreview(
    const ChessSessionState& state,
    const ChessRewardOption& option)
{
    if (option.kind == ChessRewardKind::Piece)
    {
        return ChessRewardRolePreview{option.value, 1, -1};
    }
    if (option.kind != ChessRewardKind::StarUpgrade)
    {
        return std::nullopt;
    }

    const auto piece = state.roster.find(option.value);
    assert(piece != state.roster.end());
    return ChessRewardRolePreview{piece->second.roleId, option.value2, option.value};
}

}    // namespace KysChess
