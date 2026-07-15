#pragma once

#include <glaze/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace KysChess::ProtocolDetail
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
struct InspectPreparedBattleParams { std::string detail = "summary"; };
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
    std::optional<int> chess_instance_id;
    std::optional<int> role_id;
    std::string name;
    std::string team;
    int star{};
    std::optional<std::string> weapon;
    std::optional<std::string> armor;
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
    std::optional<std::string> battle_id;
    std::optional<std::string> metadata_scope;
    std::vector<PreparedUnitDto> units;
    std::optional<std::vector<MapDto>> map_candidates;
    std::optional<int> chosen_map_id;
    std::string chosen_map_name;
    std::optional<std::string> coordinate_system;
    std::optional<std::string> board;
    std::optional<std::vector<ComboDto>> ally_synergies;
    std::optional<std::vector<ComboDto>> enemy_synergies;
    std::optional<std::uint32_t> battle_seed;
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


}  // namespace KysChess::ProtocolDetail
