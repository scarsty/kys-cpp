#include "ChessGameplayEffectInternal.h"

namespace KysChess::GameplayEffects
{
namespace
{

struct SevenStarVolley final : GameplayEffectDefinition
{
    int 出招次數{};
    int 武功威力{};
    int 特效編號{};
    static constexpr std::string_view Name = "七星歸一";
    static constexpr auto Parameters = std::array<Parameter<SevenStarVolley>, 3>{ {
        {{"出招次數", 2, 1000}, &SevenStarVolley::出招次數},
        {{"武功威力", 0, 1000000}, &SevenStarVolley::武功威力},
        {{"特效編號", 0, 1000000}, &SevenStarVolley::特效編號} } };
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .observation = EffectObservationScope::ComboMemberEventSource,
            .selector = EffectSelector{.kind = EffectSelectorKind::OriginalAttackTarget},
            .everyNthEvent = 出招次數,
            .actions = {EffectAction{.value = ModifyAttackAction{
                .through = false,
                .tracking = true,
                .mainProjectile = false,
                .targets = AttackTargetPolicy::SelectedTargets,
                .propagation = CastPropagationPolicy::NoEffectRules,
                .addToBaseAttack = true,
                .source = EffectSelector{.kind = EffectSelectorKind::Self},
                .damageKind = BattleDamageKind::Physical,
                .independentProjectile = IndependentProjectile{.visualEffectId = 特效編號, .speed = 18, .lifetimeFrames = 120, .magicPower = 武功威力},
                .activationLog = "七星歸一"}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("同門每出招{}次：全員劍氣合擊，武功威力{}", 出招次數, 武功威力);
        return std::format("同羈絆成員共同累計出招{}次時，所有存活成員向觸發者的目標各發射一道追蹤劍氣，以發射者自身攻擊力及{}武功威力，依正常攻防公式計算物理傷害，可被閃避與格擋。合擊不打斷動作、不計入出招次數，也不觸發其他出招或命中效果。", 出招次數, 武功威力);
    }
};

struct MemberDeathShield final : GameplayEffectDefinition
{
    int 陣亡人數{ };
    int 生命百分比{ };
    static constexpr std::string_view Name = "同羈絆陣亡補盾";
    static constexpr auto Parameters = std::array<Parameter<MemberDeathShield>, 2>{ { { { "陣亡人數", 1, 1000 }, &MemberDeathShield::陣亡人數 },
        { { "生命百分比", 0, 1000000 }, &MemberDeathShield::生命百分比 } } };
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return { EffectRule{ .event = EffectEvent::AllyDied,
            .conditions = { EventTargetBelongsToBoundSourceCondition{ } },
            .everyNthEvent = 陣亡人數,
            .actions = { EffectAction{ .value = ChangeResourceAction{ .resource = BattleResource::Shield,
                                           .amount = EffectNumber{ .base = EffectNumberBase::TargetMaxHp, .percent = 生命百分比 },
                                           .kind = ResourceChangeKind::Grant } } } } };
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("每{}名同羈絆友軍陣亡：護盾+血上限{}%", 陣亡人數, 生命百分比);

        return std::format("每{}名同羈絆友軍陣亡：自身獲得血上限{}%護盾。", 陣亡人數, 生命百分比);
    }
};

struct MemberDeathAttribute final : GameplayEffectDefinition
{
    int 屬性{ };
    int 點數{ };
    static constexpr std::array<std::string_view, 2> Choices{ "攻擊", "防禦" };
    static constexpr std::string_view Name = "同羈絆陣亡屬性加成";
    static constexpr auto Parameters = std::array<Parameter<MemberDeathAttribute>, 2>{ { { { "屬性", 0, 1, Choices }, &MemberDeathAttribute::屬性 },
        { { "點數", -1000000, 1000000 }, &MemberDeathAttribute::點數 } } };
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return { EffectRule{ .event = EffectEvent::AllyDied,
            .conditions = { EventTargetBelongsToBoundSourceCondition{ } },
            .actions = { EffectAction{ .value = ModifyAttributeAction{
                                           .attribute = 屬性 == 0 ? BattleAttribute::Attack : BattleAttribute::Defence,
                                           .amount = EffectNumber{ .flat = 點數 } } } } } };
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("同羈絆友軍陣亡：{}", compactAttributeDescription(屬性 == 0 ? BattleAttribute::Attack : BattleAttribute::Defence, 點數, false));

