#include "ChessJsonProtocol.h"

#include "BattleStarStats.h"
#include "ChessAsciiBoard.h"
#include "ChessBattleMapCatalog.h"
#include "ChessReplayJson.h"
#include "ChessReplayVerifier.h"
#include "ChessRewardRules.h"

#include <glaze/json.hpp>

#include <cassert>
#include <charconv>
#include <algorithm>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <span>
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
    std::string detail = "full";
    std::optional<bool> position_swap_enabled;
};

struct ActParams
{
    glz::raw_json action;
    std::string detail = "summary";
};

struct ObserveParams { std::string detail = "full"; };
struct RoleParams { int role_id = -1; };
struct ComboParams { std::string combo_name; };
struct EquipmentParams { int item_id = -1; };
struct ChallengeParams { std::string challenge_name; };
struct ShopSlotParams { int slot = -1; };
struct ChessInstanceParams { int chess_instance_id = -1; };
struct ShopOddsParams { std::optional<int> level; };

struct VerifyParams
{
    std::string replay_jsonl;
};

struct SlotParams { std::string slot; };
struct SaveParams { std::string slot; std::string label; };
struct ImportSaveParams { std::string slot; std::string payload; };

enum class ObservationDetail { Compact, Full };
enum class ActionResponseDetail { Summary, Compact, Full };

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

struct RoleStatsDto
{
    int max_hp{};
    int max_mp{};
    int attack{};
    int defence{};
    int speed{};
    int fist{};
    int sword{};
    int knife{};
    int unusual{};
    int hidden_weapon{};
};
struct AbilityDto
{
    struct StarPower { int star{}; int power{}; };
    int magic_id{};
    std::string name;
    std::vector<StarPower> power_by_star;
    int mp_cost{};
    int select_distance{};
    int area_distance{};
    std::string geometry;
    std::vector<std::string> effects;
    std::string effect_note;
};
struct RoleDto
{
    int role_id{};
    std::string name;
    int cost{};
    RoleStatsDto base_stats;
    std::vector<AbilityDto> abilities;
    std::vector<std::string> combos;
};
struct ShopSlotDto { int slot{}; int role_id = -1; std::string name; int cost{}; };
struct PieceDto
{
    int instance_id{};
    int role_id{};
    std::string name;
    int star{};
    bool deployed{};
    int fights_won{};
    std::optional<RoleStatsDto> current_stats;
    std::optional<std::string> current_stats_note;
};
struct EquipmentInfoDto
{
    struct CharacterBonus
    {
        std::vector<std::string> roles;
        std::vector<std::string> effects;
        std::vector<std::string> counts_as_combos;
    };
    int item_id = -1;
    std::string name;
    int tier{};
    std::string type;
    std::optional<std::vector<std::string>> base_stat_effects;
    std::optional<std::vector<std::string>> special_effects;
    std::optional<std::vector<std::string>> counts_as_combos;
    std::optional<std::vector<CharacterBonus>> character_bonuses;
    std::optional<std::string> combo_counting_note;
};
struct EquipmentDto
{
    int instance_id{};
    EquipmentInfoDto equipment;
    int assigned_chess_instance_id = -1;
};
struct NamedIdDto { int id{}; std::string name; };
struct PreparedUnitDto
{
    int unit_id{};
    int chess_instance_id = -1;
    int role_id{};
    std::string name;
    std::string team;
    int star{};
    std::string weapon;
    std::string armor;
    int x{};
    int y{};
    std::optional<RoleStatsDto> preview_stats;
    std::optional<std::string> stats_note;
    std::optional<std::vector<AbilityDto>> abilities;
};
struct MapDto { int map_id{}; std::string name; };
struct ComboDto;
struct PreparedBattleDto
{
    std::string battle_id;
    std::string metadata_scope;
    std::vector<PreparedUnitDto> units;
    std::vector<MapDto> map_candidates;
    int chosen_map_id = -1;
    std::string chosen_map_name;
    std::string coordinate_system;
    std::string board;
    std::vector<ComboDto> ally_synergies;
    std::vector<ComboDto> enemy_synergies;
    std::uint32_t battle_seed{};
};
struct SkillDamageDto
{
    int skill_id = -1;
    std::string name;
    int damage{};
};
struct BattleSurvivorDto
{
    int unit_id = -1;
    int chess_instance_id = -1;
    int role_id = -1;
    std::string name;
    std::string team;
    int hp{};
    int mp{};
    bool summoned{};
};
struct BattleUnitStatsDto
{
    struct DamageBreakdown
    {
        int skill{};
        int basic_attack{};
        int status{};
        int combo{};
        int equipment{};
        int other{};
    };
    int unit_id = -1;
    int role_id = -1;
    std::string name;
    std::string team;
    int star{};
    int damage_dealt{};
    int damage_taken{};
    int kills{};
    std::optional<int> healing_done;
    std::optional<int> magic_points_restored;
    std::optional<int> magic_points_drained;
    std::optional<int> magic_points_drain_events;
    std::optional<int> projectile_potential_damage_cancelled;
    std::optional<int> projectile_cancellations;
    std::optional<int> blocks;
    std::optional<int> dodges;
    std::optional<int> poison_payload_events;
    std::optional<int> poison_application_events;
    std::optional<int> poison_ticks;
    std::optional<int> poison_damage;
    std::optional<int> bleed_applications;
    std::optional<int> hitstun_applications;
    std::optional<int> hitstun_duration_frames;
    std::optional<int> stun_applications;
    std::optional<int> stun_duration_frames;
    std::optional<int> knockback_applications;
    std::optional<int> magic_block_applications;
    std::optional<int> magic_block_duration_frames;
    std::optional<int> cooldown_manipulations;
    std::optional<int> cooldown_manipulation_frames;
    std::optional<int> invulnerability_triggers;
    std::optional<int> death_prevention_triggers;
    std::optional<RoleStatsDto> initial_combat_stats;
    std::optional<RoleStatsDto> initial_stat_delta_from_special_effects;
    std::optional<int> enemy_attack_debuff;
    std::optional<int> enemy_defence_debuff;
    DamageBreakdown damage_breakdown;
    std::vector<SkillDamageDto> skill_damage;
    std::vector<SkillDamageDto> non_skill_damage_sources;
};
struct BattleEffectActivationDto
{
    std::string type;
    std::string description;
    int frame{};
    std::optional<int> source_unit_id;
    std::optional<std::string> source_name;
    std::optional<std::string> source_team;
    std::optional<std::string> source_kind;
    std::optional<int> target_unit_id;
    std::optional<std::string> target_name;
    std::optional<std::string> target_team;
    std::optional<int> value;
    std::optional<int> duration_frames;
    std::optional<int> previous_value;
    std::optional<int> new_value;
    std::optional<int> delta;
    std::optional<int> ability_id;
    std::optional<std::string> ability_name;
    std::optional<int> poison_percent;
    std::optional<int> scheduled_ticks;
    std::optional<int> source_projectile_id;
    std::optional<int> opposing_projectile_id;
    std::optional<int> source_value_before;
    std::optional<int> opposing_value_before;
    std::optional<int> cancelled_potential_damage;
    std::optional<int> source_value_after;
    std::optional<int> opposing_value_after;
};
struct BattleImportantEffectDto
{
    std::string type;
    std::optional<int> source_unit_id;
    std::optional<std::string> source_name;
    std::optional<std::string> source_team;
    std::optional<std::string> source_kind;
    std::optional<int> target_unit_id;
    std::optional<std::string> target_name;
    std::optional<std::string> target_team;
    std::optional<int> count;
    std::optional<int> value;
    std::optional<int> attack_delta;
    std::optional<int> defence_delta;
    std::string description;
};
struct BattleKeyEventDto
{
    int frame{};
    std::string description;
};
struct BattleResultDto
{
    std::string detail;
    std::optional<PreparedBattleDto> initial_board;
    std::string outcome;
    std::string outcome_description;
    int end_frame{};
    std::vector<BattleSurvivorDto> survivors;
    std::vector<BattleUnitStatsDto> unit_stats;
    std::vector<BattleImportantEffectDto> important_effects;
    std::optional<std::vector<BattleEffectActivationDto>> effect_activations;
    std::vector<BattleKeyEventDto> key_events;
    std::string summary;
    std::string digest;
};
struct RewardOptionDto
{
    std::string id;
    std::string kind;
    std::string label;
    std::string description;
    int role_id = -1;
    int chess_instance_id = -1;
    std::optional<EquipmentInfoDto> equipment;
};
struct ChallengeEnemyDto
{
    int role_id{};
    std::string name;
    int star{};
    std::optional<EquipmentInfoDto> weapon;
    std::optional<EquipmentInfoDto> armor;
};
struct ChallengeRewardDto
{
    std::string description;
};
struct ChallengeDto
{
    std::string name;
    std::string description;
    int enemy_count{};
    std::vector<ChallengeEnemyDto> enemies;
    std::vector<ChallengeRewardDto> rewards;
};
struct ShopTierOddsDto
{
    int tier{};
    int configured_weight{};
    double probability{};
    int available_role_count{};
    std::vector<NamedIdDto> available_roles;
};
struct ShopOddsDto
{
    int level{};
    int total_effective_weight{};
    std::vector<ShopTierOddsDto> tiers;
    std::string pool_note;
};
struct ShopPurchaseSynergyDto
{
    std::string name;
    int current{};
    int after_purchase{};
};
struct ShopSlotInspectionDto
{
    struct CopiesByStar { int star{}; int count{}; };
    int slot{};
    bool occupied{};
    int role_id = -1;
    std::string name;
    int cost{};
    int shop_tier{};
    int owned_copies{};
    std::vector<CopiesByStar> copies_by_star;
    bool purchase_legal{};
    std::string purchase_error;
    int gold_cost{};
    int projected_gold_after{};
    std::string purchase_result;
    std::vector<ShopPurchaseSynergyDto> synergies;
    double level_tier_probability{};
};
struct ShopInspectionDto
{
    int money{};
    int level{};
    std::vector<ShopSlotInspectionDto> slots;
    ShopOddsDto odds;
};
struct ChessInstanceInspectionDto
{
    PieceDto chess;
    int one_star_equivalent_copies{};
    int same_star_copies{};
    int copies_required_for_next_star{};
    std::vector<EquipmentDto> equipment;
    std::vector<ComboDto> synergy_contributions;
};
struct BanInspectionDto
{
    struct TierGroup
    {
        int cost{};
        std::vector<NamedIdDto> roles;
    };
    int current_ban_count{};
    int maximum_ban_count{};
    int remaining_ban_capacity{};
    std::vector<TierGroup> current_bans_by_cost;
    std::vector<TierGroup> eligible_bans_by_cost;
    std::string effect_timing;
    std::string forced_phase_note;
};
struct PendingRewardDto
{
    struct OptionGroup
    {
        std::string label;
        int count{};
    };
    std::string id;
    std::string kind;
    int option_count{};
    std::optional<std::string> option_count_description;
    std::optional<std::vector<RewardOptionDto>> options;
    int reroll_cost{};
    std::vector<int> eligible_tiers;
    std::vector<OptionGroup> option_groups;
    bool rerolled{};
    std::optional<int> maximum_selections;
    std::optional<int> remaining_selections;
    std::optional<int> current_ban_count;
    std::optional<int> maximum_total_bans;
    std::optional<bool> selection_optional;
    std::optional<std::string> decision_requirement;
};
struct ComboThresholdDto
{
    int required_count{};
    std::string name;
    std::vector<std::string> effects;
    bool active{};
};
struct ComboContributionDto
{
    int role_id{};
    std::string role_name;
    std::vector<int> unit_ids;
    int counted_star{};
    int physical_points{};
    int star_bonus_points{};
    int effective_points{};
    bool natural_member{};
    std::vector<NamedIdDto> equipment_sources;
    std::string explanation;
};
struct ComboDto
{
    std::string name;
    int physical_count{};
    int effective_count{};
    std::optional<std::vector<std::string>> members;
    std::optional<std::vector<ComboThresholdDto>> thresholds;
    std::optional<std::vector<ComboContributionDto>> contributions;
    std::optional<std::string> count_explanation;
};
struct ObservationDto
{
    std::string detail;
    std::string phase;
    std::optional<std::string> difficulty;
    std::optional<bool> position_swap_enabled;
    int money{};
    std::optional<int> interest_gold;
    std::optional<int> next_interest_threshold;
    std::optional<int> maximum_interest_gold;
    int total_campaign_rounds{};
    std::optional<int> boss_interval;
    std::optional<bool> forced_bans_enabled;
    std::optional<int> projected_base_victory_gold;
    std::optional<int> projected_victory_income;
    std::optional<bool> projected_victory_income_excludes_conditional_bonuses;
    int experience{};
    int experience_for_next_level{};
    int level{};
    int maximum_deployment{};
    std::optional<int> current_ban_count;
    std::optional<int> maximum_ban_count;
    std::optional<int> remaining_ban_capacity;
    std::optional<std::string> ban_effect_timing;
    int fight{};
    bool campaign_complete{};
    std::optional<bool> shop_locked;
    std::optional<bool> free_shop_refresh_available;
    std::optional<int> free_shop_refresh_granted_fight;
    std::vector<ShopSlotDto> shop;
    std::vector<PieceDto> roster;
    std::optional<std::string> role_metadata_scope;
    std::optional<std::vector<RoleDto>> relevant_roles;
    std::optional<std::string> equipment_metadata_scope;
    std::vector<EquipmentDto> equipment_inventory;
    std::optional<std::vector<NamedIdDto>> bans;
    std::optional<std::vector<NamedIdDto>> obtained_internal_skills;
    std::vector<std::string> completed_challenges;
    std::vector<ComboDto> combos;
    std::optional<PreparedBattleDto> prepared_battle;
    std::optional<PendingRewardDto> pending_reward;
    std::optional<std::string> last_battle_outcome;
    std::optional<std::string> last_battle_outcome_description;
    std::optional<int> last_battle_end_frame;
    std::optional<std::string> last_battle_digest;
    std::vector<std::string> legal_action_types;
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

std::string battleOutcomeId(Battle::BattleOutcome outcome);

std::string battleOutcomeDescription(Battle::BattleOutcome outcome)
{
    switch (outcome)
    {
    case Battle::BattleOutcome::InProgress: return "尚未完成";
    case Battle::BattleOutcome::PlayerVictory: return "我方勝利";
    case Battle::BattleOutcome::PlayerDefeat: return "我方戰敗";
    case Battle::BattleOutcome::Timeout: return "超過戰鬥時間上限";
    }
    std::unreachable();
}

std::string teamName(int team)
{
    return team == 0 ? "我方" : "敵方";
}

RoleStatsDto baseRoleStats(const ChessRoleDefinition& role)
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

RoleStatsDto pieceRoleStats(
    const ChessRoleDefinition& role,
    const BalanceConfig& balance,
    int star,
    int fightsWon)
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
        fightsWon);
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

