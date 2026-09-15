#pragma once

#include "ChessBalance.h"
#include "ChessReplayHash.h"
#include "ChessRuntimeConstants.h"
#include "PreparedChessBattle.h"
#include "battle/BattleOutcome.h"
#include "battle/ChessComboResolver.h"
#include "battle/BattlePresentation.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace KysChess
{

inline constexpr int kChessFormationSlotCount = 10;

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
    ChooseReward,
    StartChallenge,
    FinishRun,
    SetFormation,
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
    UnknownChallenge,
    ChallengeAlreadyPending,
    CampaignAlreadyComplete,
    CampaignNotComplete,
    NoPreparedBattle,
    InvalidFormation,
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
    RewardChosen,
    InternalSkillAcquired,
    ChallengeCompleted,
    CampaignCompleted,
    RunFinished,
    ForcedBansSkipped,
    FreeShopRefreshGranted,
    FreeShopRefreshConsumed,
    ExperienceAwarded,
    FormationChanged,
    OpeningTalentBansQueued,
    OpeningTalentShopRefreshed,
    LuckGranted,
    LuckMerged,
    ShopGuaranteeQueued,
    ShopGuaranteeInjected,
    ShopGuaranteeCancelled,
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
    int battleFrameLimit = kChessBattleFrameLimit;
    std::optional<ChessTalentId> talent;

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

    auto operator<=>(const ChessAction&) const = default;
};

struct ChessMergeEventDetail
{
    std::vector<int> consumedInstanceIds;
    int inheritedFightsWon{};
    bool deployed{};
    std::vector<int> transferredEquipmentInstanceIds;
    bool recursiveMergeFollowed{};

    auto operator<=>(const ChessMergeEventDetail&) const = default;
};

struct ChessEnemyPlanRerollEventDetail
{
    int cost{};
    std::uint64_t previousEnemyPlanKey{};
    std::uint64_t newEnemyPlanKey{};

    auto operator<=>(const ChessEnemyPlanRerollEventDetail&) const = default;
};

struct ChessSemanticEvent
{
    ChessSemanticEventType type{};
    int primaryId{};
    int secondaryId{};
    int value{};
    std::string stableId;
    std::optional<ChessMergeEventDetail> merge;
    std::optional<ChessEnemyPlanRerollEventDetail> enemyPlanReroll;

    auto operator<=>(const ChessSemanticEvent&) const = default;
};

struct ChessSessionPiece
{
    int instanceId{};
    int roleId{};
    int star = 1;
    bool deployed = false;
    int weaponInstanceId = -1;
    int armorInstanceId = -1;
    int fightsWon{};
    int luckStacks{};

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
    int goldCost{};

    auto operator<=>(const ChessRewardOption&) const = default;
};

struct ChessPendingReward
{
    std::string id;
    ChessRewardKind kind{};
    std::vector<ChessRewardOption> options;
    int additionalOptionCost{};
    int parameter{};
    int choiceCount{};
    std::vector<int> eligibleTiers;
    std::string challengeName;

    auto operator<=>(const ChessPendingReward&) const = default;
};

struct ChessSessionState
{
    Difficulty difficulty = Difficulty::Easy;
    ChessTalentId talent = ChessTalentId::DivineArms;
    std::vector<int> shopGuarantees;
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
    std::vector<int> formationSlots = std::vector<int>(kChessFormationSlotCount, -1);
    std::map<int, ChessEquipmentInstance> equipmentInventory;
    std::vector<ChessSessionShopSlot> shop;
    std::set<int> rejectedRoleIds;
    std::set<int> seenRoleIds;
    std::set<int> bannedRoleIds;
    int selectedForcedBanCount{};
    std::set<std::string> completedChallengeNames;
    std::set<int> obtainedNeigongIds;
    std::optional<PreparedChessBattle> preparedBattle;
    std::vector<ChessPendingReward> pendingRewards;
    Battle::BattleOutcome lastBattleOutcome = Battle::BattleOutcome::InProgress;
    int lastBattleEndFrame{};
    ChessSha256 lastBattleDigest{};

    auto operator<=>(const ChessSessionState&) const = default;
};

struct ChessLegalActionDescriptor
{
    ChessActionType type{};
    std::vector<int> candidateIds;
    std::vector<std::string> candidateStableIds;
    int minimumSelection{};
    int maximumSelection{};
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
    ChessTalentId talent = ChessTalentId::DivineArms;
    std::vector<int> shopGuarantees;
    ChessSessionOptions options;
    std::string talentName;
    std::string talentDescription;
    bool talentHasLegendaryShop{};
    int luckChancePerStack{};
    int luckStackCap{};
    std::optional<BalanceConfig::PlayerEquipmentReward> nextBasicEquipmentReward;
    std::optional<BalanceConfig::PlayerEquipmentReward> nextTalentEquipmentReward;
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
    std::vector<int> formationSlots;
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
    ChessEvidenceHash evidenceHash{};
};

struct ChessAutomaticAdvanceResult
{
    int framesAdvanced{};
    std::vector<Battle::BattlePresentationFrame> frames;
    std::optional<ChessActionResult> completedAction;
};

}
