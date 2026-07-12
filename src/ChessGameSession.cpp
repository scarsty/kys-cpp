#include "ChessGameSession.h"

#include "BattleSetupFactory.h"
#include "ChessBattlePlanner.h"
#include "ChessProgressionRules.h"
#include "ChessRewardRules.h"
#include "ChessRuntimeConstants.h"
#include "HeadlessBattleRunner.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <utility>

namespace KysChess
{
namespace
{

std::string difficultyId(Difficulty difficulty)
{
    switch (difficulty)
    {
    case Difficulty::Easy: return "easy";
    case Difficulty::Normal: return "normal";
    case Difficulty::Hard: return "hard";
    }
    std::unreachable();
}

}

struct ChessGameSession::PendingTransition
{
    PendingTransition(
        ChessSessionPhase sourcePhase,
        ChessAction acceptedAction,
        ChessSha256 sourceStateHash,
        std::vector<ChessSemanticEvent> semanticEvents,
        Battle::BattleRuntimeSessionCreationResult creation)
        : phase(sourcePhase),
          action(std::move(acceptedAction)),
          preStateHash(sourceStateHash),
          events(std::move(semanticEvents)),
          initialization(std::move(creation.initialization)),
          runtime(std::make_shared<Battle::BattleRuntimeSession>(std::move(creation.session)))
    {
        collector.consumeInitialization(initialization, *runtime);
    }