void addItemStats(RoleStatsDto& stats, const ChessItemDefinition* item)
{
    if (!item)
    {
        return;
    }
    stats.max_hp += item->addMaxHP;
    stats.attack += item->addAttack;
    stats.defence += item->addDefence;
    stats.speed += item->addSpeed;
    stats.fist += item->addFist;
    stats.sword += item->addSword;
    stats.knife += item->addKnife;
    stats.unusual += item->addUnusual;
    stats.hidden_weapon += item->addHiddenWeapon;
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
    auto currentStats = pieceRoleStats(
        *role,
        content.balance(),
        piece.star,
        piece.fightsWon);
    const auto addEquipmentInstanceStats = [&](int equipmentInstanceId) {
        if (equipmentInstanceId < 0)
        {
            return;
        }
        const auto equipment = std::ranges::find(
            observation.equipmentInventory,
            equipmentInstanceId,
            &ChessEquipmentInstance::instanceId);
        assert(equipment != observation.equipmentInventory.end());
        addItemStats(currentStats, content.item(equipment->itemId));
    };
    addEquipmentInstanceStats(piece.weaponInstanceId);
    addEquipmentInstanceStats(piece.armorInstanceId);
    dto.current_stats = currentStats;
    dto.current_stats_note = "已計入星級、勝場成長與裝備基礎屬性；羈絆與裝備特殊效果於開戰時套用";
    return dto;
}

RoleStatsDto preparedUnitStats(
    const ChessGameContent& content,
    const ChessRoleDefinition& role,
    const PreparedChessBattleUnit& unit)
{
    auto stats = pieceRoleStats(
        role,
        content.balance(),
        unit.star,
        unit.fightsWon);
    addItemStats(stats, unit.weaponItemId >= 0 ? content.item(unit.weaponItemId) : nullptr);
    addItemStats(stats, unit.armorItemId >= 0 ? content.item(unit.armorItemId) : nullptr);
    return stats;
}

