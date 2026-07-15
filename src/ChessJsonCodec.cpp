#include "ChessJsonCodec.h"

#include "ChessAsciiBoard.h"
#include "ChessBattleAnalysis.h"
#include "ChessCatalogQueries.h"
#include "ChessGameQueries.h"
#include "ChessPreparedBattleAnalysis.h"
#include "ChessReplayJson.h"
#include "ChessRewardRules.h"
#include "ChessRuntimeConstants.h"

#include <glaze/json.hpp>

#include <algorithm>
#include <cassert>
#include <charconv>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <utility>

namespace KysChess::ProtocolDetail
{

std::optional<ObservationDetail> parseObservationDetail(std::string_view value)
{
    if (value == "compact") return ObservationDetail::Compact;
    if (value == "full") return ObservationDetail::Full;
    return std::nullopt;
}

std::optional<ActionResponseDetail> parseActionResponseDetail(std::string_view value)
{
    if (value == "summary") return ActionResponseDetail::Summary;
    if (value == "compact") return ActionResponseDetail::Compact;
    if (value == "full") return ActionResponseDetail::Full;
    return std::nullopt;
}

std::optional<PreparedBattleDetail> parsePreparedBattleDetail(std::string_view value)
{
    if (value == "summary") return PreparedBattleDetail::Summary;
    if (value == "compact") return PreparedBattleDetail::Compact;
    if (value == "full") return PreparedBattleDetail::Full;
    return std::nullopt;
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
    std::string errorCode,
    std::string errorMessage)
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

std::string battleOutcomeId(Battle::BattleOutcome outcome);

RoleStatsDto roleStatsDto(const ChessCalculatedStats& stats)
{
    return {
        stats.maxHp,
        stats.maxMp,
        stats.attack,
        stats.defence,
        stats.speed,
        stats.fist,
        stats.sword,
        stats.knife,
        stats.unusual,
        stats.hiddenWeapon,
    };
}

PieceDto pieceDto(
    const ChessGameContent& content,
    const ChessGameplayObservation& observation,
    const ChessSessionPiece& piece,
    bool full)
{
    const auto* role = content.role(piece.roleId);
    assert(role);
    PieceDto dto{};
    dto.instance_id = piece.instanceId;
    dto.role_id = piece.roleId;
    dto.name = role->Name;
    dto.star = piece.star;
    dto.deployed = piece.deployed;
    dto.fights_won = piece.fightsWon;
    if (!full)
    {
        return dto;
    }
    dto.current_stats = roleStatsDto(chessPieceStats(
        content,
        piece,
        observation.equipmentInventory));
    dto.current_stats_note = "已計入星級、勝場成長與裝備基礎屬性；羈絆與裝備特殊效果於開戰時套用";
    return dto;
}

AbilityDto abilityDto(const ChessAbilityMetadata& metadata)
{
    AbilityDto dto;
    dto.magic_id = metadata.magicId;
    dto.name = metadata.name;
    for (const auto& power : metadata.powerByStar)
    {
        dto.power_by_star.push_back({power.star, power.power});
    }
    dto.mp_cost = metadata.mpCost;
    dto.select_distance = metadata.selectDistance;
    dto.area_distance = metadata.areaDistance;
    dto.geometry = metadata.geometry;
    dto.effects = metadata.effects;
    dto.effect_note = metadata.effectNote;
    return dto;
}

RoleDto roleDto(const ChessGameContent& content, int roleId)
{
    const auto metadata = chessRoleMetadata(content, roleId);
    RoleDto dto;
    dto.role_id = metadata.roleId;
    dto.name = metadata.name;
    dto.cost = metadata.cost;
    dto.base_stats = roleStatsDto(metadata.baseStats);
    for (const auto& ability : metadata.abilities)
    {
        dto.abilities.push_back(abilityDto(ability));
    }
    dto.combos = metadata.combos;
    return dto;
}

std::optional<RoleDto> inspectRoleDto(
    const ChessGameSession& session,
    int roleId)
{
    if (!session.content().role(roleId))
    {
        return std::nullopt;
    }
    return roleDto(session.content(), roleId);
}

EquipmentInfoDto equipmentInfoDto(
    const ChessGameContent& content,
    int itemId,
    bool full)
{
    const auto metadata = chessEquipmentMetadata(content, itemId);
    EquipmentInfoDto dto;
    dto.item_id = metadata.itemId;
    dto.name = metadata.name;
    dto.tier = metadata.tier;
    dto.type = metadata.equipType == 0 ? "武器" : "防具";
    if (!full)
    {
        return dto;
    }
    dto.base_stat_effects.emplace();
    dto.special_effects.emplace();
    dto.counts_as_combos.emplace();
    dto.character_bonuses.emplace();
    *dto.base_stat_effects = metadata.baseStatEffects;
    *dto.special_effects = metadata.specialEffects;
    *dto.counts_as_combos = metadata.countsAsCombos;
    if (!metadata.comboCountingNote.empty())
    {
        dto.combo_counting_note = metadata.comboCountingNote;
    }
    for (const auto& metadataBonus : metadata.characterBonuses)
    {
        EquipmentInfoDto::CharacterBonus bonus;
        bonus.roles = metadataBonus.roles;
        bonus.effects = metadataBonus.effects;
        bonus.counts_as_combos = metadataBonus.countsAsCombos;
        dto.character_bonuses->push_back(std::move(bonus));
    }
    return dto;
}

ComboDto comboDto(const ChessComboMetadata& metadata, bool full)
{
    ComboDto dto;
    dto.name = metadata.name;
    dto.physical_count = metadata.physicalCount;
    dto.effective_count = metadata.effectiveCount;
    if (full)
    {
        dto.count_explanation = metadata.countExplanation;
        dto.members.emplace();
        dto.thresholds.emplace();
        dto.contributions.emplace();
    }
    if (full)
    {
        for (const auto& contribution : metadata.contributions)
        {
            ComboContributionDto contributionDto;
            contributionDto.role_id = contribution.roleId;
            contributionDto.role_name = contribution.roleName;
            contributionDto.unit_ids = contribution.unitIds;
            contributionDto.counted_star = contribution.countedStar;
            contributionDto.physical_points = contribution.physicalPoints;
            contributionDto.star_bonus_points = contribution.starBonusPoints;
            contributionDto.effective_points = contribution.effectivePoints;
            contributionDto.natural_member = contribution.naturalMember;
            for (const auto& equipment : contribution.equipmentSources)
            {
                contributionDto.equipment_sources.push_back({equipment.id, equipment.name});
            }
            contributionDto.explanation = contribution.explanation;
            dto.contributions->push_back(std::move(contributionDto));
        }
        *dto.members = metadata.members;
    }
    if (!full)
    {
        return dto;
    }
    for (const auto& threshold : metadata.thresholds)
    {
        ComboThresholdDto thresholdDto;
        thresholdDto.required_count = threshold.requiredCount;
        thresholdDto.name = threshold.name;
        thresholdDto.active = threshold.active;
        thresholdDto.effects = threshold.effects;
        dto.thresholds->push_back(std::move(thresholdDto));
    }
    return dto;
}

ComboDto comboDto(
    const ChessGameContent& content,
    const ComboDef& definition,
    int physicalCount,
    int effectiveCount,
    int activeThresholdIndex,
    bool full,
    const std::vector<ResolvedChessComboContribution>* contributions)
{
    return comboDto(chessComboMetadata(
        content,
        definition,
        physicalCount,
        effectiveCount,
        activeThresholdIndex,
        -1,
        contributions ? *contributions : std::vector<ResolvedChessComboContribution>{}),
        full);
}

std::optional<ComboDto> inspectComboDto(
    const ChessGameSession& session,
    std::string_view comboName)
{
    const auto definition = std::ranges::find(
        session.content().combos(),
        comboName,
        &ComboDef::name);
    if (definition == session.content().combos().end())
    {
        return std::nullopt;
    }
    const auto observation = session.observe();
    const auto progress = std::ranges::find(
        observation.combos,
        definition->id,
        &ChessObservedCombo::comboId);
    assert(progress != observation.combos.end());
    return comboDto(
        session.content(),
        *definition,
        progress->physicalCount,
        progress->effectiveCount,
        progress->activeThresholdIndex,
        true,
        &progress->contributions);
}

PreparedBattleDto preparedBattleDto(
    const ChessGameContent& content,
    const PreparedChessBattle& battle,
    PreparedBattleDetail detail,
    const std::set<int>& obtainedNeigongIds,
    int maximumFrames)
{
    const auto analysis = analyzePreparedChessBattle(
        battle,
        content,
        obtainedNeigongIds,
        maximumFrames);
    PreparedBattleDto prepared;
    const bool observationCompact = detail == PreparedBattleDetail::ObservationCompact;
    const bool compact = detail == PreparedBattleDetail::Compact;
    const bool full = detail == PreparedBattleDetail::Full;
    if (observationCompact || full)
    {
        prepared.battle_id = analysis.identity.stableBattleId;
        prepared.metadata_scope = full ? "complete" : "omitted_compact";
        prepared.chosen_map_id = analysis.identity.chosenMapId;
        prepared.coordinate_system = "x 向右增加，y 向下增加；A 表示我方、E 表示敵方、# 是障礙、~ 是水域、. 是可通行地面";
        prepared.map_candidates.emplace();
        for (const auto& map : analysis.identity.mapCandidates)
        {
            prepared.map_candidates->push_back({map.mapId, map.name});
        }
        prepared.battle_seed = analysis.identity.battleSeed;
    }
    if (full)
    {
        prepared.board.emplace();
    }
    if (analysis.identity.chosenMapId >= 0)
    {
        prepared.chosen_map_name = analysis.identity.chosenMapName;
        if (!observationCompact)
        {
            prepared.board = ChessAsciiBoard::render(battle, content);
        }
    }
    if (compact || full)
    {
        prepared.ally_synergies.emplace();
        prepared.enemy_synergies.emplace();
        for (const auto& synergy : analysis.allySynergies)
        {
            prepared.ally_synergies->push_back(comboDto(synergy, full));
        }
        for (const auto& synergy : analysis.enemySynergies)
        {
            prepared.enemy_synergies->push_back(comboDto(synergy, full));
        }
    }
    for (const auto& unit : analysis.units)
    {
        PreparedUnitDto unitDto;
        unitDto.unit_id = unit.unitId;
        unitDto.name = unit.name;
        unitDto.team = chessBattleTeamName(unit.team);
        unitDto.star = unit.star;
        unitDto.x = unit.x;
        unitDto.y = unit.y;
        if (observationCompact || full)
        {
            unitDto.chess_instance_id = unit.chessInstanceId;
            unitDto.role_id = unit.roleId;
        }
        if (observationCompact || full || (compact && unit.weaponItemId >= 0))
        {
            unitDto.weapon = unit.weaponName;
        }
        if (observationCompact || full || (compact && unit.armorItemId >= 0))
        {
            unitDto.armor = unit.armorName;
        }
        if (full)
        {
            unitDto.preview_stats = roleStatsDto(unit.baselineStats);
            unitDto.stats_note = analysis.baselineStatsNote;
            unitDto.abilities.emplace();
            for (const auto& ability : unit.abilities)
            {
                unitDto.abilities->push_back(abilityDto(ability));
            }
        }
        prepared.units.push_back(std::move(unitDto));
    }
    return prepared;
}

std::optional<PreparedBattleDto> inspectPreparedBattleDto(
    const ChessGameSession& session,
    PreparedBattleDetail detail)
{
    const auto observation = session.observe();
    if (!observation.preparedBattle)
    {
        return std::nullopt;
    }
    return preparedBattleDto(
        session.content(),
        *observation.preparedBattle,
        detail,
        session.state().obtainedNeigongIds,
        session.state().options.battleFrameLimit);
}

std::string rewardKindId(ChessRewardKind kind)
{
    switch (kind)
    {
    case ChessRewardKind::Equipment: return "裝備";
    case ChessRewardKind::InternalSkill: return "內功";
    case ChessRewardKind::ChallengeReward: return "遠征獎勵";
    case ChessRewardKind::Piece: return "棋子";
    case ChessRewardKind::StarUpgrade: return "升星";
    case ChessRewardKind::ForcedBan: return "禁棋";
    }
    std::unreachable();
}

RewardOptionDto rewardOptionDto(
    const ChessGameContent& content,
    const ChessPendingReward& pending,
    const ChessRewardOption& option,
    int starUpgradeRoleId = -1)
{
    RewardOptionDto dto;
    dto.id = option.id;
    dto.kind = rewardKindId(option.kind);
    if (option.kind == ChessRewardKind::Equipment)
    {
        dto.equipment = equipmentInfoDto(content, option.value);
        dto.label = dto.equipment->name;
        dto.description = std::format("{}階{}", dto.equipment->tier, dto.equipment->type);
        for (const auto& effect : *dto.equipment->base_stat_effects)
        {
            dto.description += "；基礎：" + effect;
        }
        for (const auto& effect : *dto.equipment->special_effects)
        {
            dto.description += "；特殊：" + effect;
        }
        for (const auto& comboName : *dto.equipment->counts_as_combos)
        {
            dto.description += "；計作" + comboName;
        }
        for (const auto& bonus : *dto.equipment->character_bonuses)
        {
            dto.description += "；角色加成(";
            for (std::size_t index = 0; index < bonus.roles.size(); ++index)
            {
                if (index > 0) dto.description += "、";
                dto.description += bonus.roles[index];
            }
            dto.description += ")";
            bool firstBonusEffect = true;
            auto appendBonusEffect = [&](std::string effect)
            {
                dto.description += firstBonusEffect ? "：" : "；";
                dto.description += std::move(effect);
                firstBonusEffect = false;
            };
            for (const auto& effect : bonus.effects) appendBonusEffect(effect);
            for (const auto& comboName : bonus.counts_as_combos) appendBonusEffect("計作" + comboName);
        }
    }
    else if (option.kind == ChessRewardKind::InternalSkill)
    {
        const auto found = std::ranges::find(content.neigong(), option.value, &NeigongDef::magicId);
        assert(found != content.neigong().end());
        dto.label = found->name;
        dto.description = std::format("{}階；", found->tier);
        for (std::size_t index = 0; index < found->effects.size(); ++index)
        {
            if (index > 0) dto.description += "；";
            auto effect = comboEffectDesc(found->effects[index]);
            dto.description += effect;
        }
    }
    else if (option.kind == ChessRewardKind::Piece || option.kind == ChessRewardKind::ForcedBan)
    {
        const auto* role = content.role(option.value);
        assert(role);
        dto.role_id = option.value;
        dto.label = role->Name;
        dto.description = std::format("{}費棋子", role->Cost);
    }
    else if (option.kind == ChessRewardKind::StarUpgrade)
    {
        assert(starUpgradeRoleId >= 0);
        const auto* role = content.role(starUpgradeRoleId);
        assert(role);
        dto.chess_instance_id = option.value;
        dto.role_id = starUpgradeRoleId;
        dto.label = role->Name;
        dto.description = std::format("升至 {}★", option.value2);
    }
    else
    {
        assert(pending.kind == ChessRewardKind::ChallengeReward);
        dto.label = option.id;
        dto.description = option.id;
    }
    return dto;
}

std::set<int> observationRelevantRoleIds(const ChessGameplayObservation& observation)
{
    std::set<int> result;
    for (const auto& slot : observation.shop)
    {
        if (slot.roleId >= 0) result.insert(slot.roleId);
    }
    for (const auto& piece : observation.roster)
    {
        result.insert(piece.roleId);
    }
    if (observation.preparedBattle)
    {
        for (const auto& unit : observation.preparedBattle->units)
        {
            result.insert(unit.roleId);
        }
    }
    if (observation.pendingReward)
    {
        for (const auto& option : observation.pendingReward->options)
        {
            if (option.kind == ChessRewardKind::Piece || option.kind == ChessRewardKind::ForcedBan)
            {
                result.insert(option.value);
            }
            else if (option.kind == ChessRewardKind::StarUpgrade)
            {
                const auto piece = std::ranges::find(
                    observation.roster,
                    option.value,
                    &ChessSessionPiece::instanceId);
                assert(piece != observation.roster.end());
                result.insert(piece->roleId);
            }
        }
    }
    return result;
}

ObservationDto observationDto(
    const ChessGameplayObservation& observation,
    const ChessGameContent& content,
    ObservationDetail detail,
    std::string roleMetadataScope,
    std::span<const ChessActionOffer> legalActions)
{
    ObservationDto dto{};
    const bool full = detail == ObservationDetail::Full;
    dto.detail = full ? "full" : "compact";
    dto.phase = phaseText(observation.phase);
    dto.money = observation.money;
    dto.total_campaign_rounds = content.balance().totalFights;
    dto.experience = observation.experience;
    dto.experience_for_next_level = observation.experienceForNextLevel;
    dto.level = observation.level;
    dto.maximum_deployment = observation.maximumDeployment;
    dto.fight = observation.fight;
    dto.campaign_complete = observation.campaignComplete;
    if (full)
    {
        dto.difficulty = observation.difficulty == Difficulty::Easy
            ? "easy"
            : observation.difficulty == Difficulty::Normal ? "normal" : "hard";
        dto.position_swap_enabled = observation.options.positionSwapEnabled;
        dto.interest_gold = observation.interestGold;
        dto.next_interest_threshold = observation.nextInterestThreshold;
        dto.maximum_interest_gold = observation.maximumInterestGold;
        dto.boss_interval = content.balance().bossInterval;
        dto.forced_bans_enabled = !content.balance().banUnlocks.empty();
        dto.projected_base_victory_gold = observation.projectedBaseVictoryGold;
        dto.projected_victory_income = observation.projectedVictoryIncome;
        dto.projected_victory_income_excludes_conditional_bonuses = true;
        dto.current_ban_count = static_cast<int>(observation.bans.size());
        dto.maximum_ban_count = observation.maximumBanCount;
        dto.remaining_ban_capacity = std::max(
            0,
            observation.maximumBanCount - static_cast<int>(observation.bans.size()));
        dto.ban_effect_timing = "禁棋只影響之後生成或刷新的商店；目前商店既有棋子不會被移除，仍可購買";
        dto.shop_locked = observation.shopLocked;
        dto.free_shop_refresh_available = observation.freeShopRefreshAvailable;
        dto.free_shop_refresh_granted_fight = observation.freeShopRefreshGrantedFight;
    }
    for (int index = 0; index < static_cast<int>(observation.shop.size()); ++index)
    {
        const auto& slot = observation.shop[index];
        if (slot.roleId < 0)
        {
            dto.shop.push_back({index});
            continue;
        }
        const auto* role = content.role(slot.roleId);
        assert(role);
        dto.shop.push_back({index, slot.roleId, role->Name, role->Cost});
    }
    auto relevantRoleIds = observationRelevantRoleIds(observation);
    for (const auto& piece : observation.roster)
    {
        dto.roster.push_back(ProtocolDetail::pieceDto(content, observation, piece, full));
    }
    for (const auto& equipment : observation.equipmentInventory)
    {
        dto.equipment_inventory.push_back({
            equipment.instanceId,
            equipmentInfoDto(content, equipment.itemId, full),
            equipment.assignedChessInstanceId,
        });
    }
    if (full)
    {
        dto.bans.emplace();
        for (const int roleId : observation.bans)
        {
            const auto* role = content.role(roleId);
            assert(role);
            dto.bans->push_back({roleId, role->Name});
        }
        dto.obtained_internal_skills.emplace();
        for (const int magicId : observation.obtainedNeigongIds)
        {
            const auto found = std::ranges::find(content.neigong(), magicId, &NeigongDef::magicId);
            assert(found != content.neigong().end());
            dto.obtained_internal_skills->push_back({magicId, found->name});
        }
    }
    dto.completed_challenges = observation.completedChallengeNames;
    for (const auto& combo : observation.combos)
    {
        if (combo.physicalCount == 0 || (!full && combo.activeThresholdIndex < 0))
        {
            continue;
        }
        const auto found = std::ranges::find(content.combos(), combo.comboId, &ComboDef::id);
        assert(found != content.combos().end());
        dto.combos.push_back(ProtocolDetail::comboDto(
            content,
            *found,
            combo.physicalCount,
            combo.effectiveCount,
            combo.activeThresholdIndex,
            full,
            &combo.contributions));
    }
    if (observation.preparedBattle)
    {
        dto.prepared_battle = preparedBattleDto(
            content,
            *observation.preparedBattle,
            full ? PreparedBattleDetail::Full : PreparedBattleDetail::ObservationCompact,
            std::set<int>(observation.obtainedNeigongIds.begin(), observation.obtainedNeigongIds.end()),
            observation.options.battleFrameLimit);
    }
    if (observation.pendingReward)
    {
        PendingRewardDto pending;
        pending.id = observation.pendingReward->id;
        pending.kind = rewardKindId(observation.pendingReward->kind);
        const auto optionAvailable = [&](const ChessRewardOption& option) {
            return observation.pendingReward->kind != ChessRewardKind::ForcedBan
                || !std::ranges::contains(observation.bans, option.value);
        };
        pending.option_count = static_cast<int>(std::ranges::count_if(
            observation.pendingReward->options,
            optionAvailable));
        pending.reroll_cost = observation.pendingReward->rerollCost;
        pending.eligible_tiers = observation.pendingReward->eligibleTiers;
        pending.rerolled = observation.pendingReward->rerolled;
        if (observation.pendingReward->kind == ChessRewardKind::ForcedBan)
        {
            pending.option_count_description = "目前仍可選的禁棋候選角色數量，不是剩餘選擇次數";
            pending.maximum_selections = observation.pendingReward->choiceCount;
            pending.remaining_selections = observation.pendingReward->parameter;
            pending.current_ban_count = static_cast<int>(observation.bans.size());
            pending.maximum_total_bans = observation.maximumBanCount;
            pending.selection_optional = true;
            pending.decision_requirement = "此階段強制先完成禁棋決策：可禁用至多剩餘次數，也可直接略過；略過不會消耗仍可於之後使用的禁棋容量";
        }
        if (observation.pendingReward->kind == ChessRewardKind::Equipment)
        {
            std::map<std::string, int> groups;
            for (const auto& option : observation.pendingReward->options)
            {
                const auto definition = chessEquipmentMetadata(content, option.value);
                ++groups[std::format(
                    "{}階{}",
                    definition.tier,
                    definition.equipType == 0 ? "武器" : "防具")];
            }
            for (const auto& [label, count] : groups)
            {
                pending.option_groups.push_back({label, count});
            }
        }
        const bool largeEquipmentChoice = observation.pendingReward->kind == ChessRewardKind::Equipment
            && observation.pendingReward->options.size() > 12;
        if (full
            || (observation.pendingReward->kind != ChessRewardKind::ForcedBan
                && !largeEquipmentChoice))
        {
            pending.options.emplace();
            for (const auto& option : observation.pendingReward->options)
            {
                if (!optionAvailable(option))
                {
                    continue;
                }
                int starUpgradeRoleId = -1;
                if (option.kind == ChessRewardKind::StarUpgrade)
                {
                    const auto piece = std::ranges::find(
                        observation.roster,
                        option.value,
                        &ChessSessionPiece::instanceId);
                    assert(piece != observation.roster.end());
                    starUpgradeRoleId = piece->roleId;
                }
                pending.options->push_back(rewardOptionDto(
                    content,
                    *observation.pendingReward,
                    option,
                    starUpgradeRoleId));
            }
        }
        dto.pending_reward = std::move(pending);
    }
    if (full)
    {
        dto.role_metadata_scope = !roleMetadataScope.empty()
            ? std::move(roleMetadataScope)
            : "complete";
        dto.equipment_metadata_scope = "complete";
        dto.relevant_roles.emplace();
        for (const int roleId : relevantRoleIds)
        {
            dto.relevant_roles->push_back(roleDto(content, roleId));
        }
    }
    if (full || observation.lastBattleOutcome != Battle::BattleOutcome::InProgress)
    {
        dto.last_battle_outcome = battleOutcomeId(observation.lastBattleOutcome);
        dto.last_battle_outcome_description = chessBattleOutcomeDescription(observation.lastBattleOutcome);
        dto.last_battle_end_frame = observation.lastBattleEndFrame;
    }
    if (full)
    {
        dto.last_battle_digest = chessSha256Hex(observation.lastBattleDigest);
    }
    for (const auto& legal : legalActions)
    {
        dto.legal_action_types.push_back(chessActionTypeId(legal.type));
    }
    dto.state_hash = chessSha256Hex(observation.stateHash);
    return dto;
}

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

std::string actionDescription(ChessActionType type)
{
    switch (type)
    {
    case ChessActionType::BuyShopSlot: return "購買指定商店欄位的棋子";
    case ChessActionType::RefreshShop: return "刷新全部商店欄位";
    case ChessActionType::SetShopLocked: return "設定商店是否跨回合保留";
    case ChessActionType::SellChess: return "出售指定棋子實例";
    case ChessActionType::SetDeployment: return "以棋子實例清單完整取代目前出戰陣容；空陣列代表全部下陣";
    case ChessActionType::BuyExp: return "購買經驗值";
    case ChessActionType::AddBan: return "禁用指定角色；只影響之後生成或刷新的商店，目前商店既有棋子仍可購買";
    case ChessActionType::SkipForcedBans: return "結束目前的獨佔禁棋決策階段而不再立即選擇；不消耗之後仍可使用的禁棋容量";
    case ChessActionType::Equip: return "將裝備實例交給指定棋子實例；已分配裝備會從原持有者移動";
    case ChessActionType::BuyLegendaryEquipment: return "購買指定神兵";
    case ChessActionType::SetPositionSwapEnabled: return "設定戰前是否允許交換站位";
    case ChessActionType::RerollEnemySeed: return "付費重抽下一戰敵方規劃";
    case ChessActionType::PrepareBattle: return "生成下一場主線戰鬥與敵方預覽";
    case ChessActionType::ChooseMap: return "選擇戰場";
    case ChessActionType::SwapPositions: return "交換兩個我方戰鬥單位的位置";
    case ChessActionType::StartBattle: return "依目前預覽與站位開始並結算戰鬥";
    case ChessActionType::RerollReward: return "付費刷新目前獎勵選項";
    case ChessActionType::ChooseReward: return "選擇目前獎勵選項";
    case ChessActionType::StartChallenge: return "依遠征名稱開始挑戰，不使用額外英文 ID";
    case ChessActionType::FinishRun: return "結束已通關的本局";
    }
    std::unreachable();
}

std::string challengeDescription(
    const ChessGameContent& content,
    const BalanceConfig::ChallengeDef& challenge)
{
    return chessChallengeMetadata(content, challenge).summaryDescription;
}

ChallengeDto challengeDto(
    const ChessGameContent& content,
    const BalanceConfig::ChallengeDef& challenge)
{
    const auto metadata = chessChallengeMetadata(content, challenge);
    ChallengeDto dto;
    dto.name = metadata.name;
    dto.description = metadata.description;
    dto.enemy_count = static_cast<int>(metadata.enemies.size());
    for (const auto& enemy : metadata.enemies)
    {
        ChallengeEnemyDto enemyDto;
        enemyDto.role_id = enemy.roleId;
        enemyDto.name = enemy.name;
        enemyDto.star = enemy.star;
        if (enemy.weapon)
        {
            enemyDto.weapon = equipmentInfoDto(content, enemy.weapon->itemId);
        }
        if (enemy.armor)
        {
            enemyDto.armor = equipmentInfoDto(content, enemy.armor->itemId);
        }
        dto.enemies.push_back(std::move(enemyDto));
    }
    for (const auto& reward : metadata.rewards)
    {
        dto.rewards.push_back({reward});
    }
    return dto;
}

std::optional<EquipmentInfoDto> inspectEquipmentDto(
    const ChessGameSession& session,
    int itemId)
{
    const auto definition = std::ranges::find(
        session.content().equipment(),
        itemId,
        &EquipmentDef::itemId);
    if (definition == session.content().equipment().end())
    {
        return std::nullopt;
    }
    return equipmentInfoDto(session.content(), itemId);
}

std::optional<ChallengeDto> inspectChallengeDto(
    const ChessGameSession& session,
    std::string_view challengeName)
{
    const auto definition = std::ranges::find(
        session.content().balance().challenges,
        challengeName,
        &BalanceConfig::ChallengeDef::name);
    if (definition == session.content().balance().challenges.end())
    {
        return std::nullopt;
    }
    return challengeDto(session.content(), *definition);
}

std::string legalActionExampleJson(
    const ChessGameSession& session,
    const ChessActionOffer& offer)
{
    ChessAction action;
    action.type = offer.type;
    switch (offer.type)
    {
    case ChessActionType::BuyShopSlot:
        action.shopSlot = chessActionOfferDetail<ChessShopSlotSelectionOffer>(offer).candidates.front().slot;
        break;
    case ChessActionType::SetShopLocked:
    case ChessActionType::SetPositionSwapEnabled:
        action.value = true;
        break;
    case ChessActionType::SellChess:
        action.chessInstanceId = chessActionOfferDetail<ChessPieceSelectionOffer>(offer).candidates.front().chessInstanceId;
        break;
    case ChessActionType::SetDeployment:
    {
        const auto& candidates = chessActionOfferDetail<ChessPieceSelectionOffer>(offer).candidates;
        for (std::size_t index = 0;
             index < std::min(candidates.size(), static_cast<std::size_t>(offer.maximumSelection));
             ++index)
        {
            action.chessInstanceIds.push_back(candidates[index].chessInstanceId);
        }
        break;
    }
    case ChessActionType::AddBan:
        action.roleId = chessActionOfferDetail<ChessRoleSelectionOffer>(offer).candidates.front().roleId;
        break;
    case ChessActionType::Equip:
    {
        const auto& detail = chessActionOfferDetail<ChessEquipmentAssignmentOffer>(offer);
        const auto& equipment = detail.equipment.front();
        action.equipmentInstanceId = equipment.equipmentInstanceId;
        const auto alternateTarget = std::ranges::find_if(
            detail.targets,
            [&](const auto& candidate) {
                return candidate.chessInstanceId != equipment.assignedChessInstanceId;
            });
        action.targetChessInstanceId = alternateTarget != detail.targets.end()
            ? alternateTarget->chessInstanceId
            : detail.targets.front().chessInstanceId;
        break;
    }
    case ChessActionType::BuyLegendaryEquipment:
        action.itemId = chessActionOfferDetail<ChessItemSelectionOffer>(offer).candidates.front().itemId;
        break;
    case ChessActionType::ChooseMap:
        action.mapId = chessActionOfferDetail<ChessMapSelectionOffer>(offer).candidates.front().mapId;
        break;
    case ChessActionType::SwapPositions:
    {
        const auto& candidates = chessActionOfferDetail<ChessPositionSwapOffer>(offer).candidates;
        if (candidates.size() < 2)
        {
            return chessActionPayloadSchema(offer.type);
        }
        action.chessInstanceId = candidates[0].unitId;
        action.targetChessInstanceId = candidates[1].unitId;
        break;
    }
    case ChessActionType::ChooseReward:
        action.rewardId = chessActionOfferDetail<ChessRewardSelectionOffer>(offer).candidates.front().rewardId;
        break;
    case ChessActionType::StartChallenge:
        action.challengeName = chessActionOfferDetail<ChessChallengeSelectionOffer>(offer).candidates.front().challengeName;
        break;
    default:
        break;
    }
    return serializeChessActionJson(action);
}

LegalActionDto legalActionDto(
    const ChessGameSession& session,
    const ChessActionOffer& offer)
{
    LegalActionDto dto;
    dto.type = chessActionTypeId(offer.type);
    dto.description = actionDescription(offer.type);
    dto.action_schema = glz::raw_json(chessActionPayloadSchema(offer.type));
    dto.example = glz::raw_json(legalActionExampleJson(session, offer));
    dto.minimum_selection = offer.minimumSelection;
    dto.maximum_selection = offer.maximumSelection;
    const auto& content = session.content();
    const auto& state = session.state();

    if (offer.economicConsequence)
    {
        const auto& consequence = *offer.economicConsequence;
        LegalActionDto::EconomicPreview preview;
        preview.current_gold = consequence.currentGold;
        preview.gold_cost = consequence.goldCost;
        preview.projected_gold_after = consequence.projectedGoldAfter;
        preview.experience_gained = consequence.experienceGained;
        preview.projected_experience_after = consequence.projectedExperienceAfter;
        preview.projected_level_after = consequence.projectedLevelAfter;
        preview.affected_option_count = consequence.affectedOptionCount;
        preview.consumes_free_shop_refresh = consequence.consumesFreeShopRefresh;
        switch (offer.type)
        {
        case ChessActionType::RefreshShop:
            preview.result = std::format(
                "重新生成全部 {} 個商店欄位並解除商店鎖定",
                consequence.affectedOptionCount.value());
            break;
        case ChessActionType::BuyExp:
            preview.result = std::format(
                "獲得 {} 經驗；購買後為等級 {}、累積經驗 {}",
                consequence.experienceGained.value(),
                consequence.projectedLevelAfter.value(),
                consequence.projectedExperienceAfter.value());
            break;
        case ChessActionType::RerollEnemySeed:
            preview.result = "重新生成下一戰的敵方陣容、裝備、地圖與戰鬥種子";
            break;
        case ChessActionType::RerollReward:
            preview.result = std::format(
                "重新抽取目前獎勵的 {} 個選項；每份獎勵最多重抽一次",
                consequence.affectedOptionCount.value());
            break;
        case ChessActionType::BuyLegendaryEquipment:
            preview.result = "取得所選的 4 階裝備實例並放入裝備庫";
            break;
        default:
            break;
        }
        dto.economic_preview = std::move(preview);
    }

    std::vector<LegalActionDto::Candidate> primaryCandidates;
    std::string primaryField;
    bool primaryMultiple = false;
    switch (offer.type)
    {
    case ChessActionType::BuyShopSlot:
        primaryField = "slot";
        for (const auto& candidate : chessActionOfferDetail<ChessShopSlotSelectionOffer>(offer).candidates)
        {
            const auto* role = content.role(candidate.roleId);
            assert(role);
            LegalActionDto::Candidate dtoCandidate;
            dtoCandidate.id = candidate.slot;
            dtoCandidate.value = std::to_string(candidate.slot);
            dtoCandidate.label = role->Name;
            dtoCandidate.description = std::format(
                "商店欄位 {}；{}費角色；價格 {} 金幣",
                candidate.slot,
                role->Cost,
                candidate.cost);
            dtoCandidate.gold_cost = candidate.cost;
            dtoCandidate.projected_gold_after = candidate.projectedGoldAfter;
            primaryCandidates.push_back(std::move(dtoCandidate));
        }
        break;
    case ChessActionType::SellChess:
    case ChessActionType::SetDeployment:
        primaryField = offer.type == ChessActionType::SellChess
            ? "chess_instance_id"
            : "chess_instance_ids";
        primaryMultiple = offer.type == ChessActionType::SetDeployment;
        for (const auto& candidate : chessActionOfferDetail<ChessPieceSelectionOffer>(offer).candidates)
        {
            const auto* role = content.role(candidate.roleId);
            assert(role);
            LegalActionDto::Candidate dtoCandidate;
            dtoCandidate.id = candidate.chessInstanceId;
            dtoCandidate.value = std::to_string(candidate.chessInstanceId);
            dtoCandidate.label = role->Name;
            dtoCandidate.description = std::format(
                "棋子實例 {}；{}★{}",
                candidate.chessInstanceId,
                candidate.star,
                candidate.deployed ? "；目前出戰" : "");
            if (offer.type == ChessActionType::SellChess)
            {
                dtoCandidate.gold_gain = candidate.goldGain;
                dtoCandidate.projected_gold_after = candidate.projectedGoldAfter;
                dtoCandidate.description += std::format(
                    "；出售獲得 {} 金幣",
                    candidate.goldGain.value());
            }
            primaryCandidates.push_back(std::move(dtoCandidate));
        }
        break;
    case ChessActionType::AddBan:
        primaryField = "role_id";
        for (const auto& candidate : chessActionOfferDetail<ChessRoleSelectionOffer>(offer).candidates)
        {
            const auto* role = content.role(candidate.roleId);
            assert(role);
            primaryCandidates.push_back({
                candidate.roleId,
                std::to_string(candidate.roleId),
                role->Name,
                std::format("{}費棋子", role->Cost),
            });
        }
        break;
    case ChessActionType::Equip:
    {
        primaryField = "equipment_instance_id";
        const auto& detail = chessActionOfferDetail<ChessEquipmentAssignmentOffer>(offer);
        for (const auto& candidate : detail.equipment)
        {
            const auto info = equipmentInfoDto(content, candidate.itemId);
            LegalActionDto::Candidate dtoCandidate;
            dtoCandidate.id = candidate.equipmentInstanceId;
            dtoCandidate.value = std::to_string(candidate.equipmentInstanceId);
            dtoCandidate.label = info.name;
            dtoCandidate.assigned_chess_instance_id = candidate.assignedChessInstanceId;
            if (candidate.assignedChessInstanceId < 0)
            {
                dtoCandidate.label += "（未分配）";
                dtoCandidate.description = std::format(
                    "裝備實例 {}；{}階{}；未分配",
                    candidate.equipmentInstanceId,
                    info.tier,
                    info.type);
            }
            else
            {
                const auto& owner = state.roster.at(candidate.assignedChessInstanceId);
                const auto* ownerRole = content.role(owner.roleId);
                assert(ownerRole);
                dtoCandidate.label += std::format("（{}已裝備）", ownerRole->Name);
                dtoCandidate.assigned_to = std::format(
                    "{}（棋子實例 {}）",
                    ownerRole->Name,
                    candidate.assignedChessInstanceId);
                dtoCandidate.description = std::format(
                    "裝備實例 {}；{}階{}；目前由{}（棋子實例 {}）裝備",
                    candidate.equipmentInstanceId,
                    info.tier,
                    info.type,
                    ownerRole->Name,
                    candidate.assignedChessInstanceId);
            }
            primaryCandidates.push_back(std::move(dtoCandidate));
        }
        if (!detail.equipment.empty() && detail.equipment.front().assignedChessInstanceId >= 0)
        {
            dto.example_note = detail.targets.size() > 1
                ? "範例會把目前已裝備的項目移給另一名棋子"
                : "目前沒有未分配裝備或其他棋子；範例合法，但不會改變持有者";
        }
        LegalActionDto::FieldCandidates targets;
        targets.field = "target_chess_instance_id";
        for (const auto& candidate : detail.targets)
        {
            const auto* role = content.role(candidate.roleId);
            assert(role);
            targets.candidates.push_back({
                candidate.chessInstanceId,
                std::to_string(candidate.chessInstanceId),
                role->Name,
                std::format("{}★{}", candidate.star, candidate.deployed ? "；目前出戰" : ""),
            });
        }
        dto.candidates_by_field.push_back(std::move(targets));
        break;
    }
    case ChessActionType::BuyLegendaryEquipment:
        primaryField = "item_id";
        for (const auto& candidate : chessActionOfferDetail<ChessItemSelectionOffer>(offer).candidates)
        {
            const auto info = equipmentInfoDto(content, candidate.itemId);
            LegalActionDto::Candidate dtoCandidate;
            dtoCandidate.id = candidate.itemId;
            dtoCandidate.value = std::to_string(candidate.itemId);
            dtoCandidate.label = info.name;
            dtoCandidate.description = std::format(
                "{}階{}；價格 {} 金幣",
                info.tier,
                info.type,
                candidate.cost);
            dtoCandidate.gold_cost = candidate.cost;
            dtoCandidate.projected_gold_after = candidate.projectedGoldAfter;
            primaryCandidates.push_back(std::move(dtoCandidate));
        }
        break;
    case ChessActionType::ChooseMap:
        primaryField = "map_id";
        for (const auto& candidate : chessActionOfferDetail<ChessMapSelectionOffer>(offer).candidates)
        {
            primaryCandidates.push_back({
                candidate.mapId,
                std::to_string(candidate.mapId),
                chessBattleMapDisplayName(content, candidate.mapId),
            });
        }
        break;
    case ChessActionType::SwapPositions:
        primaryField = "first_unit_id";
        for (const auto& candidate : chessActionOfferDetail<ChessPositionSwapOffer>(offer).candidates)
        {
            const auto* role = content.role(candidate.roleId);
            assert(role);
            primaryCandidates.push_back({
                candidate.unitId,
                std::to_string(candidate.unitId),
                role->Name,
                std::format(
                    "戰鬥單位 {}；位置 ({},{})",
                    candidate.unitId,
                    candidate.x,
                    candidate.y),
            });
        }
        break;
    case ChessActionType::ChooseReward:
        primaryField = "reward_id";
        for (const auto& candidate : chessActionOfferDetail<ChessRewardSelectionOffer>(offer).candidates)
        {
            assert(!state.pendingRewards.empty());
            const auto option = std::ranges::find(
                state.pendingRewards.front().options,
                candidate.rewardId,
                &ChessRewardOption::id);
            assert(option != state.pendingRewards.front().options.end());
            const int starUpgradeRoleId = option->kind == ChessRewardKind::StarUpgrade
                ? state.roster.at(option->value).roleId
                : -1;
            const auto reward = rewardOptionDto(
                content,
                state.pendingRewards.front(),
                *option,
                starUpgradeRoleId);
            LegalActionDto::Candidate dtoCandidate;
            dtoCandidate.value = candidate.rewardId;
            dtoCandidate.label = reward.label;
            dtoCandidate.description = reward.description;
            if (reward.equipment)
            {
                dtoCandidate.group = std::format(
                    "{}階{}",
                    reward.equipment->tier,
                    reward.equipment->type);
            }
            primaryCandidates.push_back(std::move(dtoCandidate));
        }
        break;
    case ChessActionType::StartChallenge:
        primaryField = "challenge_name";
        for (const auto& candidate : chessActionOfferDetail<ChessChallengeSelectionOffer>(offer).candidates)
        {
            const auto challenge = std::ranges::find(
                content.balance().challenges,
                candidate.challengeName,
                &BalanceConfig::ChallengeDef::name);
            assert(challenge != content.balance().challenges.end());
            primaryCandidates.push_back({
                {},
                candidate.challengeName,
                candidate.challengeName,
                challengeDescription(content, *challenge),
            });
        }
        break;
    default:
        break;
    }

    if (!primaryField.empty())
    {
        dto.candidates_by_field.insert(
            dto.candidates_by_field.begin(),
            {primaryField, primaryMultiple, primaryCandidates});
    }
    if (offer.type == ChessActionType::SwapPositions)
    {
        dto.candidates_by_field.push_back({
            "second_unit_id",
            false,
            std::move(primaryCandidates),
        });
    }
    return dto;
}

ShopOddsDto shopOddsDto(
    const ChessGameContent& content,
    const ChessShopOddsAnalysis& analysis)
{
    ShopOddsDto dto;
    dto.level = analysis.level;
    dto.total_effective_weight = analysis.totalEffectiveWeight;
    for (const auto& tier : analysis.tiers)
    {
        ShopTierOddsDto tierDto;
        tierDto.tier = tier.tier;
        tierDto.configured_weight = tier.configuredWeight;
        tierDto.probability = tier.probability;
        tierDto.available_role_count = static_cast<int>(tier.availableRoleIds.size());
        for (const int roleId : tier.availableRoleIds)
        {
            const auto* role = content.role(roleId);
            assert(role);
            tierDto.available_roles.push_back({roleId, role->Name});
        }
        dto.tiers.push_back(std::move(tierDto));
    }
    dto.pool_note = analysis.poolNote;
    return dto;
}

std::optional<ShopOddsDto> inspectShopOddsDto(
    const ChessGameSession& session,
    std::optional<int> requestedLevel)
{
    const int level = requestedLevel.value_or(session.state().level);
    if (level < 0
        || level >= static_cast<int>(session.content().balance().shopWeights.size()))
    {
        return std::nullopt;
    }
    return shopOddsDto(
        session.content(),
        queryChessShopOdds(session.state(), session.content(), level));
}

ShopSlotInspectionDto shopSlotInspectionDto(
    const ChessGameContent& content,
    const ChessShopSlotAnalysis& analysis)
{
    ShopSlotInspectionDto dto;
    dto.slot = analysis.slot;
    dto.occupied = analysis.occupied;
    if (!analysis.occupied)
    {
        return dto;
    }
    const auto* role = content.role(analysis.roleId);
    assert(role);
    dto.role_id = analysis.roleId;
    dto.name = role->Name;
    dto.cost = analysis.cost;
    dto.shop_tier = analysis.shopTier;
    dto.owned_copies = analysis.ownedCopies;
    for (const auto& copies : analysis.copiesByStar)
    {
        dto.copies_by_star.push_back({copies.star, copies.count});
    }
    dto.purchase_legal = analysis.purchaseError == ChessRuleErrorCode::None;
    dto.purchase_error = ruleErrorId(analysis.purchaseError);
    dto.gold_cost = analysis.goldCost;
    dto.projected_gold_after = analysis.projectedGoldAfter;
    switch (analysis.projectedResult)
    {
    case ChessProjectedPurchaseResult::BenchFull:
        dto.purchase_result = "bench_full";
        break;
    case ChessProjectedPurchaseResult::AddOneStar:
        dto.purchase_result = "add_1_star";
        break;
    case ChessProjectedPurchaseResult::Merge:
        dto.purchase_result = std::format(
            "merge_to_{}_star",
            analysis.projectedResultStar);
        break;
    }
    for (const auto& synergy : analysis.synergies)
    {
        dto.synergies.push_back({
            synergy.name,
            synergy.current,
            synergy.afterPurchase,
        });
    }
    dto.level_tier_probability = analysis.levelTierProbability;
    return dto;
}

std::optional<ShopSlotInspectionDto> inspectShopSlotDto(
    const ChessGameSession& session,
    int slotIndex)
{
    if (slotIndex < 0
        || slotIndex >= static_cast<int>(session.state().shop.size()))
    {
        return std::nullopt;
    }
    return shopSlotInspectionDto(
        session.content(),
        queryChessShopSlot(
            session.state(),
            session.content(),
            slotIndex,
            [&](const ChessAction& action) { return session.validateAction(action); }));
}

ShopInspectionDto shopInspectionDto(const ChessGameSession& session)
{
    const auto analysis = queryChessShop(
        session.state(),
        session.content(),
        [&](const ChessAction& action) { return session.validateAction(action); });
    ShopInspectionDto dto;
    dto.money = analysis.money;
    dto.level = analysis.level;
    for (const auto& slot : analysis.slots)
    {
        dto.slots.push_back(shopSlotInspectionDto(session.content(), slot));
    }
    dto.odds = shopOddsDto(session.content(), analysis.odds);
    return dto;
}

ChessInstanceInspectionDto chessInstanceInspectionDto(
    const ChessGameSession& session,
    const ChessSessionPiece& piece)
{
    const auto analysis = queryChessInstance(
        session.state(),
        session.content(),
        piece.instanceId);
    ChessInstanceInspectionDto dto;
    const auto* role = session.content().role(analysis.piece.roleId);
    assert(role);
    dto.chess.instance_id = analysis.piece.instanceId;
    dto.chess.role_id = analysis.piece.roleId;
    dto.chess.name = role->Name;
    dto.chess.star = analysis.piece.star;
    dto.chess.deployed = analysis.piece.deployed;
    dto.chess.fights_won = analysis.piece.fightsWon;
    dto.chess.current_stats = roleStatsDto(analysis.currentStats);
    dto.chess.current_stats_note = "已計入星級、勝場成長與裝備基礎屬性；羈絆與裝備特殊效果於開戰時套用";
    dto.one_star_equivalent_copies = analysis.oneStarEquivalentCopies;
    dto.same_star_copies = analysis.sameStarCopies;
    dto.copies_required_for_next_star = analysis.copiesRequiredForNextStar;
    for (const auto& equipment : analysis.equipment)
    {
        dto.equipment.push_back({
            equipment.instanceId,
            equipmentInfoDto(session.content(), equipment.itemId),
            equipment.assignedChessInstanceId,
        });
    }
    for (const auto& synergy : analysis.synergyContributions)
    {
        dto.synergy_contributions.push_back(comboDto(synergy));
    }
    return dto;
}

std::optional<ChessInstanceInspectionDto> inspectChessInstanceDto(
    const ChessGameSession& session,
    int chessInstanceId)
{
    const auto piece = session.state().roster.find(chessInstanceId);
    if (piece == session.state().roster.end())
    {
        return std::nullopt;
    }
    return chessInstanceInspectionDto(session, piece->second);
}

BanInspectionDto banInspectionDto(const ChessGameSession& session)
{
    const auto analysis = queryChessBans(
        session.state(),
        session.content(),
        session.legalActions());
    BanInspectionDto dto;
    dto.current_ban_count = analysis.currentBanCount;
    dto.maximum_ban_count = analysis.maximumBanCount;
    dto.remaining_ban_capacity = analysis.remainingBanCapacity;
    const auto mapGroups = [&](const std::vector<ChessBanTierGroup>& groups) {
        std::vector<BanInspectionDto::TierGroup> result;
        for (const auto& group : groups)
        {
            BanInspectionDto::TierGroup dtoGroup;
            dtoGroup.cost = group.cost;
            for (const int roleId : group.roleIds)
            {
                const auto* role = session.content().role(roleId);
                assert(role);
                dtoGroup.roles.push_back({roleId, role->Name});
            }
            result.push_back(std::move(dtoGroup));
        }
        return result;
    };
    dto.current_bans_by_cost = mapGroups(analysis.currentBansByCost);
    dto.eligible_bans_by_cost = mapGroups(analysis.eligibleBansByCost);
    dto.effect_timing = analysis.effectTiming;
    dto.forced_phase_note = analysis.forcedPhaseNote;
    return dto;
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

SemanticEventDto semanticEventDto(
    const ChessGameContent& content,
    const ChessSemanticEvent& event)
{
    SemanticEventDto dto;
    dto.type = semanticEventTypeId(event.type);
    const auto assignStable = [&dto](ChessSemanticEventStableFields stable)
    {
        dto.primary_id = stable.primaryId;
        dto.secondary_id = stable.secondaryId;
        dto.value = stable.value;
        dto.stable_id = std::move(stable.stableId);
    };
    if (event.type == ChessSemanticEventType::GoldAwarded)
    {
        const auto& detail = chessSemanticEventDetail<ChessGoldAwardedEventDetail>(event);
        dto.base_gold = detail.baseGold;
        dto.interest_gold = detail.interestGold;
        dto.other_gold = detail.otherGold;
        dto.total_gold = detail.totalGold;
        if (detail.sourceComboId >= 0)
        {
            const auto combo = std::ranges::find(
                content.combos(),
                detail.sourceComboId,
                &ComboDef::id);
            if (combo != content.combos().end())
            {
                dto.other_gold_source = combo->name;
            }
        }
        return dto;
    }
    if (event.type == ChessSemanticEventType::ChessMerged)
    {
        const auto& detail = chessSemanticEventDetail<ChessMergeEventDetail>(event);
        const auto* role = content.role(detail.roleId);
        assert(role);
        dto.role_id = detail.roleId;
        dto.role_name = role->Name;
        dto.consumed_instance_ids = detail.consumedInstanceIds;
        dto.new_instance_id = detail.newInstanceId;
        dto.result_star = detail.resultStar;
        dto.inherited_fights_won = detail.inheritedFightsWon;
        dto.deployed = detail.deployed;
        dto.transferred_equipment_instance_ids = detail.transferredEquipmentInstanceIds;
        dto.recursive_merge_followed = detail.recursiveMergeFollowed;
        return dto;
    }
    if (event.type == ChessSemanticEventType::EnemyPlanRerolled)
    {
        const auto& detail = chessSemanticEventDetail<ChessEnemyPlanRerollEventDetail>(event);
        dto.cost = detail.cost;
        dto.previous_enemy_plan_key = std::format(
            "0x{:016x}",
            detail.previousEnemyPlanKey);
        dto.new_enemy_plan_key = std::format(
            "0x{:016x}",
            detail.newEnemyPlanKey);
        dto.effect = "下一次準備戰鬥時重新生成敵方陣容、裝備、地圖與戰鬥種子";
        return dto;
    }
    if (event.type == ChessSemanticEventType::EquipmentAcquired)
    {
        const auto& detail =
            chessSemanticEventDetail<ChessEquipmentAcquiredEventDetail>(event);
        assignStable({detail.equipmentInstanceId, detail.itemId, {}, {}});
        return dto;
    }
    if (event.type == ChessSemanticEventType::EquipmentAssigned)
    {
        const auto& detail =
            chessSemanticEventDetail<ChessEquipmentAssignedEventDetail>(event);
        assignStable({
            detail.equipmentInstanceId,
            detail.targetChessInstanceId,
            detail.itemId,
            {},
        });
        return dto;
    }
    if (event.type == ChessSemanticEventType::LegendaryEquipmentPurchased)
    {
        const auto& detail =
            chessSemanticEventDetail<ChessLegendaryEquipmentPurchasedEventDetail>(event);
        assignStable({
            detail.equipmentInstanceId,
            detail.itemId,
            detail.cost,
            {},
        });
        return dto;
    }
    if (event.type == ChessSemanticEventType::RewardOffered)
    {
        const auto& detail =
            chessSemanticEventDetail<ChessRewardOfferedEventDetail>(event);
        assignStable({{}, {}, detail.optionCount, detail.rewardId});
        return dto;
    }
    if (event.type == ChessSemanticEventType::RewardRerolled)
    {
        const auto& detail =
            chessSemanticEventDetail<ChessRewardRerolledEventDetail>(event);
        assignStable({{}, {}, detail.cost, detail.rewardId});
        return dto;
    }
    if (event.type == ChessSemanticEventType::RewardChosen)
    {
        const auto& detail =
            chessSemanticEventDetail<ChessRewardChosenEventDetail>(event);
        assignStable({{}, {}, {}, detail.rewardId});
        return dto;
    }
    assignStable(chessSemanticEventStableFields(event));
    return dto;
}

bool deploymentChanged(const ChessSessionState& before, const ChessSessionState& after)
{
    if (before.roster.size() != after.roster.size())
    {
        return true;
    }
    for (const auto& [instanceId, piece] : before.roster)
    {
        const auto found = after.roster.find(instanceId);
        if (found == after.roster.end() || found->second.deployed != piece.deployed)
        {
            return true;
        }
    }
    return false;
}

SummaryActionResultDto summaryActionResultDto(
    const ChessGameSession& session,
    const ChessSessionState& before,
    const ChessActionResult& actionResult,
    ChessActionType actionType)
{
    const auto& after = session.state();
    SummaryActionResultDto dto{};
    dto.accepted = actionResult.accepted;
    dto.error_code = ruleErrorId(actionResult.error);
    dto.description = actionResult.description;
    for (const auto& event : actionResult.events)
    {
        dto.events.push_back(semanticEventTypeId(event.type));
        if (event.type == ChessSemanticEventType::ChessMerged)
        {
            const auto& merge = chessSemanticEventDetail<ChessMergeEventDetail>(event);
            const auto* role = session.content().role(merge.roleId);
            assert(role);
            dto.changes.merged_units.push_back({
                merge.roleId,
                role->Name,
                merge.newInstanceId,
                merge.resultStar,
            });
        }
    }
    const auto assignDelta = [](std::optional<int>& field, int value) {
        if (value != 0)
        {
            field = value;
        }
    };
    assignDelta(dto.changes.money, after.money - before.money);
    assignDelta(dto.changes.experience, after.experience - before.experience);
    assignDelta(dto.changes.level, after.level - before.level);
    assignDelta(dto.changes.fight, after.fight - before.fight);
    dto.changes.shop_changed = after.shop != before.shop;
    dto.changes.roster_changed = after.roster != before.roster;
    dto.changes.deployment_changed = deploymentChanged(before, after);
    dto.changes.equipment_changed = after.equipmentInventory != before.equipmentInventory;
    dto.changes.bans_changed = after.bannedRoleIds != before.bannedRoleIds;
    dto.changes.reward_changed = after.pendingRewards != before.pendingRewards;
    dto.changes.prepared_battle_changed = after.preparedBattle != before.preparedBattle;
    for (const auto& event : actionResult.events)
    {
        switch (event.type)
        {
        case ChessSemanticEventType::ShopRefreshed:
            dto.changes.shop_changed = true;
            break;
        case ChessSemanticEventType::ChessPurchased:
        case ChessSemanticEventType::ChessMerged:
        case ChessSemanticEventType::ChessSold:
            dto.changes.roster_changed = true;
            break;
        case ChessSemanticEventType::DeploymentChanged:
            dto.changes.deployment_changed = true;
            break;
        case ChessSemanticEventType::EquipmentAcquired:
        case ChessSemanticEventType::EquipmentAssigned:
        case ChessSemanticEventType::LegendaryEquipmentPurchased:
            dto.changes.equipment_changed = true;
            break;
        case ChessSemanticEventType::RoleBanned:
            dto.changes.bans_changed = true;
            break;
        case ChessSemanticEventType::RewardOffered:
        case ChessSemanticEventType::RewardRerolled:
        case ChessSemanticEventType::RewardChosen:
        case ChessSemanticEventType::ForcedBansSkipped:
            dto.changes.reward_changed = true;
            break;
        case ChessSemanticEventType::BattlePrepared:
        case ChessSemanticEventType::MapChosen:
        case ChessSemanticEventType::FormationSwapped:
        case ChessSemanticEventType::BattleStarted:
        case ChessSemanticEventType::BattleEnded:
            dto.changes.prepared_battle_changed = true;
            break;
        default:
            break;
        }
    }
    dto.phase = phaseText(after.phase);
    for (const auto& legal : session.legalActions())
    {
        dto.legal_action_types.push_back(chessActionTypeId(legal.type));
    }
    dto.state_hash = chessSha256Hex(session.observe().stateHash);
    if (actionType == ChessActionType::StartBattle && session.lastBattleResult())
    {
        const auto& battle = *session.lastBattleResult();
        dto.battle = SummaryBattleDto{
            battleOutcomeId(battle.summary.outcome),
            chessBattleOutcomeDescription(battle.summary.outcome),
            battle.summary.endFrame,
        };
    }
    return dto;
}

CompactRejectedActionResultDto compactRejectedActionResultDto(
    const ChessGameSession& session,
    const ChessActionResult& actionResult)
{
    assert(!actionResult.accepted);
    CompactRejectedActionResultDto dto;
    dto.accepted = false;
    dto.error_code = ruleErrorId(actionResult.error);
    dto.description = actionResult.description;
    dto.next_observation.phase = phaseText(session.observe().phase);
    for (const auto& legal : session.legalActions())
    {
        dto.next_observation.legal_action_types.push_back(
            chessActionTypeId(legal.type));
    }
    dto.next_observation.state_hash = chessSha256Hex(session.observe().stateHash);
    return dto;
}

ActionResultDto actionResultDto(
    const ChessGameSession& session,
    const ChessActionResult& actionResult,
    ChessActionType actionType,
    ActionResponseDetail detail)
{
    assert(detail != ActionResponseDetail::Summary);
    const auto observationDetail = detail == ActionResponseDetail::Full
        ? ObservationDetail::Full
        : ObservationDetail::Compact;
    const auto legalActions = session.legalActions();
    ActionResultDto dto;
    dto.accepted = actionResult.accepted;
    dto.error_code = ruleErrorId(actionResult.error);
    dto.description = actionResult.description;
    dto.next_observation = observationDto(
        session.observe(),
        session.content(),
        observationDetail,
        {},
        legalActions);
    dto.replay_sequence = actionResult.replaySequence;
    if (detail == ActionResponseDetail::Full)
    {
        dto.pre_state_hash = chessSha256Hex(actionResult.preStateHash);
        dto.post_state_hash = chessSha256Hex(actionResult.postStateHash);
        dto.event_hash = chessSha256Hex(actionResult.eventHash);
        dto.rng_digest = chessSha256Hex(actionResult.rngDigest);
        dto.chain_hash = chessSha256Hex(actionResult.chainHash);
    }
    for (const auto& event : actionResult.events)
    {
        dto.events.push_back(semanticEventDto(session.content(), event));
    }
    if (actionResult.accepted
        && detail == ActionResponseDetail::Full
        && actionType == ChessActionType::StartBattle
        && session.lastBattlePrepared()
        && session.lastBattleResult())
    {
        dto.battle = battleResultDto(
            session.content(),
            *session.lastBattlePrepared(),
            *session.lastBattleResult(),
            observationDetail);
    }
    return dto;
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

BattleEffectActivationDto battleEffectActivationDto(
    const ChessBattleEffectActivation& activation)
{
    BattleEffectActivationDto dto;
    dto.type = activation.type;
    dto.description = activation.description;
    dto.frame = activation.frame;
    dto.source_unit_id = activation.sourceUnitId;
    dto.source_name = activation.sourceName;
    if (activation.sourceTeam)
    {
        dto.source_team = chessBattleTeamName(*activation.sourceTeam);
    }
    dto.source_kind = activation.sourceKind;
    dto.target_unit_id = activation.targetUnitId;
    dto.target_name = activation.targetName;
    if (activation.targetTeam)
    {
        dto.target_team = chessBattleTeamName(*activation.targetTeam);
    }
    dto.value = activation.value;
    dto.duration_frames = activation.durationFrames;
    dto.previous_value = activation.previousValue;
    dto.new_value = activation.newValue;
    dto.delta = activation.delta;
    dto.ability_id = activation.abilityId;
    dto.ability_name = activation.abilityName;
    dto.poison_percent = activation.poisonPercent;
    dto.scheduled_ticks = activation.scheduledTicks;
    dto.source_projectile_id = activation.sourceProjectileId;
    dto.opposing_projectile_id = activation.opposingProjectileId;
    dto.source_value_before = activation.sourceValueBefore;
    dto.opposing_value_before = activation.opposingValueBefore;
    dto.cancelled_potential_damage = activation.cancelledPotentialDamage;
    dto.source_value_after = activation.sourceValueAfter;
    dto.opposing_value_after = activation.opposingValueAfter;
    return dto;
}

BattleImportantEffectDto battleImportantEffectDto(
    const ChessBattleImportantEffect& effect)
{
    BattleImportantEffectDto dto;
    dto.type = effect.type;
    dto.source_unit_id = effect.sourceUnitId;
    dto.source_name = effect.sourceName;
    if (effect.sourceTeam)
    {
        dto.source_team = chessBattleTeamName(*effect.sourceTeam);
    }
    dto.source_kind = effect.sourceKind;
    dto.target_unit_id = effect.targetUnitId;
    dto.target_name = effect.targetName;
    if (effect.targetTeam)
    {
        dto.target_team = chessBattleTeamName(*effect.targetTeam);
    }
    dto.count = effect.count;
    dto.value = effect.value;
    dto.attack_delta = effect.attackDelta;
    dto.defence_delta = effect.defenceDelta;
    dto.description = effect.description;
    return dto;
}

BattleUnitStatsDto battleUnitStatsDto(
    const ChessBattleUnitAnalysis& analysis,
    bool full)
{
    BattleUnitStatsDto dto;
    dto.unit_id = analysis.unitId;
    dto.role_id = analysis.roleId;
    dto.name = analysis.name;
    dto.team = chessBattleTeamName(analysis.team);
    dto.star = analysis.star;
    dto.damage_dealt = analysis.damageDealt;
    dto.damage_taken = analysis.damageTaken;
    dto.kills = analysis.kills;
    const auto assignMetric = [full](std::optional<int>& field, int value)
    {
        if (full || value != 0)
        {
            field = value;
        }
    };
    assignMetric(dto.healing_done, analysis.healingDone);
    assignMetric(dto.magic_points_restored, analysis.magicPointsRestored);
    assignMetric(dto.magic_points_drained, analysis.magicPointsDrained);
    assignMetric(dto.magic_points_drain_events, analysis.magicPointsDrainEvents);
    assignMetric(
        dto.projectile_potential_damage_cancelled,
        analysis.projectilePotentialDamageCancelled);
    assignMetric(dto.projectile_cancellations, analysis.projectileCancellations);
    assignMetric(dto.blocks, analysis.blocks);
    assignMetric(dto.dodges, analysis.dodges);
    assignMetric(dto.poison_payload_events, analysis.poisonPayloadEvents);
    assignMetric(dto.poison_application_events, analysis.poisonApplicationEvents);
    assignMetric(dto.poison_ticks, analysis.poisonTicks);
    assignMetric(dto.poison_damage, analysis.poisonDamage);
    assignMetric(dto.bleed_applications, analysis.bleedApplications);
    assignMetric(dto.hitstun_applications, analysis.hitstunApplications);
    assignMetric(dto.hitstun_duration_frames, analysis.hitstunDurationFrames);
    assignMetric(dto.stun_applications, analysis.stunApplications);
    assignMetric(dto.stun_duration_frames, analysis.stunDurationFrames);
    assignMetric(dto.knockback_applications, analysis.knockbackApplications);
    assignMetric(dto.magic_block_applications, analysis.magicBlockApplications);
    assignMetric(dto.magic_block_duration_frames, analysis.magicBlockDurationFrames);
    assignMetric(dto.cooldown_manipulations, analysis.cooldownManipulations);
    assignMetric(dto.cooldown_manipulation_frames, analysis.cooldownManipulationFrames);
    assignMetric(dto.invulnerability_triggers, analysis.invulnerabilityTriggers);
    assignMetric(dto.death_prevention_triggers, analysis.deathPreventionTriggers);
    assignMetric(dto.enemy_attack_debuff, analysis.enemyAttackDebuff);
    assignMetric(dto.enemy_defence_debuff, analysis.enemyDefenceDebuff);
    if (full)
    {
        dto.initial_combat_stats = roleStatsDto(analysis.initialCombatStats);
        dto.initial_stat_delta_from_special_effects =
            roleStatsDto(analysis.initialStatDeltaFromSpecialEffects);
    }
    dto.damage_breakdown = {
        analysis.damageBreakdown.skill,
        analysis.damageBreakdown.basicAttack,
        analysis.damageBreakdown.status,
        analysis.damageBreakdown.combo,
        analysis.damageBreakdown.equipment,
        analysis.damageBreakdown.other,
    };
    for (const auto& damage : analysis.skillDamage)
    {
        dto.skill_damage.push_back({damage.skillId, damage.name, damage.damage});
    }
    for (const auto& damage : analysis.nonSkillDamageSources)
    {
        dto.non_skill_damage_sources.push_back({
            damage.skillId,
            damage.name,
            damage.damage,
        });
    }
    return dto;
}

BattleResultDto battleResultDto(
    const ChessGameContent& content,
    const PreparedChessBattle& prepared,
    const HeadlessBattleResult& battle,
    ObservationDetail detail)
{
    BattleResultDto dto;
    const bool full = detail == ObservationDetail::Full;
    const auto analysis = analyzeChessBattleResult(content, prepared, battle);
    dto.detail = full ? "full" : "compact";
    if (full)
    {
        dto.initial_board = preparedBattleDto(
            content,
            prepared,
            PreparedBattleDetail::Full,
            {},
            kChessBattleFrameLimit);
        dto.effect_activations.emplace();
    }
    dto.outcome = battleOutcomeId(analysis.outcome);
    dto.outcome_description = analysis.outcomeDescription;
    dto.end_frame = analysis.endFrame;
    for (const auto& survivor : analysis.survivors)
    {
        dto.survivors.push_back({
            survivor.unitId,
            survivor.chessInstanceId,
            survivor.roleId,
            survivor.name,
            chessBattleTeamName(survivor.team),
            survivor.hp,
            survivor.mp,
            survivor.summoned,
        });
    }
    for (const auto& unit : analysis.unitStats)
    {
        dto.unit_stats.push_back(battleUnitStatsDto(unit, full));
    }
    for (const auto& effect : analysis.importantEffects)
    {
        dto.important_effects.push_back(battleImportantEffectDto(effect));
    }
    if (full)
    {
        for (const auto& activation : analysis.effectActivations)
        {
            dto.effect_activations->push_back(battleEffectActivationDto(activation));
        }
    }
    for (const auto& event : analysis.keyEvents)
    {
        dto.key_events.push_back({event.frame, event.description});
    }
    dto.summary = analysis.summary;
    dto.digest = chessSha256Hex(analysis.digest);
    return dto;
}

std::optional<BattleResultDto> inspectLastBattleDto(
    const ChessGameSession& session,
    ObservationDetail detail)
{
    if (!session.lastBattlePrepared() || !session.lastBattleResult())
    {
        return std::nullopt;
    }
    return battleResultDto(
        session.content(),
        *session.lastBattlePrepared(),
        *session.lastBattleResult(),
        detail);
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
    const ChessSaveStore& saves,
    ObservationDetail detail)
{
    SessionObservationDto result{};
    const auto legalActions = session.legalActions();
    result.game_state = observationDto(
        session.observe(),
        session.content(),
        detail,
        {},
        legalActions);
    for (const auto& slot : saves.list(session))
    {
        result.save_slots.push_back(saveSlotDto(slot));
    }
    result.operations = {
        "inspect_role",
        "inspect_shop_slot",
        "inspect_shop",
        "get_shop_odds",
        "inspect_chess_instance",
        "inspect_bans",
        "inspect_combo",
        "inspect_equipment",
        "inspect_challenge",
        "inspect_prepared_battle",
        "inspect_last_battle",
        "list_saves",
        "inspect_save",
        "save_game",
        "load_game",
        "export_save",
        "import_save",
        "export_replay",
    };
    result.load_consequence = "載入會替換目前遊戲狀態、亂數與完整重播紀錄，捨棄存檔點之後的目前行動，但保留存檔目錄。";
    return result;
}


}  // namespace KysChess::ProtocolDetail
