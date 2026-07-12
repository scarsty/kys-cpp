#include "ChessJsonProtocol.h"

#include "ChessReplayJson.h"
#include "ChessReplayVerifier.h"

#include <glaze/json.hpp>

#include <cassert>
#include <charconv>
#include <algorithm>
#include <optional>
#include <utility>

namespace KysChess
{
namespace ProtocolDetail
{

struct RequestDto
{
    glz::raw_json id = "null";
    std::string method;
    glz::raw_json params = "{}";
};

struct ResponseDto
{
    glz::raw_json id = "null";
    bool ok = false;
    std::optional<glz::raw_json> result;
    std::optional<std::string> error_code;
    std::optional<std::string> error_message;
};

struct NewParams
{
    std::string difficulty = "normal";
    std::string seed = "0x0000000000000001";
    std::optional<bool> position_swap_enabled;
};

struct ActParams
{
    glz::raw_json action;
};

struct VerifyParams
{
    std::string replay_jsonl;
};

struct SlotParams { std::string slot; };
struct SaveParams { std::string slot; std::string label; };
struct ImportSaveParams { std::string slot; std::string payload; };

template<typename T>
std::optional<T> readJson(std::string_view json)
{
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    T value;
    if (const auto result = glz::read<options>(value, json); result)
    {
        return std::nullopt;
    }
    return value;
}

template<typename T>
std::string writeJson(const T& value)
{
    auto result = glz::write_json(value);
    return result ? std::move(result.value()) : std::string{};
}

std::optional<Difficulty> parseDifficulty(std::string_view value)
{
    if (value == "easy") return Difficulty::Easy;
    if (value == "normal") return Difficulty::Normal;
    if (value == "hard") return Difficulty::Hard;
    return std::nullopt;
}

std::optional<std::uint64_t> parseSeed(std::string_view value)
{
    if (value.size() != 18 || !value.starts_with("0x"))
    {
        return std::nullopt;
    }
    std::uint64_t seed{};
    const auto result = std::from_chars(value.data() + 2, value.data() + value.size(), seed, 16);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size()
        ? std::optional(seed)
        : std::nullopt;
}

std::string response(
    const glz::raw_json& id,
    bool ok,
    std::string result,
    std::string errorCode = {},
    std::string errorMessage = {})
{
    ResponseDto dto;
    dto.id = id;
    dto.ok = ok;
    if (ok)
    {
        dto.result = glz::raw_json(std::move(result));
    }
    else
    {
        dto.error_code = std::move(errorCode);
        dto.error_message = std::move(errorMessage);
    }
    return writeJson(dto);
}

struct ShopSlotDto { int slot{}; int role_id{}; int tier{}; };
struct PieceDto { int instance_id{}; int role_id{}; int star{}; bool deployed{}; int fights_won{}; };
struct EquipmentDto { int instance_id{}; int item_id{}; int assigned_chess_instance_id{}; };
struct PreparedUnitDto { int unit_id{}; int chess_instance_id{}; int role_id{}; int team{}; int star{}; int weapon_item_id{}; int armor_item_id{}; int x{}; int y{}; };
struct PreparedBattleDto
{
    std::string battle_id;
    std::vector<PreparedUnitDto> units;
    std::vector<int> map_candidates;
    int chosen_map_id = -1;
    std::uint32_t battle_seed{};
};
struct BattleDigestEventDto
{
    std::string type;
    int frame{};
    int source_unit_id = -1;
    int target_unit_id = -1;
    int amount{};
    int stable_effect_id = -1;
    int skill_id = -1;
    int status_id = -1;
    int resource_id = -1;
    int related_attack_id = -1;
};
struct BattleSurvivorDto
{
    int unit_id = -1;
    int chess_instance_id = -1;
    int role_id = -1;
    int team = -1;
    int hp{};
    int mp{};
};
struct BattleResultDto
{
    PreparedBattleDto initial_board;
    std::vector<BattleDigestEventDto> events;
    std::string outcome;
    int end_frame{};
    std::vector<BattleSurvivorDto> survivors;
    std::string digest;
};
struct RewardOptionDto { std::string id; int kind{}; int value{}; int value2{}; };
struct PendingRewardDto
{
    std::string id;
    int kind{};
    std::vector<RewardOptionDto> options;
    int reroll_cost{};
    std::vector<int> eligible_tiers;
    bool rerolled{};
};
struct ComboDto
{
    int combo_id{};
    int physical_count{};
    int effective_count{};
    int active_threshold_index = -1;
    int next_threshold_index = -1;
};
struct ObservationDto
{
    std::string phase;
    std::string difficulty;
    bool position_swap_enabled{};
    int money{};
    int experience{};
    int experience_for_next_level{};
    int level{};
    int maximum_deployment{};
    int fight{};
    bool campaign_complete{};
    bool shop_locked{};
    bool free_shop_refresh_available{};
    int free_shop_refresh_granted_fight = -1;
    std::vector<ShopSlotDto> shop;
    std::vector<PieceDto> roster;
    std::vector<EquipmentDto> equipment_inventory;
    std::vector<int> bans;
    std::vector<int> seen_roles;
    std::vector<int> obtained_neigong_ids;
    std::vector<std::string> completed_challenge_ids;
    std::vector<ComboDto> combos;
    std::optional<PreparedBattleDto> prepared_battle;
    std::optional<PendingRewardDto> pending_reward;
    int last_battle_outcome{};
    int last_battle_end_frame{};
    std::string last_battle_digest;
    std::string state_hash;
};

std::string phaseText(ChessSessionPhase phase)
{
    switch (phase)
    {
    case ChessSessionPhase::Management: return "Management";
    case ChessSessionPhase::BattlePreparation: return "BattlePreparation";
    case ChessSessionPhase::BattleResolution: return "BattleResolution";
    case ChessSessionPhase::RewardChoice: return "RewardChoice";
    case ChessSessionPhase::Complete: return "Complete";
    }
    std::unreachable();
}

PreparedBattleDto preparedBattleDto(const PreparedChessBattle& battle)
{
    PreparedBattleDto prepared;
    prepared.battle_id = battle.stableBattleId;
    prepared.map_candidates = battle.mapCandidates;
    prepared.chosen_map_id = battle.chosenMapId;
    prepared.battle_seed = battle.battleSeed;
    for (const auto& unit : battle.units)
    {
        prepared.units.push_back({
            unit.unitId,
            unit.chessInstanceId,
            unit.roleId,
            unit.team,
            unit.star,
            unit.weaponItemId,
            unit.armorItemId,
            unit.x,
            unit.y,
        });
    }
    for (const auto& [firstId, secondId] : battle.formationSwaps)
    {
        auto first = std::ranges::find(prepared.units, firstId, &PreparedUnitDto::unit_id);
        auto second = std::ranges::find(prepared.units, secondId, &PreparedUnitDto::unit_id);
        assert(first != prepared.units.end() && second != prepared.units.end());
        std::swap(first->x, second->x);
        std::swap(first->y, second->y);
    }
    return prepared;
}

ObservationDto observationDto(const ChessGameplayObservation& observation)
{
    ObservationDto dto;
    dto.phase = phaseText(observation.phase);
    dto.difficulty = observation.difficulty == Difficulty::Easy
        ? "easy"
        : observation.difficulty == Difficulty::Normal ? "normal" : "hard";
    dto.position_swap_enabled = observation.options.positionSwapEnabled;
    dto.money = observation.money;
    dto.experience = observation.experience;
    dto.experience_for_next_level = observation.experienceForNextLevel;
    dto.level = observation.level;
    dto.maximum_deployment = observation.maximumDeployment;
    dto.fight = observation.fight;
    dto.campaign_complete = observation.campaignComplete;
    dto.shop_locked = observation.shopLocked;
    dto.free_shop_refresh_available = observation.freeShopRefreshAvailable;
    dto.free_shop_refresh_granted_fight = observation.freeShopRefreshGrantedFight;
    for (int index = 0; index < static_cast<int>(observation.shop.size()); ++index)
    {
        dto.shop.push_back({index, observation.shop[index].roleId, observation.shop[index].tier});
    }
    for (const auto& piece : observation.roster)
    {
        dto.roster.push_back({piece.instanceId, piece.roleId, piece.star, piece.deployed, piece.fightsWon});
    }
    for (const auto& equipment : observation.equipmentInventory)
    {
        dto.equipment_inventory.push_back({
            equipment.instanceId,
            equipment.itemId,
            equipment.assignedChessInstanceId,
        });
    }
    dto.bans = observation.bans;
    dto.seen_roles = observation.seenRoles;
    dto.obtained_neigong_ids = observation.obtainedNeigongIds;
    dto.completed_challenge_ids = observation.completedChallengeIds;
    for (const auto& combo : observation.combos)
    {
        dto.combos.push_back({
            combo.comboId,
            combo.physicalCount,
            combo.effectiveCount,
            combo.activeThresholdIndex,
            combo.nextThresholdIndex,
        });
    }
    if (observation.preparedBattle)
    {
        dto.prepared_battle = preparedBattleDto(*observation.preparedBattle);
    }
    if (observation.pendingReward)
    {
        PendingRewardDto pending;
        pending.id = observation.pendingReward->id;
        pending.kind = static_cast<int>(observation.pendingReward->kind);
        pending.reroll_cost = observation.pendingReward->rerollCost;
        pending.eligible_tiers = observation.pendingReward->eligibleTiers;
        pending.rerolled = observation.pendingReward->rerolled;
        for (const auto& option : observation.pendingReward->options)
        {
            pending.options.push_back({
                option.id,
                static_cast<int>(option.kind),
                option.value,
                option.value2,
            });
        }
        dto.pending_reward = std::move(pending);
    }
    dto.last_battle_outcome = static_cast<int>(observation.lastBattleOutcome);
    dto.last_battle_end_frame = observation.lastBattleEndFrame;
    dto.last_battle_digest = chessSha256Hex(observation.lastBattleDigest);
    dto.state_hash = chessSha256Hex(observation.stateHash);
    return dto;
}

struct LegalActionDto
{
    std::string type;
    std::vector<int> candidate_ids;
    std::vector<std::string> candidate_stable_ids;
    int minimum_selection{};
    int maximum_selection{};
};

struct SaveSlotDto
{
    std::string slot;
    bool occupied{};
    std::uint64_t revision{};
    std::string label;
    int fight{};
    int level{};
    int money{};
    int roster_count{};
    std::uint64_t replay_sequence{};
    std::string state_hash;
    bool compatible{};
};

struct SessionObservationDto
{
    ObservationDto game_state;
    std::vector<SaveSlotDto> save_slots;
    std::vector<std::string> operations;
    std::string load_consequence;
};

struct SemanticEventDto
{
    std::string type;
    int primary_id{};
    int secondary_id{};
    int value{};
    std::string stable_id;
};

struct ActionResultDto
{
    bool accepted{};
    std::string error_code;
    std::string description;
    std::vector<SemanticEventDto> events;
    ObservationDto next_observation;
    std::optional<BattleResultDto> battle;
    std::uint64_t replay_sequence{};
    std::string pre_state_hash;
    std::string post_state_hash;
    std::string event_hash;
    std::string rng_digest;
    std::string chain_hash;
};

struct ReplayExportDto { std::string replay_jsonl; };
struct ReplayVerificationDto
{
    bool valid{};
    int mismatch{};
    std::uint64_t sequence{};
    std::string message;
};

struct SaveResultDto { std::string slot; std::uint64_t revision{}; };
struct InspectSaveDto { SaveSlotDto summary; std::string payload; };
struct ExportSaveDto { std::string payload; };
struct TimelineReplacementDto
{
    std::string loaded_slot;
    std::uint64_t previous_sequence{};
    std::uint64_t restored_sequence{};
    std::uint64_t discarded_active_actions{};
    std::string current_state_hash;
};

std::string ruleErrorId(ChessRuleErrorCode error)
{
    switch (error)
    {
    case ChessRuleErrorCode::None: return "none";
    case ChessRuleErrorCode::WrongPhase: return "wrong_phase";
    case ChessRuleErrorCode::TransitionPending: return "transition_pending";
    case ChessRuleErrorCode::UnsupportedAction: return "unsupported_action";
    case ChessRuleErrorCode::InvalidShopSlot: return "invalid_shop_slot";
    case ChessRuleErrorCode::EmptyShopSlot: return "empty_shop_slot";
    case ChessRuleErrorCode::InsufficientGold: return "insufficient_gold";
    case ChessRuleErrorCode::BenchFull: return "bench_full";
    case ChessRuleErrorCode::UnknownChessInstance: return "unknown_chess_instance";
    case ChessRuleErrorCode::DuplicateIdentifier: return "duplicate_identifier";
    case ChessRuleErrorCode::DeploymentLimitExceeded: return "deployment_limit_exceeded";
    case ChessRuleErrorCode::MaximumLevel: return "maximum_level";
    case ChessRuleErrorCode::InvalidRole: return "invalid_role";
    case ChessRuleErrorCode::BanLimitReached: return "ban_limit_reached";
    case ChessRuleErrorCode::RoleNotSeen: return "role_not_seen";
    case ChessRuleErrorCode::RoleAlreadyBanned: return "role_already_banned";
    case ChessRuleErrorCode::DeploymentRequired: return "deployment_required";
    case ChessRuleErrorCode::InvalidMap: return "invalid_map";
    case ChessRuleErrorCode::InvalidSwap: return "invalid_swap";
    case ChessRuleErrorCode::UnknownEquipmentInstance: return "unknown_equipment_instance";
    case ChessRuleErrorCode::InvalidEquipment: return "invalid_equipment";
    case ChessRuleErrorCode::EquipmentTypeMismatch: return "equipment_type_mismatch";
    case ChessRuleErrorCode::LegendaryShopLocked: return "legendary_shop_locked";
    case ChessRuleErrorCode::InvalidReward: return "invalid_reward";
    case ChessRuleErrorCode::RewardRerollUnavailable: return "reward_reroll_unavailable";
    case ChessRuleErrorCode::UnknownChallenge: return "unknown_challenge";
    case ChessRuleErrorCode::ChallengeAlreadyPending: return "challenge_already_pending";
    case ChessRuleErrorCode::CampaignAlreadyComplete: return "campaign_already_complete";
    case ChessRuleErrorCode::CampaignNotComplete: return "campaign_not_complete";
    case ChessRuleErrorCode::NoPreparedBattle: return "no_prepared_battle";
    }
    std::unreachable();
}

std::string semanticEventTypeId(ChessSemanticEventType type)
{
    switch (type)
    {
    case ChessSemanticEventType::ShopRefreshed: return "shop_refreshed";
    case ChessSemanticEventType::ShopLockChanged: return "shop_lock_changed";
    case ChessSemanticEventType::ChessPurchased: return "chess_purchased";
    case ChessSemanticEventType::ChessMerged: return "chess_merged";
    case ChessSemanticEventType::ChessSold: return "chess_sold";
    case ChessSemanticEventType::ExperiencePurchased: return "experience_purchased";
    case ChessSemanticEventType::LevelChanged: return "level_changed";
    case ChessSemanticEventType::DeploymentChanged: return "deployment_changed";
    case ChessSemanticEventType::RoleBanned: return "role_banned";
    case ChessSemanticEventType::PositionSwapOptionChanged: return "position_swap_option_changed";
    case ChessSemanticEventType::EnemyPlanRerolled: return "enemy_plan_rerolled";
    case ChessSemanticEventType::EquipmentAcquired: return "equipment_acquired";
    case ChessSemanticEventType::EquipmentAssigned: return "equipment_assigned";
    case ChessSemanticEventType::LegendaryEquipmentPurchased: return "legendary_equipment_purchased";
    case ChessSemanticEventType::BattlePrepared: return "battle_prepared";
    case ChessSemanticEventType::MapChosen: return "map_chosen";
    case ChessSemanticEventType::FormationSwapped: return "formation_swapped";
    case ChessSemanticEventType::BattleStarted: return "battle_started";
    case ChessSemanticEventType::BattleEnded: return "battle_ended";
    case ChessSemanticEventType::GoldAwarded: return "gold_awarded";
    case ChessSemanticEventType::FightAdvanced: return "fight_advanced";
    case ChessSemanticEventType::RewardOffered: return "reward_offered";
    case ChessSemanticEventType::RewardRerolled: return "reward_rerolled";
    case ChessSemanticEventType::RewardChosen: return "reward_chosen";
    case ChessSemanticEventType::InternalSkillAcquired: return "internal_skill_acquired";
    case ChessSemanticEventType::ChallengeCompleted: return "challenge_completed";
    case ChessSemanticEventType::CampaignCompleted: return "campaign_completed";
    case ChessSemanticEventType::RunFinished: return "run_finished";
    case ChessSemanticEventType::ForcedBansSkipped: return "forced_bans_skipped";
    case ChessSemanticEventType::FreeShopRefreshGranted: return "free_shop_refresh_granted";
    case ChessSemanticEventType::FreeShopRefreshConsumed: return "free_shop_refresh_consumed";
    case ChessSemanticEventType::ExperienceAwarded: return "experience_awarded";
    }
    std::unreachable();
}

std::string battleEventTypeId(Battle::BattleGameplayEventType type)
{
    switch (type)
    {
    case Battle::BattleGameplayEventType::CastStarted: return "cast_started";
    case Battle::BattleGameplayEventType::AttackSpawned: return "attack_spawned";
    case Battle::BattleGameplayEventType::ProjectileMoved: return "projectile_moved";
    case Battle::BattleGameplayEventType::ProjectileHit: return "projectile_hit";
    case Battle::BattleGameplayEventType::ProjectileExpired: return "projectile_expired";
    case Battle::BattleGameplayEventType::ProjectileCancelled: return "projectile_cancelled";
    case Battle::BattleGameplayEventType::DamageApplied: return "damage_applied";
    case Battle::BattleGameplayEventType::StatusApplied: return "status_applied";
    case Battle::BattleGameplayEventType::ResourceChanged: return "resource_changed";
    case Battle::BattleGameplayEventType::UnitDied: return "unit_died";
    case Battle::BattleGameplayEventType::BattleEnded: return "battle_ended";
    }
    std::unreachable();
}

std::string battleOutcomeId(Battle::BattleOutcome outcome)
{
    switch (outcome)
    {
    case Battle::BattleOutcome::InProgress: return "in_progress";
    case Battle::BattleOutcome::PlayerVictory: return "player_victory";
    case Battle::BattleOutcome::PlayerDefeat: return "player_defeat";
    case Battle::BattleOutcome::Timeout: return "timeout";
    }
    std::unreachable();
}

BattleResultDto battleResultDto(
    const PreparedChessBattle& prepared,
    const HeadlessBattleResult& battle)
{
    BattleResultDto dto;
    dto.initial_board = preparedBattleDto(prepared);
    for (const auto& event : battle.digestEvents)
    {
        dto.events.push_back({
            battleEventTypeId(event.type),
            event.frame,
            event.sourceUnitId,
            event.targetUnitId,
            event.amount,
            event.stableEffectId,
            event.skillId,
            static_cast<int>(event.statusId),
            static_cast<int>(event.resourceId),
            event.relatedAttackId,
        });
    }
    dto.outcome = battleOutcomeId(battle.summary.outcome);
    dto.end_frame = battle.summary.endFrame;
    for (const auto& survivor : battle.summary.survivors)
    {
        dto.survivors.push_back({
            survivor.unitId,
            survivor.chessInstanceId,
            survivor.roleId,
            survivor.team,
            survivor.hp,
            survivor.mp,
        });
    }
    dto.digest = chessSha256Hex(battle.digest);
    return dto;
}

std::string checkpointErrorId(ChessCheckpointError error)
{
    switch (error)
    {
    case ChessCheckpointError::None: return "none";
    case ChessCheckpointError::Malformed: return "save_not_found_or_malformed";
    case ChessCheckpointError::IncompatibleGameVersion: return "save_game_version_mismatch";
    case ChessCheckpointError::UnrepresentableSnapshot: return "save_snapshot_unrepresentable";
    case ChessCheckpointError::UnstableBoundary: return "unstable_boundary";
    }
    std::unreachable();
}

}

using namespace ProtocolDetail;

namespace
{

SaveSlotDto saveSlotDto(const ChessSaveSlotSummary& slot)
{
    return {
        slot.slotId,
        slot.occupied,
        slot.revision,
        slot.label,
        slot.fight,
        slot.level,
        slot.money,
        slot.rosterCount,
        slot.replaySequence,
        chessSha256Hex(slot.stateHash),
        slot.compatible,
    };
}

SessionObservationDto sessionObservationDto(
    const ChessGameSession& session,
    const ChessSaveStore& saves)
{
    SessionObservationDto result;
    result.game_state = observationDto(session.observe());
    for (const auto& slot : saves.list(session))
    {
        result.save_slots.push_back(saveSlotDto(slot));
    }
    result.operations = {
        "list_saves",
        "inspect_save",
        "save_game",
        "load_game",
        "export_save",
        "import_save",
        "export_replay",
        "verify_replay",
    };
    result.load_consequence = "載入會替換目前遊戲狀態、亂數與完整重播紀錄，捨棄存檔點之後的目前行動，但保留存檔目錄。";
    return result;
}

}

ChessJsonProtocol::ChessJsonProtocol(ContentProvider contentProvider)
    : contentProvider_(std::move(contentProvider))
{
}

ChessJsonProtocol::ChessJsonProtocol(std::shared_ptr<const ChessGameContent> fixedContent)
    : fixedContent_(std::move(fixedContent))
{
}

std::shared_ptr<const ChessGameContent> ChessJsonProtocol::loadContent(Difficulty difficulty)
{
    if (fixedContent_)
    {
        return fixedContent_->difficulty() == difficulty ? fixedContent_ : nullptr;
    }
    return contentProvider_ ? contentProvider_(difficulty) : nullptr;
}

std::string ChessJsonProtocol::handleLine(std::string_view requestJson)
{
    const auto request = readJson<RequestDto>(requestJson);
    if (!request)
    {
        return response(glz::raw_json("null"), false, {}, "malformed_request", "要求不是有效 JSON");
    }

    if (request->method == "new")
    {
        const auto params = readJson<NewParams>(request->params.str);
        const auto difficulty = params ? parseDifficulty(params->difficulty) : std::nullopt;
        const auto seed = params ? parseSeed(params->seed) : std::nullopt;
        if (!params || !difficulty || !seed)
        {
            return response(request->id, false, {}, "invalid_params", "難度或種子格式無效");
        }
        auto content = loadContent(*difficulty);
        if (!content)
        {
            return response(request->id, false, {}, "content_load_failed", "無法載入遊戲內容");
        }
        ChessSessionOptions options;
        if (params->position_swap_enabled)
        {
            options.positionSwapEnabled = *params->position_swap_enabled;
        }
        session_ = std::make_unique<ChessGameSession>(std::move(content), *seed, options);
        return response(request->id, true, writeJson(sessionObservationDto(*session_, saves_)));
    }

    if (request->method == "verify_replay")
    {
        const auto params = readJson<VerifyParams>(request->params.str);
        ChessReplayJsonError parseError;
        const auto replay = params ? parseChessReplayJsonl(params->replay_jsonl, parseError) : std::nullopt;
        if (!replay)
        {
            return response(request->id, false, {}, "malformed_replay", parseError.message);
        }
        const auto difficulty = parseDifficulty(replay->header.difficulty);
        if (!difficulty)
        {
            return response(request->id, true, writeJson(ReplayVerificationDto{
                false,
                static_cast<int>(ChessReplayMismatch::Header),
                0,
                "重播難度識別碼無效",
            }));
        }
        auto verificationContent = fixedContent_ ? fixedContent_ : loadContent(*difficulty);
        if (!verificationContent)
        {
            return response(request->id, false, {}, "content_load_failed", "無法載入驗證規則內容");
        }
        const auto verification = ChessReplayVerifier::verify(std::move(verificationContent), *replay);
        return response(request->id, true, writeJson(ReplayVerificationDto{
            verification.valid,
            static_cast<int>(verification.mismatch),
            verification.sequence,
            verification.message,
        }));
    }

    if (!session_)
    {
        return response(request->id, false, {}, "no_session", "請先建立遊戲工作階段");
    }
    if (request->method == "observe")
    {
        return response(request->id, true, writeJson(sessionObservationDto(*session_, saves_)));
    }
    if (request->method == "legal_actions")
    {
        std::vector<LegalActionDto> legal;
        for (const auto& descriptor : session_->legalActions())
        {
            legal.push_back({
                chessActionTypeId(descriptor.type),
                descriptor.candidateIds,
                descriptor.candidateStableIds,
                descriptor.minimumSelection,
                descriptor.maximumSelection,
            });
        }
        return response(request->id, true, writeJson(legal));
    }
    if (request->method == "act")
    {
        const auto params = readJson<ActParams>(request->params.str);
        const auto action = params ? parseChessActionJson(params->action.str) : std::nullopt;
        if (!action)
        {
            return response(request->id, false, {}, "invalid_action", "操作內容無效");
        }
        const auto result = session_->submitAndDrain(*action);
        ActionResultDto dto{
            result.accepted,
            ruleErrorId(result.error),
            result.description,
            {},
            observationDto(session_->observe()),
            std::nullopt,
            result.replaySequence,
            chessSha256Hex(result.preStateHash),
            chessSha256Hex(result.postStateHash),
            chessSha256Hex(result.eventHash),
            chessSha256Hex(result.rngDigest),
            chessSha256Hex(result.chainHash),
        };
        for (const auto& event : result.events)
        {
            dto.events.push_back({
                semanticEventTypeId(event.type),
                event.primaryId,
                event.secondaryId,
                event.value,
                event.stableId,
            });
        }
        if (result.accepted
            && action->type == ChessActionType::StartBattle
            && session_->lastBattlePrepared()
            && session_->lastBattleResult())
        {
            dto.battle = battleResultDto(
                *session_->lastBattlePrepared(),
                *session_->lastBattleResult());
        }
        return response(request->id, true, writeJson(dto));
    }
    if (request->method == "list_saves")
    {
        std::vector<SaveSlotDto> slots;
        for (const auto& slot : saves_.list(*session_))
        {
            slots.push_back(saveSlotDto(slot));
        }
        return response(request->id, true, writeJson(slots));
    }
    if (request->method == "save_game")
    {
        const auto params = readJson<SaveParams>(request->params.str);
        if (!params || params->slot.empty())
        {
            return response(request->id, false, {}, "invalid_params", "缺少存檔欄位");
        }
        const auto error = saves_.save(params->slot, *session_, params->label);
        if (error != ChessCheckpointError::None)
        {
            return response(request->id, false, {}, checkpointErrorId(error), "無法建立存檔");
        }
        const auto* checkpoint = saves_.inspect(params->slot);
        return response(request->id, true, writeJson(SaveResultDto{params->slot, checkpoint->saveRevision}));
    }
    if (request->method == "inspect_save")
    {
        const auto params = readJson<SlotParams>(request->params.str);
        const auto* checkpoint = params ? saves_.inspect(params->slot) : nullptr;
        if (!checkpoint)
        {
            return response(request->id, false, {}, "save_not_found", "存檔不存在");
        }
        const auto summaries = saves_.list(*session_);
        const auto summary = std::ranges::find(summaries, params->slot, &ChessSaveSlotSummary::slotId);
        return response(request->id, true, writeJson(InspectSaveDto{
            saveSlotDto(*summary),
            checkpoint->serializeJson(),
        }));
    }
    if (request->method == "load_game")
    {
        const auto params = readJson<SlotParams>(request->params.str);
        if (!params)
        {
            return response(request->id, false, {}, "invalid_params", "缺少存檔欄位");
        }
        ChessTimelineReplacement replacement;
        const auto error = saves_.load(params->slot, *session_, replacement);
        if (error != ChessCheckpointError::None)
        {
            return response(request->id, false, {}, checkpointErrorId(error), "無法載入存檔");
        }
        return response(request->id, true, writeJson(TimelineReplacementDto{
            replacement.loadedSlot,
            replacement.previousSequence,
            replacement.restoredSequence,
            replacement.discardedActiveActions,
            chessSha256Hex(replacement.currentStateHash),
        }));
    }
    if (request->method == "export_save")
    {
        const auto params = readJson<SlotParams>(request->params.str);
        const auto payload = params ? saves_.exportSave(params->slot) : std::nullopt;
        if (!payload)
        {
            return response(request->id, false, {}, "save_not_found", "存檔不存在");
        }
        return response(request->id, true, writeJson(ExportSaveDto{*payload}));
    }
    if (request->method == "import_save")
    {
        const auto params = readJson<ImportSaveParams>(request->params.str);
        if (!params)
        {
            return response(request->id, false, {}, "invalid_params", "匯入資料無效");
        }
        const auto error = saves_.importSave(
            params->slot,
            params->payload,
            session_->content().gameVersion());
        if (error != ChessCheckpointError::None)
        {
            return response(request->id, false, {}, checkpointErrorId(error), "無法匯入存檔");
        }
        const auto* checkpoint = saves_.inspect(params->slot);
        return response(request->id, true, writeJson(SaveResultDto{params->slot, checkpoint->saveRevision}));
    }
    if (request->method == "export_replay")
    {
        const auto replay = session_->exportReplay();
        if (!replay)
        {
            return response(request->id, false, {}, "unstable_boundary", "自動流程尚未完成，不能匯出重播");
        }
        return response(request->id, true, writeJson(ReplayExportDto{serializeChessReplayJsonl(*replay)}));
    }
    return response(request->id, false, {}, "unknown_method", "未知的通訊協定方法");
}

}