std::vector<std::string> magicEffects(const ChessGameContent& content, int magicId)
{
    std::vector<std::string> result;
    const auto definition = std::ranges::find(content.magicEffects(), magicId, &ChessMagicEffectDefinition::magicId);
    if (definition != content.magicEffects().end())
    {
        for (const auto& effect : definition->effects)
        {
            result.push_back(comboEffectDesc(effect));
        }
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

AbilityDto abilityDto(
    const ChessGameContent& content,
    const ChessMagicDefinition& magic,
    std::vector<AbilityDto::StarPower> powerByStar)
{
    auto effects = magicEffects(content, magic.ID);
    const bool hasConfiguredEffects = !effects.empty();
    return {
        magic.ID,
        magic.Name,
        std::move(powerByStar),
        magic.NeedMP,
        magic.SelectDistance,
        magic.AttackDistance,
        magicGeometry(magic),
        std::move(effects),
        hasConfiguredEffects
            ? std::string{}
            : "沒有額外配置的特殊效果；仍會依威力、角色屬性與範圍造成基礎武學效果",
    };
}

RoleDto roleDto(const ChessGameContent& content, int roleId)
{
    const auto* role = content.role(roleId);
    assert(role);
    RoleDto dto;
    dto.role_id = roleId;
    dto.name = role->Name;
    dto.cost = role->Cost;
    dto.base_stats = baseRoleStats(*role);
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
            auto& powerByStar = dto.abilities[found->second].power_by_star;
            const auto existing = std::ranges::find(powerByStar, star, &AbilityDto::StarPower::star);
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
        abilityIndexes.emplace(magicId, dto.abilities.size());
        dto.abilities.push_back(abilityDto(
            content,
            *magic,
            {{star, role->MagicPower[slot]}}));
    }
    for (const auto& combo : content.combos())
    {
        if (std::ranges::contains(combo.memberRoleIds, roleId))
        {
            dto.combos.push_back(combo.name);
        }
    }
    return dto;
}

const EquipmentDef& requireEquipment(const ChessGameContent& content, int itemId)
{
    const auto found = std::ranges::find(content.equipment(), itemId, &EquipmentDef::itemId);
    assert(found != content.equipment().end());
    return *found;
}

void appendItemStat(std::vector<std::string>& effects, std::string_view name, int value)
{
    if (value != 0)
    {
        effects.push_back(std::format("{}{:+}", name, value));
    }
}

EquipmentInfoDto equipmentInfoDto(
    const ChessGameContent& content,
    int itemId,
    bool full = true)
{
    const auto& definition = requireEquipment(content, itemId);
    const auto* item = content.item(itemId);
    assert(item);
    EquipmentInfoDto dto;
    dto.item_id = itemId;
    dto.name = item->name;
    dto.tier = definition.tier;
    dto.type = definition.equipType == 0 ? "武器" : "防具";
    if (!full)
    {
        return dto;
    }
    dto.base_stat_effects.emplace();
    dto.special_effects.emplace();
    dto.counts_as_combos.emplace();
    dto.character_bonuses.emplace();
    appendItemStat(*dto.base_stat_effects, "生命", item->addMaxHP);
    appendItemStat(*dto.base_stat_effects, "攻擊", item->addAttack);
    appendItemStat(*dto.base_stat_effects, "防禦", item->addDefence);
    appendItemStat(*dto.base_stat_effects, "速度", item->addSpeed);
    appendItemStat(*dto.base_stat_effects, "拳掌", item->addFist);
    appendItemStat(*dto.base_stat_effects, "御劍", item->addSword);
    appendItemStat(*dto.base_stat_effects, "耍刀", item->addKnife);
    appendItemStat(*dto.base_stat_effects, "特殊", item->addUnusual);
    appendItemStat(*dto.base_stat_effects, "暗器", item->addHiddenWeapon);
    for (const auto& effect : definition.effects)
    {
        if (effect.type != EffectType::ActAsCombo)
        {
            dto.special_effects->push_back(comboEffectDesc(effect));
        }
    }
    *dto.counts_as_combos = definition.actAsComboNames;
    if (!dto.counts_as_combos->empty())
    {
        dto.combo_counting_note = "讓裝備者視為該羈絆的一名成員；同一角色在同一羈絆只計一次，裝在原成員身上不會額外加點";
    }
    for (const auto& synergy : content.equipmentSynergies())
    {
        if (synergy.equipmentId != itemId)
        {
            continue;
        }
        EquipmentInfoDto::CharacterBonus bonus;
        for (const int roleId : synergy.roleIds)
        {
            const auto* role = content.role(roleId);
            assert(role);
            bonus.roles.push_back(role->Name);
        }
        bonus.counts_as_combos = synergy.actAsComboNames;
        for (const auto& effect : synergy.effects)
        {
            bonus.effects.push_back(comboEffectDesc(effect));
        }
        dto.character_bonuses->push_back(std::move(bonus));
    }
    return dto;
}

std::string itemName(const ChessGameContent& content, int itemId)
{
    if (itemId < 0)
    {
        return {};
    }
    const auto* item = content.item(itemId);
    assert(item);
    return item->name;
}

ComboDto comboDto(
    const ChessGameContent& content,
    const ComboDef& definition,
    int physicalCount,
    int effectiveCount,
    int activeThresholdIndex,
    bool full = true,
    const std::vector<ResolvedChessComboContribution>* contributions = nullptr)
{
    ComboDto dto;
    dto.name = definition.name;
    dto.physical_count = physicalCount;
    dto.effective_count = effectiveCount;
    if (full)
    {
        dto.count_explanation = "physical_count 是不重複角色數；effective_count 再加上此羈絆允許的星級加點；裝備只讓非成員取得成員資格，不會讓同一角色重複計點";
        dto.members.emplace();
        dto.thresholds.emplace();
        dto.contributions.emplace();
    }
    if (full && contributions)
    {
        for (const auto& contribution : *contributions)
        {
            const auto* role = content.role(contribution.roleId);
            assert(role);
            ComboContributionDto contributionDto;
            contributionDto.role_id = contribution.roleId;
            contributionDto.role_name = role->Name;
            contributionDto.unit_ids = contribution.unitIds;
            contributionDto.counted_star = contribution.countedStar;
            contributionDto.physical_points = contribution.physicalPoints;
            contributionDto.star_bonus_points = contribution.starBonusPoints;
            contributionDto.effective_points = contribution.physicalPoints
                + contribution.starBonusPoints;
            contributionDto.natural_member = contribution.naturalMember;
            for (const int itemId : contribution.equipmentItemIds)
            {
                contributionDto.equipment_sources.push_back({itemId, itemName(content, itemId)});
            }
            if (contribution.naturalMember && !contribution.equipmentItemIds.empty())
            {
                contributionDto.explanation = "角色本身已屬於羈絆；裝備亦可計作此羈絆，但不重複加點";
            }
            else if (!contribution.equipmentItemIds.empty())
            {
                contributionDto.explanation = "角色由裝備取得此羈絆的成員資格";
            }
            else
            {
                contributionDto.explanation = "角色本身屬於此羈絆";
            }
            if (contribution.starBonusPoints > 0)
            {
                contributionDto.explanation += std::format(
                    "；{}★另提供 {} 點星級加成",
                    contribution.countedStar,
                    contribution.starBonusPoints);
            }
            dto.contributions->push_back(std::move(contributionDto));
        }
    }
    if (full)
    {
        for (const int roleId : definition.memberRoleIds)
        {
            const auto* role = content.role(roleId);
            assert(role);
            dto.members->push_back(role->Name);
        }
    }
    if (!full)
    {
        return dto;
    }
    for (int index = 0; index < static_cast<int>(definition.thresholds.size()); ++index)
    {
        const auto& threshold = definition.thresholds[index];
        ComboThresholdDto thresholdDto;
        thresholdDto.required_count = threshold.count;
        thresholdDto.name = threshold.name;
        thresholdDto.active = index <= activeThresholdIndex;
        for (const auto& effect : threshold.effects)
        {
            thresholdDto.effects.push_back(comboEffectDesc(effect));
        }
        dto.thresholds->push_back(std::move(thresholdDto));
    }
    return dto;
}

std::vector<ComboDto> preparedTeamSynergies(
    const ChessGameContent& content,
    const PreparedChessBattle& battle,
    int team)
{
    ChessSessionState state;
    int nextPieceId = 1;
    int nextEquipmentId = 1;
    for (const auto& unit : battle.units)
    {
        if (unit.team != team)
        {
            continue;
        }
        ChessSessionPiece piece;
        piece.instanceId = nextPieceId++;
        piece.roleId = unit.roleId;
        piece.star = unit.star;
        piece.deployed = true;
        piece.fightsWon = unit.fightsWon;
        const auto addEquipment = [&](int itemId, int& instanceField) {
            if (itemId < 0)
            {
                return;
            }
            const int instanceId = nextEquipmentId++;
            state.equipmentInventory.emplace(
                instanceId,
                ChessEquipmentInstance{instanceId, itemId, piece.instanceId});
            instanceField = instanceId;
        };
        addEquipment(unit.weaponItemId, piece.weaponInstanceId);
        addEquipment(unit.armorItemId, piece.armorInstanceId);
        state.roster.emplace(piece.instanceId, piece);
    }
    std::vector<ComboDto> result;
    for (const auto& definition : content.combos())
    {
        const auto progress = evaluateChessComboProgress(state, content, definition);
        if (!progress.active)
        {
            continue;
        }
        result.push_back(comboDto(
            content,
            definition,
            progress.physicalCount,
            progress.effectiveCount,
            progress.activeThresholdIndex,
            true,
            &progress.contributions));
    }
    return result;
}

std::string mapName(const ChessGameContent& content, int mapId)
{
    if (const auto curated = ChessBattleMapCatalog::displayName(mapId); !curated.empty())
    {
        return std::string(curated);
    }
    const auto found = content.battleMaps().find(mapId);
    assert(found != content.battleMaps().end());
    return found->second.name.empty()
        ? std::format("戰場 {}", mapId)
        : found->second.name;
}

PreparedBattleDto preparedBattleDto(
    const ChessGameContent& content,
    const PreparedChessBattle& battle,
    bool full = true)
{
    PreparedBattleDto prepared;
    prepared.battle_id = battle.stableBattleId;
    prepared.metadata_scope = full ? "complete" : "omitted_compact";
    prepared.chosen_map_id = battle.chosenMapId;
    prepared.coordinate_system = "x 向右增加，y 向下增加；A 表示我方、E 表示敵方、# 是障礙、~ 是水域、. 是可通行地面";
    for (const int mapId : battle.mapCandidates)
    {
        prepared.map_candidates.push_back({mapId, mapName(content, mapId)});
    }
    if (battle.chosenMapId >= 0)
    {
        prepared.chosen_map_name = mapName(content, battle.chosenMapId);
        if (full)
        {
            prepared.board = ChessAsciiBoard::render(battle, content);
        }
    }
    if (full)
    {
        prepared.ally_synergies = preparedTeamSynergies(content, battle, 0);
        prepared.enemy_synergies = preparedTeamSynergies(content, battle, 1);
    }
    prepared.battle_seed = battle.battleSeed;
    for (const auto& unit : battle.units)
    {
        const auto* role = content.role(unit.roleId);
        assert(role);
        PreparedUnitDto unitDto;
        unitDto.unit_id = unit.unitId;
        unitDto.chess_instance_id = unit.chessInstanceId;
        unitDto.role_id = unit.roleId;
        unitDto.name = role->Name;
        unitDto.team = teamName(unit.team);
        unitDto.star = unit.star;
        unitDto.weapon = itemName(content, unit.weaponItemId);
        unitDto.armor = itemName(content, unit.armorItemId);
        unitDto.x = unit.x;
        unitDto.y = unit.y;
        if (full)
        {
            unitDto.preview_stats = preparedUnitStats(content, *role, unit);
            unitDto.stats_note = "已計入星級、勝場成長與裝備基礎屬性；羈絆與裝備特殊效果另見隊伍羈絆及裝備說明";
            unitDto.abilities.emplace();
            for (const auto& [magic, power] : chessRoleMagicsForStar(content, *role, unit.star))
            {
                unitDto.abilities->push_back(abilityDto(content, *magic, {{unit.star, power}}));
            }
        }
        prepared.units.push_back(std::move(unitDto));
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
    ObservationDetail detail = ObservationDetail::Full,
    std::string roleMetadataScope = {},
    std::span<const ChessLegalActionDescriptor> legalActions = {})
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
        dto.prepared_battle = preparedBattleDto(content, *observation.preparedBattle, full);
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
                const auto& definition = requireEquipment(content, option.value);
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
        dto.last_battle_outcome_description = battleOutcomeDescription(observation.lastBattleOutcome);
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

struct LegalActionDto
{
    struct EconomicPreview
    {
        int current_gold{};
        int gold_cost{};
        int projected_gold_after{};
        std::optional<int> experience_gained;
        std::optional<int> projected_experience_after;
        std::optional<int> projected_level_after;
        std::optional<int> affected_option_count;
        std::optional<bool> consumes_free_shop_refresh;
        std::string result;
    };
    std::string type;
    std::string description;
    glz::raw_json action_schema = "{}";
    glz::raw_json example = "{}";
    std::optional<std::string> example_note;
    std::optional<EconomicPreview> economic_preview;
    struct Candidate
    {
        std::optional<int> id;
        std::string value;
        std::string label;
        std::string description;
        std::optional<std::string> group;
        std::optional<int> assigned_chess_instance_id;
        std::optional<std::string> assigned_to;
        std::optional<int> gold_cost;
        std::optional<int> gold_gain;
        std::optional<int> projected_gold_after;
    };
    struct FieldCandidates
    {
        std::string field;
        bool multiple{};
        std::vector<Candidate> candidates;
    };
    std::vector<FieldCandidates> candidates_by_field;
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
    std::optional<int> primary_id;
    std::optional<int> secondary_id;
    std::optional<int> value;
    std::optional<std::string> stable_id;
    std::optional<int> base_gold;
    std::optional<int> interest_gold;
    std::optional<int> other_gold;
    std::optional<int> total_gold;
    std::optional<std::string> other_gold_source;
    std::optional<int> role_id;
    std::optional<std::string> role_name;
    std::optional<std::vector<int>> consumed_instance_ids;
    std::optional<int> new_instance_id;
    std::optional<int> result_star;
    std::optional<int> inherited_fights_won;
    std::optional<bool> deployed;
    std::optional<std::vector<int>> transferred_equipment_instance_ids;
    std::optional<bool> recursive_merge_followed;
    std::optional<int> cost;
    std::optional<std::string> previous_enemy_plan_key;
    std::optional<std::string> new_enemy_plan_key;
    std::optional<std::string> effect;
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
    std::optional<std::string> pre_state_hash;
    std::optional<std::string> post_state_hash;
    std::optional<std::string> event_hash;
    std::optional<std::string> rng_digest;
    std::optional<std::string> chain_hash;
};

struct SummaryActionChangesDto
{
    struct Merge
    {
        int role_id{};
        std::string role_name;
        int new_instance_id{};
        int result_star{};
    };
    std::optional<int> money;
    std::optional<int> experience;
    std::optional<int> level;
    std::optional<int> fight;
    bool shop_changed{};
    bool roster_changed{};
    bool deployment_changed{};
    bool equipment_changed{};
    bool bans_changed{};
    bool reward_changed{};
    bool prepared_battle_changed{};
    std::vector<Merge> merged_units;
};

struct SummaryBattleDto
{
    std::string outcome;
    std::string outcome_description;
    int end_frame{};
};

struct SummaryActionResultDto
{
    bool accepted{};
    std::string error_code;
    std::string description;
    std::vector<std::string> events;
    SummaryActionChangesDto changes;
    std::string phase;
    std::vector<std::string> legal_action_types;
    std::string state_hash;
    std::optional<SummaryBattleDto> battle;
};

struct CompactRejectedObservationDto
{
    std::string phase;
    std::vector<std::string> legal_action_types;
    std::string state_hash;
};

struct CompactRejectedActionResultDto
{
    bool accepted{};
    std::string error_code;
    std::string description;
    CompactRejectedObservationDto next_observation;
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
    std::string result = std::format(
        "{}；{} 名敵人；{}；武器 {}/{}、防具 {}/{}；獎勵擇一：",
        challenge.description,
        challenge.enemies.size(),
        starSummary,
        equippedWeapons,
        challenge.enemies.size(),
        equippedArmor,
        challenge.enemies.size());
    for (std::size_t index = 0; index < challenge.rewards.size(); ++index)
    {
        if (index > 0) result += "、";
        result += chessChallengeRewardDescription(content, challenge.rewards[index]);
    }
    return result;
}

ChallengeDto challengeDto(
    const ChessGameContent& content,
    const BalanceConfig::ChallengeDef& challenge)
{
    ChallengeDto dto;
    dto.name = challenge.name;
    dto.description = challenge.description;
    dto.enemy_count = static_cast<int>(challenge.enemies.size());
    for (const auto& enemy : challenge.enemies)
    {
        const auto* role = content.role(enemy.roleId);
        assert(role);
        ChallengeEnemyDto enemyDto;
        enemyDto.role_id = enemy.roleId;
        enemyDto.name = role->Name;
        enemyDto.star = enemy.star;
        if (enemy.weaponId >= 0)
        {
            enemyDto.weapon = equipmentInfoDto(content, enemy.weaponId);
        }
        if (enemy.armorId >= 0)
        {
            enemyDto.armor = equipmentInfoDto(content, enemy.armorId);
        }
        dto.enemies.push_back(std::move(enemyDto));
    }
    for (const auto& reward : challenge.rewards)
    {
        dto.rewards.push_back({chessChallengeRewardDescription(content, reward)});
    }
    return dto;
}

std::string legalActionExampleJson(
    const ChessGameSession& session,
    const ChessLegalActionDescriptor& descriptor)
{
    ChessAction action;
    action.type = descriptor.type;
    switch (descriptor.type)
    {
    case ChessActionType::BuyShopSlot:
        action.shopSlot = descriptor.candidateIds.front();
        break;
    case ChessActionType::SetShopLocked:
    case ChessActionType::SetPositionSwapEnabled:
        action.value = true;
        break;
    case ChessActionType::SellChess:
        action.chessInstanceId = descriptor.candidateIds.front();
        break;
    case ChessActionType::SetDeployment:
        action.chessInstanceIds.assign(
            descriptor.candidateIds.begin(),
            descriptor.candidateIds.begin() + std::min<std::size_t>(
                descriptor.candidateIds.size(),
                static_cast<std::size_t>(descriptor.maximumSelection)));
        break;
    case ChessActionType::AddBan:
        action.roleId = descriptor.candidateIds.front();
        break;
    case ChessActionType::Equip:
    {
        action.equipmentInstanceId = descriptor.candidateIds.front();
        const auto& equipment = session.state().equipmentInventory.at(action.equipmentInstanceId);
        const auto alternateTarget = std::ranges::find_if(
            session.state().roster,
            [&](const auto& entry) {
                return entry.first != equipment.assignedChessInstanceId;
            });
        action.targetChessInstanceId = alternateTarget != session.state().roster.end()
            ? alternateTarget->first
            : session.state().roster.begin()->first;
        break;
    }
    case ChessActionType::BuyLegendaryEquipment:
        action.itemId = descriptor.candidateIds.front();
        break;
    case ChessActionType::ChooseMap:
        action.mapId = descriptor.candidateIds.front();
        break;
    case ChessActionType::SwapPositions:
        if (descriptor.candidateIds.size() >= 2)
        {
            action.chessInstanceId = descriptor.candidateIds[0];
            action.targetChessInstanceId = descriptor.candidateIds[1];
        }
        else
        {
            return chessActionPayloadSchema(descriptor.type);
        }
        break;
    case ChessActionType::ChooseReward:
        action.rewardId = descriptor.candidateStableIds.front();
        break;
    case ChessActionType::StartChallenge:
        action.challengeName = descriptor.candidateStableIds.front();
        break;
    default:
        break;
    }
    return serializeChessActionJson(action);
}

LegalActionDto legalActionDto(
    const ChessGameSession& session,
    const ChessLegalActionDescriptor& descriptor)
{
    LegalActionDto dto;
    dto.type = chessActionTypeId(descriptor.type);
    dto.description = actionDescription(descriptor.type);
    dto.action_schema = glz::raw_json(chessActionPayloadSchema(descriptor.type));
    dto.example = glz::raw_json(legalActionExampleJson(session, descriptor));
    dto.minimum_selection = descriptor.minimumSelection;
    dto.maximum_selection = descriptor.maximumSelection;
    const auto& content = session.content();
    const auto& state = session.state();
    const auto economicPreview = [&]() -> std::optional<LegalActionDto::EconomicPreview> {
        LegalActionDto::EconomicPreview preview;
        preview.current_gold = state.money;
        switch (descriptor.type)
        {
        case ChessActionType::RefreshShop:
            preview.gold_cost = state.freeShopRefreshAvailable ? 0 : content.balance().refreshCost;
            preview.projected_gold_after = state.money - preview.gold_cost;
            preview.affected_option_count = static_cast<int>(state.shop.size());
            preview.consumes_free_shop_refresh = state.freeShopRefreshAvailable;
            preview.result = std::format(
                "重新生成全部 {} 個商店欄位並解除商店鎖定",
                state.shop.size());
            return preview;
        case ChessActionType::BuyExp:
        {
            auto projected = state;
            ChessManagementRules::gainExperience(
                projected,
                content,
                content.balance().buyExpAmount);
            preview.gold_cost = content.balance().buyExpCost;
            preview.projected_gold_after = state.money - preview.gold_cost;
            preview.experience_gained = content.balance().buyExpAmount;
            preview.projected_experience_after = projected.experience;
            preview.projected_level_after = projected.level;
            preview.result = std::format(
                "獲得 {} 經驗；購買後為等級 {}、累積經驗 {}",
                content.balance().buyExpAmount,
                projected.level,
                projected.experience);
            return preview;
        }
        case ChessActionType::RerollEnemySeed:
            preview.gold_cost = content.balance().enemyRerollCost;
            preview.projected_gold_after = state.money - preview.gold_cost;
            preview.result = "重新生成下一戰的敵方陣容、裝備、地圖與戰鬥種子";
            return preview;
        case ChessActionType::RerollReward:
            assert(!state.pendingRewards.empty());
            preview.gold_cost = state.pendingRewards.front().rerollCost;
            preview.projected_gold_after = state.money - preview.gold_cost;
            preview.affected_option_count = static_cast<int>(state.pendingRewards.front().options.size());
            preview.result = std::format(
                "重新抽取目前獎勵的 {} 個選項；每份獎勵最多重抽一次",
                state.pendingRewards.front().options.size());
            return preview;
        case ChessActionType::BuyLegendaryEquipment:
            preview.gold_cost = content.balance().legendaryShop.price;
            preview.projected_gold_after = state.money - preview.gold_cost;
            preview.affected_option_count = 1;
            preview.result = "取得所選的 4 階裝備實例並放入裝備庫";
            return preview;
        default:
            return std::nullopt;
        }
    }();
    dto.economic_preview = economicPreview;
    if (descriptor.type == ChessActionType::Equip)
    {
        const auto& equipment = state.equipmentInventory.at(descriptor.candidateIds.front());
        if (equipment.assignedChessInstanceId >= 0)
        {
            dto.example_note = state.roster.size() > 1
                ? "範例會把目前已裝備的項目移給另一名棋子"
                : "目前沒有未分配裝備或其他棋子；範例合法，但不會改變持有者";
        }
    }
    std::vector<LegalActionDto::Candidate> primaryCandidates;
    for (const int id : descriptor.candidateIds)
    {
        LegalActionDto::Candidate candidate;
        candidate.id = id;
        if (descriptor.type == ChessActionType::BuyShopSlot)
        {
            const auto& slot = state.shop.at(id);
            const auto* role = content.role(slot.roleId);
            assert(role);
            const int cost = ChessManagementRules::pieceValue(content, slot.roleId, 1);
            candidate.label = role->Name;
            candidate.description = std::format(
                "商店欄位 {}；{}費角色；價格 {} 金幣",
                id,
                role->Cost,
                cost);
            candidate.gold_cost = cost;
            candidate.projected_gold_after = state.money - cost;
        }
        else if (descriptor.type == ChessActionType::SellChess
            || descriptor.type == ChessActionType::SetDeployment)
        {
            const auto& piece = state.roster.at(id);
            const auto* role = content.role(piece.roleId);
            assert(role);
            candidate.label = role->Name;
            candidate.description = std::format("棋子實例 {}；{}★{}", id, piece.star, piece.deployed ? "；目前出戰" : "");
            if (descriptor.type == ChessActionType::SellChess)
            {
                const int gain = ChessManagementRules::pieceValue(content, piece.roleId, piece.star);
                candidate.gold_gain = gain;
                candidate.projected_gold_after = state.money + gain;
                candidate.description += std::format("；出售獲得 {} 金幣", gain);
            }
        }
        else if (descriptor.type == ChessActionType::AddBan)
        {
            const auto* role = content.role(id);
            assert(role);
            candidate.label = role->Name;
            candidate.description = std::format("{}費棋子", role->Cost);
        }
        else if (descriptor.type == ChessActionType::Equip)
        {
            const auto& equipment = state.equipmentInventory.at(id);
            const auto info = equipmentInfoDto(content, equipment.itemId);
            candidate.label = info.name;
            candidate.assigned_chess_instance_id = equipment.assignedChessInstanceId;
            if (equipment.assignedChessInstanceId < 0)
            {
                candidate.label += "（未分配）";
                candidate.description = std::format("裝備實例 {}；{}階{}；未分配", id, info.tier, info.type);
            }
            else
            {
                const auto& owner = state.roster.at(equipment.assignedChessInstanceId);
                const auto* ownerRole = content.role(owner.roleId);
                assert(ownerRole);
                candidate.label += std::format("（{}已裝備）", ownerRole->Name);
                candidate.assigned_to = std::format(
                    "{}（棋子實例 {}）",
                    ownerRole->Name,
                    equipment.assignedChessInstanceId);
                candidate.description = std::format(
                    "裝備實例 {}；{}階{}；目前由{}（棋子實例 {}）裝備",
                    id,
                    info.tier,
                    info.type,
                    ownerRole->Name,
                    equipment.assignedChessInstanceId);
            }
        }
        else if (descriptor.type == ChessActionType::BuyLegendaryEquipment)
        {
            const auto info = equipmentInfoDto(content, id);
            const int cost = content.balance().legendaryShop.price;
            candidate.label = info.name;
            candidate.description = std::format("{}階{}；價格 {} 金幣", info.tier, info.type, cost);
            candidate.gold_cost = cost;
            candidate.projected_gold_after = state.money - cost;
        }
        else if (descriptor.type == ChessActionType::ChooseMap)
        {
            candidate.label = mapName(content, id);
        }
        else if (descriptor.type == ChessActionType::SwapPositions)
        {
            assert(state.preparedBattle);
            const auto unit = std::ranges::find(state.preparedBattle->units, id, &PreparedChessBattleUnit::unitId);
            assert(unit != state.preparedBattle->units.end());
            const auto* role = content.role(unit->roleId);
            assert(role);
            candidate.label = role->Name;
            candidate.description = std::format("戰鬥單位 {}；位置 ({},{})", id, unit->x, unit->y);
        }
        candidate.value = std::to_string(id);
        primaryCandidates.push_back(std::move(candidate));
    }
    for (const auto& value : descriptor.candidateStableIds)
    {
        LegalActionDto::Candidate candidate;
        candidate.value = value;
        candidate.label = value;
        if (descriptor.type == ChessActionType::ChooseReward)
        {
            assert(!state.pendingRewards.empty());
            const auto option = std::ranges::find(state.pendingRewards.front().options, value, &ChessRewardOption::id);
            assert(option != state.pendingRewards.front().options.end());
            const int starUpgradeRoleId = option->kind == ChessRewardKind::StarUpgrade
                ? state.roster.at(option->value).roleId
                : -1;
            const auto reward = rewardOptionDto(
                content,
                state.pendingRewards.front(),
                *option,
                starUpgradeRoleId);
            candidate.label = reward.label;
            candidate.description = reward.description;
            if (reward.equipment)
            {
                candidate.group = std::format(
                    "{}階{}",
                    reward.equipment->tier,
                    reward.equipment->type);
            }
        }
        else if (descriptor.type == ChessActionType::StartChallenge)
        {
            const auto challenge = std::ranges::find(content.balance().challenges, value, &BalanceConfig::ChallengeDef::name);
            assert(challenge != content.balance().challenges.end());
            candidate.description = challengeDescription(content, *challenge);
        }
        primaryCandidates.push_back(std::move(candidate));
    }
    const auto primaryField = [&]() -> std::string {
        switch (descriptor.type)
        {
        case ChessActionType::BuyShopSlot: return "slot";
        case ChessActionType::SellChess: return "chess_instance_id";
        case ChessActionType::SetDeployment: return "chess_instance_ids";
        case ChessActionType::AddBan: return "role_id";
        case ChessActionType::Equip: return "equipment_instance_id";
        case ChessActionType::BuyLegendaryEquipment: return "item_id";
        case ChessActionType::ChooseMap: return "map_id";
        case ChessActionType::SwapPositions: return "first_unit_id";
        case ChessActionType::ChooseReward: return "reward_id";
        case ChessActionType::StartChallenge: return "challenge_name";
        default: return {};
        }
    }();
    if (!primaryField.empty())
    {
        dto.candidates_by_field.push_back({
            primaryField,
            descriptor.type == ChessActionType::SetDeployment,
            primaryCandidates,
        });
    }
    if (descriptor.type == ChessActionType::Equip)
    {
        LegalActionDto::FieldCandidates targets;
        targets.field = "target_chess_instance_id";
        for (const auto& [instanceId, piece] : state.roster)
        {
            const auto* role = content.role(piece.roleId);
            assert(role);
            targets.candidates.push_back({
                instanceId,
                std::to_string(instanceId),
                role->Name,
                std::format("{}★{}", piece.star, piece.deployed ? "；目前出戰" : ""),
            });
        }
        dto.candidates_by_field.push_back(std::move(targets));
    }
    else if (descriptor.type == ChessActionType::SwapPositions)
    {
        dto.candidates_by_field.push_back({"second_unit_id", false, primaryCandidates});
    }
    return dto;
}

ShopOddsDto shopOddsDto(const ChessGameSession& session, int level)
{
    const auto& content = session.content();
    const auto& state = session.state();
    assert(level >= 0 && level < static_cast<int>(content.balance().shopWeights.size()));
    ShopOddsDto dto{};
    dto.level = level;
    for (int tier = 1; tier <= 5; ++tier)
    {
        const auto candidates = ChessManagementRules::shopCandidatesForTier(state, content, tier);
        if (!candidates.empty())
        {
            dto.total_effective_weight += content.balance().shopWeights[level][tier - 1];
        }
    }
    for (int tier = 1; tier <= 5; ++tier)
    {
        ShopTierOddsDto tierDto{};
        tierDto.tier = tier;
        tierDto.configured_weight = content.balance().shopWeights[level][tier - 1];
        const auto candidates = ChessManagementRules::shopCandidatesForTier(state, content, tier);
        tierDto.available_role_count = static_cast<int>(candidates.size());
        if (!candidates.empty() && dto.total_effective_weight > 0)
        {
            tierDto.probability = static_cast<double>(tierDto.configured_weight)
                / dto.total_effective_weight;
        }
        for (const int roleId : candidates)
        {
            const auto* role = content.role(roleId);
            assert(role);
            tierDto.available_roles.push_back({roleId, role->Name});
        }
        dto.tiers.push_back(std::move(tierDto));
    }
    dto.pool_note = "機率只在目前仍有候選角色的費用層級間重新正規化；候選已排除禁棋及本次刷新避免立即重複的角色";
    return dto;
}

ShopSlotInspectionDto shopSlotInspectionDto(const ChessGameSession& session, int slotIndex)
{
    const auto& state = session.state();
    const auto& content = session.content();
    assert(slotIndex >= 0 && slotIndex < static_cast<int>(state.shop.size()));
    ShopSlotInspectionDto dto{};
    dto.slot = slotIndex;
    const auto& slot = state.shop[slotIndex];
    if (slot.roleId < 0)
    {
        return dto;
    }
    const auto* role = content.role(slot.roleId);
    assert(role);
    dto.occupied = true;
    dto.role_id = role->ID;
    dto.name = role->Name;
    dto.cost = role->Cost;
    dto.shop_tier = slot.tier;
    for (int star = 1; star <= 3; ++star)
    {
        const int count = static_cast<int>(std::ranges::count_if(
            state.roster,
            [&](const auto& entry) {
                return entry.second.roleId == role->ID && entry.second.star == star;
            }));
        dto.copies_by_star.push_back({star, count});
        dto.owned_copies += count * (star == 1 ? 1 : star == 2 ? 3 : 9);
    }
    const int price = ChessManagementRules::pieceValue(content, role->ID, 1);
    dto.gold_cost = price;
    dto.projected_gold_after = state.money - price;
    ChessAction purchase;
    purchase.type = ChessActionType::BuyShopSlot;
    purchase.shopSlot = slotIndex;
    const auto validation = ChessManagementRules::validate(state, content, purchase);
    dto.purchase_legal = validation == ChessRuleErrorCode::None;
    dto.purchase_error = ruleErrorId(validation);

    auto projected = state;
    std::vector<ChessSemanticEvent> projectedEvents;
    if (ChessManagementRules::canGrantPiece(projected, content, role->ID))
    {
        ChessManagementRules::grantPiece(projected, content, role->ID, projectedEvents);
        int resultStar = 1;
        for (const auto& event : projectedEvents)
        {
            if (event.type == ChessSemanticEventType::ChessMerged
                && event.secondaryId == role->ID)
            {
                resultStar = std::max(resultStar, event.value);
            }
        }
        dto.purchase_result = resultStar > 1
            ? std::format("merge_to_{}_star", resultStar)
            : "add_1_star";
    }
    else
    {
        dto.purchase_result = "bench_full";
    }
    for (const auto& combo : content.combos())
    {
        if (!std::ranges::contains(combo.memberRoleIds, role->ID))
        {
            continue;
        }
        const auto current = evaluateChessComboProgress(state, content, combo);
        const auto afterPurchase = evaluateChessComboProgress(projected, content, combo);
        dto.synergies.push_back({
            combo.name,
            current.effectiveCount,
            afterPurchase.effectiveCount,
        });
    }
    const auto odds = shopOddsDto(session, state.level);
    dto.level_tier_probability = odds.tiers.at(slot.tier - 1).probability;
    return dto;
}

ShopInspectionDto shopInspectionDto(const ChessGameSession& session)
{
    ShopInspectionDto dto{};
    dto.money = session.state().money;
    dto.level = session.state().level;
    for (int slot = 0; slot < static_cast<int>(session.state().shop.size()); ++slot)
    {
        dto.slots.push_back(shopSlotInspectionDto(session, slot));
    }
    dto.odds = shopOddsDto(session, session.state().level);
    return dto;
}

ChessInstanceInspectionDto chessInstanceInspectionDto(
    const ChessGameSession& session,
    const ChessSessionPiece& piece)
{
    const auto observation = session.observe();
    ChessInstanceInspectionDto dto{};
    dto.chess = pieceDto(session.content(), observation, piece, true);
    for (const auto& [instanceId, owned] : session.state().roster)
    {
        if (owned.roleId != piece.roleId)
        {
            continue;
        }
        dto.one_star_equivalent_copies += owned.star == 1 ? 1 : owned.star == 2 ? 3 : 9;
        if (owned.star == piece.star)
        {
            ++dto.same_star_copies;
        }
    }
    dto.copies_required_for_next_star = piece.star >= 3
        ? 0
        : std::max(0, 3 - dto.same_star_copies);
    for (const int equipmentInstanceId : {piece.weaponInstanceId, piece.armorInstanceId})
    {
        if (equipmentInstanceId < 0)
        {
            continue;
        }
        const auto& equipment = session.state().equipmentInventory.at(equipmentInstanceId);
        dto.equipment.push_back({
            equipment.instanceId,
            equipmentInfoDto(session.content(), equipment.itemId),
            equipment.assignedChessInstanceId,
        });
    }
    for (const auto& observedCombo : observation.combos)
    {
        const auto contribution = std::ranges::find_if(
            observedCombo.contributions,
            [&](const auto& item) {
                return std::ranges::contains(item.unitIds, piece.instanceId);
            });
        if (contribution == observedCombo.contributions.end())
        {
            continue;
        }
        const auto definition = std::ranges::find(
            session.content().combos(),
            observedCombo.comboId,
            &ComboDef::id);
        assert(definition != session.content().combos().end());
        dto.synergy_contributions.push_back(comboDto(
            session.content(),
            *definition,
            observedCombo.physicalCount,
            observedCombo.effectiveCount,
            observedCombo.activeThresholdIndex,
            true,
            &observedCombo.contributions));
    }
    return dto;
}

BanInspectionDto banInspectionDto(const ChessGameSession& session)
{
    BanInspectionDto dto{};
    const auto& state = session.state();
    const auto& content = session.content();
    dto.current_ban_count = static_cast<int>(state.bannedRoleIds.size());
    dto.maximum_ban_count = ChessManagementRules::maximumBanCount(state, content);
    dto.remaining_ban_capacity = std::max(0, dto.maximum_ban_count - dto.current_ban_count);
    const auto groupedRoles = [&](const std::vector<int>& roleIds) {
        std::map<int, std::vector<NamedIdDto>> groups;
        for (const int roleId : roleIds)
        {
            const auto* role = content.role(roleId);
            assert(role);
            groups[role->Cost].push_back({roleId, role->Name});
        }
        std::vector<BanInspectionDto::TierGroup> result;
        for (auto& [cost, roles] : groups)
        {
            result.push_back({cost, std::move(roles)});
        }
        return result;
    };
    dto.current_bans_by_cost = groupedRoles(std::vector<int>(
        state.bannedRoleIds.begin(),
        state.bannedRoleIds.end()));
    std::vector<int> eligible;
    const auto legalActions = session.legalActions();
    const auto legalBan = std::ranges::find(
        legalActions,
        ChessActionType::AddBan,
        &ChessLegalActionDescriptor::type);
    if (legalBan != legalActions.end())
    {
        eligible = legalBan->candidateIds;
    }
    dto.eligible_bans_by_cost = groupedRoles(eligible);
    dto.effect_timing = "禁棋只影響之後生成或刷新的商店；目前商店既有棋子仍可購買";
    dto.forced_phase_note = "強制禁棋只強制先解決獨佔決策階段；可選擇禁棋或略過，略過不消耗禁棋容量";
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
    if (event.type == ChessSemanticEventType::GoldAwarded)
    {
        dto.base_gold = event.primaryId;
        dto.interest_gold = event.secondaryId;
        dto.other_gold = event.value - event.primaryId - event.secondaryId;
        dto.total_gold = event.value;
        if (event.stableId.starts_with("combo:"))
        {
            int comboId{};
            const auto idText = std::string_view(event.stableId).substr(6);
            const auto parsed = std::from_chars(
                idText.data(),
                idText.data() + idText.size(),
                comboId);
            if (parsed.ec == std::errc{} && parsed.ptr == idText.data() + idText.size())
            {
                const auto combo = std::ranges::find(content.combos(), comboId, &ComboDef::id);
                if (combo != content.combos().end()) dto.other_gold_source = combo->name;
            }
        }
        return dto;
    }
    if (event.type == ChessSemanticEventType::ChessMerged && event.merge)
    {
        const auto* role = content.role(event.secondaryId);
        assert(role);
        dto.role_id = event.secondaryId;
        dto.role_name = role->Name;
        dto.consumed_instance_ids = event.merge->consumedInstanceIds;
        dto.new_instance_id = event.primaryId;
        dto.result_star = event.value;
        dto.inherited_fights_won = event.merge->inheritedFightsWon;
        dto.deployed = event.merge->deployed;
        dto.transferred_equipment_instance_ids = event.merge->transferredEquipmentInstanceIds;
        dto.recursive_merge_followed = event.merge->recursiveMergeFollowed;
        return dto;
    }
    if (event.type == ChessSemanticEventType::EnemyPlanRerolled && event.enemyPlanReroll)
    {
        dto.cost = event.enemyPlanReroll->cost;
        dto.previous_enemy_plan_key = std::format(
            "0x{:016x}",
            event.enemyPlanReroll->previousEnemyPlanKey);
        dto.new_enemy_plan_key = std::format(
            "0x{:016x}",
            event.enemyPlanReroll->newEnemyPlanKey);
        dto.effect = "下一次準備戰鬥時重新生成敵方陣容、裝備、地圖與戰鬥種子";
        return dto;
    }
    dto.primary_id = event.primaryId;
    dto.secondary_id = event.secondaryId;
    dto.value = event.value;
    dto.stable_id = event.stableId;
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
            const auto* role = session.content().role(event.secondaryId);
            assert(role);
            dto.changes.merged_units.push_back({
                event.secondaryId,
                role->Name,
                event.primaryId,
                event.value,
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
            battleOutcomeDescription(battle.summary.outcome),
            battle.summary.endFrame,
        };
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

std::string battleEventText(const BattleReportEvent& event)
{
    std::string result = event.skillName;
    for (const auto& segment : event.segments)
    {
        result += segment.text;
    }
    return result.empty() ? "未標記效果" : result;
}

bool containsText(std::string_view text, std::string_view needle)
{
    return text.find(needle) != std::string_view::npos;
}

int eventDurationFrames(const BattleReportEvent& event)
{
    for (const auto& segment : event.segments)
    {
        if (segment.tone != Battle::BattleLogTextTone::DurationValue)
        {
            continue;
        }
        int value{};
        const auto parsed = std::from_chars(
            segment.text.data(),
            segment.text.data() + segment.text.size(),
            value);
        if (parsed.ec == std::errc{})
        {
            return value;
        }
    }
    return 0;
}

struct UnitCombatAggregate
{
    int healingDone{};
    int magicPointsRestored{};
    int magicPointsDrained{};
    int magicPointsDrainEvents{};
    int blocks{};
    int dodges{};
    int poisonPayloadEvents{};
    int poisonApplicationEvents{};
    int poisonTicks{};
    int poisonDamage{};
    int bleedApplications{};
    int hitstunApplications{};
    int hitstunDurationFrames{};
    int stunApplications{};
    int stunDurationFrames{};
    int knockbackApplications{};
    int magicBlockApplications{};
    int magicBlockDurationFrames{};
    int cooldownManipulations{};
    int cooldownManipulationFrames{};
    int invulnerabilityTriggers{};
    int deathPreventionTriggers{};
    BattleUnitStatsDto::DamageBreakdown damage;
    std::map<std::string, int> nonSkillDamageSources;
    std::map<std::string, int> nonSkillDamageSourceCounts;
};

void addDamageCategory(
    UnitCombatAggregate& aggregate,
    const BattleReportEvent& event,
    const ChessGameContent& content)
{
    const auto text = battleEventText(event);
    if (containsText(text, "中毒"))
    {
        ++aggregate.poisonTicks;
        aggregate.poisonDamage += event.value;
    }
    if (event.skillId >= 0)
    {
        aggregate.damage.skill += event.value;
        return;
    }
    aggregate.nonSkillDamageSources[text] += event.value;
    ++aggregate.nonSkillDamageSourceCounts[text];
    if (containsText(text, "中毒") || containsText(text, "流血"))
    {
        aggregate.damage.status += event.value;
        return;
    }
    if (containsText(text, "殉爆")
        || std::ranges::any_of(content.combos(), [&](const auto& combo) {
            return containsText(text, combo.name);
        }))
    {
        aggregate.damage.combo += event.value;
        return;
    }
    if (std::ranges::any_of(content.equipment(), [&](const auto& equipment) {
            const auto* item = content.item(equipment.itemId);
            return item && containsText(text, item->name);
        }))
    {
        aggregate.damage.equipment += event.value;
        return;
    }
    if (text == "未標記效果" || containsText(text, "普通攻擊"))
    {
        aggregate.damage.basic_attack += event.value;
        return;
    }
    aggregate.damage.other += event.value;
}

std::string battleEffectType(const BattleReportEvent& event)
{
    const auto label = battleEventText(event);
    if (event.category == Battle::BattleLogCategory::Cast) return "ability_cast";
    if (event.category == Battle::BattleLogCategory::ProjectileCancel) return "projectile_cancelled";
    if (event.type == BattleReportEventType::Heal
        && event.resourceId == Battle::BattleResourceSemanticId::MagicPoints)
    {
        return "magic_points_restored";
    }
    switch (event.statusId)
    {
    case Battle::BattleStatusSemanticId::Hitstun: return "hitstun_applied";
    case Battle::BattleStatusSemanticId::Stun: return "stun_applied";
    case Battle::BattleStatusSemanticId::Poison: return "poison_applied";
    case Battle::BattleStatusSemanticId::Bleed: return "bleed_applied";
    case Battle::BattleStatusSemanticId::DamageReduceDebuff: return "damage_reduction_applied";
    case Battle::BattleStatusSemanticId::MpBlocked: return "magic_block_applied";
    case Battle::BattleStatusSemanticId::BlockedByInvincible: return "blocked_by_invulnerability";
    case Battle::BattleStatusSemanticId::BlockedByFirstHit: return "blocked_by_first_hit";
    case Battle::BattleStatusSemanticId::DeathPrevented: return "death_prevented";
    case Battle::BattleStatusSemanticId::ExecuteTriggered: return "execute_triggered";
    case Battle::BattleStatusSemanticId::Knockback: return "knockback_applied";
    case Battle::BattleStatusSemanticId::EnemyTopDebuff: return "enemy_top_debuff_changed";
    case Battle::BattleStatusSemanticId::MagicPointsDrained: return "magic_points_drained";
    case Battle::BattleStatusSemanticId::PoisonPayload: return "poison_payload";
    case Battle::BattleStatusSemanticId::None: break;
    }
    if (event.resourceId == Battle::BattleResourceSemanticId::Cooldown) return "cooldown_changed";
    if (containsText(label, "閃避")) return "dodge";
    if (containsText(label, "格擋")) return "block";
    if (containsText(label, "無敵")) return "invulnerability";
    return "status_effect";
}

BattleEffectActivationDto battleEffectActivationDto(const BattleReportEvent& event)
{
    BattleEffectActivationDto dto;
    dto.type = battleEffectType(event);
    dto.description = battleEventText(event);
    dto.frame = event.frame;
    if (event.sourceId >= 0)
    {
        dto.source_unit_id = event.sourceId;
    }
    if (!event.sourceName.empty()) dto.source_name = event.sourceName;
    if (event.sourceTeam >= 0) dto.source_team = teamName(event.sourceTeam);
    if (!event.sourceKind.empty()) dto.source_kind = event.sourceKind;
    if (event.targetId >= 0)
    {
        dto.target_unit_id = event.targetId;
        dto.target_name = event.targetName;
        dto.target_team = teamName(event.targetTeam);
    }
    if (event.category == Battle::BattleLogCategory::Cast)
    {
        if (event.skillId >= 0) dto.ability_id = event.skillId;
        if (!event.skillName.empty()) dto.ability_name = event.skillName;
    }
    if (event.statusId == Battle::BattleStatusSemanticId::PoisonPayload)
    {
        dto.poison_percent = event.value;
        dto.scheduled_ticks = event.secondaryValue;
    }
    else if (event.statusId == Battle::BattleStatusSemanticId::Poison)
    {
        dto.poison_percent = event.value;
    }

    if (event.category == Battle::BattleLogCategory::ProjectileCancel)
    {
        dto.source_projectile_id = event.effectId;
        dto.opposing_projectile_id = event.secondaryEffectId;
        dto.source_value_before = event.value;
        dto.opposing_value_before = event.secondaryValue;
        dto.cancelled_potential_damage = std::min(event.value, event.secondaryValue);
        dto.source_value_after = std::max(0, event.value - event.secondaryValue);
        dto.opposing_value_after = std::max(0, event.secondaryValue - event.value);
        return dto;
    }
    if (event.statusId == Battle::BattleStatusSemanticId::EnemyTopDebuff)
    {
        dto.previous_value = event.previousValue;
        dto.new_value = event.newValue;
        dto.delta = event.value;
        return dto;
    }
    if (event.value != 0)
    {
        dto.value = event.value;
    }
    const int durationFrames = event.statusId == Battle::BattleStatusSemanticId::Knockback
        ? event.secondaryValue
        : eventDurationFrames(event);
    if (durationFrames > 0)
    {
        dto.duration_frames = durationFrames;
    }
    return dto;
}

void addCombatEffect(UnitCombatAggregate& aggregate, const BattleReportEvent& event)
{
    const auto label = battleEventText(event);

    if (event.statusId == Battle::BattleStatusSemanticId::Poison) ++aggregate.poisonApplicationEvents;
    if (event.statusId == Battle::BattleStatusSemanticId::PoisonPayload) ++aggregate.poisonPayloadEvents;
    if (event.statusId == Battle::BattleStatusSemanticId::MagicPointsDrained)
    {
        aggregate.magicPointsDrained += event.value;
        ++aggregate.magicPointsDrainEvents;
    }
    if (event.statusId == Battle::BattleStatusSemanticId::Bleed) ++aggregate.bleedApplications;
    if (event.statusId == Battle::BattleStatusSemanticId::Hitstun)
    {
        ++aggregate.hitstunApplications;
        aggregate.hitstunDurationFrames += eventDurationFrames(event);
    }
    if (event.statusId == Battle::BattleStatusSemanticId::Stun)
    {
        ++aggregate.stunApplications;
        aggregate.stunDurationFrames += eventDurationFrames(event);
    }
    if (event.statusId == Battle::BattleStatusSemanticId::Knockback)
    {
        ++aggregate.knockbackApplications;
    }
    if (event.statusId == Battle::BattleStatusSemanticId::MpBlocked)
    {
        ++aggregate.magicBlockApplications;
        aggregate.magicBlockDurationFrames += eventDurationFrames(event);
    }
    if (event.resourceId == Battle::BattleResourceSemanticId::Cooldown)
    {
        ++aggregate.cooldownManipulations;
        aggregate.cooldownManipulationFrames += event.value;
    }
    if (event.statusId == Battle::BattleStatusSemanticId::DeathPrevented
        || containsText(label, "死亡庇護")
        || containsText(label, "免死"))
    {
        ++aggregate.deathPreventionTriggers;
    }
    if (event.statusId == Battle::BattleStatusSemanticId::BlockedByFirstHit
        || containsText(label, "格擋"))
    {
        ++aggregate.blocks;
    }
    if (containsText(label, "閃避")) ++aggregate.dodges;
    if (event.statusId == Battle::BattleStatusSemanticId::BlockedByInvincible
        || event.resourceId == Battle::BattleResourceSemanticId::Invincibility
        || containsText(label, "無敵"))
    {
        ++aggregate.invulnerabilityTriggers;
    }
}

BattleResultDto battleResultDto(
    const ChessGameContent& content,
    const PreparedChessBattle& prepared,
    const HeadlessBattleResult& battle,
    ObservationDetail detail)
{
    BattleResultDto dto;
    const bool full = detail == ObservationDetail::Full;
    dto.detail = full ? "full" : "compact";
    if (full)
    {
        dto.initial_board = preparedBattleDto(content, prepared);
        dto.effect_activations.emplace();
    }
    auto assignMetric = [full](std::optional<int>& field, int value)
    {
        if (full || value != 0)
        {
            field = value;
        }
    };
    dto.outcome = battleOutcomeId(battle.summary.outcome);
    dto.outcome_description = battleOutcomeDescription(battle.summary.outcome);
    dto.end_frame = battle.summary.endFrame;
    for (const auto& survivor : battle.summary.survivors)
    {
        const auto* role = content.role(survivor.roleId);
        assert(role);
        dto.survivors.push_back({
            survivor.unitId,
            survivor.chessInstanceId,
            survivor.roleId,
            role->Name,
            teamName(survivor.team),
            survivor.hp,
            survivor.mp,
            survivor.summoned,
        });
    }
    int allyDamage{};
    int enemyDamage{};
    const PreparedChessBattleUnit* topUnit = nullptr;
    int topDamage = -1;
    std::map<int, UnitCombatAggregate> combatByUnit;
    for (const auto& event : battle.report.events())
    {
        if (event.type == BattleReportEventType::Damage && event.sourceId >= 0)
        {
            addDamageCategory(combatByUnit[event.sourceId], event, content);
        }
        else if (event.type == BattleReportEventType::Heal && event.sourceId >= 0)
        {
            if (full && event.resourceId == Battle::BattleResourceSemanticId::MagicPoints)
            {
                dto.effect_activations->push_back(battleEffectActivationDto(event));
            }
            auto& aggregate = combatByUnit[event.sourceId];
            if (event.resourceId == Battle::BattleResourceSemanticId::MagicPoints)
            {
                aggregate.magicPointsRestored += event.value;
            }
            else if (event.resourceId == Battle::BattleResourceSemanticId::HitPoints)
            {
                aggregate.healingDone += event.value;
            }
        }
        else if (event.type == BattleReportEventType::Status)
        {
            if (full)
            {
                dto.effect_activations->push_back(battleEffectActivationDto(event));
            }
            const int unitId = event.sourceId >= 0 ? event.sourceId : event.targetId;
            if (unitId >= 0)
            {
                addCombatEffect(combatByUnit[unitId], event);
            }
        }
    }
    for (const auto& unit : prepared.units)
    {
        const auto* role = content.role(unit.roleId);
        assert(role);
        BattleUnitStatsDto stats;
        stats.unit_id = unit.unitId;
        stats.role_id = unit.roleId;
        stats.name = role->Name;
        stats.team = teamName(unit.team);
        stats.star = unit.star;
        const auto baselineStats = preparedUnitStats(content, *role, unit);
        const auto initialized = std::ranges::find(
            battle.initialization.roleDeltas,
            unit.unitId,
            &Battle::BattleInitializationRoleDelta::unitId);
        assert(initialized != battle.initialization.roleDeltas.end());
        if (full)
        {
            stats.initial_combat_stats = RoleStatsDto{
                initialized->vitals.maxHp,
                initialized->vitals.maxMp,
                initialized->stats.attack,
                initialized->stats.defence,
                initialized->stats.speed,
                initialized->fist,
                initialized->sword,
                initialized->knife,
                initialized->unusual,
                initialized->hiddenWeapon,
            };
            const auto& combatStats = *stats.initial_combat_stats;
            stats.initial_stat_delta_from_special_effects = RoleStatsDto{
                combatStats.max_hp - baselineStats.max_hp,
                combatStats.max_mp - baselineStats.max_mp,
                combatStats.attack - baselineStats.attack,
                combatStats.defence - baselineStats.defence,
                combatStats.speed - baselineStats.speed,
                combatStats.fist - baselineStats.fist,
                combatStats.sword - baselineStats.sword,
                combatStats.knife - baselineStats.knife,
                combatStats.unusual - baselineStats.unusual,
                combatStats.hidden_weapon - baselineStats.hidden_weapon,
            };
        }
        assignMetric(stats.enemy_attack_debuff, 0);
        assignMetric(stats.enemy_defence_debuff, 0);
        if (const auto debuff = std::ranges::find(
                battle.initialization.enemyTopDebuffs,
                unit.unitId,
                &Battle::BattleInitializationEnemyTopDebuffDelta::unitId);
            debuff != battle.initialization.enemyTopDebuffs.end())
        {
            assignMetric(stats.enemy_attack_debuff, debuff->attackDelta);
            assignMetric(stats.enemy_defence_debuff, debuff->defenceDelta);
        }
        if (const auto found = battle.report.stats().find(unit.unitId); found != battle.report.stats().end())
        {
            stats.damage_dealt = found->second.damageDealt;
            stats.damage_taken = found->second.damageTaken;
            stats.kills = found->second.kills;
            for (const auto& [skillId, damage] : found->second.damagePerSkillId)
            {
                const auto* magic = content.magic(skillId);
                stats.skill_damage.push_back({skillId, magic ? magic->Name : "一般攻擊或效果", damage});
            }
        }
        assignMetric(
            stats.projectile_potential_damage_cancelled,
            battle.report.projectilePotentialDamageCancelledForUnit(unit.unitId));
        assignMetric(
            stats.projectile_cancellations,
            battle.report.projectileCancellationCountForUnit(unit.unitId));
        const UnitCombatAggregate emptyAggregate;
        const auto aggregateIt = combatByUnit.find(unit.unitId);
        const auto& aggregate = aggregateIt != combatByUnit.end()
            ? aggregateIt->second
            : emptyAggregate;
        assignMetric(stats.healing_done, aggregate.healingDone);
        assignMetric(stats.magic_points_restored, aggregate.magicPointsRestored);
        assignMetric(stats.magic_points_drained, aggregate.magicPointsDrained);
        assignMetric(stats.magic_points_drain_events, aggregate.magicPointsDrainEvents);
        assignMetric(stats.blocks, aggregate.blocks);
        assignMetric(stats.dodges, aggregate.dodges);
        assignMetric(stats.poison_payload_events, aggregate.poisonPayloadEvents);
        assignMetric(stats.poison_application_events, aggregate.poisonApplicationEvents);
        assignMetric(stats.poison_ticks, aggregate.poisonTicks);
        assignMetric(stats.poison_damage, aggregate.poisonDamage);
        assignMetric(stats.bleed_applications, aggregate.bleedApplications);
        assignMetric(stats.hitstun_applications, aggregate.hitstunApplications);
        assignMetric(stats.hitstun_duration_frames, aggregate.hitstunDurationFrames);
        assignMetric(stats.stun_applications, aggregate.stunApplications);
        assignMetric(stats.stun_duration_frames, aggregate.stunDurationFrames);
        assignMetric(stats.knockback_applications, aggregate.knockbackApplications);
        assignMetric(stats.magic_block_applications, aggregate.magicBlockApplications);
        assignMetric(stats.magic_block_duration_frames, aggregate.magicBlockDurationFrames);
        assignMetric(stats.cooldown_manipulations, aggregate.cooldownManipulations);
        assignMetric(stats.cooldown_manipulation_frames, aggregate.cooldownManipulationFrames);
        assignMetric(stats.invulnerability_triggers, aggregate.invulnerabilityTriggers);
        assignMetric(stats.death_prevention_triggers, aggregate.deathPreventionTriggers);
        stats.damage_breakdown = aggregate.damage;
        for (const auto& [source, damage] : aggregate.nonSkillDamageSources)
        {
            stats.non_skill_damage_sources.push_back({-1, source, damage});
        }
        auto metricValue = [](const std::optional<int>& metric) { return metric.value_or(0); };
        const int enemyAttackDebuff = metricValue(stats.enemy_attack_debuff);
        const int enemyDefenceDebuff = metricValue(stats.enemy_defence_debuff);
        const int projectileCancelled = metricValue(stats.projectile_potential_damage_cancelled);
        const int projectileCancellations = metricValue(stats.projectile_cancellations);
        const int magicPointsDrained = metricValue(stats.magic_points_drained);
        const int magicPointsDrainEvents = metricValue(stats.magic_points_drain_events);
        const int magicPointsRestored = metricValue(stats.magic_points_restored);
        const int poisonPayloadEvents = metricValue(stats.poison_payload_events);
        const int poisonApplicationEvents = metricValue(stats.poison_application_events);
        const int poisonTicks = metricValue(stats.poison_ticks);
        const int poisonDamage = metricValue(stats.poison_damage);
        const int bleedApplications = metricValue(stats.bleed_applications);
        const int stunApplications = metricValue(stats.stun_applications);
        const int stunDurationFrames = metricValue(stats.stun_duration_frames);
        const int deathPreventionTriggers = metricValue(stats.death_prevention_triggers);
        auto appendSourceImportantEffect = [&](
                                               std::string type,
                                               int count,
                                               int value,
                                               std::string description)
        {
            if (count <= 0)
            {
                return;
            }
            BattleImportantEffectDto effect;
            effect.type = std::move(type);
            effect.source_unit_id = unit.unitId;
            effect.source_name = role->Name;
            effect.source_team = teamName(unit.team);
            effect.count = count;
            if (value != 0)
            {
                effect.value = value;
            }
            effect.description = std::move(description);
            dto.important_effects.push_back(std::move(effect));
        };
        if (enemyAttackDebuff != 0 || enemyDefenceDebuff != 0)
        {
            BattleImportantEffectDto effect;
            effect.type = "opening_enemy_debuff";
            effect.source_team = teamName(1 - unit.team);
            effect.source_kind = "combo";
            effect.source_name = "陰險";
            effect.target_unit_id = unit.unitId;
            effect.target_name = role->Name;
            const auto targetTeam = teamName(unit.team);
            effect.target_team = targetTeam;
            effect.attack_delta = enemyAttackDebuff;
            effect.defence_delta = enemyDefenceDebuff;
            effect.description = std::format(
                "陰險使[{}] {}開戰攻擊{:+}、防禦{:+}",
                targetTeam,
                role->Name,
                enemyAttackDebuff,
                enemyDefenceDebuff);
            dto.important_effects.push_back(std::move(effect));
        }
        appendSourceImportantEffect(
            "projectile_cancellation",
            projectileCancellations,
            projectileCancelled,
            std::format(
                "[{}] {}抵消 {} 點潛在彈道傷害（{} 次碰撞）",
                teamName(unit.team),
                role->Name,
                projectileCancelled,
                projectileCancellations));
        appendSourceImportantEffect(
            "magic_points_drained",
            magicPointsDrainEvents,
            magicPointsDrained,
            std::format(
                "[{}] {}吸取 {} MP，實際回復 {} MP",
                teamName(unit.team),
                role->Name,
                magicPointsDrained,
                magicPointsRestored));
        appendSourceImportantEffect(
            "poison_activity",
            poisonPayloadEvents,
            0,
            std::format(
                "[{}] {}送出 {} 次施毒效果，成功套用或強化 {} 次，觸發 {} 次毒傷，共 {} 點",
                teamName(unit.team),
                role->Name,
                poisonPayloadEvents,
                poisonApplicationEvents,
                poisonTicks,
                poisonDamage));
        appendSourceImportantEffect(
            "bleed_applied",
            bleedApplications,
            0,
            std::format(
                "[{}] {}施加流血 {} 次",
                teamName(unit.team),
                role->Name,
                bleedApplications));
        appendSourceImportantEffect(
            "stun_applied",
            stunApplications,
            stunDurationFrames,
            std::format(
                "[{}] {}施加眩暈 {} 次，共 {} 幀",
                teamName(unit.team),
                role->Name,
                stunApplications,
                stunDurationFrames));
        appendSourceImportantEffect(
            "death_prevented",
            deathPreventionTriggers,
            0,
            std::format(
                "[{}] {}觸發免死 {} 次",
                teamName(unit.team),
                role->Name,
                deathPreventionTriggers));
        for (const auto& [source, damage] : aggregate.nonSkillDamageSources)
        {
            if (!containsText(source, "殉爆"))
            {
                continue;
            }
            appendSourceImportantEffect(
                "death_explosion_damage",
                aggregate.nonSkillDamageSourceCounts.at(source),
                damage,
                std::format(
                    "[{}] {}的殉爆觸發 {} 次，共造成 {} 點傷害",
                    teamName(unit.team),
                    role->Name,
                    aggregate.nonSkillDamageSourceCounts.at(source),
                    damage));
        }
        if (unit.team == 0) allyDamage += stats.damage_dealt;
        else enemyDamage += stats.damage_dealt;
        if (stats.damage_dealt > topDamage)
        {
            topDamage = stats.damage_dealt;
            topUnit = &unit;
        }
        dto.unit_stats.push_back(std::move(stats));
    }
    for (const auto& event : battle.report.events())
    {
        if (event.type == BattleReportEventType::Kill)
        {
            dto.key_events.push_back({
                event.frame,
                std::format(
                    "[{}] {} 擊倒 [{}] {}",
                    teamName(event.sourceTeam),
                    event.sourceName,
                    teamName(event.targetTeam),
                    event.targetName),
            });
        }
    }
    const int allySurvivors = static_cast<int>(std::ranges::count_if(
        battle.summary.survivors,
        [](const auto& survivor) { return survivor.team == 0 && !survivor.summoned; }));
    const int enemySurvivors = static_cast<int>(std::ranges::count_if(
        battle.summary.survivors,
        [](const auto& survivor) { return survivor.team == 1 && !survivor.summoned; }));
    const int allySummonedSurvivors = static_cast<int>(std::ranges::count_if(
        battle.summary.survivors,
        [](const auto& survivor) { return survivor.team == 0 && survivor.summoned; }));
    const int enemySummonedSurvivors = static_cast<int>(std::ranges::count_if(
        battle.summary.survivors,
        [](const auto& survivor) { return survivor.team == 1 && survivor.summoned; }));
    const int allyCount = static_cast<int>(std::ranges::count(prepared.units, 0, &PreparedChessBattleUnit::team));
    const int enemyCount = static_cast<int>(prepared.units.size()) - allyCount;
    dto.summary = std::format(
        "{}；我方存活 {}/{}，敵方存活 {}/{}；總傷害我方 {}、敵方 {}",
        dto.outcome_description,
        allySurvivors,
        allyCount,
        enemySurvivors,
        enemyCount,
        allyDamage,
        enemyDamage);
    if (allySummonedSurvivors > 0 || enemySummonedSurvivors > 0)
    {
        dto.summary += std::format(
            "；另有召喚單位存活我方 {}、敵方 {}",
            allySummonedSurvivors,
            enemySummonedSurvivors);
    }
    if (topUnit)
    {
        const auto* role = content.role(topUnit->roleId);
        assert(role);
        dto.summary += std::format(
            "；全場最高輸出為[{}] {}的 {} 點",
            teamName(topUnit->team),
            role->Name,
            topDamage);
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
    const ChessSaveStore& saves,
    ObservationDetail detail = ObservationDetail::Full)
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
        const auto detail = params ? parseObservationDetail(params->detail) : std::nullopt;
        if (!params || !difficulty || !seed || !detail)
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
        return response(request->id, true, writeJson(sessionObservationDto(*session_, saves_, *detail)));
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
        const auto params = readJson<ObserveParams>(request->params.str);
        const auto detail = params ? parseObservationDetail(params->detail) : std::nullopt;
        if (!params || !detail)
        {
            return response(request->id, false, {}, "invalid_params", "detail 必須是 compact 或 full");
        }
        return response(
            request->id,
            true,
            writeJson(sessionObservationDto(*session_, saves_, *detail)));
    }
    if (request->method == "legal_actions")
    {
        std::vector<LegalActionDto> legal;
        for (const auto& descriptor : session_->legalActions())
        {
            legal.push_back(legalActionDto(*session_, descriptor));
        }
        return response(request->id, true, writeJson(legal));
    }
    if (request->method == "inspect_shop_slot")
    {
        const auto params = readJson<ShopSlotParams>(request->params.str);
        if (!params
            || params->slot < 0
            || params->slot >= static_cast<int>(session_->state().shop.size()))
        {
            return response(request->id, false, {}, "invalid_shop_slot", "商店欄位不存在");
        }
        return response(
            request->id,
            true,
            writeJson(shopSlotInspectionDto(*session_, params->slot)));
    }
    if (request->method == "inspect_shop")
    {
        return response(request->id, true, writeJson(shopInspectionDto(*session_)));
    }
    if (request->method == "get_shop_odds")
    {
        const auto params = readJson<ShopOddsParams>(request->params.str);
        const int level = params && params->level
            ? *params->level
            : session_->state().level;
        if (!params
            || level < 0
            || level >= static_cast<int>(session_->content().balance().shopWeights.size()))
        {
            return response(request->id, false, {}, "invalid_level", "等級超出商店機率表範圍");
        }
        return response(request->id, true, writeJson(shopOddsDto(*session_, level)));
    }
    if (request->method == "inspect_chess_instance")
    {
        const auto params = readJson<ChessInstanceParams>(request->params.str);
        const auto piece = params
            ? session_->state().roster.find(params->chess_instance_id)
            : session_->state().roster.end();
        if (!params || piece == session_->state().roster.end())
        {
            return response(request->id, false, {}, "unknown_chess_instance", "棋子實例不存在");
        }
        return response(
            request->id,
            true,
            writeJson(chessInstanceInspectionDto(*session_, piece->second)));
    }
    if (request->method == "inspect_bans")
    {
        return response(request->id, true, writeJson(banInspectionDto(*session_)));
    }
    if (request->method == "inspect_role")
    {
        const auto params = readJson<RoleParams>(request->params.str);
        if (!params || !session_->content().role(params->role_id))
        {
            return response(request->id, false, {}, "invalid_role", "角色不存在");
        }
        return response(
            request->id,
            true,
            writeJson(roleDto(session_->content(), params->role_id)));
    }
    if (request->method == "inspect_combo")
    {
        const auto params = readJson<ComboParams>(request->params.str);
        const auto definition = params
            ? std::ranges::find(
                session_->content().combos(),
                params->combo_name,
                &ComboDef::name)
            : session_->content().combos().end();
        if (!params || definition == session_->content().combos().end())
        {
            return response(request->id, false, {}, "invalid_combo", "羈絆不存在");
        }
        const auto observation = session_->observe();
        const auto progress = std::ranges::find(
            observation.combos,
            definition->id,
            &ChessObservedCombo::comboId);
        assert(progress != observation.combos.end());
        return response(request->id, true, writeJson(comboDto(
            session_->content(),
            *definition,
            progress->physicalCount,
            progress->effectiveCount,
            progress->activeThresholdIndex,
            true,
            &progress->contributions)));
    }
    if (request->method == "inspect_equipment")
    {
        const auto params = readJson<EquipmentParams>(request->params.str);
        const auto definition = params
            ? std::ranges::find(
                session_->content().equipment(),
                params->item_id,
                &EquipmentDef::itemId)
            : session_->content().equipment().end();
        if (!params || definition == session_->content().equipment().end())
        {
            return response(request->id, false, {}, "invalid_equipment", "裝備不存在");
        }
        return response(
            request->id,
            true,
            writeJson(equipmentInfoDto(session_->content(), params->item_id)));
    }
    if (request->method == "inspect_challenge")
    {
        const auto params = readJson<ChallengeParams>(request->params.str);
        const auto definition = params
            ? std::ranges::find(
                session_->content().balance().challenges,
                params->challenge_name,
                &BalanceConfig::ChallengeDef::name)
            : session_->content().balance().challenges.end();
        if (!params || definition == session_->content().balance().challenges.end())
        {
            return response(request->id, false, {}, "invalid_challenge", "遠征挑戰不存在");
        }
        return response(
            request->id,
            true,
            writeJson(challengeDto(session_->content(), *definition)));
    }
    if (request->method == "inspect_prepared_battle")
    {
        const auto observation = session_->observe();
        if (!observation.preparedBattle)
        {
            return response(request->id, false, {}, "no_prepared_battle", "目前沒有已準備的戰鬥");
        }
        return response(request->id, true, writeJson(preparedBattleDto(
            session_->content(),
            *observation.preparedBattle)));
    }
    if (request->method == "inspect_last_battle")
    {
        if (!session_->lastBattlePrepared() || !session_->lastBattleResult())
        {
            return response(request->id, false, {}, "no_last_battle", "目前沒有已完成的戰鬥");
        }
        return response(request->id, true, writeJson(battleResultDto(
            session_->content(),
            *session_->lastBattlePrepared(),
            *session_->lastBattleResult(),
            ObservationDetail::Full)));
    }
    if (request->method == "act")
    {
        const auto params = readJson<ActParams>(request->params.str);
        std::string actionError;
        const auto action = params
            ? parseChessActionJson(params->action.str, actionError)
            : std::nullopt;
        const auto requestedDetail = params
            ? parseActionResponseDetail(params->detail)
            : std::nullopt;
        if (params && !requestedDetail)
        {
            return response(
                request->id,
                false,
                {},
                "invalid_params",
                "detail 必須是 summary、compact 或 full");
        }
        if (!action)
        {
            return response(
                request->id,
                false,
                {},
                "invalid_action",
                params ? actionError : "缺少 action JSON 物件");
        }
        const auto before = session_->state();
        const auto result = session_->submitAndDrain(*action);
        if (*requestedDetail == ActionResponseDetail::Summary)
        {
            return response(request->id, true, writeJson(summaryActionResultDto(
                *session_,
                before,
                result,
                action->type)));
        }
        if (!result.accepted && *requestedDetail == ActionResponseDetail::Compact)
        {
            CompactRejectedActionResultDto rejected{};
            rejected.accepted = false;
            rejected.error_code = ruleErrorId(result.error);
            rejected.description = result.description;
            rejected.next_observation.phase = phaseText(session_->state().phase);
            for (const auto& legal : session_->legalActions())
            {
                rejected.next_observation.legal_action_types.push_back(
                    chessActionTypeId(legal.type));
            }
            rejected.next_observation.state_hash = chessSha256Hex(session_->observe().stateHash);
            return response(request->id, true, writeJson(rejected));
        }
        const auto responseDetail = *requestedDetail == ActionResponseDetail::Full
            ? ObservationDetail::Full
            : ObservationDetail::Compact;
        const auto legalActions = session_->legalActions();
        ActionResultDto dto{};
        dto.accepted = result.accepted;
        dto.error_code = ruleErrorId(result.error);
        dto.description = result.description;
        dto.next_observation = observationDto(
            session_->observe(),
            session_->content(),
            responseDetail,
            {},
            legalActions);
        dto.replay_sequence = result.replaySequence;
        if (*requestedDetail == ActionResponseDetail::Full)
        {
            dto.pre_state_hash = chessSha256Hex(result.preStateHash);
            dto.post_state_hash = chessSha256Hex(result.postStateHash);
            dto.event_hash = chessSha256Hex(result.eventHash);
            dto.rng_digest = chessSha256Hex(result.rngDigest);
            dto.chain_hash = chessSha256Hex(result.chainHash);
        }
        for (const auto& event : result.events)
        {
            dto.events.push_back(semanticEventDto(session_->content(), event));
        }
        if (result.accepted
            && *requestedDetail == ActionResponseDetail::Full
            && action->type == ChessActionType::StartBattle
            && session_->lastBattlePrepared()
            && session_->lastBattleResult())
        {
            dto.battle = battleResultDto(
                session_->content(),
                *session_->lastBattlePrepared(),
                *session_->lastBattleResult(),
                responseDetail);
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