        return std::format("同羈絆友軍陣亡：自身{}{:+}。", Choices[屬性], 點數);
    }
};

struct LastComboMemberBerserk final : GameplayEffectDefinition
{
    int 暴擊百分比{};
    int 冷卻縮減百分比{};
    int 吸血百分比{};
    int 持續幀數{};
    static constexpr std::string_view Name = "最後羈絆成員狂暴";
    static constexpr auto Parameters = std::array<Parameter<LastComboMemberBerserk>, 4>{ {
        {{"暴擊百分比", 0, 1000000}, &LastComboMemberBerserk::暴擊百分比},
        {{"冷卻縮減百分比", 0, 1000000}, &LastComboMemberBerserk::冷卻縮減百分比},
        {{"吸血百分比", 0, 1000000}, &LastComboMemberBerserk::吸血百分比},
        {{"持續幀數", 1, 1000000}, &LastComboMemberBerserk::持續幀數} } };
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        auto behavior = std::make_shared<StatusBehaviorDefinition>();
        behavior->rules = {EffectRule{
            .id = EffectRuleId{1},
            .event = EffectEvent::DamageResolved,
            .observation = EffectObservationScope::StatusHolderEventSource,
            .selector = EffectSelector{.kind = EffectSelectorKind::StatusHolder},
            .conditions = {DamagePerspectiveCondition{}, DamageOriginIsAttackCondition{}},
            .actions = {EffectAction{.value = ChangeResourceAction{
                .amount = EffectNumber{.base = EffectNumberBase::FinalHpDamage, .percent = 吸血百分比},
                .healKind = EffectHealKind::Lifesteal}}}}};

