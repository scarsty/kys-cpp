#include "ChessGameplayEffectInternal.h"

namespace KysChess::GameplayEffects
{
namespace
{

struct HitArmorBreakMarks final : GameplayEffectDefinition
{
    int 印記數{};
    int 持續幀數{};
    static constexpr std::string_view Name = "命中穿甲印記";
    static constexpr auto Parameters = std::array<Parameter<HitArmorBreakMarks>, 2>{
        {Parameter<HitArmorBreakMarks>{{"印記數", 0, 1000000}, &HitArmorBreakMarks::印記數},
         Parameter<HitArmorBreakMarks>{{"持續幀數", 1, 1000000}, &HitArmorBreakMarks::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::MainProjectileBeforeDamage,
            .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
            .actions
            = {EffectAction{.value = catalogStatus(ApplyStatusAction{.status = BattleStatusKind::SevenStarMark,
                                                                     .durationFrames = 持續幀數,
                                                                     .quantity = SetStatusMarks{.count = 印記數}})}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "命中施加{}枚穿甲印記，持續{}幀；友軍技能命中時忽略50%"
                "防禦並消耗1枚，耗盡時眩暈30幀。再次施加會取代原有印記。",
                印記數,
                持續幀數);
        }
        return std::format(
            "命中施加{}枚穿甲印記，持續{}幀；友軍技能命中時忽略50%防禦並消耗1枚，耗盡時眩暈30幀。", 印記數, 持續幀數);
    }
};

struct ProtectLowestHealth final : GameplayEffectDefinition
{
    int 友軍數{};
    int 承傷上限百分比{};
    static constexpr std::string_view Name = "保護低血友軍";
    static constexpr auto Parameters = std::array<Parameter<ProtectLowestHealth>, 2>{
        {Parameter<ProtectLowestHealth>{{"友軍數", 1, 1000000}, &ProtectLowestHealth::友軍數},
         Parameter<ProtectLowestHealth>{{"承傷上限百分比", 1, 1000000}, &ProtectLowestHealth::承傷上限百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::LowestHpAllies, .count = 友軍數},
            .actions
            = {EffectAction{.value = ApplyStatusAction{
                                .status = BattleStatusKind::SingleHitCapLayer,
                                .quantity = SetStatusTriggerCharges{.count = 1},
                                .behavior = std::make_shared<StatusBehaviorDefinition>(StatusBehaviorDefinition{
                                    .rules = {EffectRule{
                                        .id = EffectRuleId{.value = 1},
                                        .event = EffectEvent::StatusPersistent,
                                        .observation = EffectObservationScope::StatusHolderEventSource,
                                        .selector = EffectSelector{.kind = EffectSelectorKind::StatusHolder},
                                        .actions = {EffectAction{
                                            .value = ModifyDamageAction{
                                                .perspective = DamageModifierPerspective::Incoming,
                                                .stage = DamageModifierStage::Final,
                                                .channel = DamageChannel::All,
                                                .amount = EffectNumber{.base = EffectNumberBase::ApplicationTargetMaxHp,
                                                                       .percent = 承傷上限百分比,
                                                                       .minimum = 1},
                                                .operation = DamageModifierOperation::CapSingleHitAtValue}}}}}})}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招時，生命比例最低的{}名友軍下次承傷不超過各自最大生命的{}%。每人抵擋一次傷害，傷害上限最低為1點。",
                友軍數,
                承傷上限百分比);
        }
        return std::format("出招時，生命比例最低的{}名友軍下次承傷不超過各自最大生命的{}%。", 友軍數, 承傷上限百分比);
    }
};

struct HitSilence final : GameplayEffectDefinition
{
    int 持續幀數{};
    static constexpr std::string_view Name = "命中封內";
    static constexpr auto Parameters = std::array<Parameter<HitSilence>, 1>{
        {Parameter<HitSilence>{{"持續幀數", 1, 1000000}, &HitSilence::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .actions = {EffectAction{.value = ApplyStatusAction{.status = BattleStatusKind::MpBlocked,
                                                                               .durationFrames = 持續幀數}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("命中使目標封內{}幀，期間無法恢復內力。再次施加保留較長持續時間。", 持續幀數);
        }
        return std::format("命中使目標封內{}幀，期間無法恢復內力。", 持續幀數);
    }
};

struct HitStun final : GameplayEffectDefinition
{
    int 持續幀數{};
    static constexpr std::string_view Name = "命中眩暈";
    static constexpr auto Parameters
        = std::array<Parameter<HitStun>, 1>{{Parameter<HitStun>{{"持續幀數", 1, 1000000}, &HitStun::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                       .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                       .actions = {EffectAction{.value = ApplyStatusAction{
                                                    .status = BattleStatusKind::Stun,
                                                    .durationFrames = 持續幀數,
                                                    .reapplication = StatusReapplicationPolicy::KeepLongerDuration}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("命中使目標眩暈{}幀。再次施加保留較長持續時間。", 持續幀數);
        }
        return std::format("命中使目標眩暈{}幀。", 持續幀數);
    }
};

struct CastStunEnemies final : GameplayEffectDefinition
{
    int 持續幀數{};
    static constexpr std::string_view Name = "出招全體眩暈";
    static constexpr auto Parameters = std::array<Parameter<CastStunEnemies>, 1>{
        {Parameter<CastStunEnemies>{{"持續幀數", 1, 1000000}, &CastStunEnemies::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::AttackCommitted,
                       .selector = EffectSelector{.kind = EffectSelectorKind::Enemies},
                       .actions = {EffectAction{.value = ApplyStatusAction{
                                                    .status = BattleStatusKind::Stun,
                                                    .durationFrames = 持續幀數,
                                                    .reapplication = StatusReapplicationPolicy::KeepLongerDuration}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("出招使所有敵人眩暈{}幀。再次施加保留較長持續時間。", 持續幀數);
        }
        return std::format("出招使所有敵人眩暈{}幀。", 持續幀數);
    }
};

struct StackDeathPoisonExplosion final : GameplayEffectDefinition
{
    int 每次層數{};
    int 層數上限{};
    int 半徑格數{};
    int 每星每層傷害{};
    int 中毒次數{};
    int 中毒間隔幀數{};
    int 中毒生命百分比{};
    static constexpr std::string_view Name = "蓄積死亡毒爆";
    static constexpr auto Parameters = std::array<Parameter<StackDeathPoisonExplosion>, 7>{
        {Parameter<StackDeathPoisonExplosion>{{"每次層數", 1, 1000}, &StackDeathPoisonExplosion::每次層數},
         Parameter<StackDeathPoisonExplosion>{{"層數上限", 1, 1000}, &StackDeathPoisonExplosion::層數上限},
         Parameter<StackDeathPoisonExplosion>{{"半徑格數", 1, 1000000}, &StackDeathPoisonExplosion::半徑格數},
         Parameter<StackDeathPoisonExplosion>{{"每星每層傷害", 0, 1000000}, &StackDeathPoisonExplosion::每星每層傷害},
         Parameter<StackDeathPoisonExplosion>{{"中毒次數", 1, 1000}, &StackDeathPoisonExplosion::中毒次數},
         Parameter<StackDeathPoisonExplosion>{{"中毒間隔幀數", 1, 1000000}, &StackDeathPoisonExplosion::中毒間隔幀數},
         Parameter<StackDeathPoisonExplosion>{{"中毒生命百分比", 0, 1000000},
                                              &StackDeathPoisonExplosion::中毒生命百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions = {EffectAction{
                .value = ApplyStatusAction{
                    .status = BattleStatusKind::PoisonExplosion,
                    .quantity = AddStatusLayers{.count = 每次層數, .limit = 層數上限},
                    .behavior = std::make_shared<StatusBehaviorDefinition>(StatusBehaviorDefinition{
                        .rules = {EffectRule{
                            .id = EffectRuleId{.value = 1},
                            .event = EffectEvent::UnitDied,
                            .observation = EffectObservationScope::StatusHolderEventTarget,
                            .selector = EffectSelector{.kind = EffectSelectorKind::UnitsInRadius,
                                                       .radiusTiles = 半徑格數,
                                                       .team = EffectTeamFilter::Enemy},
                            .actions
                            = {EffectAction{.value = DealDamageAction{.amount
                                                                      = EffectNumber{.base = EffectNumberBase::SourceStar,
                                                                                     .percent = 每星每層傷害 * 100,
                                                                                     .statusScale = StatusNumberScale::PerContributionLayer},
                                                                      .kind = BattleDamageKind::Pure}},
                               EffectAction{
                                   .value = ApplyStatusAction{
                                       .durationFrames
                                       = 中毒次數 * 中毒間隔幀數,
                                       .quantity = SetStatusTriggerCharges{.count = 中毒次數},
                                       .reapplication = StatusReapplicationPolicy::ReplaceExistingPoison,
                                       .behavior = std::make_shared<StatusBehaviorDefinition>(StatusBehaviorDefinition{
                                           .rules = {EffectRule{
                                               .id = EffectRuleId{.value = 1},
                                               .event = EffectEvent::FrameAdvanced,
                                               .observation = EffectObservationScope::StatusHolderEventSource,
                                               .selector = EffectSelector{.kind = EffectSelectorKind::StatusHolder},
                                               .intervalFrames = 中毒間隔幀數,
                                               .actions
                                               = {EffectAction{
                                                      .value
                                                      = DealDamageAction{.amount
                                                                         = EffectNumber{.base = EffectNumberBase::TargetCurrentHp,
                                                                                        .percent = 中毒生命百分比,
                                                                                        .minimum = 1},
                                                                         .kind = BattleDamageKind::Poison}},
                                                  EffectAction{.value = ConsumeThisStatusAction{}}}}}})}}}}}})}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "每次出招累積{}層，最多{}層；死亡時逐層引爆，對{}格內敵人每層造成每星{}傷害並施毒。中毒持續{}幀，共{}"
                "次，每次造成目標當前生命{}%傷害。",
                每次層數,
                層數上限,
                半徑格數,
                每星每層傷害,
                中毒次數 * 中毒間隔幀數,
                中毒次數,
                中毒生命百分比);
        }
        return std::format("每次出招累積{}層，最多{}層；死亡時逐層引爆，對{}格內敵人每層造成每星{}傷害並施毒。",
                           每次層數,
                           層數上限,
                           半徑格數,
                           每星每層傷害);
    }
};

struct CastStackDamageBlocks final : GameplayEffectDefinition
{
    int 每次抵擋數{};
    int 抵擋上限{};
    static constexpr std::string_view Name = "出招累積傷害抵擋";
    static constexpr auto Parameters = std::array<Parameter<CastStackDamageBlocks>, 2>{
        {Parameter<CastStackDamageBlocks>{{"每次抵擋數", 1, 1000000}, &CastStackDamageBlocks::每次抵擋數},
         Parameter<CastStackDamageBlocks>{{"抵擋上限", 1, 1000000}, &CastStackDamageBlocks::抵擋上限}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions = {EffectAction{
                .value = ApplyStatusAction{
                    .status = BattleStatusKind::DamageBlockLayer,
                    .quantity = AddDamageBlockCharges{.count = 每次抵擋數, .limit = 抵擋上限},
                    .behavior = std::make_shared<StatusBehaviorDefinition>(StatusBehaviorDefinition{
                        .rules = {EffectRule{.id = EffectRuleId{.value = 1},
                                             .event = EffectEvent::StatusPersistent,
                                             .observation = EffectObservationScope::StatusHolderEventSource,
                                             .selector = EffectSelector{.kind = EffectSelectorKind::StatusHolder},
                                             .actions = {EffectAction{.value = BlockPositiveDamageAction{}}}}}})}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("出招增加{}次傷害抵擋，最多{}次。每次抵擋一次大於零的非處決傷害，此來源獨立累積。",
                               每次抵擋數,
                               抵擋上限);
        }
        return std::format("出招增加{}次傷害抵擋，最多{}次。", 每次抵擋數, 抵擋上限);
    }
};

struct HasteDodgeAfterimages final : GameplayEffectDefinition
{
    int 速度百分比{};
    int 閃避百分比{};
    int 持續幀數{};
    int 敵人數{};
    int 殘影數{};
    int 殘影傷害百分比{};
    static constexpr std::string_view Name = "加速閃避與殘影攻擊";
    static constexpr auto Parameters = std::array<Parameter<HasteDodgeAfterimages>, 6>{
        {Parameter<HasteDodgeAfterimages>{{"速度百分比", -1000000, 1000000}, &HasteDodgeAfterimages::速度百分比},
         Parameter<HasteDodgeAfterimages>{{"閃避百分比", -1000000, 1000000}, &HasteDodgeAfterimages::閃避百分比},
         Parameter<HasteDodgeAfterimages>{{"持續幀數", 1, 1000000}, &HasteDodgeAfterimages::持續幀數},
         Parameter<HasteDodgeAfterimages>{{"敵人數", 1, 1000}, &HasteDodgeAfterimages::敵人數},
         Parameter<HasteDodgeAfterimages>{{"殘影數", 0, 1000000}, &HasteDodgeAfterimages::殘影數},
         Parameter<HasteDodgeAfterimages>{{"殘影傷害百分比", 0, 1000000}, &HasteDodgeAfterimages::殘影傷害百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions
            = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Speed,
                                                           .amount = EffectNumber{.flat = 速度百分比},
                                                           .operation = AttributeOperation::PercentAdd,
                                                           .durationFrames = 持續幀數,
                                                           .stack = EffectStackPolicy::Refresh}},
               EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::DodgeChance,
                                                           .amount = EffectNumber{.flat = 閃避百分比},
                                                           .operation = AttributeOperation::PercentagePointAdd,
                                                           .durationFrames = 持續幀數,
                                                           .stack = EffectStackPolicy::Refresh}},
               EffectAction{
                   .value = ApplyStatusAction{
                       .status = BattleStatusKind::Shadowless,
                       .durationFrames = 持續幀數,
                       .reapplication = StatusReapplicationPolicy::RefreshDuration,
                       .behavior = std::make_shared<StatusBehaviorDefinition>(StatusBehaviorDefinition{
                           .rules = {EffectRule{
                               .id = EffectRuleId{.value = 1},
                               .event = EffectEvent::AttackSpawned,
                               .observation = EffectObservationScope::StatusHolderEventSource,
                               .selector = EffectSelector{.kind = EffectSelectorKind::NearestEnemies, .count = 敵人數},
                               .conditions = {IsRootAttackCondition{}},
                               .actions = {EffectAction{
                                   .value = ModifyAttackAction{
                                       .pattern = AttackPattern{.kind = AttackPatternKind::EchoNearestOthers,
                                                                .projectileCount = 殘影數},
                                       .strengthPct = 殘影傷害百分比,
                                       .mainProjectile = false,
                                       .targets = AttackTargetPolicy::SelectedTargets,
                                       .propagation = CastPropagationPolicy::NoEffectRules,
                                       .addToBaseAttack = true}}}}}})}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招時速度{:+}%、閃避率{:+}%，持續{}幀；期間攻擊向最近{}名敵人各追加{}道殘影，每道{}%"
                "傷害。殘影不觸發追加效果，重複施加刷新時間。",
                速度百分比,
                閃避百分比,
                持續幀數,
                敵人數,
                殘影數,
                殘影傷害百分比);
        }
        return std::format(
            "出招時速度{:+}%、閃避率{:+}%，持續{}幀；期間攻擊向最近{}名敵人各追加{}道殘影，每道{}%傷害。",
            速度百分比,
            閃避百分比,
            持續幀數,
            敵人數,
            殘影數,
            殘影傷害百分比);
    }
};

struct HitHealingBlockSlow final : GameplayEffectDefinition
{
    int 持續幀數{};
    static constexpr std::string_view Name = "命中禁療減速";
    static constexpr auto Parameters = std::array<Parameter<HitHealingBlockSlow>, 1>{
        {Parameter<HitHealingBlockSlow>{{"持續幀數", 1, 1000000}, &HitHealingBlockSlow::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::MainProjectileBeforeDamage,
            .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
            .actions = {EffectAction{.value = catalogStatus(ApplyStatusAction{.status = BattleStatusKind::ColdPoison,
                                                                              .durationFrames = 持續幀數})}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "命中使目標無法恢復生命且速度降低25%，持續{}幀。再次施加取代剩餘時間，兩項效果一同移除。", 持續幀數);
        }
        return std::format("命中使目標無法恢復生命且速度降低25%，持續{}幀。", 持續幀數);
    }
};

struct DetonateAndPoisonEnemies final : GameplayEffectDefinition
{
    int 中毒次數{};
    int 中毒間隔幀數{};
    int 生命傷害百分比{};
    static constexpr std::string_view Name = "全體引毒再施毒";
    static constexpr auto Parameters = std::array<Parameter<DetonateAndPoisonEnemies>, 3>{
        {Parameter<DetonateAndPoisonEnemies>{{"中毒次數", 1, 1000}, &DetonateAndPoisonEnemies::中毒次數},
         Parameter<DetonateAndPoisonEnemies>{{"中毒間隔幀數", 1, 1000000}, &DetonateAndPoisonEnemies::中毒間隔幀數},
         Parameter<DetonateAndPoisonEnemies>{{"生命傷害百分比", 0, 1000000},
                                             &DetonateAndPoisonEnemies::生命傷害百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::Enemies},
            .actions
            = {EffectAction{.value = StateMachineAction{SettleRemainingStatusDamageAction{}}},
               EffectAction{.value = RemoveStatusAction{.statuses = {BattleStatusKind::Poison}}},
               EffectAction{
                   .value = ApplyStatusAction{
                       .durationFrames = 中毒次數 * 中毒間隔幀數,
                       .quantity = SetStatusTriggerCharges{.count = 中毒次數},
                       .reapplication = StatusReapplicationPolicy::ReplaceExistingPoison,
                       .behavior = std::make_shared<StatusBehaviorDefinition>(StatusBehaviorDefinition{
                           .rules = {EffectRule{
                               .id = EffectRuleId{.value = 1},
                               .event = EffectEvent::FrameAdvanced,
                               .observation = EffectObservationScope::StatusHolderEventSource,
                               .selector = EffectSelector{.kind = EffectSelectorKind::StatusHolder},
                               .intervalFrames = 中毒間隔幀數,
                               .actions = {
                                   EffectAction{.value = DealDamageAction{.amount = EffectNumber{.base
                                                                                                 = EffectNumberBase::TargetCurrentHp,
                                                                                                 .percent = 生命傷害百分比,
                                                                                                 .minimum = 1},
                                                                          .kind = BattleDamageKind::Poison}},
                                   EffectAction{.value = ConsumeThisStatusAction{}}}}}})}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招立即結算所有敵人剩餘中毒傷害，再施加持續{}幀的中毒。新中毒每{}幀造成目標當前生命{}%"
                "傷害，最低1點，共{}次；取代原有中毒。",
                中毒次數 * 中毒間隔幀數,
                中毒間隔幀數,
                生命傷害百分比,
                中毒次數);
        }
        return std::format("出招立即結算所有敵人剩餘中毒傷害，再施加持續{}幀的中毒。", 中毒次數 * 中毒間隔幀數);
    }
};

struct HitRestoreVictimMp final : GameplayEffectDefinition
{
    int 觸發次數{};
    int 固定回內{};
    int 每星回內{};
    static constexpr std::string_view Name = "命中使敵攻擊回內";
    static constexpr auto Parameters = std::array<Parameter<HitRestoreVictimMp>, 3>{
        {Parameter<HitRestoreVictimMp>{{"觸發次數", 1, 1000}, &HitRestoreVictimMp::觸發次數},
         Parameter<HitRestoreVictimMp>{{"固定回內", 0, 1000000}, &HitRestoreVictimMp::固定回內},
         Parameter<HitRestoreVictimMp>{{"每星回內", 0, 1000000}, &HitRestoreVictimMp::每星回內}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::MainProjectileBeforeDamage,
            .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
            .actions = {EffectAction{
                .value = catalogStatus(ApplyStatusAction{
                    .status = BattleStatusKind::NeutralizeForce,
                    .quantity = SetStatusTriggerCharges{.count = 觸發次數},
                    .neutralizeMpRecovery = EffectNumber{
                        .base = EffectNumberBase::SourceStar, .flat = 固定回內, .percent = 每星回內 * 100}})}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "命中使敵人之後{}次命中為其攻擊目標回復內力。回復量為{}加每星{}內力；再次施加取代原有效果。",
                觸發次數,
                固定回內,
                每星回內);
        }
        return std::format("命中使敵人之後{}次命中為其攻擊目標回復內力。", 觸發次數);
    }
};

struct AbsorbAndReturnDamage final : GameplayEffectDefinition
{
    int 吸收百分比{};
    int 持續幀數{};
    int 敵人數{};
    int 返還百分比{};
    static constexpr std::string_view Name = "吸收傷害後返還";
    static constexpr auto Parameters = std::array<Parameter<AbsorbAndReturnDamage>, 4>{
        {Parameter<AbsorbAndReturnDamage>{{"吸收百分比", 0, 1000000}, &AbsorbAndReturnDamage::吸收百分比},
         Parameter<AbsorbAndReturnDamage>{{"持續幀數", 1, 1000000}, &AbsorbAndReturnDamage::持續幀數},
         Parameter<AbsorbAndReturnDamage>{{"敵人數", 1, 1000}, &AbsorbAndReturnDamage::敵人數},
         Parameter<AbsorbAndReturnDamage>{{"返還百分比", 0, 1000000}, &AbsorbAndReturnDamage::返還百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions = {EffectAction{.value = StateMachineAction{StartDamageAbsorptionAction{
                                         .slot = EffectStateSlot::AbsorbedDamage,
                                         .absorbedPct = 吸收百分比,
                                         .durationFrames = 持續幀數,
                                         .settleOnSourceDeath = true,
                                         .settlementTarget = EffectSelector{.kind = EffectSelectorKind::Enemies,
                                                                            .count = 敵人數,
                                                                            .tieBreak = EffectTieBreak::BattleRandom},
                                         .returnedPct = 返還百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招後{}幀內吸收受到傷害的{}%，結束時將累積值的{}%以純粹傷害返還給隨機{}"
                "名敵人。來源死亡時立即結算，結算後清空記錄。",
                持續幀數,
                吸收百分比,
                返還百分比,
                敵人數);
        }
        return std::format("出招後{}幀內吸收受到傷害的{}%，結束時將累積值的{}%以純粹傷害返還給隨機{}名敵人。",
                           持續幀數,
                           吸收百分比,
                           返還百分比,
                           敵人數);
    }
};

struct GrowingHitStun final : GameplayEffectDefinition
{
    int 每次層數{};
    int 層數上限{};
    int 基礎幀數{};
    int 每層延長幀數{};
    int 最長幀數{};
    static constexpr std::string_view Name = "出招累積眩暈時間";
    static constexpr auto Parameters = std::array<Parameter<GrowingHitStun>, 5>{
        {Parameter<GrowingHitStun>{{"每次層數", 1, 1000}, &GrowingHitStun::每次層數},
         Parameter<GrowingHitStun>{{"層數上限", 1, 1000}, &GrowingHitStun::層數上限},
         Parameter<GrowingHitStun>{{"基礎幀數", 1, 1000000}, &GrowingHitStun::基礎幀數},
         Parameter<GrowingHitStun>{{"每層延長幀數", 1, 1000000}, &GrowingHitStun::每層延長幀數},
         Parameter<GrowingHitStun>{{"最長幀數", 1, 1000000}, &GrowingHitStun::最長幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
                    .event = EffectEvent::AttackCommitted,
                    .actions = {EffectAction{
                        .value = StateMachineAction{ChangeStateValueAction{
                            .slot = EffectStateSlot::PermanentCastProgress, .delta = 每次層數, .maximum = 層數上限}}}}},
                EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .actions = {EffectAction{
                               .value = ApplyStatusAction{
                                   .status = BattleStatusKind::Stun,
                                   .duration = EffectNumber{.base = EffectNumberBase::StoredStateValue,
                                                            .stateSlot = EffectStateSlot::PermanentCastProgress,
                                                            .flat = 基礎幀數,
                                                            .percent = 每層延長幀數 * 100,
                                                            .maximum = 最長幀數},
                                   .reapplication = StatusReapplicationPolicy::KeepLongerDuration}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "每次出招增加{}層，最多{}層；命中眩暈時間為{}幀，每層再延長{}幀，最長{}幀。再次眩暈保留較長持續時間。",
                每次層數,
                層數上限,
                基礎幀數,
                每層延長幀數,
                最長幀數);
        }
        return std::format("每次出招增加{}層，最多{}層；命中眩暈時間為{}幀，每層再延長{}幀，最長{}幀。",
                           每次層數,
                           層數上限,
                           基礎幀數,
                           每層延長幀數,
                           最長幀數);
    }
};

struct ProtectNextAttack final : GameplayEffectDefinition
{
    int 友軍數{};
    int 持續幀數{};
    static constexpr std::string_view Name = "低血友軍閃避保護";
    static constexpr auto Parameters = std::array<Parameter<ProtectNextAttack>, 2>{
        {Parameter<ProtectNextAttack>{{"友軍數", 1, 1000000}, &ProtectNextAttack::友軍數},
         Parameter<ProtectNextAttack>{{"持續幀數", 1, 1000000}, &ProtectNextAttack::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::AttackCommitted,
                           .selector = EffectSelector{.kind = EffectSelectorKind::LowestHpAllies, .count = 友軍數},
                           .actions = {EffectAction{
                               .value = ApplyStatusAction{
                                   .status = BattleStatusKind::NextAttackMiss,
                                   .durationFrames = 持續幀數,
                                   .quantity = SetStatusTriggerCharges{.count = 1},
                                   .behavior = std::make_shared<StatusBehaviorDefinition>(StatusBehaviorDefinition{
                                       .rules = {EffectRule{
                                           .id = EffectRuleId{.value = 1},
                                           .event = EffectEvent::HitBeforeDamage,
                                           .observation = EffectObservationScope::StatusHolderEventTarget,
                                           .selector = EffectSelector{.kind = EffectSelectorKind::StatusHolder},
                                           .actions = {EffectAction{.value = MakeIncomingAttackMissAction{}}}}}})}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招保護生命比例最低的{}名友軍，持續{}幀，使每人的下一次受擊落空。每名友軍抵擋一次攻擊。",
                友軍數,
                持續幀數);
        }
        return std::format("出招保護生命比例最低的{}名友軍，持續{}幀，使每人的下一次受擊落空。", 友軍數, 持續幀數);
    }
};

struct CastStackDamageAndReduction final : GameplayEffectDefinition
{
    int 每次層數{};
    int 層數上限{};
    int 每層增傷百分比{};
    int 每層承傷百分比{};
    static constexpr std::string_view Name = "出招疊加增傷減傷";
    static constexpr auto Parameters = std::array<Parameter<CastStackDamageAndReduction>, 4>{
        {Parameter<CastStackDamageAndReduction>{{"每次層數", 1, 1000}, &CastStackDamageAndReduction::每次層數},
         Parameter<CastStackDamageAndReduction>{{"層數上限", 1, 1000}, &CastStackDamageAndReduction::層數上限},
         Parameter<CastStackDamageAndReduction>{{"每層增傷百分比", -1000000, 1000000},
                                                &CastStackDamageAndReduction::每層增傷百分比},
         Parameter<CastStackDamageAndReduction>{{"每層承傷百分比", -1000000, 1000000},
                                                &CastStackDamageAndReduction::每層承傷百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions = {EffectAction{
                .value = ApplyStatusAction{
                    .status = BattleStatusKind::BattleSpirit,
                    .quantity = AddStatusLayers{.count = 每次層數, .limit = 層數上限},
                    .behavior = std::make_shared<StatusBehaviorDefinition>(StatusBehaviorDefinition{
                        .rules = {EffectRule{
                            .id = EffectRuleId{.value = 1},
                            .event = EffectEvent::StatusPersistent,
                            .observation = EffectObservationScope::StatusHolderEventSource,
                            .selector = EffectSelector{.kind = EffectSelectorKind::StatusHolder},
                            .actions = {
                                EffectAction{.value = ModifyDamageAction{.amount = EffectNumber{.flat = 每層增傷百分比,
                                                                                                .statusScale = StatusNumberScale::PerContributionLayer},
                                                                         .operation
                                                                         = DamageModifierOperation::PercentAdd}},
                                EffectAction{
                                    .value = ModifyDamageAction{
                                        .perspective = DamageModifierPerspective::Incoming,
                                        .channel = DamageChannel::All,
                                        .amount = EffectNumber{.flat = 每層承傷百分比,
                                                               .statusScale = StatusNumberScale::PerContributionLayer},
                                        .operation = DamageModifierOperation::PercentAdd}}}}}})}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("每次出招增加{}層，最多{}層；每層技能傷害{:+}%、承受傷害{:+}%。此來源獨立累積層數。",
                               每次層數,
                               層數上限,
                               每層增傷百分比,
                               每層承傷百分比);
        }
        return std::format("每次出招增加{}層，最多{}層；每層技能傷害{:+}%、承受傷害{:+}%。",
                           每次層數,
                           層數上限,
                           每層增傷百分比,
                           每層承傷百分比);
    }
};

struct HitVulnerableHealingPenalty final : GameplayEffectDefinition
{
    int 持續幀數{};
    static constexpr std::string_view Name = "命中易傷減療";
    static constexpr auto Parameters = std::array<Parameter<HitVulnerableHealingPenalty>, 1>{
        {Parameter<HitVulnerableHealingPenalty>{{"持續幀數", 1, 1000000}, &HitVulnerableHealingPenalty::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::MainProjectileBeforeDamage,
            .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
            .actions = {EffectAction{.value = catalogStatus(ApplyStatusAction{.status = BattleStatusKind::WitheredBone,
                                                                              .durationFrames = 持續幀數})}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "命中使目標承受傷害增加25%、受到治療減少75%，持續{}幀。再次施加取代剩餘時間，兩項效果一同移除。",
                持續幀數);
        }
        return std::format("命中使目標承受傷害增加25%、受到治療減少75%，持續{}幀。", 持續幀數);
    }
};

struct HitCancelNextCast final : GameplayEffectDefinition
{
    int 次數{};
    static constexpr std::string_view Name = "命中使下次出招落空";
    static constexpr auto Parameters = std::array<Parameter<HitCancelNextCast>, 1>{
        {Parameter<HitCancelNextCast>{{"次數", 1, 1000}, &HitCancelNextCast::次數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .actions = {EffectAction{.value = catalogStatus(ApplyStatusAction{
                                                        .status = BattleStatusKind::Blinded,
                                                        .quantity = SetStatusTriggerCharges{.count = 次數}})}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("命中使目標之後{}次出招落空。再次施加取代原有效果。", 次數);
        }
        return std::format("命中使目標之後{}次出招落空。", 次數);
    }
};

struct ReceivedHitStunAttacker final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 持續幀數{};
    static constexpr std::string_view Name = "受擊眩暈攻擊者";
    static constexpr auto Parameters = std::array<Parameter<ReceivedHitStunAttacker>, 2>{
        {Parameter<ReceivedHitStunAttacker>{{"機率百分比", 0, 100}, &ReceivedHitStunAttacker::機率百分比},
         Parameter<ReceivedHitStunAttacker>{{"持續幀數", 1, 1000000}, &ReceivedHitStunAttacker::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::DamageResolved,
                           .selector = EffectSelector{.kind = EffectSelectorKind::SourceUnit},
                           .conditions = {DamagePerspectiveCondition{.perspective = DamagePerspective::Received},
                                          AcceptedHitCondition{},
                                          TargetNotInvincibleCondition{}},
                           .chancePct = 機率百分比,
                           .actions = {EffectAction{.value = ApplyStatusAction{
                                                        .status = BattleStatusKind::Stun,
                                                        .durationFrames = 持續幀數,
                                                        .reapplication = StatusReapplicationPolicy::ExtendDuration}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "受擊後有{}%機率使攻擊者眩暈{}幀。須為有效命中且目標非無敵，重複眩暈延長時間。", 機率百分比, 持續幀數);
        }
        return std::format("受擊後有{}%機率使攻擊者眩暈{}幀。", 機率百分比, 持續幀數);
    }
};

struct InitialStatusShield final : GameplayEffectDefinition
{
    int 護盾點數{};
    static constexpr std::string_view Name = "開場狀態護盾";
    static constexpr auto Parameters = std::array<Parameter<InitialStatusShield>, 1>{
        {Parameter<InitialStatusShield>{{"護盾點數", 0, 1000000}, &InitialStatusShield::護盾點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::StatusShield,
                                                                   .amount = EffectNumber{.flat = 護盾點數},
                                                                   .kind = ResourceChangeKind::RefreshToAtLeast}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("開場時狀態護盾至少為{}。", 護盾點數); }
};

struct CastInvincibility final : GameplayEffectDefinition
{
    int 持續幀數{};
    static constexpr std::string_view Name = "出招無敵";
    static constexpr auto Parameters = std::array<Parameter<CastInvincibility>, 1>{
        {Parameter<CastInvincibility>{{"持續幀數", 1, 1000000}, &CastInvincibility::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::InvincibilityFrames,
                                                                   .amount = EffectNumber{.flat = 持續幀數},
                                                                   .kind = ResourceChangeKind::RefreshToAtLeast}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("出招獲得{}幀無敵。已有較長無敵時保留較長時間。", 持續幀數);
        }
        return std::format("出招獲得{}幀無敵。", 持續幀數);
    }
};

struct HitPoison final : GameplayEffectDefinition
{
    int 中毒次數{};
    int 中毒間隔幀數{};
    int 生命傷害百分比{};
    static constexpr std::string_view Name = "命中施毒";
    static constexpr auto Parameters = std::array<Parameter<HitPoison>, 3>{
        {Parameter<HitPoison>{{"中毒次數", 1, 1000}, &HitPoison::中毒次數},
         Parameter<HitPoison>{{"中毒間隔幀數", 1, 1000000}, &HitPoison::中毒間隔幀數},
         Parameter<HitPoison>{{"生命傷害百分比", 0, 1000000}, &HitPoison::生命傷害百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::HitBeforeDamage,
            .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
            .actions = {EffectAction{
                .value = ApplyStatusAction{
                    .durationFrames = 中毒次數 * 中毒間隔幀數,
                    .quantity = SetStatusTriggerCharges{.count = 中毒次數},
                    .reapplication = StatusReapplicationPolicy::KeepHigherDamage,
                    .poisonSameEventMerge = PoisonSameEventMerge::SumDamagePercent,
                    .behavior = std::make_shared<StatusBehaviorDefinition>(StatusBehaviorDefinition{
                        .rules = {EffectRule{
                            .id = EffectRuleId{.value = 1},
                            .event = EffectEvent::FrameAdvanced,
                            .observation = EffectObservationScope::StatusHolderEventSource,
                            .selector = EffectSelector{.kind = EffectSelectorKind::StatusHolder},
                            .intervalFrames = 中毒間隔幀數,
                            .actions
                            = {EffectAction{.value = DealDamageAction{.amount
                                                                      = EffectNumber{.base = EffectNumberBase::TargetCurrentHp,
                                                                                     .percent = 生命傷害百分比,
                                                                                     .minimum = 1},
                                                                      .kind = BattleDamageKind::Poison}},
                               EffectAction{.value = ConsumeThisStatusAction{}}}}}})}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "命中施加中毒，持續{}幀；每{}幀造成目標當前生命{}%傷害，共{}"
                "次。每次最低1傷害；同來源同事件的毒傷合計後與現有相容中毒取較高。",
                中毒次數 * 中毒間隔幀數,
                中毒間隔幀數,
                生命傷害百分比,
                中毒次數);
        }
        return std::format("命中施加中毒，持續{}幀；每{}幀造成目標當前生命{}%傷害，共{}次。",
                           中毒次數 * 中毒間隔幀數,
                           中毒間隔幀數,
                           生命傷害百分比,
                           中毒次數);
    }
};

struct KillInvincibility final : GameplayEffectDefinition
{
    int 持續幀數{};
    static constexpr std::string_view Name = "擊殺無敵";
    static constexpr auto Parameters = std::array<Parameter<KillInvincibility>, 1>{
        {Parameter<KillInvincibility>{{"持續幀數", 1, 1000000}, &KillInvincibility::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::DamageResolved,
            .conditions = {DamagePerspectiveCondition{}, DamageKilledTargetCondition{}},
            .actions = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::InvincibilityFrames,
                                                                   .amount = EffectNumber{.flat = 持續幀數},
                                                                   .kind = ResourceChangeKind::RefreshToAtLeast}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("擊殺敵人後獲得{}幀無敵。已有較長無敵時保留較長時間。", 持續幀數);
        }
        return std::format("擊殺敵人後獲得{}幀無敵。", 持續幀數);
    }
};

struct ChanceHitSilence final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 持續幀數{};
    static constexpr std::string_view Name = "機率命中封內";
    static constexpr auto Parameters = std::array<Parameter<ChanceHitSilence>, 2>{
        {Parameter<ChanceHitSilence>{{"機率百分比", 0, 100}, &ChanceHitSilence::機率百分比},
         Parameter<ChanceHitSilence>{{"持續幀數", 1, 1000000}, &ChanceHitSilence::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::DamageResolved,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .conditions
                           = {DamagePerspectiveCondition{}, AcceptedHitCondition{}, TargetNotInvincibleCondition{}},
                           .chancePct = 機率百分比,
                           .actions = {EffectAction{.value = ApplyStatusAction{.status = BattleStatusKind::MpBlocked,
                                                                               .durationFrames = 持續幀數}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "有效命中非無敵目標後，有{}%機率使其封內{}幀，期間無法恢復內力。再次施加保留較長持續時間。",
                機率百分比,
                持續幀數);
        }
        return std::format("有效命中非無敵目標後，有{}%機率使其封內{}幀，期間無法恢復內力。", 機率百分比, 持續幀數);
    }
};

struct ChanceHitStun final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 持續幀數{};
    static constexpr std::string_view Name = "機率命中眩暈";
    static constexpr auto Parameters = std::array<Parameter<ChanceHitStun>, 2>{
        {Parameter<ChanceHitStun>{{"機率百分比", 0, 100}, &ChanceHitStun::機率百分比},
         Parameter<ChanceHitStun>{{"持續幀數", 1, 1000000}, &ChanceHitStun::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::DamageResolved,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .conditions = {DamagePerspectiveCondition{},
                                          DamageOriginIsAttackCondition{},
                                          IsMainProjectileCondition{},
                                          AcceptedHitCondition{},
                                          TargetNotInvincibleCondition{}},
                           .chancePct = 機率百分比,
                           .actions = {EffectAction{.value = ApplyStatusAction{
                                                        .status = BattleStatusKind::Stun,
                                                        .durationFrames = 持續幀數,
                                                        .reapplication = StatusReapplicationPolicy::ExtendDuration}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "技能有效命中非無敵目標後，有{}%機率使其眩暈{}幀。重複眩暈延長時間。", 機率百分比, 持續幀數);
        }
        return std::format("技能有效命中非無敵目標後，有{}%機率使其眩暈{}幀。", 機率百分比, 持續幀數);
    }
};

struct ChanceHitBleed final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 層數{};
    int 目標層數上限{};
    static constexpr std::string_view Name = "機率命中流血";
    static constexpr auto Parameters = std::array<Parameter<ChanceHitBleed>, 3>{
        {Parameter<ChanceHitBleed>{{"機率百分比", 0, 100}, &ChanceHitBleed::機率百分比},
         Parameter<ChanceHitBleed>{{"層數", 1, 1000}, &ChanceHitBleed::層數},
         Parameter<ChanceHitBleed>{{"目標層數上限", 1, 1000}, &ChanceHitBleed::目標層數上限}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::DamageResolved,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .conditions = {DamagePerspectiveCondition{},
                                          DamageOriginIsAttackCondition{},
                                          AcceptedHitCondition{.requirePositiveDamage = true},
                                          TargetNotInvincibleCondition{}},
                           .chancePct = 機率百分比,
                           .actions = {EffectAction{.value = catalogStatus(ApplyStatusAction{
                                                        .status = BattleStatusKind::Bleed,
                                                        .quantity = AddSharedStatusLayers{
                                                            .count = 層數, .targetTotalLimit = 目標層數上限}})}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "技能造成生命傷害後，有{}%機率施加{}層流血，目標最多{}層。流血每層每10幀造成目標最大生命1%"
                "傷害，每次最低1點；無敵目標不受影響。",
                機率百分比,
                層數,
                目標層數上限);
        }
        return std::format("技能造成生命傷害後，有{}%機率施加{}層流血，目標最多{}層。", 機率百分比, 層數, 目標層數上限);
    }
};

struct DamageInvincibility final : GameplayEffectDefinition
{
    int 持續幀數{};
    static constexpr std::string_view Name = "受傷短暫無敵";
    static constexpr auto Parameters = std::array<Parameter<DamageInvincibility>, 1>{
        {Parameter<DamageInvincibility>{{"持續幀數", 1, 1000000}, &DamageInvincibility::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::DamageResolved,
            .conditions = {DamagePerspectiveCondition{.perspective = DamagePerspective::Received},
                           AcceptedHitCondition{.requirePositiveDamage = true}},
            .actions = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::InvincibilityFrames,
                                                                   .amount = EffectNumber{.flat = 持續幀數},
                                                                   .kind = ResourceChangeKind::Grant}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("受到生命傷害後，增加{}幀無敵。", 持續幀數); }
};

struct InitialDamageBlocks final : GameplayEffectDefinition
{
    int 抵擋次數{};
    static constexpr std::string_view Name = "開場傷害抵擋";
    static constexpr auto Parameters = std::array<Parameter<InitialDamageBlocks>, 1>{
        {Parameter<InitialDamageBlocks>{{"抵擋次數", 1, 1000}, &InitialDamageBlocks::抵擋次數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{
                .value = ApplyStatusAction{
                    .status = BattleStatusKind::DamageBlockLayer,
                    .quantity = SetDamageBlockCharges{.count = 抵擋次數},
                    .behavior = std::make_shared<StatusBehaviorDefinition>(StatusBehaviorDefinition{
                        .rules = {EffectRule{.id = EffectRuleId{.value = 1},
                                             .event = EffectEvent::StatusPersistent,
                                             .observation = EffectObservationScope::StatusHolderEventSource,
                                             .selector = EffectSelector{.kind = EffectSelectorKind::StatusHolder},
                                             .actions = {EffectAction{.value = BlockPositiveDamageAction{}}}}}})}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("開場獲得{}次傷害抵擋。每次抵擋一次大於零的非處決傷害。", 抵擋次數);
        }
        return std::format("開場獲得{}次傷害抵擋。", 抵擋次數);
    }
};

struct PeriodicInvincibility final : GameplayEffectDefinition
{
    int 間隔幀數{};
    int 持續幀數{};
    static constexpr std::string_view Name = "定時無敵";
    static constexpr auto Parameters = std::array<Parameter<PeriodicInvincibility>, 2>{
        {Parameter<PeriodicInvincibility>{{"間隔幀數", 1, 1000000}, &PeriodicInvincibility::間隔幀數},
         Parameter<PeriodicInvincibility>{{"持續幀數", 1, 1000000}, &PeriodicInvincibility::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::FrameAdvanced,
            .intervalFrames = 間隔幀數,
            .actions = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::InvincibilityFrames,
                                                                   .amount = EffectNumber{.flat = 持續幀數},
                                                                   .kind = ResourceChangeKind::RefreshToAtLeast}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("每{}幀獲得{}幀無敵。已有較長無敵時保留較長時間。", 間隔幀數, 持續幀數);
        }
        return std::format("每{}幀獲得{}幀無敵。", 間隔幀數, 持續幀數);
    }
};

struct PreventLethalDamage final : GameplayEffectDefinition
{
    int 無敵幀數{};
    static constexpr std::string_view Name = "首次致命傷害保命";
    static constexpr auto Parameters = std::array<Parameter<PreventLethalDamage>, 1>{
        {Parameter<PreventLethalDamage>{{"無敵幀數", 1, 1000000}, &PreventLethalDamage::無敵幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions = {EffectAction{
                               .value = StateMachineAction{PreventDeathAction{.invincibilityFrames = 無敵幀數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    { return std::format("首次受到致命傷害時保留1生命，並獲得{}幀無敵。", 無敵幀數); }
};

}    // namespace

void appendStatusEffects(std::vector<GameplayEffectRegistration>& entries)
{
    entries.push_back(registration<HitArmorBreakMarks>());
    entries.push_back(registration<ProtectLowestHealth>());
    entries.push_back(registration<HitSilence>());
    entries.push_back(registration<HitStun>());
    entries.push_back(registration<CastStunEnemies>());
    entries.push_back(registration<StackDeathPoisonExplosion>());
    entries.push_back(registration<CastStackDamageBlocks>());
    entries.push_back(registration<HasteDodgeAfterimages>());
    entries.push_back(registration<HitHealingBlockSlow>());
    entries.push_back(registration<DetonateAndPoisonEnemies>());
    entries.push_back(registration<HitRestoreVictimMp>());
    entries.push_back(registration<AbsorbAndReturnDamage>());
    entries.push_back(registration<GrowingHitStun>());
    entries.push_back(registration<ProtectNextAttack>());
    entries.push_back(registration<CastStackDamageAndReduction>());
    entries.push_back(registration<HitVulnerableHealingPenalty>());
    entries.push_back(registration<HitCancelNextCast>());
    entries.push_back(registration<ReceivedHitStunAttacker>());
    entries.push_back(registration<InitialStatusShield>());
    entries.push_back(registration<CastInvincibility>());
    entries.push_back(registration<HitPoison>());
    entries.push_back(registration<KillInvincibility>());
    entries.push_back(registration<ChanceHitSilence>());
    entries.push_back(registration<ChanceHitStun>());
    entries.push_back(registration<ChanceHitBleed>());
    entries.push_back(registration<DamageInvincibility>());
    entries.push_back(registration<InitialDamageBlocks>());
    entries.push_back(registration<PeriodicInvincibility>());
    entries.push_back(registration<PreventLethalDamage>());
}

}    // namespace KysChess::GameplayEffects
