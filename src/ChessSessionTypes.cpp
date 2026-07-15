#include "ChessSessionTypes.h"

#include <format>

namespace KysChess
{

ChessSemanticEventStableFields chessSemanticEventStableFields(
    const ChessSemanticEvent& event)
{
    ChessSemanticEventStableFields result;
    switch (event.type)
    {
    case ChessSemanticEventType::ShopRefreshed:
        result.value = chessSemanticEventDetail<ChessShopRefreshedEventDetail>(event).cost;
        break;
    case ChessSemanticEventType::ShopLockChanged:
        result.value = chessSemanticEventDetail<ChessShopLockChangedEventDetail>(event).locked ? 1 : 0;
        break;
    case ChessSemanticEventType::ChessPurchased:
    {
        const auto& detail = chessSemanticEventDetail<ChessPurchasedEventDetail>(event);
        result = {detail.chessInstanceId, detail.roleId, detail.cost, {}};
        break;
    }
    case ChessSemanticEventType::ChessMerged:
    {
        const auto& detail = chessSemanticEventDetail<ChessMergeEventDetail>(event);
        result = {detail.newInstanceId, detail.roleId, detail.resultStar, {}};
        break;
    }
    case ChessSemanticEventType::ChessSold:
    {
        const auto& detail = chessSemanticEventDetail<ChessSoldEventDetail>(event);
        result = {detail.chessInstanceId, detail.roleId, detail.goldGained, {}};
        break;
    }
    case ChessSemanticEventType::ExperiencePurchased:
        result.value = chessSemanticEventDetail<ChessExperiencePurchasedEventDetail>(event).amount;
        break;
    case ChessSemanticEventType::LevelChanged:
        result.value = chessSemanticEventDetail<ChessLevelChangedEventDetail>(event).level;
        break;
    case ChessSemanticEventType::DeploymentChanged:
        result.value = static_cast<int>(
            chessSemanticEventDetail<ChessDeploymentChangedEventDetail>(event).chessInstanceIds.size());
        break;
    case ChessSemanticEventType::RoleBanned:
        result.primaryId = chessSemanticEventDetail<ChessRoleBannedEventDetail>(event).roleId;
        break;
    case ChessSemanticEventType::PositionSwapOptionChanged:
        result.value = chessSemanticEventDetail<ChessPositionSwapOptionChangedEventDetail>(event).enabled ? 1 : 0;
        break;
    case ChessSemanticEventType::EnemyPlanRerolled:
        result.value = chessSemanticEventDetail<ChessEnemyPlanRerollEventDetail>(event).cost;
        break;
    case ChessSemanticEventType::EquipmentAcquired:
    {
        const auto& detail = chessSemanticEventDetail<ChessEquipmentAcquiredEventDetail>(event);
        result.primaryId = detail.equipmentInstanceId;
        result.secondaryId = detail.itemId;
        break;
    }
    case ChessSemanticEventType::EquipmentAssigned:
    {
        const auto& detail = chessSemanticEventDetail<ChessEquipmentAssignedEventDetail>(event);
        result = {
            detail.equipmentInstanceId,
            detail.targetChessInstanceId,
            detail.itemId,
            {},
        };
        break;
    }
    case ChessSemanticEventType::LegendaryEquipmentPurchased:
    {
        const auto& detail = chessSemanticEventDetail<ChessLegendaryEquipmentPurchasedEventDetail>(event);
        result = {detail.equipmentInstanceId, detail.itemId, detail.cost, {}};
        break;
    }
    case ChessSemanticEventType::BattlePrepared:
        result.stableId = chessSemanticEventDetail<ChessBattlePreparedEventDetail>(event).stableBattleId;
        break;
    case ChessSemanticEventType::MapChosen:
        result.primaryId = chessSemanticEventDetail<ChessMapChosenEventDetail>(event).mapId;
        break;
    case ChessSemanticEventType::FormationSwapped:
    {
        const auto& detail = chessSemanticEventDetail<ChessFormationSwappedEventDetail>(event);
        result.primaryId = detail.firstUnitId;
        result.secondaryId = detail.secondUnitId;
        break;
    }
    case ChessSemanticEventType::BattleStarted:
        result.stableId = chessSemanticEventDetail<ChessBattleStartedEventDetail>(event).stableBattleId;
        break;
    case ChessSemanticEventType::BattleEnded:
    {
        const auto& detail = chessSemanticEventDetail<ChessBattleEndedEventDetail>(event);
        result.value = static_cast<int>(detail.outcome);
        result.stableId = detail.stableBattleId;
        break;
    }
    case ChessSemanticEventType::GoldAwarded:
    {
        const auto& detail = chessSemanticEventDetail<ChessGoldAwardedEventDetail>(event);
        result.primaryId = detail.baseGold;
        result.secondaryId = detail.interestGold;
        result.value = detail.totalGold;
        if (detail.sourceComboId >= 0)
        {
            result.stableId = std::format("combo:{}", detail.sourceComboId);
        }
        break;
    }
    case ChessSemanticEventType::FightAdvanced:
        result.value = chessSemanticEventDetail<ChessFightAdvancedEventDetail>(event).fight;
        break;
    case ChessSemanticEventType::RewardOffered:
    {
        const auto& detail = chessSemanticEventDetail<ChessRewardOfferedEventDetail>(event);
        result.value = detail.optionCount;
        result.stableId = detail.rewardId;
        break;
    }
    case ChessSemanticEventType::RewardRerolled:
    {
        const auto& detail = chessSemanticEventDetail<ChessRewardRerolledEventDetail>(event);
        result.value = detail.cost;
        result.stableId = detail.rewardId;
        break;
    }
    case ChessSemanticEventType::RewardChosen:
        result.stableId = chessSemanticEventDetail<ChessRewardChosenEventDetail>(event).rewardId;
        break;
    case ChessSemanticEventType::InternalSkillAcquired:
        result.primaryId = chessSemanticEventDetail<ChessInternalSkillAcquiredEventDetail>(event).magicId;
        break;
    case ChessSemanticEventType::ChallengeCompleted:
        result.stableId = chessSemanticEventDetail<ChessChallengeCompletedEventDetail>(event).challengeName;
        break;
    case ChessSemanticEventType::FreeShopRefreshGranted:
        result.value = chessSemanticEventDetail<ChessFreeShopRefreshGrantedEventDetail>(event).fight;
        break;
    case ChessSemanticEventType::ExperienceAwarded:
        result.value = chessSemanticEventDetail<ChessExperienceAwardedEventDetail>(event).amount;
        break;
    case ChessSemanticEventType::RunFinished:
    case ChessSemanticEventType::CampaignCompleted:
    case ChessSemanticEventType::ForcedBansSkipped:
    case ChessSemanticEventType::FreeShopRefreshConsumed:
        break;
    }
    return result;
}

}  // namespace KysChess
