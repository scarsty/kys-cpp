#include "ChessGameplayEffectInternal.h"

namespace KysChess::GameplayEffects
{
namespace
{

struct CastSelfHeal final : GameplayEffectDefinition
{
    int 固定治療{ };
    int 每星治療{ };
    int 生命治療百分比{ };
    static constexpr std::string_view Name = "出招治療自身";
    static constexpr auto Parameters = std::array<Parameter<CastSelfHeal>, 3>{ { { { "固定治療", 0, 1000000, { }, 0 }, &CastSelfHeal::固定治療 },
        { { "每星治療", 0, 1000000, { }, 0 }, &CastSelfHeal::每星治療 },
        { { "生命治療百分比", 0, 1000000, { }, 0 }, &CastSelfHeal::生命治療百分比 } } };
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return { EffectRule{ .event = EffectEvent::AttackCommitted,
            .actions = { EffectAction{ .value = recoveryAmount(BattleResource::Hp, 固定治療, 每星治療,
                                           生命治療百分比, EffectNumberBase::SourceMaxHp) } } } };
    }
    std::string describe(EffectDescriptionStyle) const override
    {
        return std::format("出招：回血{}", recoveryDescription(固定治療, 每星治療, 生命治療百分比));
    }
};

template <BattleResource Resource, bool Team = false>
struct CastShield final : GameplayEffectDefinition
{
    int 固定護盾{ };
    int 每星護盾{ };
    int 生命護盾百分比{ };
    static constexpr std::string_view Name = Resource == BattleResource::Shield ? "出招護盾" : Resource == BattleResource::StatusShield ? "出招狀態護盾" :
        Team                                                                                                                            ? "出招全隊僵直護盾" :
                                                                                                                                          "出招僵直護盾";
    static constexpr auto Parameters = std::array<Parameter<CastShield>, 3>{ { { { "固定護盾", 0, 1000000, { }, 0 }, &CastShield::固定護盾 },
        { { "每星護盾", 0, 1000000, { }, 0 }, &CastShield::每星護盾 },
        { { "生命護盾百分比", 0, 1000000, { }, 0 }, &CastShield::生命護盾百分比 } } };
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return { EffectRule{ .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{ .kind = Team ? EffectSelectorKind::Allies : EffectSelectorKind::Self },
            .actions = { EffectAction{ .value = recoveryAmount(Resource, 固定護盾, 每星護盾, 生命護盾百分比) } } } };
    }
    std::string describe(EffectDescriptionStyle ) const override
    {
        const auto label = Resource == BattleResource::Shield ? "護盾"
            : Resource == BattleResource::StatusShield ? "狀態盾" : "僵直盾";
        return std::format("出招：{}{}+{}", Team ? "全隊" : "", label,
            recoveryDescription(固定護盾, 每星護盾, 生命護盾百分比));
    }
};

template <bool Team>
struct CastCleanse final : GameplayEffectDefinition
{
    static constexpr std::string_view Name = Team ? "出招全隊解控" : "出招淨化自身";
    static constexpr auto Parameters = std::array<Parameter<CastCleanse>, 0>{ };
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return { EffectRule{ .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{ .kind = Team ? EffectSelectorKind::Allies : EffectSelectorKind::Self },
            .actions = { EffectAction{ .value = Team ? RemoveStatusAction{ .controlOnly = true, .clearCurrentActionStagger = true } : RemoveStatusAction{ .negativeOnly = true } } } } };
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return Team ? "出招：全隊解控、解除僵直" : "出招：清除負面";

        return Team ? "出招解除全隊控制與當前僵直，保留當前位置與動作。" : "出招清除自身所有負面效果。";
    }
};

struct CastRestoreAndHeal final : GameplayEffectDefinition
{
    int 回復內力{};
    int 每星治療{};
    int 友軍數{};
    static constexpr std::string_view Name = "出招回內療癒";
    static constexpr auto Parameters = std::array<Parameter<CastRestoreAndHeal>, 3>{
        {Parameter<CastRestoreAndHeal>{{"回復內力", 0, 1000000}, &CastRestoreAndHeal::回復內力},
         Parameter<CastRestoreAndHeal>{{"每星治療", 0, 1000000}, &CastRestoreAndHeal::每星治療},
         Parameter<CastRestoreAndHeal>{{"友軍數", 1, 1000000}, &CastRestoreAndHeal::友軍數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::AttackCommitted,
                       .actions
                       = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::Mp,
                                                                     .amount = EffectNumber{.flat = 回復內力}}},
                          EffectAction{
                              .value = ChangeResourceAction{.amount = EffectNumber{.base = EffectNumberBase::SourceStar,
                                                                                   .percent = 每星治療 * 100},
                                                            .healRequiresFullMp = true}}}},
            EffectRule{
                .event = EffectEvent::AttackCommitted,
                .selector
                = EffectSelector{.kind = EffectSelectorKind::LowestMpAllies, .count = 友軍數, .excludeOwner = true},
                .actions = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::Mp,
                                                                       .amount = EffectNumber{.flat = 回復內力}}},
                            EffectAction{.value = ChangeResourceAction{
                                             .amount = EffectNumber{.base = EffectNumberBase::SourceStar,
                                                                    .percent = 每星治療 * 100},
                                             .healRequiresFullMp = true}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招時，自身與內力最低的{}名友軍回復{}內力；回復後滿內力者再回復每星{}"
                "生命。不包含自身在友軍名額內，先回內再判斷是否滿內。",
                友軍數,
                回復內力,
                每星治療);
        }
        return std::format(
            "出招：自身及內力最低{}名友軍回{}內，滿內者每星回{}血", 友軍數, 回復內力, 每星治療);
    }
};

