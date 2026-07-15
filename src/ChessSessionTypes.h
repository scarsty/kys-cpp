#pragma once

#include "ChessBalance.h"
#include "ChessReplayHash.h"
#include "PreparedChessBattle.h"
#include "battle/BattleOutcome.h"
#include "battle/ChessComboResolver.h"
#include "battle/BattlePresentation.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace KysChess
{

enum class ChessSessionPhase : std::uint16_t
{
    Management,
    BattlePreparation,
    BattleResolution,
    RewardChoice,
    Complete,
};

enum class ChessActionType : std::uint16_t
{
    BuyShopSlot,
    RefreshShop,
    SetShopLocked,
    SellChess,
    SetDeployment,
    BuyExp,
    AddBan,
    SkipForcedBans,
    Equip,
    BuyLegendaryEquipment,
    SetPositionSwapEnabled,
    RerollEnemySeed,
    PrepareBattle,
    ChooseMap,
    SwapPositions,
    StartBattle,
    RerollReward,
    ChooseReward,
    StartChallenge,
    FinishRun,
};

enum class ChessRuleErrorCode : std::uint16_t
{
    None,
    WrongPhase,
    TransitionPending,
    UnsupportedAction,
    InvalidShopSlot,
    EmptyShopSlot,
    InsufficientGold,
    BenchFull,
    UnknownChessInstance,
    DuplicateIdentifier,
    DeploymentLimitExceeded,
    MaximumLevel,
    InvalidRole,
    BanLimitReached,
    RoleNotSeen,
    RoleAlreadyBanned,
    DeploymentRequired,
    InvalidMap,
    InvalidSwap,
    UnknownEquipmentInstance,
    InvalidEquipment,
    EquipmentTypeMismatch,
    LegendaryShopLocked,
    InvalidReward,
    RewardRerollUnavailable,
    UnknownChallenge,
    ChallengeAlreadyPending,
    CampaignAlreadyComplete,
    CampaignNotComplete,
    NoPreparedBattle,
};

enum class ChessSemanticEventType : std::uint16_t
{
    ShopRefreshed,
    ShopLockChanged,
    ChessPurchased,
    ChessMerged,
    ChessSold,
    ExperiencePurchased,
    LevelChanged,
    DeploymentChanged,
    RoleBanned,
    PositionSwapOptionChanged,
    EnemyPlanRerolled,
    EquipmentAcquired,
    EquipmentAssigned,
    LegendaryEquipmentPurchased,
    BattlePrepared,
    MapChosen,
    FormationSwapped,
    BattleStarted,
    BattleEnded,
    GoldAwarded,
    FightAdvanced,
    RewardOffered,
    RewardRerolled,
    RewardChosen,
    InternalSkillAcquired,
    ChallengeCompleted,
    CampaignCompleted,
    RunFinished,
    ForcedBansSkipped,
    FreeShopRefreshGranted,
    FreeShopRefreshConsumed,
    ExperienceAwarded,
};

enum class ChessRewardKind : std::uint16_t
{
    Equipment,
    InternalSkill,
    ChallengeReward,
    Piece,
    StarUpgrade,
    ForcedBan,
};

struct ChessSessionOptions
{
    bool positionSwapEnabled = true;
    int battleFrameLimit = 36000;

    auto operator<=>(const ChessSessionOptions&) const = default;
};

struct ChessAction
{
    ChessActionType type{};
    int shopSlot{};
    bool value{};
    int chessInstanceId{};
    int targetChessInstanceId{};
    int equipmentInstanceId{};
    int roleId{};
    int itemId{};
    int mapId{};
    std::string challengeName;
    std::string rewardId;
    std::vector<int> chessInstanceIds;
};

struct ChessShopRefreshedEventDetail
{
    int cost{};
    auto operator<=>(const ChessShopRefreshedEventDetail&) const = default;
};

struct ChessShopLockChangedEventDetail
{
    bool locked{};
    auto operator<=>(const ChessShopLockChangedEventDetail&) const = default;
};

struct ChessPurchasedEventDetail
{
    int chessInstanceId{};
    int roleId{};
    int cost{};
    auto operator<=>(const ChessPurchasedEventDetail&) const = default;
};

struct ChessMergeEventDetail
{
    int newInstanceId{};
    int roleId{};
    int resultStar{};
    std::vector<int> consumedInstanceIds;
    int inheritedFightsWon{};
    bool deployed{};
    std::vector<int> transferredEquipmentInstanceIds;
    bool recursiveMergeFollowed{};

    auto operator<=>(const ChessMergeEventDetail&) const = default;
};

struct ChessSoldEventDetail
{
    int chessInstanceId{};
    int roleId{};
    int goldGained{};
    auto operator<=>(const ChessSoldEventDetail&) const = default;
};

struct ChessExperiencePurchasedEventDetail
{
    int amount{};
    auto operator<=>(const ChessExperiencePurchasedEventDetail&) const = default;
};

struct ChessLevelChangedEventDetail
{
    int level{};
    auto operator<=>(const ChessLevelChangedEventDetail&) const = default;
};

struct ChessDeploymentChangedEventDetail
{
    std::vector<int> chessInstanceIds;
    auto operator<=>(const ChessDeploymentChangedEventDetail&) const = default;
};

struct ChessRoleBannedEventDetail
{
    int roleId{};
    auto operator<=>(const ChessRoleBannedEventDetail&) const = default;
};

struct ChessPositionSwapOptionChangedEventDetail
{
    bool enabled{};
    auto operator<=>(const ChessPositionSwapOptionChangedEventDetail&) const = default;
};

struct ChessEnemyPlanRerollEventDetail
{
    int cost{};
    std::uint64_t previousEnemyPlanKey{};
    std::uint64_t newEnemyPlanKey{};

    auto operator<=>(const ChessEnemyPlanRerollEventDetail&) const = default;
};

struct ChessEquipmentAcquiredEventDetail
{
    int equipmentInstanceId{};
    int itemId{};
    auto operator<=>(const ChessEquipmentAcquiredEventDetail&) const = default;
};

struct ChessEquipmentAssignedEventDetail
{
    int equipmentInstanceId{};
    int itemId{};
    int targetChessInstanceId{};
    int previousChessInstanceId = -1;
    int displacedEquipmentInstanceId = -1;
    auto operator<=>(const ChessEquipmentAssignedEventDetail&) const = default;
};

struct ChessLegendaryEquipmentPurchasedEventDetail
{
    int equipmentInstanceId{};
    int itemId{};
    int cost{};
    auto operator<=>(const ChessLegendaryEquipmentPurchasedEventDetail&) const = default;
};

struct ChessBattlePreparedEventDetail
{
    std::string stableBattleId;
    auto operator<=>(const ChessBattlePreparedEventDetail&) const = default;
};

struct ChessMapChosenEventDetail
{
    int mapId{};
    auto operator<=>(const ChessMapChosenEventDetail&) const = default;
};

struct ChessFormationSwappedEventDetail
{
    int firstUnitId{};
    int secondUnitId{};
    auto operator<=>(const ChessFormationSwappedEventDetail&) const = default;
};

struct ChessBattleStartedEventDetail
{
    std::string stableBattleId;
    auto operator<=>(const ChessBattleStartedEventDetail&) const = default;
};

struct ChessBattleEndedEventDetail
{
    Battle::BattleOutcome outcome = Battle::BattleOutcome::InProgress;
    std::string stableBattleId;
    auto operator<=>(const ChessBattleEndedEventDetail&) const = default;
};

struct ChessGoldAwardedEventDetail
{
    int baseGold{};
    int interestGold{};
    int otherGold{};
    int totalGold{};
    int sourceComboId = -1;
    auto operator<=>(const ChessGoldAwardedEventDetail&) const = default;
};

struct ChessFightAdvancedEventDetail
{
    int fight{};
    auto operator<=>(const ChessFightAdvancedEventDetail&) const = default;
};

struct ChessRewardOfferedEventDetail
{
    std::string rewardId;
    int optionCount{};
    auto operator<=>(const ChessRewardOfferedEventDetail&) const = default;
};

struct ChessRewardRerolledEventDetail
{
    std::string rewardId;
    int cost{};
    int optionCount{};
    auto operator<=>(const ChessRewardRerolledEventDetail&) const = default;
};

struct ChessRewardChosenEventDetail
{
    std::string rewardId;
    ChessRewardKind kind{};
    int value{};
    int value2{};
    auto operator<=>(const ChessRewardChosenEventDetail&) const = default;
};

struct ChessInternalSkillAcquiredEventDetail
{
    int magicId{};
    auto operator<=>(const ChessInternalSkillAcquiredEventDetail&) const = default;
};

struct ChessChallengeCompletedEventDetail
{
    std::string challengeName;
    auto operator<=>(const ChessChallengeCompletedEventDetail&) const = default;
};

struct ChessFreeShopRefreshGrantedEventDetail
{
    int fight{};
    auto operator<=>(const ChessFreeShopRefreshGrantedEventDetail&) const = default;
};

struct ChessExperienceAwardedEventDetail
{
    int amount{};
    auto operator<=>(const ChessExperienceAwardedEventDetail&) const = default;
};

using ChessSemanticEventPayload = std::variant<
    std::monostate,
    ChessShopRefreshedEventDetail,
    ChessShopLockChangedEventDetail,
    ChessPurchasedEventDetail,
    ChessMergeEventDetail,
    ChessSoldEventDetail,
    ChessExperiencePurchasedEventDetail,
    ChessLevelChangedEventDetail,
    ChessDeploymentChangedEventDetail,
    ChessRoleBannedEventDetail,
    ChessPositionSwapOptionChangedEventDetail,
    ChessEnemyPlanRerollEventDetail,
    ChessEquipmentAcquiredEventDetail,
    ChessEquipmentAssignedEventDetail,
    ChessLegendaryEquipmentPurchasedEventDetail,
    ChessBattlePreparedEventDetail,
    ChessMapChosenEventDetail,
    ChessFormationSwappedEventDetail,
    ChessBattleStartedEventDetail,
    ChessBattleEndedEventDetail,
    ChessGoldAwardedEventDetail,
    ChessFightAdvancedEventDetail,
    ChessRewardOfferedEventDetail,
    ChessRewardRerolledEventDetail,
    ChessRewardChosenEventDetail,
    ChessInternalSkillAcquiredEventDetail,
    ChessChallengeCompletedEventDetail,
    ChessFreeShopRefreshGrantedEventDetail,
    ChessExperienceAwardedEventDetail>;

struct ChessSemanticEvent
{
    ChessSemanticEventType type{};
    ChessSemanticEventPayload payload;
};

struct ChessSemanticEventStableFields
{
    int primaryId{};
    int secondaryId{};
    int value{};
    std::string stableId;
};

template<typename Detail>
const Detail& chessSemanticEventDetail(const ChessSemanticEvent& event)
{
    return std::get<Detail>(event.payload);
}

template<typename Detail>
Detail& chessSemanticEventDetail(ChessSemanticEvent& event)
{
    return std::get<Detail>(event.payload);
}

ChessSemanticEventStableFields chessSemanticEventStableFields(
    const ChessSemanticEvent& event);

struct ChessSessionPiece
{
    int instanceId{};
    int roleId{};
    int star = 1;
    bool deployed = false;
    int weaponInstanceId = -1;
    int armorInstanceId = -1;
    int fightsWon{};

    auto operator<=>(const ChessSessionPiece&) const = default;
};

struct ChessSessionShopSlot
{
    int roleId = -1;
    int tier{};

    auto operator<=>(const ChessSessionShopSlot&) const = default;
};

struct ChessEquipmentInstance
{
    int instanceId = -1;
    int itemId = -1;
    int assignedChessInstanceId = -1;

    auto operator<=>(const ChessEquipmentInstance&) const = default;
};

struct ChessRewardOption
{
    std::string id;
    ChessRewardKind kind{};
    int value{};
    int value2{};

    auto operator<=>(const ChessRewardOption&) const = default;
};

struct ChessPendingReward
{
    std::string id;
    ChessRewardKind kind{};
    std::vector<ChessRewardOption> options;
    int rerollCost{};
    int parameter{};
    int choiceCount{};
    std::vector<int> eligibleTiers;
    bool rerolled = false;
    std::string challengeName;

    auto operator<=>(const ChessPendingReward&) const = default;
};

struct ChessSessionState
{
    Difficulty difficulty = Difficulty::Easy;
    int money{};
    int experience{};
    int level{};
    int fight{};
    bool campaignComplete = false;
    bool shopLocked = false;
    bool freeShopRefreshAvailable = false;
    int freeShopRefreshGrantedFight = -1;
    int nextChessInstanceId = 1;
    int nextEquipmentInstanceId = 1;
    ChessSessionPhase phase = ChessSessionPhase::Management;
    ChessSessionOptions options;
    std::map<int, ChessSessionPiece> roster;
    std::map<int, ChessEquipmentInstance> equipmentInventory;
    std::vector<ChessSessionShopSlot> shop;
    std::set<int> rejectedRoleIds;
    std::set<int> seenRoleIds;
    std::set<int> bannedRoleIds;
    std::set<std::string> completedChallengeNames;
    std::set<int> obtainedNeigongIds;
    std::optional<PreparedChessBattle> preparedBattle;
    std::vector<ChessPendingReward> pendingRewards;
    Battle::BattleOutcome lastBattleOutcome = Battle::BattleOutcome::InProgress;
    int lastBattleEndFrame{};
    ChessSha256 lastBattleDigest{};

    auto operator<=>(const ChessSessionState&) const = default;
};

struct ChessObservedCombo
{
    int comboId{};
    int physicalCount{};
    int effectiveCount{};
    int activeThresholdIndex = -1;
    int nextThresholdIndex = -1;
    std::vector<ResolvedChessComboContribution> contributions;

    auto operator<=>(const ChessObservedCombo&) const = default;
};

struct ChessGameplayObservation
{
    ChessSessionPhase phase{};
    Difficulty difficulty = Difficulty::Easy;
    ChessSessionOptions options;
    int money{};
    int interestGold{};
    std::optional<int> nextInterestThreshold;
    int maximumInterestGold{};
    int projectedBaseVictoryGold{};
    int projectedVictoryIncome{};
    int experience{};
    int experienceForNextLevel{};
    int level{};
    int maximumDeployment{};
    int maximumBanCount{};
    int fight{};
    bool campaignComplete{};
    bool shopLocked{};
    bool freeShopRefreshAvailable{};
    int freeShopRefreshGrantedFight = -1;
    std::vector<ChessSessionShopSlot> shop;
    std::vector<ChessSessionPiece> roster;
    std::vector<ChessEquipmentInstance> equipmentInventory;
    std::vector<int> bans;
    std::vector<int> seenRoles;
    std::vector<int> obtainedNeigongIds;
    std::vector<std::string> completedChallengeNames;
    std::vector<ChessObservedCombo> combos;
    std::optional<PreparedChessBattle> preparedBattle;
    std::optional<ChessPendingReward> pendingReward;
    Battle::BattleOutcome lastBattleOutcome = Battle::BattleOutcome::InProgress;
    int lastBattleEndFrame{};
    ChessSha256 lastBattleDigest{};
    ChessSha256 stateHash{};
};

struct ChessActionResult
{
    bool accepted = false;
    bool transitionPending = false;
    ChessRuleErrorCode error = ChessRuleErrorCode::None;
    std::string description;
    std::vector<ChessSemanticEvent> events;
    std::uint64_t replaySequence{};
    ChessSha256 preStateHash{};
    ChessSha256 postStateHash{};
    ChessSha256 eventHash{};
    ChessSha256 rngDigest{};
    ChessSha256 chainHash{};
};

struct ChessAutomaticAdvanceResult
{
    int framesAdvanced{};
    std::vector<Battle::BattlePresentationFrame> frames;
    std::optional<ChessActionResult> completedAction;
};

}
