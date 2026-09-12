#include "ChessGameplayEffectInternal.h"

namespace KysChess::GameplayEffects
{
namespace
{

struct HitDefencePenalty final : GameplayEffectDefinition
{
    int 防禦百分比{};
    int 持續幀數{};
    static constexpr std::string_view Name = "命中降低防禦";
    static constexpr auto Parameters = std::array<Parameter<HitDefencePenalty>, 2>{
        {Parameter<HitDefencePenalty>{{"防禦百分比", -1000000, 1000000}, &HitDefencePenalty::防禦百分比},
         Parameter<HitDefencePenalty>{{"持續幀數", 1, 1000000}, &HitDefencePenalty::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Defence,
                                                                          .amount = EffectNumber{.flat = 防禦百分比},
                                                                          .operation = AttributeOperation::PercentAdd,
                                                                          .durationFrames = 持續幀數,
                                                                          .stack = EffectStackPolicy::Refresh}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("命中使目標防禦{:+}%，持續{}幀。重複施加刷新持續時間。", 防禦百分比, 持續幀數);
        }
        return std::format("命中使目標防禦{:+}%，持續{}幀。", 防禦百分比, 持續幀數);
    }
};

struct CastStackBlock final : GameplayEffectDefinition
{
    int 每層格擋百分比{};
    int 層數上限{};
    static constexpr std::string_view Name = "出招疊加格擋";
    static constexpr auto Parameters = std::array<Parameter<CastStackBlock>, 2>{
        {Parameter<CastStackBlock>{{"每層格擋百分比", -1000000, 1000000}, &CastStackBlock::每層格擋百分比},
         Parameter<CastStackBlock>{{"層數上限", 1, 1000}, &CastStackBlock::層數上限}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::BlockChance,
                                                                    .amount = EffectNumber{.flat = 每層格擋百分比},
                                                                    .operation = AttributeOperation::PercentagePointAdd,
                                                                    .stack = EffectStackPolicy::AddStack,
                                                                    .stackLimit = 層數上限}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("每次出招格擋率{:+}%，最多{}層。", 每層格擋百分比, 層數上限); }
};

struct CastStatBuff final : GameplayEffectDefinition
{
    int 攻擊百分比{};
    int 防禦百分比{};
    int 速度百分比{};
    int 持續幀數{};
    static constexpr std::string_view Name = "出招強化攻防速度";
    static constexpr auto Parameters = std::array<Parameter<CastStatBuff>, 4>{
        {Parameter<CastStatBuff>{{"攻擊百分比", -1000000, 1000000}, &CastStatBuff::攻擊百分比},
         Parameter<CastStatBuff>{{"防禦百分比", -1000000, 1000000}, &CastStatBuff::防禦百分比},
         Parameter<CastStatBuff>{{"速度百分比", -1000000, 1000000}, &CastStatBuff::速度百分比},
         Parameter<CastStatBuff>{{"持續幀數", 1, 1000000}, &CastStatBuff::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::AttackCommitted,
                           .actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Attack,
                                                                          .amount = EffectNumber{.flat = 攻擊百分比},
                                                                          .operation = AttributeOperation::PercentAdd,
                                                                          .durationFrames = 持續幀數,
                                                                          .stack = EffectStackPolicy::Refresh}},
                              EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Defence,
                                                                          .amount = EffectNumber{.flat = 防禦百分比},
                                                                          .operation = AttributeOperation::PercentAdd,
                                                                          .durationFrames = 持續幀數,
                                                                          .stack = EffectStackPolicy::Refresh}},
                              EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Speed,
                                                                          .amount = EffectNumber{.flat = 速度百分比},
                                                                          .operation = AttributeOperation::PercentAdd,
                                                                          .durationFrames = 持續幀數,
                                                                          .stack = EffectStackPolicy::Refresh}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("出招時攻擊{:+}%、防禦{:+}%、速度{:+}%，持續{}幀。重複觸發刷新持續時間。",
                               攻擊百分比,
                               防禦百分比,
                               速度百分比,
                               持續幀數);
        }
        return std::format(
            "出招時攻擊{:+}%、防禦{:+}%、速度{:+}%，持續{}幀。", 攻擊百分比, 防禦百分比, 速度百分比, 持續幀數);
    }
};