struct CastStackPureDamage final : GameplayEffectDefinition
{
    int 每次層數{};
    int 層數上限{};
    int 每層純粹傷害{};
    static constexpr std::string_view Name = "出招疊加純粹傷害";
    static constexpr auto Parameters = std::array<Parameter<CastStackPureDamage>, 3>{
        { Parameter<CastStackPureDamage>{ { "每次層數", 1, 1000 }, &CastStackPureDamage::每次層數 },
            Parameter<CastStackPureDamage>{ { "層數上限", 1, 1000 }, &CastStackPureDamage::層數上限 },
            Parameter<CastStackPureDamage>{ { "每層純粹傷害", 0, 1000000 },
                &CastStackPureDamage::每層純粹傷害 } }
    };
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return { EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions
            = { EffectAction{
                .value = ApplyStatusAction{
                    .status = BattleStatusKind::TrueQi,
                    .quantity = AddStatusLayers{ .count = 每次層數, .limit = 層數上限 },
                    .behavior = std::make_shared<StatusBehaviorDefinition>(StatusBehaviorDefinition{
                        .rules = { EffectRule{
                            .id = EffectRuleId{ .value = 1 },
                            .event = EffectEvent::HitBeforeDamage,
                            .observation = EffectObservationScope::StatusHolderEventSource,
                            .selector = EffectSelector{ .kind = EffectSelectorKind::HitTarget },
                            .actions = { EffectAction{
                                .value = DealDamageAction{
                                    .amount = EffectNumber{ .flat = 每層純粹傷害,
                                        .statusScale = StatusNumberScale::PerContributionLayer },
                                    .kind = BattleDamageKind::Pure } } } } } }) } } } } };
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("出招：疊{}層，上限{}層；每層命中+{}純粹傷害", 每次層數, 層數上限, 每層純粹傷害);

        return std::format("出招疊{}層（上限{}）；每層命中+{}純粹傷害。此來源獨立累積層數。",
            每次層數, 層數上限, 每層純粹傷害);
    }
};