    ChessSessionPhase phase{};
    ChessAction action;
    ChessSha256 preStateHash{};
    std::vector<ChessSemanticEvent> events;
    Battle::BattleInitializationResult initialization;
    std::shared_ptr<Battle::BattleRuntimeSession> runtime;
    BattleReportCollector collector;
    std::vector<Battle::BattleDigestEvent> digestEvents;
};

ChessReplayHeader ChessGameSession::makeReplayHeader(
    const ChessGameContent& content,
    std::uint64_t rootSeed,
    const ChessSessionOptions& options)
{
    ChessReplayHeader header;
    header.gameVersion = content.gameVersion();
    header.difficulty = difficultyId(content.difficulty());
    header.rootSeed = rootSeed;
    header.options = options;
    header.options.battleFrameLimit = kChessBattleFrameLimit;
    return header;
}

ChessGameSession::ChessGameSession(
    std::shared_ptr<const ChessGameContent> content,
    std::uint64_t rootSeed,
    ChessSessionOptions options)
    : content_(std::move(content)),
      random_(rootSeed),
      journal_(makeReplayHeader(*content_, rootSeed, options))
{
    state_.difficulty = content_->difficulty();
    state_.money = content_->balance().initialMoney;
    state_.options = journal_.header().options;
    state_.campaignComplete = content_->balance().totalFights <= 0;
    ChessManagementRules::initializeShop(state_, *content_, random_);
}

std::unique_ptr<ChessGameSession> ChessGameSession::createStandaloneBattle(
    std::shared_ptr<const ChessGameContent> content,
    std::uint64_t rootSeed,
    PreparedChessBattle preparedBattle,
    std::set<int> obtainedNeigongIds,
    ChessSessionOptions options)
{
    assert(preparedBattle.kind == PreparedChessBattleKind::Standalone);
    assert(preparedBattle.chosenMapId >= 0);
    auto session = std::make_unique<ChessGameSession>(
        std::move(content),
        rootSeed,
        options);
    session->state_.phase = ChessSessionPhase::BattlePreparation;
    session->state_.preparedBattle = std::move(preparedBattle);
    session->state_.obtainedNeigongIds = std::move(obtainedNeigongIds);
    session->replayExportEnabled_ = false;
    return session;
}

ChessGameSession::~ChessGameSession() = default;

bool ChessGameSession::isStableDecisionBoundary() const
{
    assert((pendingTransition_ != nullptr)
        == (state_.phase == ChessSessionPhase::BattleResolution));
    return pendingTransition_ == nullptr;
}

const Battle::BattleInitializationResult* ChessGameSession::pendingBattleInitialization() const
{
    return pendingTransition_ ? &pendingTransition_->initialization : nullptr;
}

const Battle::BattleRuntimeSession* ChessGameSession::pendingBattleRuntime() const
{
    return pendingTransition_ ? pendingTransition_->runtime.get() : nullptr;
}

const Battle::BattleRuntimeSession* ChessGameSession::presentationBattleRuntime() const
{
    return pendingTransition_ ? pendingTransition_->runtime.get() : lastBattleRuntime_.get();
}

void ChessGameSession::discardPendingTransition()
{
    pendingTransition_.reset();
}

void ChessGameSession::replaceWith(ChessGameSession&& other)
{
    content_ = std::move(other.content_);
    state_ = std::move(other.state_);
    random_ = std::move(other.random_);
    journal_ = std::move(other.journal_);
    lastBattleResult_ = std::move(other.lastBattleResult_);
    lastBattleRuntime_ = std::move(other.lastBattleRuntime_);
    lastBattlePrepared_ = std::move(other.lastBattlePrepared_);
    pendingTransition_ = std::move(other.pendingTransition_);
    replayExportEnabled_ = other.replayExportEnabled_;
}

ChessGameplayObservation ChessGameSession::observe() const
{
    ChessGameplayObservation observation;
    observation.phase = state_.phase;
    observation.difficulty = content_->difficulty();
    observation.options = state_.options;
    observation.money = state_.money;
    observation.experience = state_.experience;
    observation.experienceForNextLevel = ChessManagementRules::experienceForNextLevel(state_, *content_);
    observation.level = state_.level;
    observation.maximumDeployment = ChessManagementRules::maximumDeployment(state_, *content_);
    observation.fight = state_.fight;
    observation.campaignComplete = state_.campaignComplete;
    observation.shopLocked = state_.shopLocked;
    observation.freeShopRefreshAvailable = state_.freeShopRefreshAvailable;
    observation.freeShopRefreshGrantedFight = state_.freeShopRefreshGrantedFight;
    observation.shop = state_.shop;
    for (const auto& [id, piece] : state_.roster)
    {
        observation.roster.push_back(piece);
    }
    for (const auto& [id, equipment] : state_.equipmentInventory)
    {
        observation.equipmentInventory.push_back(equipment);
    }
    observation.bans.assign(state_.bannedRoleIds.begin(), state_.bannedRoleIds.end());
    observation.seenRoles.assign(state_.seenRoleIds.begin(), state_.seenRoleIds.end());
    observation.obtainedNeigongIds.assign(
        state_.obtainedNeigongIds.begin(),
        state_.obtainedNeigongIds.end());
    observation.completedChallengeIds.assign(
        state_.completedChallengeIds.begin(),
        state_.completedChallengeIds.end());
    for (const auto& combo : content_->combos())
    {
        const auto progress = evaluateChessComboProgress(state_, *content_, combo);
        observation.combos.push_back({
            combo.id,
            progress.physicalCount,
            progress.effectiveCount,
            progress.activeThresholdIndex,
            progress.nextThresholdIndex,
        });
    }
    observation.preparedBattle = state_.preparedBattle;
    if (!state_.pendingRewards.empty())
    {
        observation.pendingReward = state_.pendingRewards.front();
    }
    observation.lastBattleOutcome = state_.lastBattleOutcome;
    observation.lastBattleEndFrame = state_.lastBattleEndFrame;
    observation.lastBattleDigest = state_.lastBattleDigest;
    observation.stateHash = canonicalChessStateHash(state_, random_);
    return observation;
}

std::vector<ChessLegalActionDescriptor> ChessGameSession::legalActions() const
{
    if (state_.phase == ChessSessionPhase::Complete)
    {
        return {};
    }
    if (state_.phase == ChessSessionPhase::BattlePreparation)
    {
        std::vector<ChessLegalActionDescriptor> result;
        assert(state_.preparedBattle);
        if (state_.preparedBattle->chosenMapId < 0
            && !state_.preparedBattle->mapCandidates.empty())
        {
            ChessLegalActionDescriptor choose{ChessActionType::ChooseMap};
            choose.candidateIds = state_.preparedBattle->mapCandidates;
            result.push_back(std::move(choose));
        }
        if (state_.options.positionSwapEnabled)
        {
            ChessLegalActionDescriptor swap{ChessActionType::SwapPositions};
            for (const auto& unit : state_.preparedBattle->units)
            {
                if (unit.team == 0)
                {
                    swap.candidateIds.push_back(unit.unitId);
                }
            }
            result.push_back(std::move(swap));
        }
        if (state_.preparedBattle->chosenMapId >= 0 || state_.preparedBattle->mapCandidates.empty())
        {
            result.push_back({ChessActionType::StartBattle});
        }
        return result;
    }
    if (state_.phase == ChessSessionPhase::RewardChoice)
    {
        assert(!state_.pendingRewards.empty());
        const auto& pending = state_.pendingRewards.front();
        if (pending.kind == ChessRewardKind::ForcedBan)
        {
            ChessLegalActionDescriptor ban{ChessActionType::AddBan};
            for (const auto& option : pending.options)
            {
                ChessAction action;
                action.type = ChessActionType::AddBan;
                action.roleId = option.value;
                if (validateAction(action) == ChessRuleErrorCode::None)
                {
                    ban.candidateIds.push_back(option.value);
                }
            }
            return {std::move(ban), {ChessActionType::SkipForcedBans}};
        }
        ChessLegalActionDescriptor choose{ChessActionType::ChooseReward};
        for (const auto& option : pending.options)
        {
            choose.candidateStableIds.push_back(option.id);
        }
        std::vector<ChessLegalActionDescriptor> result{std::move(choose)};
        ChessAction reroll;
        reroll.type = ChessActionType::RerollReward;
        if (validateAction(reroll) == ChessRuleErrorCode::None)
        {
            result.push_back({ChessActionType::RerollReward});
        }
        return result;
    }
    if (state_.phase != ChessSessionPhase::Management)
    {
        return {};
    }
    std::vector<ChessLegalActionDescriptor> result;
    ChessAction action;
    action.type = ChessActionType::RefreshShop;
    if (validateAction(action) == ChessRuleErrorCode::None)
    {
        result.push_back({ChessActionType::RefreshShop});
    }
    result.push_back({ChessActionType::SetShopLocked});
    ChessLegalActionDescriptor buy{ChessActionType::BuyShopSlot};
    for (int slot = 0; slot < static_cast<int>(state_.shop.size()); ++slot)
    {
        action = {};
        action.type = ChessActionType::BuyShopSlot;
        action.shopSlot = slot;
        if (validateAction(action) == ChessRuleErrorCode::None)
        {
            buy.candidateIds.push_back(slot);
        }
    }
    if (!buy.candidateIds.empty())
    {
        result.push_back(std::move(buy));
    }
    ChessLegalActionDescriptor sell{ChessActionType::SellChess};
    ChessLegalActionDescriptor deployment{ChessActionType::SetDeployment};
    deployment.maximumSelection = ChessManagementRules::maximumDeployment(state_, *content_);
    for (const auto& [id, piece] : state_.roster)
    {
        sell.candidateIds.push_back(id);
        deployment.candidateIds.push_back(id);
    }
    if (!sell.candidateIds.empty())
    {
        result.push_back(std::move(sell));
    }
    result.push_back(std::move(deployment));
    if (state_.bannedRoleIds.size()
        < static_cast<std::size_t>(ChessManagementRules::maximumBanCount(state_, *content_)))
    {
        ChessLegalActionDescriptor ban{ChessActionType::AddBan};
        for (const int roleId : state_.seenRoleIds)
        {
            action = {};
            action.type = ChessActionType::AddBan;
            action.roleId = roleId;
            if (validateAction(action) == ChessRuleErrorCode::None)
            {
                ban.candidateIds.push_back(roleId);
            }
        }
        if (!ban.candidateIds.empty())
        {
            result.push_back(std::move(ban));
        }
    }
    result.push_back({ChessActionType::SetPositionSwapEnabled});
    action = {};
    action.type = ChessActionType::RerollEnemySeed;
    if (validateAction(action) == ChessRuleErrorCode::None)
    {
        result.push_back({ChessActionType::RerollEnemySeed});
    }
    if (!state_.equipmentInventory.empty() && !state_.roster.empty())
    {
        ChessLegalActionDescriptor equip{ChessActionType::Equip};
        for (const auto& [id, equipment] : state_.equipmentInventory)
        {
            equip.candidateIds.push_back(id);
        }
        result.push_back(std::move(equip));
    }
    if (content_->balance().legendaryShop.unlockFight > 0
        && state_.fight >= content_->balance().legendaryShop.unlockFight)
    {
        ChessLegalActionDescriptor legendary{ChessActionType::BuyLegendaryEquipment};
        for (const auto& equipment : content_->equipment())
        {
            if (equipment.tier == 4)
            {
                action = {};
                action.type = ChessActionType::BuyLegendaryEquipment;
                action.itemId = equipment.itemId;
                if (validateAction(action) == ChessRuleErrorCode::None)
                {
                    legendary.candidateIds.push_back(equipment.itemId);
                }
            }
        }
        if (!legendary.candidateIds.empty())
        {
            result.push_back(std::move(legendary));
        }
    }
    action = {};
    action.type = ChessActionType::BuyExp;
    if (validateAction(action) == ChessRuleErrorCode::None)
    {
        result.push_back({ChessActionType::BuyExp});
    }
    const int deployed = static_cast<int>(std::ranges::count_if(state_.roster, [](const auto& entry) {
        return entry.second.deployed;
    }));
    if (deployed > 0)
    {
        if (!state_.campaignComplete)
        {
            action = {};
            action.type = ChessActionType::PrepareBattle;
            if (validateAction(action) == ChessRuleErrorCode::None)
            {
                result.push_back({ChessActionType::PrepareBattle});
            }
        }
        ChessLegalActionDescriptor challenge{ChessActionType::StartChallenge};
        for (const auto& definition : content_->balance().challenges)
        {
            action = {};
            action.type = ChessActionType::StartChallenge;
            action.challengeId = definition.id;
            if (validateAction(action) == ChessRuleErrorCode::None)
            {
                challenge.candidateStableIds.push_back(definition.id);
            }
        }
        if (!challenge.candidateStableIds.empty())
        {
            result.push_back(std::move(challenge));
        }
    }
    if (state_.campaignComplete)
    {
        result.push_back({ChessActionType::FinishRun});
    }
    return result;
}

std::string ChessGameSession::errorDescription(ChessRuleErrorCode error)
{
    switch (error)
    {
    case ChessRuleErrorCode::None: return {};
    case ChessRuleErrorCode::WrongPhase: return "目前階段不允許此操作";
    case ChessRuleErrorCode::TransitionPending: return "自動流程尚未完成";
    case ChessRuleErrorCode::UnsupportedAction: return "尚未支援此操作";
    case ChessRuleErrorCode::InvalidShopSlot: return "商店欄位不存在";
    case ChessRuleErrorCode::EmptyShopSlot: return "商店欄位已售出";
    case ChessRuleErrorCode::InsufficientGold: return "金幣不足";
    case ChessRuleErrorCode::BenchFull: return "備戰區已滿";
    case ChessRuleErrorCode::UnknownChessInstance: return "棋子實例不存在";
    case ChessRuleErrorCode::DuplicateIdentifier: return "識別碼重複";
    case ChessRuleErrorCode::DeploymentLimitExceeded: return "出戰人數超過上限";
    case ChessRuleErrorCode::MaximumLevel: return "已達最高等級";
    case ChessRuleErrorCode::InvalidRole: return "角色資料不存在";
    case ChessRuleErrorCode::BanLimitReached: return "禁棋數量已達上限";
    case ChessRuleErrorCode::RoleNotSeen: return "尚未遇過此角色";
    case ChessRuleErrorCode::RoleAlreadyBanned: return "此角色已被禁用";
    case ChessRuleErrorCode::DeploymentRequired: return "出戰人數不足";
    case ChessRuleErrorCode::InvalidMap: return "戰場選項不存在";
    case ChessRuleErrorCode::InvalidSwap: return "佈陣交換無效";
    case ChessRuleErrorCode::UnknownEquipmentInstance: return "裝備實例不存在";
    case ChessRuleErrorCode::InvalidEquipment: return "裝備資料不存在";
    case ChessRuleErrorCode::EquipmentTypeMismatch: return "裝備類型不符";
    case ChessRuleErrorCode::LegendaryShopLocked: return "神兵商店尚未開放";
    case ChessRuleErrorCode::InvalidReward: return "獎勵選項不存在";
    case ChessRuleErrorCode::RewardRerollUnavailable: return "此獎勵不可再次刷新";
    case ChessRuleErrorCode::UnknownChallenge: return "遠征挑戰不存在";
    case ChessRuleErrorCode::ChallengeAlreadyPending: return "已有遠征挑戰待處理";
    case ChessRuleErrorCode::CampaignAlreadyComplete: return "主線戰役已通關";
    case ChessRuleErrorCode::CampaignNotComplete: return "尚未通關，不能結束本局";
    case ChessRuleErrorCode::NoPreparedBattle: return "尚未準備戰鬥";
    }
    std::unreachable();
}

ChessRuleErrorCode ChessGameSession::validateAction(const ChessAction& action) const
{
    const auto deployedCount = static_cast<int>(std::ranges::count_if(state_.roster, [](const auto& entry) {
        return entry.second.deployed;
    }));
    if (state_.phase == ChessSessionPhase::Management)
    {
        if (action.type == ChessActionType::PrepareBattle)
        {
            if (state_.campaignComplete)
            {
                return ChessRuleErrorCode::CampaignAlreadyComplete;
            }
            return deployedCount == 0
                ? ChessRuleErrorCode::DeploymentRequired
                : ChessRuleErrorCode::None;
        }
        if (action.type == ChessActionType::StartChallenge)
        {
            if (deployedCount == 0)
            {
                return ChessRuleErrorCode::DeploymentRequired;
            }
            return std::ranges::contains(
                content_->balance().challenges,
                action.challengeId,
                &BalanceConfig::ChallengeDef::id)
                ? ChessRuleErrorCode::None
                : ChessRuleErrorCode::UnknownChallenge;
        }
        if (action.type == ChessActionType::FinishRun)
        {
            return state_.campaignComplete
                ? ChessRuleErrorCode::None
                : ChessRuleErrorCode::CampaignNotComplete;
        }
        return ChessManagementRules::validate(state_, *content_, action);
    }
    if (state_.phase == ChessSessionPhase::BattlePreparation)
    {
        if (!state_.preparedBattle)
        {
            return ChessRuleErrorCode::NoPreparedBattle;
        }
        if (action.type == ChessActionType::ChooseMap)
        {
            return state_.preparedBattle->chosenMapId < 0
                && !state_.preparedBattle->mapCandidates.empty()
                && std::ranges::contains(state_.preparedBattle->mapCandidates, action.mapId)
                ? ChessRuleErrorCode::None
                : ChessRuleErrorCode::InvalidMap;
        }
        if (action.type == ChessActionType::SwapPositions)
        {
            if (!state_.options.positionSwapEnabled || action.chessInstanceId == action.targetChessInstanceId)
            {
                return ChessRuleErrorCode::InvalidSwap;
            }
            const auto first = std::ranges::find(
                state_.preparedBattle->units,
                action.chessInstanceId,
                &PreparedChessBattleUnit::unitId);
            const auto second = std::ranges::find(
                state_.preparedBattle->units,
                action.targetChessInstanceId,
                &PreparedChessBattleUnit::unitId);
            return first != state_.preparedBattle->units.end()
                && second != state_.preparedBattle->units.end()
                && first->team == 0
                && second->team == 0
                ? ChessRuleErrorCode::None
                : ChessRuleErrorCode::InvalidSwap;
        }
        if (action.type == ChessActionType::StartBattle)
        {
            return state_.preparedBattle->chosenMapId >= 0
                || state_.preparedBattle->mapCandidates.empty()
                ? ChessRuleErrorCode::None
                : ChessRuleErrorCode::InvalidMap;
        }
        return ChessRuleErrorCode::UnsupportedAction;
    }
    if (state_.phase == ChessSessionPhase::RewardChoice)
    {
        if (!state_.pendingRewards.empty()
            && state_.pendingRewards.front().kind == ChessRewardKind::ForcedBan)
        {
            return ChessManagementRules::validate(state_, *content_, action);
        }
        return ChessRewardRules::validate(state_, *content_, action);
    }
    return ChessRuleErrorCode::WrongPhase;
}

void ChessGameSession::applyAction(
    const ChessAction& action,
    std::vector<ChessSemanticEvent>& events)
{
    if (state_.phase == ChessSessionPhase::Management)
    {
        if (action.type == ChessActionType::PrepareBattle)
        {
            state_.preparedBattle = ChessBattlePlanner::prepareCampaign(state_, *content_, random_);
            state_.phase = ChessSessionPhase::BattlePreparation;
            events.push_back({ChessSemanticEventType::BattlePrepared, {}, {}, {}, state_.preparedBattle->stableBattleId});
            return;
        }
        if (action.type == ChessActionType::StartChallenge)
        {
            const auto found = std::ranges::find(
                content_->balance().challenges,
                action.challengeId,
                &BalanceConfig::ChallengeDef::id);
            state_.preparedBattle = ChessBattlePlanner::prepareChallenge(
                state_, *content_, random_, *found);
            state_.phase = ChessSessionPhase::BattlePreparation;
            events.push_back({ChessSemanticEventType::BattlePrepared, {}, {}, {}, action.challengeId});
            return;
        }
        if (action.type == ChessActionType::FinishRun)
        {
            state_.phase = ChessSessionPhase::Complete;
            events.push_back({ChessSemanticEventType::RunFinished});
            return;
        }
        ChessManagementRules::apply(state_, *content_, random_, action, events);
        return;
    }
    if (state_.phase == ChessSessionPhase::BattlePreparation)
    {
        if (action.type == ChessActionType::ChooseMap)
        {
            state_.preparedBattle->chosenMapId = action.mapId;
            BattleSetupFactory::populateBaseFormation(*state_.preparedBattle, *content_);
            events.push_back({ChessSemanticEventType::MapChosen, action.mapId});
            return;
        }
        if (action.type == ChessActionType::SwapPositions)
        {
            state_.preparedBattle->formationSwaps.emplace_back(
                action.chessInstanceId,
                action.targetChessInstanceId);
            events.push_back({
                ChessSemanticEventType::FormationSwapped,
                action.chessInstanceId,
                action.targetChessInstanceId,
            });
            return;
        }
        assert(action.type == ChessActionType::StartBattle);
        std::unreachable();
    }
    if (state_.phase == ChessSessionPhase::RewardChoice)
    {
        if (state_.pendingRewards.front().kind == ChessRewardKind::ForcedBan)
        {
            ChessManagementRules::apply(state_, *content_, random_, action, events);
        }
        else
        {
            ChessRewardRules::apply(state_, *content_, random_, action, events);
        }
        return;
    }
    std::unreachable();
}

ChessActionResult ChessGameSession::finalizeAction(
    ChessSessionPhase phase,
    const ChessAction& action,
    const ChessSha256& preStateHash,
    std::vector<ChessSemanticEvent> events)
{
    ChessActionResult result;
    result.accepted = true;
    result.preStateHash = preStateHash;
    result.events = std::move(events);
    result.postStateHash = canonicalChessStateHash(state_, random_);
    result.eventHash = canonicalChessEventHash(result.events);
    result.rngDigest = canonicalChessRngDigest(random_);
    const auto& record = journal_.append(
        phase,
        action,
        result.preStateHash,
        result.postStateHash,
        result.eventHash,
        result.rngDigest);
    result.replaySequence = record.sequence;
    result.chainHash = record.chainHash;
    return result;
}

ChessActionResult ChessGameSession::beginAction(const ChessAction& action)
{
    ChessActionResult result;
    result.preStateHash = canonicalChessStateHash(state_, random_);
    if (pendingTransition_)
    {
        result.error = ChessRuleErrorCode::TransitionPending;
        result.description = errorDescription(result.error);
        result.postStateHash = result.preStateHash;
        result.rngDigest = canonicalChessRngDigest(random_);
        result.chainHash = journal_.chainHash();
        return result;
    }
    result.error = validateAction(action);
    if (result.error != ChessRuleErrorCode::None)
    {
        result.description = errorDescription(result.error);
        result.postStateHash = result.preStateHash;
        result.rngDigest = canonicalChessRngDigest(random_);
        result.chainHash = journal_.chainHash();
        return result;
    }

    const auto phase = state_.phase;
    std::vector<ChessSemanticEvent> events;
    if (action.type != ChessActionType::StartBattle)
    {
        applyAction(action, events);
        return finalizeAction(phase, action, result.preStateHash, std::move(events));
    }

    assert(state_.preparedBattle);
    lastBattlePrepared_ = state_.preparedBattle;
    state_.phase = ChessSessionPhase::BattleResolution;
    events.push_back({ChessSemanticEventType::BattleStarted, {}, {}, {}, state_.preparedBattle->stableBattleId});
    auto input = BattleSetupFactory::build(
        *state_.preparedBattle,
        *content_,
        state_.obtainedNeigongIds,
        kChessBattleFrameLimit);
    auto creation = Battle::BattleRuntimeSession::createInitialized(std::move(input));
    pendingTransition_ = std::make_unique<PendingTransition>(
        phase,
        action,
        result.preStateHash,
        std::move(events),
        std::move(creation));
    result.accepted = true;
    result.transitionPending = true;
    result.rngDigest = canonicalChessRngDigest(random_);
    result.chainHash = journal_.chainHash();
    return result;
}

ChessAutomaticAdvanceResult ChessGameSession::advanceAutomatic(int frameBudget)
{
    assert(frameBudget > 0);
    ChessAutomaticAdvanceResult result;
    if (!pendingTransition_)
    {
        return result;
    }

    while (result.framesAdvanced < frameBudget
        && !pendingTransition_->runtime->runtime().result.ended)
    {
        auto frame = pendingTransition_->runtime->runFrame();
        pendingTransition_->collector.consumeFrame(frame, *pendingTransition_->runtime);
        auto digest = Battle::battleDigestEvents(frame);
        pendingTransition_->digestEvents.insert(
            pendingTransition_->digestEvents.end(),
            digest.begin(),
            digest.end());
        result.frames.push_back(std::move(frame));
        ++result.framesAdvanced;
    }
    if (!pendingTransition_->runtime->runtime().result.ended)
    {
        return result;
    }

    auto battle = std::make_shared<HeadlessBattleResult>();
    battle->initialization = pendingTransition_->initialization;
    battle->digestEvents = std::move(pendingTransition_->digestEvents);
    battle->report = pendingTransition_->collector.report();
    battle->summary = BattleSummaryBuilder::build(
        *pendingTransition_->runtime,
        battle->report);
    battle->finalRuntime = pendingTransition_->runtime->runtime();
    battle->digest = HeadlessBattleRunner::digest(*battle);
    ChessProgressionRules::applyBattleResult(
        state_,
        *content_,
        random_,
        *battle,
        pendingTransition_->events);
    lastBattleResult_ = std::move(battle);
    lastBattleRuntime_ = pendingTransition_->runtime;

    const auto phase = pendingTransition_->phase;
    auto action = std::move(pendingTransition_->action);
    const auto preStateHash = pendingTransition_->preStateHash;
    auto events = std::move(pendingTransition_->events);
    pendingTransition_.reset();
    result.completedAction = finalizeAction(
        phase,
        action,
        preStateHash,
        std::move(events));
    return result;
}

ChessActionResult ChessGameSession::submitAndDrain(const ChessAction& action)
{
    auto result = beginAction(action);
    if (!result.transitionPending)
    {
        return result;
    }
    for (;;)
    {
        auto advance = advanceAutomatic(std::numeric_limits<int>::max());
        if (advance.completedAction)
        {
            return std::move(*advance.completedAction);
        }
    }
}

std::optional<ChessReplay> ChessGameSession::exportReplay() const
{
    if (!replayExportEnabled_ || !isStableDecisionBoundary())
    {
        return std::nullopt;
    }
    return journal_.exportReplay(state_, canonicalChessStateHash(state_, random_));
}

}
