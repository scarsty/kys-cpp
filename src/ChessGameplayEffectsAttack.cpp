#include "ChessGameplayEffectInternal.h"

namespace KysChess::GameplayEffects
{
namespace
{

struct SideAttacks final : GameplayEffectDefinition
{
    int 側翼數{};
    int 展開角度{};
    int 傷害百分比{};
    static constexpr std::string_view Name = "追加側翼攻擊";
    static constexpr auto Parameters = std::array<Parameter<SideAttacks>, 3>{
        {Parameter<SideAttacks>{{"側翼數", 0, 1000000}, &SideAttacks::側翼數},
         Parameter<SideAttacks>{{"展開角度", 0, 1000000}, &SideAttacks::展開角度},
         Parameter<SideAttacks>{{"傷害百分比", 0, 1000000}, &SideAttacks::傷害百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::AttackCommitted,
                           .selector = EffectSelector{.kind = EffectSelectorKind::OriginalAttackTarget},
                           .actions = {EffectAction{
                               .value = ModifyAttackAction{.pattern = AttackPattern{.kind = AttackPatternKind::Flanks,
                                                                                    .projectileCount = 側翼數,
                                                                                    .spreadDegrees = 展開角度},
                                                           .strengthPct = 傷害百分比,
                                                           .propagation = CastPropagationPolicy::SourceHitRulesOnly,
                                                           .addToBaseAttack = true}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招時追加{}道側翼攻擊，每道{}%傷害，展開{}度。側翼可觸發此來源的命中效果，不觸發其他追加攻擊。",
                側翼數,
                傷害百分比,
                展開角度);
        }
        return std::format("出招：{}道側翼攻擊，各{}%傷害，展開{}度", 側翼數, 傷害百分比, 展開角度);
    }
};

struct PiercingFan final : GameplayEffectDefinition
{
    int 彈道數{};
    int 展開角度{};
    int 傷害百分比{};
    static constexpr std::string_view Name = "扇形貫穿攻擊";
    static constexpr auto Parameters = std::array<Parameter<PiercingFan>, 3>{
        {Parameter<PiercingFan>{{"彈道數", 1, 1000}, &PiercingFan::彈道數},
         Parameter<PiercingFan>{{"展開角度", 0, 1000000}, &PiercingFan::展開角度},
         Parameter<PiercingFan>{{"傷害百分比", 0, 1000000}, &PiercingFan::傷害百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::CastPlanned,
                           .selector = EffectSelector{.kind = EffectSelectorKind::OriginalAttackTarget},
                           .actions = {EffectAction{.value = ModifyAttackAction{
                                                        .pattern = AttackPattern{.kind = AttackPatternKind::Fan,
                                                                                 .projectileCount = 彈道數,
                                                                                 .spreadDegrees = 展開角度},
                                                        .strengthPct = 傷害百分比,
                                                        .through = true,
                                                        .sameTargetHitLimit = 1,
                                                        .propagation = CastPropagationPolicy::SourceHitRulesOnly}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "改為{}道貫穿攻擊，每道{}%傷害，展開{}度；每名敵人最多命中一次。可觸發此來源的命中效果。",
                彈道數,
                傷害百分比,
                展開角度);
        }
        return std::format(
            "改為{}道貫穿攻擊，各{}%傷害，展開{}度；每敵至多命中1次", 彈道數, 傷害百分比, 展開角度);
    }
};

struct CopyLivingAttack final : GameplayEffectDefinition
{
    int 來源數{};
    static constexpr std::string_view Name = "複製存活角色攻擊";
    static constexpr auto Parameters = std::array<Parameter<CopyLivingAttack>, 1>{
        {Parameter<CopyLivingAttack>{{"來源數", 1, 1000000}, &CopyLivingAttack::來源數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::UltimateCommitted,
            .actions = {EffectAction{
                .value = StateMachineAction{CopyAttackDefinitionAction{
                    .sourceUnits = EffectSelector{.kind = EffectSelectorKind::AllLivingUnits,
                                                  .tieBreak = EffectTieBreak::BattleRandom,
                                                  .excludeOwner = true},
                    .filter = CopiedMagicFilter{.conditions = {CopiedMagicCondition::HasUltimateAttackDefinition,
                                                               CopiedMagicCondition::ExcludesRecursiveEffects}},
                    .copyCount = 來源數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "施放絕招時，隨機複製其他存活角色的{}"
                "個絕招攻擊。只複製攻擊本身，不複製其絕招效果；排除複製來源，避免遞迴複製。",
                來源數);
        }
        return std::format("絕招：追加隨機其他存活角色的{}個絕招攻擊，不含絕招效果", 來源數);
    }
};

struct RangedAttack final : GameplayEffectDefinition
{
    static constexpr std::string_view Name = "武功遠程化";
    static constexpr auto Parameters = std::array<Parameter<RangedAttack>, 0>{{}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::CastPlanned,
                           .actions = {EffectAction{.value = ModifyCastAction{.rangeMode = CastRangeMode::Ranged}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override { return "武功改為遠程攻擊。";
    }
};

struct HitIgnoreDefence final : GameplayEffectDefinition
{
    int 忽略防禦百分比{};
    static constexpr std::string_view Name = "命中忽略防禦";
    static constexpr auto Parameters = std::array<Parameter<HitIgnoreDefence>, 1>{
        {Parameter<HitIgnoreDefence>{{"忽略防禦百分比", 0, 1000000}, &HitIgnoreDefence::忽略防禦百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .actions = {EffectAction{.value = ModifyDamageAction{
                                                        .amount = EffectNumber{.flat = 忽略防禦百分比},
                                                        .operation = DamageModifierOperation::IgnoreDefensePercent}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("技能命中：忽略{}%防禦", 忽略防禦百分比);
        return std::format("技能命中忽略目標{}%防禦。", 忽略防禦百分比);
    }
};

struct HitFlatSkillDamage final : GameplayEffectDefinition
{
    int 傷害點數{};
    static constexpr std::string_view Name = "命中技能固定加傷";
    static constexpr auto Parameters = std::array<Parameter<HitFlatSkillDamage>, 1>{
        {Parameter<HitFlatSkillDamage>{{"傷害點數", 0, 1000000}, &HitFlatSkillDamage::傷害點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .actions
                           = {EffectAction{.value = ModifyDamageAction{.amount = EffectNumber{.flat = 傷害點數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("技能命中：固定傷害+{}點", 傷害點數);
        return std::format("技能命中在計算防禦前附加{}點傷害。", 傷害點數);
    }
};

struct MissingHealthPureDamage final : GameplayEffectDefinition
{
    int 攻擊轉換百分比{};
    static constexpr std::string_view Name = "失血追加純粹傷害";
    static constexpr auto Parameters = std::array<Parameter<MissingHealthPureDamage>, 1>{
        {Parameter<MissingHealthPureDamage>{{"攻擊轉換百分比", 0, 1000000}, &MissingHealthPureDamage::攻擊轉換百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .actions = {EffectAction{
                               .value = DealDamageAction{
                                   .amount = EffectNumber{.base = EffectNumberBase::SourceAttack,
                                                          .multiplierBase = EffectNumberBase::SourceMissingHpRatio,
                                                          .percent = 攻擊轉換百分比},
                                   .kind = BattleDamageKind::Pure}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("命中：純粹加傷=攻×失血比例×{}%", 攻擊轉換百分比);
        return std::format("命中追加純粹傷害，等於攻擊×已損生命比例×{}%。", 攻擊轉換百分比);
    }
};

struct ReceivedDamageCharge final : GameplayEffectDefinition
{
    int 傷害轉換百分比{};
    static constexpr std::string_view Name = "承傷蓄力加傷";
    static constexpr auto Parameters = std::array<Parameter<ReceivedDamageCharge>, 1>{
        {Parameter<ReceivedDamageCharge>{{"傷害轉換百分比", 0, 1000000}, &ReceivedDamageCharge::傷害轉換百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::DamageResolved,
                           .conditions = {DamagePerspectiveCondition{.perspective = DamagePerspective::Received},
                                          DamageOriginIsAttackCondition{}},
                           .actions = {EffectAction{.value = StateMachineAction{RecordMaximumDamageAction{}}}}},
                EffectRule{.event = EffectEvent::AttackCommitted,
                           .actions = {EffectAction{.value = StateMachineAction{TransferStateValueAction{
                                                        .destinationSlot = EffectStateSlot::CastMaximumHpDamage}}}}},
                EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .actions = {EffectAction{
                               .value = StateMachineAction{ConsumeRecordedMaximumAction{
                                   .slot = EffectStateSlot::CastMaximumHpDamage, .percent = 傷害轉換百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "記錄受到的最大單次技能生命傷害，下次出招命中時追加記錄值{}%"
                "的純粹傷害。出招時取走記錄，此次出招首次命中後消耗；護盾吸收的傷害不計入。各來源獨立記錄。",
                傷害轉換百分比);
        }
        return std::format("下次出招命中：追加最大單次技能承受血傷{}%純粹傷害", 傷害轉換百分比);
    }
};

struct ChanceExecuteWounded final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 生命門檻百分比{};
    int 處決生命百分比{};
    static constexpr std::string_view Name = "低血機率處決";
    static constexpr auto Parameters = std::array<Parameter<ChanceExecuteWounded>, 3>{
        {Parameter<ChanceExecuteWounded>{{"機率百分比", 0, 100}, &ChanceExecuteWounded::機率百分比},
         Parameter<ChanceExecuteWounded>{{"生命門檻百分比", 0, 1000000}, &ChanceExecuteWounded::生命門檻百分比},
         Parameter<ChanceExecuteWounded>{{"處決生命百分比", 0, 1000000}, &ChanceExecuteWounded::處決生命百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .conditions = {TargetHpRatioAtMostCondition{.percent = 生命門檻百分比}},
                           .chancePct = 機率百分比,
                           .activationLimit = EffectActivationLimit{.maxEvaluations = 1},
                           .actions = {EffectAction{
                               .value = DealDamageAction{.amount = EffectNumber{.base = EffectNumberBase::TargetMaxHp,
                                                                                .percent = 處決生命百分比},
                                                         .kind = BattleDamageKind::Execute}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "命中生命不高於{}%的敵人時，有{}%機率造成其最大生命{}%的處決傷害。同一次出招對每名敵人只判定一次。",
                生命門檻百分比,
                機率百分比,
                處決生命百分比);
        }
        return std::format("命中：血量≤{}%敵人，{}%機率處決其血上限{}%",
                           生命門檻百分比,
                           機率百分比,
                           處決生命百分比);
    }
};

struct TargetSquarePureDamage final : GameplayEffectDefinition
{
    int 每星傷害{};
    int 方形邊長{};
    static constexpr std::string_view Name = "目標周圍純粹傷害";
    static constexpr auto Parameters = std::array<Parameter<TargetSquarePureDamage>, 2>{
        {Parameter<TargetSquarePureDamage>{{"每星傷害", 0, 1000000}, &TargetSquarePureDamage::每星傷害},
         Parameter<TargetSquarePureDamage>{{"方形邊長", 1, 1000000}, &TargetSquarePureDamage::方形邊長}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::OriginalAttackTarget},
            .actions
            = {EffectAction{.value = DealDamageAction{
                                .amount = EffectNumber{.base = EffectNumberBase::SourceStar, .percent = 每星傷害 * 100},
                                .kind = BattleDamageKind::Pure,
                                .area = DamageArea{.kind = DamageAreaKind::Square, .squareSideTiles = 方形邊長},
                                .perCast = PerCastHitPolicy{.perTargetLimit = 1}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("出招對目標周圍邊長{}格的方形區域造成每星{}純粹傷害。同次出招對每名目標最多生效一次。",
                               方形邊長,
                               每星傷害);
        }
        return std::format("出招：目標周圍方形邊長{}格，每星{}純粹傷害", 方形邊長, 每星傷害);
    }
};

struct ClearEnemyProjectiles final : GameplayEffectDefinition
{
    int 清除半徑百分比{};
    static constexpr std::string_view Name = "貫穿清除敵彈";
    static constexpr auto Parameters = std::array<Parameter<ClearEnemyProjectiles>, 1>{
        {Parameter<ClearEnemyProjectiles>{{"清除半徑百分比", 1, 1000000}, &ClearEnemyProjectiles::清除半徑百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::AttackCommitted,
                       .selector = EffectSelector{.kind = EffectSelectorKind::OriginalAttackTarget},
                       .actions = {EffectAction{
                           .value = ModifyAttackAction{.through = true, .projectileClearRadiusPct = 清除半徑百分比}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("攻擊貫穿敵人，沿途清除敵方彈道，清除半徑為命中半徑的{}%。可以清除敵方絕招彈道。",
                               清除半徑百分比);
        }
        return std::format("攻擊貫穿、清除敵方彈道，清除半徑=命中半徑{}%", 清除半徑百分比);
    }
};

struct DelayedSameTargetAttacks final : GameplayEffectDefinition
{
    int 追加次數{};
    int 間隔幀數{};
    int 傷害百分比{};
    static constexpr std::string_view Name = "延遲追加連擊";
    static constexpr auto Parameters = std::array<Parameter<DelayedSameTargetAttacks>, 3>{
        {Parameter<DelayedSameTargetAttacks>{{"追加次數", 1, 1000}, &DelayedSameTargetAttacks::追加次數},
         Parameter<DelayedSameTargetAttacks>{{"間隔幀數", 1, 1000000}, &DelayedSameTargetAttacks::間隔幀數},
         Parameter<DelayedSameTargetAttacks>{{"傷害百分比", 0, 1000000}, &DelayedSameTargetAttacks::傷害百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::OriginalAttackTarget},
            .actions = {EffectAction{
                .value = ModifyAttackAction{.pattern = AttackPattern{.kind = AttackPatternKind::SamePointSequence,
                                                                     .projectileCount = 追加次數,
                                                                     .intervalFrames = 間隔幀數},
                                            .strengthPct = 傷害百分比,
                                            .targets = AttackTargetPolicy::SamePoint,
                                            .propagation = CastPropagationPolicy::SourceHitRulesOnly,
                                            .addToBaseAttack = true}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招後每隔{}幀向相同落點追加攻擊，共{}次，每次{}%傷害。追加攻擊可觸發此來源的命中效果。",
                間隔幀數,
                追加次數,
                傷害百分比);
        }
        return std::format("出招：每{}幀追擊同一落點，共{}次，各{}%傷害", 間隔幀數, 追加次數, 傷害百分比);
    }
};

struct AttackOrdinalStun final : GameplayEffectDefinition
{
    int 第幾道攻擊{};
    int 持續幀數{};
    static constexpr std::string_view Name = "指定連擊眩暈";
    static constexpr auto Parameters = std::array<Parameter<AttackOrdinalStun>, 2>{
        {Parameter<AttackOrdinalStun>{{"第幾道攻擊", 1, 1000}, &AttackOrdinalStun::第幾道攻擊},
         Parameter<AttackOrdinalStun>{{"持續幀數", 1, 1000000}, &AttackOrdinalStun::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                       .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                       .conditions = {AttackOrdinalEqualsCondition{.ordinal = 第幾道攻擊 - 1}},
                       .actions = {EffectAction{.value = ApplyStatusAction{
                                                    .status = BattleStatusKind::Stun,
                                                    .durationFrames = 持續幀數,
                                                    .reapplication = StatusReapplicationPolicy::KeepLongerDuration}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("第{}道攻擊命中使目標眩暈{}幀。再次眩暈保留較長時間。", 第幾道攻擊, 持續幀數);
        }
        return std::format("第{}道命中：眩暈{}幀", 第幾道攻擊, 持續幀數);
    }
};

struct MatchingAllyFollowup final : GameplayEffectDefinition
{
    int 友軍傷害百分比{};
    int 自身傷害百分比{};
    static constexpr std::string_view Name = "同武功友軍合擊";
    static constexpr auto Parameters = std::array<Parameter<MatchingAllyFollowup>, 2>{
        {Parameter<MatchingAllyFollowup>{{"友軍傷害百分比", 0, 1000000}, &MatchingAllyFollowup::友軍傷害百分比},
         Parameter<MatchingAllyFollowup>{{"自身傷害百分比", 0, 1000000}, &MatchingAllyFollowup::自身傷害百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::OriginalAttackTarget},
            .actions = {EffectAction{
                .value = std::make_shared<ConditionalEffectAction>(ConditionalEffectAction{
                    .conditions = {OtherLivingAllyUsesBoundMagicCondition{}},
                    .whenTrue = {EffectAction{
                        .value = ModifyAttackAction{.strengthPct = 友軍傷害百分比,
                                                    .targets = AttackTargetPolicy::SameTarget,
                                                    .propagation = CastPropagationPolicy::SuppressUltimateRules,
                                                    .addToBaseAttack = true,
                                                    .source = EffectSelector{.kind = EffectSelectorKind::Allies,
                                                                             .count = 1,
                                                                             .excludeOwner = true,
                                                                             .requiredBoundMagic = true}}}},
                    .whenFalse = {EffectAction{
                        .value = ModifyAttackAction{.strengthPct = 自身傷害百分比,
                                                    .mainProjectile = false,
                                                    .targets = AttackTargetPolicy::SameTarget,
                                                    .propagation = CastPropagationPolicy::SuppressUltimateRules,
                                                    .addToBaseAttack = true}}}})}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招時，若有存活友軍使用相同武功，由該友軍追加{}%傷害攻擊；否則自身追加{}%"
                "傷害攻擊。自身替代攻擊不觸發絕招效果。",
                友軍傷害百分比,
                自身傷害百分比);
        }
        return std::format("出招：同武功存活友軍追擊{}%傷害；無則自身追擊{}%傷害",
                           友軍傷害百分比,
                           自身傷害百分比);
    }
};

struct HitKnockback final : GameplayEffectDefinition
{
    int 距離格數{};
    static constexpr std::string_view Name = "主彈命中擊退";
    static constexpr auto Parameters = std::array<Parameter<HitKnockback>, 1>{ { { { "距離格數", 0, 1000000 }, &HitKnockback::距離格數 } } };
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return { EffectRule{ .event = EffectEvent::MainProjectileBeforeDamage,
            .selector = EffectSelector{ .kind = EffectSelectorKind::HitTarget },
            .actions
            = { EffectAction{ .value = ForceMoveAction{ .distanceTiles = 距離格數,
                                  .collision = ForceMoveCollision::StopBeforeBlocked,
                                  .blocked = ForceMoveBlockedResult::Shorten } } } } };
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("命中：擊退{}格", 距離格數);

        return std::format("命中：擊退{}格，遇障礙停止。", 距離格數);
    }
};

struct FixedCastMpCost final : GameplayEffectDefinition
{
    int 消耗內力{};
    static constexpr std::string_view Name = "設定出招耗內";
    static constexpr auto Parameters = std::array<Parameter<FixedCastMpCost>, 1>{
        {Parameter<FixedCastMpCost>{{"消耗內力", 0, 1000000}, &FixedCastMpCost::消耗內力}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::CastPlanned,
                       .actions = {EffectAction{.value = ModifyCastAction{.mpCost = EffectNumber{.flat = 消耗內力}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("出招：耗{}內", 消耗內力);
        return std::format("出招消耗{}內力。", 消耗內力);
    }
};

struct FarthestPureTracking final : GameplayEffectDefinition
{
    int 每星傷害{};
    static constexpr std::string_view Name = "最遠敵人追蹤純粹傷害";
    static constexpr auto Parameters = std::array<Parameter<FarthestPureTracking>, 1>{
        {Parameter<FarthestPureTracking>{{"每星傷害", 0, 1000000}, &FarthestPureTracking::每星傷害}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::FarthestEnemy},
            .actions = {EffectAction{
                .value = ModifyAttackAction{
                    .tracking = true,
                    .mainProjectile = false,
                    .targets = AttackTargetPolicy::SelectedTargets,
                    .propagation = CastPropagationPolicy::SuppressUltimateRules,
                    .addToBaseAttack = true,
                    .damageOverride = EffectNumber{.base = EffectNumberBase::SourceStar, .percent = 每星傷害 * 100},
                    .damageKind = BattleDamageKind::Pure}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("出招向最遠敵人追加追蹤攻擊，造成每星{}純粹傷害。追加攻擊不觸發絕招效果。", 每星傷害);
        }
        return std::format("出招：追擊最遠敵人，每星{}純粹傷害", 每星傷害);
    }
};

struct HitPullStunGroup final : GameplayEffectDefinition
{
    int 敵人數{};
    int 半徑格數{};
    int 牽引格數{};
    int 眩暈幀數{};
    static constexpr std::string_view Name = "命中牽引眩暈";
    static constexpr auto Parameters = std::array<Parameter<HitPullStunGroup>, 4>{
        {Parameter<HitPullStunGroup>{{"敵人數", 1, 1000}, &HitPullStunGroup::敵人數},
         Parameter<HitPullStunGroup>{{"半徑格數", 1, 1000000}, &HitPullStunGroup::半徑格數},
         Parameter<HitPullStunGroup>{{"牽引格數", 0, 1000000}, &HitPullStunGroup::牽引格數},
         Parameter<HitPullStunGroup>{{"眩暈幀數", 1, 1000000}, &HitPullStunGroup::眩暈幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::MainProjectileBeforeDamage,
            .selector = EffectSelector{.kind = EffectSelectorKind::UnitsInRadius,
                                       .count = 敵人數,
                                       .radiusTiles = 半徑格數,
                                       .team = EffectTeamFilter::Enemy,
                                       .requiredTarget = EffectRequiredTarget::HitTarget},
            .actions = {EffectAction{.value = ForceMoveAction{.direction = ForceMoveDirection::TowardSource,
                                                              .distanceTiles = 牽引格數,
                                                              .collision = ForceMoveCollision::StopBeforeBlocked,
                                                              .blocked = ForceMoveBlockedResult::Shorten}},
                        EffectAction{.value = ApplyStatusAction{.status = BattleStatusKind::Stun,
                                                                .durationFrames = 眩暈幀數,
                                                                .reapplication
                                                                = StatusReapplicationPolicy::KeepLongerDuration}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("命中時牽引{}格內最多{}名敵人{}格，並眩暈{}幀。包含原命中目標；牽引遇障礙停止。",
                               半徑格數,
                               敵人數,
                               牽引格數,
                               眩暈幀數);
        }
        return std::format("命中：牽引{}格內至多{}敵{}格，眩暈{}幀", 半徑格數, 敵人數, 牽引格數, 眩暈幀數);
    }
};

struct BorrowEnemyEffects final : GameplayEffectDefinition
{
    static constexpr std::string_view Name = "借用敵方絕招效果";
    static constexpr auto Parameters = std::array<Parameter<BorrowEnemyEffects>, 0>{{}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::CastPlanned,
            .actions = {EffectAction{
                .value = StateMachineAction{BorrowEffectRulesAction{
                    .sourceUnits
                    = EffectSelector{.kind = EffectSelectorKind::Enemies, .tieBreak = EffectTieBreak::BattleRandom},
                    .sourceCount = EffectNumber{.base = EffectNumberBase::SourceStar,
                                                .percent = 50,
                                                .rounding = EffectRounding::Ceil,
                                                .minimum = 1,
                                                .maximum = 2},
                    .filter = BorrowedRuleFilter{.allowedActionCategories
                                                 = {BorrowedRuleActionCategory::AttributeModifier,
                                                    BorrowedRuleActionCategory::DamageModifier,
                                                    BorrowedRuleActionCategory::ResourceChange,
                                                    BorrowedRuleActionCategory::HealTransactionModifier,
                                                    BorrowedRuleActionCategory::Status,
                                                    BorrowedRuleActionCategory::Damage,
                                                    BorrowedRuleActionCategory::Attack,
                                                    BorrowedRuleActionCategory::ForcedMovement,
                                                    BorrowedRuleActionCategory::Area,
                                                    BorrowedRuleActionCategory::Cast,
                                                    BorrowedRuleActionCategory::StateValue,
                                                    BorrowedRuleActionCategory::DamageMemory,
                                                    BorrowedRuleActionCategory::DamageAbsorption,
                                                    BorrowedRuleActionCategory::StatusDamageSettlement}}}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return "施放絕招時，依星級隨機借用1至2名敵人的絕招效果。每兩星增加一個來源，向上取整，最低1、最多2；不借用"
                   "會造成遞迴複製或借用的效果。";
        }
        return "絕招：本次出招依星級隨機借用1至2名敵人的絕招效果";
    }
};

struct ChanceRepeatCast final : GameplayEffectDefinition
{
    int 機率百分比{};
    static constexpr std::string_view Name = "機率免費再出招";
    static constexpr auto Parameters = std::array<Parameter<ChanceRepeatCast>, 1>{
        {Parameter<ChanceRepeatCast>{{"機率百分比", 0, 100}, &ChanceRepeatCast::機率百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::CastContinuation,
            .selector = EffectSelector{.kind = EffectSelectorKind::OriginalAttackTarget},
            .chancePct = 機率百分比,
            .actions
            = {EffectAction{.value = ModifyCastAction{.freeAdditionalCast = true,
                                                      .propagation = CastPropagationPolicy::SuppressUltimateRules}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("出招後有{}%機率免費追加相同出招。追加出招不再次觸發絕招效果。", 機率百分比);
        }
        return std::format("出招：{}%機率免費再出招一次", 機率百分比);
    }
};

struct DelayedAlternateFollowup final : GameplayEffectDefinition
{
    int 延遲幀數{};
    int 傷害百分比{};
    int 抵擋機率百分比{};
    static constexpr std::string_view Name = "延遲追擊並抵擋";
    static constexpr auto Parameters = std::array<Parameter<DelayedAlternateFollowup>, 3>{
        {Parameter<DelayedAlternateFollowup>{{"延遲幀數", 1, 1000000}, &DelayedAlternateFollowup::延遲幀數},
         Parameter<DelayedAlternateFollowup>{{"傷害百分比", 0, 1000000}, &DelayedAlternateFollowup::傷害百分比},
         Parameter<DelayedAlternateFollowup>{{"抵擋機率百分比", 0, 100}, &DelayedAlternateFollowup::抵擋機率百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions = {EffectAction{.value = ModifyAttackAction{.runtimeBehavior = DelayedAlternateAttackBehavior{
                                                                     .delayFrames = 延遲幀數,
                                                                     .damagePct = 傷害百分比,
                                                                     .attackerBlockGainChancePct = 抵擋機率百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招{}幀後以{}%傷害追擊附近其他敵人；追擊時有{}%"
                "機率獲得一次傷害抵擋。附近沒有其他敵人時追擊原目標，抵擋最多保留一次。",
                延遲幀數,
                傷害百分比,
                抵擋機率百分比);
        }
        return std::format("出招：{}幀後追擊附近其他敵人，{}%傷害；追擊：{}%機率抵擋+1次",
                           延遲幀數,
                           傷害百分比,
                           抵擋機率百分比);
    }
};

struct ChanceHitKnockback final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 距離像素{};
    int 鎖定幀數{};
    static constexpr std::string_view Name = "機率命中擊退";
    static constexpr auto Parameters = std::array<Parameter<ChanceHitKnockback>, 3>{
        {Parameter<ChanceHitKnockback>{{"機率百分比", 0, 100}, &ChanceHitKnockback::機率百分比},
         Parameter<ChanceHitKnockback>{{"距離像素", 0, 1000000}, &ChanceHitKnockback::距離像素},
         Parameter<ChanceHitKnockback>{{"鎖定幀數", 1, 1000000}, &ChanceHitKnockback::鎖定幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .chancePct = 機率百分比,
                           .actions
                           = {EffectAction{.value = ForceMoveAction{.distancePixels = 距離像素,
                                                                    .lockFrames = 鎖定幀數,
                                                                    .collision = ForceMoveCollision::StopBeforeBlocked,
                                                                    .blocked = ForceMoveBlockedResult::Shorten}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("命中有{}%機率擊退敵人{}像素，鎖定{}幀。遇障礙停止。", 機率百分比, 距離像素, 鎖定幀數);
        }
        return std::format("命中：{}%機率擊退{}像素，鎖定{}幀", 機率百分比, 距離像素, 鎖定幀數);
    }
};

struct EveryNthHitDamage final : GameplayEffectDefinition
{
    int 命中次數{};
    int 傷害百分比{};
    static constexpr std::string_view Name = "每數次命中倍傷";
    static constexpr auto Parameters = std::array<Parameter<EveryNthHitDamage>, 2>{
        {Parameter<EveryNthHitDamage>{{"命中次數", 1, 1000}, &EveryNthHitDamage::命中次數},
         Parameter<EveryNthHitDamage>{{"傷害百分比", 0, 1000000}, &EveryNthHitDamage::傷害百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::HitBeforeDamage,
            .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
            .everyNthEvent = 命中次數,
            .actions = {EffectAction{.value = ModifyDamageAction{.stage = DamageModifierStage::AfterDefense,
                                                                 .amount = EffectNumber{.flat = 傷害百分比},
                                                                 .operation = DamageModifierOperation::Multiply}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("每{}次命中，該次技能傷害為{}%。在計算防禦後生效。", 命中次數, 傷害百分比);
        }
        return std::format("每{}次命中：該次技能傷害{}%", 命中次數, 傷害百分比);
    }
};

struct PoisonedTargetDamage final : GameplayEffectDefinition
{
    int 百分比{};
    static constexpr std::string_view Name = "對中毒敵人增傷";
    static constexpr auto Parameters = std::array<Parameter<PoisonedTargetDamage>, 1>{
        {Parameter<PoisonedTargetDamage>{{"百分比", -1000000, 1000000}, &PoisonedTargetDamage::百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::HitBeforeDamage,
            .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
            .conditions = {TargetHasStateCondition{}},
            .actions = {EffectAction{.value = ModifyDamageAction{.stage = DamageModifierStage::Final,
                                                                 .channel = DamageChannel::All,
                                                                 .amount = EffectNumber{.flat = 百分比},
                                                                 .operation = DamageModifierOperation::PercentAdd}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("對中毒敵人造成傷害{:+}%。在最終傷害階段生效。", 百分比);
        }
        return std::format("對中毒敵人傷害{:+}%", 百分比);
    }
};

struct FlatDamageBonus final : GameplayEffectDefinition
{
    int 點數{};
    static constexpr std::string_view Name = "固定傷害加成";
    static constexpr auto Parameters = std::array<Parameter<FlatDamageBonus>, 1>{
        {Parameter<FlatDamageBonus>{{"點數", -1000000, 1000000}, &FlatDamageBonus::點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.actions = {EffectAction{.value = ModifyDamageAction{.channel = DamageChannel::All,
                                                                            .amount = EffectNumber{.flat = 點數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("固定傷害{:+}點", 點數);
        return std::format("造成傷害{:+}點，在計算防禦前生效。", 點數);
    }
};

struct ChanceExecuteAfterDamage final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 生命門檻百分比{};
    static constexpr std::string_view Name = "傷害後低血處決";
    static constexpr auto Parameters = std::array<Parameter<ChanceExecuteAfterDamage>, 2>{
        {Parameter<ChanceExecuteAfterDamage>{{"機率百分比", 0, 100}, &ChanceExecuteAfterDamage::機率百分比},
         Parameter<ChanceExecuteAfterDamage>{{"生命門檻百分比", 0, 1000000},
                                             &ChanceExecuteAfterDamage::生命門檻百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                       .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                       .chancePct = 機率百分比,
                       .actions = {EffectAction{.value = ModifyDamageAction{
                                                    .stage = DamageModifierStage::Final,
                                                    .channel = DamageChannel::All,
                                                    .amount = EffectNumber{.flat = 生命門檻百分比},
                                                    .operation = DamageModifierOperation::ExecuteBelowMaxHpPercent}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("命中：{}%機率處決傷後血量<{}%目標", 機率百分比, 生命門檻百分比);

        return std::format(
            "命中有{}%機率在普通傷害結算後，處決生命低於最大生命{}%的目標。", 機率百分比, 生命門檻百分比);
    }
};

struct ChanceCastEnemyHealthDamage final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 生命百分比{};
    static constexpr std::string_view Name = "出招全敵生命傷害";
    static constexpr auto Parameters = std::array<Parameter<ChanceCastEnemyHealthDamage>, 2>{
        {Parameter<ChanceCastEnemyHealthDamage>{{"機率百分比", 0, 100}, &ChanceCastEnemyHealthDamage::機率百分比},
         Parameter<ChanceCastEnemyHealthDamage>{{"生命百分比", 0, 1000000}, &ChanceCastEnemyHealthDamage::生命百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::Enemies},
            .chancePct = 機率百分比,
            .actions
            = {EffectAction{.value = DealDamageAction{.amount = EffectNumber{.base = EffectNumberBase::TargetCurrentHp,
                                                                             .percent = 生命百分比,
                                                                             .minimum = 1},
                                                      .kind = BattleDamageKind::Effect,
                                                      .appliesDamageModifiers = false,
                                                      .triggersHurtInvincibility = false}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招有{}%機率對所有敵人造成其當前生命{}%傷害，最低1點。此傷害不受一般傷害加減影響，也不觸發受傷無敵。",
                機率百分比,
                生命百分比);
        }
        return std::format("出招：{}%機率傷全敵當前血量{}%", 機率百分比, 生命百分比);
    }
};

struct SlidingAttack final : GameplayEffectDefinition
{
    static constexpr std::string_view Name = "滑步攻擊";
    static constexpr auto Parameters = std::array<Parameter<SlidingAttack>, 0>{{}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::CastPlanned,
                           .actions
                           = {EffectAction{.value = ModifyCastAction{.mobility = CastMobilityPolicy::DashAttack}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return "使用近戰武功時，可滑步接近近戰範圍外、滑步範圍內的敵人，沿途攻擊並追加一次武功攻擊。";
        }
        return "近戰滑步：沿途攻擊，再追加1次武功";
    }
};

struct MissingHealthAttackDamage final : GameplayEffectDefinition
{
    int 攻擊百分比{};
    static constexpr std::string_view Name = "失血增加技能傷害";
    static constexpr auto Parameters = std::array<Parameter<MissingHealthAttackDamage>, 1>{
        {Parameter<MissingHealthAttackDamage>{{"攻擊百分比", 0, 1000000}, &MissingHealthAttackDamage::攻擊百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::HitBeforeDamage,
                           .actions = {EffectAction{
                               .value = ModifyDamageAction{
                                   .amount = EffectNumber{.base = EffectNumberBase::SourceAttack,
                                                          .multiplierBase = EffectNumberBase::SourceMissingHpRatio,
                                                          .percent = 攻擊百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("技能傷害增加攻擊×已損生命比例×{}%點。在計算防禦前生效。", 攻擊百分比);
        }
        return std::format("技能加傷=攻×失血比例×{}%", 攻擊百分比);
    }
};

struct ChanceSpiralBleedAttack final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 彈道數{};
    int 流血層數{};
    int 基礎幀數{};
    int 每星幀數{};
    static constexpr std::string_view Name = "機率螺旋流血攻擊";
    static constexpr auto Parameters = std::array<Parameter<ChanceSpiralBleedAttack>, 5>{
        {Parameter<ChanceSpiralBleedAttack>{{"機率百分比", 0, 100}, &ChanceSpiralBleedAttack::機率百分比},
         Parameter<ChanceSpiralBleedAttack>{{"彈道數", 1, 1000}, &ChanceSpiralBleedAttack::彈道數},
         Parameter<ChanceSpiralBleedAttack>{{"流血層數", 1, 1000}, &ChanceSpiralBleedAttack::流血層數},
         Parameter<ChanceSpiralBleedAttack>{{"基礎幀數", 1, 1000000}, &ChanceSpiralBleedAttack::基礎幀數},
         Parameter<ChanceSpiralBleedAttack>{{"每星幀數", 1, 1000000}, &ChanceSpiralBleedAttack::每星幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::AttackCommitted,
                           .chancePct = 機率百分比,
                           .actions = {EffectAction{
                               .value = ModifyAttackAction{.runtimeBehavior = ExpandingSpiralAttackBehavior{
                                                               .projectileCount = 彈道數,
                                                               .bleedStacks = 流血層數,
                                                               .baseFrames = 基礎幀數,
                                                               .framesPerStar = 每星幀數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招有{}%機率追加{}道擴張螺旋攻擊，持續幀數為{}+星級×{}，命中施加{}層流血。流血每層每10幀造成目標最大生命1%"
                "傷害，每次最低1點。",
                機率百分比,
                彈道數,
                基礎幀數,
                每星幀數,
                流血層數);
        }
        return std::format("出招：{}%機率追加{}道擴張螺旋，{}+星級×{}幀；命中：流血+{}層",
                           機率百分比,
                           彈道數,
                           基礎幀數,
                           每星幀數,
                           流血層數);
    }
};

struct FastRangedAttack final : GameplayEffectDefinition
{
    int 彈速百分比{};
    int 最小射程格數{};
    static constexpr std::string_view Name = "高速遠程攻擊";
    static constexpr auto Parameters = std::array<Parameter<FastRangedAttack>, 2>{
        {Parameter<FastRangedAttack>{{"彈速百分比", 0, 1000000}, &FastRangedAttack::彈速百分比},
         Parameter<FastRangedAttack>{{"最小射程格數", 0, 1000000}, &FastRangedAttack::最小射程格數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::CastPlanned,
                       .actions = {EffectAction{.value = ModifyCastAction{.rangeMode = CastRangeMode::Ranged,
                                                                          .projectileSpeedPct = 彈速百分比,
                                                                          .minimumSelectDistance = 最小射程格數}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("武功改為遠程攻擊，彈道速度為{}%，最小選擇距離為{}格。", 彈速百分比, 最小射程格數);
        }
        return std::format("改為遠程：彈速{}%，最小射程{}格", 彈速百分比, 最小射程格數);
    }
};

struct ChanceHitTrackingAttack final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 範圍像素{};
    int 傷害百分比{};
    static constexpr std::string_view Name = "命中追加追蹤攻擊";
    static constexpr auto Parameters = std::array<Parameter<ChanceHitTrackingAttack>, 3>{
        {Parameter<ChanceHitTrackingAttack>{{"機率百分比", 0, 100}, &ChanceHitTrackingAttack::機率百分比},
         Parameter<ChanceHitTrackingAttack>{{"範圍像素", 0, 1000000}, &ChanceHitTrackingAttack::範圍像素},
         Parameter<ChanceHitTrackingAttack>{{"傷害百分比", 0, 1000000}, &ChanceHitTrackingAttack::傷害百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::MainProjectileBeforeDamage,
                           .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                           .chancePct = 機率百分比,
                           .actions = {EffectAction{
                               .value = ModifyAttackAction{.runtimeBehavior = NearbyTrackingAttackBehavior{
                                                               .rangePixels = 範圍像素, .damagePct = 傷害百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("命中有{}%機率在{}像素內追加追蹤攻擊，造成{}%傷害。", 機率百分比, 範圍像素, 傷害百分比);
        }
        return std::format("命中：{}%機率追擊{}像素內敵人，{}%傷害", 機率百分比, 範圍像素, 傷害百分比);
    }
};

struct BlinkAttack final : GameplayEffectDefinition
{
    static constexpr std::string_view Name = "閃擊";
    static constexpr auto Parameters = std::array<Parameter<BlinkAttack>, 0>{{}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::CastPlanned,
                           .actions
                           = {EffectAction{.value = ModifyCastAction{.mobility = CastMobilityPolicy::BlinkAttack}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return "出招時瞬移至敵人附近攻擊，交替選擇隨機敵人與較脆弱的非無敵敵人。脆弱程度由最大生命與防禦判定。敵人"
                   "附近須有可站立的空位。";
        }
        return "出招：瞬移攻擊，隨機、脆弱敵人交替";
    }
};

struct CastClones final : GameplayEffectDefinition
{
    int 分身數{};
    static constexpr std::string_view Name = "出招生成分身";
    static constexpr auto Parameters
        = std::array<Parameter<CastClones>, 1>{{Parameter<CastClones>{{"分身數", 1, 1000}, &CastClones::分身數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.actions = {EffectAction{.value = StateMachineAction{GenerateClonesAction{.count = 分身數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("出招：{}個分身", 分身數);
        return std::format("出招生成{}個分身。", 分身數);
    }
};

struct MpRatioDamage final : GameplayEffectDefinition
{
    int 基礎傷害百分比{};
    int 滿內增傷百分比{};
    static constexpr std::string_view Name = "內力比例技能增傷";
    static constexpr auto Parameters = std::array<Parameter<MpRatioDamage>, 2>{
        {Parameter<MpRatioDamage>{{"基礎傷害百分比", 0, 1000000}, &MpRatioDamage::基礎傷害百分比},
         Parameter<MpRatioDamage>{{"滿內增傷百分比", 0, 1000000}, &MpRatioDamage::滿內增傷百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::HitBeforeDamage,
            .actions = {EffectAction{
                .value = ModifyDamageAction{.amount = EffectNumber{.base = EffectNumberBase::SourceCurrentMpRatio,
                                                                   .flat = 基礎傷害百分比,
                                                                   .percent = 滿內增傷百分比 * 100},
                                            .operation = DamageModifierOperation::Multiply}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("技能傷害{}%至{}%（隨內力比例提高）", 基礎傷害百分比, 基礎傷害百分比 + 滿內增傷百分比);

        auto text = std::format(
            "技能傷害為{}%至{}%，當前內力比例越高，傷害越高。", 基礎傷害百分比, 基礎傷害百分比 + 滿內增傷百分比);
        if (style == EffectDescriptionStyle::Full)
        {
            text += std::format("內力加成為當前內力比例×{}個百分點，向零取整。在計算防禦前生效。", 滿內增傷百分比);
        }
        return text;
    }
};

struct BouncingAttacks final : GameplayEffectDefinition
{
    int 追加命中次數{};
    int 機率百分比{};
    int 範圍像素{};
    static constexpr std::string_view Name = "攻擊機率彈射";
    static constexpr auto Parameters = std::array<Parameter<BouncingAttacks>, 3>{
        {Parameter<BouncingAttacks>{{"追加命中次數", 1, 1000}, &BouncingAttacks::追加命中次數},
         Parameter<BouncingAttacks>{{"機率百分比", 0, 100}, &BouncingAttacks::機率百分比},
         Parameter<BouncingAttacks>{{"範圍像素", 0, 1000000}, &BouncingAttacks::範圍像素}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions = {EffectAction{
                .value = ModifyAttackAction{
                    .runtimeBehavior = ProjectileBounceAttackBehavior{
                        .additionalHits = 追加命中次數, .chancePct = 機率百分比, .rangePixels = 範圍像素}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("命中：{}%機率在{}像素內彈射，追加至多{}次", 機率百分比, 範圍像素, 追加命中次數);
        return std::format("攻擊命中時有{}%機率在{}像素內彈射，最多追加命中{}次。", 機率百分比, 範圍像素, 追加命中次數);
    }
};

struct AdditionalUltimateProjectiles final : GameplayEffectDefinition
{
    int 彈道數{};
    static constexpr std::string_view Name = "絕招追加彈道";
    static constexpr auto Parameters = std::array<Parameter<AdditionalUltimateProjectiles>, 1>{
        {Parameter<AdditionalUltimateProjectiles>{{"彈道數", 1, 1000}, &AdditionalUltimateProjectiles::彈道數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::CastPlanned,
                           .conditions = {IsUltimateCondition{}},
                           .actions = {EffectAction{.value = ModifyCastAction{.additionalProjectiles = 彈道數}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("絕招：追加{}道彈道", 彈道數);
        return std::format("絕招追加{}道彈道。", 彈道數);
    }
};

struct DeathExplosionTracking final : GameplayEffectDefinition
{
    int 生命傷害百分比{};
    int 範圍格數{};
    int 目標數{};
    int 眩暈幀數{};
    static constexpr std::string_view Name = "陣亡追蹤爆炸";
    static constexpr auto Parameters = std::array<Parameter<DeathExplosionTracking>, 4>{
        {Parameter<DeathExplosionTracking>{{"生命傷害百分比", 0, 1000000}, &DeathExplosionTracking::生命傷害百分比},
         Parameter<DeathExplosionTracking>{{"範圍格數", 0, 1000000}, &DeathExplosionTracking::範圍格數},
         Parameter<DeathExplosionTracking>{{"目標數", 0, 1000000}, &DeathExplosionTracking::目標數},
         Parameter<DeathExplosionTracking>{{"眩暈幀數", 1, 1000000}, &DeathExplosionTracking::眩暈幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::UnitDied,
            .actions = {EffectAction{
                .value = DealDamageAction{
                    .amount
                    = EffectNumber{.base = EffectNumberBase::SourceMaxHp, .percent = 生命傷害百分比, .minimum = 1},
                    .appliesDamageModifiers = false,
                    .triggersHurtInvincibility = false,
                    .areaProjectiles = AreaProjectileDamageDelivery{.rangeTiles = 範圍格數,
                                                                    .maximumTargets = 目標數,
                                                                    .stunFrames = 眩暈幀數,
                                                                    .trackEventSource = true}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "死亡時對{}格內最多{}名敵人發射追蹤攻擊，造成自身最大生命{}%的物理傷害並眩暈{}"
                "幀。包含仍存活的攻擊者；最低1傷害，不受一般傷害加減影響，也不觸發受傷無敵。",
                範圍格數,
                目標數,
                生命傷害百分比,
                眩暈幀數);
        }
        return std::format("陣亡：追擊{}格內至多{}敵，自身血上限{}%物傷、眩暈{}幀",
                           範圍格數,
                           目標數,
                           生命傷害百分比,
                           眩暈幀數);
    }
};

}    // namespace

void appendAttackEffects(std::vector<GameplayEffectRegistration>& entries)
{
    entries.push_back(registration<SideAttacks>());
    entries.push_back(registration<PiercingFan>());
    entries.push_back(registration<CopyLivingAttack>());
    entries.push_back(registration<RangedAttack>());
    entries.push_back(registration<HitIgnoreDefence>());
    entries.push_back(registration<HitFlatSkillDamage>());
    entries.push_back(registration<MissingHealthPureDamage>());
    entries.push_back(registration<ReceivedDamageCharge>());
    entries.push_back(registration<ChanceExecuteWounded>());
    entries.push_back(registration<TargetSquarePureDamage>());
    entries.push_back(registration<ClearEnemyProjectiles>());
    entries.push_back(registration<DelayedSameTargetAttacks>());
    entries.push_back(registration<AttackOrdinalStun>());
    entries.push_back(registration<MatchingAllyFollowup>());
    entries.push_back(registration<HitKnockback>());
    entries.push_back(registration<FixedCastMpCost>());
    entries.push_back(registration<FarthestPureTracking>());
    entries.push_back(registration<HitPullStunGroup>());
    entries.push_back(registration<BorrowEnemyEffects>());
    entries.push_back(registration<ChanceRepeatCast>());
    entries.push_back(registration<DelayedAlternateFollowup>());
    entries.push_back(registration<ChanceHitKnockback>());
    entries.push_back(registration<EveryNthHitDamage>());
    entries.push_back(registration<PoisonedTargetDamage>());
    entries.push_back(registration<FlatDamageBonus>());
    entries.push_back(registration<ChanceExecuteAfterDamage>());
    entries.push_back(registration<ChanceCastEnemyHealthDamage>());
    entries.push_back(registration<SlidingAttack>());
    entries.push_back(registration<MissingHealthAttackDamage>());
    entries.push_back(registration<ChanceSpiralBleedAttack>());
    entries.push_back(registration<FastRangedAttack>());
    entries.push_back(registration<ChanceHitTrackingAttack>());
    entries.push_back(registration<BlinkAttack>());
    entries.push_back(registration<CastClones>());
    entries.push_back(registration<MpRatioDamage>());
    entries.push_back(registration<BouncingAttacks>());
    entries.push_back(registration<AdditionalUltimateProjectiles>());
    entries.push_back(registration<DeathExplosionTracking>());
}

}    // namespace KysChess::GameplayEffects