        return {EffectRule{
            .event = EffectEvent::AllyDied,
            .selector = EffectSelector{.kind = EffectSelectorKind::ComboMembers, .team = EffectTeamFilter::Ally},
            .conditions = {EventTargetBelongsToBoundSourceCondition{}, SourceIsLastAliveComboMemberCondition{}},
            .maxActivations = 1,
            .actions = {
                EffectAction{.value = ModifyAttributeAction{
                    .attribute = BattleAttribute::CriticalChance,
                    .amount = EffectNumber{.flat = 暴擊百分比},
                    .operation = AttributeOperation::PercentagePointAdd,
                    .durationFrames = 持續幀數}},
                EffectAction{.value = ModifyAttributeAction{
                    .attribute = BattleAttribute::CooldownReduction,
                    .amount = EffectNumber{.flat = 冷卻縮減百分比},
                    .operation = AttributeOperation::PercentagePointAdd,
                    .durationFrames = 持續幀數}},
                EffectAction{.value = ApplyStatusAction{
                    .status = BattleStatusKind::Berserk,
                    .durationFrames = 持續幀數,
                    .reapplication = StatusReapplicationPolicy::RefreshDuration,
                    .behavior = std::move(behavior)}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("同羈絆僅剩自身：狂暴，暴擊+{}%、冷卻-{}%、吸血{}%，{}幀；每場一次",
                暴擊百分比, 冷卻縮減百分比, 吸血百分比, 持續幀數);
        return std::format("場上只剩最後一名此羈絆的棋子時，獲得「狂暴」：暴擊率增加{}%、冷卻縮減增加{}%，攻擊吸血{}%，持續{}幀。每場戰鬥觸發一次；吸血按實際造成的生命傷害計算。",
            暴擊百分比, 冷卻縮減百分比, 吸血百分比, 持續幀數);
    }
};

struct MultiTargetTeamHaste final : GameplayEffectDefinition
{
    int 命中人數{};
    int 速度百分比{};
    int 持續幀數{};
    static constexpr std::string_view Name = "多目標命中加速";
    static constexpr auto Parameters = std::array<Parameter<MultiTargetTeamHaste>, 3>{
        {Parameter<MultiTargetTeamHaste>{{"命中人數", 0, 1000}, &MultiTargetTeamHaste::命中人數},
         Parameter<MultiTargetTeamHaste>{{"速度百分比", -1000000, 1000000}, &MultiTargetTeamHaste::速度百分比},
         Parameter<MultiTargetTeamHaste>{{"持續幀數", 1, 1000000}, &MultiTargetTeamHaste::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::CastSettled,
                           .selector = EffectSelector{.kind = EffectSelectorKind::Allies},
                           .conditions = {CastDistinctTargetCountAtLeastCondition{.count = 命中人數}},
                           .actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Speed,
                                                                          .amount = EffectNumber{.flat = 速度百分比},
                                                                          .operation = AttributeOperation::PercentAdd,
                                                                          .durationFrames = 持續幀數,
                                                                          .stack = EffectStackPolicy::Refresh}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("一次出招命中至少{}名敵人後，全隊速度{:+}%，持續{}幀。重複觸發刷新持續時間。",
                               命中人數,
                               速度百分比,
                               持續幀數);
        }
        return std::format("單次出招命中≥{}敵：全隊速度{:+}%，{}幀", 命中人數, 速度百分比, 持續幀數);
    }
};

struct ShareAllyDamageArea final : GameplayEffectDefinition
{
    int 半徑格數{};
    int 持續幀數{};
    int 轉移減傷百分比{};
    static constexpr std::string_view Name = "友軍傷害分擔區域";
    static constexpr auto Parameters = std::array<Parameter<ShareAllyDamageArea>, 3>{
        {Parameter<ShareAllyDamageArea>{{"半徑格數", 1, 1000000}, &ShareAllyDamageArea::半徑格數},
         Parameter<ShareAllyDamageArea>{{"持續幀數", 1, 1000000}, &ShareAllyDamageArea::持續幀數},
         Parameter<ShareAllyDamageArea>{{"轉移減傷百分比", 0, 1000000}, &ShareAllyDamageArea::轉移減傷百分比}}};
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
                                                    .modifiers = {AreaModifier{.kind = AreaModifierKind::DamageRedirect,
                                                                               .relation = EffectTeamFilter::Ally,
                                                                               .percent = 轉移減傷百分比}}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招建立半徑{}格的區域，持續{}幀；替區域內友軍承受傷害，轉移傷害減少{}%"
                "。不替自身分擔，分擔傷害不再連鎖分擔；來源死亡時區域消失。",
                半徑格數,
                持續幀數,
                轉移減傷百分比);
        }
        return std::format("出招：半徑{}格，{}幀；代友軍承傷，轉移傷害-{}%",
                           半徑格數,
                           持續幀數,
                           轉移減傷百分比);
    }
};

struct HitDisruptionArea final : GameplayEffectDefinition
{
    int 半徑格數{};
    int 持續幀數{};
    int 速度百分比{};
    int 彈速百分比{};
    int 彈道壓制百分比{};
    static constexpr std::string_view Name = "命中減速干擾區域";
    static constexpr auto Parameters = std::array<Parameter<HitDisruptionArea>, 5>{
        {Parameter<HitDisruptionArea>{{"半徑格數", 1, 1000000}, &HitDisruptionArea::半徑格數},
         Parameter<HitDisruptionArea>{{"持續幀數", 1, 1000000}, &HitDisruptionArea::持續幀數},
         Parameter<HitDisruptionArea>{{"速度百分比", -1000000, 1000000}, &HitDisruptionArea::速度百分比},
         Parameter<HitDisruptionArea>{{"彈速百分比", -1000000, 1000000}, &HitDisruptionArea::彈速百分比},
         Parameter<HitDisruptionArea>{{"彈道壓制百分比", -1000000, 1000000}, &HitDisruptionArea::彈道壓制百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::MainProjectileBeforeDamage,
            .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
            .actions = {EffectAction{
                .value = CreateAreaAction{
                    .radiusTiles = 半徑格數,
                    .durationFrames = 持續幀數,
                    .merge = AreaMergePolicy::RefreshSameSource,
                    .modifiers = {AreaModifier{.relation = EffectTeamFilter::Enemy,
                                               .attribute = BattleAttribute::Speed,
                                               .amount = EffectNumber{.flat = 速度百分比}},
                                  AreaModifier{.kind = AreaModifierKind::AttackSpawn,
                                               .relation = EffectTeamFilter::Enemy,
                                               .tracking = false,
                                               .overlap = AreaOverlapPolicy::Any,
                                               .trackingOverlap = AreaOverlapPolicy::Any},
                                  AreaModifier{.kind = AreaModifierKind::AttackSpawn,
                                               .relation = EffectTeamFilter::Enemy,
                                               .speedPct = 彈速百分比,
                                               .speedOverlap = AreaOverlapPolicy::KeepStrongest},
                                  AreaModifier{.kind = AreaModifierKind::AttackSpawn,
                                               .relation = EffectTeamFilter::Enemy,
                                               .projectilePressurePct = 彈道壓制百分比,
                                               .projectilePressureOverlap = AreaOverlapPolicy::KeepStrongest}}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("命中：半徑{}格，{}幀；敵人速度{:+}%，敵方彈速{:+}%、彈道壓制{:+}%、禁止追蹤", 半徑格數, 持續幀數, 速度百分比, 彈速百分比, 彈道壓制百分比);

        return std::format(
            "命中建立半徑{}格的區域，持續{}幀；敵人速度{:+}%，敵方彈速{:+}%、彈道壓制{:+}%，並禁止追蹤。",
            半徑格數,
            持續幀數,
            速度百分比,
            彈速百分比,
            彈道壓制百分比);
    }
};

struct ProtectiveArea final : GameplayEffectDefinition
{
    int 半徑格數{};
    int 持續幀數{};
    int 格擋百分比{};
    int 敵方傷害百分比{};
    static constexpr std::string_view Name = "格擋抗退護陣";
    static constexpr auto Parameters = std::array<Parameter<ProtectiveArea>, 4>{
        {Parameter<ProtectiveArea>{{"半徑格數", 1, 1000000}, &ProtectiveArea::半徑格數},
         Parameter<ProtectiveArea>{{"持續幀數", 1, 1000000}, &ProtectiveArea::持續幀數},
         Parameter<ProtectiveArea>{{"格擋百分比", -1000000, 1000000}, &ProtectiveArea::格擋百分比},
         Parameter<ProtectiveArea>{{"敵方傷害百分比", -1000000, 1000000}, &ProtectiveArea::敵方傷害百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::AttackCommitted,
                           .actions = {EffectAction{
                               .value = CreateAreaAction{
                                   .radiusTiles = 半徑格數,
                                   .anchor = AreaAnchor::FollowSourceUnit,
                                   .durationFrames = 持續幀數,
                                   .sourceDeath = AreaSourceDeathPolicy::RemoveImmediately,
                                   .merge = AreaMergePolicy::RefreshSameSource,
                                   .modifiers = {AreaModifier{.relation = EffectTeamFilter::Ally,
                                                              .attribute = BattleAttribute::BlockChance,
                                                              .amount = EffectNumber{.flat = 格擋百分比}},
                                                 AreaModifier{.kind = AreaModifierKind::ForcedMoveImmunity,
                                                              .relation = EffectTeamFilter::Ally,
                                                              .blockedDirection = ForceMoveDirection::AwayFromSource,
                                                              .overlap = AreaOverlapPolicy::Any},
                                                 AreaModifier{.kind = AreaModifierKind::OutgoingDamage,
                                                              .relation = EffectTeamFilter::Enemy,
                                                              .percent = 敵方傷害百分比}}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招建立半徑{}格的護陣，持續{}幀；友軍格擋率{:+}%且免疫擊退，敵人造成傷害{:+}%。來源死亡時護陣消失。",
                半徑格數,
                持續幀數,
                格擋百分比,
                敵方傷害百分比);
        }
        return std::format("出招：護陣半徑{}格，{}幀；友軍格擋{:+}%、免擊退，敵人傷害{:+}%",
                           半徑格數,
                           持續幀數,
                           格擋百分比,
                           敵方傷害百分比);
    }
};

struct LowHealthMemberTeamAttack final : GameplayEffectDefinition
{
    int 生命門檻百分比{};
    int 攻擊百分比{};
    int 持續幀數{};
    static constexpr std::string_view Name = "成員低血全員增攻";
    static constexpr auto Parameters = std::array<Parameter<LowHealthMemberTeamAttack>, 3>{
        {Parameter<LowHealthMemberTeamAttack>{{"生命門檻百分比", 0, 1000000},
                                              &LowHealthMemberTeamAttack::生命門檻百分比},
         Parameter<LowHealthMemberTeamAttack>{{"攻擊百分比", -1000000, 1000000},
                                              &LowHealthMemberTeamAttack::攻擊百分比},
         Parameter<LowHealthMemberTeamAttack>{{"持續幀數", 1, 1000000}, &LowHealthMemberTeamAttack::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::FrameAdvanced,
                           .selector = EffectSelector{.kind = EffectSelectorKind::ComboMembers},
                           .conditions = {SourceHpRatioBelowCondition{.percent = 生命門檻百分比}},
                           .maxActivations = 1,
                           .sharedCooldownFrames = 200,
                           .actions
                           = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Attack,
                                                                          .amount = EffectNumber{.flat = 攻擊百分比},
                                                                          .operation = AttributeOperation::PercentAdd,
                                                                          .durationFrames = 持續幀數}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "任一成員生命首次低於{}%時，所有成員攻擊{:+}%，持續{}"
                "幀。每名成員每場各觸發一次，同來源兩次觸發至少間隔200幀。",
                生命門檻百分比,
                攻擊百分比,
                持續幀數);
        }
        return std::format(
            "任一成員首次血量<{}%：全員{:+}%攻，{}幀", 生命門檻百分比, 攻擊百分比, 持續幀數);
    }
};

template <bool BeforeDamage>
struct HitOutgoingDamagePenalty final : GameplayEffectDefinition
{
    int 傷害百分比{};
    int 持續幀數{};
    static constexpr std::string_view Name = BeforeDamage ? "主彈命中弱化傷害" : "命中削弱敵方傷害";
    static constexpr auto Parameters = std::array<Parameter<HitOutgoingDamagePenalty>, 2>{
        {Parameter<HitOutgoingDamagePenalty>{{"傷害百分比", -1000000, 1000000}, &HitOutgoingDamagePenalty::傷害百分比},
         Parameter<HitOutgoingDamagePenalty>{{"持續幀數", 1, 1000000}, &HitOutgoingDamagePenalty::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return { EffectRule{
            .event = BeforeDamage ? EffectEvent::MainProjectileBeforeDamage : EffectEvent::DamageResolved,
            .selector = EffectSelector{ .kind = EffectSelectorKind::HitTarget },
            .conditions = BeforeDamage ? std::vector<EffectCondition>{ } : std::vector<EffectCondition>{ DamagePerspectiveCondition{ }, AcceptedHitCondition{ .requirePositiveDamage = true } },
            .actions = { EffectAction{ .value = ModifyDamageAction{ .stage = BeforeDamage ? DamageModifierStage::Final : DamageModifierStage::BeforeDefense,
                                           .channel = DamageChannel::All,
                                           .amount = EffectNumber{ .flat = 傷害百分比 },
                                           .operation = DamageModifierOperation::PercentAdd,
                                           .durationFrames = 持續幀數,
                                           .stack = BeforeDamage ? EffectStackPolicy::Refresh : EffectStackPolicy::Independent } } } } };
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if constexpr (BeforeDamage)
        {
            if (style == EffectDescriptionStyle::Compact)
                return std::format("主彈命中：目標傷害{:+}%，{}幀", 傷害百分比, 持續幀數);
            return std::format("主彈命中：目標傷害{:+}%，持續{}幀；重複施加刷新時間。", 傷害百分比, 持續幀數);
        }
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("造成生命傷害後，使目標造成傷害{:+}%，持續{}幀。在計算防禦前生效，各次施加獨立。",
                               傷害百分比,
                               持續幀數);
        }
        return std::format("傷血：目標傷害{:+}%，{}幀", 傷害百分比, 持續幀數);
    }
};

struct AdaptToAttacker final : GameplayEffectDefinition
{
    int 每層承傷百分比{};
    int 層數上限{};
    static constexpr std::string_view Name = "適應攻擊者";
    static constexpr auto Parameters = std::array<Parameter<AdaptToAttacker>, 2>{
        {Parameter<AdaptToAttacker>{{"每層承傷百分比", -1000000, 1000000}, &AdaptToAttacker::每層承傷百分比},
         Parameter<AdaptToAttacker>{{"層數上限", 1, 1000}, &AdaptToAttacker::層數上限}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::HitBeforeDamage,
            .observation = EffectObservationScope::EventTarget,
            .actions = {EffectAction{.value = ModifyDamageAction{.perspective = DamageModifierPerspective::Incoming,
                                                                 .stage = DamageModifierStage::AfterDefense,
                                                                 .amount = EffectNumber{.flat = 每層承傷百分比},
                                                                 .operation = DamageModifierOperation::PercentAdd,
                                                                 .stack = EffectStackPolicy::AddStack,
                                                                 .stackLimit = 層數上限,
                                                                 .stackScope = EffectStackScope::EventSource}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "每次被命中後，來自該攻擊者的技能傷害{:+}%，最多{}層。每名攻擊者分別累積，只降低持有者受到的傷害。",
                每層承傷百分比,
                層數上限);
        }
        return std::format("受擊：該敵對自身技能傷害{:+}%，上限{}層", 每層承傷百分比, 層數上限);
    }
};



struct ReceivedHitDelayCounter final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 冷卻百分比{};
    static constexpr std::string_view Name = "受擊延長敵方冷卻";
    static constexpr auto Parameters = std::array<Parameter<ReceivedHitDelayCounter>, 2>{
        {Parameter<ReceivedHitDelayCounter>{{"機率百分比", 0, 100}, &ReceivedHitDelayCounter::機率百分比},
         Parameter<ReceivedHitDelayCounter>{{"冷卻百分比", 0, 1000000}, &ReceivedHitDelayCounter::冷卻百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute
                                                                    = BattleAttribute::IncomingCooldownExtensionChance,
                                                                    .amount = EffectNumber{.flat = 機率百分比}}},
                        EffectAction{.value = ModifyAttributeAction{.attribute
                                                                    = BattleAttribute::IncomingCooldownExtensionPercent,
                                                                    .amount = EffectNumber{.flat = 冷卻百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("受擊：{}%機率攻擊者出招冷卻+{}%", 機率百分比, 冷卻百分比);
        return std::format("受擊時有{}%機率，使攻擊者的出招冷卻延長{}%。", 機率百分比, 冷卻百分比);
    }
};

struct HitDelayEnemyCooldown final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 冷卻百分比{};
    static constexpr std::string_view Name = "命中延長敵方冷卻";
    static constexpr auto Parameters = std::array<Parameter<HitDelayEnemyCooldown>, 2>{
        {Parameter<HitDelayEnemyCooldown>{{"機率百分比", 0, 100}, &HitDelayEnemyCooldown::機率百分比},
         Parameter<HitDelayEnemyCooldown>{{"冷卻百分比", 0, 1000000}, &HitDelayEnemyCooldown::冷卻百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute
                                                                    = BattleAttribute::OutgoingCooldownExtensionChance,
                                                                    .amount = EffectNumber{.flat = 機率百分比}}},
                        EffectAction{.value = ModifyAttributeAction{.attribute
                                                                    = BattleAttribute::OutgoingCooldownExtensionPercent,
                                                                    .amount = EffectNumber{.flat = 冷卻百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("命中：{}%機率目標出招冷卻+{}%", 機率百分比, 冷卻百分比);
        return std::format("攻擊有{}%機率使目標出招冷卻延長{}%。", 機率百分比, 冷卻百分比);
    }
};

struct AdaptDodgeToAttacker final : GameplayEffectDefinition
{
    int 每層閃避百分比{};
    int 層數上限{};
    static constexpr std::string_view Name = "對攻擊者累積閃避";
    static constexpr auto Parameters = std::array<Parameter<AdaptDodgeToAttacker>, 2>{
        {Parameter<AdaptDodgeToAttacker>{{"每層閃避百分比", -1000000, 1000000}, &AdaptDodgeToAttacker::每層閃避百分比},
         Parameter<AdaptDodgeToAttacker>{{"層數上限", 1, 1000}, &AdaptDodgeToAttacker::層數上限}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::HitBeforeDamage,
            .observation = EffectObservationScope::EventTarget,
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::DodgeChance,
                                                                    .amount = EffectNumber{.flat = 每層閃避百分比},
                                                                    .stack = EffectStackPolicy::AddStack,
                                                                    .stackLimit = 層數上限,
                                                                    .stackScope = EffectStackScope::EventSource}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "每次被命中後，對該攻擊者的閃避率{:+}%，最多{}層。每名攻擊者分別累積。", 每層閃避百分比, 層數上限);
        }
        return std::format("受擊：對該敵閃避{:+}%，上限{}層", 每層閃避百分比, 層數上限);
    }
};

struct WeakenStrongestEnemies final : GameplayEffectDefinition
{
    int 敵人數{};
    int 攻擊點數{};
    int 防禦點數{};
    static constexpr std::string_view Name = "壓制最強敵人";
    static constexpr auto Parameters = std::array<Parameter<WeakenStrongestEnemies>, 3>{
        {Parameter<WeakenStrongestEnemies>{{"敵人數", 1, 1000}, &WeakenStrongestEnemies::敵人數},
         Parameter<WeakenStrongestEnemies>{{"攻擊點數", -1000000, 1000000}, &WeakenStrongestEnemies::攻擊點數},
         Parameter<WeakenStrongestEnemies>{{"防禦點數", -1000000, 1000000}, &WeakenStrongestEnemies::防禦點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::FrameAdvanced,
                       .selector = EffectSelector{.kind = EffectSelectorKind::StrongestEnemies, .count = 敵人數},
                       .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Attack,
                                                                               .amount = EffectNumber{.flat = 攻擊點數},
                                                                               .durationFrames = 1,
                                                                               .stack = EffectStackPolicy::AddStack,
                                                                               .stackLimit = 10}},
                                   EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::Defence,
                                                                               .amount = EffectNumber{.flat = 防禦點數},
                                                                               .durationFrames = 1,
                                                                               .stack = EffectStackPolicy::AddStack,
                                                                               .stackLimit = 10}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "持續壓制最強的{}名敵人，使其攻擊{:+}、防禦{:+}。同來源最多累積10層，離開壓制目標後失效。",
                敵人數,
                攻擊點數,
                防禦點數);
        }
        return std::format("壓制最強{}敵：{:+}攻、{:+}防", 敵人數, 攻擊點數, 防禦點數);
    }
};


struct PeriodicFreeUltimate final : GameplayEffectDefinition
{
    int 間隔幀數{};
    static constexpr std::string_view Name = "定時免費絕招";
    static constexpr auto Parameters = std::array<Parameter<PeriodicFreeUltimate>, 1>{
        {Parameter<PeriodicFreeUltimate>{{"間隔幀數", 1, 1000000}, &PeriodicFreeUltimate::間隔幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::FrameAdvanced,
                           .intervalFrames = 間隔幀數,
                           .actions = {EffectAction{
                               .value = ModifyCastAction{.autoUltimate = AutoUltimateCastRequest{.announce = true}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("每{}幀：免費絕招", 間隔幀數);
        return std::format("每{}幀免費施放絕招。", 間隔幀數);
    }
};

struct RescueReposition final : GameplayEffectDefinition
{
    int 次數{};
    static constexpr std::string_view Name = "低血友軍挪移救援";
    static constexpr auto Parameters = std::array<Parameter<RescueReposition>, 1>{
        {Parameter<RescueReposition>{{"次數", 1, 1000}, &RescueReposition::次數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions = {EffectAction{
                               .value = StateMachineAction{ConfigureRescueRepositionAction{.activations = 次數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("友軍受擊血量<25%：移至安全處、回血上限10%、無敵10幀；每場{}次", 次數);
        return std::format("友軍受擊降至25%生命以下時，將其挪至安全位置，回復10%最大生命並獲得10幀無敵；每場最多{}次。",
                           次數);
    }
};

struct ExecuteReposition final : GameplayEffectDefinition
{
    int 次數{};
    static constexpr std::string_view Name = "低血敵人牽引追擊";
    static constexpr auto Parameters = std::array<Parameter<ExecuteReposition>, 1>{
        {Parameter<ExecuteReposition>{{"次數", 1, 1000}, &ExecuteReposition::次數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.actions = {EffectAction{.value = StateMachineAction{ConfigureRescueRepositionAction{
                                                    .mode = RescueRepositionMode::Execute, .activations = 次數}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("敵人受傷血量<15%、附近無我方：拉近並普攻；每場{}次", 次數);
        return std::format("敵人受傷降至15%生命以下且附近沒有我方角色時，將其拉至身旁並追加普通攻擊；每場最多{}次。",
                           次數);
    }
};

}    // namespace

void appendTacticalEffects(std::vector<GameplayEffectRegistration>& entries)
{
    entries.push_back(registration<SevenStarVolley>());
    entries.push_back(registration<MemberDeathShield>());
    entries.push_back(registration<MemberDeathAttribute>());
    entries.push_back(registration<LastComboMemberBerserk>());
    entries.push_back(registration<HitOutgoingDamagePenalty<true>>());
    entries.push_back(registration<MultiTargetTeamHaste>());
    entries.push_back(registration<ShareAllyDamageArea>());
    entries.push_back(registration<HitDisruptionArea>());
    entries.push_back(registration<ProtectiveArea>());
    entries.push_back(registration<LowHealthMemberTeamAttack>());
    entries.push_back(registration<HitOutgoingDamagePenalty<false>>());
    entries.push_back(registration<AdaptToAttacker>());
    entries.push_back(registration<ReceivedHitDelayCounter>());
    entries.push_back(registration<HitDelayEnemyCooldown>());
    entries.push_back(registration<AdaptDodgeToAttacker>());
    entries.push_back(registration<WeakenStrongestEnemies>());
    entries.push_back(registration<PeriodicFreeUltimate>());
    entries.push_back(registration<RescueReposition>());
    entries.push_back(registration<ExecuteReposition>());
}

}    // namespace KysChess::GameplayEffects