struct SwordAlliesSureHit final : GameplayEffectDefinition
{
    int 持續幀數{};
    static constexpr std::string_view Name = "御劍友軍必中";
    static constexpr auto Parameters = std::array<Parameter<SwordAlliesSureHit>, 1>{
        {Parameter<SwordAlliesSureHit>{{"持續幀數", 1, 1000000}, &SwordAlliesSureHit::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::AttackCommitted,
                           .selector = EffectSelector{.kind = EffectSelectorKind::AlliesUsingMartialCategory,
                                                      .requiredMartialCategory = EffectMartialCategory::Sword},
                           .actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::GuaranteedHit,
                                                                          .amount = EffectNumber{.flat = 1},
                                                                          .operation = AttributeOperation::Override,
                                                                          .durationFrames = 持續幀數,
                                                                          .stack = EffectStackPolicy::Refresh}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招時，友方御劍角色獲得必中，持續{}幀。無視閃避與格擋，仍受無敵與護盾限制；重複觸發刷新時間。",
                持續幀數);
        }
        return std::format("出招時，友方御劍角色獲得必中，持續{}幀。", 持續幀數);
    }
};

struct CastDamageReduction final : GameplayEffectDefinition
{
    int 減傷百分比{};
    int 持續幀數{};
    static constexpr std::string_view Name = "出招減傷";
    static constexpr auto Parameters = std::array<Parameter<CastDamageReduction>, 2>{
        {Parameter<CastDamageReduction>{{"減傷百分比", -1000000, 1000000}, &CastDamageReduction::減傷百分比},
         Parameter<CastDamageReduction>{{"持續幀數", 1, 1000000}, &CastDamageReduction::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::DamageReduction,
                                                                    .amount = EffectNumber{.flat = 減傷百分比},
                                                                    .operation = AttributeOperation::PercentagePointAdd,
                                                                    .durationFrames = 持續幀數,
                                                                    .stack = EffectStackPolicy::Refresh}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("出招時傷害減免{:+}%，持續{}幀。重複觸發刷新持續時間。", 減傷百分比, 持續幀數);
        }
        return std::format("出招時傷害減免{:+}%，持續{}幀。", 減傷百分比, 持續幀數);
    }
};

struct CastDodgeCriticalBuff final : GameplayEffectDefinition
{
    int 閃避百分比{};
    int 暴擊百分比{};
    int 持續幀數{};
    static constexpr std::string_view Name = "出招強化閃避暴擊";
    static constexpr auto Parameters = std::array<Parameter<CastDodgeCriticalBuff>, 3>{
        {Parameter<CastDodgeCriticalBuff>{{"閃避百分比", -1000000, 1000000}, &CastDodgeCriticalBuff::閃避百分比},
         Parameter<CastDodgeCriticalBuff>{{"暴擊百分比", -1000000, 1000000}, &CastDodgeCriticalBuff::暴擊百分比},
         Parameter<CastDodgeCriticalBuff>{{"持續幀數", 1, 1000000}, &CastDodgeCriticalBuff::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::DodgeChance,
                                                                    .amount = EffectNumber{.flat = 閃避百分比},
                                                                    .operation = AttributeOperation::PercentagePointAdd,
                                                                    .durationFrames = 持續幀數,
                                                                    .stack = EffectStackPolicy::Refresh}},
                        EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::CriticalChance,
                                                                    .amount = EffectNumber{.flat = 暴擊百分比},
                                                                    .operation = AttributeOperation::PercentagePointAdd,
                                                                    .durationFrames = 持續幀數,
                                                                    .stack = EffectStackPolicy::Refresh}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招時閃避率{:+}%、暴擊率{:+}%，持續{}幀。重複觸發刷新持續時間。", 閃避百分比, 暴擊百分比, 持續幀數);
        }
        return std::format("出招時閃避率{:+}%、暴擊率{:+}%，持續{}幀。", 閃避百分比, 暴擊百分比, 持續幀數);
    }
};

struct CastTeamAttack final : GameplayEffectDefinition
{
    int 攻擊點數{};
    int 持續幀數{};
    static constexpr std::string_view Name = "出招全隊攻擊加成";
    static constexpr auto Parameters = std::array<Parameter<CastTeamAttack>, 2>{
        {Parameter<CastTeamAttack>{{"攻擊點數", -1000000, 1000000}, &CastTeamAttack::攻擊點數},
         Parameter<CastTeamAttack>{{"持續幀數", 1, 1000000}, &CastTeamAttack::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::AttackCommitted,
                       .selector = EffectSelector{.kind = EffectSelectorKind::Allies},
                       .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Attack,
                                                                               .amount = EffectNumber{.flat = 攻擊點數},
                                                                               .durationFrames = 持續幀數,
                                                                               .stack = EffectStackPolicy::Refresh}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("出招時全隊攻擊{:+}，持續{}幀。重複觸發刷新持續時間。", 攻擊點數, 持續幀數);
        }
        return std::format("出招時全隊攻擊{:+}，持續{}幀。", 攻擊點數, 持續幀數);
    }
};