struct CastTeamMp final : GameplayEffectDefinition
{
    int 回復內力{};
    static constexpr std::string_view Name = "出招全隊回內";
    static constexpr auto Parameters = std::array<Parameter<CastTeamMp>, 1>{
        {Parameter<CastTeamMp>{{"回復內力", 0, 1000000}, &CastTeamMp::回復內力}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::AttackCommitted,
                           .selector = EffectSelector{.kind = EffectSelectorKind::Allies},
                           .actions
                           = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::Mp,
                                                                         .amount = EffectNumber{.flat = 回復內力}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("出招：全隊回{}內", 回復內力);
        return std::format("出招時全隊回復{}內力。", 回復內力);
    }
};

struct TransferEnemyMp final : GameplayEffectDefinition
{
    int 轉移內力{};
    static constexpr std::string_view Name = "奪內援助友軍";
    static constexpr auto Parameters = std::array<Parameter<TransferEnemyMp>, 1>{
        {Parameter<TransferEnemyMp>{{"轉移內力", 0, 1000000}, &TransferEnemyMp::轉移內力}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::HighestMpEnemy},
            .actions = {EffectAction{
                .value = ChangeResourceAction{
                    .resource = BattleResource::Mp,
                    .amount = EffectNumber{.flat = 轉移內力},
                    .kind = ResourceChangeKind::Transfer,
                    .transferDestination = EffectSelector{.kind = EffectSelectorKind::LowestMpAllies, .count = 1}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("出招時，從內力最高的敵人轉移{}內力給內力最低的友軍。", 轉移內力);
        }
        return std::format("出招：轉移{}內力，敵方最高→友方最低", 轉移內力);
    }
};

struct HealAndCleanseLowest final : GameplayEffectDefinition
{
    int 友軍數{};
    int 固定治療{};
    int 每星治療{ };
    int 生命治療百分比{};
    int 淨化數{};
    static constexpr std::string_view Name = "治療淨化低血友軍";
    static constexpr auto Parameters = std::array<Parameter<HealAndCleanseLowest>, 5>{
        { Parameter<HealAndCleanseLowest>{ { "友軍數", 1, 1000000 }, &HealAndCleanseLowest::友軍數 },
            Parameter<HealAndCleanseLowest>{ { "固定治療", 0, 1000000, { }, 0 }, &HealAndCleanseLowest::固定治療 },
            Parameter<HealAndCleanseLowest>{ { "每星治療", 0, 1000000, { }, 0 }, &HealAndCleanseLowest::每星治療 },
            Parameter<HealAndCleanseLowest>{ { "生命治療百分比", 0, 1000000 }, &HealAndCleanseLowest::生命治療百分比 },
            Parameter<HealAndCleanseLowest>{ { "淨化數", 0, 1000000 }, &HealAndCleanseLowest::淨化數 } }
    };
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return { EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{ .kind = EffectSelectorKind::LowestHpAllies, .count = 友軍數 },
            .actions
            = { EffectAction{ .value = recoveryAmount(BattleResource::Hp, 固定治療, 每星治療, 生命治療百分比) },
                EffectAction{ .value = RemoveStatusAction{ .negativeOnly = true, .count = 淨化數 } } } } };
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        return std::format("出招：血比最低{}名友軍，回血{}，清除{}個負面{}", 友軍數,
            recoveryDescription(固定治療, 每星治療, 生命治療百分比), 淨化數,
            style == EffectDescriptionStyle::Full ? "；優先清除剩餘時間最長的負面效果。" : "");
    }
};

struct CastDamageShield final : GameplayEffectDefinition
{
    int 護盾轉換百分比{};
    static constexpr std::string_view Name = "出招傷害轉盾";
    static constexpr auto Parameters = std::array<Parameter<CastDamageShield>, 1>{
        {Parameter<CastDamageShield>{{"護盾轉換百分比", 0, 1000000}, &CastDamageShield::護盾轉換百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::DamageResolved,
                           .conditions = {DamagePerspectiveCondition{}, DamageOriginIsAttackCondition{}},
                           .actions = {EffectAction{.value = StateMachineAction{RecordMaximumDamageAction{
                                                        .slot = EffectStateSlot::CastMaximumHpDamage}}}}},
                EffectRule{.event = EffectEvent::CastSettled,
                           .actions = {EffectAction{.value = StateMachineAction{ConsumeRecordedMaximumAction{
                                                        .slot = EffectStateSlot::CastMaximumHpDamage,
                                                        .destination = StateValueDestination::ShieldAmount,
                                                        .percent = 護盾轉換百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "一次出招結束時，將此次造成的最大單次技能生命傷害的{}%轉為護盾。各次出招分別記錄，結算後清空。",
                護盾轉換百分比);
        }
        return std::format("出招結束：本次最大單次技能血傷{}%轉護盾", 護盾轉換百分比);
    }
};

struct HealRemovePoisonBleed final : GameplayEffectDefinition
{
    int 友軍數{};
    int 生命治療百分比{};
    static constexpr std::string_view Name = "治療解毒止血";
    static constexpr auto Parameters = std::array<Parameter<HealRemovePoisonBleed>, 2>{
        {Parameter<HealRemovePoisonBleed>{{"友軍數", 1, 1000000}, &HealRemovePoisonBleed::友軍數},
         Parameter<HealRemovePoisonBleed>{{"生命治療百分比", 0, 1000000}, &HealRemovePoisonBleed::生命治療百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::LowestHpAllies, .count = 友軍數},
            .actions
            = {EffectAction{.value = ChangeResourceAction{.amount = EffectNumber{.base = EffectNumberBase::TargetMaxHp,
                                                                                 .percent = 生命治療百分比}}},
               EffectAction{.value = RemoveStatusAction{.statuses = {BattleStatusKind::Poison}}},
               EffectAction{.value = RemoveStatusAction{.statuses = {BattleStatusKind::Bleed}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
            "出招時，生命比例最低的{}名友軍回復最大生命的{}%，並移除中毒與流血。", 友軍數, 生命治療百分比);
        }
        return std::format(
            "出招：血比最低{}名友軍回血上限{}%，解毒、止血", 友軍數, 生命治療百分比);
    }
};

struct ReceivedDamageShield final : GameplayEffectDefinition
{
    int 護盾轉換百分比{};
    static constexpr std::string_view Name = "承傷蓄力護盾";
    static constexpr auto Parameters = std::array<Parameter<ReceivedDamageShield>, 1>{
        {Parameter<ReceivedDamageShield>{{"護盾轉換百分比", 0, 1000000}, &ReceivedDamageShield::護盾轉換百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::DamageResolved,
                           .conditions = {DamagePerspectiveCondition{.perspective = DamagePerspective::Received},
                                          DamageOriginIsAttackCondition{}},
                           .actions = {EffectAction{.value = StateMachineAction{RecordMaximumDamageAction{}}}}},
                EffectRule{.event = EffectEvent::AttackCommitted,
                           .actions = {EffectAction{
                               .value = StateMachineAction{ConsumeRecordedMaximumAction{
                                   .destination = StateValueDestination::ShieldAmount, .percent = 護盾轉換百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "記錄受到的最大單次技能生命傷害，下次出招時將記錄值的{}%轉為護盾。結算後清空，護盾吸收的傷害不計入。",
                護盾轉換百分比);
        }
        return std::format("下次出招：最大單次技能承受血傷{}%轉護盾", 護盾轉換百分比);
    }
};


struct SkillLifeSteal final : GameplayEffectDefinition
{
    int 吸血百分比{};
    static constexpr std::string_view Name = "技能生命傷害吸血";
    static constexpr auto Parameters = std::array<Parameter<SkillLifeSteal>, 1>{
        {Parameter<SkillLifeSteal>{{"吸血百分比", 0, 1000000}, &SkillLifeSteal::吸血百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::DamageResolved,
                           .conditions = {DamagePerspectiveCondition{}, DamageOriginIsAttackCondition{}},
                           .actions = {EffectAction{.value = ChangeResourceAction{
                                                        .amount = EffectNumber{.base = EffectNumberBase::FinalHpDamage,
                                                                               .percent = 吸血百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("造成技能傷害後，回復實際生命傷害的{}%。護盾吸收部分不計入治療量。", 吸血百分比);
        }
        return std::format("技能吸血{}%", 吸血百分比);
    }
};



struct HitMpRecovery final : GameplayEffectDefinition
{
    int 內力{};
    static constexpr std::string_view Name = "命中回內";
    static constexpr auto Parameters = std::array<Parameter<HitMpRecovery>, 1>{
        {Parameter<HitMpRecovery>{{"內力", 0, 1000000}, &HitMpRecovery::內力}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::DamageResolved,
                       .conditions = {DamagePerspectiveCondition{}, AcceptedHitCondition{}},
                       .actions = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::Mp,
                                                                              .amount = EffectNumber{.flat = 內力}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("命中：回{}內", 內力);
        return std::format("有效命中後回復{}內力。", 內力);
    }
};

struct PeriodicHealthRecovery final : GameplayEffectDefinition
{
    int 間隔幀數{};
    int 生命百分比{};
    static constexpr std::string_view Name = "定時生命回復";
    static constexpr auto Parameters = std::array<Parameter<PeriodicHealthRecovery>, 2>{
        {Parameter<PeriodicHealthRecovery>{{"間隔幀數", 1, 1000000}, &PeriodicHealthRecovery::間隔幀數},
         Parameter<PeriodicHealthRecovery>{{"生命百分比", 0, 1000000}, &PeriodicHealthRecovery::生命百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::FrameAdvanced,
                           .intervalFrames = 間隔幀數,
                           .actions = {EffectAction{
                               .value = ChangeResourceAction{
                                   .amount = EffectNumber{.base = EffectNumberBase::TargetMaxHp, .percent = 生命百分比},
                                   .healKind = EffectHealKind::Regeneration}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("每{}幀：回血上限{}%", 間隔幀數, 生命百分比);
        return std::format("每{}幀回復最大生命的{}%。", 間隔幀數, 生命百分比);
    }
};

struct LowHealthEmergencyHeal final : GameplayEffectDefinition
{
    int 生命門檻百分比{};
    int 治療生命百分比{};
    static constexpr std::string_view Name = "低血緊急回血";
    static constexpr auto Parameters = std::array<Parameter<LowHealthEmergencyHeal>, 2>{
        {Parameter<LowHealthEmergencyHeal>{{"生命門檻百分比", 0, 1000000}, &LowHealthEmergencyHeal::生命門檻百分比},
         Parameter<LowHealthEmergencyHeal>{{"治療生命百分比", 0, 1000000}, &LowHealthEmergencyHeal::治療生命百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::FrameAdvanced,
                           .conditions = {SourceHpRatioBelowCondition{.percent = 生命門檻百分比}},
                           .maxActivations = 1,
                           .actions = {EffectAction{.value = ChangeResourceAction{
                                                        .amount = EffectNumber{.base = EffectNumberBase::TargetMaxHp,
                                                                               .percent = 治療生命百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("生命首次低於{}%時，回復最大生命的{}%。每場觸發一次。", 生命門檻百分比, 治療生命百分比);
        }
        return std::format("首次血量<{}%：回血上限{}%", 生命門檻百分比, 治療生命百分比);
    }
};

struct HitStealMp final : GameplayEffectDefinition
{
    int 內力{};
    static constexpr std::string_view Name = "命中奪內";
    static constexpr auto Parameters
        = std::array<Parameter<HitStealMp>, 1>{{Parameter<HitStealMp>{{"內力", 0, 1000000}, &HitStealMp::內力}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::DamageResolved,
                       .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                       .conditions = {DamagePerspectiveCondition{}, AcceptedHitCondition{}},
                       .actions = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::Mp,
                                                                              .amount = EffectNumber{.flat = 內力},
                                                                              .kind = ResourceChangeKind::Drain}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("命中：奪{}內", 內力);
        return std::format("有效命中後，從目標奪取{}內力。", 內力);
    }
};

struct KillHeal final : GameplayEffectDefinition
{
    int 生命百分比{};
    static constexpr std::string_view Name = "擊殺回血";
    static constexpr auto Parameters
        = std::array<Parameter<KillHeal>, 1>{{Parameter<KillHeal>{{"生命百分比", 0, 1000000}, &KillHeal::生命百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::DamageResolved,
                           .conditions = {DamagePerspectiveCondition{}, DamageKilledTargetCondition{}},
                           .actions = {EffectAction{
                               .value = ChangeResourceAction{
                                   .amount = EffectNumber{.base = EffectNumberBase::TargetMaxHp, .percent = 生命百分比},
                                   .healKind = EffectHealKind::KillReward}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("擊殺：回血上限{}%", 生命百分比);
        return std::format("擊殺敵人後回復最大生命的{}%。", 生命百分比);
    }
};

struct HealingAura final : GameplayEffectDefinition
{
    int 半徑格數{};
    int 間隔幀數{};
    int 治療點數{};
    static constexpr std::string_view Name = "友軍治療光環";
    static constexpr auto Parameters = std::array<Parameter<HealingAura>, 3>{
        {Parameter<HealingAura>{{"半徑格數", 1, 1000000}, &HealingAura::半徑格數},
         Parameter<HealingAura>{{"間隔幀數", 1, 1000000}, &HealingAura::間隔幀數},
         Parameter<HealingAura>{{"治療點數", 0, 1000000}, &HealingAura::治療點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::FrameAdvanced,
                       .selector = EffectSelector{.kind = EffectSelectorKind::UnitsInRadius,
                                                  .radiusTiles = 半徑格數,
                                                  .team = EffectTeamFilter::Ally,
                                                  .excludeOwner = true},
                       .intervalFrames = 間隔幀數,
                       .actions = {EffectAction{.value = ChangeResourceAction{.amount = EffectNumber{.flat = 治療點數},
                                                                              .healKind = EffectHealKind::Aura}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("每{}幀為{}格內其他友軍回復{}生命。不治療自身。", 間隔幀數, 半徑格數, 治療點數);
        }
        return std::format("每{}幀：{}格內其他友軍回{}血", 間隔幀數, 半徑格數, 治療點數);
    }
};

struct CooldownAura final : GameplayEffectDefinition
{
    int 半徑格數{};
    int 間隔幀數{};
    int 冷卻百分比{};
    static constexpr std::string_view Name = "友軍減冷卻光環";
    static constexpr auto Parameters = std::array<Parameter<CooldownAura>, 3>{
        { Parameter<CooldownAura>{ { "半徑格數", 1, 1000000 }, &CooldownAura::半徑格數 },
            Parameter<CooldownAura>{ { "間隔幀數", 1, 1000000 }, &CooldownAura::間隔幀數 },
            Parameter<CooldownAura>{ { "冷卻百分比", 0, 1000000 }, &CooldownAura::冷卻百分比 } }
    };
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return { EffectRule{ .event = EffectEvent::FrameAdvanced,
            .selector = EffectSelector{ .kind = EffectSelectorKind::UnitsInRadius,
                .radiusTiles = 半徑格數,
                .team = EffectTeamFilter::Ally,
                .excludeOwner = true },
            .intervalFrames = 間隔幀數,
            .actions
            = { EffectAction{ .value = ChangeResourceAction{
                                  .resource = BattleResource::ActiveCooldown,
                                  .amount = EffectNumber{ .base = EffectNumberBase::TargetCurrentCooldown,
                                      .percent = 冷卻百分比,
                                      .rounding = EffectRounding::Ceil },
                                  .kind = ResourceChangeKind::Remove } } } } };
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("每{}幀：{}格內其他友軍當前冷卻-{}%", 間隔幀數, 半徑格數, 冷卻百分比);

        return std::format("每{}幀：{}格內其他友軍當前冷卻-{}%；不影響自身。", 間隔幀數, 半徑格數, 冷卻百分比);
    }
};

struct ChanceCastTeamMp final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 內力{};
    static constexpr std::string_view Name = "機率出招全隊回內";
    static constexpr auto Parameters = std::array<Parameter<ChanceCastTeamMp>, 2>{
        {Parameter<ChanceCastTeamMp>{{"機率百分比", 0, 100}, &ChanceCastTeamMp::機率百分比},
         Parameter<ChanceCastTeamMp>{{"內力", 0, 1000000}, &ChanceCastTeamMp::內力}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::AttackCommitted,
                       .selector = EffectSelector{.kind = EffectSelectorKind::Allies},
                       .chancePct = 機率百分比,
                       .actions = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::Mp,
                                                                              .amount = EffectNumber{.flat = 內力}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("出招：{}%機率全隊回{}內", 機率百分比, 內力);
        return std::format("出招有{}%機率使全隊回復{}內力。", 機率百分比, 內力);
    }
};

struct ChanceCastTeamShield final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 護盾點數{};
    static constexpr std::string_view Name = "機率出招全隊護盾";
    static constexpr auto Parameters = std::array<Parameter<ChanceCastTeamShield>, 2>{
        {Parameter<ChanceCastTeamShield>{{"機率百分比", 0, 100}, &ChanceCastTeamShield::機率百分比},
         Parameter<ChanceCastTeamShield>{{"護盾點數", 0, 1000000}, &ChanceCastTeamShield::護盾點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::Allies},
            .chancePct = 機率百分比,
            .actions = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::Shield,
                                                                   .amount = EffectNumber{.flat = 護盾點數},
                                                                   .kind = ResourceChangeKind::RefreshToAtLeast}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("出招：{}%機率全隊護盾≥{}", 機率百分比, 護盾點數);
        return std::format("出招有{}%機率使全隊護盾至少為{}。", 機率百分比, 護盾點數);
    }
};

struct HitHeal final : GameplayEffectDefinition
{
    int 生命{};
    static constexpr std::string_view Name = "命中回血";
    static constexpr auto Parameters
        = std::array<Parameter<HitHeal>, 1>{{Parameter<HitHeal>{{"生命", 0, 1000000}, &HitHeal::生命}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::DamageResolved,
                       .conditions = {DamagePerspectiveCondition{}, AcceptedHitCondition{}},
                       .actions = {EffectAction{.value = ChangeResourceAction{.amount = EffectNumber{.flat = 生命},
                                                                              .healKind = EffectHealKind::OnHit}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("命中：回{}血", 生命);
        return std::format("有效命中後回復{}生命。", 生命);
    }
};

struct HitAfflictedShield final : GameplayEffectDefinition
{
    int 生命百分比{};
    int 護盾上限百分比{};
    static constexpr std::string_view Name = "命中負面敵人護盾";
    static constexpr auto Parameters = std::array<Parameter<HitAfflictedShield>, 2>{
        {Parameter<HitAfflictedShield>{{"生命百分比", 1, 1000000}, &HitAfflictedShield::生命百分比},
         Parameter<HitAfflictedShield>{{"護盾上限百分比", 1, 1000000}, &HitAfflictedShield::護盾上限百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::DamageResolved,
            .selector = EffectSelector{.kind = EffectSelectorKind::Self},
            .conditions = {DamagePerspectiveCondition{}, AcceptedHitCondition{}, EventTargetHasNegativeStatusCondition{}},
            .activationLimit = EffectActivationLimit{.scope = EffectActivationScope::PerCastPerTarget, .maxEvaluations = 1},
            .actions = {EffectAction{.value = ChangeResourceAction{
                .resource = BattleResource::Shield,
                .amount = EffectNumber{.base = EffectNumberBase::SourceMaxHp, .percent = 生命百分比},
                .kind = ResourceChangeKind::Grant,
                .sourceShieldMaxHpPct = 護盾上限百分比}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("命中：負面敵人→護盾+血上限{}%，上限{}%，每招一次", 生命百分比, 護盾上限百分比);
        return std::format("命中負面狀態敵人獲得最大生命{}%護盾，累積上限{}%，每招一次。", 生命百分比, 護盾上限百分比);
    }
};

struct DeathHealMembers final : GameplayEffectDefinition
{
    int 生命百分比{};
    static constexpr std::string_view Name = "陣亡治療羈絆成員";
    static constexpr auto Parameters = std::array<Parameter<DeathHealMembers>, 1>{
        {Parameter<DeathHealMembers>{{"生命百分比", 0, 1000000}, &DeathHealMembers::生命百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::UnitDied,
            .selector = EffectSelector{.kind = EffectSelectorKind::ComboMembers},
            .actions = {EffectAction{
                .value = ChangeResourceAction{
                    .amount = EffectNumber{.base = EffectNumberBase::TargetMaxHp, .percent = 生命百分比, .minimum = 1},
                    .healKind = EffectHealKind::DeathMedical,
                    .healSourcePolicy = EffectHealSourcePolicy::AllowDead}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("陣亡：其他羈絆成員回血上限{}%", 生命百分比);
        return std::format("自身死亡時，其他羈絆成員回復各自最大生命的{}%，最低1點。", 生命百分比);
    }
};

struct UltimateReadyTeamHeal final : GameplayEffectDefinition
{
    int 生命{};
    static constexpr std::string_view Name = "絕招冷卻完成治療";
    static constexpr auto Parameters = std::array<Parameter<UltimateReadyTeamHeal>, 1>{
        {Parameter<UltimateReadyTeamHeal>{{"生命", 0, 1000000}, &UltimateReadyTeamHeal::生命}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::UltimateCooldownFinished,
                           .selector = EffectSelector{.kind = EffectSelectorKind::Allies},
                           .actions = {EffectAction{.value = ChangeResourceAction{.amount = EffectNumber{.flat = 生命},
                                                                                  .healKind = EffectHealKind::Team}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("絕招就緒：全隊回{}血", 生命);
        return std::format("絕招冷卻完成時，全隊回復{}生命。", 生命);
    }
};

struct ChanceHitTeamHeal final : GameplayEffectDefinition
{
    int 機率百分比{};
    int 生命百分比{};
    static constexpr std::string_view Name = "命中機率全隊回血";
    static constexpr auto Parameters = std::array<Parameter<ChanceHitTeamHeal>, 2>{
        {Parameter<ChanceHitTeamHeal>{{"機率百分比", 0, 100}, &ChanceHitTeamHeal::機率百分比},
         Parameter<ChanceHitTeamHeal>{{"生命百分比", 0, 1000000}, &ChanceHitTeamHeal::生命百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::DamageResolved,
                           .selector = EffectSelector{.kind = EffectSelectorKind::Allies},
                           .conditions = {DamagePerspectiveCondition{}, AcceptedHitCondition{}},
                           .chancePct = 機率百分比,
                           .actions = {EffectAction{
                               .value = ChangeResourceAction{
                                   .amount = EffectNumber{.base = EffectNumberBase::TargetMaxHp, .percent = 生命百分比},
                                   .healKind = EffectHealKind::Team}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("命中：{}%機率全隊回血上限{}%", 機率百分比, 生命百分比);
        return std::format("有效命中後，有{}%機率使全隊回復各自最大生命的{}%。", 機率百分比, 生命百分比);
    }
};

struct CastDrainEnemyMp final : GameplayEffectDefinition
{
    int 內力{};
    static constexpr std::string_view Name = "出招全敵扣內";
    static constexpr auto Parameters = std::array<Parameter<CastDrainEnemyMp>, 1>{
        {Parameter<CastDrainEnemyMp>{{"內力", 0, 1000000}, &CastDrainEnemyMp::內力}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::AttackCommitted,
                       .selector = EffectSelector{.kind = EffectSelectorKind::Enemies},
                       .actions = {EffectAction{.value = ChangeResourceAction{.resource = BattleResource::Mp,
                                                                              .amount = EffectNumber{.flat = 內力},
                                                                              .kind = ResourceChangeKind::Remove}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("出招：全敵失{}內", 內力);
        return std::format("出招使所有敵人失去{}內力。", 內力);
    }
};

struct InitialHealthShield final : GameplayEffectDefinition
{
    int 生命百分比{};
    static constexpr std::string_view Name = "開場生命護盾";
    static constexpr auto Parameters = std::array<Parameter<InitialHealthShield>, 1>{
        {Parameter<InitialHealthShield>{{"生命百分比", 0, 1000000}, &InitialHealthShield::生命百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions = {EffectAction{
                               .value = ChangeResourceAction{
                                   .resource = BattleResource::Shield,
                                   .amount = EffectNumber{.base = EffectNumberBase::TargetMaxHp, .percent = 生命百分比},
                                   .kind = ResourceChangeKind::Grant}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("開場：護盾=血上限{}%", 生命百分比);
        return std::format("開場獲得最大生命{}%的護盾。", 生命百分比);
    }
};

struct ClearMelodyShield final : GameplayEffectDefinition
{
    int 出招次數{};
    int 生命護盾百分比{};
    static constexpr std::string_view Name = "清音護心";
    static constexpr auto Parameters = std::array<Parameter<ClearMelodyShield>, 2>{{
        {{"出招次數", 1, 1000}, &ClearMelodyShield::出招次數},
        {{"生命護盾百分比", 1, 1000000}, &ClearMelodyShield::生命護盾百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::LowestHpAllies, .count = 1},
            .everyNthEvent = 出招次數 == 1 ? 0 : 出招次數,
            .naturalCastsOnly = true,
            .actions = {EffectAction{.value = ChangeResourceAction{
                .resource = BattleResource::Shield,
                .amount = EffectNumber{.base = EffectNumberBase::TargetMaxHp, .percent = 生命護盾百分比},
                .kind = ResourceChangeKind::Grant,
                .sourceShieldMaxHpPct = 生命護盾百分比}}}}};
    }
    std::string describe(EffectDescriptionStyle) const override
    {
        return std::format("每{}次出招：最低生命比例友軍獲最大生命{}%護盾，可選自身；持續至耗盡，同來源補滿不疊加。",
            出招次數, 生命護盾百分比);
    }
};

struct PoisonConversion final : GameplayEffectDefinition
{
    int 毒傷轉化百分比{};
    int 治療上限窗口幀數{};
    int 治療上限生命百分比{};
    static constexpr std::string_view Name = "化毒養身";
    static constexpr auto Parameters = std::array<Parameter<PoisonConversion>, 3>{{
        {{"毒傷轉化百分比", 0, 100}, &PoisonConversion::毒傷轉化百分比},
        {{"治療上限窗口幀數", 1, 1000000}, &PoisonConversion::治療上限窗口幀數},
        {{"治療上限生命百分比", 0, 1000000}, &PoisonConversion::治療上限生命百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.actions = {EffectAction{.value = StateMachineAction{
            ConfigurePoisonConversionAction{毒傷轉化百分比, 治療上限窗口幀數, 治療上限生命百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("自身毒傷{}%轉為吸血並等量扣傷；任意{}幀共用上限為血上限{}%，受禁療減療影響",
                毒傷轉化百分比, 治療上限窗口幀數, 治療上限生命百分比);
        return std::format("自身毒傷{}%轉為回血並等量扣除毒傷；任意{}幀內共回復至多血上限{}%。受禁療減療影響，未轉化部分仍造成傷害。",
            毒傷轉化百分比, 治療上限窗口幀數, 治療上限生命百分比);
    }
};

}    // namespace

void appendRecoveryEffects(std::vector<GameplayEffectRegistration>& entries)
{
    entries.push_back(registration<ClearMelodyShield>());
    entries.push_back(registration<PoisonConversion>());
    entries.push_back(registration<CastSelfHeal>());
    entries.push_back(registration<CastShield<BattleResource::Shield>>());
    entries.push_back(registration<CastShield<BattleResource::StatusShield>>());
    entries.push_back(registration<CastShield<BattleResource::StaggerShield>>());
    entries.push_back(registration<CastShield<BattleResource::StaggerShield, true>>());
    entries.push_back(registration<CastCleanse<false>>());
    entries.push_back(registration<CastCleanse<true>>());
    entries.push_back(registration<CastRestoreAndHeal>());
    entries.push_back(registration<CastStackPureDamage>());
    entries.push_back(registration<CastTeamMp>());
    entries.push_back(registration<TransferEnemyMp>());
    entries.push_back(registration<HealAndCleanseLowest>());
    entries.push_back(registration<CastDamageShield>());
    entries.push_back(registration<HealRemovePoisonBleed>());
    entries.push_back(registration<ReceivedDamageShield>());
    entries.push_back(registration<SkillLifeSteal>());
    entries.push_back(registration<HitMpRecovery>());
    entries.push_back(registration<PeriodicHealthRecovery>());
    entries.push_back(registration<LowHealthEmergencyHeal>());
    entries.push_back(registration<HitStealMp>());
    entries.push_back(registration<KillHeal>());
    entries.push_back(registration<HealingAura>());
    entries.push_back(registration<CooldownAura>());
    entries.push_back(registration<ChanceCastTeamMp>());
    entries.push_back(registration<ChanceCastTeamShield>());
    entries.push_back(registration<HitHeal>());
    entries.push_back(registration<HitAfflictedShield>());
    entries.push_back(registration<DeathHealMembers>());
    entries.push_back(registration<UltimateReadyTeamHeal>());
    entries.push_back(registration<ChanceHitTeamHeal>());
    entries.push_back(registration<CastDrainEnemyMp>());
    entries.push_back(registration<InitialHealthShield>());
}

}    // namespace KysChess::GameplayEffects
