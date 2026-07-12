#include "ChessObservationText.h"

#include "ChessReplayJson.h"

#include <algorithm>
#include <cassert>
#include <format>

namespace KysChess
{
namespace
{

std::string phaseText(ChessSessionPhase phase)
{
    switch (phase)
    {
    case ChessSessionPhase::Management: return "整備";
    case ChessSessionPhase::BattlePreparation: return "戰前佈陣";
    case ChessSessionPhase::BattleResolution: return "戰鬥結算";
    case ChessSessionPhase::RewardChoice: return "選擇獎勵";
    case ChessSessionPhase::Complete: return "本局完成";
    }
    std::unreachable();
}

std::string roleName(const ChessGameContent& content, int roleId)
{
    const auto* role = content.role(roleId);
    return role ? role->Name : "未知角色";
}

std::string activeComboEffectSourceText(
    const ChessGameplayObservation& observation,
    const ChessGameContent& content,
    EffectType effectType)
{
    std::string result;
    for (const auto& observed : observation.combos)
    {
        if (observed.activeThresholdIndex < 0)
        {
            continue;
        }
        const auto combo = std::ranges::find(content.combos(), observed.comboId, &ComboDef::id);
        assert(combo != content.combos().end());
        assert(observed.activeThresholdIndex < static_cast<int>(combo->thresholds.size()));
        const auto& effects = combo->thresholds[observed.activeThresholdIndex].effects;
        if (!std::ranges::contains(effects, effectType, &ComboEffect::type))
        {
            continue;
        }
        if (!result.empty())
        {
            result += "、";
        }
        result += combo->name;
    }
    return result.empty() ? "羈絆效果" : result;
}

}

std::string ChessObservationText::format(
    const ChessGameplayObservation& observation,
    const ChessGameContent& content,
    const std::vector<ChessLegalActionDescriptor>& legalActions)
{
    std::string text = std::format(
        "難度：{}  階段：{}  第{}戰  等級{}  經驗{}/{}  金幣${}\n",
        ChessBalance::difficultyDisplayNameTraditional(observation.difficulty),
        phaseText(observation.phase),
        observation.fight + 1,
        observation.level + 1,
        observation.experience,
        observation.experienceForNextLevel,
        observation.money);
    if (observation.campaignComplete)
    {
        text += "主線已通關；仍可整備或挑戰遠征，完成後可用 finish_run 結束本局。\n";
    }
    if (observation.freeShopRefreshAvailable)
    {
        text += std::format(
            "{}：下一次刷新商店免費。\n",
            activeComboEffectSourceText(observation, content, EffectType::FreeRefresh));
    }
    if (!observation.combos.empty())
    {
        text += "羈絆：\n";
        for (const auto& observed : observation.combos)
        {
            const auto found = std::ranges::find(
                content.combos(),
                observed.comboId,
                &ComboDef::id);
            if (found != content.combos().end() && observed.activeThresholdIndex >= 0)
            {
                text += std::format(
                    "  {} {}/{}\n",
                    found->name,
                    observed.effectiveCount,
                    found->thresholds[observed.activeThresholdIndex].count);
            }
        }
    }
    if (!observation.obtainedNeigongIds.empty())
    {
        text += "內功：\n";
        for (const int magicId : observation.obtainedNeigongIds)
        {
            const auto found = std::ranges::find(content.neigong(), magicId, &NeigongDef::magicId);
            text += std::format(
                "  {} magic={}\n",
                found == content.neigong().end() ? "未知內功" : found->name,
                magicId);
        }
    }
    if (!observation.completedChallengeIds.empty())
    {
        text += "已完成遠征：";
        for (const auto& challengeId : observation.completedChallengeIds)
        {
            text += " " + challengeId;
        }
        text += "\n";
    }
    if (!observation.shop.empty())
    {
        text += "商店：\n";
        for (int slot = 0; slot < static_cast<int>(observation.shop.size()); ++slot)
        {
            const auto& entry = observation.shop[slot];
            text += entry.roleId < 0
                ? std::format("  [{}] 已售出\n", slot)
                : std::format(
                    "  [{}] {} role={} 階級={} 價格=${}\n",
                    slot,
                    roleName(content, entry.roleId),
                    entry.roleId,
                    entry.tier,
                    content.balance().tierPrices[entry.tier - 1]);
        }
    }
    text += std::format("棋子（出戰上限{}）：\n", observation.maximumDeployment);
    for (const auto& piece : observation.roster)
    {
        text += std::format(
            "  #{} {} role={} ★{} {} 勝場={} 武器={} 防具={}\n",
            piece.instanceId,
            roleName(content, piece.roleId),
            piece.roleId,
            piece.star,
            piece.deployed ? "出戰" : "備戰",
            piece.fightsWon,
            piece.weaponInstanceId,
            piece.armorInstanceId);
    }
    if (!observation.equipmentInventory.empty())
    {
        text += "裝備：\n";
        for (const auto& equipment : observation.equipmentInventory)
        {
            const auto* item = content.item(equipment.itemId);
            text += std::format(
                "  裝備#{} {} item={} 指派={}\n",
                equipment.instanceId,
                item ? item->name : "未知裝備",
                equipment.itemId,
                equipment.assignedChessInstanceId);
        }
    }
    if (observation.pendingReward)
    {
        text += std::format("待選獎勵 {}：\n", observation.pendingReward->id);
        for (const auto& option : observation.pendingReward->options)
        {
            text += std::format("  {}\n", option.id);
        }
    }
    text += "可用操作：\n";
    for (const auto& action : legalActions)
    {
        text += std::format("  {}", chessActionTypeId(action.type));
        if (!action.candidateIds.empty())
        {
            text += " ids=";
            for (const int id : action.candidateIds)
            {
                text += std::format("{} ", id);
            }
        }
        if (!action.candidateStableIds.empty())
        {
            text += " 選項=";
            for (const auto& id : action.candidateStableIds)
            {
                text += id + " ";
            }
        }
        text += "\n";
    }
    return text;
}

}
