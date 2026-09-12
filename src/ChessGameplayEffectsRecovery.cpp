#include "ChessGameplayEffectInternal.h"

namespace KysChess::GameplayEffects
{
namespace
{

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
            "出招：自身及內力最低{}友軍回{}內，滿內者每星回{}血", 友軍數, 回復內力, 每星治療);
    }
};

struct CastHealAndStackPureDamage final : GameplayEffectDefinition
{
    int 固定治療{};
    int 生命治療百分比{};
    int 每次層數{};
    int 層數上限{};
    int 每層純粹傷害{};
    static constexpr std::string_view Name = "出招回血疊加純粹傷害";
    static constexpr auto Parameters = std::array<Parameter<CastHealAndStackPureDamage>, 5>{
        {Parameter<CastHealAndStackPureDamage>{{"固定治療", 0, 1000000}, &CastHealAndStackPureDamage::固定治療},
         Parameter<CastHealAndStackPureDamage>{{"生命治療百分比", 0, 1000000},
                                               &CastHealAndStackPureDamage::生命治療百分比},
         Parameter<CastHealAndStackPureDamage>{{"每次層數", 1, 1000}, &CastHealAndStackPureDamage::每次層數},
         Parameter<CastHealAndStackPureDamage>{{"層數上限", 1, 1000}, &CastHealAndStackPureDamage::層數上限},
         Parameter<CastHealAndStackPureDamage>{{"每層純粹傷害", 0, 1000000},
                                               &CastHealAndStackPureDamage::每層純粹傷害}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions
            = {EffectAction{.value = ChangeResourceAction{.amount = EffectNumber{.base = EffectNumberBase::SourceMaxHp,
                                                                                 .flat = 固定治療,
                                                                                 .percent = 生命治療百分比}}},
               EffectAction{
                   .value = ApplyStatusAction{
                       .status = BattleStatusKind::TrueQi,
                       .quantity = AddStatusLayers{.count = 每次層數, .limit = 層數上限},
                       .behavior = std::make_shared<StatusBehaviorDefinition>(StatusBehaviorDefinition{
                           .rules = {EffectRule{
                               .id = EffectRuleId{.value = 1},
                               .event = EffectEvent::HitBeforeDamage,
                               .observation = EffectObservationScope::StatusHolderEventSource,
                               .selector = EffectSelector{.kind = EffectSelectorKind::HitTarget},
                               .actions = {EffectAction{
                                   .value = DealDamageAction{
                                       .amount = EffectNumber{.flat = 每層純粹傷害,
                                                              .statusScale = StatusNumberScale::PerContributionLayer},
                                       .kind = BattleDamageKind::Pure}}}}}})}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招回復{}生命與最大生命的{}%，並增加{}層命中加傷，最多{}層；每層命中附加{}"
                "純粹傷害。此來源獨立累積層數。",
                固定治療,
                生命治療百分比,
                每次層數,
                層數上限,
                每層純粹傷害);
        }
        return std::format("出招回血{}+血上限{}%，疊{}層（上限{}）；每層命中+{}純粹傷害",
                           固定治療,
                           生命治療百分比,
                           每次層數,
                           層數上限,
                           每層純粹傷害);
    }
};

struct CleanseAndProtect final : GameplayEffectDefinition
{
    int 狀態護盾{};
    int 僵直護盾{};
    static constexpr std::string_view Name = "淨化並抵抗控制";
    static constexpr auto Parameters = std::array<Parameter<CleanseAndProtect>, 2>{
        {Parameter<CleanseAndProtect>{{"狀態護盾", 0, 1000000}, &CleanseAndProtect::狀態護盾},
         Parameter<CleanseAndProtect>{{"僵直護盾", 0, 1000000}, &CleanseAndProtect::僵直護盾}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {
            EffectRule{.event = EffectEvent::AttackCommitted,
                       .actions = {EffectAction{.value = RemoveStatusAction{.negativeOnly = true}},
                                   EffectAction{.value = ChangeResourceAction{.resource = BattleResource::StatusShield,
                                                                              .amount = EffectNumber{.flat = 狀態護盾},
                                                                              .kind = ResourceChangeKind::Grant}},
                                   EffectAction{.value = ChangeResourceAction{.resource = BattleResource::StaggerShield,
                                                                              .amount = EffectNumber{.flat = 僵直護盾},
                                                                              .kind = ResourceChangeKind::Grant}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("出招清除自身所有負面效果，獲得{}狀態護盾及{}僵直護盾。", 狀態護盾, 僵直護盾);
        }
        return std::format("出招：清除負面，狀態盾+{}、僵直盾+{}", 狀態護盾, 僵直護盾);
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
            return std::format("出招：全隊內力+{}", 回復內力);
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
    int 生命治療百分比{};
    int 淨化數{};
    static constexpr std::string_view Name = "治療淨化低血友軍";
    static constexpr auto Parameters = std::array<Parameter<HealAndCleanseLowest>, 4>{
        {Parameter<HealAndCleanseLowest>{{"友軍數", 1, 1000000}, &HealAndCleanseLowest::友軍數},
         Parameter<HealAndCleanseLowest>{{"固定治療", 0, 1000000}, &HealAndCleanseLowest::固定治療},
         Parameter<HealAndCleanseLowest>{{"生命治療百分比", 0, 1000000}, &HealAndCleanseLowest::生命治療百分比},
         Parameter<HealAndCleanseLowest>{{"淨化數", 0, 1000000}, &HealAndCleanseLowest::淨化數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::LowestHpAllies, .count = 友軍數},
            .actions
            = {EffectAction{.value = ChangeResourceAction{.amount = EffectNumber{.base = EffectNumberBase::TargetMaxHp,
                                                                                 .flat = 固定治療,
                                                                                 .percent = 生命治療百分比}}},
               EffectAction{.value = RemoveStatusAction{.negativeOnly = true, .count = 淨化數}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招時，生命比例最低的{}名友軍回復{}生命與各自最大生命的{}%，並清除{}"
                "個負面效果。優先清除剩餘時間最長的負面效果。",
                友軍數,
                固定治療,
                生命治療百分比,
                淨化數);
        }
        return std::format("出招：血比最低{}友軍，回血{}+血上限{}%，清除{}個負面",
                           友軍數,
                           固定治療,
                           生命治療百分比,
                           淨化數);
    }
};

struct TeamCleanseControlHaste final : GameplayEffectDefinition
{
    int 僵直護盾{};
    int 速度百分比{};
    int 持續幀數{};
    static constexpr std::string_view Name = "全隊解控加速";
    static constexpr auto Parameters = std::array<Parameter<TeamCleanseControlHaste>, 3>{
        {Parameter<TeamCleanseControlHaste>{{"僵直護盾", 0, 1000000}, &TeamCleanseControlHaste::僵直護盾},
         Parameter<TeamCleanseControlHaste>{{"速度百分比", -1000000, 1000000}, &TeamCleanseControlHaste::速度百分比},
         Parameter<TeamCleanseControlHaste>{{"持續幀數", 1, 1000000}, &TeamCleanseControlHaste::持續幀數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .selector = EffectSelector{.kind = EffectSelectorKind::Allies},
            .actions
            = {EffectAction{.value = RemoveStatusAction{.controlOnly = true, .clearCurrentActionStagger = true}},
               EffectAction{.value = ChangeResourceAction{.resource = BattleResource::StaggerShield,
                                                          .amount = EffectNumber{.flat = 僵直護盾},
                                                          .kind = ResourceChangeKind::Grant}},
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
            return std::format(
                "出招解除全隊控制與當前僵直，獲得{}僵直護盾，速度{:+}%，持續{}"
                "幀。解除僵直保留當前位置與動作，重複加速刷新時間。",
                僵直護盾,
                速度百分比,
                持續幀數);
        }
        return std::format(
            "出招：全隊解控、解除僵直，僵直盾+{}，速度{:+}%持續{}幀", 僵直護盾, 速度百分比, 持續幀數);
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
            "出招：血比最低{}名友軍回血{}%生命上限，解毒、止血", 友軍數, 生命治療百分比);
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

struct CastStarShield final : GameplayEffectDefinition
{
    int 每星護盾{};
    static constexpr std::string_view Name = "出招每星護盾";
    static constexpr auto Parameters = std::array<Parameter<CastStarShield>, 1>{
        {Parameter<CastStarShield>{{"每星護盾", 0, 1000000}, &CastStarShield::每星護盾}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::AttackCommitted,
                           .actions = {EffectAction{.value = ChangeResourceAction{
                                                        .resource = BattleResource::Shield,
                                                        .amount = EffectNumber{.base = EffectNumberBase::SourceStar,
                                                                               .percent = 每星護盾 * 100},
                                                        .kind = ResourceChangeKind::Grant}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("出招：每星護盾+{}", 每星護盾);
        return std::format("出招獲得每星{}護盾。", 每星護盾);
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

struct CastBlockAndShield final : GameplayEffectDefinition
{
    int 格擋百分比{};
    int 持續幀數{};
    int 護盾點數{};
    static constexpr std::string_view Name = "出招格擋護盾";
    static constexpr auto Parameters = std::array<Parameter<CastBlockAndShield>, 3>{
        {Parameter<CastBlockAndShield>{{"格擋百分比", -1000000, 1000000}, &CastBlockAndShield::格擋百分比},
         Parameter<CastBlockAndShield>{{"持續幀數", 1, 1000000}, &CastBlockAndShield::持續幀數},
         Parameter<CastBlockAndShield>{{"護盾點數", 0, 1000000}, &CastBlockAndShield::護盾點數}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{
            .event = EffectEvent::AttackCommitted,
            .actions = {EffectAction{.value = ModifyAttributeAction{.attribute = BattleAttribute::BlockChance,
                                                                    .amount = EffectNumber{.flat = 格擋百分比},
                                                                    .operation = AttributeOperation::PercentagePointAdd,
                                                                    .durationFrames = 持續幀數,
                                                                    .stack = EffectStackPolicy::Refresh}},
                        EffectAction{.value = ChangeResourceAction{.resource = BattleResource::Shield,
                                                                   .amount = EffectNumber{.flat = 護盾點數},
                                                                   .kind = ResourceChangeKind::Grant}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format(
                "出招獲得{}護盾，格擋率{:+}%，持續{}幀。重複格擋加成刷新時間。", 護盾點數, 格擋百分比, 持續幀數);
        }
        return std::format("出招：護盾+{}，格擋{:+}%持續{}幀", 護盾點數, 格擋百分比, 持續幀數);
    }
};

struct CastMaxHealthHeal final : GameplayEffectDefinition
{
    int 生命治療百分比{};
    static constexpr std::string_view Name = "出招按生命回血";
    static constexpr auto Parameters = std::array<Parameter<CastMaxHealthHeal>, 1>{
        {Parameter<CastMaxHealthHeal>{{"生命治療百分比", 0, 1000000}, &CastMaxHealthHeal::生命治療百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::AttackCommitted,
                           .actions = {EffectAction{.value = ChangeResourceAction{
                                                        .amount = EffectNumber{.base = EffectNumberBase::SourceMaxHp,
                                                                               .percent = 生命治療百分比}}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Compact)
            return std::format("出招：回復血上限{}%", 生命治療百分比);
        return std::format("出招回復自身最大生命的{}%。", 生命治療百分比);
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
            return std::format("命中回{}內", 內力);
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
            return std::format("每{}幀回血上限{}%", 間隔幀數, 生命百分比);
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
        return std::format("首次血量<{}%：回復血上限{}%", 生命門檻百分比, 治療生命百分比);
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
            return std::format("命中奪{}內", 內力);
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
            return std::format("擊殺：回復血上限{}%", 生命百分比);
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

struct HealingCooldownAura final : GameplayEffectDefinition
{
    int 半徑格數{};
    int 間隔幀數{};
    int 治療點數{};
    int 冷卻百分比{};
    static constexpr std::string_view Name = "治療減冷卻光環";
    static constexpr auto Parameters = std::array<Parameter<HealingCooldownAura>, 4>{
        {Parameter<HealingCooldownAura>{{"半徑格數", 1, 1000000}, &HealingCooldownAura::半徑格數},
         Parameter<HealingCooldownAura>{{"間隔幀數", 1, 1000000}, &HealingCooldownAura::間隔幀數},
         Parameter<HealingCooldownAura>{{"治療點數", 0, 1000000}, &HealingCooldownAura::治療點數},
         Parameter<HealingCooldownAura>{{"冷卻百分比", 0, 1000000}, &HealingCooldownAura::冷卻百分比}}};
    std::string_view name() const override { return Name; }
    std::vector<EffectRule> buildRules() const override
    {
        return {EffectRule{.event = EffectEvent::FrameAdvanced,
                           .selector = EffectSelector{.kind = EffectSelectorKind::UnitsInRadius,
                                                      .radiusTiles = 半徑格數,
                                                      .team = EffectTeamFilter::Ally,
                                                      .excludeOwner = true},
                           .intervalFrames = 間隔幀數,
                           .actions
                           = {EffectAction{.value = ChangeResourceAction{.amount = EffectNumber{.flat = 治療點數},
                                                                         .healKind = EffectHealKind::Aura}},
                              EffectAction{.value = ChangeResourceAction{
                                               .resource = BattleResource::ActiveCooldown,
                                               .amount = EffectNumber{.base = EffectNumberBase::TargetCurrentCooldown,
                                                                      .percent = 冷卻百分比,
                                                                      .rounding = EffectRounding::Ceil},
                                               .kind = ResourceChangeKind::Remove}}}}};
    }
    std::string describe(EffectDescriptionStyle style) const override
    {
        if (style == EffectDescriptionStyle::Full)
        {
            return std::format("每{}幀為{}格內其他友軍回復{}生命，並移除其當前冷卻的{}%。不影響自身。",
                               間隔幀數,
                               半徑格數,
                               治療點數,
                               冷卻百分比);
        }
        return std::format(
            "每{}幀：{}格內其他友軍回{}血、當前冷卻-{}%", 間隔幀數, 半徑格數, 治療點數, 冷卻百分比);
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
            return std::format("出招{}%機率：全隊內力+{}", 機率百分比, 內力);
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
            return std::format("出招{}%機率：全隊護盾≥{}", 機率百分比, 護盾點數);
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
            return std::format("命中回{}血", 生命);
        return std::format("有效命中後回復{}生命。", 生命);
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
            return std::format("絕招就緒：全隊回血{}", 生命);
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
            return std::format("命中{}%機率：全隊回血上限{}%", 機率百分比, 生命百分比);
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
            return std::format("出招：全敵內力-{}", 內力);
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
            return std::format("開場護盾=血上限{}%", 生命百分比);
        return std::format("開場獲得最大生命{}%的護盾。", 生命百分比);
    }
};

}    // namespace

void appendRecoveryEffects(std::vector<GameplayEffectRegistration>& entries)
{
    entries.push_back(registration<CastRestoreAndHeal>());
    entries.push_back(registration<CastHealAndStackPureDamage>());
    entries.push_back(registration<CleanseAndProtect>());
    entries.push_back(registration<CastTeamMp>());
    entries.push_back(registration<TransferEnemyMp>());
    entries.push_back(registration<HealAndCleanseLowest>());
    entries.push_back(registration<TeamCleanseControlHaste>());
    entries.push_back(registration<CastDamageShield>());
    entries.push_back(registration<HealRemovePoisonBleed>());
    entries.push_back(registration<ReceivedDamageShield>());
    entries.push_back(registration<CastStarShield>());
    entries.push_back(registration<SkillLifeSteal>());
    entries.push_back(registration<CastBlockAndShield>());
    entries.push_back(registration<CastMaxHealthHeal>());
    entries.push_back(registration<HitMpRecovery>());
    entries.push_back(registration<PeriodicHealthRecovery>());
    entries.push_back(registration<LowHealthEmergencyHeal>());
    entries.push_back(registration<HitStealMp>());
    entries.push_back(registration<KillHeal>());
    entries.push_back(registration<HealingAura>());
    entries.push_back(registration<HealingCooldownAura>());
    entries.push_back(registration<ChanceCastTeamMp>());
    entries.push_back(registration<ChanceCastTeamShield>());
    entries.push_back(registration<HitHeal>());
    entries.push_back(registration<DeathHealMembers>());
    entries.push_back(registration<UltimateReadyTeamHeal>());
    entries.push_back(registration<ChanceHitTeamHeal>());
    entries.push_back(registration<CastDrainEnemyMp>());
    entries.push_back(registration<InitialHealthShield>());
}

}    // namespace KysChess::GameplayEffects
