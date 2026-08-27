#include "ChessBattleEffects.h"
#include "ChessEffectAuthoringDescriptors.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <set>
#include <span>
#include <string_view>
#include <type_traits>

namespace KysChess
{

namespace
{

using namespace EffectAuthoring;

template <typename Enum>
constexpr AuthorEnumLabel authorLabel(std::string_view name, Enum value)
{
    return AuthorEnumLabel{ name, static_cast<std::int64_t>(value) };
}

template <typename Enum>
std::optional<Enum> parseLabel(
    std::string_view label,
    const AuthorEnumDescriptor& descriptor)
{
    const auto found = std::ranges::find(descriptor.labels, label, &AuthorEnumLabel::name);
    if (found == descriptor.labels.end()) return std::nullopt;
    return static_cast<Enum>(found->value);
}

bool validateKnownKeys(
    const YAML::Node& node,
    std::initializer_list<std::string_view> allowed,
    std::string& error)
{
    if (!node || !node.IsMap())
    {
        error = "節點必須是映射表";
        return false;
    }
    std::set<std::string> keys;
    for (const auto& entry : node)
    {
        const auto key = entry.first.as<std::string>();
        if (!keys.insert(key).second)
        {
            error = std::format("重複欄位「{}」", key);
            return false;
        }
        if (std::ranges::find(allowed, key) == allowed.end())
        {
            error = std::format("未知欄位「{}」", key);
            return false;
        }
    }
    return true;
}

bool validateUniqueKeys(const YAML::Node& node, std::string& error)
{
    if (!node || !node.IsMap())
    {
        error = "節點必須是映射表";
        return false;
    }
    std::set<std::string> keys;
    for (const auto& entry : node)
    {
        const auto key = entry.first.as<std::string>();
        if (!keys.insert(key).second)
        {
            error = std::format("重複欄位「{}」", key);
            return false;
        }
    }
    return true;
}

bool isDynamicPayloadKey(PayloadDynamicKeyClass keyClass, std::string_view key);

enum class AttackRuntimeBehaviorKind
{
    ProjectileBounce,
    NearbyTracking,
    DelayedAlternate,
    ExpandingSpiral,
};

enum class StateMachineMechanism
{
    ChangeStateValue,
    TransferStateValue,
    RecordMaximumSkillDamage,
    ConsumeRecordAsDamage,
    ConsumeRecordAsShield,
    StartDamageAbsorption,
    SettleDamageAbsorption,
    BorrowEffectRules,
    CopyAttackDefinition,
    SettleRemainingStatusDamage,
    GenerateClones,
    PreventDeath,
    ConfigureProtectReposition,
    ConfigureExecuteReposition,
};

static constexpr std::array activationScopeLabels{
    authorLabel("每次施放每個目標", EffectActivationScope::PerCastPerTarget),
};
static constexpr AuthorEnumDescriptor activationScopeEnum{
    "EffectActivationScope", activationScopeLabels,
};

static constexpr std::array battleDamageKindLabels{
    authorLabel("物理", BattleDamageKind::Physical),
    authorLabel("招式", BattleDamageKind::Skill),
    authorLabel("純粹", BattleDamageKind::Pure),
    authorLabel("中毒", BattleDamageKind::Poison),
    authorLabel("流血", BattleDamageKind::Bleed),
    authorLabel("特效", BattleDamageKind::Effect),
    authorLabel("反彈", BattleDamageKind::Reflected),
    authorLabel("處決", BattleDamageKind::Execute),
};
static constexpr AuthorEnumDescriptor battleDamageKindEnum{
    "BattleDamageKind", battleDamageKindLabels,
};

static constexpr std::array effectRoundingLabels{
    authorLabel("向零", EffectRounding::TowardZero),
    authorLabel("向下", EffectRounding::Floor),
    authorLabel("向上", EffectRounding::Ceil),
    authorLabel("四捨五入", EffectRounding::Nearest),
};
static constexpr AuthorEnumDescriptor effectRoundingEnum{
    "EffectRounding", effectRoundingLabels,
};

static constexpr std::array effectNumberBaseLabels{
    authorLabel("固定值", EffectNumberBase::Constant),
    authorLabel("來源星級", EffectNumberBase::SourceStar),
    authorLabel("來源攻擊", EffectNumberBase::SourceAttack),
    authorLabel("來源最大生命", EffectNumberBase::SourceMaxHp),
    authorLabel("來源已損生命比例", EffectNumberBase::SourceMissingHpRatio),
    authorLabel("來源目前內力比例", EffectNumberBase::SourceCurrentMpRatio),
    authorLabel("目標最大生命", EffectNumberBase::TargetMaxHp),
    authorLabel("目標目前生命", EffectNumberBase::TargetCurrentHp),
    authorLabel("目標目前護盾", EffectNumberBase::TargetCurrentShield),
    authorLabel("目標目前冷卻", EffectNumberBase::TargetCurrentCooldown),
    authorLabel("實際生命傷害", EffectNumberBase::FinalHpDamage),
    authorLabel("累計狀態值", EffectNumberBase::AccumulatedStateValue),
    authorLabel("來源狀態強度", EffectNumberBase::SourceStatusPotency),
    authorLabel("來源狀態層數", EffectNumberBase::SourceStatusStacks),
    authorLabel("狀態槽值", EffectNumberBase::StoredStateValue),
};
static constexpr AuthorEnumDescriptor effectNumberBaseEnum{
    "EffectNumberBase", effectNumberBaseLabels,
};

static constexpr std::array selectorKindLabels{
    authorLabel("自身", EffectSelectorKind::Self),
    authorLabel("來源單位", EffectSelectorKind::SourceUnit),
    authorLabel("交易目標", EffectSelectorKind::TransactionTarget),
    authorLabel("命中目標", EffectSelectorKind::HitTarget),
    authorLabel("原攻擊目標", EffectSelectorKind::OriginalAttackTarget),
    authorLabel("羈絆成員", EffectSelectorKind::ComboMembers),
    authorLabel("所有存活單位", EffectSelectorKind::AllLivingUnits),
    authorLabel("友軍", EffectSelectorKind::Allies),
    authorLabel("全隊", EffectSelectorKind::Allies),
    authorLabel("敵軍", EffectSelectorKind::Enemies),
    authorLabel("所有敵人", EffectSelectorKind::Enemies),
    authorLabel("最低生命友軍", EffectSelectorKind::LowestHpAllies),
    authorLabel("最低內力友軍", EffectSelectorKind::LowestMpAllies),
    authorLabel("最高內力敵人", EffectSelectorKind::HighestMpEnemy),
    authorLabel("最強敵人", EffectSelectorKind::StrongestEnemies),
    authorLabel("最近敵人", EffectSelectorKind::NearestEnemies),
    authorLabel("最遠敵人", EffectSelectorKind::FarthestEnemy),
    authorLabel("半徑內單位", EffectSelectorKind::UnitsInRadius),
    authorLabel("方形內單位", EffectSelectorKind::UnitsInSquare),
    authorLabel("指定武器友軍", EffectSelectorKind::AlliesUsingWeapon),
};
static constexpr AuthorEnumDescriptor selectorKindEnum{
    "EffectSelectorKind", selectorKindLabels,
};

static constexpr std::array teamFilterLabels{
    authorLabel("不限", EffectTeamFilter::Any),
    authorLabel("友方", EffectTeamFilter::Ally),
    authorLabel("敵方", EffectTeamFilter::Enemy),
};
static constexpr AuthorEnumDescriptor teamFilterEnum{
    "EffectTeamFilter", teamFilterLabels,
};
static constexpr std::array areaRelationLabels{
    authorLabel("友方", EffectTeamFilter::Ally),
    authorLabel("敵方", EffectTeamFilter::Enemy),
};
static constexpr AuthorEnumDescriptor areaRelationEnum{
    "AreaUnitRelation", areaRelationLabels,
};

static constexpr std::array tieBreakLabels{
    authorLabel("單位ID", EffectTieBreak::UnitId),
    authorLabel("戰鬥亂數", EffectTieBreak::BattleRandom),
};
static constexpr AuthorEnumDescriptor tieBreakEnum{
    "EffectTieBreak", tieBreakLabels,
};

static constexpr std::array requiredTargetLabels{
    authorLabel("自身", EffectRequiredTarget::Self),
    authorLabel("來源單位", EffectRequiredTarget::SourceUnit),
    authorLabel("交易目標", EffectRequiredTarget::TransactionTarget),
    authorLabel("命中目標", EffectRequiredTarget::HitTarget),
    authorLabel("原攻擊目標", EffectRequiredTarget::OriginalAttackTarget),
};
static constexpr AuthorEnumDescriptor requiredTargetEnum{
    "EffectRequiredTarget", requiredTargetLabels,
};

static constexpr std::array damagePerspectiveLabels{
    authorLabel("造成", DamagePerspective::Dealt),
    authorLabel("承受", DamagePerspective::Received),
};
static constexpr AuthorEnumDescriptor damagePerspectiveEnum{
    "DamagePerspective", damagePerspectiveLabels,
};

static constexpr std::array stackPolicyLabels{
    authorLabel("獨立", EffectStackPolicy::Independent),
    authorLabel("刷新", EffectStackPolicy::Refresh),
    authorLabel("取代", EffectStackPolicy::Replace),
    authorLabel("保留最強", EffectStackPolicy::KeepStrongest),
    authorLabel("增加層數", EffectStackPolicy::AddStack),
};
static constexpr AuthorEnumDescriptor stackPolicyEnum{
    "EffectStackPolicy", stackPolicyLabels,
};

static constexpr std::array statusKindLabels{
    authorLabel("中毒", BattleStatusKind::Poison),
    authorLabel("流血", BattleStatusKind::Bleed),
    authorLabel("眩暈", BattleStatusKind::Stun),
    authorLabel("封內", BattleStatusKind::MpBlocked),
    authorLabel("寒毒", BattleStatusKind::ColdPoison),
    authorLabel("枯骨", BattleStatusKind::WitheredBone),
    authorLabel("七星", BattleStatusKind::SevenStarMark),
    authorLabel("化勁", BattleStatusKind::NeutralizeForce),
    authorLabel("刺目", BattleStatusKind::Blinded),
    authorLabel("下一次攻擊落空", BattleStatusKind::NextAttackMiss),
    authorLabel("傷害抵擋", BattleStatusKind::DamageBlockLayer),
    authorLabel("單次承傷上限", BattleStatusKind::SingleHitCapLayer),
    authorLabel("戰意", BattleStatusKind::BattleSpirit),
    authorLabel("真氣", BattleStatusKind::TrueQi),
    authorLabel("毒爆", BattleStatusKind::PoisonExplosion),
    authorLabel("無影", BattleStatusKind::Shadowless),
    authorLabel("下一次攻擊必定暴擊", BattleStatusKind::NextAttackCritical),
};
static constexpr AuthorEnumDescriptor statusKindEnum{
    "BattleStatusKind", statusKindLabels,
};

static constexpr std::array damageChannelLabels{
    authorLabel("招式", DamageChannel::Skill),
    authorLabel("持續傷害", DamageChannel::Dot),
    authorLabel("特效", DamageChannel::Effect),
    authorLabel("反彈", DamageChannel::Reflected),
    authorLabel("全部", DamageChannel::All),
};
static constexpr AuthorEnumDescriptor damageChannelEnum{
    "DamageChannel", damageChannelLabels,
};

static constexpr std::array stateSlotLabels{
    authorLabel("最大招式生命傷害", EffectStateSlot::MaximumSkillHpDamage),
    authorLabel("本次施放最高生命傷害", EffectStateSlot::CastMaximumHpDamage),
    authorLabel("累計吸收傷害", EffectStateSlot::AbsorbedDamage),
    authorLabel("永久施放進展", EffectStateSlot::PermanentCastProgress),
};
static constexpr AuthorEnumDescriptor stateSlotEnum{
    "EffectStateSlot", stateSlotLabels,
};

static constexpr std::array resourceLabels{
    authorLabel("生命", BattleResource::Hp),
    authorLabel("內力", BattleResource::Mp),
    authorLabel("護盾", BattleResource::Shield),
    authorLabel("狀態護盾", BattleResource::StatusShield),
    authorLabel("僵直護盾", BattleResource::StaggerShield),
    authorLabel("目前冷卻", BattleResource::ActiveCooldown),
    authorLabel("僵直吸收幀數", BattleResource::ControlImmunityFrames),
    authorLabel("無敵幀數", BattleResource::InvincibilityFrames),
};
static constexpr AuthorEnumDescriptor resourceEnum{
    "BattleResource", resourceLabels,
};

static constexpr std::array resourceChangeKindLabels{
    authorLabel("回復", ResourceChangeKind::Restore),
    authorLabel("奪取", ResourceChangeKind::Drain),
    authorLabel("獲得", ResourceChangeKind::Grant),
    authorLabel("移除", ResourceChangeKind::Remove),
    authorLabel("轉移", ResourceChangeKind::Transfer),
    authorLabel("至少刷新至", ResourceChangeKind::RefreshToAtLeast),
};
static constexpr AuthorEnumDescriptor resourceChangeKindEnum{
    "ResourceChangeKind", resourceChangeKindLabels,
};

static constexpr std::array healKindLabels{
    authorLabel("直接", EffectHealKind::Direct),
    authorLabel("隊伍", EffectHealKind::Team),
    authorLabel("光環", EffectHealKind::Aura),
    authorLabel("命中", EffectHealKind::OnHit),
    authorLabel("擊殺獎勵", EffectHealKind::KillReward),
    authorLabel("死亡醫療", EffectHealKind::DeathMedical),
    authorLabel("救援", EffectHealKind::Rescue),
    authorLabel("生命回復", EffectHealKind::Regeneration),
    authorLabel("吸血", EffectHealKind::Lifesteal),
};
static constexpr AuthorEnumDescriptor healKindEnum{
    "EffectHealKind", healKindLabels,
};

static constexpr std::array healSourcePolicyLabels{
    authorLabel("來源必須存活", EffectHealSourcePolicy::RequireAlive),
    authorLabel("允許死亡來源", EffectHealSourcePolicy::AllowDead),
};
static constexpr AuthorEnumDescriptor healSourcePolicyEnum{
    "EffectHealSourcePolicy", healSourcePolicyLabels,
};

static constexpr std::array stackScopeLabels{
    authorLabel("共用", EffectStackScope::Shared),
    authorLabel("事件來源", EffectStackScope::EventSource),
};
static constexpr AuthorEnumDescriptor stackScopeEnum{
    "EffectStackScope", stackScopeLabels,
};

static constexpr std::array borrowedRuleActionCategoryLabels{
    authorLabel("屬性修正", BorrowedRuleActionCategory::AttributeModifier),
    authorLabel("傷害修正", BorrowedRuleActionCategory::DamageModifier),
    authorLabel("資源變更", BorrowedRuleActionCategory::ResourceChange),
    authorLabel("治療交易修正", BorrowedRuleActionCategory::HealTransactionModifier),
    authorLabel("狀態", BorrowedRuleActionCategory::Status),
    authorLabel("傷害", BorrowedRuleActionCategory::Damage),
    authorLabel("攻擊", BorrowedRuleActionCategory::Attack),
    authorLabel("強制移動", BorrowedRuleActionCategory::ForcedMovement),
    authorLabel("區域", BorrowedRuleActionCategory::Area),
    authorLabel("修改施放", BorrowedRuleActionCategory::Cast),
    authorLabel("狀態值", BorrowedRuleActionCategory::StateValue),
    authorLabel("傷害記憶", BorrowedRuleActionCategory::DamageMemory),
    authorLabel("傷害吸收", BorrowedRuleActionCategory::DamageAbsorption),
    authorLabel("狀態傷害結算", BorrowedRuleActionCategory::StatusDamageSettlement),
};
static constexpr AuthorEnumDescriptor borrowedRuleActionCategoryEnum{
    "BorrowedRuleActionCategory", borrowedRuleActionCategoryLabels,
};

static constexpr std::array copiedMagicConditionLabels{
    authorLabel("有絕招攻擊定義", CopiedMagicCondition::HasUltimateAttackDefinition),
    authorLabel("排除複製與借用遞迴", CopiedMagicCondition::ExcludesRecursiveEffects),
};
static constexpr AuthorEnumDescriptor copiedMagicConditionEnum{
    "CopiedMagicCondition", copiedMagicConditionLabels,
};

static constexpr std::array battleAttributeLabels{
    authorLabel("最大生命", BattleAttribute::MaxHp),
    authorLabel("攻擊", BattleAttribute::Attack),
    authorLabel("防禦", BattleAttribute::Defence),
    authorLabel("速度", BattleAttribute::Speed),
    authorLabel("暴擊率", BattleAttribute::CriticalChance),
    authorLabel("暴擊傷害", BattleAttribute::CriticalDamage),
    authorLabel("閃避率", BattleAttribute::DodgeChance),
    authorLabel("格擋率", BattleAttribute::BlockChance),
    authorLabel("傷害減免", BattleAttribute::DamageReduction),
    authorLabel("技能傷害", BattleAttribute::SkillDamage),
    authorLabel("彈道壓制傷害", BattleAttribute::ProjectilePressureDamage),
    authorLabel("冷卻縮減", BattleAttribute::CooldownReduction),
    authorLabel("內力回復加成", BattleAttribute::MpRecoveryBonus),
    authorLabel("僵直抗性", BattleAttribute::StaggerResistance),
    authorLabel("彈道反射率", BattleAttribute::ProjectileReflectChance),
    authorLabel("技能反彈百分比", BattleAttribute::SkillReflectPercent),
    authorLabel("格擋絕招反擊率", BattleAttribute::CounterUltimateBlockChance),
    authorLabel("閃避後暴擊", BattleAttribute::CriticalAfterDodge),
    authorLabel("滑步機率", BattleAttribute::DashChance),
    authorLabel("攻擊冷卻延長率", BattleAttribute::OutgoingCooldownExtensionChance),
    authorLabel("攻擊冷卻延長百分比", BattleAttribute::OutgoingCooldownExtensionPercent),
    authorLabel("受擊冷卻延長反擊率", BattleAttribute::IncomingCooldownExtensionChance),
    authorLabel("受擊冷卻延長百分比", BattleAttribute::IncomingCooldownExtensionPercent),
};
static constexpr AuthorEnumDescriptor battleAttributeEnum{
    "BattleAttribute", battleAttributeLabels,
};

static constexpr std::array attackPatternKindLabels{
    authorLabel("保留", AttackPatternKind::Preserve),
    authorLabel("扇形", AttackPatternKind::Fan),
    authorLabel("側翼", AttackPatternKind::Flanks),
    authorLabel("同落點延遲", AttackPatternKind::SamePointSequence),
    authorLabel("多目標", AttackPatternKind::MultiTarget),
    authorLabel("最近其他敵人殘影", AttackPatternKind::EchoNearestOthers),
};
static constexpr AuthorEnumDescriptor attackPatternKindEnum{
    "AttackPatternKind", attackPatternKindLabels,
};

static constexpr std::array propagationPolicyLabels{
    authorLabel("來源全部規則", CastPropagationPolicy::SourceRules),
    authorLabel("僅來源命中規則", CastPropagationPolicy::SourceHitRulesOnly),
    authorLabel("不傳播大招規則", CastPropagationPolicy::SuppressUltimateRules),
    authorLabel("借用大招規則", CastPropagationPolicy::BorrowedUltimateRules),
    authorLabel("不傳播效果規則", CastPropagationPolicy::NoEffectRules),
};
static constexpr AuthorEnumDescriptor propagationPolicyEnum{
    "CastPropagationPolicy", propagationPolicyLabels,
};

static constexpr std::array attackRuntimeBehaviorKindLabels{
    authorLabel("彈道彈射", AttackRuntimeBehaviorKind::ProjectileBounce),
    authorLabel("範圍追蹤", AttackRuntimeBehaviorKind::NearbyTracking),
    authorLabel("延遲替代攻擊", AttackRuntimeBehaviorKind::DelayedAlternate),
    authorLabel("擴張螺旋", AttackRuntimeBehaviorKind::ExpandingSpiral),
};
static constexpr AuthorEnumDescriptor attackRuntimeBehaviorKindEnum{
    "AttackRuntimeBehaviorKind", attackRuntimeBehaviorKindLabels,
};

static constexpr std::array areaModifierKindLabels{
    authorLabel("屬性修正", AreaModifierKind::Attribute),
    authorLabel("造成傷害修正", AreaModifierKind::OutgoingDamage),
    authorLabel("攻擊生成修正", AreaModifierKind::AttackSpawn),
    authorLabel("強制移動免疫", AreaModifierKind::ForcedMoveImmunity),
};
static constexpr AuthorEnumDescriptor areaModifierKindEnum{
    "AreaModifierKind", areaModifierKindLabels,
};

static constexpr std::array forceMoveDirectionLabels{
    authorLabel("遠離來源", ForceMoveDirection::AwayFromSource),
    authorLabel("接近來源", ForceMoveDirection::TowardSource),
};
static constexpr AuthorEnumDescriptor forceMoveDirectionEnum{
    "ForceMoveDirection", forceMoveDirectionLabels,
};
static constexpr std::array areaBlockedDirectionLabels{
    authorLabel("遠離來源", ForceMoveDirection::AwayFromSource),
    authorLabel("接近來源", ForceMoveDirection::TowardSource),
    authorLabel("接近指定點", ForceMoveDirection::TowardPoint),
};
static constexpr AuthorEnumDescriptor areaBlockedDirectionEnum{
    "AreaBlockedDirection", areaBlockedDirectionLabels,
};

static constexpr std::array areaOverlapPolicyLabels{
    authorLabel("相加", AreaOverlapPolicy::Add),
    authorLabel("保留最強", AreaOverlapPolicy::KeepStrongest),
    authorLabel("任一", AreaOverlapPolicy::Any),
};
static constexpr AuthorEnumDescriptor areaOverlapPolicyEnum{
    "AreaOverlapPolicy", areaOverlapPolicyLabels,
};

static constexpr std::array attributeOperationLabels{
    authorLabel("固定加算", AttributeOperation::FlatAdd),
    authorLabel("百分比加算", AttributeOperation::PercentAdd),
    authorLabel("覆寫", AttributeOperation::Override),
    authorLabel("乘算", AttributeOperation::Multiply),
    authorLabel("至少為", AttributeOperation::AtLeast),
};
static constexpr AuthorEnumDescriptor attributeOperationEnum{
    "AttributeOperation", attributeOperationLabels,
};

static constexpr std::array damageModifierPerspectiveLabels{
    authorLabel("造成", DamageModifierPerspective::Outgoing),
    authorLabel("承受", DamageModifierPerspective::Incoming),
};
static constexpr AuthorEnumDescriptor damageModifierPerspectiveEnum{
    "DamageModifierPerspective", damageModifierPerspectiveLabels,
};

static constexpr std::array damageModifierStageLabels{
    authorLabel("防禦前", DamageModifierStage::BeforeDefense),
    authorLabel("防禦後", DamageModifierStage::AfterDefense),
    authorLabel("最終", DamageModifierStage::Final),
};
static constexpr AuthorEnumDescriptor damageModifierStageEnum{
    "DamageModifierStage", damageModifierStageLabels,
};

static constexpr std::array damageModifierOperationLabels{
    authorLabel("固定加算", DamageModifierOperation::FlatAdd),
    authorLabel("百分比加算", DamageModifierOperation::PercentAdd),
    authorLabel("乘算", DamageModifierOperation::Multiply),
    authorLabel("忽略防禦百分比", DamageModifierOperation::IgnoreDefensePercent),
    authorLabel("單次承傷上限", DamageModifierOperation::CapSingleHitAtMaxHpPercent),
    authorLabel("低於最大生命百分比時處決", DamageModifierOperation::ExecuteBelowMaxHpPercent),
};
static constexpr AuthorEnumDescriptor damageModifierOperationEnum{
    "DamageModifierOperation", damageModifierOperationLabels,
};

static constexpr std::array healModifierOperationLabels{
    authorLabel("阻止", HealModifierOperation::Block),
    authorLabel("受到治療乘算", HealModifierOperation::MultiplyReceived),
};
static constexpr AuthorEnumDescriptor healModifierOperationEnum{
    "HealModifierOperation", healModifierOperationLabels,
};

static constexpr std::array statusSourceMatchLabels{
    authorLabel("不限", StatusSourceMatch::Any),
    authorLabel("效果擁有者", StatusSourceMatch::EffectOwner),
};
static constexpr AuthorEnumDescriptor statusSourceMatchEnum{
    "StatusSourceMatch", statusSourceMatchLabels,
};

static constexpr std::array statusRemovalOrderLabels{
    authorLabel("最長剩餘", StatusRemovalOrder::LongestRemaining),
    authorLabel("最舊", StatusRemovalOrder::Oldest),
    authorLabel("最新", StatusRemovalOrder::Newest),
};
static constexpr AuthorEnumDescriptor statusRemovalOrderEnum{
    "StatusRemovalOrder", statusRemovalOrderLabels,
};

static constexpr std::array damageAreaKindLabels{
    authorLabel("單體", DamageAreaKind::SingleTarget),
    authorLabel("圓形", DamageAreaKind::Circle),
    authorLabel("方形", DamageAreaKind::Square),
};
static constexpr AuthorEnumDescriptor damageAreaKindEnum{
    "DamageAreaKind", damageAreaKindLabels,
};

static constexpr std::array areaProjectileVisualLabels{
    authorLabel("死亡爆炸", AreaProjectileVisual::DeathBlast),
    authorLabel("護盾爆炸", AreaProjectileVisual::ShieldBlast),
};
static constexpr AuthorEnumDescriptor areaProjectileVisualEnum{
    "AreaProjectileVisual", areaProjectileVisualLabels,
};

static constexpr std::array attackTargetPolicyLabels{
    authorLabel("保留", AttackTargetPolicy::Preserve),
    authorLabel("選擇目標", AttackTargetPolicy::SelectedTargets),
    authorLabel("同落點", AttackTargetPolicy::SamePoint),
    authorLabel("同目標", AttackTargetPolicy::SameTarget),
};
static constexpr AuthorEnumDescriptor attackTargetPolicyEnum{
    "AttackTargetPolicy", attackTargetPolicyLabels,
};

static constexpr std::array forceMoveCollisionLabels{
    authorLabel("佔位前停止", ForceMoveCollision::StopBeforeOccupied),
    authorLabel("阻擋前停止", ForceMoveCollision::StopBeforeBlocked),
    authorLabel("遇阻停止", ForceMoveCollision::StopBeforeBlocked),
};
static constexpr AuthorEnumDescriptor forceMoveCollisionEnum{
    "ForceMoveCollision", forceMoveCollisionLabels,
};

static constexpr std::array forceMoveBlockedResultLabels{
    authorLabel("停止", ForceMoveBlockedResult::Stop),
    authorLabel("縮短", ForceMoveBlockedResult::Shorten),
};
static constexpr AuthorEnumDescriptor forceMoveBlockedResultEnum{
    "ForceMoveBlockedResult", forceMoveBlockedResultLabels,
};

static constexpr std::array areaShapeLabels{
    authorLabel("圓形", AreaShape::Circle),
    authorLabel("棋格方形", AreaShape::GridSquare),
};
static constexpr AuthorEnumDescriptor areaShapeEnum{
    "AreaShape", areaShapeLabels,
};

static constexpr std::array areaAnchorLabels{
    authorLabel("命中位置", AreaAnchor::HitPosition),
    authorLabel("跟隨來源", AreaAnchor::FollowSourceUnit),
};
static constexpr AuthorEnumDescriptor areaAnchorEnum{
    "AreaAnchor", areaAnchorLabels,
};

static constexpr std::array areaSourceDeathPolicyLabels{
    authorLabel("保留至到期", AreaSourceDeathPolicy::PersistUntilExpiry),
    authorLabel("立即移除", AreaSourceDeathPolicy::RemoveImmediately),
};
static constexpr AuthorEnumDescriptor areaSourceDeathPolicyEnum{
    "AreaSourceDeathPolicy", areaSourceDeathPolicyLabels,
};

static constexpr std::array areaMergePolicyLabels{
    authorLabel("獨立", AreaMergePolicy::Independent),
    authorLabel("同來源刷新", AreaMergePolicy::RefreshSameSource),
    authorLabel("同來源取代", AreaMergePolicy::ReplaceSameSource),
};
static constexpr AuthorEnumDescriptor areaMergePolicyEnum{
    "AreaMergePolicy", areaMergePolicyLabels,
};

static constexpr std::array castRangeModeLabels{
    authorLabel("保留", CastRangeMode::Preserve),
    authorLabel("遠程", CastRangeMode::Ranged),
};
static constexpr AuthorEnumDescriptor castRangeModeEnum{
    "CastRangeMode", castRangeModeLabels,
};

static constexpr std::array castMobilityPolicyLabels{
    authorLabel("保留", CastMobilityPolicy::Preserve),
    authorLabel("滑步攻擊", CastMobilityPolicy::DashAttack),
    authorLabel("閃擊", CastMobilityPolicy::BlinkAttack),
};
static constexpr AuthorEnumDescriptor castMobilityPolicyEnum{
    "CastMobilityPolicy", castMobilityPolicyLabels,
};

static constexpr std::array stateMachineMechanismLabels{
    authorLabel("變更狀態值", StateMachineMechanism::ChangeStateValue),
    authorLabel("轉移狀態值", StateMachineMechanism::TransferStateValue),
    authorLabel("記錄最大招式生命傷害", StateMachineMechanism::RecordMaximumSkillDamage),
    authorLabel("消耗記錄為傷害", StateMachineMechanism::ConsumeRecordAsDamage),
    authorLabel("消耗記錄為護盾", StateMachineMechanism::ConsumeRecordAsShield),
    authorLabel("開始傷害吸收", StateMachineMechanism::StartDamageAbsorption),
    authorLabel("結算傷害吸收", StateMachineMechanism::SettleDamageAbsorption),
    authorLabel("借用效果規則", StateMachineMechanism::BorrowEffectRules),
    authorLabel("複製攻擊定義", StateMachineMechanism::CopyAttackDefinition),
    authorLabel("結算剩餘狀態傷害", StateMachineMechanism::SettleRemainingStatusDamage),
    authorLabel("生成分身", StateMachineMechanism::GenerateClones),
    authorLabel("死亡庇護", StateMachineMechanism::PreventDeath),
    authorLabel("保護挪移", StateMachineMechanism::ConfigureProtectReposition),
    authorLabel("處決挪移", StateMachineMechanism::ConfigureExecuteReposition),
};
static constexpr AuthorEnumDescriptor stateMachineMechanismEnum{
    "StateMachineMechanism", stateMachineMechanismLabels,
};

static constexpr std::array observationScopeLabels{
    authorLabel("效果擁有者", EffectObservationScope::Owner),
    authorLabel("效果擁有者同隊事件來源", EffectObservationScope::OwnerTeamEventSource),
    authorLabel("事件目標", EffectObservationScope::EventTarget),
};
static constexpr AuthorEnumDescriptor observationScopeEnum{
    "EffectObservationScope", observationScopeLabels,
};

static constexpr std::array castMatchLabels{
    authorLabel("綁定武功", EffectCastMatch::BoundMagic),
    authorLabel("效果擁有者任意施放", EffectCastMatch::OwnerAnyCast),
};
static constexpr AuthorEnumDescriptor castMatchEnum{
    "EffectCastMatch", castMatchLabels,
};

static constexpr std::array authorEnumDescriptors{
    &activationScopeEnum,
    &battleDamageKindEnum,
    &effectRoundingEnum,
    &effectNumberBaseEnum,
    &selectorKindEnum,
    &teamFilterEnum,
    &areaRelationEnum,
    &tieBreakEnum,
    &requiredTargetEnum,
    &damagePerspectiveEnum,
    &stackPolicyEnum,
    &statusKindEnum,
    &damageChannelEnum,
    &stateSlotEnum,
    &resourceEnum,
    &resourceChangeKindEnum,
    &healKindEnum,
    &healSourcePolicyEnum,
    &stackScopeEnum,
    &borrowedRuleActionCategoryEnum,
    &copiedMagicConditionEnum,
    &battleAttributeEnum,
    &attackPatternKindEnum,
    &propagationPolicyEnum,
    &attackRuntimeBehaviorKindEnum,
    &areaModifierKindEnum,
    &forceMoveDirectionEnum,
    &areaBlockedDirectionEnum,
    &areaOverlapPolicyEnum,
    &attributeOperationEnum,
    &damageModifierPerspectiveEnum,
    &damageModifierStageEnum,
    &damageModifierOperationEnum,
    &healModifierOperationEnum,
    &statusSourceMatchEnum,
    &statusRemovalOrderEnum,
    &damageAreaKindEnum,
    &areaProjectileVisualEnum,
    &attackTargetPolicyEnum,
    &forceMoveCollisionEnum,
    &forceMoveBlockedResultEnum,
    &areaShapeEnum,
    &areaAnchorEnum,
    &areaSourceDeathPolicyEnum,
    &areaMergePolicyEnum,
    &castRangeModeEnum,
    &castMobilityPolicyEnum,
    &stateMachineMechanismEnum,
    &observationScopeEnum,
    &castMatchEnum,
};

bool validatePayloadNodeShape(
    const YAML::Node& node,
    const PayloadFieldDescriptor& field,
    std::string& error)
{
    const auto reject = [&]
    {
        error = std::format("「{}」欄位外形不符合 payload descriptor", field.name);
        return false;
    };
    switch (field.shape)
    {
    case PayloadNodeShape::Any:
        return true;
    case PayloadNodeShape::Scalar:
    case PayloadNodeShape::String:
        return node.IsScalar() || reject();
    case PayloadNodeShape::Integer:
        if (!node.IsScalar()) return reject();
        try
        {
            static_cast<void>(node.as<int>());
            return true;
        }
        catch (const YAML::Exception&)
        {
            return reject();
        }
    case PayloadNodeShape::Boolean:
        if (!node.IsScalar()) return reject();
        try
        {
            static_cast<void>(node.as<bool>());
            return true;
        }
        catch (const YAML::Exception&)
        {
            return reject();
        }
    case PayloadNodeShape::Map:
    case PayloadNodeShape::ActionNode:
        return node.IsMap() || reject();
    case PayloadNodeShape::Sequence:
    case PayloadNodeShape::ActionList:
    case PayloadNodeShape::ConditionList:
        return node.IsSequence() || reject();
    case PayloadNodeShape::Number:
    case PayloadNodeShape::Selector:
        return (node.IsScalar() || node.IsMap()) || reject();
    case PayloadNodeShape::StringOrSequence:
        return (node.IsScalar() || node.IsSequence()) || reject();
    }
    assert(false);
    return false;
}

class PayloadView
{
public:
    PayloadView(const YAML::Node& node, const PayloadDescriptor& descriptor)
        : node_(node), descriptor_(descriptor)
    {
    }

    bool validate(std::string& error) const
    {
        if (!node_ || !node_.IsMap())
        {
            error = std::format("payload「{}」必須是映射表", descriptor_.name);
            return false;
        }
        std::set<std::string> supplied;
        for (const auto& entry : node_)
        {
            const auto key = entry.first.as<std::string>();
            if (!supplied.insert(key).second)
            {
                error = std::format("重複欄位「{}」", key);
                return false;
            }
            const auto* field = findField(key);
            if (!field)
            {
                if (isDynamicPayloadKey(descriptor_.dynamicKeyClass, key))
                {
                    const PayloadFieldDescriptor dynamicField{
                        key, true, descriptor_.dynamicValueShape,
                    };
                    if (!validatePayloadNodeShape(entry.second, dynamicField, error)) return false;
                    continue;
                }
                error = std::format("未知欄位「{}」", key);
                return false;
            }
            if (!validatePayloadNodeShape(entry.second, *field, error)) return false;
        }
        for (const auto& field : descriptor_.fields)
        {
            if (field.required && !node_[std::string(field.name)])
            {
                error = std::format("缺少「{}」欄位", field.name);
                return false;
            }
        }
        return true;
    }

    YAML::Node operator[](std::string_view key) const
    {
        const auto field = node_[std::string(key)];
        if (field) consumed_.insert(std::string(key));
        return field;
    }

    std::vector<std::pair<std::string, YAML::Node>> dynamicEntries() const
    {
        std::vector<std::pair<std::string, YAML::Node>> entries;
        for (const auto& entry : node_)
        {
            const auto key = entry.first.as<std::string>();
            if (findField(key)) continue;
            assert(isDynamicPayloadKey(descriptor_.dynamicKeyClass, key));
            consumed_.insert(key);
            entries.emplace_back(key, entry.second);
        }
        return entries;
    }

    bool finish(std::string& error) const
    {
        for (const auto& entry : node_)
        {
            const auto key = entry.first.as<std::string>();
            if (!consumed_.contains(key))
            {
                error = std::format(
                    "payload「{}」的欄位「{}」通過外形驗證但未被 typed parser 消耗",
                    descriptor_.name,
                    key);
                return false;
            }
        }
        return true;
    }

private:
    const PayloadFieldDescriptor* findField(std::string_view name) const
    {
        const auto found = std::ranges::find(descriptor_.fields, name, &PayloadFieldDescriptor::name);
        return found == descriptor_.fields.end() ? nullptr : &*found;
    }

    YAML::Node node_;
    const PayloadDescriptor& descriptor_;
    mutable std::set<std::string> consumed_;
};

template <typename Node>
bool requiredString(const Node& node, std::string_view key, std::string& value, std::string& error)
{
    const auto field = node[std::string(key)];
    if (!field)
    {
        error = std::format("缺少「{}」欄位", key);
        return false;
    }
    try
    {
        value = field.as<std::string>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("「{}」不是有效字串: {}", key, ex.what());
        return false;
    }
}

template <typename Node>
bool optionalInt(const Node& node, std::string_view key, int& value, std::string& error)
{
    const auto field = node[std::string(key)];
    if (!field) return true;
    try
    {
        value = field.as<int>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("「{}」不是有效整數: {}", key, ex.what());
        return false;
    }
}

template <typename Node>
bool requiredInt(const Node& node, std::string_view key, int& value, std::string& error)
{
    if (!node[std::string(key)])
    {
        error = std::format("缺少「{}」欄位", key);
        return false;
    }
    return optionalInt(node, key, value, error);
}

static constexpr std::array activationLimitFields{
    PayloadFieldDescriptor{
        "範圍", true, PayloadNodeShape::String, "每次施放每個目標", {},
        PayloadSchemaReference::None, &activationScopeEnum,
    },
    PayloadFieldDescriptor{ "次數", true, PayloadNodeShape::Integer, "1" },
};
static constexpr PayloadDescriptor activationLimitPayload{
    "觸發限制", activationLimitFields, R"(範圍: 每次施放每個目標
次數: 1)",
};

bool parseActivationLimitNode(
    const YAML::Node& node,
    EffectActivationLimit& out,
    std::string& error)
{
    PayloadView payload(node, activationLimitPayload);
    if (!payload.validate(error)) return false;

    std::string scopeLabel;
    if (!requiredString(payload, "範圍", scopeLabel, error)) return false;
    const auto scope = parseLabel<EffectActivationScope>(scopeLabel, activationScopeEnum);
    if (!scope)
    {
        error = std::format("未知觸發限制範圍「{}」", scopeLabel);
        return false;
    }

    out = {};
    out.scope = *scope;
    return requiredInt(payload, "次數", out.maxEvaluations, error)
        && payload.finish(error);
}

template <typename Node>
bool optionalBool(const Node& node, std::string_view key, bool& value, std::string& error)
{
    const auto field = node[std::string(key)];
    if (!field) return true;
    try
    {
        value = field.as<bool>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("「{}」不是有效布林值: {}", key, ex.what());
        return false;
    }
}

template <typename Node>
bool optionalBool(const Node& node,
                  std::string_view key,
                  std::optional<bool>& value,
                  std::string& error)
{
    const auto field = node[std::string(key)];
    if (!field) return true;
    try
    {
        value = field.as<bool>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("「{}」不是有效布林值: {}", key, ex.what());
        return false;
    }
}

bool parseDamageKindLabel(
    std::string_view label,
    BattleDamageKind& out,
    std::string& error)
{
    const auto parsed = parseLabel<BattleDamageKind>(label, battleDamageKindEnum);
    if (!parsed)
    {
        error = std::format("未知傷害種類「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseStatusKind(std::string_view label, BattleStatusKind& out, std::string& error);
bool parseEffectStateSlot(const YAML::Node& node, EffectStateSlot& out, std::string& error);

static constexpr std::array effectNumberFields{
    PayloadFieldDescriptor{
        "基準", false, PayloadNodeShape::String, "來源攻擊", {},
        PayloadSchemaReference::None, &effectNumberBaseEnum,
    },
    PayloadFieldDescriptor{
        "乘數基準", false, PayloadNodeShape::String, "來源星級", {},
        PayloadSchemaReference::None, &effectNumberBaseEnum,
    },
    PayloadFieldDescriptor{
        "狀態", false, PayloadNodeShape::String, "中毒", {},
        PayloadSchemaReference::None, &statusKindEnum,
    },
    PayloadFieldDescriptor{
        "狀態槽", false, PayloadNodeShape::String, "最大招式生命傷害", {},
        PayloadSchemaReference::None, &stateSlotEnum,
    },
    PayloadFieldDescriptor{ "固定", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{ "百分比", false, PayloadNodeShape::Integer, "100" },
    PayloadFieldDescriptor{
        "取整", false, PayloadNodeShape::String, "向下", {},
        PayloadSchemaReference::None, &effectRoundingEnum,
    },
    PayloadFieldDescriptor{ "最小", false, PayloadNodeShape::Integer, "0" },
    PayloadFieldDescriptor{ "最大", false, PayloadNodeShape::Integer, "100" },
    PayloadFieldDescriptor{ "目標最大生命百分比", false, PayloadNodeShape::Integer, "10", "{}" },
    PayloadFieldDescriptor{ "來源最大生命百分比", false, PayloadNodeShape::Integer, "10", "{}" },
    PayloadFieldDescriptor{ "目標目前生命百分比", false, PayloadNodeShape::Integer, "10", "{}" },
    PayloadFieldDescriptor{ "目標目前護盾百分比", false, PayloadNodeShape::Integer, "10", "{}" },
    PayloadFieldDescriptor{ "實際生命傷害百分比", false, PayloadNodeShape::Integer, "10", "{}" },
    PayloadFieldDescriptor{ "每星級", false, PayloadNodeShape::Integer, "10", "{}" },
};
static constexpr PayloadDescriptor effectNumberPayload{
    "效果數值", effectNumberFields, "固定: 1", PayloadDynamicKeyClass::None,
    PayloadNodeShape::Any, {}, {}, 1,
};

bool parseEffectNumberNode(const YAML::Node& node, EffectNumber& out, std::string& error)
{
    if (!node)
    {
        error = "缺少「數值」欄位";
        return false;
    }
    if (node.IsScalar())
    {
        try
        {
            out = {};
            out.flat = node.as<int>();
            return true;
        }
        catch (const YAML::Exception& ex)
        {
            error = std::format("數值不是有效整數: {}", ex.what());
            return false;
        }
    }

    PayloadView payload(node, effectNumberPayload);
    if (!payload.validate(error)) return false;

    struct NumberAlias
    {
        std::string_view field;
        EffectNumberBase base;
        int percentMultiplier;
    };
    static constexpr std::array aliases{
        NumberAlias{ "目標最大生命百分比", EffectNumberBase::TargetMaxHp, 1 },
        NumberAlias{ "來源最大生命百分比", EffectNumberBase::SourceMaxHp, 1 },
        NumberAlias{ "目標目前生命百分比", EffectNumberBase::TargetCurrentHp, 1 },
        NumberAlias{ "目標目前護盾百分比", EffectNumberBase::TargetCurrentShield, 1 },
        NumberAlias{ "實際生命傷害百分比", EffectNumberBase::FinalHpDamage, 1 },
        NumberAlias{ "每星級", EffectNumberBase::SourceStar, 100 },
    };
    const NumberAlias* matchedAlias = nullptr;
    for (const auto& alias : aliases)
    {
        if (!payload[alias.field]) continue;
        if (matchedAlias)
        {
            error = "簡式數值只能使用一個基準別名";
            return false;
        }
        matchedAlias = &alias;
    }
    if (matchedAlias)
    {
        int value{};
        if (!requiredInt(payload, matchedAlias->field, value, error)) return false;
        if (matchedAlias->percentMultiplier != 1
            && (value > std::numeric_limits<int>::max() / matchedAlias->percentMultiplier
                || value < std::numeric_limits<int>::min() / matchedAlias->percentMultiplier))
        {
            error = std::format("「{}」超出有效範圍", matchedAlias->field);
            return false;
        }
        out = {};
        out.base = matchedAlias->base;
        out.percent = value * matchedAlias->percentMultiplier;
        if (const auto rounding = payload["取整"])
        {
            const auto label = rounding.as<std::string>();
            const auto parsed = parseLabel<EffectRounding>(label, effectRoundingEnum);
            if (!parsed)
            {
                error = std::format("未知取整方式「{}」", label);
                return false;
            }
            out.rounding = *parsed;
        }
        return payload.finish(error);
    }
    out = {};
    auto readBase = [&](const YAML::Node& base, EffectNumberBase& destination)
    {
        const auto label = base.as<std::string>();
        const auto parsed = parseLabel<EffectNumberBase>(label, effectNumberBaseEnum);
        if (!parsed)
        {
            error = std::format("未知數值基準「{}」", label);
            return false;
        }
        destination = *parsed;
        return true;
    };
    if (const auto base = payload["基準"])
    {
        if (!readBase(base, out.base)) return false;
    }
    if (const auto multiplier = payload["乘數基準"])
    {
        EffectNumberBase parsedMultiplier{};
        if (!readBase(multiplier, parsedMultiplier)) return false;
        out.multiplierBase = parsedMultiplier;
    }
    if (const auto status = payload["狀態"])
    {
        BattleStatusKind parsedStatus{};
        const auto label = status.as<std::string>();
        if (!parseStatusKind(label, parsedStatus, error)) return false;
        out.status = std::string(battleStatusLabel(parsedStatus));
    }
    if (payload["狀態槽"])
    {
        EffectStateSlot slot{};
        if (!parseEffectStateSlot(payload["狀態槽"], slot, error)) return false;
        out.stateSlot = slot;
    }
    if (!optionalInt(payload, "固定", out.flat, error)
        || !optionalInt(payload, "百分比", out.percent, error)) return false;
    if (const auto rounding = payload["取整"])
    {
        const auto label = rounding.as<std::string>();
        const auto parsed = parseLabel<EffectRounding>(label, effectRoundingEnum);
        if (!parsed)
        {
            error = std::format("未知取整方式「{}」", label);
            return false;
        }
        out.rounding = *parsed;
    }
    if (payload["最小"])
    {
        int value{};
        if (!requiredInt(payload, "最小", value, error)) return false;
        out.minimum = value;
    }
    if (payload["最大"])
    {
        int value{};
        if (!requiredInt(payload, "最大", value, error)) return false;
        out.maximum = value;
    }
    return payload.finish(error);
}

static constexpr std::array selectorFields{
    PayloadFieldDescriptor{
        "類型", true, PayloadNodeShape::String, "半徑內單位", {},
        PayloadSchemaReference::None, &selectorKindEnum,
    },
    PayloadFieldDescriptor{ "數量", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "半徑格數", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{
        "方形邊長", false, PayloadNodeShape::Integer, "3", "類型: 方形內單位",
    },
    PayloadFieldDescriptor{
        "隊伍", false, PayloadNodeShape::String, "敵方", {},
        PayloadSchemaReference::None, &teamFilterEnum,
    },
    PayloadFieldDescriptor{
        "平手", false, PayloadNodeShape::String, "戰鬥亂數", {},
        PayloadSchemaReference::None, &tieBreakEnum,
    },
    PayloadFieldDescriptor{ "排除效果擁有者", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "武功", false, PayloadNodeShape::Integer, "26", "類型: 羈絆成員" },
    PayloadFieldDescriptor{
        "武器類型", false, PayloadNodeShape::Integer, "1", "類型: 指定武器友軍",
    },
    PayloadFieldDescriptor{
        "必含目標", false, PayloadNodeShape::String, "自身", {},
        PayloadSchemaReference::None, &requiredTargetEnum,
    },
};
static constexpr PayloadDescriptor selectorPayload{
    "目標選擇器", selectorFields, R"(類型: 半徑內單位
半徑格數: 3)",
};

bool parseSelectorNode(const YAML::Node& node, EffectSelector& out, std::string& error)
{
    out = {};
    std::string label;
    std::optional<PayloadView> payload;
    if (node.IsScalar()) label = node.as<std::string>();
    else
    {
        payload.emplace(node, selectorPayload);
        if (!payload->validate(error)
            || !requiredString(*payload, "類型", label, error)) return false;
    }

    const auto kind = parseLabel<EffectSelectorKind>(label, selectorKindEnum);
    if (!kind)
    {
        error = std::format("未知目標選擇器「{}」", label);
        return false;
    }
    out.kind = *kind;
    if (payload)
    {
        bool acceptsCount{};
        bool acceptsTie{};
        bool acceptsMagic{};
        bool acceptsRequiredTarget{};
        bool acceptsRadius{};
        bool acceptsSquare{};
        bool acceptsTeam{};
        bool acceptsWeapon{};
        switch (out.kind)
        {
        case EffectSelectorKind::Self:
        case EffectSelectorKind::SourceUnit:
        case EffectSelectorKind::TransactionTarget:
        case EffectSelectorKind::HitTarget:
        case EffectSelectorKind::OriginalAttackTarget:
            break;
        case EffectSelectorKind::ComboMembers:
        case EffectSelectorKind::AllLivingUnits:
        case EffectSelectorKind::Allies:
        case EffectSelectorKind::Enemies:
            acceptsMagic = true;
            [[fallthrough]];
        case EffectSelectorKind::LowestHpAllies:
        case EffectSelectorKind::LowestMpAllies:
        case EffectSelectorKind::StrongestEnemies:
        case EffectSelectorKind::NearestEnemies:
            acceptsCount = true;
            acceptsTie = true;
            acceptsRequiredTarget = true;
            break;
        case EffectSelectorKind::HighestMpEnemy:
        case EffectSelectorKind::FarthestEnemy:
            acceptsTie = true;
            break;
        case EffectSelectorKind::UnitsInRadius:
            acceptsRadius = true;
            [[fallthrough]];
        case EffectSelectorKind::UnitsInSquare:
            acceptsSquare = out.kind == EffectSelectorKind::UnitsInSquare;
            acceptsCount = true;
            acceptsTie = true;
            acceptsTeam = true;
            acceptsRequiredTarget = true;
            break;
        case EffectSelectorKind::AlliesUsingWeapon:
            acceptsWeapon = true;
            acceptsRequiredTarget = true;
            break;
        }
        if (!optionalBool(*payload, "排除效果擁有者", out.excludeOwner, error)
            || (acceptsCount && !optionalInt(*payload, "數量", out.count, error))
            || (acceptsRadius && !optionalInt(*payload, "半徑格數", out.radiusTiles, error))
            || (acceptsSquare && !optionalInt(*payload, "方形邊長", out.squareSideTiles, error))
            || (acceptsMagic && !optionalInt(*payload, "武功", out.requiredMagicId, error))
            || (acceptsWeapon && !optionalInt(*payload, "武器類型", out.requiredWeaponType, error))) return false;
        if (acceptsTeam)
        {
            if (const auto team = (*payload)["隊伍"])
            {
                const auto teamLabel = team.as<std::string>();
                const auto parsed = parseLabel<EffectTeamFilter>(teamLabel, teamFilterEnum);
                if (!parsed)
                {
                    error = std::format("未知隊伍篩選「{}」", teamLabel);
                    return false;
                }
                out.team = *parsed;
            }
        }
        if (acceptsTie)
        {
            if (const auto tie = (*payload)["平手"])
            {
                const auto tieLabel = tie.as<std::string>();
                const auto parsed = parseLabel<EffectTieBreak>(tieLabel, tieBreakEnum);
                if (!parsed)
                {
                    error = std::format("未知平手規則「{}」", tieLabel);
                    return false;
                }
                out.tieBreak = *parsed;
            }
        }
        if (acceptsRequiredTarget)
        {
            if (const auto required = (*payload)["必含目標"])
            {
                const auto requiredLabel = required.as<std::string>();
                const auto parsed = parseLabel<EffectRequiredTarget>(requiredLabel, requiredTargetEnum);
                if (!parsed)
                {
                    error = std::format("未知必含目標「{}」", requiredLabel);
                    return false;
                }
                out.requiredTarget = *parsed;
            }
        }
        if (!payload->finish(error)) return false;
    }
    if (out.count < 0)
    {
        error = "目標數量不可為負數";
        return false;
    }
    if (out.kind == EffectSelectorKind::AlliesUsingWeapon && out.requiredWeaponType < 0)
    {
        error = "指定武器友軍需要非負「武器類型」";
        return false;
    }
    return true;
}

static constexpr std::array timingDescriptors{
    TimingDescriptor{ "開場", EffectEvent::BattleInitialized, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "每幀", EffectEvent::FrameAdvanced, EffectSelectorKind::Self, TimingIntervalPolicy::Forbidden, TimingIntent::None },
    TimingDescriptor{ "每隔", EffectEvent::FrameAdvanced, EffectSelectorKind::Self, TimingIntervalPolicy::RequiredPositive, TimingIntent::None },
    TimingDescriptor{ "絕招冷卻完成", EffectEvent::UltimateCooldownFinished, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "施放規劃", EffectEvent::CastPlanned, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "攻擊提交", EffectEvent::AttackCommitted, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "絕招施放", EffectEvent::UltimateCommitted, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "攻擊生成", EffectEvent::AttackSpawned, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "主彈命中", EffectEvent::MainProjectileBeforeDamage, EffectSelectorKind::HitTarget, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "命中", EffectEvent::HitBeforeDamage, EffectSelectorKind::HitTarget, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "傷害後", EffectEvent::DamageResolved, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "治療嘗試", EffectEvent::HealAttempted, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "治療套用", EffectEvent::HealApplied, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "施放延續", EffectEvent::CastContinuation, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "施放結算完成", EffectEvent::CastSettled, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "護盾破裂", EffectEvent::ShieldBroken, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "單位死亡", EffectEvent::UnitDied, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "友軍死亡", EffectEvent::AllyDied, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "造成傷害後", EffectEvent::DamageResolved, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::DamageDealt },
    TimingDescriptor{ "受傷後", EffectEvent::DamageResolved, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::DamageReceived },
    TimingDescriptor{ "擊殺後", EffectEvent::DamageResolved, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::Kill },
};

static constexpr std::array<PayloadFieldDescriptor, 0> emptyPayloadFields{};
static constexpr std::array conditionMagicFields{
    PayloadFieldDescriptor{ "武功", true, PayloadNodeShape::Integer, "26" },
};
static constexpr std::array conditionPercentFields{
    PayloadFieldDescriptor{ "百分比", true, PayloadNodeShape::Integer, "30" },
};
static constexpr std::array conditionStatusFields{
    PayloadFieldDescriptor{
        "狀態", true, PayloadNodeShape::String, "中毒", {},
        PayloadSchemaReference::None, &statusKindEnum,
    },
};
static constexpr std::array conditionStackFields{
    PayloadFieldDescriptor{
        "狀態", true, PayloadNodeShape::String, "戰意", {},
        PayloadSchemaReference::None, &statusKindEnum,
    },
    PayloadFieldDescriptor{ "層數", true, PayloadNodeShape::Integer, "2" },
};
static constexpr std::array conditionCountFields{
    PayloadFieldDescriptor{ "數量", true, PayloadNodeShape::Integer, "2" },
};
static constexpr std::array conditionOrdinalFields{
    PayloadFieldDescriptor{ "序號", true, PayloadNodeShape::Integer, "1" },
};
static constexpr std::array conditionHealKindsFields{
    PayloadFieldDescriptor{
        "治療種類", true, PayloadNodeShape::Sequence, R"(- 命中
- 吸血)", {}, PayloadSchemaReference::None, &healKindEnum,
    },
};
static constexpr std::array conditionPerspectiveFields{
    PayloadFieldDescriptor{
        "方位", true, PayloadNodeShape::String, "承受", {},
        PayloadSchemaReference::None, &damagePerspectiveEnum,
    },
};
static constexpr std::array conditionDamageKindsFields{
    PayloadFieldDescriptor{
        "傷害種類", true, PayloadNodeShape::Sequence, R"(- 招式
- 特效)", {}, PayloadSchemaReference::None, &damageChannelEnum,
    },
};
static constexpr std::array conditionAcceptedHitFields{
    PayloadFieldDescriptor{ "需要正傷害", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "排除反彈", false, PayloadNodeShape::Boolean, "true" },
};

static constexpr PayloadDescriptor conditionEmptyPayload{ "無參數條件", emptyPayloadFields, "{}" };
static constexpr PayloadDescriptor conditionMagicPayload{ "武功相符", conditionMagicFields, "武功: 26" };
static constexpr PayloadDescriptor conditionPercentPayload{ "百分比條件", conditionPercentFields, "百分比: 30" };
static constexpr PayloadDescriptor conditionStatusPayload{ "狀態條件", conditionStatusFields, "狀態: 中毒" };
static constexpr PayloadDescriptor conditionStackPayload{
    "自身層數至少", conditionStackFields, R"(狀態: 戰意
層數: 2)",
};
static constexpr PayloadDescriptor conditionCountPayload{ "數量條件", conditionCountFields, "數量: 2" };
static constexpr PayloadDescriptor conditionOrdinalPayload{ "攻擊序號", conditionOrdinalFields, "序號: 1" };
static constexpr PayloadDescriptor conditionHealKindsPayload{
    "治療種類符合", conditionHealKindsFields, R"(治療種類:
  - 命中)",
};
static constexpr PayloadDescriptor conditionPerspectivePayload{ "傷害方位", conditionPerspectiveFields, "方位: 承受" };
static constexpr PayloadDescriptor conditionDamageKindsPayload{
    "傷害種類符合", conditionDamageKindsFields, R"(傷害種類:
  - 招式)",
};
static constexpr PayloadDescriptor conditionAcceptedHitPayload{ "已接受命中", conditionAcceptedHitFields, "{}" };

static constexpr std::array conditionDescriptors{
    ConditionDescriptor{ "僅限絕招", 0, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "武功相符", 1, ConditionAuthorForm::SingleParameter, "武功", &conditionMagicPayload },
    ConditionDescriptor{ "僅限主彈道", 2, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "僅限根攻擊", 3, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "自身生命不高於", 4, ConditionAuthorForm::SingleParameter, "百分比", &conditionPercentPayload },
    ConditionDescriptor{ "自身生命低於", 5, ConditionAuthorForm::SingleParameter, "百分比", &conditionPercentPayload },
    ConditionDescriptor{ "自身為最後存活", 6, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "目標生命不高於", 7, ConditionAuthorForm::SingleParameter, "百分比", &conditionPercentPayload },
    ConditionDescriptor{ "目標非無敵", 8, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "自身有狀態", 9, ConditionAuthorForm::SingleParameter, "狀態", &conditionStatusPayload },
    ConditionDescriptor{ "目標有狀態", 10, ConditionAuthorForm::SingleParameter, "狀態", &conditionStatusPayload },
    ConditionDescriptor{ "目標有此來源狀態", 11, ConditionAuthorForm::SingleParameter, "狀態", &conditionStatusPayload },
    ConditionDescriptor{ "自身層數至少", 12, ConditionAuthorForm::Map, {}, &conditionStackPayload },
    ConditionDescriptor{ "其他存活友軍使用武功", 13, ConditionAuthorForm::SingleParameter, "武功", &conditionMagicPayload },
    ConditionDescriptor{ "不同目標數至少", 14, ConditionAuthorForm::SingleParameter, "數量", &conditionCountPayload },
    ConditionDescriptor{ "攻擊序號", 15, ConditionAuthorForm::SingleParameter, "序號", &conditionOrdinalPayload },
    ConditionDescriptor{ "治療種類符合", 16, ConditionAuthorForm::SingleParameter, "治療種類", &conditionHealKindsPayload },
    ConditionDescriptor{ "傷害來自招式", 17, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "傷害造成死亡", 18, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "已接受命中", 19, ConditionAuthorForm::ScalarOrMap, {}, &conditionAcceptedHitPayload },
    ConditionDescriptor{ "事件目標屬於綁定來源", 20, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "傷害方位", 21, ConditionAuthorForm::SingleParameter, "方位", &conditionPerspectivePayload },
    ConditionDescriptor{ "傷害種類符合", 22, ConditionAuthorForm::SingleParameter, "傷害種類", &conditionDamageKindsPayload },
    ConditionDescriptor{ "受益者施放前滿內", 23, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "有合法隨機目標", 24, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
};

static constexpr std::array attackRuntimeBehaviorFields{
    PayloadFieldDescriptor{
        "類型", true, PayloadNodeShape::String, "彈道彈射", {},
        PayloadSchemaReference::None, &attackRuntimeBehaviorKindEnum,
    },
    PayloadFieldDescriptor{ "追加命中次數", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{ "機率", false, PayloadNodeShape::Integer, "50" },
    PayloadFieldDescriptor{ "範圍像素", false, PayloadNodeShape::Integer, "120" },
    PayloadFieldDescriptor{
        "傷害倍率", false, PayloadNodeShape::Integer, "80", R"(類型: 範圍追蹤
範圍像素: 120
傷害倍率: 100)",
    },
    PayloadFieldDescriptor{
        "延遲幀數", false, PayloadNodeShape::Integer, "10", R"(類型: 延遲替代攻擊
延遲幀數: 5
傷害倍率: 100
攻擊者獲得格擋機率: 20)",
    },
    PayloadFieldDescriptor{
        "攻擊者獲得格擋機率", false, PayloadNodeShape::Integer, "20", R"(類型: 延遲替代攻擊
延遲幀數: 5
傷害倍率: 100
攻擊者獲得格擋機率: 20)",
    },
    PayloadFieldDescriptor{
        "彈道數量", false, PayloadNodeShape::Integer, "4", R"(類型: 擴張螺旋
彈道數量: 4
流血層數: 1)",
    },
    PayloadFieldDescriptor{
        "流血層數", false, PayloadNodeShape::Integer, "1", R"(類型: 擴張螺旋
彈道數量: 4
流血層數: 1)",
    },
};
static constexpr PayloadDescriptor attackRuntimeBehaviorPayload{
    "攻擊執行行為", attackRuntimeBehaviorFields, R"(類型: 彈道彈射
追加命中次數: 1
機率: 50
範圍像素: 120)",
};

static constexpr std::array areaModifierFields{
    PayloadFieldDescriptor{
        "類型", true, PayloadNodeShape::String, "強制移動免疫", {},
        PayloadSchemaReference::None, &areaModifierKindEnum,
    },
    PayloadFieldDescriptor{
        "關係", true, PayloadNodeShape::String, "友方", {},
        PayloadSchemaReference::None, &areaRelationEnum,
    },
    PayloadFieldDescriptor{
        "屬性", false, PayloadNodeShape::String, "攻擊", R"(類型: 屬性修正
關係: 友方
屬性: 防禦
數值: 10
重疊方式: 相加)", PayloadSchemaReference::None, &battleAttributeEnum,
    },
    PayloadFieldDescriptor{
        "數值", false, PayloadNodeShape::Number, "10", R"(類型: 屬性修正
關係: 友方
屬性: 防禦
數值: 10
重疊方式: 相加)", PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "百分比", false, PayloadNodeShape::Integer, "10", R"(類型: 造成傷害修正
關係: 友方
百分比: 10
傷害種類: 招式
重疊方式: 相加)",
    },
    PayloadFieldDescriptor{
        "傷害種類", false, PayloadNodeShape::String, "招式", R"(類型: 造成傷害修正
關係: 友方
百分比: 10
傷害種類: 招式
重疊方式: 相加)", PayloadSchemaReference::None, &damageChannelEnum,
    },
    PayloadFieldDescriptor{
        "追蹤", false, PayloadNodeShape::Boolean, "true", R"(類型: 攻擊生成修正
關係: 友方
追蹤: true
重疊方式: 任一)",
    },
    PayloadFieldDescriptor{
        "彈速百分比", false, PayloadNodeShape::Integer, "10", R"(類型: 攻擊生成修正
關係: 友方
彈速百分比: 10
重疊方式: 相加)",
    },
    PayloadFieldDescriptor{
        "彈道壓制百分比", false, PayloadNodeShape::Integer, "10", R"(類型: 攻擊生成修正
關係: 友方
彈道壓制百分比: 10
重疊方式: 相加)",
    },
    PayloadFieldDescriptor{
        "阻擋方向", false, PayloadNodeShape::String, "遠離來源", {},
        PayloadSchemaReference::None, &areaBlockedDirectionEnum,
    },
    PayloadFieldDescriptor{
        "重疊方式", true, PayloadNodeShape::String, "任一", {},
        PayloadSchemaReference::None, &areaOverlapPolicyEnum,
    },
};
static constexpr PayloadDescriptor areaModifierPayload{
    "區域修正", areaModifierFields, R"(類型: 強制移動免疫
關係: 友方
阻擋方向: 遠離來源
重疊方式: 任一)",
};

static constexpr std::array areaProjectileFields{
    PayloadFieldDescriptor{ "範圍格數", true, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{ "最多目標", true, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "眩暈幀數", true, PayloadNodeShape::Integer, "10" },
    PayloadFieldDescriptor{ "追蹤事件來源", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{
        "特效", true, PayloadNodeShape::String, "死亡爆炸", {},
        PayloadSchemaReference::None, &areaProjectileVisualEnum,
    },
};
static constexpr PayloadDescriptor areaProjectilePayload{
    "區域投射物", areaProjectileFields, R"(範圍格數: 3
最多目標: 2
眩暈幀數: 10
特效: 死亡爆炸)",
};

static constexpr std::array autoUltimateFields{
    PayloadFieldDescriptor{ "消耗內力", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "顯示公告", false, PayloadNodeShape::Boolean, "true" },
};
static constexpr PayloadDescriptor autoUltimatePayload{
    "自動絕招", autoUltimateFields, "{}",
};

static constexpr std::array attributeModifierFields{
    PayloadFieldDescriptor{
        "屬性", true, PayloadNodeShape::String, "攻擊", {},
        PayloadSchemaReference::None, &battleAttributeEnum,
    },
    PayloadFieldDescriptor{
        "方式", true, PayloadNodeShape::String, "固定加算", {},
        PayloadSchemaReference::None, &attributeOperationEnum,
    },
    PayloadFieldDescriptor{
        "數值", true, PayloadNodeShape::Number, "1", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{ "持續幀數", false, PayloadNodeShape::Integer, "30" },
    PayloadFieldDescriptor{
        "合併方式", false, PayloadNodeShape::String, "刷新", {},
        PayloadSchemaReference::None, &stackPolicyEnum,
    },
    PayloadFieldDescriptor{ "層數上限", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{ "每層", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{
        "疊加範圍", false, PayloadNodeShape::String, "事件來源", {},
        PayloadSchemaReference::None, &stackScopeEnum,
    },
};
static constexpr std::array damageModifierFields{
    PayloadFieldDescriptor{
        "方位", false, PayloadNodeShape::String, "造成", {},
        PayloadSchemaReference::None, &damageModifierPerspectiveEnum,
    },
    PayloadFieldDescriptor{
        "階段", true, PayloadNodeShape::String, "防禦前", {},
        PayloadSchemaReference::None, &damageModifierStageEnum,
    },
    PayloadFieldDescriptor{
        "傷害種類", true, PayloadNodeShape::String, "招式", {},
        PayloadSchemaReference::None, &damageChannelEnum,
    },
    PayloadFieldDescriptor{
        "方式", true, PayloadNodeShape::String, "固定加算", {},
        PayloadSchemaReference::None, &damageModifierOperationEnum,
    },
    PayloadFieldDescriptor{
        "數值", true, PayloadNodeShape::Number, "1", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{ "持續幀數", false, PayloadNodeShape::Integer, "30" },
    PayloadFieldDescriptor{
        "合併方式", false, PayloadNodeShape::String, "刷新", {},
        PayloadSchemaReference::None, &stackPolicyEnum,
    },
    PayloadFieldDescriptor{ "層數上限", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{
        "疊加範圍", false, PayloadNodeShape::String, "事件來源", {},
        PayloadSchemaReference::None, &stackScopeEnum,
    },
};
static constexpr std::array resourceChangeFields{
    PayloadFieldDescriptor{
        "資源", true, PayloadNodeShape::String, "內力", {},
        PayloadSchemaReference::None, &resourceEnum,
    },
    PayloadFieldDescriptor{
        "方式", true, PayloadNodeShape::String, "回復", {},
        PayloadSchemaReference::None, &resourceChangeKindEnum,
    },
    PayloadFieldDescriptor{
        "數值", true, PayloadNodeShape::Number, "1", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "轉移目標", false, PayloadNodeShape::Selector, "自身", {},
        PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "治療種類", false, PayloadNodeShape::String, "直接", {},
        PayloadSchemaReference::None, &healKindEnum,
    },
    PayloadFieldDescriptor{
        "來源政策", false, PayloadNodeShape::String, "允許死亡來源", {},
        PayloadSchemaReference::None, &healSourcePolicyEnum,
    },
};
static constexpr std::array healTransactionModifierFields{
    PayloadFieldDescriptor{
        "方式", true, PayloadNodeShape::String, "阻止", {},
        PayloadSchemaReference::None, &healModifierOperationEnum,
    },
    PayloadFieldDescriptor{
        "百分比", false, PayloadNodeShape::Integer, "50", R"(方式: 受到治療乘算
百分比: 50
治療種類:
  - 直接)",
    },
    PayloadFieldDescriptor{
        "治療種類", true, PayloadNodeShape::Sequence, R"(- 直接
- 命中)", {}, PayloadSchemaReference::None, &healKindEnum,
    },
};
static constexpr std::array applyStatusFields{
    PayloadFieldDescriptor{
        "狀態", true, PayloadNodeShape::String, "中毒", {},
        PayloadSchemaReference::None, &statusKindEnum,
    },
    PayloadFieldDescriptor{
        "持續幀數", false, PayloadNodeShape::Number, "30", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "套用次數", false, PayloadNodeShape::Number, "2", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{ "層數", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{
        "強度", false, PayloadNodeShape::Number, "10", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "次要強度", false, PayloadNodeShape::Number, "5", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "合併方式", false, PayloadNodeShape::String, "增加層數", {},
        PayloadSchemaReference::None, &stackPolicyEnum,
    },
    PayloadFieldDescriptor{ "層數上限", false, PayloadNodeShape::Integer, "5" },
    PayloadFieldDescriptor{ "同事件合計強度", false, PayloadNodeShape::Boolean, "true" },
};
static constexpr std::array consumeStatusFields{
    PayloadFieldDescriptor{
        "狀態", true, PayloadNodeShape::String, "中毒", {},
        PayloadSchemaReference::None, &statusKindEnum,
    },
    PayloadFieldDescriptor{ "層數", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{
        "狀態來源", false, PayloadNodeShape::String, "效果擁有者", {},
        PayloadSchemaReference::None, &statusSourceMatchEnum,
    },
    PayloadFieldDescriptor{
        "最後一層", false, PayloadNodeShape::ActionNode, R"(套用狀態:
  狀態: 眩暈)", {}, PayloadSchemaReference::ActionNode,
    },
};
static constexpr std::array removeStatusFields{
    PayloadFieldDescriptor{
        "狀態", false, PayloadNodeShape::StringOrSequence, R"(- 中毒
- 流血)", {}, PayloadSchemaReference::None, &statusKindEnum,
    },
    PayloadFieldDescriptor{ "僅負面", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "僅控制", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "解除目前動作僵直", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "數量", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{
        "順序", false, PayloadNodeShape::String, "最長剩餘", {},
        PayloadSchemaReference::None, &statusRemovalOrderEnum,
    },
};
static constexpr std::array dealDamageFields{
    PayloadFieldDescriptor{
        "數值", true, PayloadNodeShape::Number, "10", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "交易次數", false, PayloadNodeShape::Number, "2", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "傷害種類", true, PayloadNodeShape::String, "純粹", {},
        PayloadSchemaReference::None, &battleDamageKindEnum,
    },
    PayloadFieldDescriptor{
        "範圍", false, PayloadNodeShape::String, "圓形", {},
        PayloadSchemaReference::None, &damageAreaKindEnum,
    },
    PayloadFieldDescriptor{ "半徑格數", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "方形邊長", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{ "同目標命中上限", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{ "套用傷害修正", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "觸發受傷無敵", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{
        "區域投射物", false, PayloadNodeShape::Map, R"(範圍格數: 3
最多目標: 2
眩暈幀數: 10
特效: 死亡爆炸)", {}, PayloadSchemaReference::Payload, nullptr,
        &areaProjectilePayload,
    },
};
static constexpr std::array modifyAttackFields{
    PayloadFieldDescriptor{
        "樣式", false, PayloadNodeShape::String, "扇形", {},
        PayloadSchemaReference::None, &attackPatternKindEnum,
    },
    PayloadFieldDescriptor{ "數量", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{ "展開角度", false, PayloadNodeShape::Integer, "30" },
    PayloadFieldDescriptor{ "間隔幀數", false, PayloadNodeShape::Integer, "5" },
    PayloadFieldDescriptor{ "傷害倍率", false, PayloadNodeShape::Integer, "100" },
    PayloadFieldDescriptor{ "貫穿", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "追蹤", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "視為主彈道", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "同目標命中上限", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{
        "目標政策", false, PayloadNodeShape::String, "選擇目標", {},
        PayloadSchemaReference::None, &attackTargetPolicyEnum,
    },
    PayloadFieldDescriptor{
        "傳播政策", false, PayloadNodeShape::String, "來源全部規則", {},
        PayloadSchemaReference::None, &propagationPolicyEnum,
    },
    PayloadFieldDescriptor{ "追加至基礎攻擊", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{
        "攻擊來源", false, PayloadNodeShape::Selector, "自身", {},
        PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "傷害數值", false, PayloadNodeShape::Number, "10", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "傷害種類", false, PayloadNodeShape::String, "招式", {},
        PayloadSchemaReference::None, &battleDamageKindEnum,
    },
    PayloadFieldDescriptor{
        "執行行為", false, PayloadNodeShape::Map, R"(類型: 彈道彈射
追加命中次數: 1
機率: 50
範圍像素: 120)", {}, PayloadSchemaReference::Payload, nullptr,
        &attackRuntimeBehaviorPayload,
    },
};
static constexpr std::array forceMoveFields{
    PayloadFieldDescriptor{
        "方向", true, PayloadNodeShape::String, "遠離來源", {},
        PayloadSchemaReference::None, &forceMoveDirectionEnum,
    },
    PayloadFieldDescriptor{ "距離格數", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "距離像素", false, PayloadNodeShape::Integer, "40" },
    PayloadFieldDescriptor{ "鎖定幀數", false, PayloadNodeShape::Integer, "10" },
    PayloadFieldDescriptor{
        "碰撞", true, PayloadNodeShape::String, "阻擋前停止", {},
        PayloadSchemaReference::None, &forceMoveCollisionEnum,
    },
    PayloadFieldDescriptor{
        "受阻結果", true, PayloadNodeShape::String, "縮短", {},
        PayloadSchemaReference::None, &forceMoveBlockedResultEnum,
    },
};
static constexpr std::array createAreaFields{
    PayloadFieldDescriptor{
        "形狀", true, PayloadNodeShape::String, "圓形", {},
        PayloadSchemaReference::None, &areaShapeEnum,
    },
    PayloadFieldDescriptor{ "半徑格數", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "方形邊長", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{
        "錨點", true, PayloadNodeShape::String, "命中位置", {},
        PayloadSchemaReference::None, &areaAnchorEnum,
    },
    PayloadFieldDescriptor{ "持續幀數", true, PayloadNodeShape::Integer, "60" },
    PayloadFieldDescriptor{
        "來源死亡", true, PayloadNodeShape::String, "保留至到期", {},
        PayloadSchemaReference::None, &areaSourceDeathPolicyEnum,
    },
    PayloadFieldDescriptor{
        "合併方式", true, PayloadNodeShape::String, "獨立", {},
        PayloadSchemaReference::None, &areaMergePolicyEnum,
    },
    PayloadFieldDescriptor{
        "區域修正", true, PayloadNodeShape::Sequence, R"(- 類型: 強制移動免疫
  關係: 友方
  阻擋方向: 遠離來源
  重疊方式: 任一)", {}, PayloadSchemaReference::PayloadList, nullptr,
        &areaModifierPayload,
    },
};
static constexpr std::array modifyCastFields{
    PayloadFieldDescriptor{
        "內力消耗", false, PayloadNodeShape::Number, "10", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "射程模式", false, PayloadNodeShape::String, "保留", {},
        PayloadSchemaReference::None, &castRangeModeEnum,
    },
    PayloadFieldDescriptor{ "彈道速度百分比", false, PayloadNodeShape::Integer, "120" },
    PayloadFieldDescriptor{ "最小選擇距離", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "追加彈道數", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{
        "機動政策", false, PayloadNodeShape::String, "滑步攻擊", {},
        PayloadSchemaReference::None, &castMobilityPolicyEnum,
    },
    PayloadFieldDescriptor{
        "自動絕招", false, PayloadNodeShape::Map, R"(消耗內力: true
顯示公告: true)", {}, PayloadSchemaReference::Payload, nullptr,
        &autoUltimatePayload,
    },
    PayloadFieldDescriptor{ "免費追加施放", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{
        "傳播政策", false, PayloadNodeShape::String, "來源全部規則", {},
        PayloadSchemaReference::None, &propagationPolicyEnum,
    },
    PayloadFieldDescriptor{
        "樣式", false, PayloadNodeShape::String, "扇形", {},
        PayloadSchemaReference::None, &attackPatternKindEnum,
    },
    PayloadFieldDescriptor{ "數量", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{ "展開角度", false, PayloadNodeShape::Integer, "30" },
    PayloadFieldDescriptor{ "間隔幀數", false, PayloadNodeShape::Integer, "5" },
};
static constexpr std::array stateMachineFields{
    PayloadFieldDescriptor{
        "機制", true, PayloadNodeShape::String, "生成分身", {},
        PayloadSchemaReference::None, &stateMachineMechanismEnum,
    },
    PayloadFieldDescriptor{
        "狀態槽", false, PayloadNodeShape::String, "永久施放進展", R"(機制: 變更狀態值
狀態槽: 永久施放進展
增量: 1)", PayloadSchemaReference::None, &stateSlotEnum,
    },
    PayloadFieldDescriptor{
        "來源狀態槽", false, PayloadNodeShape::String, "最大招式生命傷害", R"(機制: 轉移狀態值
來源狀態槽: 最大招式生命傷害
目標狀態槽: 本次施放最高生命傷害)", PayloadSchemaReference::None, &stateSlotEnum,
    },
    PayloadFieldDescriptor{
        "目標狀態槽", false, PayloadNodeShape::String, "本次施放最高生命傷害", R"(機制: 轉移狀態值
來源狀態槽: 最大招式生命傷害
目標狀態槽: 本次施放最高生命傷害)", PayloadSchemaReference::None, &stateSlotEnum,
    },
    PayloadFieldDescriptor{
        "增量", false, PayloadNodeShape::Integer, "1", R"(機制: 變更狀態值
狀態槽: 永久施放進展
增量: 1)",
    },
    PayloadFieldDescriptor{
        "最小", false, PayloadNodeShape::Integer, "0", R"(機制: 變更狀態值
狀態槽: 永久施放進展
增量: 1)",
    },
    PayloadFieldDescriptor{
        "最大", false, PayloadNodeShape::Integer, "10", R"(機制: 變更狀態值
狀態槽: 永久施放進展
增量: 1)",
    },
    PayloadFieldDescriptor{
        "百分比", false, PayloadNodeShape::Integer, "50", R"(機制: 消耗記錄為護盾
狀態槽: 最大招式生命傷害
百分比: 100)",
    },
    PayloadFieldDescriptor{
        "消耗後清除", false, PayloadNodeShape::Boolean, "true", R"(機制: 消耗記錄為護盾
狀態槽: 最大招式生命傷害)",
    },
    PayloadFieldDescriptor{
        "持續幀數", false, PayloadNodeShape::Integer, "60", R"(機制: 開始傷害吸收
狀態槽: 累計吸收傷害
百分比: 50
持續幀數: 60
結算目標: 自身
結算傷害種類: 純粹
結算百分比: 100)",
    },
    PayloadFieldDescriptor{
        "死亡結算", false, PayloadNodeShape::Boolean, "true", R"(機制: 開始傷害吸收
狀態槽: 累計吸收傷害
百分比: 50
持續幀數: 60
結算目標: 自身
結算傷害種類: 純粹
結算百分比: 100)",
    },
    PayloadFieldDescriptor{
        "結算目標", false, PayloadNodeShape::Selector, "自身", R"(機制: 開始傷害吸收
狀態槽: 累計吸收傷害
百分比: 50
持續幀數: 60
結算目標: 自身
結算傷害種類: 純粹
結算百分比: 100)", PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "結算傷害種類", false, PayloadNodeShape::String, "純粹", R"(機制: 開始傷害吸收
狀態槽: 累計吸收傷害
百分比: 50
持續幀數: 60
結算目標: 自身
結算傷害種類: 純粹
結算百分比: 100)", PayloadSchemaReference::None, &battleDamageKindEnum,
    },
    PayloadFieldDescriptor{
        "結算百分比", false, PayloadNodeShape::Integer, "100", R"(機制: 開始傷害吸收
狀態槽: 累計吸收傷害
百分比: 50
持續幀數: 60
結算目標: 自身
結算傷害種類: 純粹
結算百分比: 100)",
    },
    PayloadFieldDescriptor{
        "目標", false, PayloadNodeShape::Selector, "自身", R"(機制: 結算傷害吸收
狀態槽: 累計吸收傷害)", PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "來源數量", false, PayloadNodeShape::Number, "1", R"(機制: 借用效果規則
目標: 友軍
來源數量: 1
允許動作類別:
  - 傷害修正
傳播政策: 來源全部規則)", PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "允許動作類別", false, PayloadNodeShape::Sequence, R"(- 傷害修正
- 狀態)", R"(機制: 借用效果規則
目標: 友軍
來源數量: 1
允許動作類別:
  - 傷害修正
傳播政策: 來源全部規則)", PayloadSchemaReference::None,
        &borrowedRuleActionCategoryEnum,
    },
    PayloadFieldDescriptor{
        "傳播政策", false, PayloadNodeShape::String, "來源全部規則", R"(機制: 借用效果規則
目標: 友軍
來源數量: 1
允許動作類別:
  - 傷害修正
傳播政策: 來源全部規則)", PayloadSchemaReference::None, &propagationPolicyEnum,
    },
    PayloadFieldDescriptor{
        "可選武功條件", false, PayloadNodeShape::Sequence, R"(- 有絕招攻擊定義
- 排除複製與借用遞迴)", R"(機制: 複製攻擊定義
目標: 友軍
可選武功條件:
  - 有絕招攻擊定義
來源數量: 1
傳播政策: 來源全部規則)", PayloadSchemaReference::None, &copiedMagicConditionEnum,
    },
    PayloadFieldDescriptor{
        "狀態", false, PayloadNodeShape::String, "中毒", R"(機制: 結算剩餘狀態傷害
狀態: 中毒)", PayloadSchemaReference::None, &statusKindEnum,
    },
    PayloadFieldDescriptor{
        "數量", false, PayloadNodeShape::Integer, "2", R"(機制: 生成分身
數量: 1)",
    },
    PayloadFieldDescriptor{
        "無敵幀數", false, PayloadNodeShape::Integer, "30", R"(機制: 死亡庇護
無敵幀數: 30)",
    },
    PayloadFieldDescriptor{
        "次數", false, PayloadNodeShape::Integer, "1", R"(機制: 保護挪移
次數: 1)",
    },
};
static constexpr std::array conditionalFields{
    PayloadFieldDescriptor{
        "條件", true, PayloadNodeShape::ConditionList, R"(- 僅限絕招)", {},
        PayloadSchemaReference::ConditionList,
    },
    PayloadFieldDescriptor{
        "成立", true, PayloadNodeShape::ActionList, R"(- 獲得護盾: 1)", {},
        PayloadSchemaReference::ActionList,
    },
    PayloadFieldDescriptor{
        "否則", false, PayloadNodeShape::ActionList, R"(- 回復內力: 1)", {},
        PayloadSchemaReference::ActionList,
    },
};

static constexpr PayloadDescriptor attributeModifierPayload{
    "屬性修正", attributeModifierFields, R"(屬性: 攻擊
方式: 固定加算
數值: 1)",
};
static constexpr PayloadDescriptor damageModifierPayload{
    "傷害修正", damageModifierFields, R"(方位: 造成
階段: 防禦前
傷害種類: 招式
方式: 固定加算
數值: 1)",
};
static constexpr PayloadDescriptor resourceChangePayload{
    "資源變更", resourceChangeFields, R"(資源: 內力
方式: 回復
數值: 1)",
};
static constexpr PayloadDescriptor healTransactionModifierPayload{
    "治療交易修正", healTransactionModifierFields, R"(方式: 阻止
治療種類:
  - 直接)",
};
static constexpr PayloadDescriptor applyStatusPayload{
    "套用狀態", applyStatusFields, "狀態: 中毒",
};
static constexpr PayloadDescriptor consumeStatusPayload{
    "消耗狀態", consumeStatusFields, "狀態: 中毒",
};
static constexpr PayloadDescriptor removeStatusPayload{
    "移除狀態", removeStatusFields, "狀態: 中毒",
};
static constexpr PayloadDescriptor dealDamagePayload{
    "造成傷害", dealDamageFields, R"(數值: 1
傷害種類: 純粹)",
};
static constexpr PayloadDescriptor modifyAttackPayload{
    "修改攻擊", modifyAttackFields, "樣式: 保留",
};
static constexpr PayloadDescriptor forceMovePayload{
    "強制移動", forceMoveFields, R"(方向: 遠離來源
距離格數: 1
碰撞: 阻擋前停止
受阻結果: 縮短)",
};
static constexpr PayloadDescriptor createAreaPayload{
    "建立區域", createAreaFields, R"(形狀: 圓形
半徑格數: 1
錨點: 命中位置
持續幀數: 1
來源死亡: 保留至到期
合併方式: 獨立
區域修正:
  - 類型: 強制移動免疫
    關係: 友方
    阻擋方向: 遠離來源
    重疊方式: 任一)",
};
static constexpr PayloadDescriptor modifyCastPayload{
    "修改施放", modifyCastFields, "射程模式: 保留",
};
static constexpr PayloadDescriptor stateMachinePayload{
    "狀態機", stateMachineFields, R"(機制: 生成分身
數量: 1)",
};
static constexpr PayloadDescriptor conditionalPayload{
    "條件分支", conditionalFields, R"(條件:
  - 僅限絕招
成立:
  - 獲得護盾: 1)",
};

static constexpr std::array actionDescriptors{
    ActionDescriptor{ "屬性修正", 0, ActionPayloadKind::AttributeModifier, &attributeModifierPayload },
    ActionDescriptor{ "傷害修正", 1, ActionPayloadKind::DamageModifier, &damageModifierPayload },
    ActionDescriptor{ "資源變更", 2, ActionPayloadKind::ResourceChange, &resourceChangePayload },
    ActionDescriptor{ "治療交易修正", 3, ActionPayloadKind::HealTransactionModifier, &healTransactionModifierPayload },
    ActionDescriptor{ "套用狀態", 4, ActionPayloadKind::ApplyStatus, &applyStatusPayload },
    ActionDescriptor{ "消耗狀態", 5, ActionPayloadKind::ConsumeStatus, &consumeStatusPayload },
    ActionDescriptor{ "移除狀態", 6, ActionPayloadKind::RemoveStatus, &removeStatusPayload },
    ActionDescriptor{ "造成傷害", 7, ActionPayloadKind::Damage, &dealDamagePayload },
    ActionDescriptor{ "修改攻擊", 8, ActionPayloadKind::Attack, &modifyAttackPayload },
    ActionDescriptor{ "強制移動", 9, ActionPayloadKind::ForceMove, &forceMovePayload },
    ActionDescriptor{ "建立區域", 10, ActionPayloadKind::Area, &createAreaPayload },
    ActionDescriptor{ "修改施放", 11, ActionPayloadKind::Cast, &modifyCastPayload },
    ActionDescriptor{ "狀態機", 12, ActionPayloadKind::StateMachine, &stateMachinePayload },
    ActionDescriptor{ "條件分支", 13, ActionPayloadKind::Conditional, &conditionalPayload },
};

static constexpr std::array<PayloadFieldDescriptor, 0> attributePercentageFields{};
static constexpr PayloadDescriptor attributePercentagePayload{
    "屬性加成.百分比", attributePercentageFields, "攻擊: 1",
    PayloadDynamicKeyClass::BattleAttribute, PayloadNodeShape::Integer, "攻擊", "1", 1,
};
static constexpr std::array attributeBonusFields{
    PayloadFieldDescriptor{
        "百分比", false, PayloadNodeShape::Map, "攻擊: 10", "{}",
        PayloadSchemaReference::Payload, nullptr, &attributePercentagePayload,
    },
    PayloadFieldDescriptor{ "持續幀數", false, PayloadNodeShape::Integer, "30" },
    PayloadFieldDescriptor{
        "合併方式", false, PayloadNodeShape::String, "刷新", {},
        PayloadSchemaReference::None, &stackPolicyEnum,
    },
    PayloadFieldDescriptor{ "層數上限", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{ "每層", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{
        "疊加範圍", false, PayloadNodeShape::String, "事件來源", {},
        PayloadSchemaReference::None, &stackScopeEnum,
    },
};
static constexpr std::array resourceMacroFields{
    PayloadFieldDescriptor{
        "資源", true, PayloadNodeShape::String, "內力", {},
        PayloadSchemaReference::None, &resourceEnum,
    },
    PayloadFieldDescriptor{
        "數值", true, PayloadNodeShape::Number, "1", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "轉移目標", false, PayloadNodeShape::Selector, "自身", {},
        PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "治療種類", false, PayloadNodeShape::String, "直接", {},
        PayloadSchemaReference::None, &healKindEnum,
    },
    PayloadFieldDescriptor{
        "來源政策", false, PayloadNodeShape::String, "允許死亡來源", {},
        PayloadSchemaReference::None, &healSourcePolicyEnum,
    },
};
static constexpr std::array healMacroFields{
    PayloadFieldDescriptor{
        "數值", true, PayloadNodeShape::Number, "1", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "治療種類", false, PayloadNodeShape::String, "直接", {},
        PayloadSchemaReference::None, &healKindEnum,
    },
    PayloadFieldDescriptor{
        "來源政策", false, PayloadNodeShape::String, "允許死亡來源", {},
        PayloadSchemaReference::None, &healSourcePolicyEnum,
    },
};
static constexpr std::array forceMoveMacroFields{
    PayloadFieldDescriptor{ "距離格數", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "距離像素", false, PayloadNodeShape::Integer, "40" },
    PayloadFieldDescriptor{ "鎖定幀數", false, PayloadNodeShape::Integer, "10" },
};
static constexpr PayloadDescriptor attributeBonusPayload{
    "屬性加成", attributeBonusFields, "攻擊: 1",
    PayloadDynamicKeyClass::BattleAttribute, PayloadNodeShape::Integer, "攻擊", "1", 1,
    "百分比",
};
static constexpr PayloadDescriptor resourceMacroPayload{
    "資源巨集", resourceMacroFields, R"(資源: 內力
數值: 1)",
};
static constexpr PayloadDescriptor healMacroPayload{
    "回復生命", healMacroFields, "數值: 1",
};
static constexpr PayloadDescriptor forceMoveMacroPayload{
    "移動巨集", forceMoveMacroFields, "距離格數: 1",
};

static constexpr std::array macroDescriptors{
    MacroDescriptor{ "屬性加成", MacroPayloadKind::AttributeBonus, &attributeBonusPayload },
    MacroDescriptor{ "回復資源", MacroPayloadKind::Resource, &resourceMacroPayload },
    MacroDescriptor{ "獲得資源", MacroPayloadKind::Resource, &resourceMacroPayload },
    MacroDescriptor{ "奪取資源", MacroPayloadKind::Resource, &resourceMacroPayload },
    MacroDescriptor{ "回復內力", MacroPayloadKind::Number, &effectNumberPayload },
    MacroDescriptor{ "回復生命", MacroPayloadKind::Heal, &healMacroPayload },
    MacroDescriptor{ "獲得護盾", MacroPayloadKind::Number, &effectNumberPayload },
    MacroDescriptor{ "忽略防禦", MacroPayloadKind::Number, &effectNumberPayload },
    MacroDescriptor{ "單次承傷上限", MacroPayloadKind::Number, &effectNumberPayload },
    MacroDescriptor{ "擊退", MacroPayloadKind::ForceMove, &forceMoveMacroPayload },
    MacroDescriptor{ "拉近", MacroPayloadKind::ForceMove, &forceMoveMacroPayload },
};

static constexpr std::array ruleFields{
    PayloadFieldDescriptor{
        "時機", true, PayloadNodeShape::String, "傷害後", {},
        PayloadSchemaReference::Timing,
    },
    PayloadFieldDescriptor{
        "觀察範圍", false, PayloadNodeShape::String, "效果擁有者", {},
        PayloadSchemaReference::None, &observationScopeEnum,
    },
    PayloadFieldDescriptor{
        "施放匹配", false, PayloadNodeShape::String, "綁定武功", {},
        PayloadSchemaReference::None, &castMatchEnum,
    },
    PayloadFieldDescriptor{
        "目標", false, PayloadNodeShape::Selector, "自身", {},
        PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "條件", false, PayloadNodeShape::ConditionList, R"(- 已接受命中)", {},
        PayloadSchemaReference::ConditionList,
    },
    PayloadFieldDescriptor{ "機率", false, PayloadNodeShape::Integer, "50" },
    PayloadFieldDescriptor{ "次數", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{ "同來源冷卻幀數", false, PayloadNodeShape::Integer, "10" },
    PayloadFieldDescriptor{
        "間隔幀數", false, PayloadNodeShape::Integer, "30", R"(時機: 每隔
間隔幀數: 30
獲得護盾: 1)",
    },
    PayloadFieldDescriptor{ "每N次事件", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{
        "觸發限制", false, PayloadNodeShape::Map, R"(範圍: 每次施放每個目標
次數: 1)", R"(時機: 命中
觸發限制:
  範圍: 每次施放每個目標
  次數: 1
獲得護盾: 1)", PayloadSchemaReference::Payload, nullptr,
        &activationLimitPayload,
    },
    PayloadFieldDescriptor{
        "重複次數", false, PayloadNodeShape::Number, "2", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "動作", false, PayloadNodeShape::ActionList, R"(- 獲得護盾: 1)", R"(時機: 傷害後
動作:
  - 獲得護盾: 1)", PayloadSchemaReference::ActionList,
    },
};
static constexpr PayloadDescriptor rulePayload{
    "效果規則", ruleFields, R"(時機: 傷害後
獲得護盾: 1)", PayloadDynamicKeyClass::NamedAction,
    PayloadNodeShape::Any, "獲得護盾", "1", 2,
};

enum class PayloadProbeKind
{
    EffectNumber,
    Selector,
    Condition,
    Action,
    Macro,
    ActivationLimit,
    AttackRuntimeBehavior,
    AreaModifier,
    AreaProjectile,
    AutoUltimate,
    AttributePercentage,
    Rule,
};

struct PayloadProbeDescriptor
{
    const PayloadDescriptor* payload;
    PayloadProbeKind kind;
    std::string_view authorName{};
};

static constexpr std::array payloadProbeDescriptors{
    PayloadProbeDescriptor{ &activationLimitPayload, PayloadProbeKind::ActivationLimit },
    PayloadProbeDescriptor{ &effectNumberPayload, PayloadProbeKind::EffectNumber },
    PayloadProbeDescriptor{ &selectorPayload, PayloadProbeKind::Selector },
    PayloadProbeDescriptor{ &conditionEmptyPayload, PayloadProbeKind::Condition, "僅限絕招" },
    PayloadProbeDescriptor{ &conditionMagicPayload, PayloadProbeKind::Condition, "武功相符" },
    PayloadProbeDescriptor{ &conditionPercentPayload, PayloadProbeKind::Condition, "自身生命低於" },
    PayloadProbeDescriptor{ &conditionStatusPayload, PayloadProbeKind::Condition, "自身有狀態" },
    PayloadProbeDescriptor{ &conditionStackPayload, PayloadProbeKind::Condition, "自身層數至少" },
    PayloadProbeDescriptor{ &conditionCountPayload, PayloadProbeKind::Condition, "不同目標數至少" },
    PayloadProbeDescriptor{ &conditionOrdinalPayload, PayloadProbeKind::Condition, "攻擊序號" },
    PayloadProbeDescriptor{ &conditionHealKindsPayload, PayloadProbeKind::Condition, "治療種類符合" },
    PayloadProbeDescriptor{ &conditionPerspectivePayload, PayloadProbeKind::Condition, "傷害方位" },
    PayloadProbeDescriptor{ &conditionDamageKindsPayload, PayloadProbeKind::Condition, "傷害種類符合" },
    PayloadProbeDescriptor{ &conditionAcceptedHitPayload, PayloadProbeKind::Condition, "已接受命中" },
    PayloadProbeDescriptor{ &attributeModifierPayload, PayloadProbeKind::Action, "屬性修正" },
    PayloadProbeDescriptor{ &damageModifierPayload, PayloadProbeKind::Action, "傷害修正" },
    PayloadProbeDescriptor{ &resourceChangePayload, PayloadProbeKind::Action, "資源變更" },
    PayloadProbeDescriptor{ &healTransactionModifierPayload, PayloadProbeKind::Action, "治療交易修正" },
    PayloadProbeDescriptor{ &applyStatusPayload, PayloadProbeKind::Action, "套用狀態" },
    PayloadProbeDescriptor{ &consumeStatusPayload, PayloadProbeKind::Action, "消耗狀態" },
    PayloadProbeDescriptor{ &removeStatusPayload, PayloadProbeKind::Action, "移除狀態" },
    PayloadProbeDescriptor{ &dealDamagePayload, PayloadProbeKind::Action, "造成傷害" },
    PayloadProbeDescriptor{ &modifyAttackPayload, PayloadProbeKind::Action, "修改攻擊" },
    PayloadProbeDescriptor{ &forceMovePayload, PayloadProbeKind::Action, "強制移動" },
    PayloadProbeDescriptor{ &createAreaPayload, PayloadProbeKind::Action, "建立區域" },
    PayloadProbeDescriptor{ &modifyCastPayload, PayloadProbeKind::Action, "修改施放" },
    PayloadProbeDescriptor{ &stateMachinePayload, PayloadProbeKind::Action, "狀態機" },
    PayloadProbeDescriptor{ &conditionalPayload, PayloadProbeKind::Action, "條件分支" },
    PayloadProbeDescriptor{ &attackRuntimeBehaviorPayload, PayloadProbeKind::AttackRuntimeBehavior },
    PayloadProbeDescriptor{ &areaModifierPayload, PayloadProbeKind::AreaModifier },
    PayloadProbeDescriptor{ &areaProjectilePayload, PayloadProbeKind::AreaProjectile },
    PayloadProbeDescriptor{ &autoUltimatePayload, PayloadProbeKind::AutoUltimate },
    PayloadProbeDescriptor{ &attributePercentagePayload, PayloadProbeKind::AttributePercentage },
    PayloadProbeDescriptor{ &attributeBonusPayload, PayloadProbeKind::Macro, "屬性加成" },
    PayloadProbeDescriptor{ &resourceMacroPayload, PayloadProbeKind::Macro, "回復資源" },
    PayloadProbeDescriptor{ &healMacroPayload, PayloadProbeKind::Macro, "回復生命" },
    PayloadProbeDescriptor{ &forceMoveMacroPayload, PayloadProbeKind::Macro, "擊退" },
    PayloadProbeDescriptor{ &rulePayload, PayloadProbeKind::Rule },
};

template <typename Descriptor, std::size_t Size>
consteval bool descriptorNamesAreUnique(const std::array<Descriptor, Size>& descriptors)
{
    for (std::size_t i = 0; i < Size; ++i)
        for (std::size_t j = i + 1; j < Size; ++j)
            if (descriptors[i].name == descriptors[j].name) return false;
    return true;
}

template <typename Descriptor, std::size_t Size>
consteval bool descriptorIndicesCoverVariants(const std::array<Descriptor, Size>& descriptors)
{
    std::array<bool, Size> seen{};
    for (const auto& descriptor : descriptors)
    {
        if (descriptor.variantIndex >= Size || seen[descriptor.variantIndex]) return false;
        seen[descriptor.variantIndex] = true;
    }
    return std::ranges::all_of(seen, [](bool value) { return value; });
}

template <typename Left, std::size_t LeftSize, typename Right, std::size_t RightSize>
consteval bool descriptorNamesAreDisjoint(
    const std::array<Left, LeftSize>& left,
    const std::array<Right, RightSize>& right)
{
    for (const auto& a : left)
        for (const auto& b : right)
            if (a.name == b.name) return false;
    return true;
}

template <typename Descriptor, std::size_t Size, std::size_t FieldCount>
consteval bool descriptorNamesAreDisjointFromFields(
    const std::array<Descriptor, Size>& descriptors,
    const std::array<PayloadFieldDescriptor, FieldCount>& fields)
{
    for (const auto& descriptor : descriptors)
        for (const auto& field : fields)
            if (descriptor.name == field.name) return false;
    return true;
}

consteval bool payloadFieldNamesAreUnique(const PayloadDescriptor& descriptor)
{
    for (std::size_t i = 0; i < descriptor.fields.size(); ++i)
        for (std::size_t j = i + 1; j < descriptor.fields.size(); ++j)
            if (descriptor.fields[i].name == descriptor.fields[j].name) return false;
    return true;
}

template <typename Descriptor, std::size_t Size>
consteval bool descriptorPayloadFieldsAreUnique(
    const std::array<Descriptor, Size>& descriptors)
{
    for (const auto& descriptor : descriptors)
        if (descriptor.payload && !payloadFieldNamesAreUnique(*descriptor.payload)) return false;
    return true;
}

consteval bool authorEnumMetadataIsValid()
{
    for (std::size_t index = 0; index < authorEnumDescriptors.size(); ++index)
    {
        const auto& descriptor = *authorEnumDescriptors[index];
        if (descriptor.name.empty() || descriptor.labels.empty()) return false;
        for (std::size_t other = index + 1; other < authorEnumDescriptors.size(); ++other)
            if (descriptor.name == authorEnumDescriptors[other]->name) return false;
        for (std::size_t label = 0; label < descriptor.labels.size(); ++label)
            for (std::size_t other = label + 1; other < descriptor.labels.size(); ++other)
                if (descriptor.labels[label].name == descriptor.labels[other].name) return false;
    }
    return true;
}

consteval bool payloadMetadataIsComplete(const PayloadDescriptor& descriptor)
{
    if (descriptor.name.empty() || descriptor.minimalProbe.empty()
        || !payloadFieldNamesAreUnique(descriptor)) return false;
    if (descriptor.dynamicKeyClass != PayloadDynamicKeyClass::None
        && (descriptor.dynamicProbeKey.empty() || descriptor.dynamicProbeValue.empty())) return false;
    if (!descriptor.dynamicAlternativeField.empty()
        && (descriptor.dynamicKeyClass == PayloadDynamicKeyClass::None
            || std::ranges::none_of(descriptor.fields, [&](const auto& field)
            {
                return field.name == descriptor.dynamicAlternativeField;
            }))) return false;
    for (const auto& field : descriptor.fields)
    {
        if (field.probeValue.empty()) return false;
        if (field.enumLabels && field.enumLabels->labels.empty()) return false;
        const bool nested = field.schemaReference == PayloadSchemaReference::Payload
            || field.schemaReference == PayloadSchemaReference::PayloadList;
        if (nested != (field.nestedPayload != nullptr)) return false;
        if ((field.shape == PayloadNodeShape::String
                || field.shape == PayloadNodeShape::StringOrSequence)
            && !field.enumLabels
            && field.schemaReference != PayloadSchemaReference::Timing) return false;
    }
    return true;
}

consteval bool payloadProbeRegistryIsComplete()
{
    for (std::size_t index = 0; index < payloadProbeDescriptors.size(); ++index)
    {
        if (!payloadMetadataIsComplete(*payloadProbeDescriptors[index].payload)) return false;
        for (std::size_t other = index + 1; other < payloadProbeDescriptors.size(); ++other)
            if (payloadProbeDescriptors[index].payload == payloadProbeDescriptors[other].payload)
                return false;
    }
    return true;
}

static_assert(actionDescriptors.size() == std::variant_size_v<EffectActionValue>);
static_assert(conditionDescriptors.size() == std::variant_size_v<EffectCondition>);
static_assert(descriptorIndicesCoverVariants(actionDescriptors));
static_assert(descriptorIndicesCoverVariants(conditionDescriptors));
static_assert(descriptorNamesAreUnique(timingDescriptors));
static_assert(descriptorNamesAreUnique(actionDescriptors));
static_assert(descriptorNamesAreUnique(conditionDescriptors));
static_assert(descriptorNamesAreUnique(macroDescriptors));
static_assert(authorEnumMetadataIsValid());
static_assert(payloadProbeRegistryIsComplete());
static_assert(std::ranges::all_of(actionDescriptors, [](const auto& descriptor)
{
    return descriptor.payload && !descriptor.payload->minimalProbe.empty();
}));
static_assert(std::ranges::all_of(conditionDescriptors, [](const auto& descriptor)
{
    return descriptor.payload && !descriptor.payload->minimalProbe.empty();
}));
static_assert(std::ranges::all_of(macroDescriptors, [](const auto& descriptor)
{
    return descriptor.payload && !descriptor.payload->minimalProbe.empty();
}));
static_assert(descriptorPayloadFieldsAreUnique(actionDescriptors));
static_assert(descriptorPayloadFieldsAreUnique(conditionDescriptors));
static_assert(descriptorPayloadFieldsAreUnique(macroDescriptors));
static_assert(descriptorNamesAreDisjoint(actionDescriptors, macroDescriptors));
static_assert(payloadFieldNamesAreUnique(rulePayload));
static_assert(descriptorNamesAreDisjointFromFields(actionDescriptors, ruleFields));
static_assert(descriptorNamesAreDisjointFromFields(macroDescriptors, ruleFields));

}

namespace EffectAuthoring
{

std::span<const TimingDescriptor> timingDescriptors()
{
    return KysChess::timingDescriptors;
}

std::span<const ConditionDescriptor> conditionDescriptors()
{
    return KysChess::conditionDescriptors;
}

std::span<const ActionDescriptor> actionDescriptors()
{
    return KysChess::actionDescriptors;
}

std::span<const MacroDescriptor> macroDescriptors()
{
    return KysChess::macroDescriptors;
}

const AuthorEnumDescriptor& battleAttributeDescriptor()
{
    return battleAttributeEnum;
}

const AuthorEnumDescriptor& selectorKindDescriptor()
{
    return selectorKindEnum;
}

const PayloadDescriptor& effectNumberDescriptor()
{
    return effectNumberPayload;
}

const PayloadDescriptor& selectorDescriptor()
{
    return selectorPayload;
}

const PayloadDescriptor& ruleDescriptor()
{
    return rulePayload;
}

const TimingDescriptor* findTimingDescriptor(std::string_view name)
{
    const auto descriptors = timingDescriptors();
    const auto found = std::ranges::find(descriptors, name, &TimingDescriptor::name);
    return found == descriptors.end() ? nullptr : &*found;
}

const ConditionDescriptor* findConditionDescriptor(std::string_view name)
{
    const auto descriptors = conditionDescriptors();
    const auto found = std::ranges::find(descriptors, name, &ConditionDescriptor::name);
    return found == descriptors.end() ? nullptr : &*found;
}

const ActionDescriptor* findActionDescriptor(std::string_view name)
{
    const auto descriptors = actionDescriptors();
    const auto found = std::ranges::find(descriptors, name, &ActionDescriptor::name);
    return found == descriptors.end() ? nullptr : &*found;
}

const MacroDescriptor* findMacroDescriptor(std::string_view name)
{
    const auto descriptors = macroDescriptors();
    const auto found = std::ranges::find(descriptors, name, &MacroDescriptor::name);
    return found == descriptors.end() ? nullptr : &*found;
}

}

namespace
{

using namespace EffectAuthoring;

enum class ConditionNodeForm
{
    Scalar,
    SingleParameter,
    NamedPayload,
};

bool parseConditionPayload(
    const ConditionDescriptor& descriptor,
    const YAML::Node& payload,
    ConditionNodeForm form,
    EffectCondition& out,
    std::string& error)
{
    const auto type = descriptor.name;
    std::optional<PayloadView> payloadView;
    if (form == ConditionNodeForm::NamedPayload)
    {
        payloadView.emplace(payload, *descriptor.payload);
        if (!payloadView->validate(error)) return false;
    }
    else if (form == ConditionNodeForm::Scalar)
    {
        if (!descriptor.payload->fields.empty()
            && descriptor.form != ConditionAuthorForm::ScalarOrMap)
        {
            error = std::format("條件「{}」需要參數，不能使用 scalar 簡式", type);
            return false;
        }
    }
    else
    {
        if (descriptor.payload->fields.size() != 1
            || descriptor.payload->fields.front().name != descriptor.singleParameterField)
        {
            error = std::format("條件「{}」不支援單參數簡式", type);
            return false;
        }
        if (!validatePayloadNodeShape(payload, descriptor.payload->fields.front(), error)) return false;
    }
    const auto valueNode = [&](std::string_view field)
    {
        return form == ConditionNodeForm::SingleParameter
            ? payload
            : (*payloadView)[field];
    };
    const auto readInt = [&](std::string_view field, int& value)
    {
        const auto valueField = valueNode(field);
        if (!valueField)
        {
            error = std::format("缺少「{}」欄位", field);
            return false;
        }
        try
        {
            value = valueField.as<int>();
            return true;
        }
        catch (const YAML::Exception& ex)
        {
            error = std::format("「{}」不是有效整數: {}", field, ex.what());
            return false;
        }
    };
    const auto readString = [&](std::string_view field, std::string& value)
    {
        const auto valueField = valueNode(field);
        if (!valueField)
        {
            error = std::format("缺少「{}」欄位", field);
            return false;
        }
        try
        {
            value = valueField.as<std::string>();
            return true;
        }
        catch (const YAML::Exception& ex)
        {
            error = std::format("「{}」不是有效字串: {}", field, ex.what());
            return false;
        }
    };
    const auto readStrings = [&](std::string_view field, std::vector<std::string>& values)
    {
        const auto valueField = valueNode(field);
        if (!valueField || !valueField.IsSequence() || valueField.size() == 0)
        {
            error = std::format("「{}」必須是非空列表", field);
            return false;
        }
        try
        {
            values.reserve(valueField.size());
            for (const auto& value : valueField) values.push_back(value.as<std::string>());
            return true;
        }
        catch (const YAML::Exception& ex)
        {
            error = std::format("「{}」含有無效字串: {}", field, ex.what());
            return false;
        }
    };

    if (type == "僅限絕招")
    {
        out = IsUltimateCondition{};
    }
    else if (type == "武功相符")
    {
        int magicId{};
        if (!readInt("武功", magicId)) return false;
        out = MagicIdEqualsCondition{ magicId };
    }
    else if (type == "僅限主彈道")
    {
        out = IsMainProjectileCondition{};
    }
    else if (type == "僅限根攻擊")
    {
        out = IsRootAttackCondition{};
    }
    else if (type == "自身生命不高於")
    {
        int percent{};
        if (!readInt("百分比", percent)) return false;
        out = SourceHpRatioAtMostCondition{ percent };
    }
    else if (type == "自身生命低於")
    {
        int percent{};
        if (!readInt("百分比", percent)) return false;
        out = SourceHpRatioBelowCondition{ percent };
    }
    else if (type == "自身為最後存活")
    {
        out = SourceIsLastAliveCondition{};
    }
    else if (type == "目標生命不高於")
    {
        int percent{};
        if (!readInt("百分比", percent)) return false;
        out = TargetHpRatioAtMostCondition{ percent };
    }
    else if (type == "目標非無敵")
    {
        out = TargetNotInvincibleCondition{};
    }
    else if (type == "自身有狀態")
    {
        std::string state;
        if (!readString("狀態", state)) return false;
        out = SourceHasStateCondition{ std::move(state) };
    }
    else if (type == "目標有狀態")
    {
        std::string state;
        if (!readString("狀態", state)) return false;
        out = TargetHasStateCondition{ std::move(state) };
    }
    else if (type == "目標有此來源狀態")
    {
        std::string state;
        BattleStatusKind parsed{};
        if (!readString("狀態", state)
            || !parseStatusKind(state, parsed, error)) return false;
        out = TargetHasStateFromEffectOwnerCondition{
            std::string(battleStatusLabel(parsed)),
        };
    }
    else if (type == "自身層數至少")
    {
        std::string stack;
        int count{};
        if (!readString("狀態", stack)
            || !readInt("層數", count)) return false;
        out = SourceStackAtLeastCondition{ std::move(stack), count };
    }
    else if (type == "其他存活友軍使用武功")
    {
        int magicId{};
        if (!readInt("武功", magicId)) return false;
        out = OtherLivingAllyUsesMagicCondition{ magicId };
    }
    else if (type == "不同目標數至少")
    {
        int count{};
        if (!readInt("數量", count)) return false;
        out = CastDistinctTargetCountAtLeastCondition{ count };
    }
    else if (type == "攻擊序號")
    {
        int ordinal{};
        if (!readInt("序號", ordinal)) return false;
        out = AttackOrdinalEqualsCondition{ ordinal };
    }
    else if (type == "治療種類符合" || type == "傷害種類符合")
    {
        const auto fieldName = type == "治療種類符合" ? "治療種類" : "傷害種類";
        std::vector<std::string> labels;
        if (!readStrings(fieldName, labels)) return false;
        if (type == "治療種類符合") out = HealKindInCondition{ std::move(labels) };
        else out = DamageKindInCondition{ std::move(labels) };
    }
    else if (type == "傷害來自招式")
    {
        out = DamageOriginIsAttackCondition{};
    }
    else if (type == "已接受命中")
    {
        AcceptedHitCondition condition;
        if (form == ConditionNodeForm::Scalar)
        {
            out = condition;
            return true;
        }
        if (form != ConditionNodeForm::NamedPayload
            || !optionalBool(*payloadView, "需要正傷害", condition.requirePositiveDamage, error)
            || !optionalBool(*payloadView, "排除反彈", condition.excludeReflected, error)) return false;
        out = condition;
    }
    else if (type == "事件目標屬於綁定來源")
    {
        out = EventTargetBelongsToBoundSourceCondition{};
    }
    else if (type == "傷害造成死亡")
    {
        out = DamageKilledTargetCondition{};
    }
    else if (type == "傷害方位")
    {
        std::string perspective;
        if (!readString("方位", perspective)) return false;
        const auto parsed = parseLabel<DamagePerspective>(perspective, damagePerspectiveEnum);
        if (!parsed)
        {
            error = std::format("未知傷害方位「{}」", perspective);
            return false;
        }
        out = DamagePerspectiveCondition{ *parsed };
    }
    else if (type == "受益者施放前滿內")
    {
        out = TargetMpWasFullBeforeCastCondition{};
    }
    else if (type == "有合法隨機目標")
    {
        out = RandomSelectionAvailableCondition{};
    }
    else
    {
        error = std::format("未知條件「{}」", type);
        return false;
    }
    return !payloadView || payloadView->finish(error);
}

bool parseConditionNode(const YAML::Node& node, EffectCondition& out, std::string& error)
{
    if (!node)
    {
        error = "缺少條件";
        return false;
    }
    if (node.IsScalar())
    {
        const auto type = node.as<std::string>();
        const auto* descriptor = findConditionDescriptor(type);
        if (!descriptor
            || (descriptor->form != ConditionAuthorForm::Scalar
                && descriptor->form != ConditionAuthorForm::ScalarOrMap))
        {
            error = std::format("條件「{}」不可使用 scalar 外形", type);
            return false;
        }
        return parseConditionPayload(
            *descriptor, node, ConditionNodeForm::Scalar, out, error);
    }
    if (!node.IsMap())
    {
        error = "條件必須是映射表或簡式名稱";
        return false;
    }
    if (!validateUniqueKeys(node, error)) return false;
    if (node.size() != 1)
    {
        error = "具名條件必須恰有一個條件欄位";
        return false;
    }
    const auto entry = *node.begin();
    const auto type = entry.first.as<std::string>();
    const auto* descriptor = findConditionDescriptor(type);
    if (!descriptor)
    {
        error = std::format("未知條件「{}」", type);
        return false;
    }
    if (descriptor->form == ConditionAuthorForm::Scalar)
    {
        error = std::format("無參數條件「{}」請寫成 scalar 列表項目", type);
        return false;
    }
    if (descriptor->form == ConditionAuthorForm::SingleParameter)
    {
        return parseConditionPayload(
            *descriptor,
            entry.second,
            ConditionNodeForm::SingleParameter,
            out,
            error);
    }
    if (!entry.second.IsMap())
    {
        error = std::format("條件「{}」的 payload 必須是映射表", type);
        return false;
    }
    return parseConditionPayload(
        *descriptor,
        entry.second,
        ConditionNodeForm::NamedPayload,
        out,
        error);
}

bool parseStackPolicy(const YAML::Node& node, EffectStackPolicy& out, std::string& error)
{
    if (!node) return true;
    const auto label = node.as<std::string>();
    const auto parsed = parseLabel<EffectStackPolicy>(label, stackPolicyEnum);
    if (!parsed)
    {
        error = std::format("未知合併方式「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseStatusKind(std::string_view label, BattleStatusKind& out, std::string& error)
{
    const auto parsed = parseLabel<BattleStatusKind>(label, statusKindEnum);
    if (!parsed)
    {
        error = std::format("未知狀態「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseDamageChannel(std::string_view label, DamageChannel& out, std::string& error)
{
    const auto parsed = parseLabel<DamageChannel>(label, damageChannelEnum);
    if (!parsed)
    {
        error = std::format("未知傷害種類「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseEffectStateSlot(const YAML::Node& node, EffectStateSlot& out, std::string& error)
{
    if (!node)
    {
        error = "缺少「狀態槽」欄位";
        return false;
    }
    const auto label = node.as<std::string>();
    const auto parsed = parseLabel<EffectStateSlot>(label, stateSlotEnum);
    if (!parsed)
    {
        error = std::format("未知狀態槽「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseResourceLabel(
    std::string_view label,
    BattleResource& out,
    std::string& error)
{
    const auto parsed = parseLabel<BattleResource>(label, resourceEnum);
    if (!parsed)
    {
        error = std::format("未知資源「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseResourceChangeKindLabel(
    std::string_view label,
    ResourceChangeKind& out,
    std::string& error)
{
    const auto parsed = parseLabel<ResourceChangeKind>(label, resourceChangeKindEnum);
    if (!parsed)
    {
        error = std::format("未知資源變更方式「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

template <typename Node>
bool parseResourceMetadata(
    const Node& node,
    ChangeResourceAction& action,
    std::string& error)
{
    if (node["轉移目標"])
    {
        EffectSelector selector;
        if (!parseSelectorNode(node["轉移目標"], selector, error)) return false;
        action.transferDestination = std::move(selector);
    }
    if (const auto healKind = node["治療種類"])
    {
        const auto label = healKind.as<std::string>();
        const auto parsed = parseLabel<EffectHealKind>(label, healKindEnum);
        if (!parsed)
        {
            error = std::format("未知治療種類「{}」", label);
            return false;
        }
        action.healKind = *parsed;
    }
    if (const auto sourcePolicy = node["來源政策"])
    {
        const auto label = sourcePolicy.as<std::string>();
        const auto parsed = parseLabel<EffectHealSourcePolicy>(label, healSourcePolicyEnum);
        if (!parsed)
        {
            error = std::format("未知治療來源政策「{}」", label);
            return false;
        }
        action.healSourcePolicy = *parsed;
    }
    return true;
}

template <typename Node>
bool parseAttributeModifierQualifiers(
    const Node& node,
    ModifyAttributeAction& out,
    std::string& error)
{
    if (!optionalInt(node, "持續幀數", out.durationFrames, error)
        || !parseStackPolicy(node["合併方式"], out.stack, error)
        || !optionalBool(node, "每層", out.perStack, error)) return false;
    if (node["層數上限"])
    {
        int limit{};
        if (!requiredInt(node, "層數上限", limit, error)) return false;
        out.stackLimit = limit;
    }
    if (const auto scope = node["疊加範圍"])
    {
        const auto label = scope.as<std::string>();
        const auto parsed = parseLabel<EffectStackScope>(label, stackScopeEnum);
        if (!parsed)
        {
            error = std::format("未知疊加範圍「{}」", label);
            return false;
        }
        out.stackScope = *parsed;
    }
    return true;
}

bool parseActionPayload(
    std::string_view type,
    PayloadView& node,
    EffectAction& out,
    std::string& error);
bool parseAuthorActionNode(
    const YAML::Node& node,
    std::vector<EffectAction>& out,
    std::string& error);

bool parseActionList(const YAML::Node& node, std::vector<EffectAction>& out, std::string& error)
{
    if (!node || !node.IsSequence() || node.size() == 0)
    {
        error = "動作必須是非空列表";
        return false;
    }
    out.clear();
    for (std::size_t i = 0; i < node.size(); ++i)
    {
        std::vector<EffectAction> actions;
        if (!parseAuthorActionNode(node[i], actions, error))
        {
            error = std::format("動作#{}: {}", i + 1, error);
            return false;
        }
        out.insert(
            out.end(),
            std::make_move_iterator(actions.begin()),
            std::make_move_iterator(actions.end()));
    }
    return true;
}

bool parseBorrowedRuleFilter(
    const YAML::Node& node,
    BorrowedRuleFilter& out,
    std::string& error)
{
    if (!node || !node.IsSequence() || node.size() == 0)
    {
        error = "允許動作類別必須是非空列表";
        return false;
    }
    out.allowedActionCategories.clear();
    std::set<BorrowedRuleActionCategory> seen;
    for (const auto& value : node)
    {
        const auto label = value.as<std::string>();
        const auto parsed = parseLabel<BorrowedRuleActionCategory>(
            label, borrowedRuleActionCategoryEnum);
        if (!parsed)
        {
            error = std::format("未知可借用動作類別「{}」", label);
            return false;
        }
        if (!seen.insert(*parsed).second)
        {
            error = std::format("可借用動作類別「{}」重複", label);
            return false;
        }
        out.allowedActionCategories.push_back(*parsed);
    }
    return true;
}

bool parseCopiedMagicFilter(
    const YAML::Node& node,
    CopiedMagicFilter& out,
    std::string& error)
{
    if (!node || !node.IsSequence() || node.size() == 0)
    {
        error = "可選武功條件必須是非空列表";
        return false;
    }
    out.conditions.clear();
    std::set<CopiedMagicCondition> seen;
    for (const auto& value : node)
    {
        const auto label = value.as<std::string>();
        const auto parsed = parseLabel<CopiedMagicCondition>(label, copiedMagicConditionEnum);
        if (!parsed)
        {
            error = std::format("未知可選武功條件「{}」", label);
            return false;
        }
        if (!seen.insert(*parsed).second)
        {
            error = std::format("可選武功條件「{}」重複", label);
            return false;
        }
        out.conditions.push_back(*parsed);
    }
    return true;
}

consteval bool attributeNamesAreUniqueAndReservedFieldsAreDisjoint()
{
    for (std::size_t i = 0; i < battleAttributeLabels.size(); ++i)
    {
        for (std::size_t j = i + 1; j < battleAttributeLabels.size(); ++j)
            if (battleAttributeLabels[i].name == battleAttributeLabels[j].name) return false;
        for (const auto& field : attributeBonusFields)
            if (battleAttributeLabels[i].name == field.name) return false;
    }
    return true;
}

static_assert(attributeNamesAreUniqueAndReservedFieldsAreDisjoint());

bool isDynamicPayloadKey(PayloadDynamicKeyClass keyClass, std::string_view key)
{
    if (keyClass == PayloadDynamicKeyClass::None) return false;
    if (keyClass == PayloadDynamicKeyClass::BattleAttribute)
        return std::ranges::any_of(
            battleAttributeLabels,
            [=](const auto& entry) { return entry.name == key; });
    assert(keyClass == PayloadDynamicKeyClass::NamedAction);
    return findActionDescriptor(key) || findMacroDescriptor(key);
}

bool parseAttribute(std::string_view label, BattleAttribute& out, std::string& error)
{
    const auto parsed = parseLabel<BattleAttribute>(label, battleAttributeEnum);
    if (!parsed)
    {
        error = std::format("未知屬性「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

template <typename Node>
bool parseAttackPatternFields(const Node& node, AttackPattern& pattern, std::string& error)
{
    if (const auto style = node["樣式"])
    {
        const auto label = style.as<std::string>();
        const auto parsed = parseLabel<AttackPatternKind>(label, attackPatternKindEnum);
        if (!parsed)
        {
            error = std::format("未知攻擊樣式「{}」", label);
            return false;
        }
        pattern.kind = *parsed;
    }
    return optionalInt(node, "數量", pattern.projectileCount, error)
        && optionalInt(node, "展開角度", pattern.spreadDegrees, error)
        && optionalInt(node, "間隔幀數", pattern.intervalFrames, error);
}

bool parseAttackRuntimeBehavior(
    const YAML::Node& node,
    AttackRuntimeBehavior& out,
    std::string& error)
{
    PayloadView payload(node, attackRuntimeBehaviorPayload);
    if (!payload.validate(error)) return false;
    std::string type;
    if (!requiredString(payload, "類型", type, error)) return false;

    const auto kind = parseLabel<AttackRuntimeBehaviorKind>(type, attackRuntimeBehaviorKindEnum);
    if (!kind)
    {
        error = std::format("未知攻擊執行行為「{}」", type);
        return false;
    }
    if (*kind == AttackRuntimeBehaviorKind::ProjectileBounce)
    {
        ProjectileBounceAttackBehavior behavior;
        if (!requiredInt(payload, "追加命中次數", behavior.additionalHits, error)
            || !requiredInt(payload, "機率", behavior.chancePct, error)
            || !requiredInt(payload, "範圍像素", behavior.rangePixels, error)) return false;
        out = behavior;
        return payload.finish(error);
    }
    if (*kind == AttackRuntimeBehaviorKind::NearbyTracking)
    {
        NearbyTrackingAttackBehavior behavior;
        if (!requiredInt(payload, "範圍像素", behavior.rangePixels, error)
            || !requiredInt(payload, "傷害倍率", behavior.damagePct, error)) return false;
        out = behavior;
        return payload.finish(error);
    }
    if (*kind == AttackRuntimeBehaviorKind::DelayedAlternate)
    {
        DelayedAlternateAttackBehavior behavior;
        if (!requiredInt(payload, "延遲幀數", behavior.delayFrames, error)
            || !requiredInt(payload, "傷害倍率", behavior.damagePct, error)
            || !requiredInt(payload, "攻擊者獲得格擋機率",
                behavior.attackerBlockGainChancePct,
                error)) return false;
        out = behavior;
        return payload.finish(error);
    }
    if (*kind == AttackRuntimeBehaviorKind::ExpandingSpiral)
    {
        ExpandingSpiralAttackBehavior behavior;
        if (!requiredInt(payload, "彈道數量", behavior.projectileCount, error)
            || !requiredInt(payload, "流血層數", behavior.bleedStacks, error)) return false;
        out = behavior;
        return payload.finish(error);
    }
    assert(false);
    return false;
}

bool parsePropagation(const YAML::Node& node, CastPropagationPolicy& out, std::string& error)
{
    if (!node) return true;
    const auto label = node.as<std::string>();
    const auto parsed = parseLabel<CastPropagationPolicy>(label, propagationPolicyEnum);
    if (!parsed)
    {
        error = std::format("未知傳播政策「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseAreaModifierNode(const YAML::Node& node, AreaModifier& out, std::string& error)
{
    PayloadView payload(node, areaModifierPayload);
    if (!payload.validate(error)) return false;
    out = {};
    std::string type;
    if (!requiredString(payload, "類型", type, error)) return false;
    const auto kind = parseLabel<AreaModifierKind>(type, areaModifierKindEnum);
    if (!kind)
    {
        error = std::format("未知區域修正類型「{}」", type);
        return false;
    }
    out.kind = *kind;
    const auto relation = payload["關係"];
    if (!relation)
    {
        error = "區域修正缺少「關係」欄位";
        return false;
    }
    {
        const auto label = relation.as<std::string>();
        const auto parsed = parseLabel<EffectTeamFilter>(label, areaRelationEnum);
        if (!parsed)
        {
            error = std::format("未知區域關係「{}」", label);
            return false;
        }
        out.relation = *parsed;
    }
    if (const auto attribute = payload["屬性"])
    {
        if (!parseAttribute(attribute.as<std::string>(), out.attribute, error)) return false;
    }
    if (payload["數值"] && !parseEffectNumberNode(payload["數值"], out.amount, error)) return false;
    if (!optionalInt(payload, "百分比", out.percent, error)) return false;
    if (const auto damageKind = payload["傷害種類"])
    {
        if (!parseDamageChannel(damageKind.as<std::string>(), out.damageChannel, error)) return false;
    }
    if (payload["追蹤"])
    {
        bool tracking{};
        if (!optionalBool(payload, "追蹤", tracking, error)) return false;
        out.tracking = tracking;
    }
    if (payload["彈速百分比"])
    {
        int value{};
        if (!requiredInt(payload, "彈速百分比", value, error)) return false;
        out.speedPct = value;
    }
    if (payload["彈道壓制百分比"])
    {
        int value{};
        if (!requiredInt(payload, "彈道壓制百分比", value, error)) return false;
        out.projectilePressurePct = value;
    }
    if (const auto direction = payload["阻擋方向"])
    {
        const auto label = direction.as<std::string>();
        const auto parsed = parseLabel<ForceMoveDirection>(label, areaBlockedDirectionEnum);
        if (!parsed)
        {
            error = std::format("未知強制移動方向「{}」", label);
            return false;
        }
        out.blockedDirection = *parsed;
    }
    const auto overlap = payload["重疊方式"];
    if (!overlap)
    {
        error = "區域修正缺少「重疊方式」欄位";
        return false;
    }
    {
        const auto label = overlap.as<std::string>();
        const auto parsed = parseLabel<AreaOverlapPolicy>(label, areaOverlapPolicyEnum);
        if (!parsed)
        {
            error = std::format("未知區域重疊方式「{}」", label);
            return false;
        }
        out.overlap = *parsed;
    }

    const auto unexpected = [&](bool condition, std::string_view field)
    {
        if (!condition) return false;
        error = std::format("區域修正「{}」不接受「{}」欄位", type, field);
        return true;
    };
    if (out.kind == AreaModifierKind::Attribute)
    {
        if (!payload["屬性"] || !payload["數值"])
        {
            error = "屬性區域修正需要「屬性」與「數值」";
            return false;
        }
        if (unexpected(payload["百分比"] || payload["傷害種類"] || payload["追蹤"]
                || payload["彈速百分比"] || payload["彈道壓制百分比"] || payload["阻擋方向"], "非屬性修正")) return false;
        if (out.overlap == AreaOverlapPolicy::Any)
        {
            error = "數值屬性區域修正不可使用「任一」重疊方式";
            return false;
        }
    }
    else if (out.kind == AreaModifierKind::OutgoingDamage)
    {
        if (!payload["百分比"] || !payload["傷害種類"])
        {
            error = "造成傷害區域修正需要「百分比」與「傷害種類」";
            return false;
        }
        if (unexpected(payload["屬性"] || payload["數值"] || payload["追蹤"]
                || payload["彈速百分比"] || payload["彈道壓制百分比"] || payload["阻擋方向"], "非傷害修正")) return false;
        if (out.overlap == AreaOverlapPolicy::Any)
        {
            error = "數值傷害區域修正不可使用「任一」重疊方式";
            return false;
        }
    }
    else if (out.kind == AreaModifierKind::AttackSpawn)
    {
        if (!payload["追蹤"] && !payload["彈速百分比"] && !payload["彈道壓制百分比"])
        {
            error = "攻擊生成區域修正至少需要一個修正欄位";
            return false;
        }
        if (unexpected(payload["屬性"] || payload["數值"] || payload["百分比"]
                || payload["傷害種類"] || payload["阻擋方向"], "非攻擊生成修正")) return false;
        if (payload["追蹤"] && out.overlap != AreaOverlapPolicy::Any)
        {
            error = "追蹤布林修正必須使用「任一」重疊方式";
            return false;
        }
        if ((payload["彈速百分比"] || payload["彈道壓制百分比"])
            && out.overlap == AreaOverlapPolicy::Any)
        {
            error = "數值攻擊生成修正不可使用「任一」重疊方式";
            return false;
        }
    }
    else
    {
        if (!payload["阻擋方向"])
        {
            error = "強制移動免疫需要「阻擋方向」";
            return false;
        }
        if (unexpected(payload["屬性"] || payload["數值"] || payload["百分比"]
                || payload["傷害種類"] || payload["追蹤"] || payload["彈速百分比"]
                || payload["彈道壓制百分比"], "非強制移動免疫")) return false;
        if (out.overlap != AreaOverlapPolicy::Any)
        {
            error = "強制移動免疫必須使用「任一」重疊方式";
            return false;
        }
    }
    if (out.tracking) out.trackingOverlap = out.overlap;
    if (out.speedPct) out.speedOverlap = out.overlap;
    if (out.projectilePressurePct) out.projectilePressureOverlap = out.overlap;
    return payload.finish(error);
}

bool parseActionPayload(
    std::string_view type,
    PayloadView& node,
    EffectAction& out,
    std::string& error)
{
    if (type == "屬性修正")
    {
        ModifyAttributeAction action;
        std::string attribute;
        std::string operation;
        if (!requiredString(node, "屬性", attribute, error)
            || !requiredString(node, "方式", operation, error)
            || !parseAttribute(attribute, action.attribute, error)
            || !parseEffectNumberNode(node["數值"], action.amount, error)) return false;
        const auto parsedOperation = parseLabel<AttributeOperation>(operation, attributeOperationEnum);
        if (!parsedOperation)
        {
            error = std::format("未知屬性運算「{}」", operation);
            return false;
        }
        action.operation = *parsedOperation;
        if (!parseAttributeModifierQualifiers(node, action, error)) return false;
        out.value = std::move(action);
        return true;
    }
    if (type == "傷害修正")
    {
        ModifyDamageAction action;
        std::string stage;
        std::string channel;
        std::string operation;
        if (const auto perspective = node["方位"])
        {
            const auto label = perspective.as<std::string>();
            const auto parsed = parseLabel<DamageModifierPerspective>(
                label, damageModifierPerspectiveEnum);
            if (!parsed)
            {
                error = std::format("未知傷害修正方位「{}」", label);
                return false;
            }
            action.perspective = *parsed;
        }
        if (!requiredString(node, "階段", stage, error)
            || !requiredString(node, "傷害種類", channel, error)
            || !requiredString(node, "方式", operation, error)
            || !parseEffectNumberNode(node["數值"], action.amount, error)) return false;
        const auto parsedStage = parseLabel<DamageModifierStage>(stage, damageModifierStageEnum);
        const auto parsedOperation = parseLabel<DamageModifierOperation>(
            operation, damageModifierOperationEnum);
        if (!parsedStage || !parseDamageChannel(channel, action.channel, error) || !parsedOperation)
        {
            if (error.empty()) error = "未知傷害修正階段或方式";
            return false;
        }
        action.stage = *parsedStage;
        action.operation = *parsedOperation;
        if (!optionalInt(node, "持續幀數", action.durationFrames, error)
            || !parseStackPolicy(node["合併方式"], action.stack, error)) return false;
        if (node["層數上限"])
        {
            int limit{};
            if (!requiredInt(node, "層數上限", limit, error)) return false;
            action.stackLimit = limit;
        }
        if (const auto scope = node["疊加範圍"])
        {
            const auto label = scope.as<std::string>();
            const auto parsed = parseLabel<EffectStackScope>(label, stackScopeEnum);
            if (!parsed)
            {
                error = std::format("未知疊加範圍「{}」", label);
                return false;
            }
            action.stackScope = *parsed;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "資源變更")
    {
        ChangeResourceAction action;
        std::string resource;
        std::string kind;
        if (!requiredString(node, "資源", resource, error)
            || !requiredString(node, "方式", kind, error)
            || !parseResourceLabel(resource, action.resource, error)
            || !parseResourceChangeKindLabel(kind, action.kind, error)
            || !parseEffectNumberNode(node["數值"], action.amount, error)
            || !parseResourceMetadata(node, action, error)) return false;
        out.value = std::move(action);
        return true;
    }
    if (type == "治療交易修正")
    {
        ModifyHealTransactionAction action;
        std::string operation;
        if (!requiredString(node, "方式", operation, error)) return false;
        const auto parsedOperation = parseLabel<HealModifierOperation>(
            operation, healModifierOperationEnum);
        if (!parsedOperation)
        {
            error = std::format("未知治療修正方式「{}」", operation);
            return false;
        }
        action.operation = *parsedOperation;
        if (action.operation == HealModifierOperation::Block && node["百分比"])
        {
            error = "阻止治療不可填寫「百分比」";
            return false;
        }
        if (action.operation == HealModifierOperation::MultiplyReceived && !node["百分比"])
        {
            error = "受到治療乘算需要「百分比」";
            return false;
        }
        if (!optionalInt(node, "百分比", action.percent, error)
            || action.percent < 0 || action.percent > 100)
        {
            if (error.empty()) error = "治療乘算百分比必須介於 0 與 100";
            return false;
        }
        const auto kinds = node["治療種類"];
        if (!kinds || !kinds.IsSequence() || kinds.size() == 0)
        {
            error = "治療種類必須是非空列表";
            return false;
        }
        for (const auto& kind : kinds) action.kinds.push_back(kind.as<std::string>());
        out.value = std::move(action);
        return true;
    }
    if (type == "套用狀態")
    {
        ApplyStatusAction action;
        std::string status;
        if (!requiredString(node, "狀態", status, error)
            || !parseStatusKind(status, action.status, error)
            || !optionalInt(node, "層數", action.stacks, error)
            || !parseStackPolicy(node["合併方式"], action.stack, error)
            || !optionalBool(node, "同事件合計強度", action.aggregatePotencyWithinEvent, error)) return false;
        if (const auto duration = node["持續幀數"])
        {
            if (duration.IsScalar())
            {
                if (!requiredInt(node, "持續幀數", action.durationFrames, error)) return false;
            }
            else
            {
                EffectNumber formula;
                if (!parseEffectNumberNode(duration, formula, error)) return false;
                action.duration = std::move(formula);
            }
        }
        if (node["套用次數"])
        {
            EffectNumber count;
            if (!parseEffectNumberNode(node["套用次數"], count, error)) return false;
            action.applicationCount = std::move(count);
        }
        if (node["強度"] && !parseEffectNumberNode(node["強度"], action.potency, error)) return false;
        if (node["次要強度"] && !parseEffectNumberNode(node["次要強度"], action.secondaryPotency, error)) return false;
        if (node["層數上限"])
        {
            int limit{};
            if (!requiredInt(node, "層數上限", limit, error)) return false;
            action.stackLimit = limit;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "消耗狀態")
    {
        ConsumeStatusAction action;
        std::string status;
        if (!requiredString(node, "狀態", status, error)
            || !parseStatusKind(status, action.status, error)
            || !optionalInt(node, "層數", action.stacks, error)) return false;
        if (const auto source = node["狀態來源"])
        {
            const auto label = source.as<std::string>();
            const auto parsed = parseLabel<StatusSourceMatch>(label, statusSourceMatchEnum);
            if (!parsed)
            {
                error = std::format("未知狀態來源「{}」", label);
                return false;
            }
            action.source = *parsed;
        }
        if (const auto depleted = node["最後一層"])
        {
            std::vector<EffectAction> nestedActions;
            if (!parseAuthorActionNode(depleted, nestedActions, error)) return false;
            if (nestedActions.size() != 1)
            {
                error = "消耗最後一層需要恰好一個套用狀態動作";
                return false;
            }
            auto nested = std::move(nestedActions.front());
            const auto* statusAction = std::get_if<ApplyStatusAction>(&nested.value);
            if (!statusAction)
            {
                error = "消耗最後一層目前只允許套用狀態";
                return false;
            }
            action.whenDepleted = *statusAction;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "移除狀態")
    {
        RemoveStatusAction action;
        if (!optionalBool(node, "僅負面", action.negativeOnly, error)
            || !optionalBool(node, "僅控制", action.controlOnly, error)
            || !optionalBool(node, "解除目前動作僵直", action.clearCurrentActionStagger, error)
            || !optionalInt(node, "數量", action.count, error)) return false;
        if (const auto statuses = node["狀態"])
        {
            const auto append = [&](const YAML::Node& item)
            {
                BattleStatusKind status{};
                if (!parseStatusKind(item.as<std::string>(), status, error)) return false;
                action.statuses.push_back(status);
                return true;
            };
            if (statuses.IsSequence())
            {
                for (const auto& status : statuses) if (!append(status)) return false;
            }
            else if (!append(statuses)) return false;
        }
        if (const auto order = node["順序"])
        {
            const auto label = order.as<std::string>();
            const auto parsed = parseLabel<StatusRemovalOrder>(label, statusRemovalOrderEnum);
            if (!parsed)
            {
                error = std::format("未知狀態移除順序「{}」", label);
                return false;
            }
            action.order = *parsed;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "造成傷害")
    {
        DealDamageAction action;
        std::string kind;
        if (!requiredString(node, "傷害種類", kind, error)
            || !parseEffectNumberNode(node["數值"], action.amount, error)
            || !parseDamageKindLabel(kind, action.kind, error)) return false;
        if (node["交易次數"])
        {
            EffectNumber count;
            if (!parseEffectNumberNode(node["交易次數"], count, error)) return false;
            action.transactionCount = std::move(count);
        }
        if (const auto area = node["範圍"])
        {
            const auto label = area.as<std::string>();
            const auto parsed = parseLabel<DamageAreaKind>(label, damageAreaKindEnum);
            if (!parsed)
            {
                error = std::format("未知傷害範圍「{}」", label);
                return false;
            }
            action.area.kind = *parsed;
        }
        if (!optionalInt(node, "半徑格數", action.area.radiusTiles, error)
            || !optionalInt(node, "方形邊長", action.area.squareSideTiles, error)
            || !optionalInt(node, "同目標命中上限", action.perCast.perTargetLimit, error)
            || !optionalBool(node, "套用傷害修正", action.appliesDamageModifiers, error)
            || !optionalBool(node, "觸發受傷無敵", action.triggersHurtInvincibility, error)) return false;
        if (const auto projectileNode = node["區域投射物"])
        {
            PayloadView projectile(projectileNode, areaProjectilePayload);
            if (!projectile.validate(error)) return false;
            AreaProjectileDamageDelivery delivery;
            std::string visual;
            if (!requiredInt(projectile, "範圍格數", delivery.rangeTiles, error)
                || !requiredInt(projectile, "最多目標", delivery.maximumTargets, error)
                || !requiredInt(projectile, "眩暈幀數", delivery.stunFrames, error)
                || !optionalBool(projectile, "追蹤事件來源", delivery.trackEventSource, error)
                || !requiredString(projectile, "特效", visual, error)) return false;
            const auto parsedVisual = parseLabel<AreaProjectileVisual>(visual, areaProjectileVisualEnum);
            if (!parsedVisual)
            {
                error = std::format("未知區域投射物特效「{}」", visual);
                return false;
            }
            delivery.visual = *parsedVisual;
            if (!projectile.finish(error)) return false;
            action.areaProjectiles = delivery;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "修改攻擊")
    {
        ModifyAttackAction action;
        if (!parseAttackPatternFields(node, action.pattern, error)
            || !optionalInt(node, "傷害倍率", action.strengthPct, error)
            || !optionalBool(node, "貫穿", action.through, error)
            || !optionalBool(node, "追蹤", action.tracking, error)
            || !optionalBool(node, "視為主彈道", action.mainProjectile, error)
            || !optionalInt(node, "同目標命中上限", action.sameTargetHitLimit, error)
            || !optionalBool(node, "追加至基礎攻擊", action.addToBaseAttack, error)
            || !parsePropagation(node["傳播政策"], action.propagation, error)) return false;
        if (node["攻擊來源"])
        {
            EffectSelector selector;
            if (!parseSelectorNode(node["攻擊來源"], selector, error)) return false;
            action.source = std::move(selector);
        }
        if (node["傷害數值"])
        {
            EffectNumber number;
            if (!parseEffectNumberNode(node["傷害數值"], number, error)) return false;
            action.damageOverride = number;
        }
        if (const auto kind = node["傷害種類"])
        {
            const auto label = kind.as<std::string>();
            BattleDamageKind parsed{};
            if (!parseDamageKindLabel(label, parsed, error)) return false;
            action.damageKind = parsed;
        }
        if (const auto policy = node["目標政策"])
        {
            const auto label = policy.as<std::string>();
            const auto parsed = parseLabel<AttackTargetPolicy>(label, attackTargetPolicyEnum);
            if (!parsed)
            {
                error = std::format("未知攻擊目標政策「{}」", label);
                return false;
            }
            action.targets = *parsed;
        }
        if (node["執行行為"]
            && !parseAttackRuntimeBehavior(node["執行行為"], action.runtimeBehavior, error)) return false;
        out.value = std::move(action);
        return true;
    }
    if (type == "強制移動")
    {
        ForceMoveAction action;
        std::string direction;
        std::string collision;
        std::string blocked;
        if (!requiredString(node, "方向", direction, error)
            || !optionalInt(node, "距離格數", action.distanceTiles, error)
            || !optionalInt(node, "距離像素", action.distancePixels, error)
            || !optionalInt(node, "鎖定幀數", action.lockFrames, error)
            || !requiredString(node, "碰撞", collision, error)
            || !requiredString(node, "受阻結果", blocked, error)) return false;
        const auto parsedDirection = parseLabel<ForceMoveDirection>(
            direction, forceMoveDirectionEnum);
        const auto parsedCollision = parseLabel<ForceMoveCollision>(
            collision, forceMoveCollisionEnum);
        const auto parsedBlocked = parseLabel<ForceMoveBlockedResult>(
            blocked, forceMoveBlockedResultEnum);
        if (!parsedDirection)
        {
            error = std::format("未知強制移動方向「{}」", direction);
            return false;
        }
        if (!parsedCollision)
        {
            error = std::format("未知強制移動碰撞方式「{}」", collision);
            return false;
        }
        if (!parsedBlocked)
        {
            error = std::format("未知強制移動受阻結果「{}」", blocked);
            return false;
        }
        action.direction = *parsedDirection;
        action.collision = *parsedCollision;
        action.blocked = *parsedBlocked;
        out.value = std::move(action);
        return true;
    }
    if (type == "建立區域")
    {
        CreateAreaAction action;
        std::string shape;
        std::string anchor;
        std::string death;
        std::string merge;
        if (!requiredString(node, "形狀", shape, error)
            || !requiredString(node, "錨點", anchor, error)
            || !requiredString(node, "來源死亡", death, error)
            || !requiredString(node, "合併方式", merge, error)
            || !requiredInt(node, "持續幀數", action.durationFrames, error)
            || !optionalInt(node, "半徑格數", action.radiusTiles, error)
            || !optionalInt(node, "方形邊長", action.squareSideTiles, error)) return false;
        const auto parsedShape = parseLabel<AreaShape>(shape, areaShapeEnum);
        const auto parsedAnchor = parseLabel<AreaAnchor>(anchor, areaAnchorEnum);
        const auto parsedDeath = parseLabel<AreaSourceDeathPolicy>(death, areaSourceDeathPolicyEnum);
        const auto parsedMerge = parseLabel<AreaMergePolicy>(merge, areaMergePolicyEnum);
        if (!parsedShape || !parsedAnchor || !parsedDeath || !parsedMerge)
        {
            error = "未知區域形狀、錨點、死亡或合併政策";
            return false;
        }
        action.shape = *parsedShape;
        action.anchor = *parsedAnchor;
        action.sourceDeath = *parsedDeath;
        action.merge = *parsedMerge;
        const auto modifiers = node["區域修正"];
        if (!modifiers || !modifiers.IsSequence() || modifiers.size() == 0)
        {
            error = "建立區域需要非空區域修正列表";
            return false;
        }
        for (const auto& modifierNode : modifiers)
        {
            AreaModifier modifier;
            if (!parseAreaModifierNode(modifierNode, modifier, error)) return false;
            action.modifiers.push_back(std::move(modifier));
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "修改施放")
    {
        ModifyCastAction action;
        if (node["內力消耗"])
        {
            EffectNumber number;
            if (!parseEffectNumberNode(node["內力消耗"], number, error)) return false;
            action.mpCost = number;
        }
        if (const auto range = node["射程模式"])
        {
            const auto label = range.as<std::string>();
            const auto parsed = parseLabel<CastRangeMode>(label, castRangeModeEnum);
            if (!parsed)
            {
                error = std::format("未知射程模式「{}」", label);
                return false;
            }
            action.rangeMode = *parsed;
        }
        if (const auto mobility = node["機動政策"])
        {
            const auto label = mobility.as<std::string>();
            const auto parsed = parseLabel<CastMobilityPolicy>(label, castMobilityPolicyEnum);
            if (!parsed)
            {
                error = std::format("未知施放機動政策「{}」", label);
                return false;
            }
            action.mobility = *parsed;
        }
        if (const auto autoUltimate = node["自動絕招"])
        {
            PayloadView autoUltimateView(autoUltimate, autoUltimatePayload);
            if (!autoUltimateView.validate(error)) return false;
            AutoUltimateCastRequest request;
            if (!optionalBool(autoUltimateView, "消耗內力", request.consumeMp, error)
                || !optionalBool(autoUltimateView, "顯示公告", request.announce, error)
                || !autoUltimateView.finish(error)) return false;
            action.autoUltimate = request;
        }
        if (!optionalInt(node, "彈道速度百分比", action.projectileSpeedPct, error)
            || !optionalInt(node, "最小選擇距離", action.minimumSelectDistance, error)
            || !optionalInt(node, "追加彈道數", action.additionalProjectiles, error)
            || !optionalBool(node, "免費追加施放", action.freeAdditionalCast, error)
            || !parsePropagation(node["傳播政策"], action.propagation, error)) return false;
        if (node["樣式"] || node["數量"] || node["展開角度"] || node["間隔幀數"])
        {
            AttackPattern pattern;
            if (!parseAttackPatternFields(node, pattern, error)) return false;
            action.replacementPattern = pattern;
        }
        out.value = std::move(action);
        return true;
    }
    if (type == "狀態機")
    {
        std::string mechanism;
        if (!requiredString(node, "機制", mechanism, error)) return false;
        const auto parsedMechanism = parseLabel<StateMachineMechanism>(
            mechanism, stateMachineMechanismEnum);
        if (!parsedMechanism)
        {
            error = std::format("未知狀態機機制「{}」", mechanism);
            return false;
        }
        if (*parsedMechanism == StateMachineMechanism::ChangeStateValue)
        {
            ChangeStateValueAction action;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)
                || !requiredInt(node, "增量", action.delta, error)) return false;
            if (node["最小"])
            {
                int value{};
                if (!requiredInt(node, "最小", value, error)) return false;
                action.minimum = value;
            }
            if (node["最大"])
            {
                int value{};
                if (!requiredInt(node, "最大", value, error)) return false;
                action.maximum = value;
            }
            out.value = StateMachineAction{ action };
        }
        else if (*parsedMechanism == StateMachineMechanism::TransferStateValue)
        {
            TransferStateValueAction action;
            if (!parseEffectStateSlot(
                    node["來源狀態槽"],
                    action.sourceSlot,
                    error)
                || !parseEffectStateSlot(
                    node["目標狀態槽"],
                    action.destinationSlot,
                    error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (*parsedMechanism == StateMachineMechanism::RecordMaximumSkillDamage)
        {
            RecordMaximumDamageAction action;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (*parsedMechanism == StateMachineMechanism::ConsumeRecordAsDamage
            || *parsedMechanism == StateMachineMechanism::ConsumeRecordAsShield)
        {
            ConsumeRecordedMaximumAction action;
            action.destination = *parsedMechanism == StateMachineMechanism::ConsumeRecordAsShield
                ? StateValueDestination::ShieldAmount
                : StateValueDestination::DamageAmount;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)
                || !optionalInt(node, "百分比", action.percent, error)
                || !optionalBool(node, "消耗後清除", action.clearAfterConsume, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (*parsedMechanism == StateMachineMechanism::StartDamageAbsorption)
        {
            StartDamageAbsorptionAction action;
            std::string settlementDamageKind;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)
                || !requiredInt(node, "百分比", action.absorbedPct, error)
                || !requiredInt(node, "持續幀數", action.durationFrames, error)
                || !optionalBool(node, "死亡結算", action.settleOnSourceDeath, error)
                || !parseSelectorNode(node["結算目標"], action.settlementTarget, error)
                || !requiredString(node, "結算傷害種類", settlementDamageKind, error)
                || !parseDamageKindLabel(settlementDamageKind, action.settlementDamageKind, error)
                || !requiredInt(node, "結算百分比", action.returnedPct, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (*parsedMechanism == StateMachineMechanism::SettleDamageAbsorption)
        {
            SettleDamageAbsorptionAction action;
            if (!parseEffectStateSlot(node["狀態槽"], action.slot, error)
                || !optionalInt(node, "百分比", action.returnedPct, error)) return false;
            if (node["目標"] && !parseSelectorNode(node["目標"], action.target, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (*parsedMechanism == StateMachineMechanism::BorrowEffectRules)
        {
            BorrowEffectRulesAction action;
            if (!parseSelectorNode(node["目標"], action.sourceUnits, error)
                || !parseEffectNumberNode(node["來源數量"], action.sourceCount, error)
                || !parseBorrowedRuleFilter(node["允許動作類別"], action.filter, error)
                || !parsePropagation(node["傳播政策"], action.propagation, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (*parsedMechanism == StateMachineMechanism::CopyAttackDefinition)
        {
            CopyAttackDefinitionAction action;
            if (!parseSelectorNode(node["目標"], action.sourceUnits, error)
                || !parseCopiedMagicFilter(node["可選武功條件"], action.filter, error)
                || !optionalInt(node, "來源數量", action.copyCount, error)
                || !parsePropagation(node["傳播政策"], action.propagation, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (*parsedMechanism == StateMachineMechanism::SettleRemainingStatusDamage)
        {
            SettleRemainingStatusDamageAction action;
            std::string status;
            if (!requiredString(node, "狀態", status, error)
                || !parseStatusKind(status, action.status, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (*parsedMechanism == StateMachineMechanism::GenerateClones)
        {
            GenerateClonesAction action;
            if (!requiredInt(node, "數量", action.count, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (*parsedMechanism == StateMachineMechanism::PreventDeath)
        {
            PreventDeathAction action;
            if (!requiredInt(node, "無敵幀數", action.invincibilityFrames, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else if (*parsedMechanism == StateMachineMechanism::ConfigureProtectReposition
            || *parsedMechanism == StateMachineMechanism::ConfigureExecuteReposition)
        {
            ConfigureRescueRepositionAction action;
            action.mode = *parsedMechanism == StateMachineMechanism::ConfigureProtectReposition
                ? RescueRepositionMode::Protect
                : RescueRepositionMode::Execute;
            if (!requiredInt(node, "次數", action.activations, error)) return false;
            out.value = StateMachineAction{ action };
        }
        else assert(false);
        return true;
    }
    if (type == "條件分支")
    {
        const auto conditions = node["條件"];
        if (!conditions || !conditions.IsSequence() || conditions.size() == 0)
        {
            error = "條件分支需要非空條件列表";
            return false;
        }
        auto conditional = std::make_shared<ConditionalEffectAction>();
        for (const auto& conditionNode : conditions)
        {
            EffectCondition condition;
            if (!parseConditionNode(conditionNode, condition, error)) return false;
            conditional->conditions.push_back(std::move(condition));
        }
        if (!parseActionList(node["成立"], conditional->whenTrue, error)) return false;
        if (node["否則"] && !parseActionList(node["否則"], conditional->whenFalse, error)) return false;
        out.value = std::move(conditional);
        return true;
    }

    error = std::format("未知動作類型「{}」", type);
    return false;
}

bool parseNamedResourceAction(
    std::string_view name,
    const YAML::Node& node,
    EffectAction& out,
    std::string& error)
{
    PayloadView payload(node, resourceMacroPayload);
    if (!payload.validate(error)) return false;
    ChangeResourceAction action;
    std::string resource;
    if (!requiredString(payload, "資源", resource, error)
        || !parseResourceLabel(resource, action.resource, error)
        || !parseEffectNumberNode(payload["數值"], action.amount, error)) return false;
    if (name == "回復資源") action.kind = ResourceChangeKind::Restore;
    else if (name == "獲得資源") action.kind = ResourceChangeKind::Grant;
    else action.kind = ResourceChangeKind::Drain;
    if (!parseResourceMetadata(payload, action, error)
        || !payload.finish(error)) return false;
    out.value = std::move(action);
    return true;
}

bool parseNonEmptyEffectNumber(
    const YAML::Node& node,
    EffectNumber& out,
    std::string& error)
{
    if (node && node.IsMap() && node.size() == 0)
    {
        error = "簡式數值不可是空映射表";
        return false;
    }
    return parseEffectNumberNode(node, out, error);
}

bool parseAttributeBonusMacro(
    const YAML::Node& node,
    std::vector<EffectAction>& out,
    std::string& error)
{
    PayloadView payload(node, attributeBonusPayload);
    if (!payload.validate(error)) return false;

    ModifyAttributeAction qualifierPrototype;
    if (!parseAttributeModifierQualifiers(payload, qualifierPrototype, error)) return false;
    std::set<BattleAttribute> allAttributes;
    std::vector<ModifyAttributeAction> fixedActions;
    std::vector<ModifyAttributeAction> percentageActions;
    const auto appendAttribute = [&](
        std::string_view label,
        const YAML::Node& value,
        AttributeOperation operation,
        std::vector<ModifyAttributeAction>& actions)
    {
        BattleAttribute attribute{};
        if (!parseAttribute(label, attribute, error)) return false;
        if (!allAttributes.insert(attribute).second)
        {
            error = std::format("屬性加成重複指定屬性「{}」", label);
            return false;
        }
        if (!value.IsScalar())
        {
            error = std::format("屬性加成「{}」必須是 scalar 數字", label);
            return false;
        }
        ModifyAttributeAction action = qualifierPrototype;
        action.attribute = attribute;
        action.operation = operation;
        if (!parseEffectNumberNode(value, action.amount, error)) return false;
        actions.push_back(std::move(action));
        return true;
    };

    for (const auto& [label, value] : payload.dynamicEntries())
    {
        if (!appendAttribute(label, value, AttributeOperation::FlatAdd, fixedActions))
            return false;
    }
    if (const auto percentage = payload["百分比"])
    {
        if (!percentage.IsMap() || percentage.size() == 0)
        {
            error = "屬性加成「百分比」必須是非空映射表";
            return false;
        }
        PayloadView percentagePayload(percentage, attributePercentagePayload);
        if (!percentagePayload.validate(error)) return false;
        for (const auto& [label, value] : percentagePayload.dynamicEntries())
        {
            if (!appendAttribute(
                    label,
                    value,
                    AttributeOperation::PercentAdd,
                    percentageActions)) return false;
        }
        if (!percentagePayload.finish(error)) return false;
    }
    if (fixedActions.empty() && percentageActions.empty())
    {
        error = "屬性加成至少需要一個屬性";
        return false;
    }

    out.clear();
    std::ranges::sort(fixedActions, {}, &ModifyAttributeAction::attribute);
    std::ranges::sort(percentageActions, {}, &ModifyAttributeAction::attribute);
    for (auto& action : fixedActions) out.push_back(EffectAction{ std::move(action) });
    for (auto& action : percentageActions) out.push_back(EffectAction{ std::move(action) });
    return payload.finish(error);
}

bool parseNamedAction(
    std::string_view name,
    const YAML::Node& payload,
    std::vector<EffectAction>& out,
    std::string& error)
{
    out.clear();
    if (const auto* descriptor = findActionDescriptor(name))
    {
        if (payload.IsMap() && payload["類型"])
        {
            error = std::format("具名動作「{}」不可包含舊式「類型」欄位", name);
            return false;
        }
        PayloadView payloadView(payload, *descriptor->payload);
        if (!payloadView.validate(error)) return false;
        EffectAction action;
        if (!parseActionPayload(descriptor->name, payloadView, action, error)
            || !payloadView.finish(error)) return false;
        if (action.value.index() != descriptor->variantIndex)
        {
            error = std::format("動作「{}」dispatch 到錯誤的 typed variant", name);
            return false;
        }
        out.push_back(std::move(action));
        return true;
    }
    const auto* macro = findMacroDescriptor(name);
    if (!macro)
    {
        error = std::format("未知動作「{}」", name);
        return false;
    }
    if (payload.IsMap() && payload["類型"])
    {
        error = std::format("具名動作「{}」不可包含舊式「類型」欄位", name);
        return false;
    }
    if (macro->payloadKind == MacroPayloadKind::AttributeBonus)
        return parseAttributeBonusMacro(payload, out, error);
    if (name == "回復資源" || name == "獲得資源" || name == "奪取資源")
    {
        EffectAction action;
        if (!parseNamedResourceAction(name, payload, action, error)) return false;
        out.push_back(std::move(action));
        return true;
    }
    if (name == "回復內力" || name == "獲得護盾")
    {
        ChangeResourceAction action;
        action.resource = name == "回復內力" ? BattleResource::Mp : BattleResource::Shield;
        action.kind = name == "回復內力" ? ResourceChangeKind::Restore : ResourceChangeKind::Grant;
        if (!parseNonEmptyEffectNumber(payload, action.amount, error)) return false;
        out.push_back(EffectAction{ std::move(action) });
        return true;
    }
    if (name == "回復生命")
    {
        ChangeResourceAction action;
        action.resource = BattleResource::Hp;
        action.kind = ResourceChangeKind::Restore;
        if (payload.IsMap() && payload.size() == 0)
        {
            error = "回復生命不可使用空映射表";
            return false;
        }
        const bool metadataPayload = payload.IsMap()
            && (payload["數值"] || payload["治療種類"] || payload["來源政策"]);
        if (metadataPayload)
        {
            PayloadView healPayload(payload, *macro->payload);
            if (!healPayload.validate(error)
                || !parseNonEmptyEffectNumber(healPayload["數值"], action.amount, error)
                || !parseResourceMetadata(healPayload, action, error)
                || !healPayload.finish(error)) return false;
        }
        else if (!parseNonEmptyEffectNumber(payload, action.amount, error)) return false;
        out.push_back(EffectAction{ std::move(action) });
        return true;
    }
    if (name == "忽略防禦" || name == "單次承傷上限")
    {
        ModifyDamageAction action;
        if (!parseNonEmptyEffectNumber(payload, action.amount, error)) return false;
        if (name == "忽略防禦")
        {
            action.perspective = DamageModifierPerspective::Outgoing;
            action.stage = DamageModifierStage::BeforeDefense;
            action.channel = DamageChannel::Skill;
            action.operation = DamageModifierOperation::IgnoreDefensePercent;
        }
        else
        {
            action.perspective = DamageModifierPerspective::Incoming;
            action.stage = DamageModifierStage::Final;
            action.channel = DamageChannel::All;
            action.operation = DamageModifierOperation::CapSingleHitAtMaxHpPercent;
        }
        out.push_back(EffectAction{ std::move(action) });
        return true;
    }
    if (name == "擊退" || name == "拉近")
    {
        PayloadView movePayload(payload, *macro->payload);
        if (!movePayload.validate(error)) return false;
        ForceMoveAction action;
        action.direction = name == "擊退"
            ? ForceMoveDirection::AwayFromSource
            : ForceMoveDirection::TowardSource;
        action.collision = ForceMoveCollision::StopBeforeBlocked;
        action.blocked = ForceMoveBlockedResult::Shorten;
        if (!optionalInt(movePayload, "距離格數", action.distanceTiles, error)
            || !optionalInt(movePayload, "距離像素", action.distancePixels, error)
            || !optionalInt(movePayload, "鎖定幀數", action.lockFrames, error)
            || !movePayload.finish(error)) return false;
        out.push_back(EffectAction{ std::move(action) });
        return true;
    }

    error = std::format("動作巨集「{}」尚未實作", name);
    return false;
}

bool parseAuthorActionNode(
    const YAML::Node& node,
    std::vector<EffectAction>& out,
    std::string& error)
{
    if (!node || !node.IsMap())
    {
        error = "動作必須是映射表";
        return false;
    }
    if (!validateUniqueKeys(node, error)) return false;
    if (node.size() != 1)
    {
        error = "動作必須恰有一個具名動作欄位";
        return false;
    }
    const auto entry = *node.begin();
    return parseNamedAction(
        entry.first.as<std::string>(),
        entry.second,
        out,
        error);
}

}  // namespace

std::optional<int> effectiveConstantEffectNumberValue(const EffectNumber& number)
{
    if (number.base != EffectNumberBase::Constant || number.multiplierBase)
    {
        return std::nullopt;
    }

    int value = number.flat;
    if (number.minimum)
    {
        value = std::max(value, *number.minimum);
    }
    if (number.maximum)
    {
        value = std::min(value, *number.maximum);
    }
    return value;
}

bool battleAttributeUsesPercentagePoints(BattleAttribute attribute)
{
    switch (attribute)
    {
    case BattleAttribute::MaxHp:
    case BattleAttribute::Attack:
    case BattleAttribute::Defence:
    case BattleAttribute::Speed:
    case BattleAttribute::ProjectilePressureDamage:
        return false;
    case BattleAttribute::CriticalChance:
    case BattleAttribute::CriticalDamage:
    case BattleAttribute::DodgeChance:
    case BattleAttribute::BlockChance:
    case BattleAttribute::DamageReduction:
    case BattleAttribute::SkillDamage:
    case BattleAttribute::CooldownReduction:
    case BattleAttribute::MpRecoveryBonus:
    case BattleAttribute::StaggerResistance:
    case BattleAttribute::ProjectileReflectChance:
    case BattleAttribute::SkillReflectPercent:
    case BattleAttribute::CounterUltimateBlockChance:
    case BattleAttribute::CriticalAfterDodge:
    case BattleAttribute::DashChance:
    case BattleAttribute::OutgoingCooldownExtensionChance:
    case BattleAttribute::OutgoingCooldownExtensionPercent:
    case BattleAttribute::IncomingCooldownExtensionChance:
    case BattleAttribute::IncomingCooldownExtensionPercent:
        return true;
    }
    assert(false);
    return false;
}

bool attributeModifierIsNegative(AttributeOperation operation, int amount)
{
    switch (operation)
    {
    case AttributeOperation::FlatAdd:
    case AttributeOperation::PercentAdd:
    case AttributeOperation::Override:
        return amount < 0;
    case AttributeOperation::Multiply:
        return amount < 100;
    case AttributeOperation::AtLeast:
        return false;
    }
    assert(false);
    return false;
}

namespace
{

// The description pipeline deliberately keeps the parsed effect payload typed
// until the final rendering pass.  These nodes are private because the only
// public description API is effectDescription(EffectRule, style, context).
struct EffectDescriptionNode;
using EffectDescriptionChild = std::unique_ptr<EffectDescriptionNode>;

enum class DescriptionSubjectId
{
    None,
    RuleEvent,
    SelectedTargets,
    RecordedState,
};

struct DescriptionSubject
{
    DescriptionSubjectId id = DescriptionSubjectId::None;
};

enum class DescriptionPredicate
{
    ModifyAttribute = 0,
    ModifyDamage,
    ChangeResource,
    ModifyHealTransaction,
    ApplyStatus,
    ConsumeStatus,
    RemoveStatus,
    DealDamage,
    ModifyAttack,
    ForceMove,
    CreateArea,
    ModifyCast,
    StateMachine,
    Conditional,
    Trigger,
    RuleLimits,
    ResetRecordedState,
    RecordDamageAbsorption,
    SettleDamageAbsorption,
};

static_assert(
    static_cast<std::size_t>(DescriptionPredicate::Conditional) + 1
    == std::variant_size_v<EffectActionValue>);

struct DescriptionActionArgument
{
    EffectActionValue value;
};

struct DescriptionTriggerArgument
{
    EffectEvent event{};
    EffectObservationScope observation = EffectObservationScope::Owner;
    EffectCastMatch castMatch = EffectCastMatch::BoundMagic;
};

struct DescriptionResetStateArgument
{
    EffectStateSlot slot{};
};

struct DescriptionDamageAbsorptionRecordArgument
{
    int absorbedPct{};
    int durationFrames{};
};

struct DescriptionDamageAbsorptionSettlementArgument
{
    EffectSelector target;
    BattleDamageKind damageKind = BattleDamageKind::Pure;
    int returnedPct{};
    bool settleOnSourceDeath{};
};

using DescriptionArgument = std::variant<
    DescriptionActionArgument,
    DescriptionTriggerArgument,
    DescriptionResetStateArgument,
    DescriptionDamageAbsorptionRecordArgument,
    DescriptionDamageAbsorptionSettlementArgument>;

struct DescriptionChanceQualifier
{
    int percent{};
    auto operator<=>(const DescriptionChanceQualifier&) const = default;
};

struct DescriptionDurationFramesQualifier
{
    int frames{};
    auto operator<=>(const DescriptionDurationFramesQualifier&) const = default;
};

struct DescriptionStackPolicyQualifier
{
    EffectStackPolicy policy{};
    auto operator<=>(const DescriptionStackPolicyQualifier&) const = default;
};

struct DescriptionStackLimitQualifier
{
    int count{};
    auto operator<=>(const DescriptionStackLimitQualifier&) const = default;
};

struct DescriptionPerStackQualifier
{
    bool enabled{};
    auto operator<=>(const DescriptionPerStackQualifier&) const = default;
};

struct DescriptionStackScopeQualifier
{
    EffectStackScope scope{};
    auto operator<=>(const DescriptionStackScopeQualifier&) const = default;
};

struct DescriptionMaxActivationsQualifier
{
    int count{};
    auto operator<=>(const DescriptionMaxActivationsQualifier&) const = default;
};

struct DescriptionSharedCooldownQualifier
{
    int frames{};
    auto operator<=>(const DescriptionSharedCooldownQualifier&) const = default;
};

struct DescriptionIntervalQualifier
{
    int frames{};
    auto operator<=>(const DescriptionIntervalQualifier&) const = default;
};

struct DescriptionEveryNthEventQualifier
{
    int count{};
    auto operator<=>(const DescriptionEveryNthEventQualifier&) const = default;
};

struct DescriptionRepetitionCountQualifier
{
    EffectNumber count;

    bool operator==(const DescriptionRepetitionCountQualifier& other) const
    {
        return count.base == other.count.base
            && count.multiplierBase == other.count.multiplierBase
            && count.status == other.count.status
            && count.stateSlot == other.count.stateSlot
            && count.flat == other.count.flat
            && count.percent == other.count.percent
            && count.rounding == other.count.rounding
            && count.minimum == other.count.minimum
            && count.maximum == other.count.maximum;
    }
};

struct DescriptionActivationLimitQualifier
{
    EffectActivationScope scope{};
    int count{};
    auto operator<=>(const DescriptionActivationLimitQualifier&) const = default;
};

using DescriptionQualifier = std::variant<
    DescriptionChanceQualifier,
    DescriptionDurationFramesQualifier,
    DescriptionStackPolicyQualifier,
    DescriptionStackLimitQualifier,
    DescriptionPerStackQualifier,
    DescriptionStackScopeQualifier,
    DescriptionMaxActivationsQualifier,
    DescriptionSharedCooldownQualifier,
    DescriptionIntervalQualifier,
    DescriptionEveryNthEventQualifier,
    DescriptionRepetitionCountQualifier,
    DescriptionActivationLimitQualifier>;

struct DescriptionClause
{
    DescriptionSubject subject{};
    DescriptionPredicate predicate{};
    std::vector<DescriptionArgument> arguments;
    std::vector<DescriptionQualifier> qualifiers;
};

enum class DescriptionSequenceKind
{
    Rule,
    Actions,
};

struct DescriptionSequence
{
    DescriptionSequenceKind kind = DescriptionSequenceKind::Actions;
    std::vector<EffectDescriptionChild> steps;
};

struct DescriptionSimultaneous
{
    std::vector<EffectDescriptionChild> actions;
    std::vector<DescriptionQualifier> qualifiers;
};

struct DescriptionStatusDepletedCondition
{
    BattleStatusKind status{};
};

using DescriptionConditionValue = std::variant<
    EffectCondition,
    DescriptionStatusDepletedCondition>;

struct DescriptionCondition
{
    DescriptionConditionValue value;
};

enum class DescriptionConditionalScope
{
    Rule,
    PerTarget,
};

struct DescriptionConditional
{
    DescriptionConditionalScope scope = DescriptionConditionalScope::PerTarget;
    std::vector<DescriptionCondition> conditions;
    std::vector<DescriptionQualifier> qualifiers;
    EffectDescriptionChild whenTrue;
    EffectDescriptionChild whenFalse;
};

struct DescriptionTargetSet
{
    EffectSelector selector;
};

struct DescriptionForEach
{
    DescriptionTargetSet targets{};
    EffectDescriptionChild body;
};

struct DescriptionStateCycle
{
    EffectDescriptionChild record;
    EffectDescriptionChild consume;
    EffectDescriptionChild reset;
    std::optional<int> initialValue;
};

struct EffectDescriptionNode
{
    std::variant<
        DescriptionClause,
        DescriptionSequence,
        DescriptionSimultaneous,
        DescriptionConditional,
        DescriptionForEach,
        DescriptionStateCycle> value;

    template<typename T>
    explicit EffectDescriptionNode(T node)
        : value(std::move(node))
    {
    }
};

template<typename T>
EffectDescriptionChild makeDescriptionNode(T node)
{
    return std::make_unique<EffectDescriptionNode>(std::move(node));
}

std::string ruleEventLabel(EffectEvent event, EffectDescriptionStyle style)
{
    const bool detailed = style == EffectDescriptionStyle::Detailed;
    const bool compact = style == EffectDescriptionStyle::Compact;
    switch (event)
    {
    case EffectEvent::BattleInitialized: return detailed ? "戰鬥開始時" : "";
    case EffectEvent::FrameAdvanced: return "每幀";
    case EffectEvent::UltimateCooldownFinished: return compact ? "絕招冷卻完成" : "絕招冷卻完成時";
    case EffectEvent::CastPlanned: return compact ? "準備施放" : detailed ? "規劃施放時" : "準備施放時";
    case EffectEvent::AttackCommitted: return compact ? "出手" : "出手時";
    case EffectEvent::UltimateCommitted: return compact ? "絕招" : "施放絕招時";
    case EffectEvent::AttackSpawned: return compact ? "彈道生成" : "攻擊生成時";
    case EffectEvent::MainProjectileBeforeDamage:
        return compact ? "主彈命中" : detailed ? "主彈道命中、傷害結算前" : "主彈道命中時";
    case EffectEvent::HitBeforeDamage:
        return compact ? "命中" : detailed ? "命中且傷害結算前" : "每次命中時";
    case EffectEvent::DamageResolved: return compact ? "傷害後" : "傷害結算後";
    case EffectEvent::HealAttempted: return compact ? "治療前" : "嘗試治療時";
    case EffectEvent::HealApplied: return compact ? "治療後" : detailed ? "實際治療後" : "治療生效後";
    case EffectEvent::CastContinuation:
        return compact ? "施放延續" : detailed ? "本次施放第一輪攻擊完成後" : "第一輪攻擊後";
    case EffectEvent::CastSettled:
        return compact ? "施放結算" : detailed ? "本次施放全部攻擊結算完成後" : "本次施放結算後";
    case EffectEvent::ShieldBroken: return compact ? "破盾" : "護盾破裂時";
    case EffectEvent::UnitDied: return compact ? "死亡" : detailed ? "自身死亡時" : "死亡時";
    case EffectEvent::AllyDied: return compact ? "友軍死亡" : "友軍死亡時";
    }
    assert(false);
    return {};
}

std::string selectorLabel(const EffectSelector& selector, bool compact)
{
    const auto countSuffix = selector.count > 0 ? std::format("{}人", selector.count) : std::string{};
    std::string result;
    switch (selector.kind)
    {
    case EffectSelectorKind::Self: result = "自身"; break;
    case EffectSelectorKind::SourceUnit: result = "來源單位"; break;
    case EffectSelectorKind::TransactionTarget: result = "交易目標"; break;
    case EffectSelectorKind::HitTarget: result = "命中目標"; break;
    case EffectSelectorKind::OriginalAttackTarget: result = "原攻擊目標"; break;
    case EffectSelectorKind::ComboMembers: result = "羈絆成員"; break;
    case EffectSelectorKind::AllLivingUnits:
        result = selector.count > 0 ? std::format("存活單位{}", countSuffix) : "所有存活單位";
        break;
    case EffectSelectorKind::Allies:
        result = selector.count > 0 ? std::format("友軍{}", countSuffix) : "全隊";
        break;
    case EffectSelectorKind::Enemies:
        result = selector.count > 0 ? std::format("敵軍{}", countSuffix) : "所有敵人";
        break;
    case EffectSelectorKind::LowestHpAllies:
        result = selector.count > 0 ? std::format("生命比例最低{}", countSuffix) : "生命比例最低的友軍";
        break;
    case EffectSelectorKind::LowestMpAllies:
        result = selector.count > 0 ? std::format("內力最低{}", countSuffix) : "內力最低的友軍";
        break;
    case EffectSelectorKind::HighestMpEnemy: result = "內力最高的敵人"; break;
    case EffectSelectorKind::StrongestEnemies:
        result = selector.count > 0
            ? std::format("最強{}名敵人", selector.count)
            : "最強的敵人";
        break;
    case EffectSelectorKind::NearestEnemies:
        result = selector.count > 0
            ? std::format("最近{}名敵人", selector.count)
            : "最近的敵人";
        break;
    case EffectSelectorKind::FarthestEnemy: result = "最遠的敵人"; break;
    case EffectSelectorKind::UnitsInRadius:
    {
        const auto relation = selector.team == EffectTeamFilter::Ally ? "友軍"
            : selector.team == EffectTeamFilter::Enemy ? "敵軍"
            : "單位";
        result = selector.count > 0
            ? std::format("{}格內至多{}名{}", selector.radiusTiles, selector.count, relation)
            : std::format("{}格內所有{}", selector.radiusTiles, relation);
        break;
    }
    case EffectSelectorKind::UnitsInSquare:
    {
        const auto relation = selector.team == EffectTeamFilter::Ally ? "友軍"
            : selector.team == EffectTeamFilter::Enemy ? "敵軍"
            : "單位";
        result = selector.count > 0
            ? std::format("{}×{}範圍內至多{}名{}",
                selector.squareSideTiles,
                selector.squareSideTiles,
                selector.count,
                relation)
            : std::format("{}×{}範圍內所有{}",
                selector.squareSideTiles,
                selector.squareSideTiles,
                relation);
        break;
    }
    case EffectSelectorKind::AlliesUsingWeapon:
        result = compact
            ? std::format("友方武器{}", selector.requiredWeaponType)
            : std::format("使用武器類型{}的友軍", selector.requiredWeaponType);
        break;
    }
    assert(!result.empty());
    if (selector.excludeOwner)
    {
        result += "（不含自身）";
    }
    if (selector.requiredMagicId >= 0)
    {
        result += std::format("（使用武功{}）", selector.requiredMagicId);
    }
    if (selector.requiredTarget)
    {
        switch (*selector.requiredTarget)
        {
        case EffectRequiredTarget::Self: result += "（必含自身）"; break;
        case EffectRequiredTarget::SourceUnit: result += "（必含來源單位）"; break;
        case EffectRequiredTarget::TransactionTarget: result += "（必含交易目標）"; break;
        case EffectRequiredTarget::HitTarget: result += "（必含命中目標）"; break;
        case EffectRequiredTarget::OriginalAttackTarget: result += "（必含原攻擊目標）"; break;
        }
    }
    if (selector.tieBreak == EffectTieBreak::BattleRandom)
    {
        if (selector.kind == EffectSelectorKind::Enemies && selector.count > 0)
            result = "隨機" + result;
        else
            result += "（隨機決定同順位）";
    }
    return result;
}

std::string_view damageKindLabel(BattleDamageKind kind)
{
    switch (kind)
    {
    case BattleDamageKind::Physical: return "物理";
    case BattleDamageKind::Skill: return "招式";
    case BattleDamageKind::Pure: return "純粹";
    case BattleDamageKind::Poison: return "中毒";
    case BattleDamageKind::Bleed: return "流血";
    case BattleDamageKind::Effect: return "特效";
    case BattleDamageKind::Reflected: return "反彈";
    case BattleDamageKind::Execute: return "處決";
    }
    assert(false);
    return {};
}

std::string numberLabel(const EffectNumber& number)
{
    std::string base;
    const bool attackScaledByMissingHp = number.base == EffectNumberBase::SourceAttack
        && number.multiplierBase == EffectNumberBase::SourceMissingHpRatio;
    if (attackScaledByMissingHp)
    {
        base = std::format("目前攻擊×已損生命比例×{}%", number.percent);
    }
    else switch (number.base)
    {
    case EffectNumberBase::Constant:
        break;
    case EffectNumberBase::SourceStar:
        base = number.percent % 100 == 0
            ? std::format("星級×{}", number.percent / 100)
            : std::format("星級×{}%", number.percent);
        break;
    case EffectNumberBase::SourceAttack: base = std::format("攻擊的{}%", number.percent); break;
    case EffectNumberBase::SourceMaxHp: base = std::format("自身{}%最大生命", number.percent); break;
    case EffectNumberBase::SourceMissingHpRatio: base = std::format("已損生命比例的{}%", number.percent); break;
    case EffectNumberBase::SourceCurrentMpRatio: base = std::format("目前內力比例的{}%", number.percent); break;
    case EffectNumberBase::TargetMaxHp: base = std::format("目標{}%最大生命", number.percent); break;
    case EffectNumberBase::TargetCurrentHp: base = std::format("目標目前生命的{}%", number.percent); break;
    case EffectNumberBase::TargetCurrentShield: base = std::format("目標目前護盾的{}%", number.percent); break;
    case EffectNumberBase::TargetCurrentCooldown: base = std::format("目標目前冷卻的{}%", number.percent); break;
    case EffectNumberBase::FinalHpDamage: base = std::format("實際生命傷害的{}%", number.percent); break;
    case EffectNumberBase::AccumulatedStateValue: base = std::format("累計值的{}%", number.percent); break;
    case EffectNumberBase::SourceStatusPotency: base = std::format("{}強度的{}%", number.status, number.percent); break;
    case EffectNumberBase::SourceStatusStacks: base = std::format("{}層數的{}%", number.status, number.percent); break;
    case EffectNumberBase::StoredStateValue: base = std::format("狀態槽值的{}%", number.percent); break;
    }

    if (number.multiplierBase && !attackScaledByMissingHp)
    {
        switch (*number.multiplierBase)
        {
        case EffectNumberBase::Constant: base += "×固定基準"; break;
        case EffectNumberBase::SourceStar: base += "×星級"; break;
        case EffectNumberBase::SourceAttack: base += "×來源攻擊"; break;
        case EffectNumberBase::SourceMaxHp: base += "×來源最大生命"; break;
        case EffectNumberBase::SourceMissingHpRatio: base += "×來源已損生命比例"; break;
        case EffectNumberBase::SourceCurrentMpRatio: base += "×來源目前內力比例"; break;
        case EffectNumberBase::TargetMaxHp: base += "×目標最大生命"; break;
        case EffectNumberBase::TargetCurrentHp: base += "×目標目前生命"; break;
        case EffectNumberBase::TargetCurrentShield: base += "×目標目前護盾"; break;
        case EffectNumberBase::TargetCurrentCooldown: base += "×目標目前冷卻"; break;
        case EffectNumberBase::FinalHpDamage: base += "×實際生命傷害"; break;
        case EffectNumberBase::AccumulatedStateValue: base += "×累計狀態值"; break;
        case EffectNumberBase::SourceStatusPotency: base += std::format("×{}強度", number.status); break;
        case EffectNumberBase::SourceStatusStacks: base += std::format("×{}層數", number.status); break;
        case EffectNumberBase::StoredStateValue: base += "×狀態槽值"; break;
        }
    }
    if (number.base == EffectNumberBase::Constant)
    {
        return std::to_string(number.flat);
    }
    if (number.flat == 0)
    {
        return base;
    }
    return number.flat > 0
        ? std::format("{}＋{}", base, number.flat)
        : std::format("{}{}", base, number.flat);
}

std::string boundedNumberLabel(const EffectNumber& number)
{
    auto result = numberLabel(number);
    if (number.rounding == EffectRounding::Ceil)
    {
        result += "（向上取整）";
    }
    else if (number.rounding == EffectRounding::Floor)
    {
        result += "（向下取整）";
    }
    else if (number.rounding == EffectRounding::Nearest)
    {
        result += "（四捨五入）";
    }
    if (number.minimum)
    {
        result += std::format("·至少{}", *number.minimum);
    }
    if (number.maximum)
    {
        result += std::format("·至多{}", *number.maximum);
    }
    return result;
}

std::string descriptionNumberLabel(
    const EffectNumber& number,
    EffectDescriptionStyle style)
{
    if (style == EffectDescriptionStyle::Detailed)
        return boundedNumberLabel(number);

    if (const auto constant = effectiveConstantEffectNumberValue(number))
        return std::to_string(*constant);

    auto result = numberLabel(number);
    if (style == EffectDescriptionStyle::Compact)
    {
        if (number.minimum) result += std::format("·至少{}", *number.minimum);
        if (number.maximum) result += std::format("·至多{}", *number.maximum);
        return result;
    }
    if (number.minimum || number.maximum)
    {
        result += "（";
        if (number.minimum) result += std::format("至少{}", *number.minimum);
        if (number.minimum && number.maximum) result += "，";
        if (number.maximum) result += std::format("至多{}", *number.maximum);
        result += "）";
    }
    return result;
}

std::string_view borrowedRuleActionCategoryLabel(
    BorrowedRuleActionCategory category)
{
    switch (category)
    {
    case BorrowedRuleActionCategory::AttributeModifier: return "屬性修正";
    case BorrowedRuleActionCategory::DamageModifier: return "傷害修正";
    case BorrowedRuleActionCategory::ResourceChange: return "資源變更";
    case BorrowedRuleActionCategory::HealTransactionModifier: return "治療交易修正";
    case BorrowedRuleActionCategory::Status: return "狀態";
    case BorrowedRuleActionCategory::Damage: return "傷害";
    case BorrowedRuleActionCategory::Attack: return "攻擊";
    case BorrowedRuleActionCategory::ForcedMovement: return "強制移動";
    case BorrowedRuleActionCategory::Area: return "區域";
    case BorrowedRuleActionCategory::Cast: return "修改施放";
    case BorrowedRuleActionCategory::StateValue: return "狀態值";
    case BorrowedRuleActionCategory::DamageMemory: return "傷害記憶";
    case BorrowedRuleActionCategory::DamageAbsorption: return "傷害吸收";
    case BorrowedRuleActionCategory::StatusDamageSettlement: return "狀態傷害結算";
    }
    assert(false);
    return {};
}

std::string borrowedRuleFilterLabel(const BorrowedRuleFilter& filter)
{
    std::string result;
    for (const auto category : filter.allowedActionCategories)
    {
        if (!result.empty())
        {
            result += "、";
        }
        result += borrowedRuleActionCategoryLabel(category);
    }
    return result;
}

std::string_view copiedMagicConditionLabel(CopiedMagicCondition condition)
{
    switch (condition)
    {
    case CopiedMagicCondition::HasUltimateAttackDefinition:
        return "有絕招攻擊定義";
    case CopiedMagicCondition::ExcludesRecursiveEffects:
        return "排除複製與借用遞迴";
    }
    assert(false);
    return {};
}

std::string copiedMagicFilterLabel(const CopiedMagicFilter& filter)
{
    std::string result;
    for (const auto condition : filter.conditions)
    {
        if (!result.empty())
        {
            result += "、";
        }
        result += copiedMagicConditionLabel(condition);
    }
    return result;
}

std::string attributeLabel(BattleAttribute attribute, bool compact)
{
    switch (attribute)
    {
    case BattleAttribute::MaxHp: return compact ? "生命" : "最大生命";
    case BattleAttribute::Attack: return compact ? "攻" : "攻擊";
    case BattleAttribute::Defence: return compact ? "防" : "防禦";
    case BattleAttribute::Speed: return "速度";
    case BattleAttribute::CriticalChance: return compact ? "暴擊" : "暴擊率";
    case BattleAttribute::CriticalDamage: return "暴擊傷害";
    case BattleAttribute::DodgeChance: return compact ? "閃避" : "閃避率";
    case BattleAttribute::BlockChance: return compact ? "格擋" : "格擋率";
    case BattleAttribute::DamageReduction: return "傷害減免";
    case BattleAttribute::SkillDamage: return "技能傷害";
    case BattleAttribute::ProjectilePressureDamage: return compact ? "彈壓傷" : "彈道壓制傷害";
    case BattleAttribute::CooldownReduction: return "冷卻縮減";
    case BattleAttribute::MpRecoveryBonus: return compact ? "回內加成" : "內力回復加成";
    case BattleAttribute::StaggerResistance: return compact ? "僵抗" : "僵直抗性";
    case BattleAttribute::ProjectileReflectChance: return compact ? "彈反" : "彈道反射率";
    case BattleAttribute::SkillReflectPercent: return "技能反彈百分比";
    case BattleAttribute::CounterUltimateBlockChance: return compact ? "格擋反招" : "格擋絕招反擊率";
    case BattleAttribute::CriticalAfterDodge: return "閃避後暴擊";
    case BattleAttribute::DashChance: return "滑步機率";
    case BattleAttribute::OutgoingCooldownExtensionChance: return "攻擊冷卻延長率";
    case BattleAttribute::OutgoingCooldownExtensionPercent: return "攻擊冷卻延長百分比";
    case BattleAttribute::IncomingCooldownExtensionChance: return "受擊冷卻延長反擊率";
    case BattleAttribute::IncomingCooldownExtensionPercent: return "受擊冷卻延長百分比";
    }
    assert(false);
    return {};
}

std::string joinedDescriptionLabels(
    std::span<const std::string> labels,
    bool compact)
{
    std::string result;
    for (const auto& label : labels)
    {
        if (!result.empty()) result += compact ? "／" : "、";
        result += label;
    }
    return result;
}

std::string conditionLabel(const EffectCondition& condition, bool compact)
{
    return std::visit(
        [compact](const auto& typed) -> std::string
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, IsUltimateCondition>) return "僅限絕招";
            else if constexpr (std::is_same_v<T, MagicIdEqualsCondition>) return compact ? std::format("武功{}", typed.magicId) : std::format("武功ID為{}", typed.magicId);
            else if constexpr (std::is_same_v<T, IsMainProjectileCondition>) return "僅限主彈道";
            else if constexpr (std::is_same_v<T, IsRootAttackCondition>) return "僅限根攻擊";
            else if constexpr (std::is_same_v<T, SourceHpRatioAtMostCondition>) return std::format("自身生命不高於{}%", typed.percent);
            else if constexpr (std::is_same_v<T, SourceHpRatioBelowCondition>) return std::format("自身生命低於{}%", typed.percent);
            else if constexpr (std::is_same_v<T, SourceIsLastAliveCondition>) return "自身為最後存活";
            else if constexpr (std::is_same_v<T, TargetHpRatioAtMostCondition>) return std::format("目標生命不高於{}%", typed.percent);
            else if constexpr (std::is_same_v<T, TargetNotInvincibleCondition>) return "目標非無敵";
            else if constexpr (std::is_same_v<T, SourceHasStateCondition>) return std::format("自身有{}", typed.state);
            else if constexpr (std::is_same_v<T, TargetHasStateCondition>) return std::format("目標有{}", typed.state);
            else if constexpr (std::is_same_v<T, TargetHasStateFromEffectOwnerCondition>) return std::format("目標有此來源的{}", typed.state);
            else if constexpr (std::is_same_v<T, SourceStackAtLeastCondition>) return std::format("{}至少{}層", typed.stack, typed.count);
            else if constexpr (std::is_same_v<T, OtherLivingAllyUsesMagicCondition>) return std::format("另一名武功{}使用者存活", typed.magicId);
            else if constexpr (std::is_same_v<T, CastDistinctTargetCountAtLeastCondition>) return std::format("本次命中至少{}名不同敵人", typed.count);
            else if constexpr (std::is_same_v<T, AttackOrdinalEqualsCondition>) return std::format("第{}道攻擊", typed.ordinal + 1);
            else if constexpr (std::is_same_v<T, HealKindInCondition>)
                return std::format("治療種類為{}", joinedDescriptionLabels(typed.kinds, compact));
            else if constexpr (std::is_same_v<T, DamageOriginIsAttackCondition>) return "傷害來自招式";
            else if constexpr (std::is_same_v<T, DamageKilledTargetCondition>) return "該次傷害造成死亡";
            else if constexpr (std::is_same_v<T, AcceptedHitCondition>)
            {
                std::string result = "已接受命中";
                if (typed.requirePositiveDamage) result += "且傷害為正";
                if (typed.excludeReflected) result += "且不是反彈";
                return result;
            }
            else if constexpr (std::is_same_v<T, EventTargetBelongsToBoundSourceCondition>)
                return "事件目標屬於此效果來源";
            else if constexpr (std::is_same_v<T, DamagePerspectiveCondition>)
                return typed.perspective == DamagePerspective::Dealt ? "自身造成的傷害" : "自身承受的傷害";
            else if constexpr (std::is_same_v<T, DamageKindInCondition>)
                return std::format("傷害種類為{}", joinedDescriptionLabels(typed.kinds, compact));
            else if constexpr (std::is_same_v<T, TargetMpWasFullBeforeCastCondition>) return "受益者施放前內力已滿";
            else if constexpr (std::is_same_v<T, RandomSelectionAvailableCondition>) return "有合法隨機目標";
            else static_assert(false, "Unhandled effect condition");
        },
        condition);
}

bool descriptionQualifierIsSuppressed(
    std::span<const DescriptionQualifier> suppressed,
    const DescriptionQualifier& qualifier)
{
    return std::ranges::find(suppressed, qualifier) != suppressed.end();
}

std::string_view stackPolicyLabel(EffectStackPolicy policy, bool compact)
{
    switch (policy)
    {
    case EffectStackPolicy::Independent: return compact ? "獨立" : "各次效果獨立存在";
    case EffectStackPolicy::Refresh: return compact ? "刷新" : "重複套用只刷新時間";
    case EffectStackPolicy::Replace: return compact ? "取代" : "重複套用會取代既有效果";
    case EffectStackPolicy::KeepStrongest: return compact ? "取強" : "只保留最強效果";
    case EffectStackPolicy::AddStack: return compact ? "疊層" : "重複套用會增加層數";
    }
    assert(false);
    return {};
}

std::string_view damageChannelLabel(DamageChannel channel)
{
    switch (channel)
    {
    case DamageChannel::Skill: return "招式傷害";
    case DamageChannel::Dot: return "持續傷害";
    case DamageChannel::Effect: return "特效傷害";
    case DamageChannel::Reflected: return "反彈傷害";
    case DamageChannel::All: return "所有傷害";
    }
    assert(false);
    return {};
}

std::string_view damageChannelSourceLabel(DamageChannel channel)
{
    switch (channel)
    {
    case DamageChannel::Skill: return "招式";
    case DamageChannel::Dot: return "持續效果";
    case DamageChannel::Effect: return "特效";
    case DamageChannel::Reflected: return "反彈";
    case DamageChannel::All: return "任意來源";
    }
    assert(false);
    return {};
}

std::string_view damageStageLabel(DamageModifierStage stage)
{
    switch (stage)
    {
    case DamageModifierStage::BeforeDefense: return "防禦結算前";
    case DamageModifierStage::AfterDefense: return "防禦結算後";
    case DamageModifierStage::Final: return "最終結算";
    }
    assert(false);
    return {};
}

std::string_view healKindLabel(EffectHealKind kind)
{
    switch (kind)
    {
    case EffectHealKind::Direct: return "直接治療";
    case EffectHealKind::Team: return "隊伍治療";
    case EffectHealKind::Aura: return "治療光環";
    case EffectHealKind::OnHit: return "命中治療";
    case EffectHealKind::KillReward: return "擊殺治療";
    case EffectHealKind::DeathMedical: return "死亡醫療";
    case EffectHealKind::Rescue: return "救援治療";
    case EffectHealKind::Regeneration: return "持續回復";
    case EffectHealKind::Lifesteal: return "吸血";
    }
    assert(false);
    return {};
}

void appendTimedStackQualifiers(
    std::string& result,
    int durationFrames,
    EffectStackPolicy stack,
    const std::optional<int>& stackLimit,
    EffectStackScope stackScope,
    bool perStack,
    EffectDescriptionStyle style,
    bool ordinaryRefresh,
    std::span<const DescriptionQualifier> suppressed)
{
    const bool detailed = style == EffectDescriptionStyle::Detailed;
    const bool compact = style == EffectDescriptionStyle::Compact;
    const DescriptionQualifier duration = DescriptionDurationFramesQualifier{ durationFrames };
    const DescriptionQualifier stackPolicy = DescriptionStackPolicyQualifier{ stack };
    if (stack != EffectStackPolicy::Independent
        && !(ordinaryRefresh && stack == EffectStackPolicy::Refresh && !detailed)
        && !(!detailed && stack == EffectStackPolicy::AddStack && stackLimit)
        && !descriptionQualifierIsSuppressed(suppressed, stackPolicy))
    {
        result += compact ? "·" : detailed ? "；" : "，";
        result += stackPolicyLabel(stack, compact);
    }

    if (stackLimit)
    {
        const DescriptionQualifier limit = DescriptionStackLimitQualifier{ *stackLimit };
        if (!descriptionQualifierIsSuppressed(suppressed, limit))
            result += compact
                ? std::format("×{}層", *stackLimit)
                : std::format("，最多{}層", *stackLimit);
    }
    if (perStack)
    {
        const DescriptionQualifier qualifier = DescriptionPerStackQualifier{ true };
        if (!descriptionQualifierIsSuppressed(suppressed, qualifier))
            result += compact ? "·按層計算" : "，數值按每層計算";
    }
    if (stackScope == EffectStackScope::EventSource)
    {
        const DescriptionQualifier qualifier = DescriptionStackScopeQualifier{ stackScope };
        if (!descriptionQualifierIsSuppressed(suppressed, qualifier))
            result += compact ? "·分來源疊加" : "，各事件來源分別疊加";
    }
    if (durationFrames > 0 && !descriptionQualifierIsSuppressed(suppressed, duration))
        result += compact
            ? std::format("·{}幀", durationFrames)
            : detailed
            ? std::format("·{}幀", durationFrames)
            : std::format("，持續{}幀", durationFrames);
}

bool usesStackingOutgoingSkillDamagePhrase(const ModifyDamageAction& action)
{
    const auto amount = effectiveConstantEffectNumberValue(action.amount);
    return action.perspective == DamageModifierPerspective::Outgoing
        && action.channel == DamageChannel::Skill
        && action.stage == DamageModifierStage::AfterDefense
        && action.operation == DamageModifierOperation::PercentAdd
        && amount
        && *amount > 0
        && action.durationFrames > 0
        && action.stack == EffectStackPolicy::AddStack
        && action.stackLimit.has_value()
        && action.stackScope == EffectStackScope::Shared;
}

std::string renderDescriptionActionArgument(
    const EffectActionValue& action,
    EffectDescriptionStyle style,
    std::span<const DescriptionQualifier> suppressed = {});

std::string renderDescriptionActionArgument(
    const EffectActionValue& action,
    EffectDescriptionStyle style,
    std::span<const DescriptionQualifier> suppressed)
{
    const bool detailed = style == EffectDescriptionStyle::Detailed;
    const bool compact = style == EffectDescriptionStyle::Compact;
    const std::string_view qualifierSeparator = compact || detailed ? "·" : "，";
    return std::visit(
        [style, detailed, compact, qualifierSeparator, suppressed](const auto& typed) -> std::string
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, ModifyAttributeAction>)
            {
                const auto attribute = attributeLabel(typed.attribute, compact);
                const auto amount = descriptionNumberLabel(typed.amount, style);
                const auto attributeUnit = battleAttributeUsesPercentagePoints(typed.attribute)
                    ? "%"
                    : "";
                const auto constantAmount = effectiveConstantEffectNumberValue(typed.amount);
                const bool hasBounds = typed.amount.minimum || typed.amount.maximum;
                const bool useConstantPhrase = constantAmount && (!detailed || !hasBounds);
                std::string result;
                switch (typed.operation)
                {
                case AttributeOperation::FlatAdd:
                    result = useConstantPhrase
                        ? std::format(
                            "{}{:+}{}",
                            attribute,
                            *constantAmount,
                            attributeUnit)
                        : std::format("{}增加{}{}", attribute, amount, attributeUnit);
                    break;
                case AttributeOperation::PercentAdd:
                    if (!detailed
                        && typed.attribute == BattleAttribute::DamageReduction
                        && constantAmount)
                    {
                        result = std::format("減傷{:+}%", *constantAmount);
                    }
                    else if (useConstantPhrase)
                        result = std::format("{}{:+}%", attribute, *constantAmount);
                    else if (constantAmount)
                        result = std::format("{}增加{}%", attribute, amount);
                    else
                        result = std::format("{}增加{}", attribute, amount);
                    break;
                case AttributeOperation::Override:
                    result = std::format("{}改為{}{}", attribute, amount, attributeUnit);
                    break;
                case AttributeOperation::Multiply:
                    result = typed.amount.base == EffectNumberBase::Constant
                        ? std::format("{}乘以{}%", attribute, amount)
                        : std::format("{}乘以{}", attribute, amount);
                    break;
                case AttributeOperation::AtLeast:
                    result = std::format("{}至少為{}{}", attribute, amount, attributeUnit);
                    break;
                }
                appendTimedStackQualifiers(
                    result,
                    typed.durationFrames,
                    typed.stack,
                    typed.stackLimit,
                    typed.stackScope,
                    typed.perStack,
                    style,
                    true,
                    suppressed);
                return result;
            }
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
            {
                const auto constantAmount = effectiveConstantEffectNumberValue(typed.amount);
                if (!detailed && usesStackingOutgoingSkillDamagePhrase(typed))
                {
                    if (compact)
                    {
                        return std::format(
                            "增傷{}%×{}層·{}幀",
                            *constantAmount,
                            *typed.stackLimit,
                            typed.durationFrames);
                    }
                    return std::format(
                        "使招式傷害{:+}%，最多{}層；最後一次命中{}幀後清除層數",
                        *constantAmount,
                        *typed.stackLimit,
                        typed.durationFrames);
                }

                const auto percentAmount = [&]
                {
                    auto amount = descriptionNumberLabel(typed.amount, style);
                    if (typed.amount.base == EffectNumberBase::Constant) amount += "%";
                    return amount;
                };
                std::string result;
                const bool reviewedIncoming = !detailed
                    && typed.perspective == DamageModifierPerspective::Incoming
                    && typed.channel == DamageChannel::All
                    && typed.stage == DamageModifierStage::BeforeDefense
                    && typed.operation == DamageModifierOperation::PercentAdd
                    && constantAmount;
                const bool reviewedOutgoing = !detailed
                    && typed.perspective == DamageModifierPerspective::Outgoing
                    && typed.channel == DamageChannel::Skill
                    && typed.stage == DamageModifierStage::AfterDefense
                    && typed.operation == DamageModifierOperation::PercentAdd
                    && constantAmount;
                if (reviewedIncoming)
                {
                    result = *constantAmount < 0
                        ? std::format(
                            "減傷{}%",
                            -static_cast<std::int64_t>(*constantAmount))
                        : std::format("受傷{:+}%", *constantAmount);
                }
                else if (reviewedOutgoing)
                {
                    result = *constantAmount > 0
                        ? std::format("增傷{}%", *constantAmount)
                        : std::format("造成傷害{:+}%", *constantAmount);
                }
                else
                {
                    const auto perspective = typed.perspective == DamageModifierPerspective::Outgoing
                        ? compact ? "造成" : "造成的"
                        : compact ? "承受" : "承受的";
                    const auto damageContext = compact
                        ? std::format("{}{}", perspective, damageChannelLabel(typed.channel))
                        : std::format(
                            "{}{}（{}）",
                            perspective,
                            damageChannelLabel(typed.channel),
                            damageStageLabel(typed.stage));
                    if (typed.operation == DamageModifierOperation::IgnoreDefensePercent)
                        result = std::format("{}忽略{}防禦", damageContext, percentAmount());
                    else if (typed.operation == DamageModifierOperation::CapSingleHitAtMaxHpPercent)
                        result = std::format("{}下一次單次承傷不超過{}最大生命", damageContext, percentAmount());
                    else if (typed.operation == DamageModifierOperation::ExecuteBelowMaxHpPercent)
                        result = std::format("{}普通傷害後生命低於{}最大生命時處決", damageContext, percentAmount());
                    else
                    {
                        auto amount = descriptionNumberLabel(typed.amount, style);
                        if ((typed.operation == DamageModifierOperation::PercentAdd
                                || typed.operation == DamageModifierOperation::Multiply)
                            && typed.amount.base == EffectNumberBase::Constant)
                        {
                            amount += "%";
                        }
                        const auto operation = typed.operation == DamageModifierOperation::FlatAdd
                            ? "加算"
                            : typed.operation == DamageModifierOperation::PercentAdd
                            ? "百分比加算"
                            : "乘以";
                        result = std::format("{}{}{}", damageContext, operation, amount);
                    }
                    if (compact)
                    {
                        result += typed.stage == DamageModifierStage::BeforeDefense
                            ? "·防前"
                            : typed.stage == DamageModifierStage::AfterDefense
                            ? "·防後"
                            : "·最終";
                    }
                }
                appendTimedStackQualifiers(
                    result,
                    typed.durationFrames,
                    typed.stack,
                    typed.stackLimit,
                    typed.stackScope,
                    false,
                    style,
                    false,
                    suppressed);
                return result;
            }
            else if constexpr (std::is_same_v<T, ChangeResourceAction>)
            {
                const auto resource = typed.resource == BattleResource::Hp ? "生命"
                    : typed.resource == BattleResource::Mp ? "內力"
                    : typed.resource == BattleResource::Shield ? "護盾"
                    : typed.resource == BattleResource::StatusShield ? "狀態護盾"
                    : typed.resource == BattleResource::StaggerShield ? "僵直護盾"
                    : typed.resource == BattleResource::ActiveCooldown ? "目前冷卻"
                    : typed.resource == BattleResource::ControlImmunityFrames ? "僵直吸收幀數"
                    : "無敵幀數";
                const auto verb = typed.kind == ResourceChangeKind::Restore ? "回復"
                    : typed.kind == ResourceChangeKind::Drain ? "奪取"
                    : typed.kind == ResourceChangeKind::Grant ? "獲得"
                    : typed.kind == ResourceChangeKind::Remove ? "移除"
                    : typed.kind == ResourceChangeKind::Transfer ? "轉移"
                    : "至少刷新至";
                const bool numberIncludesResource = typed.resource == BattleResource::Hp
                    && (typed.amount.base == EffectNumberBase::SourceMaxHp
                        || typed.amount.base == EffectNumberBase::TargetMaxHp
                        || typed.amount.base == EffectNumberBase::TargetCurrentHp
                        || typed.amount.base == EffectNumberBase::FinalHpDamage);
                auto result = std::format(
                    "{}{}{}",
                    verb,
                    descriptionNumberLabel(typed.amount, style),
                    numberIncludesResource ? "" : resource);
                if (typed.kind == ResourceChangeKind::Transfer && typed.transferDestination)
                    result += std::format("至{}", selectorLabel(*typed.transferDestination, compact));
                if (typed.resource == BattleResource::Hp
                    && typed.kind == ResourceChangeKind::Restore)
                {
                    result += std::format("{}{}", qualifierSeparator, healKindLabel(typed.healKind));
                    if (typed.healSourcePolicy == EffectHealSourcePolicy::AllowDead)
                        result += std::format("{}來源死亡仍可生效", qualifierSeparator);
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
            {
                auto result = typed.operation == HealModifierOperation::Block
                    ? "無法受到治療"
                    : std::format("受到的治療改為{}%", typed.percent);
                result += std::format("{}限{}", qualifierSeparator, joinedDescriptionLabels(typed.kinds, compact));
                return result;
            }
            else if constexpr (std::is_same_v<T, ApplyStatusAction>)
            {
                auto result = std::format("施加{}", battleStatusLabel(typed.status));
                if (typed.stacks != 1) result += std::format("{}層", typed.stacks);
                if (typed.applicationCount)
                    result += std::format("{}獨立{}次", qualifierSeparator, descriptionNumberLabel(*typed.applicationCount, style));
                if (typed.potency.base != EffectNumberBase::Constant || typed.potency.flat != 0)
                    result += std::format("{}強度{}", qualifierSeparator, descriptionNumberLabel(typed.potency, style));
                if (typed.secondaryPotency.base != EffectNumberBase::Constant || typed.secondaryPotency.flat != 0)
                    result += std::format("{}次要強度{}", qualifierSeparator, descriptionNumberLabel(typed.secondaryPotency, style));
                if (typed.duration) result += std::format("{}{}幀", qualifierSeparator, descriptionNumberLabel(*typed.duration, style));
                appendTimedStackQualifiers(
                    result,
                    typed.duration ? 0 : typed.durationFrames,
                    typed.stack,
                    typed.stackLimit,
                    EffectStackScope::Shared,
                    false,
                    style,
                    false,
                    suppressed);
                if (typed.aggregatePotencyWithinEvent) result += std::format("{}同事件合計強度", qualifierSeparator);
                return result;
            }
            else if constexpr (std::is_same_v<T, ConsumeStatusAction>)
            {
                auto result = std::format("消耗{}{}層", battleStatusLabel(typed.status), typed.stacks);
                if (typed.source == StatusSourceMatch::EffectOwner) result += std::format("{}僅此來源", qualifierSeparator);
                assert(!typed.whenDepleted
                    && "depleted status branches must be rendered through DescriptionConditional");
                return result;
            }
            else if constexpr (std::is_same_v<T, RemoveStatusAction>)
            {
                std::string result;
                if (typed.controlOnly) result = "清除全部控制狀態";
                else if (typed.negativeOnly) result = typed.count == 0 ? "清除全部負面狀態" : std::format("清除{}個負面狀態", typed.count);
                else if (!typed.statuses.empty())
                {
                    result = "移除";
                    for (const auto status : typed.statuses)
                    {
                        if (!result.ends_with("移除")) result += "、";
                        result += battleStatusLabel(status);
                    }
                    if (typed.count > 0) result += std::format("中的{}個", typed.count);
                }
                if (typed.count > 0)
                {
                    result += typed.order == StatusRemovalOrder::LongestRemaining
                        ? std::format("{}優先最長剩餘", qualifierSeparator)
                        : typed.order == StatusRemovalOrder::Oldest
                        ? std::format("{}優先最早套用", qualifierSeparator)
                        : std::format("{}優先最新套用", qualifierSeparator);
                }
                if (typed.clearCurrentActionStagger)
                {
                    if (!result.empty()) result += "並";
                    result += "解除目前動作僵直（保留位置與動作）";
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, DealDamageAction>)
            {
                const auto kind = std::format("{}傷害", damageKindLabel(typed.kind));
                const auto area = typed.area.kind == DamageAreaKind::Square
                    ? std::format("{}×{}範圍", typed.area.squareSideTiles, typed.area.squareSideTiles)
                    : typed.area.kind == DamageAreaKind::Circle
                    ? std::format("{}格範圍", typed.area.radiusTiles)
                    : std::string{};
                auto result = std::format("{}造成{}{}", area, descriptionNumberLabel(typed.amount, style), kind);
                if (typed.transactionCount)
                    result += std::format("{}獨立{}次", qualifierSeparator, descriptionNumberLabel(*typed.transactionCount, style));
                if (!typed.appliesDamageModifiers)
                    result += std::format("{}不套用傷害修正", qualifierSeparator);
                if (!typed.triggersHurtInvincibility)
                    result += std::format("{}不觸發受傷無敵", qualifierSeparator);
                if (typed.perCast.perTargetLimit > 0)
                    result += std::format("{}每次施放對同一目標最多命中{}次", qualifierSeparator, typed.perCast.perTargetLimit);
                if (typed.areaProjectiles)
                {
                    const auto& delivery = *typed.areaProjectiles;
                    result += std::format(
                        "{}{}區域追蹤彈{}{}格{}最多{}目標",
                        qualifierSeparator,
                        delivery.visual == AreaProjectileVisual::DeathBlast
                            ? "死亡爆炸"
                            : "護盾爆炸",
                        qualifierSeparator,
                        delivery.rangeTiles,
                        qualifierSeparator,
                        delivery.maximumTargets);
                    if (delivery.trackEventSource) result += std::format("{}必含存活事件來源", qualifierSeparator);
                    if (delivery.stunFrames > 0)
                        result += std::format("{}眩暈{}幀", qualifierSeparator, delivery.stunFrames);
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, ModifyAttackAction>)
            {
                const auto pattern = typed.pattern.kind == AttackPatternKind::Fan ? "扇形"
                    : typed.pattern.kind == AttackPatternKind::Flanks ? "側翼"
                    : typed.pattern.kind == AttackPatternKind::SamePointSequence ? "同落點延遲"
                    : typed.pattern.kind == AttackPatternKind::MultiTarget ? "多目標"
                    : typed.pattern.kind == AttackPatternKind::EchoNearestOthers ? "殘影"
                    : "原樣式";
                auto result = std::format("{}{}×{}·{}%傷害",
                    pattern,
                    typed.mainProjectile ? "主彈" : "非主彈",
                    typed.pattern.projectileCount,
                    typed.strengthPct);
                if (!compact && !detailed)
                    result = std::format("{}{}×{}，{}%傷害",
                        pattern,
                        typed.mainProjectile ? "主彈" : "非主彈",
                        typed.pattern.projectileCount,
                        typed.strengthPct);
                if (typed.pattern.spreadDegrees > 0)
                    result += std::format("{}展開{}度", qualifierSeparator, typed.pattern.spreadDegrees);
                if (typed.through) result += std::format("{}{}", qualifierSeparator, *typed.through ? "貫穿" : "不貫穿");
                if (typed.tracking) result += std::format("{}{}", qualifierSeparator, *typed.tracking ? "追蹤" : "不追蹤");
                if (typed.sameTargetHitLimit > 0) result += std::format("{}同目標{}次", qualifierSeparator, typed.sameTargetHitLimit);
                if (typed.pattern.intervalFrames > 0) result += std::format("{}間隔{}幀", qualifierSeparator, typed.pattern.intervalFrames);
                if (typed.propagation == CastPropagationPolicy::SourceHitRulesOnly) result += std::format("{}僅觸發來源命中規則", qualifierSeparator);
                else if (typed.propagation == CastPropagationPolicy::SuppressUltimateRules) result += std::format("{}不觸發大招效果", qualifierSeparator);
                else if (typed.propagation == CastPropagationPolicy::BorrowedUltimateRules) result += std::format("{}觸發借用的大招規則", qualifierSeparator);
                else if (typed.propagation == CastPropagationPolicy::NoEffectRules) result += std::format("{}不觸發任何效果規則", qualifierSeparator);
                if (typed.addToBaseAttack) result += std::format("{}追加至基礎攻擊", qualifierSeparator);
                if (typed.targets == AttackTargetPolicy::SelectedTargets) result += std::format("{}選擇目標", qualifierSeparator);
                else if (typed.targets == AttackTargetPolicy::SamePoint) result += std::format("{}同落點", qualifierSeparator);
                else if (typed.targets == AttackTargetPolicy::SameTarget) result += std::format("{}同目標", qualifierSeparator);
                if (typed.source) result += std::format("{}由{}出手", qualifierSeparator, selectorLabel(*typed.source, true));
                if (typed.damageOverride)
                {
                    result += std::format("{}{}", qualifierSeparator, descriptionNumberLabel(*typed.damageOverride, style));
                    if (typed.damageKind)
                        result += std::format("{}傷害", damageKindLabel(*typed.damageKind));
                    else
                        result += "傷害（沿用原傷害種類）";
                }
                else if (typed.damageKind)
                {
                    result += std::format("{}傷害種類改為{}", qualifierSeparator, damageKindLabel(*typed.damageKind));
                }
                std::visit(
                    [&](const auto& behavior)
                    {
                        using B = std::decay_t<decltype(behavior)>;
                        if constexpr (std::is_same_v<B, ProjectileBounceAttackBehavior>)
                        {
                            result += std::format(
                                "{}彈射追加命中{}次{}{}%{}{}像素",
                                qualifierSeparator,
                                behavior.additionalHits,
                                qualifierSeparator,
                                behavior.chancePct,
                                qualifierSeparator,
                                behavior.rangePixels);
                        }
                        else if constexpr (std::is_same_v<B, NearbyTrackingAttackBehavior>)
                        {
                            result += std::format(
                                "{}{}像素內產生{}%傷害追蹤彈",
                                qualifierSeparator,
                                behavior.rangePixels,
                                behavior.damagePct);
                        }
                        else if constexpr (std::is_same_v<B, DelayedAlternateAttackBehavior>)
                        {
                            result += std::format(
                                "{}延遲{}幀替代目標追擊{}{}%傷害{}{}%獲得格擋",
                                qualifierSeparator,
                                behavior.delayFrames,
                                qualifierSeparator,
                                behavior.damagePct,
                                qualifierSeparator,
                                behavior.attackerBlockGainChancePct);
                        }
                        else if constexpr (std::is_same_v<B, ExpandingSpiralAttackBehavior>)
                        {
                            result += std::format(
                                "{}擴張螺旋彈×{}{}流血{}層",
                                qualifierSeparator,
                                behavior.projectileCount,
                                qualifierSeparator,
                                behavior.bleedStacks);
                        }
                    },
                    typed.runtimeBehavior);
                return result;
            }
            else if constexpr (std::is_same_v<T, ForceMoveAction>)
            {
                const auto direction = typed.direction == ForceMoveDirection::AwayFromSource
                    ? "擊退"
                    : typed.direction == ForceMoveDirection::TowardSource
                    ? "拉近"
                    : "移向指定點";
                auto result = typed.distancePixels > 0
                    ? std::format("{}{}像素並鎖定{}幀", direction, typed.distancePixels, typed.lockFrames)
                    : std::format("{}{}格並鎖定{}幀", direction, typed.distanceTiles, typed.lockFrames);
                result += std::format("{}{}", qualifierSeparator,
                    typed.collision == ForceMoveCollision::StopBeforeOccupied
                        ? "在佔位前停止"
                        : "在阻擋地形前停止");
                result += std::format("{}{}", qualifierSeparator,
                    typed.blocked == ForceMoveBlockedResult::Shorten
                        ? "受阻時縮短位移"
                        : "受阻時取消位移");
                return result;
            }
            else if constexpr (std::is_same_v<T, CreateAreaAction>)
            {
                if (!detailed)
                {
                    const auto relationLabel = [compact](EffectTeamFilter relation)
                    {
                        if (compact)
                            return relation == EffectTeamFilter::Ally ? std::string("友")
                                : relation == EffectTeamFilter::Enemy ? std::string("敵")
                                : std::string("全體");
                        return relation == EffectTeamFilter::Ally ? std::string("友方")
                            : relation == EffectTeamFilter::Enemy ? std::string("敵人")
                            : std::string("所有單位");
                    };
                    std::string result;
                    if (compact)
                    {
                        result = typed.shape == AreaShape::Circle
                            ? std::format("區域{}格·{}幀", typed.radiusTiles, typed.durationFrames)
                            : std::format("區域{}×{}·{}幀", typed.squareSideTiles, typed.squareSideTiles, typed.durationFrames);
                    }
                    else
                    {
                        const auto location = typed.anchor == AreaAnchor::HitPosition
                            ? "在命中位置"
                            : "以來源單位為中心";
                        result = typed.shape == AreaShape::Circle
                            ? std::format("{}建立半徑{}格的區域，持續{}幀", location, typed.radiusTiles, typed.durationFrames)
                            : std::format("{}建立{}×{}格的區域，持續{}幀", location, typed.squareSideTiles, typed.squareSideTiles, typed.durationFrames);
                    }

                    if (typed.sourceDeath == AreaSourceDeathPolicy::RemoveImmediately)
                        result += compact ? "·來源死即消" : "，來源死亡時立即移除";
                    if (typed.merge == AreaMergePolicy::Independent)
                        result += compact ? "·區域獨立" : "，各區域獨立";
                    else if (typed.merge == AreaMergePolicy::ReplaceSameSource)
                        result += compact ? "·同源取代" : "，同來源取代舊區域";

                    struct PlayerAreaModifierLabel
                    {
                        EffectTeamFilter relation{};
                        std::string text{};
                    };
                    struct PlayerAreaAttackPercentLabel
                    {
                        EffectTeamFilter relation{};
                        std::string text{};
                        int amount{};
                    };
                    std::vector<PlayerAreaModifierLabel> modifierLabels;
                    std::vector<PlayerAreaAttackPercentLabel> attackPercentLabels;
                    std::vector<PlayerAreaModifierLabel> trackingLabels;
                    for (const auto& modifier : typed.modifiers)
                    {
                        if (modifier.kind == AreaModifierKind::Attribute)
                        {
                            const auto constantAmount = effectiveConstantEffectNumberValue(modifier.amount);
                            const bool useConstantPhrase = constantAmount
                                && (!detailed || (!modifier.amount.minimum && !modifier.amount.maximum));
                            const auto amount = useConstantPhrase
                                ? std::format("{:+}%", *constantAmount)
                                : std::format("增加{}%", descriptionNumberLabel(modifier.amount, style));
                            modifierLabels.push_back({
                                modifier.relation,
                                std::format(
                                    "{}{}",
                                    compact && modifier.attribute == BattleAttribute::Speed
                                        ? "速"
                                        : attributeLabel(modifier.attribute, compact),
                                    amount),
                            });
                        }
                        else if (modifier.kind == AreaModifierKind::OutgoingDamage)
                        {
                            modifierLabels.push_back({
                                modifier.relation,
                                std::format(
                                    "造成的{}{:+}%",
                                    damageChannelLabel(modifier.damageChannel),
                                    modifier.percent),
                            });
                        }
                        else if (modifier.kind == AreaModifierKind::AttackSpawn)
                        {
                            if (modifier.tracking)
                            {
                                trackingLabels.push_back({
                                    modifier.relation,
                                    *modifier.tracking
                                        ? "彈道可追蹤"
                                        : compact ? "禁追蹤" : "彈道無法追蹤",
                                });
                            }
                            if (modifier.speedPct)
                                attackPercentLabels.push_back({
                                    modifier.relation,
                                    "彈速",
                                    *modifier.speedPct,
                                });
                            if (modifier.projectilePressurePct)
                                attackPercentLabels.push_back({
                                    modifier.relation,
                                    compact ? "壓制" : "壓制傷害",
                                    *modifier.projectilePressurePct,
                                });
                        }
                        else
                        {
                            const auto direction = modifier.blockedDirection == ForceMoveDirection::AwayFromSource
                                ? "擊退"
                                : modifier.blockedDirection == ForceMoveDirection::TowardSource
                                ? "拉近"
                                : "指定點位移";
                            modifierLabels.push_back({
                                modifier.relation,
                                std::format("免疫{}", direction),
                            });
                        }
                    }
                    if (!compact)
                        modifierLabels.insert(
                            modifierLabels.end(),
                            trackingLabels.begin(),
                            trackingLabels.end());
                    while (!attackPercentLabels.empty())
                    {
                        const auto relation = attackPercentLabels.front().relation;
                        const int amount = attackPercentLabels.front().amount;
                        std::string labels;
                        for (auto it = attackPercentLabels.begin(); it != attackPercentLabels.end();)
                        {
                            if (it->relation != relation || it->amount != amount)
                            {
                                ++it;
                                continue;
                            }
                            if (!labels.empty()) labels += compact ? "／" : "及";
                            labels += it->text;
                            it = attackPercentLabels.erase(it);
                        }
                        modifierLabels.push_back({
                            relation,
                            std::format("{}{:+}%", labels, amount),
                        });
                    }
                    if (compact)
                        modifierLabels.insert(
                            modifierLabels.end(),
                            trackingLabels.begin(),
                            trackingLabels.end());
                    const bool commonRelation = std::ranges::all_of(
                        typed.modifiers,
                        [&](const AreaModifier& modifier)
                        {
                            return modifier.relation == typed.modifiers.front().relation;
                        });
                    bool firstModifier = true;
                    for (const auto& label : modifierLabels)
                    {
                        result += compact ? "·" : firstModifier ? "；" : "，";
                        if (!commonRelation || firstModifier)
                        {
                            if (!compact) result += "區域內";
                            result += relationLabel(label.relation);
                        }
                        result += label.text;
                        firstModifier = false;
                    }
                    return result;
                }

                auto result = typed.shape == AreaShape::Circle
                    ? std::format("建立半徑{}格、持續{}幀的區域", typed.radiusTiles, typed.durationFrames)
                    : std::format("建立{}×{}、持續{}幀的區域", typed.squareSideTiles, typed.squareSideTiles, typed.durationFrames);
                result += typed.anchor == AreaAnchor::HitPosition ? "·固定於命中位置" : "·跟隨來源單位";
                result += typed.sourceDeath == AreaSourceDeathPolicy::PersistUntilExpiry
                    ? "·來源死亡後持續至到期"
                    : "·來源死亡時立即移除";
                result += typed.merge == AreaMergePolicy::Independent
                    ? "·各區域獨立"
                    : typed.merge == AreaMergePolicy::RefreshSameSource
                    ? "·同來源刷新時間"
                    : "·同來源取代舊區域";
                const auto relationLabel = [](EffectTeamFilter relation)
                {
                    return relation == EffectTeamFilter::Ally ? "友方"
                        : relation == EffectTeamFilter::Enemy ? "敵方"
                        : "所有單位";
                };
                const auto overlapLabel = [](AreaOverlapPolicy overlap)
                {
                    return overlap == AreaOverlapPolicy::Add ? "相加"
                        : overlap == AreaOverlapPolicy::KeepStrongest ? "取最強"
                        : "任一成立";
                };
                for (const auto& modifier : typed.modifiers)
                {
                    result += compact ? "·" : "；";
                    if (modifier.kind == AreaModifierKind::Attribute)
                    {
                        const auto constantAmount = effectiveConstantEffectNumberValue(modifier.amount);
                        const bool useConstantPhrase = constantAmount
                            && (!detailed || (!modifier.amount.minimum && !modifier.amount.maximum));
                        const auto amount = useConstantPhrase
                            ? std::format("{:+}", *constantAmount)
                            : std::format("增加{}", descriptionNumberLabel(modifier.amount, style));
                        result += std::format("區域內{}{}{}",
                            relationLabel(modifier.relation),
                            attributeLabel(modifier.attribute, compact),
                            amount);
                    }
                    else if (modifier.kind == AreaModifierKind::OutgoingDamage)
                    {
                        result += std::format("區域內{}造成的{}{}%",
                            relationLabel(modifier.relation),
                            damageChannelLabel(modifier.damageChannel),
                            modifier.percent >= 0 ? std::format("+{}", modifier.percent) : std::to_string(modifier.percent));
                    }
                    else if (modifier.kind == AreaModifierKind::AttackSpawn)
                    {
                        result += std::format("區域內{}的新彈道", relationLabel(modifier.relation));
                        if (modifier.tracking) result += *modifier.tracking ? "·可追蹤" : "·不可追蹤";
                        if (modifier.speedPct) result += std::format("·彈速{}%", *modifier.speedPct);
                        if (modifier.projectilePressurePct) result += std::format("·彈道壓制傷害{}%", *modifier.projectilePressurePct);
                    }
                    else
                    {
                        const auto direction = modifier.blockedDirection == ForceMoveDirection::AwayFromSource
                            ? "擊退"
                            : modifier.blockedDirection == ForceMoveDirection::TowardSource
                            ? "拉近"
                            : "移向指定點";
                        result += std::format("區域內{}免疫{}", relationLabel(modifier.relation), direction);
                    }
                    result += std::format("·重疊{}", overlapLabel(modifier.overlap));
                    if (modifier.trackingOverlap)
                        result += std::format("·追蹤重疊{}", overlapLabel(*modifier.trackingOverlap));
                    if (modifier.speedOverlap)
                        result += std::format("·彈速重疊{}", overlapLabel(*modifier.speedOverlap));
                    if (modifier.projectilePressureOverlap)
                        result += std::format("·彈壓重疊{}", overlapLabel(*modifier.projectilePressureOverlap));
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, ModifyCastAction>)
            {
                std::string result;
                const auto append = [&](std::string fragment)
                {
                    if (!result.empty()) result += compact ? "·" : "，";
                    result += fragment;
                };
                if (typed.mpCost) append(std::format("實際消耗{}內力", descriptionNumberLabel(*typed.mpCost, style)));
                if (typed.rangeMode == CastRangeMode::Ranged) append("武功遠程化");
                else if (typed.rangeMode == CastRangeMode::Preserve) append("保留原射程模式");
                if (typed.projectileSpeedPct > 0)
                    append(std::format("彈道速度{}%", typed.projectileSpeedPct));
                if (typed.minimumSelectDistance > 0)
                    append(std::format("最小選擇距離{}", typed.minimumSelectDistance));
                if (typed.additionalProjectiles > 0)
                    append(std::format("追加彈道{}枚", typed.additionalProjectiles));
                if (typed.mobility == CastMobilityPolicy::DashAttack) append("啟用滑步攻擊");
                else if (typed.mobility == CastMobilityPolicy::BlinkAttack) append("啟用閃擊");
                if (typed.autoUltimate)
                {
                    append(std::format(
                        "自動施放絕招（{}內力，{}公告）",
                        typed.autoUltimate->consumeMp ? "消耗" : "不消耗",
                        typed.autoUltimate->announce ? "顯示" : "不顯示"));
                }
                if (typed.replacementPattern)
                {
                    const auto& pattern = *typed.replacementPattern;
                    const auto patternName = pattern.kind == AttackPatternKind::Fan ? "扇形"
                        : pattern.kind == AttackPatternKind::Flanks ? "側翼"
                        : pattern.kind == AttackPatternKind::SamePointSequence ? "同落點延遲"
                        : pattern.kind == AttackPatternKind::MultiTarget ? "多目標"
                        : pattern.kind == AttackPatternKind::EchoNearestOthers ? "殘影"
                        : "原樣式";
                    auto replacement = std::format("替換攻擊樣式為{}×{}", patternName, pattern.projectileCount);
                    if (pattern.spreadDegrees > 0) replacement += std::format("{}展開{}度", qualifierSeparator, pattern.spreadDegrees);
                    if (pattern.intervalFrames > 0) replacement += std::format("{}間隔{}幀", qualifierSeparator, pattern.intervalFrames);
                    append(std::move(replacement));
                }
                if (typed.freeAdditionalCast) append("免費追加相同施放");
                if (typed.propagation == CastPropagationPolicy::SourceHitRulesOnly)
                    append("僅觸發來源命中規則");
                else if (typed.propagation == CastPropagationPolicy::SuppressUltimateRules)
                    append("不再次觸發大招效果");
                else if (typed.propagation == CastPropagationPolicy::BorrowedUltimateRules)
                    append("觸發借用的大招規則");
                else if (typed.propagation == CastPropagationPolicy::NoEffectRules)
                    append("不觸發任何效果規則");
                return result;
            }
            else if constexpr (std::is_same_v<T, StateMachineAction>)
            {
                return std::visit(
                    [style, compact, qualifierSeparator](const auto& machine) -> std::string
                    {
                        using M = std::decay_t<decltype(machine)>;
                        if constexpr (std::is_same_v<M, ChangeStateValueAction>)
                        {
                            auto result = std::format("狀態值{:+}", machine.delta);
                            if (machine.minimum) result += std::format("{}下限{}", qualifierSeparator, *machine.minimum);
                            if (machine.maximum) result += std::format("{}上限{}", qualifierSeparator, *machine.maximum);
                            return result;
                        }
                        else if constexpr (std::is_same_v<M, TransferStateValueAction>)
                            return "轉移記錄值至另一狀態槽並清空來源";
                        else if constexpr (std::is_same_v<M, RecordMaximumDamageAction>)
                            return std::format("記錄最大單次{}生命傷害", damageChannelSourceLabel(machine.channel));
                        else if constexpr (std::is_same_v<M, ConsumeRecordedMaximumAction>)
                        {
                            const auto amount = machine.percent == 100 ? "等量" : std::format("{}%", machine.percent);
                            auto result = machine.clearAfterConsume
                                ? (machine.destination == StateValueDestination::DamageAmount
                                    ? std::format("消耗記錄值並附加{}純粹傷害", amount)
                                    : std::format("消耗記錄值並獲得{}護盾", amount))
                                : (machine.destination == StateValueDestination::DamageAmount
                                    ? std::format("讀取記錄值並附加{}純粹傷害", amount)
                                    : std::format("讀取記錄值並獲得{}護盾", amount));
                            return result;
                        }
                        else if constexpr (std::is_same_v<M, StartDamageAbsorptionAction>)
                        {
                            assert(false && "damage absorption must be rendered through DescriptionStateCycle");
                            return {};
                        }
                        else if constexpr (std::is_same_v<M, SettleDamageAbsorptionAction>)
                        {
                            auto result = std::format(
                                "將累計吸收值的{}%以{}傷害結算給{}",
                                machine.returnedPct,
                                damageKindLabel(machine.damageKind),
                                selectorLabel(machine.target, compact));
                            if (!machine.clearAfterSettle) result += std::format("{}保留累計值", qualifierSeparator);
                            return result;
                        }
                        else if constexpr (std::is_same_v<M, BorrowEffectRulesAction>)
                        {
                            return std::format(
                                "借用{}的大招規則{}數量{}{}允許類別[{}]{}不含複製與借用遞迴{}傳播借用規則",
                                selectorLabel(machine.sourceUnits, compact),
                                qualifierSeparator,
                                descriptionNumberLabel(machine.sourceCount, style),
                                qualifierSeparator,
                                borrowedRuleFilterLabel(machine.filter),
                                qualifierSeparator,
                                qualifierSeparator);
                        }
                        else if constexpr (std::is_same_v<M, CopyAttackDefinitionAction>)
                        {
                            return std::format(
                                "複製{}的絕招武功攻擊{}數量{}{}條件[{}]{}不傳播大招規則",
                                selectorLabel(machine.sourceUnits, compact),
                                qualifierSeparator,
                                machine.copyCount,
                                qualifierSeparator,
                                copiedMagicFilterLabel(machine.filter),
                                qualifierSeparator);
                        }
                        else if constexpr (std::is_same_v<M, SettleRemainingStatusDamageAction>)
                        {
                            return std::format(
                                "立即結算剩餘{}傷害",
                                battleStatusLabel(machine.status));
                        }
                        else if constexpr (std::is_same_v<M, GenerateClonesAction>)
                            return std::format("生成{}個分身", machine.count);
                        else if constexpr (std::is_same_v<M, PreventDeathAction>)
                            return std::format("首次致命傷害鎖定1生命並獲得{}幀無敵", machine.invincibilityFrames);
                        else if constexpr (std::is_same_v<M, ConfigureRescueRepositionAction>)
                            return std::format(
                                "每場可觸發{}次{}",
                                machine.activations,
                                machine.mode == RescueRepositionMode::Protect ? "保護挪移" : "處決挪移");
                        else static_assert(false, "Unhandled state machine action");
                    },
                    typed);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
            {
                assert(false && "conditional actions must be rendered through DescriptionConditional");
                return {};
            }
            else static_assert(false, "Unhandled effect action");
        },
        action);
}

std::vector<DescriptionQualifier> actionDescriptionQualifiers(
    const EffectActionValue& action)
{
    std::vector<DescriptionQualifier> qualifiers;
    const auto appendTimed = [&](
        int durationFrames,
        EffectStackPolicy stack,
        const std::optional<int>& stackLimit,
        EffectStackScope stackScope,
        bool perStack)
    {
        if (durationFrames > 0)
            qualifiers.emplace_back(DescriptionDurationFramesQualifier{ durationFrames });
        if (stack != EffectStackPolicy::Independent)
            qualifiers.emplace_back(DescriptionStackPolicyQualifier{ stack });
        if (stackLimit)
            qualifiers.emplace_back(DescriptionStackLimitQualifier{ *stackLimit });
        if (perStack)
            qualifiers.emplace_back(DescriptionPerStackQualifier{ true });
        if (stackScope == EffectStackScope::EventSource)
            qualifiers.emplace_back(DescriptionStackScopeQualifier{ stackScope });
    };

    if (const auto* modifier = std::get_if<ModifyAttributeAction>(&action))
    {
        appendTimed(
            modifier->durationFrames,
            modifier->stack,
            modifier->stackLimit,
            modifier->stackScope,
            modifier->perStack);
    }
    else if (const auto* modifier = std::get_if<ModifyDamageAction>(&action))
    {
        appendTimed(
            modifier->durationFrames,
            modifier->stack,
            modifier->stackLimit,
            modifier->stackScope,
            false);
    }
    else if (const auto* status = std::get_if<ApplyStatusAction>(&action))
    {
        appendTimed(
            status->duration ? 0 : status->durationFrames,
            status->stack,
            status->stackLimit,
            EffectStackScope::Shared,
            false);
    }
    return qualifiers;
}

std::vector<DescriptionQualifier> sharedActionDescriptionQualifiers(
    std::span<const EffectAction> actions)
{
    assert(actions.size() > 1);
    const auto* first = std::get_if<ModifyAttributeAction>(&actions.front().value);
    std::optional<int> firstAmount;
    if (first) firstAmount = effectiveConstantEffectNumberValue(first->amount);
    if (!first
        || !firstAmount
        || attributeModifierIsNegative(first->operation, *firstAmount)
        || first->durationFrames <= 0
        || (first->stack != EffectStackPolicy::Independent
            && first->stack != EffectStackPolicy::Refresh)
        || first->stackLimit
        || first->perStack
        || first->stackScope != EffectStackScope::Shared)
    {
        return {};
    }

    for (const auto& action : actions.subspan(1))
    {
        const auto* modifier = std::get_if<ModifyAttributeAction>(&action.value);
        std::optional<int> amount;
        if (modifier) amount = effectiveConstantEffectNumberValue(modifier->amount);
        if (!modifier
            || !amount
            || attributeModifierIsNegative(modifier->operation, *amount)
            || modifier->operation != first->operation
            || modifier->durationFrames != first->durationFrames
            || modifier->stack != first->stack
            || modifier->stackLimit
            || modifier->perStack
            || modifier->stackScope != EffectStackScope::Shared)
        {
            return {};
        }
    }

    // Sibling runtime stack domains remain distinct because actionOrder is part
    // of their keys.  Only the display-unobservable common duration is promoted;
    // caps, policies, scopes, counters, and winner semantics stay per action.
    return { DescriptionDurationFramesQualifier{ first->durationFrames } };
}

bool statusActionsDependOnOrder(
    const EffectActionValue& first,
    const EffectActionValue& second)
{
    const auto statusOf = [](const EffectActionValue& action)
        -> std::optional<BattleStatusKind>
    {
        if (const auto* apply = std::get_if<ApplyStatusAction>(&action)) return apply->status;
        if (const auto* consume = std::get_if<ConsumeStatusAction>(&action)) return consume->status;
        if (const auto* state = std::get_if<StateMachineAction>(&action))
        {
            if (const auto* settle = std::get_if<SettleRemainingStatusDamageAction>(state))
                return settle->status;
        }
        return std::nullopt;
    };
    const auto removalAffects = [](const RemoveStatusAction& removal, BattleStatusKind status)
    {
        return removal.negativeOnly
            || removal.controlOnly
            || std::ranges::contains(removal.statuses, status);
    };

    const auto firstStatus = statusOf(first);
    const auto secondStatus = statusOf(second);
    if (firstStatus && secondStatus && *firstStatus == *secondStatus) return true;
    if (const auto* removal = std::get_if<RemoveStatusAction>(&first))
        if (secondStatus && removalAffects(*removal, *secondStatus)) return true;
    if (const auto* removal = std::get_if<RemoveStatusAction>(&second))
        if (firstStatus && removalAffects(*removal, *firstStatus)) return true;
    return false;
}

bool actionPairDependsOnOrder(
    const EffectActionValue& first,
    const EffectActionValue& second)
{
    if (std::holds_alternative<std::shared_ptr<ConditionalEffectAction>>(first)
        || std::holds_alternative<std::shared_ptr<ConditionalEffectAction>>(second))
        return true;
    if (std::holds_alternative<StateMachineAction>(first)
        || std::holds_alternative<StateMachineAction>(second))
        return true;
    if (std::holds_alternative<DealDamageAction>(first)
        || std::holds_alternative<DealDamageAction>(second))
        return true;
    if (statusActionsDependOnOrder(first, second)) return true;

    if (const auto* lhs = std::get_if<ModifyAttributeAction>(&first))
        if (const auto* rhs = std::get_if<ModifyAttributeAction>(&second))
            return lhs->attribute == rhs->attribute;
    if (const auto* lhs = std::get_if<ModifyDamageAction>(&first))
        if (const auto* rhs = std::get_if<ModifyDamageAction>(&second))
            return lhs->perspective == rhs->perspective
                && lhs->stage == rhs->stage
                && lhs->channel == rhs->channel;
    if (const auto* lhs = std::get_if<ChangeResourceAction>(&first))
        if (const auto* rhs = std::get_if<ChangeResourceAction>(&second))
            return lhs->resource == rhs->resource;
    if (std::holds_alternative<ModifyAttackAction>(first)
        && std::holds_alternative<ModifyAttackAction>(second))
        return true;
    if (std::holds_alternative<ModifyCastAction>(first)
        && std::holds_alternative<ModifyCastAction>(second))
        return true;
    return false;
}

bool actionsDependOnOrder(const std::vector<EffectAction>& actions)
{
    for (std::size_t lhs = 0; lhs < actions.size(); ++lhs)
    {
        for (std::size_t rhs = lhs + 1; rhs < actions.size(); ++rhs)
        {
            if (actionPairDependsOnOrder(actions[lhs].value, actions[rhs].value))
                return true;
        }
    }
    return false;
}

DescriptionClause actionDescriptionClause(const EffectActionValue& action)
{
    assert(!std::holds_alternative<std::shared_ptr<ConditionalEffectAction>>(action));
    DescriptionClause clause;
    clause.subject.id = DescriptionSubjectId::SelectedTargets;
    clause.predicate = static_cast<DescriptionPredicate>(action.index());
    clause.arguments.emplace_back(DescriptionActionArgument{ action });
    clause.qualifiers = actionDescriptionQualifiers(action);
    return clause;
}

EffectDescriptionChild actionListDescriptionNode(
    const std::vector<EffectAction>& actions,
    bool coalesce);

EffectDescriptionChild actionDescriptionNode(const EffectAction& action, bool coalesce)
{
    if (const auto* conditional = std::get_if<std::shared_ptr<ConditionalEffectAction>>(
            &action.value))
    {
        assert(*conditional);
        DescriptionConditional node;
        node.scope = DescriptionConditionalScope::PerTarget;
        for (const auto& condition : (*conditional)->conditions)
            node.conditions.push_back({ DescriptionConditionValue{ condition } });
        node.whenTrue = actionListDescriptionNode((*conditional)->whenTrue, coalesce);
        node.whenFalse = actionListDescriptionNode((*conditional)->whenFalse, coalesce);
        return makeDescriptionNode(std::move(node));
    }

    if (const auto* consume = std::get_if<ConsumeStatusAction>(&action.value);
        consume && consume->whenDepleted)
    {
        auto baseValue = action.value;
        auto& baseConsume = std::get<ConsumeStatusAction>(baseValue);
        const auto depletedStatus = baseConsume.status;
        const auto depletedAction = *baseConsume.whenDepleted;
        baseConsume.whenDepleted.reset();

        DescriptionSequence sequence;
        sequence.kind = DescriptionSequenceKind::Actions;
        sequence.steps.push_back(makeDescriptionNode(
            actionDescriptionClause(baseValue)));

        DescriptionConditional depleted;
        depleted.scope = DescriptionConditionalScope::PerTarget;
        depleted.conditions.push_back({
            DescriptionConditionValue{
                DescriptionStatusDepletedCondition{ depletedStatus },
            },
        });
        depleted.whenTrue = actionDescriptionNode(EffectAction{
            EffectActionValue{ depletedAction },
        }, coalesce);
        sequence.steps.push_back(makeDescriptionNode(std::move(depleted)));
        return makeDescriptionNode(std::move(sequence));
    }

    auto clause = makeDescriptionNode(actionDescriptionClause(action.value));
    const auto* state = std::get_if<StateMachineAction>(&action.value);
    if (!state) return clause;

    DescriptionStateCycle cycle;
    if (std::holds_alternative<RecordMaximumDamageAction>(*state))
    {
        cycle.record = std::move(clause);
        cycle.initialValue = 0;
        return makeDescriptionNode(std::move(cycle));
    }
    if (const auto* consume = std::get_if<ConsumeRecordedMaximumAction>(state))
    {
        cycle.consume = std::move(clause);
        if (consume->clearAfterConsume)
        {
            DescriptionClause reset;
            reset.subject.id = DescriptionSubjectId::RecordedState;
            reset.predicate = DescriptionPredicate::ResetRecordedState;
            reset.arguments.emplace_back(DescriptionResetStateArgument{ consume->slot });
            cycle.reset = makeDescriptionNode(std::move(reset));
        }
        return makeDescriptionNode(std::move(cycle));
    }
    if (const auto* absorption = std::get_if<StartDamageAbsorptionAction>(state))
    {
        DescriptionClause record;
        record.subject.id = DescriptionSubjectId::RecordedState;
        record.predicate = DescriptionPredicate::RecordDamageAbsorption;
        record.arguments.emplace_back(DescriptionDamageAbsorptionRecordArgument{
            absorption->absorbedPct,
            absorption->durationFrames,
        });
        cycle.record = makeDescriptionNode(std::move(record));

        DescriptionClause consume;
        consume.subject.id = DescriptionSubjectId::RecordedState;
        consume.predicate = DescriptionPredicate::SettleDamageAbsorption;
        consume.arguments.emplace_back(DescriptionDamageAbsorptionSettlementArgument{
            absorption->settlementTarget,
            absorption->settlementDamageKind,
            absorption->returnedPct,
            absorption->settleOnSourceDeath,
        });
        cycle.consume = makeDescriptionNode(std::move(consume));

        DescriptionClause reset;
        reset.subject.id = DescriptionSubjectId::RecordedState;
        reset.predicate = DescriptionPredicate::ResetRecordedState;
        reset.arguments.emplace_back(DescriptionResetStateArgument{ absorption->slot });
        cycle.reset = makeDescriptionNode(std::move(reset));
        cycle.initialValue = 0;
        return makeDescriptionNode(std::move(cycle));
    }
    if (const auto* settle = std::get_if<SettleDamageAbsorptionAction>(state))
    {
        cycle.consume = std::move(clause);
        if (settle->clearAfterSettle)
        {
            DescriptionClause reset;
            reset.subject.id = DescriptionSubjectId::RecordedState;
            reset.predicate = DescriptionPredicate::ResetRecordedState;
            reset.arguments.emplace_back(DescriptionResetStateArgument{ settle->slot });
            cycle.reset = makeDescriptionNode(std::move(reset));
        }
        return makeDescriptionNode(std::move(cycle));
    }
    return clause;
}

EffectDescriptionChild actionListDescriptionNode(
    const std::vector<EffectAction>& actions,
    bool coalesce)
{
    if (actions.empty()) return {};
    if (actions.size() == 1) return actionDescriptionNode(actions.front(), coalesce);

    if (actionsDependOnOrder(actions))
    {
        DescriptionSequence sequence;
        sequence.kind = DescriptionSequenceKind::Actions;
        for (const auto& action : actions)
            sequence.steps.push_back(actionDescriptionNode(action, coalesce));
        return makeDescriptionNode(std::move(sequence));
    }

    DescriptionSimultaneous simultaneous;
    if (!coalesce)
    {
        for (const auto& action : actions)
            simultaneous.actions.push_back(actionDescriptionNode(action, false));
        return makeDescriptionNode(std::move(simultaneous));
    }

    for (std::size_t begin = 0; begin < actions.size();)
    {
        std::size_t end = begin + 1;
        std::vector<DescriptionQualifier> shared;
        while (end < actions.size())
        {
            auto candidate = sharedActionDescriptionQualifiers(
                std::span<const EffectAction>(actions).subspan(begin, end - begin + 1));
            if (candidate.empty()) break;
            shared = std::move(candidate);
            ++end;
        }
        if (end - begin > 1)
        {
            DescriptionSimultaneous group;
            group.qualifiers = std::move(shared);
            for (std::size_t index = begin; index < end; ++index)
                group.actions.push_back(actionDescriptionNode(actions[index], true));
            simultaneous.actions.push_back(makeDescriptionNode(std::move(group)));
        }
        else
        {
            simultaneous.actions.push_back(actionDescriptionNode(actions[begin], true));
        }
        begin = end;
    }
    if (simultaneous.actions.size() == 1)
        return std::move(simultaneous.actions.front());
    return makeDescriptionNode(std::move(simultaneous));
}

EffectDescriptionNode buildEffectDescriptionAst(const EffectRule& rule, bool coalesce)
{
    DescriptionSequence root;
    root.kind = DescriptionSequenceKind::Rule;

    DescriptionClause trigger;
    trigger.subject.id = DescriptionSubjectId::RuleEvent;
    trigger.predicate = DescriptionPredicate::Trigger;
    trigger.arguments.emplace_back(DescriptionTriggerArgument{
        rule.event,
        rule.observation,
        rule.castMatch,
    });
    root.steps.push_back(makeDescriptionNode(std::move(trigger)));

    DescriptionConditional guard;
    guard.scope = DescriptionConditionalScope::Rule;
    for (const auto& condition : rule.conditions)
        guard.conditions.push_back({ DescriptionConditionValue{ condition } });
    if (rule.chancePct < 100)
        guard.qualifiers.emplace_back(DescriptionChanceQualifier{ rule.chancePct });
    if (rule.intervalFrames > 0)
        guard.qualifiers.emplace_back(DescriptionIntervalQualifier{ rule.intervalFrames });
    if (rule.everyNthEvent > 0)
        guard.qualifiers.emplace_back(DescriptionEveryNthEventQualifier{ rule.everyNthEvent });
    if (rule.repetitionCount)
        guard.qualifiers.emplace_back(DescriptionRepetitionCountQualifier{ *rule.repetitionCount });
    DescriptionForEach targets;
    targets.targets.selector = rule.selector;
    targets.body = actionListDescriptionNode(rule.actions, coalesce);
    guard.whenTrue = makeDescriptionNode(std::move(targets));
    root.steps.push_back(makeDescriptionNode(std::move(guard)));

    DescriptionClause limits;
    limits.predicate = DescriptionPredicate::RuleLimits;
    if (rule.maxActivations > 0)
        limits.qualifiers.emplace_back(DescriptionMaxActivationsQualifier{ rule.maxActivations });
    if (rule.sharedCooldownFrames > 0)
        limits.qualifiers.emplace_back(DescriptionSharedCooldownQualifier{ rule.sharedCooldownFrames });
    if (rule.activationLimit)
    {
        limits.qualifiers.emplace_back(DescriptionActivationLimitQualifier{
            rule.activationLimit->scope,
            rule.activationLimit->maxEvaluations,
        });
    }
    root.steps.push_back(makeDescriptionNode(std::move(limits)));
    return EffectDescriptionNode{ std::move(root) };
}

struct DescriptionRenderContext
{
    EffectDescriptionStyle style{};
    std::optional<EffectEvent> event;
    std::span<const DescriptionQualifier> suppressedActionQualifiers;
    EffectDescriptionContext presentation{};
    int intervalFrames{};
    bool ordinaryOutgoingAcceptedHit{};
    bool sameComboAllyDeath{};
    bool stackingOutgoingSkillDamage{};
    bool topLevelTriggerHidden{};
};

std::string renderDescriptionNode(
    const EffectDescriptionNode& node,
    DescriptionRenderContext context);

std::string renderRuleQualifier(
    const DescriptionQualifier& qualifier,
    EffectDescriptionStyle style,
    bool leadingSeparator = true)
{
    const bool compact = style == EffectDescriptionStyle::Compact;
    const std::string_view separator = leadingSeparator
        ? compact ? "·" : "；"
        : "";
    return std::visit(
        [style, compact, leadingSeparator, separator](const auto& typed) -> std::string
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, DescriptionChanceQualifier>)
            {
                const std::string_view chanceSeparator = leadingSeparator
                    ? compact ? "·" : "，"
                    : "";
                return compact
                    ? std::format("{}{}%", chanceSeparator, typed.percent)
                    : std::format("{}有{}%機率", chanceSeparator, typed.percent);
            }
            else if constexpr (std::is_same_v<T, DescriptionMaxActivationsQualifier>)
                return compact
                    ? std::format("{}最多{}次", separator, typed.count)
                    : std::format("{}最多啟用{}次", separator, typed.count);
            else if constexpr (std::is_same_v<T, DescriptionSharedCooldownQualifier>)
                return compact
                    ? std::format("{}同來源冷卻{}幀", separator, typed.frames)
                    : std::format("{}同來源共用{}幀冷卻", separator, typed.frames);
            else if constexpr (std::is_same_v<T, DescriptionIntervalQualifier>)
                return compact
                    ? std::format("{}每{}幀", separator, typed.frames)
                    : std::format("{}每{}幀一次", separator, typed.frames);
            else if constexpr (std::is_same_v<T, DescriptionEveryNthEventQualifier>)
                return compact
                    ? std::format("{}每{}次", separator, typed.count)
                    : std::format("{}每{}次符合事件啟用一次", separator, typed.count);
            else if constexpr (std::is_same_v<T, DescriptionRepetitionCountQualifier>)
                return compact
                    ? std::format("{}依序×{}", separator, descriptionNumberLabel(typed.count, style))
                    : std::format("{}依序重複{}次", separator, descriptionNumberLabel(typed.count, style));
            else if constexpr (std::is_same_v<T, DescriptionActivationLimitQualifier>)
            {
                assert(typed.scope == EffectActivationScope::PerCastPerTarget);
                return compact
                    ? std::format("{}每施放每目標{}次", separator, typed.count)
                    : std::format("{}每次施放對同一目標最多判定{}次", separator, typed.count);
            }
            else return {};
        },
        qualifier);
}

std::string renderGuardQualifier(
    const DescriptionQualifier& qualifier,
    EffectDescriptionStyle style,
    bool leadingSeparator)
{
    const bool compact = style == EffectDescriptionStyle::Compact;
    const std::string_view separator = leadingSeparator
        ? compact ? "·" : "；"
        : "";
    if (const auto* interval = std::get_if<DescriptionIntervalQualifier>(&qualifier))
        return compact
            ? std::format("{}每{}幀", separator, interval->frames)
            : std::format("{}每{}幀一次", separator, interval->frames);
    if (const auto* nth = std::get_if<DescriptionEveryNthEventQualifier>(&qualifier))
        return compact
            ? std::format("{}每{}次", separator, nth->count)
            : std::format("{}每{}次符合事件啟用一次", separator, nth->count);
    return renderRuleQualifier(qualifier, style, leadingSeparator);
}

std::string renderSharedActionQualifiers(
    std::span<const DescriptionQualifier> qualifiers,
    bool compact)
{
    std::string result;
    for (const auto& qualifier : qualifiers)
    {
        std::visit(
            [&](const auto& typed)
            {
                using T = std::decay_t<decltype(typed)>;
                if constexpr (std::is_same_v<T, DescriptionDurationFramesQualifier>)
                    result += compact ? std::format("·{}幀", typed.frames) : std::format("，持續{}幀", typed.frames);
                else if constexpr (std::is_same_v<T, DescriptionStackPolicyQualifier>)
                {
                    result += compact ? "·" : "；";
                    result += stackPolicyLabel(typed.policy, compact);
                }
                else if constexpr (std::is_same_v<T, DescriptionStackLimitQualifier>)
                    result += std::format("·最多{}層", typed.count);
                else if constexpr (std::is_same_v<T, DescriptionPerStackQualifier>)
                    result += "·數值按每層計算";
                else if constexpr (std::is_same_v<T, DescriptionStackScopeQualifier>)
                    result += "·各事件來源分別疊加";
            },
            qualifier);
    }
    return result;
}

std::string renderDescriptionClause(
    const DescriptionClause& clause,
    DescriptionRenderContext context)
{
    const bool detailed = context.style == EffectDescriptionStyle::Detailed;
    const bool compact = context.style == EffectDescriptionStyle::Compact;
    if (clause.predicate == DescriptionPredicate::Trigger)
    {
        const auto& trigger = std::get<DescriptionTriggerArgument>(
            clause.arguments.front());
        const bool enclosedEvent = !detailed
            && trigger.castMatch == EffectCastMatch::BoundMagic
            && context.presentation.enclosingDefaultEvent == trigger.event;
        auto result = enclosedEvent
            ? std::string{}
            : !detailed && trigger.event == EffectEvent::FrameAdvanced && context.intervalFrames > 0
            ? std::format("每{}幀", context.intervalFrames)
            : !detailed && trigger.event == EffectEvent::DamageResolved && context.ordinaryOutgoingAcceptedHit
            ? compact ? std::string("命中後") : std::string("每次命中後")
            : !detailed && trigger.event == EffectEvent::HitBeforeDamage && context.stackingOutgoingSkillDamage
            ? compact ? std::string("命中") : std::string("每次命中")
            : !detailed && trigger.event == EffectEvent::AllyDied && context.sameComboAllyDeath
            ? compact ? std::string("同羈絆友軍死亡") : std::string("同羈絆友軍死亡時")
            : ruleEventLabel(trigger.event, context.style);
        if (trigger.observation == EffectObservationScope::OwnerTeamEventSource)
            result += compact ? (result.empty() ? "同隊來源" : "·同隊來源") : "（由效果擁有者同隊事件來源觸發）";
        else if (trigger.observation == EffectObservationScope::EventTarget)
            result += compact ? (result.empty() ? "事件目標" : "·事件目標") : "（由效果擁有者成為事件目標時觸發）";
        if (trigger.castMatch == EffectCastMatch::OwnerAnyCast)
            result += compact ? (result.empty() ? "任意施放" : "·任意施放") : "（效果擁有者任意施放）";
        return result;
    }
    if (clause.predicate == DescriptionPredicate::RuleLimits)
    {
        std::string result;
        for (const auto& qualifier : clause.qualifiers)
            result += renderRuleQualifier(qualifier, context.style);
        return result;
    }
    if (clause.predicate == DescriptionPredicate::ResetRecordedState)
        return compact ? "清空" : "清空記錄";
    if (clause.predicate == DescriptionPredicate::RecordDamageAbsorption)
    {
        const auto& record = std::get<DescriptionDamageAbsorptionRecordArgument>(
            clause.arguments.front());
        return std::format(
            "{}幀內記錄並吸收所受傷害的{}%",
            record.durationFrames,
            record.absorbedPct);
    }
    if (clause.predicate == DescriptionPredicate::SettleDamageAbsorption)
    {
        const auto& settlement = std::get<DescriptionDamageAbsorptionSettlementArgument>(
            clause.arguments.front());
        auto result = std::format(
            "將累計吸收值的{}%以{}傷害結算給{}",
            settlement.returnedPct,
            damageKindLabel(settlement.damageKind),
            selectorLabel(settlement.target, compact));
        if (settlement.settleOnSourceDeath)
            result += compact ? "·來源死亡時立即結算" : "；來源死亡時立即結算";
        return result;
    }

    const auto& action = std::get<DescriptionActionArgument>(
        clause.arguments.front());
    assert(clause.predicate == static_cast<DescriptionPredicate>(action.value.index()));
    return renderDescriptionActionArgument(
        action.value,
        context.style,
        context.suppressedActionQualifiers);
}

std::string renderDescriptionConditions(
    std::span<const DescriptionCondition> conditions,
    DescriptionConditionalScope scope,
    std::optional<EffectEvent> event,
    const DescriptionRenderContext& context)
{
    const bool detailed = context.style == EffectDescriptionStyle::Detailed;
    const bool compact = context.style == EffectDescriptionStyle::Compact;
    std::string result;
    for (const auto& condition : conditions)
    {
        const auto* parsed = std::get_if<EffectCondition>(&condition.value);
        if (parsed
            && !detailed
            && scope == DescriptionConditionalScope::Rule
            && std::holds_alternative<MagicIdEqualsCondition>(*parsed))
        {
            continue;
        }
        if (parsed
            && !detailed
            && scope == DescriptionConditionalScope::Rule
            && event == EffectEvent::UltimateCommitted
            && std::holds_alternative<IsUltimateCondition>(*parsed))
        {
            continue;
        }
        if (parsed && !detailed && scope == DescriptionConditionalScope::Rule)
        {
            if (const auto* accepted = std::get_if<AcceptedHitCondition>(parsed);
                accepted && !accepted->requirePositiveDamage && !accepted->excludeReflected)
            {
                continue;
            }
            if (context.ordinaryOutgoingAcceptedHit
                && std::holds_alternative<DamagePerspectiveCondition>(*parsed))
            {
                continue;
            }
            if (context.sameComboAllyDeath
                && std::holds_alternative<EventTargetBelongsToBoundSourceCondition>(*parsed))
            {
                continue;
            }
        }
        if (!result.empty()) result += "且";
        if (parsed)
        {
            if (!detailed)
            {
                if (const auto* accepted = std::get_if<AcceptedHitCondition>(parsed))
                {
                    if (accepted->requirePositiveDamage) result += "正傷害";
                    if (accepted->requirePositiveDamage && accepted->excludeReflected) result += "且";
                    if (accepted->excludeReflected) result += "非反彈";
                    continue;
                }
                if (std::holds_alternative<EventTargetBelongsToBoundSourceCondition>(*parsed))
                {
                    result += "同羈絆";
                    continue;
                }
            }
            result += conditionLabel(*parsed, compact);
        }
        else
        {
            const auto& depleted = std::get<DescriptionStatusDepletedCondition>(
                condition.value);
            result += std::format("{}最後一層已消耗", battleStatusLabel(depleted.status));
        }
    }
    return result;
}

std::string renderDescriptionNode(
    const EffectDescriptionNode& node,
    DescriptionRenderContext context)
{
    const bool detailed = context.style == EffectDescriptionStyle::Detailed;
    const bool compact = context.style == EffectDescriptionStyle::Compact;
    return std::visit(
        [&](const auto& typed) -> std::string
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, DescriptionClause>)
            {
                return renderDescriptionClause(typed, context);
            }
            else if constexpr (std::is_same_v<T, DescriptionSequence>)
            {
                std::string result;
                if (typed.kind == DescriptionSequenceKind::Rule)
                {
                    if (!typed.steps.empty())
                    {
                        const auto* trigger = std::get_if<DescriptionClause>(
                            &typed.steps.front()->value);
                        if (trigger && trigger->predicate == DescriptionPredicate::Trigger)
                        {
                            const auto& argument = std::get<DescriptionTriggerArgument>(
                                trigger->arguments.front());
                            context.event = argument.event;
                        }
                    }
                    for (const auto& step : typed.steps)
                        if (step) result += renderDescriptionNode(*step, context);
                    return result;
                }
                const std::string_view separator = compact ? "→" : detailed ? "，接著" : "，再";
                for (const auto& step : typed.steps)
                {
                    if (!step) continue;
                    if (!result.empty()) result += separator;
                    result += renderDescriptionNode(*step, context);
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, DescriptionSimultaneous>)
            {
                std::string result;
                auto nested = context;
                nested.suppressedActionQualifiers = typed.qualifiers;
                const std::string_view separator = compact
                    ? typed.qualifiers.empty() ? "·" : "／"
                    : "、";
                for (const auto& action : typed.actions)
                {
                    if (!action) continue;
                    if (!result.empty()) result += separator;
                    result += renderDescriptionNode(*action, nested);
                }
                result += renderSharedActionQualifiers(typed.qualifiers, compact);
                return result;
            }
            else if constexpr (std::is_same_v<T, DescriptionConditional>)
            {
                const auto conditions = renderDescriptionConditions(
                    typed.conditions,
                    typed.scope,
                    context.event,
                    context);
                if (typed.scope == DescriptionConditionalScope::Rule)
                {
                    std::string result;
                    if (!conditions.empty())
                    {
                        if (!context.topLevelTriggerHidden)
                            result += compact ? "·" : "，";
                        result += conditions;
                    }
                    for (const auto& qualifier : typed.qualifiers)
                    {
                        const auto* interval = std::get_if<DescriptionIntervalQualifier>(&qualifier);
                        if (!detailed
                            && context.event == EffectEvent::FrameAdvanced
                            && interval)
                        {
                            continue;
                        }
                        const bool leadingSeparator = !context.topLevelTriggerHidden
                            || !result.empty();
                        result += renderGuardQualifier(
                            qualifier,
                            context.style,
                            leadingSeparator);
                    }
                    if (typed.whenTrue)
                    {
                        const auto body = renderDescriptionNode(*typed.whenTrue, context);
                        const bool bodyHasSeparator = body.starts_with("，")
                            || body.starts_with("·")
                            || body.starts_with("；");
                        if (!result.empty() && !bodyHasSeparator)
                            result += compact ? "·" : "，";
                        result += body;
                    }
                    return result;
                }

                const auto whenTrue = typed.whenTrue
                    ? renderDescriptionNode(*typed.whenTrue, context)
                    : std::string{};
                const auto whenFalse = typed.whenFalse
                    ? renderDescriptionNode(*typed.whenFalse, context)
                    : std::string{};
                if (whenFalse.empty())
                    return compact ? std::format("若{}·{}", conditions, whenTrue) : std::format("若{}，{}", conditions, whenTrue);
                return compact
                    ? std::format("若{}·{}／否則{}", conditions, whenTrue, whenFalse)
                    : std::format("若{}則{}，否則{}", conditions, whenTrue, whenFalse);
            }
            else if constexpr (std::is_same_v<T, DescriptionForEach>)
            {
                const auto target = selectorLabel(typed.targets.selector, compact);
                const auto body = typed.body
                    ? renderDescriptionNode(*typed.body, context)
                    : std::string{};
                if (!detailed && typed.targets.selector.kind == EffectSelectorKind::Self)
                {
                    if (context.topLevelTriggerHidden
                        || context.ordinaryOutgoingAcceptedHit
                        || context.stackingOutgoingSkillDamage)
                    {
                        return body;
                    }
                    return (compact ? "·" : "，") + body;
                }
                if (!detailed
                    && typed.targets.selector.kind == EffectSelectorKind::HitTarget
                    && typed.body)
                {
                    const auto* clause = std::get_if<DescriptionClause>(&typed.body->value);
                    if (clause && clause->predicate == DescriptionPredicate::CreateArea)
                    {
                        const auto& argument = std::get<DescriptionActionArgument>(
                            clause->arguments.front());
                        const auto* area = std::get_if<CreateAreaAction>(&argument.value);
                        if (area && area->anchor == AreaAnchor::HitPosition)
                            return (context.topLevelTriggerHidden
                                ? std::string{}
                                : compact ? std::string("·") : std::string("，")) + body;
                    }
                }
                if (compact)
                    return std::format(
                        "{}{}·{}",
                        context.topLevelTriggerHidden ? "" : "·",
                        target,
                        body);
                return std::format(
                    "{}對{}{}",
                    context.topLevelTriggerHidden ? "" : "，",
                    target,
                    body);
            }
            else if constexpr (std::is_same_v<T, DescriptionStateCycle>)
            {
                std::string result;
                const auto append = [&](const EffectDescriptionChild& phase)
                {
                    if (!phase) return;
                    if (!result.empty()) result += compact ? "→" : detailed ? "，隨後" : "，再";
                    result += renderDescriptionNode(*phase, context);
                };
                append(typed.record);
                append(typed.consume);
                append(typed.reset);
                if (typed.initialValue)
                    result += compact
                        ? std::format("·初始{}", *typed.initialValue)
                        : std::format("；首次記錄值為{}", *typed.initialValue);
                return result;
            }
            else static_assert(false, "Unhandled effect description node");
        },
        node.value);
}

std::string renderEffectDescription(
    const EffectDescriptionNode& ast,
    DescriptionRenderContext context)
{
    return renderDescriptionNode(ast, context);
}

bool eventHasHitPayload(EffectEvent event)
{
    return event == EffectEvent::MainProjectileBeforeDamage
        || event == EffectEvent::HitBeforeDamage;
}

bool eventHasDamagePayload(EffectEvent event)
{
    return event == EffectEvent::DamageResolved;
}

bool eventHasDamageOriginPayload(EffectEvent event)
{
    return eventHasDamagePayload(event)
        || event == EffectEvent::ShieldBroken
        || event == EffectEvent::UnitDied
        || event == EffectEvent::AllyDied;
}

bool eventHasHealPayload(EffectEvent event)
{
    return event == EffectEvent::HealAttempted
        || event == EffectEvent::HealApplied;
}

bool eventHasCastAggregatePayload(EffectEvent event)
{
    return event == EffectEvent::CastContinuation
        || event == EffectEvent::CastSettled;
}

bool eventHasCastProvenance(EffectEvent event)
{
    switch (event)
    {
    case EffectEvent::CastPlanned:
    case EffectEvent::AttackCommitted:
    case EffectEvent::UltimateCommitted:
    case EffectEvent::AttackSpawned:
    case EffectEvent::MainProjectileBeforeDamage:
    case EffectEvent::HitBeforeDamage:
    case EffectEvent::CastContinuation:
    case EffectEvent::CastSettled:
        return true;
    default:
        return false;
    }
}

bool validateSelectorAtEvent(const EffectSelector& selector, EffectEvent event, std::string& error)
{
    if (selector.kind == EffectSelectorKind::HitTarget && !eventHasHitPayload(event) && !eventHasDamagePayload(event))
    {
        error = "命中目標選擇器需要命中或傷害事件";
        return false;
    }
    if (selector.kind == EffectSelectorKind::TransactionTarget
        && !eventHasDamagePayload(event)
        && !eventHasHealPayload(event)
        && event != EffectEvent::ShieldBroken
        && event != EffectEvent::UnitDied
        && event != EffectEvent::AllyDied)
    {
        error = "交易目標選擇器需要交易事件";
        return false;
    }
    if (selector.kind == EffectSelectorKind::OriginalAttackTarget
        && event != EffectEvent::CastPlanned
        && event != EffectEvent::AttackCommitted
        && event != EffectEvent::UltimateCommitted
        && event != EffectEvent::AttackSpawned
        && !eventHasHitPayload(event)
        && !eventHasDamagePayload(event)
        && !eventHasCastAggregatePayload(event))
    {
        error = "原攻擊目標選擇器需要施放、攻擊、命中、傷害或施放聚合事件";
        return false;
    }
    if (selector.requiredTarget)
    {
        EffectSelector required;
        switch (*selector.requiredTarget)
        {
        case EffectRequiredTarget::Self: required.kind = EffectSelectorKind::Self; break;
        case EffectRequiredTarget::SourceUnit: required.kind = EffectSelectorKind::SourceUnit; break;
        case EffectRequiredTarget::TransactionTarget:
            required.kind = EffectSelectorKind::TransactionTarget;
            break;
        case EffectRequiredTarget::HitTarget: required.kind = EffectSelectorKind::HitTarget; break;
        case EffectRequiredTarget::OriginalAttackTarget:
            required.kind = EffectSelectorKind::OriginalAttackTarget;
            break;
        }
        if (!validateSelectorAtEvent(required, event, error))
        {
            error = "必含目標不支援此事件：" + error;
            return false;
        }
    }
    return true;
}

bool validateSelectorSchema(const EffectSelector& selector, std::string& error)
{
    if (selector.count < 0)
    {
        error = "目標數量不可為負數";
        return false;
    }
    if (selector.kind == EffectSelectorKind::UnitsInRadius && selector.radiusTiles <= 0)
    {
        error = "半徑選擇器需要正半徑";
        return false;
    }
    if (selector.kind == EffectSelectorKind::UnitsInSquare
        && (selector.squareSideTiles <= 0 || selector.squareSideTiles % 2 == 0))
    {
        error = "方形選擇器邊長必須是正奇數";
        return false;
    }
    if (selector.kind == EffectSelectorKind::AlliesUsingWeapon && selector.requiredWeaponType < 0)
    {
        error = "指定武器友軍需要非負武器類型";
        return false;
    }
    if (selector.requiredTarget)
    {
        switch (selector.kind)
        {
        case EffectSelectorKind::Self:
        case EffectSelectorKind::SourceUnit:
        case EffectSelectorKind::TransactionTarget:
        case EffectSelectorKind::HitTarget:
        case EffectSelectorKind::OriginalAttackTarget:
        case EffectSelectorKind::HighestMpEnemy:
        case EffectSelectorKind::FarthestEnemy:
            error = "單一目標選擇器不能再指定必含目標";
            return false;
        case EffectSelectorKind::ComboMembers:
        case EffectSelectorKind::AllLivingUnits:
        case EffectSelectorKind::Allies:
        case EffectSelectorKind::Enemies:
        case EffectSelectorKind::LowestHpAllies:
        case EffectSelectorKind::LowestMpAllies:
        case EffectSelectorKind::StrongestEnemies:
        case EffectSelectorKind::NearestEnemies:
        case EffectSelectorKind::UnitsInRadius:
        case EffectSelectorKind::UnitsInSquare:
        case EffectSelectorKind::AlliesUsingWeapon:
            break;
        }
    }
    return true;
}

bool selectorHasSingleResult(const EffectSelector& selector)
{
    switch (selector.kind)
    {
    case EffectSelectorKind::Self:
    case EffectSelectorKind::SourceUnit:
    case EffectSelectorKind::TransactionTarget:
    case EffectSelectorKind::HitTarget:
    case EffectSelectorKind::OriginalAttackTarget:
    case EffectSelectorKind::HighestMpEnemy:
    case EffectSelectorKind::FarthestEnemy:
        return true;
    case EffectSelectorKind::ComboMembers:
    case EffectSelectorKind::AllLivingUnits:
    case EffectSelectorKind::Allies:
    case EffectSelectorKind::Enemies:
    case EffectSelectorKind::LowestHpAllies:
    case EffectSelectorKind::LowestMpAllies:
    case EffectSelectorKind::StrongestEnemies:
    case EffectSelectorKind::NearestEnemies:
    case EffectSelectorKind::UnitsInRadius:
    case EffectSelectorKind::UnitsInSquare:
    case EffectSelectorKind::AlliesUsingWeapon:
        return selector.count == 1;
    }
    assert(false);
    return false;
}

bool validateConditionAtEvent(const EffectCondition& condition, EffectEvent event, std::string& error)
{
    return std::visit(
        [&](const auto& typed) -> bool
        {
            using T = std::decay_t<decltype(typed)>;
            const auto reject = [&](std::string message)
            {
                error = std::move(message);
                return false;
            };
            if constexpr (std::is_same_v<T, SourceHpRatioAtMostCondition>
                || std::is_same_v<T, SourceHpRatioBelowCondition>
                || std::is_same_v<T, TargetHpRatioAtMostCondition>)
            {
                if (typed.percent < 0 || typed.percent > 100) return reject("生命比例條件必須介於 0 與 100");
                if constexpr (std::is_same_v<T, TargetHpRatioAtMostCondition>)
                {
                    if (!eventHasHitPayload(event) && !eventHasDamagePayload(event) && !eventHasHealPayload(event))
                        return reject("目標生命條件需要命中、傷害或治療事件");
                }
            }
            else if constexpr (std::is_same_v<T, IsMainProjectileCondition>
                || std::is_same_v<T, IsRootAttackCondition>
                || std::is_same_v<T, AttackOrdinalEqualsCondition>)
            {
                if (event != EffectEvent::AttackSpawned && !eventHasHitPayload(event) && !eventHasDamagePayload(event))
                    return reject("攻擊來源條件需要攻擊、命中或傷害事件");
                if constexpr (std::is_same_v<T, AttackOrdinalEqualsCondition>)
                    if (typed.ordinal < 0) return reject("攻擊序號不可為負數");
            }
            else if constexpr (std::is_same_v<T, CastDistinctTargetCountAtLeastCondition>)
            {
                if (!eventHasCastAggregatePayload(event)) return reject("不同目標數條件需要施放聚合事件");
                if (typed.count <= 0) return reject("不同目標數門檻必須為正數");
            }
            else if constexpr (std::is_same_v<T, HealKindInCondition>)
            {
                if (!eventHasHealPayload(event)) return reject("治療種類條件需要治療事件");
                if (typed.kinds.empty()) return reject("治療種類條件不可為空");
            }
            else if constexpr (std::is_same_v<T, DamageOriginIsAttackCondition>)
            {
                if (!eventHasDamageOriginPayload(event)) return reject("傷害來源條件需要傷害、破盾或死亡事件");
            }
            else if constexpr (std::is_same_v<T, DamageKilledTargetCondition>)
            {
                if (!eventHasDamagePayload(event)) return reject("擊殺條件需要傷害結算事件");
            }
            else if constexpr (std::is_same_v<T, AcceptedHitCondition>)
            {
                if (!eventHasDamagePayload(event)) return reject("接受命中條件需要傷害結算事件");
            }
            else if constexpr (std::is_same_v<T, EventTargetBelongsToBoundSourceCondition>)
            {
                if (event != EffectEvent::AllyDied && event != EffectEvent::UnitDied)
                    return reject("來源成員條件需要死亡事件");
            }
            else if constexpr (std::is_same_v<T, DamagePerspectiveCondition>)
            {
                if (!eventHasDamagePayload(event)) return reject("傷害方位條件需要傷害結算事件");
            }
            else if constexpr (std::is_same_v<T, DamageKindInCondition>)
            {
                if (!eventHasHitPayload(event) && !eventHasDamagePayload(event)) return reject("傷害種類條件需要命中或傷害事件");
                if (typed.kinds.empty()) return reject("傷害種類條件不可為空");
            }
            else if constexpr (std::is_same_v<T, TargetMpWasFullBeforeCastCondition>)
            {
                if (event != EffectEvent::CastPlanned
                    && event != EffectEvent::AttackCommitted
                    && event != EffectEvent::UltimateCommitted)
                {
                    return reject("施放前滿內條件需要施放規劃或提交事件");
                }
            }
            else if constexpr (std::is_same_v<T, MagicIdEqualsCondition>)
            {
                if (typed.magicId < 0) return reject("武功 ID 不可為負數");
            }
            else if constexpr (std::is_same_v<T, SourceStackAtLeastCondition>)
            {
                if (typed.count <= 0) return reject("狀態層數門檻必須為正數");
            }
            return true;
        },
        condition);
}

bool validateEffectNumberAtEvent(const EffectNumber& number, EffectEvent event, std::string& error)
{
    const auto usesBase = [&](EffectNumberBase base)
    {
        return number.base == base || number.multiplierBase == base;
    };
    const bool usesStatus = usesBase(EffectNumberBase::SourceStatusPotency)
        || usesBase(EffectNumberBase::SourceStatusStacks);
    if (usesStatus != !number.status.empty())
    {
        error = usesStatus
            ? "來源狀態數值基準需要指定狀態"
            : "只有來源狀態數值基準可指定狀態";
        return false;
    }
    const bool usesStoredState = usesBase(EffectNumberBase::StoredStateValue);
    if (usesStoredState != number.stateSlot.has_value())
    {
        error = usesStoredState
            ? "狀態槽值基準需要指定狀態槽"
            : "只有狀態槽值基準可指定狀態槽";
        return false;
    }
    if (number.multiplierBase
        && (number.base == EffectNumberBase::Constant || *number.multiplierBase == EffectNumberBase::Constant))
    {
        error = "乘數基準只能與兩個非固定數值基準組合";
        return false;
    }
    if (number.minimum && number.maximum && *number.minimum > *number.maximum)
    {
        error = "數值最小值不可大於最大值";
        return false;
    }
    const auto needsFinalDamage = number.base == EffectNumberBase::FinalHpDamage
        || number.multiplierBase == EffectNumberBase::FinalHpDamage;
    if (needsFinalDamage && !eventHasDamagePayload(event))
    {
        error = "實際生命傷害公式只能用於傷害結算事件";
        return false;
    }
    const auto needsCurrentShield = number.base == EffectNumberBase::TargetCurrentShield
        || number.multiplierBase == EffectNumberBase::TargetCurrentShield;
    if (needsCurrentShield
        && !eventHasHitPayload(event)
        && !eventHasDamagePayload(event)
        && event != EffectEvent::ShieldBroken)
    {
        error = "目標目前護盾公式需要命中、傷害或破盾事件";
        return false;
    }
    const auto needsCurrentCooldown = number.base == EffectNumberBase::TargetCurrentCooldown
        || number.multiplierBase == EffectNumberBase::TargetCurrentCooldown;
    if (needsCurrentCooldown
        && event != EffectEvent::FrameAdvanced
        && event != EffectEvent::DamageResolved)
    {
        error = "目標目前冷卻公式需要每幀或傷害結算事件";
        return false;
    }
    return true;
}

bool effectNumberCannotBeNegative(const EffectNumber& number)
{
    if (number.maximum && *number.maximum < 0)
    {
        return false;
    }
    if (number.minimum && *number.minimum >= 0)
    {
        return true;
    }

    const bool scaledValueCanBeNegative = number.base != EffectNumberBase::Constant
        && number.percent < 0;
    return !scaledValueCanBeNegative && number.flat >= 0;
}

bool effectNumberMustBePositive(const EffectNumber& number)
{
    if (!effectNumberCannotBeNegative(number)
        || (number.maximum && *number.maximum <= 0))
    {
        return false;
    }
    return number.flat > 0
        || (number.minimum && *number.minimum > 0);
}

template<class Value>
bool isNonEmptyUniqueAllowList(const std::vector<Value>& values)
{
    if (values.empty())
    {
        return false;
    }
    std::set<Value> unique(values.begin(), values.end());
    return unique.size() == values.size();
}

bool validateActionPayload(const EffectAction& action, EffectEvent event, std::string& error)
{
    return std::visit(
        [&](const auto& typed) -> bool
        {
            using T = std::decay_t<decltype(typed)>;
            const auto reject = [&](std::string message)
            {
                error = std::move(message);
                return false;
            };
            if constexpr (std::is_same_v<T, ModifyAttributeAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, error)) return false;
                if (typed.durationFrames < 0) return reject("屬性修正持續幀數不可為負數");
                if (typed.stack == EffectStackPolicy::AddStack && !typed.stackLimit)
                    return reject("增加層數的屬性修正需要層數上限");
                if (typed.stackLimit && *typed.stackLimit <= 0) return reject("屬性修正層數上限必須為正數");
                if (typed.stackLimit && typed.stack != EffectStackPolicy::AddStack)
                    return reject("只有增加層數的屬性修正可指定層數上限");
                if (typed.perStack && typed.stack != EffectStackPolicy::AddStack)
                    return reject("只有增加層數的屬性修正可指定每層計算");
            }
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, error)) return false;
                if (typed.durationFrames < 0) return reject("傷害修正持續幀數不可為負數");
                if (typed.stack == EffectStackPolicy::AddStack && !typed.stackLimit)
                    return reject("增加層數的傷害修正需要層數上限");
                if (typed.stackLimit && *typed.stackLimit <= 0)
                    return reject("傷害修正層數上限必須為正數");
                if (typed.stackLimit && typed.stack != EffectStackPolicy::AddStack)
                    return reject("只有增加層數的傷害修正可指定層數上限");
                if (typed.operation == DamageModifierOperation::IgnoreDefensePercent
                    && (typed.perspective != DamageModifierPerspective::Outgoing
                        || typed.stage != DamageModifierStage::BeforeDefense))
                    return reject("忽略防禦百分比只支援造成方的防禦前階段");
                if (typed.operation == DamageModifierOperation::CapSingleHitAtMaxHpPercent
                    && (typed.perspective != DamageModifierPerspective::Incoming
                        || typed.stage != DamageModifierStage::Final))
                    return reject("單次承傷上限只支援承受方的最終階段");
                if (typed.operation == DamageModifierOperation::ExecuteBelowMaxHpPercent
                    && (typed.perspective != DamageModifierPerspective::Outgoing
                        || typed.stage != DamageModifierStage::Final))
                    return reject("處決門檻只支援造成方的最終階段");
                if (typed.operation == DamageModifierOperation::Multiply
                    && typed.perspective == DamageModifierPerspective::Incoming
                    && typed.stage == DamageModifierStage::BeforeDefense)
                    return reject("承受方的防禦前階段不支援乘算傷害修正");
            }
            else if constexpr (std::is_same_v<T, ChangeResourceAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, error)) return false;
                if (!effectNumberCannotBeNegative(typed.amount))
                    return reject("資源變更數值不可產生負數");
                if ((typed.kind == ResourceChangeKind::Transfer) != typed.transferDestination.has_value())
                    return reject("只有轉移資源需要且必須提供轉移目標");
                if (typed.kind == ResourceChangeKind::RefreshToAtLeast
                    && typed.resource != BattleResource::Shield
                    && typed.resource != BattleResource::StatusShield
                    && typed.resource != BattleResource::StaggerShield
                    && typed.resource != BattleResource::ControlImmunityFrames
                    && typed.resource != BattleResource::InvincibilityFrames)
                    return reject("至少刷新只支援護盾、狀態護盾、僵直護盾、僵直吸收幀數或無敵幀數");
                if (typed.resource != BattleResource::Hp
                    && (typed.healKind != EffectHealKind::Direct
                        || typed.healSourcePolicy != EffectHealSourcePolicy::RequireAlive))
                    return reject("非生命資源不可指定治療種類或來源政策");
                if (typed.transferDestination)
                {
                    if (!validateSelectorSchema(*typed.transferDestination, error)
                        || !validateSelectorAtEvent(*typed.transferDestination, event, error)) return false;
                    if (!selectorHasSingleResult(*typed.transferDestination))
                        return reject("轉移目標選擇器必須只選取一個單位");
                }
            }
            else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
            {
                if (typed.kinds.empty()) return reject("治療交易修正需要治療種類");
                if (typed.operation == HealModifierOperation::MultiplyReceived
                    && (typed.percent < 0 || typed.percent > 100)) return reject("治療乘算百分比必須介於 0 與 100");
            }
            else if constexpr (std::is_same_v<T, ApplyStatusAction>)
            {
                if (!validateEffectNumberAtEvent(typed.potency, event, error)
                    || !validateEffectNumberAtEvent(typed.secondaryPotency, event, error)) return false;
                if (typed.applicationCount)
                {
                    if (!validateEffectNumberAtEvent(*typed.applicationCount, event, error)) return false;
                    if (!effectNumberMustBePositive(*typed.applicationCount))
                        return reject("狀態套用次數必須保證為正數");
                }
                if (typed.duration
                    && (!validateEffectNumberAtEvent(*typed.duration, event, error)
                        || !effectNumberCannotBeNegative(*typed.duration))) return false;
                if (typed.durationFrames < 0)
                    return reject("狀態持續幀數不可為負數");
                if (typed.stacks <= 0) return reject("狀態層數必須為正數");
                if (typed.stack == EffectStackPolicy::AddStack && !typed.stackLimit)
                    return reject("增加層數的狀態需要層數上限");
                if (typed.stackLimit && *typed.stackLimit <= 0) return reject("狀態層數上限必須為正數");
                if (typed.aggregatePotencyWithinEvent
                    && (typed.status != BattleStatusKind::Poison
                        || typed.stack != EffectStackPolicy::KeepStrongest
                        || typed.secondaryPotency.base != EffectNumberBase::Constant
                        || typed.secondaryPotency.flat != 0
                        || !typed.stackLimit
                        || *typed.stackLimit != typed.stacks))
                {
                    return reject("同事件合計強度只支援保留最強的中毒，且必須明確指定相同的正層數與層數上限，不可指定次要強度");
                }
            }
            else if constexpr (std::is_same_v<T, ConsumeStatusAction>)
            {
                if (typed.stacks <= 0) return reject("消耗狀態層數必須為正數");
                if (typed.whenDepleted)
                {
                    if (typed.whenDepleted->applicationCount)
                        return reject("消耗最後一層套用的狀態不可指定套用次數");
                    EffectAction nested{ EffectActionValue{ *typed.whenDepleted } };
                    if (!validateActionPayload(nested, event, error)) return false;
                }
            }
            else if constexpr (std::is_same_v<T, RemoveStatusAction>)
            {
                if (typed.statuses.empty()
                    && !typed.negativeOnly
                    && !typed.controlOnly
                    && !typed.clearCurrentActionStagger)
                    return reject("移除狀態需要狀態列表或篩選條件");
                if (typed.count < 0) return reject("移除狀態數量不可為負數");
            }
            else if constexpr (std::is_same_v<T, DealDamageAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, error)) return false;
                if (typed.transactionCount
                    && (!validateEffectNumberAtEvent(*typed.transactionCount, event, error)
                        || !effectNumberCannotBeNegative(*typed.transactionCount))) return false;
                if (typed.area.kind == DamageAreaKind::Circle && typed.area.radiusTiles <= 0)
                    return reject("圓形傷害範圍需要正半徑");
                if (typed.area.kind == DamageAreaKind::Square
                    && (typed.area.squareSideTiles <= 0 || typed.area.squareSideTiles % 2 == 0))
                    return reject("方形傷害範圍邊長必須是正奇數");
                if (typed.perCast.perTargetLimit < 0) return reject("同目標命中上限不可為負數");
                if (typed.areaProjectiles)
                {
                    const auto& delivery = *typed.areaProjectiles;
                    if (delivery.rangeTiles <= 0)
                        return reject("區域投射物範圍格數必須為正數");
                    if (delivery.maximumTargets < 0)
                        return reject("區域投射物最多目標不可為負數");
                    if (delivery.stunFrames < 0)
                        return reject("區域投射物眩暈幀數不可為負數");
                    if (typed.area.kind != DamageAreaKind::SingleTarget)
                        return reject("區域投射物不可再指定立即傷害範圍");
                    if (typed.perCast.perTargetLimit != 0)
                        return reject("區域投射物不可指定同施放命中上限");
                }
            }
            else if constexpr (std::is_same_v<T, ModifyAttackAction>)
            {
                if (typed.pattern.projectileCount <= 0) return reject("攻擊彈道數量必須為正數");
                if (typed.pattern.intervalFrames < 0) return reject("攻擊間隔幀數不可為負數");
                if (typed.strengthPct < 0 || typed.sameTargetHitLimit < 0) return reject("攻擊倍率與同目標上限不可為負數");
                if (typed.source)
                {
                    if (!validateSelectorSchema(*typed.source, error)
                        || !validateSelectorAtEvent(*typed.source, event, error)) return false;
                    if (!selectorHasSingleResult(*typed.source))
                        return reject("攻擊來源選擇器必須只選取一個單位");
                }
                if (typed.damageOverride && !validateEffectNumberAtEvent(*typed.damageOverride, event, error)) return false;
                if (!std::visit(
                        [&](const auto& behavior) -> bool
                        {
                            using B = std::decay_t<decltype(behavior)>;
                            if constexpr (std::is_same_v<B, std::monostate>)
                            {
                                return true;
                            }
                            else if constexpr (std::is_same_v<B, ProjectileBounceAttackBehavior>)
                            {
                                if (behavior.additionalHits > 0
                                    && behavior.chancePct >= 0
                                    && behavior.chancePct <= 100
                                    && behavior.rangePixels > 0) return true;
                                return reject("彈道彈射需要正追加命中次數、0 至 100 機率與正像素範圍");
                            }
                            else if constexpr (std::is_same_v<B, NearbyTrackingAttackBehavior>)
                            {
                                if (behavior.rangePixels > 0 && behavior.damagePct > 0) return true;
                                return reject("範圍追蹤需要正像素範圍與正傷害倍率");
                            }
                            else if constexpr (std::is_same_v<B, DelayedAlternateAttackBehavior>)
                            {
                                if (behavior.delayFrames > 0
                                    && behavior.damagePct > 0
                                    && behavior.attackerBlockGainChancePct >= 0
                                    && behavior.attackerBlockGainChancePct <= 100) return true;
                                return reject("延遲替代攻擊需要正延遲、正傷害倍率與 0 至 100 格擋機率");
                            }
                            else if constexpr (std::is_same_v<B, ExpandingSpiralAttackBehavior>)
                            {
                                if (behavior.projectileCount > 0 && behavior.bleedStacks > 0) return true;
                                return reject("擴張螺旋需要正彈道數量與正流血層數");
                            }
                        },
                        typed.runtimeBehavior)) return false;
            }
            else if constexpr (std::is_same_v<T, ForceMoveAction>)
            {
                if (typed.distanceTiles < 0 || typed.distancePixels < 0
                    || (typed.distanceTiles > 0) == (typed.distancePixels > 0))
                    return reject("強制移動必須且只能指定一種正距離");
                if (typed.lockFrames <= 0) return reject("強制移動鎖定幀數必須為正數");
            }
            else if constexpr (std::is_same_v<T, CreateAreaAction>)
            {
                if (typed.durationFrames <= 0 || typed.modifiers.empty()) return reject("區域需要正持續時間與非空修正");
                if (typed.shape == AreaShape::Circle && typed.radiusTiles <= 0) return reject("圓形區域需要正半徑");
                if (typed.shape == AreaShape::GridSquare
                    && (typed.squareSideTiles <= 0 || typed.squareSideTiles % 2 == 0))
                    return reject("棋格方形區域邊長必須是正奇數");
                for (const auto& modifier : typed.modifiers)
                    if (!validateEffectNumberAtEvent(modifier.amount, event, error)) return false;
            }
            else if constexpr (std::is_same_v<T, ModifyCastAction>)
            {
                if (!typed.mpCost
                    && !typed.rangeMode
                    && typed.projectileSpeedPct == 0
                    && typed.minimumSelectDistance == 0
                    && typed.additionalProjectiles == 0
                    && typed.mobility == CastMobilityPolicy::Preserve
                    && !typed.autoUltimate
                    && !typed.replacementPattern
                    && !typed.freeAdditionalCast)
                    return reject("修改施放至少需要一項變更");
                if (typed.mpCost && !validateEffectNumberAtEvent(*typed.mpCost, event, error)) return false;
                if (typed.projectileSpeedPct < 0
                    || typed.minimumSelectDistance < 0
                    || typed.additionalProjectiles < 0)
                    return reject("施放彈道速度、最小選擇距離與追加彈道數不可為負數");
            }
            else if constexpr (std::is_same_v<T, StateMachineAction>)
            {
                return std::visit(
                    [&](const auto& machine) -> bool
                    {
                        using M = std::decay_t<decltype(machine)>;
                        if constexpr (std::is_same_v<M, ChangeStateValueAction>)
                        {
                            if (machine.delta == 0) return reject("狀態值增量不可為零");
                            if (machine.minimum && machine.maximum
                                && *machine.minimum > *machine.maximum)
                                return reject("狀態值最小值不可大於最大值");
                        }
                        else if constexpr (std::is_same_v<M, TransferStateValueAction>)
                        {
                            if (machine.sourceSlot == machine.destinationSlot)
                                return reject("狀態值轉移的來源與目標狀態槽不可相同");
                            if ((machine.sourceSlot == EffectStateSlot::CastMaximumHpDamage
                                    || machine.destinationSlot == EffectStateSlot::CastMaximumHpDamage)
                                && !eventHasCastProvenance(event))
                                return reject("本次施放狀態槽需要具有施放識別的事件");
                        }
                        else if constexpr (std::is_same_v<M, ConsumeRecordedMaximumAction>)
                        {
                            if (machine.percent < 0) return reject("消耗記錄百分比不可為負數");
                        }
                        else if constexpr (std::is_same_v<M, StartDamageAbsorptionAction>)
                        {
                            if (machine.absorbedPct < 0 || machine.absorbedPct > 100 || machine.durationFrames <= 0)
                                return reject("傷害吸收需要 0 至 100 百分比與正持續時間");
                            if (machine.returnedPct < 0)
                                return reject("傷害吸收結算百分比不可為負數");
                            if (!validateSelectorSchema(machine.settlementTarget, error)
                                || !validateSelectorAtEvent(machine.settlementTarget, event, error)) return false;
                        }
                        else if constexpr (std::is_same_v<M, SettleDamageAbsorptionAction>)
                        {
                            if (machine.returnedPct < 0) return reject("傷害吸收結算百分比不可為負數");
                            if (!validateSelectorSchema(machine.target, error)
                                || !validateSelectorAtEvent(machine.target, event, error)) return false;
                        }
                        else if constexpr (std::is_same_v<M, BorrowEffectRulesAction>)
                        {
                            if (event != EffectEvent::CastPlanned)
                                return reject("借用效果規則只允許用於施放規劃事件");
                            if (machine.propagation != CastPropagationPolicy::BorrowedUltimateRules)
                                return reject("借用效果規則必須使用借用大招規則傳播政策");
                            if (!validateEffectNumberAtEvent(machine.sourceCount, event, error)
                                || !validateSelectorSchema(machine.sourceUnits, error)
                                || !validateSelectorAtEvent(machine.sourceUnits, event, error)) return false;
                            if (!effectNumberCannotBeNegative(machine.sourceCount))
                                return reject("借用效果規則的來源數量不可產生負數");
                            if (!isNonEmptyUniqueAllowList(
                                    machine.filter.allowedActionCategories))
                                return reject("借用效果規則需要非空且不重複的動作類別 allow-list");
                        }
                        else if constexpr (std::is_same_v<M, CopyAttackDefinitionAction>)
                        {
                            if (event != EffectEvent::UltimateCommitted)
                                return reject("複製攻擊定義只允許用於絕招提交事件");
                            if (machine.propagation != CastPropagationPolicy::SuppressUltimateRules)
                                return reject("複製攻擊定義必須使用不傳播大招規則傳播政策");
                            if (!machine.sourceUnits.excludeOwner)
                                return reject("複製攻擊定義的來源選擇器必須排除效果擁有者");
                            if (machine.copyCount <= 0) return reject("複製攻擊數量必須為正數");
                            if (!validateSelectorSchema(machine.sourceUnits, error)
                                || !validateSelectorAtEvent(machine.sourceUnits, event, error)) return false;
                            if (!isNonEmptyUniqueAllowList(machine.filter.conditions))
                                return reject("複製攻擊定義需要非空且不重複的可選武功條件 allow-list");
                            if (!std::ranges::contains(
                                    machine.filter.conditions,
                                    CopiedMagicCondition::HasUltimateAttackDefinition))
                                return reject("複製攻擊定義必須限制為有絕招攻擊定義的武功");
                            if (!std::ranges::contains(
                                    machine.filter.conditions,
                                    CopiedMagicCondition::ExcludesRecursiveEffects))
                                return reject("複製攻擊定義必須排除複製與借用遞迴");
                        }
                        else if constexpr (std::is_same_v<M, SettleRemainingStatusDamageAction>)
                        {
                            if (machine.status != BattleStatusKind::Poison)
                                return reject("剩餘狀態傷害結算目前只支援中毒");
                        }
                        else if constexpr (std::is_same_v<M, GenerateClonesAction>)
                        {
                            if (event != EffectEvent::BattleInitialized)
                                return reject("生成分身只允許用於戰鬥初始化事件");
                            if (machine.count <= 0) return reject("生成分身數量必須為正數");
                        }
                        else if constexpr (std::is_same_v<M, PreventDeathAction>)
                        {
                            if (event != EffectEvent::BattleInitialized)
                                return reject("死亡庇護只允許用於戰鬥初始化事件");
                            if (machine.invincibilityFrames <= 0)
                                return reject("死亡庇護無敵幀數必須為正數");
                        }
                        else if constexpr (std::is_same_v<M, ConfigureRescueRepositionAction>)
                        {
                            if (event != EffectEvent::BattleInitialized)
                                return reject("挪移次數只允許用於戰鬥初始化事件");
                            if (machine.activations <= 0) return reject("挪移次數必須為正數");
                        }
                        return true;
                    },
                    typed);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
            {
                if (!typed || typed->conditions.empty() || typed->whenTrue.empty())
                    return reject("條件分支需要條件與成立動作");
                for (const auto& condition : typed->conditions)
                    if (!validateConditionAtEvent(condition, event, error)) return false;
                for (const auto& nested : typed->whenTrue)
                    if (!validateActionPayload(nested, event, error)) return false;
                for (const auto& nested : typed->whenFalse)
                    if (!validateActionPayload(nested, event, error)) return false;
            }
            return true;
        },
        action.value);
}

bool isActionAllowedAtEvent(const EffectAction& action, EffectEvent event, std::string& error)
{
    return std::visit(
        [&](const auto& typed) -> bool
        {
            using T = std::decay_t<decltype(typed)>;
            auto reject = [&](std::string message)
            {
                error = std::move(message);
                return false;
            };

            if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
            {
                return event == EffectEvent::HealAttempted
                    || reject("治療交易修正只能用於 HealAttempted");
            }
            else if constexpr (std::is_same_v<T, ModifyCastAction>)
            {
                if (event == EffectEvent::CastPlanned) return true;
                if (event == EffectEvent::CastContinuation
                    && typed.freeAdditionalCast
                    && !typed.mpCost
                    && !typed.rangeMode
                    && typed.projectileSpeedPct == 0
                    && typed.minimumSelectDistance == 0
                    && typed.additionalProjectiles == 0
                    && typed.mobility == CastMobilityPolicy::Preserve
                    && !typed.autoUltimate
                    && !typed.replacementPattern)
                {
                    return true;
                }
                if ((event == EffectEvent::FrameAdvanced || event == EffectEvent::ShieldBroken)
                    && typed.autoUltimate
                    && !typed.mpCost
                    && !typed.rangeMode
                    && typed.projectileSpeedPct == 0
                    && typed.minimumSelectDistance == 0
                    && typed.additionalProjectiles == 0
                    && typed.mobility == CastMobilityPolicy::Preserve
                    && !typed.replacementPattern
                    && !typed.freeAdditionalCast)
                {
                    return true;
                }
                return reject("修改施放只能用於施放規劃；施放延續只允許免費 child cast；每幀與護盾破裂只允許自動絕招");
            }
            else if constexpr (std::is_same_v<T, ModifyAttackAction>)
            {
                return event == EffectEvent::CastPlanned
                    || event == EffectEvent::AttackCommitted
                    || event == EffectEvent::UltimateCommitted
                    || event == EffectEvent::AttackSpawned
                    || event == EffectEvent::MainProjectileBeforeDamage
                    || event == EffectEvent::CastContinuation
                    || reject("修改攻擊不支援此事件");
            }
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
            {
                return event == EffectEvent::BattleInitialized
                    || event == EffectEvent::UltimateCommitted
                    || event == EffectEvent::HitBeforeDamage
                    || event == EffectEvent::MainProjectileBeforeDamage
                    || event == EffectEvent::DamageResolved
                    || reject("傷害修正不支援此事件");
            }
            else if constexpr (std::is_same_v<T, ForceMoveAction>)
            {
                return event == EffectEvent::HitBeforeDamage
                    || event == EffectEvent::MainProjectileBeforeDamage
                    || reject("強制移動只允許命中傷害前事件");
            }
            else if constexpr (std::is_same_v<T, CreateAreaAction>)
            {
                return event == EffectEvent::AttackCommitted
                    || event == EffectEvent::UltimateCommitted
                    || event == EffectEvent::MainProjectileBeforeDamage
                    || event == EffectEvent::HitBeforeDamage
                    || event == EffectEvent::UnitDied
                    || reject("建立區域不支援此事件");
            }
            else if constexpr (std::is_same_v<T, DealDamageAction>)
            {
                return event != EffectEvent::CastPlanned
                    && event != EffectEvent::AttackSpawned
                    && event != EffectEvent::HealAttempted
                    || reject("造成傷害不支援此事件");
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
            {
                if (!typed) return reject("條件動作不可為空");
                for (const auto& nested : typed->whenTrue)
                    if (!isActionAllowedAtEvent(nested, event, error)) return false;
                for (const auto& nested : typed->whenFalse)
                    if (!isActionAllowedAtEvent(nested, event, error)) return false;
                return true;
            }
            else
            {
                return event != EffectEvent::HealAttempted
                    || reject("HealAttempted 只允許治療交易修正");
            }
        },
        action.value);
}

enum class ExactRuntimeActionClass
{
    Ordinary,
    Exact,
    UnsupportedComposition,
};

ExactRuntimeActionClass exactRuntimeActionClass(const EffectAction& action)
{
    return std::visit([&](const auto& typed)
    {
        using T = std::decay_t<decltype(typed)>;
        if constexpr (std::is_same_v<T, ModifyCastAction>)
        {
            const bool exact = typed.rangeMode == CastRangeMode::Ranged
                || typed.projectileSpeedPct > 0
                || typed.minimumSelectDistance > 0
                || typed.additionalProjectiles > 0
                || typed.mobility != CastMobilityPolicy::Preserve;
            if (!exact)
            {
                return ExactRuntimeActionClass::Ordinary;
            }
            const bool hasOrdinaryFields = typed.mpCost.has_value()
                || typed.autoUltimate.has_value()
                || typed.replacementPattern.has_value()
                || typed.freeAdditionalCast
                || typed.propagation != CastPropagationPolicy::SourceRules;
            return hasOrdinaryFields
                ? ExactRuntimeActionClass::UnsupportedComposition
                : ExactRuntimeActionClass::Exact;
        }
        else if constexpr (std::is_same_v<T, ModifyAttackAction>)
        {
            const bool exact = !std::holds_alternative<std::monostate>(typed.runtimeBehavior)
                && !std::holds_alternative<ExpandingSpiralAttackBehavior>(typed.runtimeBehavior);
            if (!exact)
            {
                return ExactRuntimeActionClass::Ordinary;
            }
            const bool hasOrdinaryFields = typed.pattern.kind != AttackPatternKind::Preserve
                || typed.pattern.projectileCount != 1
                || typed.pattern.spreadDegrees != 0
                || typed.pattern.intervalFrames != 0
                || typed.strengthPct != 100
                || typed.through.has_value()
                || typed.tracking.has_value()
                || !typed.mainProjectile
                || typed.sameTargetHitLimit != 0
                || typed.targets != AttackTargetPolicy::Preserve
                || typed.propagation != CastPropagationPolicy::SourceRules
                || typed.addToBaseAttack
                || typed.source.has_value()
                || typed.damageOverride.has_value()
                || typed.damageKind.has_value();
            return hasOrdinaryFields
                ? ExactRuntimeActionClass::UnsupportedComposition
                : ExactRuntimeActionClass::Exact;
        }
        else if constexpr (std::is_same_v<T, ForceMoveAction>)
        {
            return typed.distancePixels > 0
                ? ExactRuntimeActionClass::Exact
                : ExactRuntimeActionClass::Ordinary;
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
        {
            assert(typed);
            const auto containsExact = [&](const auto& actions)
            {
                return std::ranges::any_of(actions, [](const EffectAction& nested)
                {
                    return exactRuntimeActionClass(nested)
                        != ExactRuntimeActionClass::Ordinary;
                });
            };
            return containsExact(typed->whenTrue) || containsExact(typed->whenFalse)
                ? ExactRuntimeActionClass::UnsupportedComposition
                : ExactRuntimeActionClass::Ordinary;
        }
        else
        {
            return ExactRuntimeActionClass::Ordinary;
        }
    }, action.value);
}

bool exactRuntimeActionConsumesRuleChance(const EffectAction& action)
{
    if (const auto* movement = std::get_if<ForceMoveAction>(&action.value))
    {
        return movement->distancePixels > 0;
    }
    const auto* attack = std::get_if<ModifyAttackAction>(&action.value);
    return attack
        && std::holds_alternative<NearbyTrackingAttackBehavior>(
            attack->runtimeBehavior);
}

bool validateBattleInitializedNumber(const EffectNumber& number, std::string& error)
{
    const auto supported = [](EffectNumberBase base)
    {
        switch (base)
        {
        case EffectNumberBase::Constant:
        case EffectNumberBase::SourceStar:
        case EffectNumberBase::SourceAttack:
        case EffectNumberBase::SourceMaxHp:
        case EffectNumberBase::SourceCurrentMpRatio:
        case EffectNumberBase::TargetMaxHp:
        case EffectNumberBase::TargetCurrentHp:
            return true;
        case EffectNumberBase::SourceMissingHpRatio:
        case EffectNumberBase::TargetCurrentShield:
        case EffectNumberBase::TargetCurrentCooldown:
        case EffectNumberBase::FinalHpDamage:
        case EffectNumberBase::AccumulatedStateValue:
        case EffectNumberBase::SourceStatusPotency:
        case EffectNumberBase::SourceStatusStacks:
        case EffectNumberBase::StoredStateValue:
            return false;
        }
        return false;
    };
    if (!supported(number.base)
        || (number.multiplierBase && !supported(*number.multiplierBase)))
    {
        error = "戰鬥初始化數值公式使用了初始化快照未提供的資料";
        return false;
    }
    return true;
}

bool validateBattleInitializedSelector(const EffectSelector& selector, std::string& error)
{
    if (selector.tieBreak != EffectTieBreak::UnitId)
    {
        error = "戰鬥初始化目標選擇必須使用單位 ID 平手規則";
        return false;
    }
    if (selector.requiredMagicId >= 0 || selector.requiredWeaponType >= 0)
    {
        error = "戰鬥初始化快照不提供武功或武器篩選資料";
        return false;
    }
    if ((selector.kind == EffectSelectorKind::Self
            || selector.kind == EffectSelectorKind::SourceUnit)
        && selector.excludeOwner)
    {
        error = "戰鬥初始化的自身或來源單位不可排除效果擁有者";
        return false;
    }
    switch (selector.kind)
    {
    case EffectSelectorKind::Self:
    case EffectSelectorKind::SourceUnit:
    case EffectSelectorKind::ComboMembers:
    case EffectSelectorKind::AllLivingUnits:
    case EffectSelectorKind::Allies:
    case EffectSelectorKind::Enemies:
    case EffectSelectorKind::LowestHpAllies:
    case EffectSelectorKind::LowestMpAllies:
    case EffectSelectorKind::HighestMpEnemy:
    case EffectSelectorKind::StrongestEnemies:
    case EffectSelectorKind::NearestEnemies:
    case EffectSelectorKind::FarthestEnemy:
    case EffectSelectorKind::UnitsInRadius:
    case EffectSelectorKind::UnitsInSquare:
        return true;
    case EffectSelectorKind::TransactionTarget:
    case EffectSelectorKind::HitTarget:
    case EffectSelectorKind::OriginalAttackTarget:
    case EffectSelectorKind::AlliesUsingWeapon:
        error = "戰鬥初始化不支援此目標選擇器";
        return false;
    }
    return false;
}

bool validateBattleInitializedCondition(const EffectCondition& condition, std::string& error)
{
    if (std::holds_alternative<SourceIsLastAliveCondition>(condition)
        || std::holds_alternative<TargetNotInvincibleCondition>(condition))
    {
        return true;
    }
    error = "戰鬥初始化條件使用了初始化事件未提供或固定不變的資料";
    return false;
}

bool initializedCoreAttribute(BattleAttribute attribute)
{
    return attribute == BattleAttribute::MaxHp
        || attribute == BattleAttribute::Attack
        || attribute == BattleAttribute::Defence
        || attribute == BattleAttribute::Speed;
}

bool validateBattleInitializedAction(
    const EffectAction& action,
    const EffectSelector& ruleSelector,
    std::string& error)
{
    const auto reject = [&](std::string message)
    {
        error = std::move(message);
        return false;
    };
    return std::visit([&](const auto& typed) -> bool
    {
        using T = std::decay_t<decltype(typed)>;
        if constexpr (std::is_same_v<T, ModifyAttributeAction>)
        {
            if (!validateBattleInitializedNumber(typed.amount, error)) return false;
            if (initializedCoreAttribute(typed.attribute)
                && (typed.operation != AttributeOperation::FlatAdd
                    && typed.operation != AttributeOperation::PercentAdd))
                return reject("初始化核心屬性只支援固定加算或百分比加算");
            if (initializedCoreAttribute(typed.attribute)
                && (typed.durationFrames != 0
                    || typed.stack != EffectStackPolicy::Independent
                    || typed.stackLimit
                    || typed.perStack
                    || typed.stackScope != EffectStackScope::Shared))
                return reject("初始化核心屬性必須是永久、共用且獨立的修正");
            return true;
        }
        else if constexpr (std::is_same_v<T, ModifyDamageAction>)
        {
            return validateBattleInitializedNumber(typed.amount, error);
        }
        else if constexpr (std::is_same_v<T, ChangeResourceAction>)
        {
            if (!validateBattleInitializedNumber(typed.amount, error)) return false;
            if (typed.kind == ResourceChangeKind::Drain
                || typed.kind == ResourceChangeKind::Transfer)
                return reject("戰鬥初始化不支援奪取或轉移資源");
            if (typed.resource == BattleResource::Hp
                || typed.resource == BattleResource::ActiveCooldown)
                return reject("戰鬥初始化不可變更生命或目前冷卻");
            return true;
        }
        else if constexpr (std::is_same_v<T, ApplyStatusAction>)
        {
            if (!validateBattleInitializedNumber(typed.potency, error)
                || !validateBattleInitializedNumber(typed.secondaryPotency, error))
                return false;
            if (typed.duration
                && !validateBattleInitializedNumber(*typed.duration, error))
                return false;
            return !typed.applicationCount
                || validateBattleInitializedNumber(*typed.applicationCount, error);
        }
        else if constexpr (std::is_same_v<T, StateMachineAction>)
        {
            return std::visit([&](const auto& machine) -> bool
            {
                using M = std::decay_t<decltype(machine)>;
                if constexpr (std::is_same_v<M, GenerateClonesAction>)
                {
                    return ruleSelector.kind == EffectSelectorKind::Self
                        || reject("生成分身的戰鬥初始化規則必須以自身為目標");
                }
                else if constexpr (std::is_same_v<M, PreventDeathAction>
                    || std::is_same_v<M, ConfigureRescueRepositionAction>)
                {
                    return true;
                }
                else
                {
                    return reject("戰鬥初始化不支援此狀態機機制");
                }
            }, typed);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
        {
            assert(typed);
            for (const auto& condition : typed->conditions)
                if (!validateBattleInitializedCondition(condition, error)) return false;
            for (const auto& nested : typed->whenTrue)
                if (!validateBattleInitializedAction(nested, ruleSelector, error)) return false;
            for (const auto& nested : typed->whenFalse)
                if (!validateBattleInitializedAction(nested, ruleSelector, error)) return false;
            return true;
        }
        else
        {
            return reject("戰鬥初始化不支援此動作類型");
        }
    }, action.value);
}

bool validateBattleInitializedRule(const EffectRule& rule, std::string& error)
{
    if (rule.observation != EffectObservationScope::Owner)
    {
        error = "戰鬥初始化只支援效果擁有者觀察";
        return false;
    }
    if (rule.castMatch != EffectCastMatch::BoundMagic)
    {
        error = "戰鬥初始化不提供施放識別";
        return false;
    }
    if (rule.chancePct != 100
        || rule.maxActivations != 0
        || rule.sharedCooldownFrames != 0
        || rule.intervalFrames != 0
        || rule.everyNthEvent != 0
        || rule.repetitionCount
        || rule.activationLimit)
    {
        error = "戰鬥初始化必須省略機率、次數、冷卻、間隔、重複與觸發限制欄位";
        return false;
    }
    if (!validateBattleInitializedSelector(rule.selector, error)) return false;
    for (const auto& condition : rule.conditions)
        if (!validateBattleInitializedCondition(condition, error)) return false;
    for (const auto& action : rule.actions)
        if (!validateBattleInitializedAction(action, rule.selector, error)) return false;
    return true;
}

}  // namespace

std::string effectDescription(
    const EffectRule& rule,
    EffectDescriptionStyle style,
    const EffectDescriptionContext& context)
{
    const bool detailed = style == EffectDescriptionStyle::Detailed;
    const auto ast = buildEffectDescriptionAst(rule, !detailed);
    const bool ordinaryAcceptedHit = std::ranges::any_of(
        rule.conditions,
        [](const EffectCondition& condition)
        {
            const auto* accepted = std::get_if<AcceptedHitCondition>(&condition);
            return accepted
                && !accepted->requirePositiveDamage
                && !accepted->excludeReflected;
        });
    const bool outgoingPerspective = std::ranges::any_of(
        rule.conditions,
        [](const EffectCondition& condition)
        {
            const auto* perspective = std::get_if<DamagePerspectiveCondition>(&condition);
            return perspective && perspective->perspective == DamagePerspective::Dealt;
        });
    const bool sameComboAllyDeath = rule.event == EffectEvent::AllyDied
        && std::ranges::any_of(
            rule.conditions,
            [](const EffectCondition& condition)
            {
                return std::holds_alternative<EventTargetBelongsToBoundSourceCondition>(condition);
            });
    const bool stackingOutgoingSkillDamage = rule.event == EffectEvent::HitBeforeDamage
        && rule.actions.size() == 1
        && std::holds_alternative<ModifyDamageAction>(rule.actions.front().value)
        && usesStackingOutgoingSkillDamagePhrase(
            std::get<ModifyDamageAction>(rule.actions.front().value));
    const bool topLevelEventOmitted = !detailed
        && (rule.event == EffectEvent::BattleInitialized
            || (rule.castMatch == EffectCastMatch::BoundMagic
                && context.enclosingDefaultEvent == rule.event));
    const bool topLevelTriggerHidden = topLevelEventOmitted
        && rule.observation == EffectObservationScope::Owner
        && rule.castMatch == EffectCastMatch::BoundMagic;
    return renderEffectDescription(
        ast,
        DescriptionRenderContext{
            .style = style,
            .presentation = context,
            .intervalFrames = rule.intervalFrames,
            .ordinaryOutgoingAcceptedHit = rule.event == EffectEvent::DamageResolved
                && ordinaryAcceptedHit
                && outgoingPerspective,
            .sameComboAllyDeath = sameComboAllyDeath,
            .stackingOutgoingSkillDamage = stackingOutgoingSkillDamage,
            .topLevelTriggerHidden = topLevelTriggerHidden,
        });
}

bool validateEffectRule(const EffectRule& rule, std::string& error)
{
    error.clear();
    if (rule.castMatch == EffectCastMatch::OwnerAnyCast
        && rule.observation != EffectObservationScope::Owner)
    {
        error = "效果擁有者任意施放匹配只支援效果擁有者觀察";
        return false;
    }
    if (rule.castMatch == EffectCastMatch::OwnerAnyCast
        && !eventHasCastProvenance(rule.event))
    {
        error = "效果擁有者任意施放匹配需要具有施放識別的事件";
        return false;
    }
    if (rule.observation == EffectObservationScope::OwnerTeamEventSource
        && rule.event != EffectEvent::MainProjectileBeforeDamage
        && rule.event != EffectEvent::HitBeforeDamage)
    {
        error = "同隊事件來源觀察目前只支援命中傷害前事件";
        return false;
    }
    if (rule.chancePct < 0 || rule.chancePct > 100)
    {
        error = "觸發機率必須介於 0 與 100";
        return false;
    }
    if (rule.maxActivations < 0)
    {
        error = "啟用次數上限不可為負數";
        return false;
    }
    if (rule.sharedCooldownFrames < 0)
    {
        error = "同來源冷卻幀數不可為負數";
        return false;
    }
    if (rule.intervalFrames < 0)
    {
        error = "間隔幀數不可為負數";
        return false;
    }
    if (rule.intervalFrames > 0 && rule.event != EffectEvent::FrameAdvanced)
    {
        error = "間隔幀數只允許用於每幀事件";
        return false;
    }
    if (rule.everyNthEvent < 0)
    {
        error = "每N次事件不可為負數";
        return false;
    }
    if (rule.everyNthEvent == 1)
    {
        error = "每N次事件為1沒有意義，請省略此欄位";
        return false;
    }
    if (rule.repetitionCount)
    {
        if (!validateEffectNumberAtEvent(*rule.repetitionCount, rule.event, error))
            return false;
        if (!effectNumberMustBePositive(*rule.repetitionCount))
        {
            error = "規則重複次數必須保證為正數";
            return false;
        }
        const auto targetScoped = [](EffectNumberBase base)
        {
            return base == EffectNumberBase::TargetMaxHp
                || base == EffectNumberBase::TargetCurrentHp
                || base == EffectNumberBase::TargetCurrentShield
                || base == EffectNumberBase::TargetCurrentCooldown;
        };
        if (targetScoped(rule.repetitionCount->base)
            || (rule.repetitionCount->multiplierBase
                && targetScoped(*rule.repetitionCount->multiplierBase)))
        {
            error = "規則重複次數必須使用來源或事件範圍的數值，不可依個別目標而異";
            return false;
        }
    }
    if (rule.activationLimit)
    {
        if (rule.activationLimit->maxEvaluations <= 0)
        {
            error = "觸發限制次數必須大於零";
            return false;
        }
        switch (rule.activationLimit->scope)
        {
        case EffectActivationScope::PerCastPerTarget:
            if (!eventHasCastProvenance(rule.event))
            {
                error = "每次施放每個目標的觸發限制需要具有施放識別的事件";
                return false;
            }
            break;
        default:
            error = "未知的效果觸發限制範圍";
            return false;
        }
    }
    if (rule.actions.empty())
    {
        error = "規則至少需要一個動作";
        return false;
    }
    if (!validateSelectorSchema(rule.selector, error)
        || !validateSelectorAtEvent(rule.selector, rule.event, error)) return false;
    for (const auto& condition : rule.conditions)
    {
        if (!validateConditionAtEvent(condition, rule.event, error)) return false;
    }
    if (rule.event == EffectEvent::DamageResolved
        && std::ranges::any_of(rule.conditions, [](const EffectCondition& condition)
            {
                return std::holds_alternative<IsMainProjectileCondition>(condition);
            })
        && !std::ranges::any_of(rule.conditions, [](const EffectCondition& condition)
            {
                return std::holds_alternative<DamageOriginIsAttackCondition>(condition);
            }))
    {
        error = "傷害結算事件使用主彈道條件時也需要「傷害來自招式」條件";
        return false;
    }
    for (const auto& action : rule.actions)
    {
        if (!isActionAllowedAtEvent(action, rule.event, error)
            || !validateActionPayload(action, rule.event, error)) return false;
    }
    if (rule.event == EffectEvent::BattleInitialized
        && !validateBattleInitializedRule(rule, error)) return false;
    const bool hasExactRuntimeAction = std::ranges::any_of(
        rule.actions,
        [](const EffectAction& action)
        {
            return exactRuntimeActionClass(action) == ExactRuntimeActionClass::Exact;
        });
    const bool hasUnsupportedComposition = std::ranges::any_of(
        rule.actions,
        [](const EffectAction& action)
        {
            return exactRuntimeActionClass(action)
                == ExactRuntimeActionClass::UnsupportedComposition;
        });
    const bool hasOrdinaryAction = std::ranges::any_of(
        rule.actions,
        [](const EffectAction& action)
        {
            return exactRuntimeActionClass(action) == ExactRuntimeActionClass::Ordinary;
        });
    if (hasUnsupportedComposition || (hasExactRuntimeAction && hasOrdinaryAction))
    {
        error = "精確 runtime 階段動作不可與一般動作混合，也不可放在條件分支內；請拆成不同效果規則";
        return false;
    }
    const bool exactRuntimeChanceIsConsumed = rule.actions.size() == 1
        && exactRuntimeActionConsumesRuleChance(rule.actions.front());
    if (hasExactRuntimeAction
        && ((rule.chancePct != 100 && !exactRuntimeChanceIsConsumed)
            || rule.everyNthEvent > 0
            || rule.repetitionCount
            || rule.activationLimit
            || rule.maxActivations > 0
            || rule.sharedCooldownFrames > 0))
    {
        error = "精確 runtime 階段規則不可設定其消費端未直接處理的規則機率、每N次事件、觸發限制、觸發次數或同來源冷卻；runtime 消費端不共用一般規則的觸發記帳";
        return false;
    }
    return true;
}

bool ChessBattleEffects::parseEffectRule(
    const YAML::Node& node,
    EffectRule& out,
    EffectRuleId id,
    const std::string& context,
    const ChessDiagnosticSink& diagnostics)
{
    const auto mark = node.Mark();
    auto fail = [&](const std::string& message)
    {
        const auto detail = !mark.is_null()
            ? std::format("「{}」(第{}行，第{}列) {}", context, mark.line + 1, mark.column + 1, message)
            : std::format("「{}」{}", context, message);
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "效果規則", detail);
        return false;
    };

    try
    {
        std::string error;
        PayloadView payload(node, rulePayload);
        if (!payload.validate(error)) return fail(error);
        std::string promotedAction;
        for (const auto& [key, value] : payload.dynamicEntries())
        {
            static_cast<void>(value);
            if (!promotedAction.empty())
                return fail("規則只能提升一個具名動作");
            promotedAction = key;
        }
        if (payload["動作"] && !promotedAction.empty())
            return fail("規則不可同時使用提升動作與「動作」列表");
        if (!payload["動作"] && promotedAction.empty())
            return fail("規則需要一個提升動作或非空「動作」列表");

        out = {};
        out.id = id;
        std::size_t automaticConditionCount{};
        std::string timing;
        if (!requiredString(payload, "時機", timing, error)) return fail(error);
        const auto* timingDescriptor = findTimingDescriptor(timing);
        if (!timingDescriptor) return fail(std::format("未知時機「{}」", timing));
        out.event = timingDescriptor->event;
        out.selector.kind = timingDescriptor->defaultTarget;
        if (timingDescriptor->intervalPolicy == TimingIntervalPolicy::Forbidden
            && payload["間隔幀數"])
        {
            return fail("時機「每幀」禁止「間隔幀數」");
        }
        if (timingDescriptor->intervalPolicy == TimingIntervalPolicy::RequiredPositive)
        {
            if (!payload["間隔幀數"])
                return fail("時機「每隔」需要「間隔幀數」");
            if (!requiredInt(payload, "間隔幀數", out.intervalFrames, error)) return fail(error);
            if (out.intervalFrames <= 0)
                return fail("時機「每隔」的「間隔幀數」必須是正整數");
        }
        if (timingDescriptor->intent != TimingIntent::None)
        {
            out.conditions.push_back(DamagePerspectiveCondition{
                timingDescriptor->intent == TimingIntent::DamageReceived
                    ? DamagePerspective::Received
                    : DamagePerspective::Dealt,
            });
            if (timingDescriptor->intent == TimingIntent::Kill)
                out.conditions.push_back(DamageKilledTargetCondition{});
            else
                out.conditions.push_back(AcceptedHitCondition{});
            automaticConditionCount = out.conditions.size();
        }
        if (const auto observation = payload["觀察範圍"])
        {
            const auto label = observation.as<std::string>();
            const auto parsed = parseLabel<EffectObservationScope>(label, observationScopeEnum);
            if (!parsed) return fail(std::format("未知觀察範圍「{}」", label));
            out.observation = *parsed;
        }
        if (const auto castMatch = payload["施放匹配"])
        {
            const auto label = castMatch.as<std::string>();
            const auto parsed = parseLabel<EffectCastMatch>(label, castMatchEnum);
            if (!parsed) return fail(std::format("未知施放匹配「{}」", label));
            out.castMatch = *parsed;
        }
        if (payload["目標"] && !parseSelectorNode(payload["目標"], out.selector, error)) return fail(error);
        if (const auto conditions = payload["條件"])
        {
            if (!conditions.IsSequence()) return fail("「條件」必須是列表");
            for (std::size_t index = 0; index < conditions.size(); ++index)
            {
                EffectCondition condition;
                if (!parseConditionNode(conditions[index], condition, error))
                    return fail(std::format("條件#{}: {}", index + 1, error));
                if (std::ranges::any_of(
                        out.conditions.begin(),
                        out.conditions.begin() + static_cast<std::ptrdiff_t>(automaticConditionCount),
                        [&](const EffectCondition& automatic)
                        {
                            return automatic.index() == condition.index();
                        }))
                {
                    return fail(std::format("條件#{} 重複「時機」已自動加入的條件", index + 1));
                }
                out.conditions.push_back(std::move(condition));
            }
        }
        if (!optionalInt(payload, "機率", out.chancePct, error)
            || !optionalInt(payload, "次數", out.maxActivations, error)
            || !optionalInt(payload, "同來源冷卻幀數", out.sharedCooldownFrames, error)
            || (timingDescriptor->intervalPolicy != TimingIntervalPolicy::RequiredPositive
                && !optionalInt(payload, "間隔幀數", out.intervalFrames, error))
            || !optionalInt(payload, "每N次事件", out.everyNthEvent, error)) return fail(error);
        if (const auto activationLimit = payload["觸發限制"])
        {
            EffectActivationLimit parsed;
            if (!parseActivationLimitNode(activationLimit, parsed, error)) return fail(error);
            out.activationLimit = parsed;
        }
        if (payload["重複次數"])
        {
            EffectNumber count;
            if (!parseEffectNumberNode(payload["重複次數"], count, error)) return fail(error);
            out.repetitionCount = std::move(count);
        }
        if (!promotedAction.empty())
        {
            if (!parseNamedAction(promotedAction, payload[promotedAction], out.actions, error))
                return fail(error);
        }
        else if (!parseActionList(payload["動作"], out.actions, error)) return fail(error);
        if (std::ranges::any_of(out.actions, [](const EffectAction& action)
            {
                const auto* move = std::get_if<ForceMoveAction>(&action.value);
                return move && move->distancePixels > 0;
            })
            && (out.actions.size() != 1
                || out.everyNthEvent > 0
                || out.activationLimit
                || out.repetitionCount
                || out.maxActivations > 0
                || out.sharedCooldownFrames > 0))
        {
            return fail("像素擊退／拉近屬於精確階段，必須是唯一動作，且不可設定一般規則觸發記帳欄位");
        }
        if (!payload.finish(error)
            || !validateEffectRule(out, error)) return fail(error);
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        return fail(std::format("解析效果規則時發生 YAML 異常: {}", ex.what()));
    }
}

bool ChessBattleEffects::validateAuthoringDescriptorProbes(std::string& error)
{
    error.clear();
    try
    {
        const auto conditionAuthorNode = [](const ConditionDescriptor& descriptor, const YAML::Node& payload)
        {
            if (descriptor.form == ConditionAuthorForm::Scalar)
                return YAML::Node(std::string(descriptor.name));
            YAML::Node author(YAML::NodeType::Map);
            if (descriptor.form == ConditionAuthorForm::SingleParameter)
                author[std::string(descriptor.name)] = payload[std::string(descriptor.singleParameterField)];
            else
                author[std::string(descriptor.name)] = payload;
            return author;
        };
        const auto runPayloadProbe = [&](const PayloadProbeDescriptor& probe, const YAML::Node& payload)
        {
            switch (probe.kind)
            {
            case PayloadProbeKind::EffectNumber:
            {
                EffectNumber value;
                return parseEffectNumberNode(payload, value, error);
            }
            case PayloadProbeKind::Selector:
            {
                EffectSelector selector;
                return parseSelectorNode(payload, selector, error);
            }
            case PayloadProbeKind::Condition:
            {
                const auto* descriptor = findConditionDescriptor(probe.authorName);
                assert(descriptor);
                EffectCondition condition;
                return parseConditionNode(conditionAuthorNode(*descriptor, payload), condition, error);
            }
            case PayloadProbeKind::Action:
            case PayloadProbeKind::Macro:
            {
                std::vector<EffectAction> actions;
                return parseNamedAction(probe.authorName, payload, actions, error);
            }
            case PayloadProbeKind::ActivationLimit:
            {
                EffectActivationLimit limit;
                return parseActivationLimitNode(payload, limit, error);
            }
            case PayloadProbeKind::AttackRuntimeBehavior:
            {
                AttackRuntimeBehavior behavior;
                return parseAttackRuntimeBehavior(payload, behavior, error);
            }
            case PayloadProbeKind::AreaModifier:
            {
                AreaModifier modifier;
                return parseAreaModifierNode(payload, modifier, error);
            }
            case PayloadProbeKind::AreaProjectile:
            {
                auto parent = YAML::Load(std::string(dealDamagePayload.minimalProbe));
                parent["區域投射物"] = payload;
                std::vector<EffectAction> actions;
                return parseNamedAction("造成傷害", parent, actions, error);
            }
            case PayloadProbeKind::AutoUltimate:
            {
                auto parent = YAML::Load(std::string(modifyCastPayload.minimalProbe));
                parent["自動絕招"] = payload;
                std::vector<EffectAction> actions;
                return parseNamedAction("修改施放", parent, actions, error);
            }
            case PayloadProbeKind::AttributePercentage:
            {
                YAML::Node parent(YAML::NodeType::Map);
                parent["百分比"] = payload;
                std::vector<EffectAction> actions;
                return parseNamedAction("屬性加成", parent, actions, error);
            }
            case PayloadProbeKind::Rule:
            {
                ChessDiagnosticCollector diagnostics;
                EffectRule rule;
                if (parseEffectRule(
                        payload,
                        rule,
                        EffectRuleId{ 1 },
                        "descriptor probe",
                        diagnostics.sink())) return true;
                if (!diagnostics.diagnostics().empty())
                    error = diagnostics.diagnostics().back().message;
                return false;
            }
            }
            assert(false);
            return false;
        };

        for (const auto& descriptor : EffectAuthoring::actionDescriptors())
        {
            std::vector<EffectAction> actions;
            if (!parseNamedAction(
                    descriptor.name,
                    YAML::Load(std::string(descriptor.payload->minimalProbe)),
                    actions,
                    error))
            {
                error = std::format("動作 descriptor「{}」probe 失敗: {}", descriptor.name, error);
                return false;
            }
            if (actions.size() != 1 || actions.front().value.index() != descriptor.variantIndex)
            {
                error = std::format("動作 descriptor「{}」dispatch 到錯誤 variant", descriptor.name);
                return false;
            }
        }
        for (const auto& descriptor : EffectAuthoring::conditionDescriptors())
        {
            EffectCondition condition;
            const auto payload = YAML::Load(std::string(descriptor.payload->minimalProbe));
            if (!parseConditionNode(conditionAuthorNode(descriptor, payload), condition, error))
            {
                error = std::format("條件 descriptor「{}」probe 失敗: {}", descriptor.name, error);
                return false;
            }
            if (condition.index() != descriptor.variantIndex)
            {
                error = std::format("條件 descriptor「{}」dispatch 到錯誤 variant", descriptor.name);
                return false;
            }
        }
        for (const auto& descriptor : EffectAuthoring::macroDescriptors())
        {
            std::vector<EffectAction> actions;
            if (!parseNamedAction(
                    descriptor.name,
                    YAML::Load(std::string(descriptor.payload->minimalProbe)),
                    actions,
                    error))
            {
                error = std::format("動作巨集 descriptor「{}」probe 失敗: {}", descriptor.name, error);
                return false;
            }
            if (actions.empty())
            {
                error = std::format("動作巨集 descriptor「{}」probe 未產生動作", descriptor.name);
                return false;
            }
        }
        for (const auto& probe : payloadProbeDescriptors)
        {
            const auto run = [&](const YAML::Node& payload, std::string_view field)
            {
                error.clear();
                if (runPayloadProbe(probe, payload)) return true;
                error = std::format(
                    "payload descriptor「{}」{}probe 失敗: {}",
                    probe.payload->name,
                    field.empty() ? "最小 " : std::format("欄位「{}」", field),
                    error);
                return false;
            };
            if (!run(YAML::Load(std::string(probe.payload->minimalProbe)), {})) return false;
            for (const auto& field : probe.payload->fields)
            {
                const auto context = field.probeContext.empty()
                    ? probe.payload->minimalProbe
                    : field.probeContext;
                auto payload = YAML::Load(std::string(context));
                payload[std::string(field.name)] = YAML::Load(std::string(field.probeValue));
                if (!run(payload, field.name)) return false;
            }
            if (probe.payload->dynamicKeyClass != PayloadDynamicKeyClass::None)
            {
                auto payload = YAML::Load(std::string(probe.payload->minimalProbe));
                payload[std::string(probe.payload->dynamicProbeKey)] =
                    YAML::Load(std::string(probe.payload->dynamicProbeValue));
                if (!run(payload, probe.payload->dynamicProbeKey)) return false;
            }
        }
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("descriptor probe YAML 無效: {}", ex.what());
        return false;
    }
}

namespace
{

bool reportMagicLoadError(
    const YAML::Node& node,
    const std::string& context,
    const std::string& message,
    const ChessDiagnosticSink& diagnostics)
{
    const auto mark = node.Mark();
    if (!mark.is_null())
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "武功效果", std::format("「{}」(第{}行，第{}列) {}", context, mark.line + 1, mark.column + 1, message));
    }
    else
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "武功效果", std::format("「{}」{}", context, message));
    }
    return false;
}

}  // namespace

bool ChessBattleEffects::parseMagicEffects(
    const YAML::Node& root,
    std::vector<ChessMagicEffectDefinition>& out,
    const std::string& context,
    const ChessDiagnosticSink& diagnostics)
{
    out.clear();
    if (!root || !root.IsMap())
    {
        return reportMagicLoadError(root, context, "根節點必須是映射表", diagnostics);
    }

    bool effectsEnabled = true;
    if (const auto enabled = root["啟用"])
    {
        try
        {
            effectsEnabled = enabled.as<bool>();
        }
        catch (const YAML::Exception& ex)
        {
            return reportMagicLoadError(enabled, context, std::format("「啟用」欄位不是有效布林值: {}", ex.what()), diagnostics);
        }
    }

    std::string rootError;
    if (!validateKnownKeys(root, { "啟用", "絕招" }, rootError))
    {
        return reportMagicLoadError(root, context, rootError, diagnostics);
    }
    const auto entries = root["絕招"];
    if (!entries)
    {
        return reportMagicLoadError(root, context, "缺少「絕招」根節點", diagnostics);
    }
    if (!entries.IsSequence())
    {
        return reportMagicLoadError(entries, context, "「絕招」必須是列表", diagnostics);
    }

    std::set<int> seenMagicIds;
    std::vector<ChessMagicEffectDefinition> parsedDefinitions;
    for (std::size_t definitionIndex = 0; definitionIndex < entries.size(); ++definitionIndex)
    {
        const auto entryNode = entries[definitionIndex];
        std::string entryError;
        if (!validateKnownKeys(entryNode, { "武功", "名稱", "效果" }, entryError))
            return reportMagicLoadError(entryNode, context, entryError, diagnostics);

        ChessMagicEffectDefinition definition;
        definition.enabled = effectsEnabled;
        try
        {
            if (!entryNode["武功"] || !entryNode["名稱"])
                return reportMagicLoadError(entryNode, context, "絕招項目需要「武功」與「名稱」", diagnostics);
            definition.magicId = entryNode["武功"].as<int>();
            definition.name = entryNode["名稱"].as<std::string>();
            definition.purpose = "絕招";
        }
        catch (const YAML::Exception& ex)
        {
            return reportMagicLoadError(entryNode, context, std::format("絕招欄位解析失敗: {}", ex.what()), diagnostics);
        }
        if (definition.magicId < 0)
            return reportMagicLoadError(entryNode, context, "「武功」必須是非負整數", diagnostics);
        if (!seenMagicIds.insert(definition.magicId).second)
            return reportMagicLoadError(entryNode, context, std::format("武功 {} 重複定義", definition.magicId), diagnostics);

        const auto rules = entryNode["效果"];
        if (!rules || !rules.IsSequence() || rules.size() == 0)
            return reportMagicLoadError(entryNode, context, std::format("武功 {} 缺少有效「效果」列表", definition.magicId), diagnostics);
        for (std::size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
        {
            EffectRule rule;
            const auto stableId = EffectRuleId{
                static_cast<std::uint64_t>(static_cast<std::uint32_t>(definition.magicId)) << 32
                | static_cast<std::uint64_t>(ruleIndex),
            };
            if (!parseEffectRule(
                    rules[ruleIndex],
                    rule,
                    stableId,
                    std::format("{}:絕招「{}」規則#{}", context, definition.name, ruleIndex + 1),
                    diagnostics))
            {
                return false;
            }
            if (rule.selector.kind == EffectSelectorKind::ComboMembers)
            {
                return reportMagicLoadError(
                    rules[ruleIndex],
                    context,
                    std::format("武功 {} 的規則不可使用羈絆成員選擇器", definition.magicId),
                    diagnostics);
            }
            if (rule.event == EffectEvent::BattleInitialized)
            {
                return reportMagicLoadError(
                    rules[ruleIndex],
                    context,
                    std::format(
                        "武功 {} 的戰鬥初始化規則不會參與初始化流程",
                        definition.magicId),
                    diagnostics);
            }
            definition.rules.push_back(std::move(rule));
        }
        parsedDefinitions.push_back(std::move(definition));
    }
    out = std::move(parsedDefinitions);
    return true;
}

bool ChessBattleEffects::loadMagicEffectsFile(
    const std::string& path,
    std::vector<ChessMagicEffectDefinition>& out,
    const ChessDiagnosticSink& diagnostics)
{
    try
    {
        return parseMagicEffects(YAML::LoadFile(path), out, path, diagnostics);
    }
    catch (const YAML::Exception& ex)
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "武功效果", std::format("讀取「{}」失敗: {}", path, ex.what()));
        out.clear();
        return false;
    }
}

}  // namespace KysChess