struct BurningArea final : GameplayEffectDefinition
{
    int 半徑格數{};
    int 持續幀數{};
    int 每次傷害{};
    int 間隔幀數{};
    static constexpr std::string_view Name = "持續傷害區域";
    static constexpr auto Parameters = std::array<Parameter<BurningArea>, 4>{
        {Parameter<BurningArea>{{"半徑格數", 1, 1000000}, &BurningArea::半徑格數},
         Parameter<BurningArea>{{"持續幀數", 1, 1000000}, &BurningArea::持續幀數},
         Parameter<BurningArea>{{"每次傷害", 0, 1000000}, &BurningArea::每次傷害},
         Parameter<BurningArea>{{"間隔幀數", 1, 1000000}, &BurningArea::間隔幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::AttackCommitted,
                       .actions = {EffectAction{.value = CreateAreaAction{
                                                    .radiusTiles = 半徑格數,
                                                    .anchor = AreaAnchor::FollowSourceUnit,
                                                    .durationFrames = 持續幀數,
                                                    .sourceDeath = AreaSourceDeathPolicy::RemoveImmediately,
                                                    .merge = AreaMergePolicy::RefreshSameSource,
                                                    .modifiers = {AreaModifier{.kind = AreaModifierKind::PeriodicDamage,
                                                                               .relation = EffectTeamFilter::Enemy,
                                                                               .amount = EffectNumber{.flat = 每次傷害},
                                                                               .intervalFrames = 間隔幀數,
                                                                               .overlap = AreaOverlapPolicy::Add}}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("出招建立半徑{}格的區域，持續{}幀；每{}幀對區域內敵人造成{}傷害。來源死亡時區域消失。",
                               半徑格數,
                               持續幀數,
                               間隔幀數,
                               每次傷害);
        }
        return std::format(
            "出招建立半徑{}格的區域，持續{}幀；每{}幀對區域內敵人造成{}傷害。", 半徑格數, 持續幀數, 間隔幀數, 每次傷害);
    }
};

struct CastStackCritical final : GameplayEffectDefinition
{
    int 每層暴擊百分比{};
    int 層數上限{};
    int 每層暴傷百分比{};
    static constexpr std::string_view Name = "出招疊加暴擊";
    static constexpr auto Parameters = std::array<Parameter<CastStackCritical>, 3>{
        {Parameter<CastStackCritical>{{"每層暴擊百分比", -1000000, 1000000}, &CastStackCritical::每層暴擊百分比},
         Parameter<CastStackCritical>{{"層數上限", 1, 1000}, &CastStackCritical::層數上限},
         Parameter<CastStackCritical>{{"每層暴傷百分比", -1000000, 1000000}, &CastStackCritical::每層暴傷百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::CriticalChance,
                                                                    .amount = EffectNumber{.flat = 每層暴擊百分比},
                                                                    .operation = AttributeOperation::PercentagePointAdd,
                                                                    .stack = EffectStackPolicy::AddStack,
                                                                    .stackLimit = 層數上限}},
                        EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::CriticalDamage,
                                                                    .amount = EffectNumber{.flat = 每層暴傷百分比},
                                                                    .operation = AttributeOperation::PercentagePointAdd,
                                                                    .stack = EffectStackPolicy::AddStack,
                                                                    .stackLimit = 層數上限}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("每次出招暴擊率{:+}%、暴擊傷害{:+}%，最多{}層。", 每層暴擊百分比, 每層暴傷百分比, 層數上限); }
};

struct DefenceBonus final : GameplayEffectDefinition
{
    int 點數{};
    static constexpr std::string_view Name = "防禦加成";
    static constexpr auto Parameters = std::array<Parameter<DefenceBonus>, 1>{
        {Parameter<DefenceBonus>{{"點數", -1000000, 1000000}, &DefenceBonus::點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Defence,
                                                                          .amount = EffectNumber{.flat = 點數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("防禦{:+}。", 點數); }
};

struct FlatDamageReduction final : GameplayEffectDefinition
{
    int 點數{};
    static constexpr std::string_view Name = "固定承傷修正";
    static constexpr auto Parameters = std::array<Parameter<FlatDamageReduction>, 1>{
        {Parameter<FlatDamageReduction>{{"點數", -1000000, 1000000}, &FlatDamageReduction::點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{.value = ModifyDamageAction{.perspective = DamageModifierPerspective::Incoming,
                                                                 .channel = DamageChannel::All,
                                                                 .amount = EffectNumber{.flat = 點數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("每次承受傷害{:+}點，在計算防禦前生效。", 點數); }
};

struct BlockBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "格擋率加成";
    static constexpr auto Parameters = std::array<Parameter<BlockBonus>, 1>{
        {Parameter<BlockBonus>{{"百分比", -1000000, 1000000}, &BlockBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::BlockChance,
                                                                          .amount = EffectNumber{.flat = 百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("格擋率{:+}%。", 百分比); }
};

struct SkillReflectBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "技能反彈加成";
    static constexpr auto Parameters = std::array<Parameter<SkillReflectBonus>, 1>{
        {Parameter<SkillReflectBonus>{{"百分比", -1000000, 1000000}, &SkillReflectBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::SkillReflectPercent,
                                                                    .amount = EffectNumber{.flat = 百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("技能反彈{:+}%。", 百分比); }
};

struct MaxHealthBonus final : GameplayEffectDefinition
{
    int 點數{};
    static constexpr std::string_view Name = "生命上限加成";
    static constexpr auto Parameters = std::array<Parameter<MaxHealthBonus>, 1>{
        {Parameter<MaxHealthBonus>{{"點數", -1000000, 1000000}, &MaxHealthBonus::點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions
                           = {EffectAction{.value = ModifyAttributeAction{.amount = EffectNumber{.flat = 點數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("最大生命{:+}。", 點數); }
};

struct AttackPercentBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "攻擊百分比加成";
    static constexpr auto Parameters = std::array<Parameter<AttackPercentBonus>, 1>{
        {Parameter<AttackPercentBonus>{{"百分比", -1000000, 1000000}, &AttackPercentBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Attack,
                                                                    .amount = EffectNumber{.flat = 百分比},
                                                                    .operation = AttributeOperation::PercentAdd}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("攻擊{:+}%。", 百分比); }
};

struct SpeedBonus final : GameplayEffectDefinition
{
    int 點數{};
    static constexpr std::string_view Name = "速度加成";
    static constexpr auto Parameters
        = std::array<Parameter<SpeedBonus>, 1>{{Parameter<SpeedBonus>{{"點數", -1000000, 1000000}, &SpeedBonus::點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Speed,
                                                                          .amount = EffectNumber{.flat = 點數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("速度{:+}。", 點數); }
};

struct StaggerResistanceBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "僵直抗性加成";
    static constexpr auto Parameters = std::array<Parameter<StaggerResistanceBonus>, 1>{
        {Parameter<StaggerResistanceBonus>{{"百分比", -1000000, 1000000}, &StaggerResistanceBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions = {EffectAction{
                               .value = ModifyAttributeAction{.attribute = BattleAttribute::StaggerResistance,
                                                              .amount = EffectNumber{.flat = 百分比},
                                                              .operation = AttributeOperation::PercentagePointAdd}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("僵直抗性{:+}%。", 百分比); }
};

struct CooldownReductionBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "冷卻縮減加成";
    static constexpr auto Parameters = std::array<Parameter<CooldownReductionBonus>, 1>{
        {Parameter<CooldownReductionBonus>{{"百分比", -1000000, 1000000}, &CooldownReductionBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions = {EffectAction{
                               .value = ModifyAttributeAction{.attribute = BattleAttribute::CooldownReduction,
                                                              .amount = EffectNumber{.flat = 百分比},
                                                              .operation = AttributeOperation::PercentagePointAdd}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("冷卻縮減{:+}%。", 百分比); }
};

struct MpRecoveryBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "回內加成";
    static constexpr auto Parameters = std::array<Parameter<MpRecoveryBonus>, 1>{
        {Parameter<MpRecoveryBonus>{{"百分比", -1000000, 1000000}, &MpRecoveryBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions = {EffectAction{
                               .value = ModifyAttributeAction{.attribute = BattleAttribute::MpRecoveryBonus,
                                                              .amount = EffectNumber{.flat = 百分比},
                                                              .operation = AttributeOperation::PercentagePointAdd}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("內力回復加成{:+}%。", 百分比); }
};

struct HitStackDamage final : GameplayEffectDefinition
{
    int 每層增傷百分比{};
    int 持續幀數{};
    int 層數上限{};
    static constexpr std::string_view Name = "命中疊加技能傷害";
    static constexpr auto Parameters = std::array<Parameter<HitStackDamage>, 3>{
        {Parameter<HitStackDamage>{{"每層增傷百分比", -1000000, 1000000}, &HitStackDamage::每層增傷百分比},
         Parameter<HitStackDamage>{{"持續幀數", 1, 1000000}, &HitStackDamage::持續幀數},
         Parameter<HitStackDamage>{{"層數上限", 1, 1000}, &HitStackDamage::層數上限}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::HitBeforeDamage,
                           .actions
                           = {EffectAction{.value = ModifyDamageAction{.stage = DamageModifierStage::AfterDefense,
                                                                       .amount = EffectNumber{.flat = 每層增傷百分比},
                                                                       .operation = DamageModifierOperation::PercentAdd,
                                                                       .durationFrames = 持續幀數,
                                                                       .stack = EffectStackPolicy::AddStack,
                                                                       .stackLimit = 層數上限}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("命中後技能傷害{:+}%，持續{}幀，最多{}層。在計算防禦後增加技能傷害。",
                               每層增傷百分比,
                               持續幀數,
                               層數上限);
        }
        return std::format("命中後技能傷害{:+}%，持續{}幀，最多{}層。", 每層增傷百分比, 持續幀數, 層數上限);
    }
};

struct AttackBonus final : GameplayEffectDefinition
{
    int 點數{};
    static constexpr std::string_view Name = "攻擊加成";
    static constexpr auto Parameters = std::array<Parameter<AttackBonus>, 1>{
        {Parameter<AttackBonus>{{"點數", -1000000, 1000000}, &AttackBonus::點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Attack,
                                                                          .amount = EffectNumber{.flat = 點數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("攻擊{:+}。", 點數); }
};

struct DefencePercentBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "防禦百分比加成";
    static constexpr auto Parameters = std::array<Parameter<DefencePercentBonus>, 1>{
        {Parameter<DefencePercentBonus>{{"百分比", -1000000, 1000000}, &DefencePercentBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Defence,
                                                                    .amount = EffectNumber{.flat = 百分比},
                                                                    .operation = AttributeOperation::PercentAdd}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("防禦{:+}%。", 百分比); }
};

struct DodgeBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "閃避率加成";
    static constexpr auto Parameters = std::array<Parameter<DodgeBonus>, 1>{
        {Parameter<DodgeBonus>{{"百分比", -1000000, 1000000}, &DodgeBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::DodgeChance,
                                                                          .amount = EffectNumber{.flat = 百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("閃避率{:+}%。", 百分比); }
};

struct CriticalAfterDodge final : GameplayEffectDefinition
{
    static constexpr std::string_view Name = "閃避後必暴擊";
    static constexpr auto Parameters = std::array<Parameter<CriticalAfterDodge>, 0>{{}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::CriticalAfterDodge,
                                                                    .amount = EffectNumber{.flat = 1}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return "成功閃避後，下次命中必定暴擊。"; }
};

struct MaxHealthPercentBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "生命上限百分比加成";
    static constexpr auto Parameters = std::array<Parameter<MaxHealthPercentBonus>, 1>{
        {Parameter<MaxHealthPercentBonus>{{"百分比", -1000000, 1000000}, &MaxHealthPercentBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{.value = ModifyAttributeAction{.amount = EffectNumber{.flat = 百分比},
                                                                    .operation = AttributeOperation::PercentAdd}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("最大生命{:+}%。", 百分比); }
};

struct LowHealthAttack final : GameplayEffectDefinition
{
    int 生命門檻百分比{};
    int 攻擊點數{};
    static constexpr std::string_view Name = "低血攻擊加成";
    static constexpr auto Parameters = std::array<Parameter<LowHealthAttack>, 2>{
        {Parameter<LowHealthAttack>{{"生命門檻百分比", 0, 1000000}, &LowHealthAttack::生命門檻百分比},
         Parameter<LowHealthAttack>{{"攻擊點數", -1000000, 1000000}, &LowHealthAttack::攻擊點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::FrameAdvanced,
                       .conditions = {SourceHpRatioBelowCondition{.percent = 生命門檻百分比}},
                       .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Attack,
                                                                               .amount = EffectNumber{.flat = 攻擊點數},
                                                                               .durationFrames = 1,
                                                                               .stack = EffectStackPolicy::Refresh}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("生命低於{}%時，攻擊{:+}。離開低血狀態即失去加成。", 生命門檻百分比, 攻擊點數);
        }
        return std::format("生命低於{}%時，攻擊{:+}。", 生命門檻百分比, 攻擊點數);
    }
};

struct LowHealthAttackPercent final : GameplayEffectDefinition
{
    int 生命門檻百分比{};
    int 攻擊百分比{};
    static constexpr std::string_view Name = "低血攻擊百分比加成";
    static constexpr auto Parameters = std::array<Parameter<LowHealthAttackPercent>, 2>{
        {Parameter<LowHealthAttackPercent>{{"生命門檻百分比", 0, 1000000}, &LowHealthAttackPercent::生命門檻百分比},
         Parameter<LowHealthAttackPercent>{{"攻擊百分比", -1000000, 1000000}, &LowHealthAttackPercent::攻擊百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::FrameAdvanced,
                           .conditions = {SourceHpRatioBelowCondition{.percent = 生命門檻百分比}},
                           .actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Attack,
                                                                          .amount = EffectNumber{.flat = 攻擊百分比},
                                                                          .operation = AttributeOperation::PercentAdd,
                                                                          .durationFrames = 1,
                                                                          .stack = EffectStackPolicy::Refresh}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("生命低於{}%時，攻擊{:+}%。離開低血狀態即失去加成。", 生命門檻百分比, 攻擊百分比);
        }
        return std::format("生命低於{}%時，攻擊{:+}%。", 生命門檻百分比, 攻擊百分比);
    }
};

struct SkillDamageBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "技能增傷";
    static constexpr auto Parameters = std::array<Parameter<SkillDamageBonus>, 1>{
        {Parameter<SkillDamageBonus>{{"百分比", -1000000, 1000000}, &SkillDamageBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::SkillDamage,
                                                                          .amount = EffectNumber{.flat = 百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("技能傷害{:+}%。", 百分比); }
};

struct KillAttackBonus final : GameplayEffectDefinition
{
    int 點數{};
    static constexpr std::string_view Name = "擊殺增加攻擊";
    static constexpr auto Parameters = std::array<Parameter<KillAttackBonus>, 1>{
        {Parameter<KillAttackBonus>{{"點數", -1000000, 1000000}, &KillAttackBonus::點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::DamageResolved,
                           .conditions = {DamagePerspectiveCondition{}, DamageKilledTargetCondition{}},
                           .actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Attack,
                                                                          .amount = EffectNumber{.flat = 點數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("每次擊殺敵人後，攻擊{:+}。", 點數); }
};

struct ReceivedDamagePercent final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "承傷百分比修正";
    static constexpr auto Parameters = std::array<Parameter<ReceivedDamagePercent>, 1>{
        {Parameter<ReceivedDamagePercent>{{"百分比", -1000000, 1000000}, &ReceivedDamagePercent::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{.value = ModifyDamageAction{.perspective = DamageModifierPerspective::Incoming,
                                                                 .channel = DamageChannel::All,
                                                                 .amount = EffectNumber{.flat = 百分比},
                                                                 .operation = DamageModifierOperation::PercentAdd}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full) { return std::format("承受傷害{:+}%。在計算防禦前生效。", 百分比); }
        return std::format("承受傷害{:+}%。", 百分比);
    }
};

struct CriticalBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "暴擊率加成";
    static constexpr auto Parameters = std::array<Parameter<CriticalBonus>, 1>{
        {Parameter<CriticalBonus>{{"百分比", -1000000, 1000000}, &CriticalBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::CriticalChance,
                                                                          .amount = EffectNumber{.flat = 百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("暴擊率{:+}%。", 百分比); }
};

struct MinimumCriticalDamage final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "暴擊傷害下限";
    static constexpr auto Parameters = std::array<Parameter<MinimumCriticalDamage>, 1>{
        {Parameter<MinimumCriticalDamage>{{"百分比", 0, 1000000}, &MinimumCriticalDamage::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::CriticalDamage,
                                                                          .amount = EffectNumber{.flat = 百分比},
                                                                          .operation = AttributeOperation::AtLeast}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("暴擊傷害至少為{}%。", 百分比); }
};

struct MaxHitDamageCap final : GameplayEffectDefinition
{
    int 生命百分比{};
    static constexpr std::string_view Name = "單次承傷上限";
    static constexpr auto Parameters = std::array<Parameter<MaxHitDamageCap>, 1>{
        {Parameter<MaxHitDamageCap>{{"生命百分比", 0, 1000000}, &MaxHitDamageCap::生命百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{
                .value = ModifyDamageAction{.perspective = DamageModifierPerspective::Incoming,
                                            .stage = DamageModifierStage::Final,
                                            .channel = DamageChannel::All,
                                            .amount = EffectNumber{.flat = 生命百分比},
                                            .operation = DamageModifierOperation::CapSingleHitAtMaxHpPercent}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("每次承傷不超過最大生命的{}%。在最終傷害階段生效。", 生命百分比);
        }
        return std::format("每次承傷不超過最大生命的{}%。", 生命百分比);
    }
};

struct LastAliveAttack final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "最後存活攻擊加成";
    static constexpr auto Parameters = std::array<Parameter<LastAliveAttack>, 1>{
        {Parameter<LastAliveAttack>{{"百分比", -1000000, 1000000}, &LastAliveAttack::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::FrameAdvanced,
                           .conditions = {SourceIsLastAliveCondition{}},
                           .actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Attack,
                                                                          .amount = EffectNumber{.flat = 百分比},
                                                                          .operation = AttributeOperation::PercentAdd,
                                                                          .durationFrames = 1,
                                                                          .stack = EffectStackPolicy::Refresh}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("成為己方最後存活角色時，攻擊{:+}%。", 百分比); }
};

struct LastAliveBlock final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "最後存活格擋加成";
    static constexpr auto Parameters = std::array<Parameter<LastAliveBlock>, 1>{
        {Parameter<LastAliveBlock>{{"百分比", -1000000, 1000000}, &LastAliveBlock::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::FrameAdvanced,
                           .conditions = {SourceIsLastAliveCondition{}},
                           .actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::BlockChance,
                                                                          .amount = EffectNumber{.flat = 百分比},
                                                                          .durationFrames = 1,
                                                                          .stack = EffectStackPolicy::Refresh}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("成為己方最後存活角色時，格擋率{:+}%。", 百分比); }
};

struct SpeedPercentBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "速度百分比加成";
    static constexpr auto Parameters = std::array<Parameter<SpeedPercentBonus>, 1>{
        {Parameter<SpeedPercentBonus>{{"百分比", -1000000, 1000000}, &SpeedPercentBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Speed,
                                                                    .amount = EffectNumber{.flat = 百分比},
                                                                    .operation = AttributeOperation::PercentAdd}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("速度{:+}%。", 百分比); }
};

struct SlidingChanceBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "滑步機率加成";
    static constexpr auto Parameters = std::array<Parameter<SlidingChanceBonus>, 1>{
        {Parameter<SlidingChanceBonus>{{"百分比", -1000000, 1000000}, &SlidingChanceBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::DashChance,
                                                                          .amount = EffectNumber{.flat = 百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("滑步機率{:+}%。", 百分比); }
};

struct MissingHealthFlatReduction final : GameplayEffectDefinition
{
    int 點數{};
    static constexpr std::string_view Name = "失血固定承傷修正";
    static constexpr auto Parameters = std::array<Parameter<MissingHealthFlatReduction>, 1>{
        {Parameter<MissingHealthFlatReduction>{{"點數", -1000000, 1000000}, &MissingHealthFlatReduction::點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::HitBeforeDamage,
            .observation = EffectObservationScope::EventTarget,
            .actions = {EffectAction{
                .value = ModifyDamageAction{
                    .perspective = DamageModifierPerspective::Incoming,
                    .amount = EffectNumber{.base = EffectNumberBase::SourceMissingHpRatio, .percent = 點數 * 100}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        const auto magnitude = 點數 < 0 ? -點數 : 點數;
        auto text = std::format("依已損生命比例，承受技能傷害最多{}{}點。", 點數 < 0 ? "減少" : "增加", magnitude);
        if (style == EffectDescriptionStyle::Full)
        {
            text += std::format("修正量為已損生命比例×{}點，向零取整。在計算防禦前生效。", 點數);
        }
        return text;
    }
};

struct MissingHealthPercentReduction final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "失血百分比承傷修正";
    static constexpr auto Parameters
        = std::array<Parameter<MissingHealthPercentReduction>, 1>{{Parameter<MissingHealthPercentReduction>{
            {"百分比", -1000000, 1000000}, &MissingHealthPercentReduction::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::HitBeforeDamage,
            .observation = EffectObservationScope::EventTarget,
            .actions = {EffectAction{
                .value = ModifyDamageAction{
                    .perspective = DamageModifierPerspective::Incoming,
                    .amount = EffectNumber{.base = EffectNumberBase::SourceMissingHpRatio, .percent = 百分比 * 100},
                    .operation = DamageModifierOperation::PercentAdd}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        const auto magnitude = 百分比 < 0 ? -百分比 : 百分比;
        auto text = std::format("依已損生命比例，承受技能傷害最多{}{}%。", 百分比 < 0 ? "減少" : "增加", magnitude);
        if (style == EffectDescriptionStyle::Full)
        {
            text += std::format("百分比修正量為已損生命比例×{}，向零取整。在計算防禦前生效。", 百分比);
        }
        return text;
    }
};

struct TeamMaxHealthBonus final : GameplayEffectDefinition
{
    int 點數{};
    static constexpr std::string_view Name = "全隊生命加成";
    static constexpr auto Parameters = std::array<Parameter<TeamMaxHealthBonus>, 1>{
        {Parameter<TeamMaxHealthBonus>{{"點數", -1000000, 1000000}, &TeamMaxHealthBonus::點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.selector = EffectSelector{.kind = EffectSelectorKind::Allies},
                           .actions
                           = {EffectAction{.value = ModifyAttributeAction{.amount = EffectNumber{.flat = 點數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("全隊最大生命{:+}。", 點數); }
};

struct TeamAttackBonus final : GameplayEffectDefinition
{
    int 點數{};
    static constexpr std::string_view Name = "全隊攻擊加成";
    static constexpr auto Parameters = std::array<Parameter<TeamAttackBonus>, 1>{
        {Parameter<TeamAttackBonus>{{"點數", -1000000, 1000000}, &TeamAttackBonus::點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.selector = EffectSelector{.kind = EffectSelectorKind::Allies},
                           .actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Attack,
                                                                          .amount = EffectNumber{.flat = 點數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("全隊攻擊{:+}。", 點數); }
};

struct TeamDefenceBonus final : GameplayEffectDefinition
{
    int 點數{};
    static constexpr std::string_view Name = "全隊防禦加成";
    static constexpr auto Parameters = std::array<Parameter<TeamDefenceBonus>, 1>{
        {Parameter<TeamDefenceBonus>{{"點數", -1000000, 1000000}, &TeamDefenceBonus::點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.selector = EffectSelector{.kind = EffectSelectorKind::Allies},
                           .actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Defence,
                                                                          .amount = EffectNumber{.flat = 點數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return std::format("全隊防禦{:+}。", 點數); }
};

struct BlockCounterChance final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "格擋反招加成";
    static constexpr auto Parameters = std::array<Parameter<BlockCounterChance>, 1>{
        {Parameter<BlockCounterChance>{{"百分比", -1000000, 1000000}, &BlockCounterChance::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions = {EffectAction{
                               .value = ModifyAttributeAction{.attribute = BattleAttribute::CounterUltimateBlockChance,
                                                              .amount = EffectNumber{.flat = 百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("格擋反招機率{:+}%。", 百分比); }
};

struct ProjectileReflectBonus final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "彈道反射加成";
    static constexpr auto Parameters = std::array<Parameter<ProjectileReflectBonus>, 1>{
        {Parameter<ProjectileReflectBonus>{{"百分比", -1000000, 1000000}, &ProjectileReflectBonus::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions = {EffectAction{
                               .value = ModifyAttributeAction{.attribute = BattleAttribute::ProjectileReflectChance,
                                                              .amount = EffectNumber{.flat = 百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("彈道反射率{:+}%。", 百分比); }
};

}    // namespace

void appendAttributeEffects(std::vector<GameplayEffectRegistration>& entries)
{
    entries.push_back(registration<HitDefencePenalty>());
    entries.push_back(registration<CastStackBlock>());
    entries.push_back(registration<CastStatBuff>());
    entries.push_back(registration<SwordAlliesSureHit>());
    entries.push_back(registration<CastDamageReduction>());
    entries.push_back(registration<CastDodgeCriticalBuff>());
    entries.push_back(registration<CastTeamAttack>());
    entries.push_back(registration<BurningArea>());
    entries.push_back(registration<CastStackCritical>());
    entries.push_back(registration<DefenceBonus>());
    entries.push_back(registration<FlatDamageReduction>());
    entries.push_back(registration<BlockBonus>());
    entries.push_back(registration<SkillReflectBonus>());
    entries.push_back(registration<MaxHealthBonus>());
    entries.push_back(registration<AttackPercentBonus>());
    entries.push_back(registration<SpeedBonus>());
    entries.push_back(registration<StaggerResistanceBonus>());
    entries.push_back(registration<CooldownReductionBonus>());
    entries.push_back(registration<MpRecoveryBonus>());
    entries.push_back(registration<HitStackDamage>());
    entries.push_back(registration<AttackBonus>());
    entries.push_back(registration<DefencePercentBonus>());
    entries.push_back(registration<DodgeBonus>());
    entries.push_back(registration<CriticalAfterDodge>());
    entries.push_back(registration<MaxHealthPercentBonus>());
    entries.push_back(registration<LowHealthAttack>());
    entries.push_back(registration<LowHealthAttackPercent>());
    entries.push_back(registration<SkillDamageBonus>());
    entries.push_back(registration<KillAttackBonus>());
    entries.push_back(registration<ReceivedDamagePercent>());
    entries.push_back(registration<CriticalBonus>());
    entries.push_back(registration<MinimumCriticalDamage>());
    entries.push_back(registration<MaxHitDamageCap>());
    entries.push_back(registration<LastAliveAttack>());
    entries.push_back(registration<LastAliveBlock>());
    entries.push_back(registration<SpeedPercentBonus>());
    entries.push_back(registration<SlidingChanceBonus>());
    entries.push_back(registration<MissingHealthFlatReduction>());
    entries.push_back(registration<MissingHealthPercentReduction>());
    entries.push_back(registration<TeamMaxHealthBonus>());
    entries.push_back(registration<TeamAttackBonus>());
    entries.push_back(registration<TeamDefenceBonus>());
    entries.push_back(registration<BlockCounterChance>());
    entries.push_back(registration<ProjectileReflectBonus>());
}

}    // namespace KysChess::GameplayEffects
