#include "ChessEffectDescriptionInternal.h"
#include "ChessBattleEffectSemantics.h"
#include "DisplayText.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace KysChess
{
namespace EffectDescriptionDetail
{
std::string eventHeading(
    EffectEvent event,
    EffectDescriptionContainerKind kind,
    EffectObservationScope observation)
{
    if (event == EffectEvent::UnitDied)
        return kind == EffectDescriptionContainerKind::Magic
            ? "施法者死亡"
            : "效果持有者死亡";
    if (event == EffectEvent::AllyDied) return "任一友軍死亡";
    if (event == EffectEvent::CastPlanned
        && kind == EffectDescriptionContainerKind::Magic)
        return "準備施放大招";
    if ((event == EffectEvent::AttackCommitted
            || event == EffectEvent::UltimateCommitted)
        && kind == EffectDescriptionContainerKind::Magic)
        return "施放大招";
    if (event == EffectEvent::MainProjectileBeforeDamage) return "主彈命中";
    if (event == EffectEvent::HitBeforeDamage
        && observation == EffectObservationScope::OwnerTeamEventSource)
        return "任一友軍命中";
    const auto label = ruleEventLabel(event, EffectDescriptionStyle::Detailed);
    if (label.ends_with("時")) return label.substr(0, label.size() - std::string_view("時").size());
    return label.empty() ? "戰鬥開始" : label;
}

std::string_view observationScopeDescriptionLabel(EffectObservationScope observation)
{
    switch (observation)
    {
    case EffectObservationScope::Owner:
        return "效果持有者";
    case EffectObservationScope::OwnerTeamEventSource:
        return "效果持有者同隊的事件來源";
    case EffectObservationScope::EventTarget:
        return "事件目標";
    case EffectObservationScope::StatusHolderEventSource:
        return "狀態持有者作為事件來源";
    case EffectObservationScope::StatusHolderEventTarget:
        return "狀態持有者作為事件目標";
    case EffectObservationScope::StatusSourceEventSource:
        return "狀態來源單位作為事件來源";
    case EffectObservationScope::SourceOwnerTeamEventSource:
        return "來源效果擁有者同隊的事件來源";
    }
    std::unreachable();
}

std::string_view observationSubjectDescriptionLabel(EffectObservationScope observation)
{
    switch (observation)
    {
    case EffectObservationScope::Owner:
        return "效果持有者";
    case EffectObservationScope::OwnerTeamEventSource:
        return "造成該次事件的友軍";
    case EffectObservationScope::EventTarget:
        return "成為事件目標的效果持有者";
    case EffectObservationScope::StatusHolderEventSource:
        return "作為事件來源的狀態持有者";
    case EffectObservationScope::StatusHolderEventTarget:
        return "作為事件目標的狀態持有者";
    case EffectObservationScope::StatusSourceEventSource:
        return "作為事件來源的狀態來源單位";
    case EffectObservationScope::SourceOwnerTeamEventSource:
        return "作為事件來源且與來源效果擁有者同隊的單位";
    }
    std::unreachable();
}

std::string selectorRoleLabel(
    const DescriptionSelectorFact& target,
    EffectEvent event,
    bool compact,
    EffectDescriptionContainerKind kind)
{
    switch (target.role)
    {
    case DescriptionTargetRole::EffectOwner:
        return "效果持有者";
    case DescriptionTargetRole::EventSource:
        return compact ? "事件來源" : "造成該次事件的單位";
    case DescriptionTargetRole::EventTarget:
        if (target.selector.kind == EffectSelectorKind::HitTarget)
        {
            return event == EffectEvent::MainProjectileBeforeDamage
                ? compact ? "主彈目標" : "被主彈命中的敵人"
                : compact ? "命中目標" : "被命中的單位";
        }
        return event == EffectEvent::HealAttempted || event == EffectEvent::HealApplied
            ? compact ? "治療目標" : "受到治療的單位"
            : event == EffectEvent::UnitDied || event == EffectEvent::AllyDied
            ? compact ? "死亡單位" : "死亡的單位"
            : compact ? "受影響單位" : "受該次效果影響的單位";
    case DescriptionTargetRole::OriginalAttackTarget:
        return event == EffectEvent::MainProjectileBeforeDamage
            ? compact ? "主彈原目標" : "本次主彈原本選定的目標"
            : (event == EffectEvent::UltimateCommitted
                || (event == EffectEvent::AttackCommitted
                    && kind == EffectDescriptionContainerKind::Magic))
            ? compact ? "絕招目標" : "本次絕招原本選定的目標"
            : compact ? "攻擊目標" : "本次攻擊原本選定的目標";
    case DescriptionTargetRole::SelectedUnits:
        return selectorLabel(target.selector, compact);
    }
    std::unreachable();
}

bool isProjected(
    const DescriptionPlayerProjection& projection,
    EffectDescriptionStyle style)
{
    assert(style != EffectDescriptionStyle::Detailed);
    return style == EffectDescriptionStyle::Full
        ? projection.full
        : projection.compact;
}

std::vector<const EffectDescriptionFact*> projectedDescriptionConditionFacts(
    const EffectDescriptionBlock& block,
    EffectDescriptionStyle style)
{
    std::vector<const EffectDescriptionFact*> result;
    for (const auto& fact : block.conditions)
    {
        if (!isProjected(fact.projection, style)) continue;
        if (std::holds_alternative<DescriptionConditionFact>(fact.value))
            result.push_back(&fact);
    }
    return result;
}

const EffectCondition& descriptionCondition(const EffectDescriptionFact& fact)
{
    return std::get<DescriptionConditionFact>(fact.value).condition;
}

bool allActionsProjected(
    std::span<const DescriptionActionGroup> groups,
    EffectDescriptionStyle style)
{
    for (const auto& group : groups)
    {
        for (const auto& action : group.actions)
        {
            if (!isProjected(action.projection, style)) return false;
            if (const auto* branch = std::get_if<
                    std::shared_ptr<DescriptionBranch>>(&action.value))
            {
                assert(*branch);
                for (const auto& fact : (*branch)->conditions)
                    if (!isProjected(fact.projection, style)) return false;
                if (!allActionsProjected((*branch)->whenTrue, style)
                    || !allActionsProjected((*branch)->whenFalse, style))
                    return false;
            }
        }
    }
    return true;
}

bool playerCoverageProjectionIsComplete(
    const EffectDescriptionBlock& block,
    EffectDescriptionStyle style)
{
    return std::ranges::all_of(block.coverage.fields,
        [&](const DescriptionCoverageEntry& entry)
        {
            if (!entry.playerFact
                || (entry.disposition != DescriptionFieldDisposition::Visible
                    && entry.disposition != DescriptionFieldDisposition::AbsorbedByPhrase))
                return true;
            return isProjected(entry.projection, style);
        });
}

bool specializedProjectionIsComplete(
    const EffectDescriptionBlock& block,
    EffectDescriptionStyle style)
{
    const bool rootsProjected = std::ranges::all_of(block.trigger,
        [&](const EffectDescriptionFact& fact)
        {
            return isProjected(fact.projection, style);
        })
        && std::ranges::all_of(block.targets,
            [&](const EffectDescriptionFact& fact)
            {
                return isProjected(fact.projection, style);
            })
        && std::ranges::all_of(block.conditions,
            [&](const EffectDescriptionFact& fact)
            {
                return isProjected(fact.projection, style);
            })
        && allActionsProjected(block.actions, style);
    return rootsProjected
        && playerCoverageProjectionIsComplete(block, style);
}

std::string fullTriggerPrefix(
    const EffectDescriptionBlock& block,
    EffectDescriptionContainerKind kind,
    const EffectDescriptionPresentationContext& context)
{
    const auto& trigger = descriptionTrigger(block);
    if (trigger.castMatch == EffectCastMatch::BoundMagic
        && context.enclosingDefaultEvent == trigger.event)
        return {};
    auto heading = eventHeading(trigger.event, kind, trigger.observation);
    if (trigger.castMatch == EffectCastMatch::OwnerAnyCast)
        heading += "且效果持有者任意施放";
    return heading + "時，";
}

std::string compactTriggerPrefix(
    const EffectDescriptionBlock& block,
    EffectDescriptionContainerKind kind,
    const EffectDescriptionPresentationContext& context)
{
    const auto& trigger = descriptionTrigger(block);
    if (trigger.castMatch == EffectCastMatch::BoundMagic
        && context.enclosingDefaultEvent == trigger.event)
        return {};
    auto heading = eventHeading(trigger.event, kind, trigger.observation);
    if (trigger.castMatch == EffectCastMatch::OwnerAnyCast)
        heading += "（任意施放）";
    return heading + "：";
}

void appendRenderedRow(
    RenderedEffectDescriptionBlock& block,
    EffectDescriptionRowKind kind,
    std::string text,
    int indent = 0,
    EffectDescriptionSemanticBreak breakBefore = EffectDescriptionSemanticBreak::None)
{
    block.rows.push_back({
        .kind = kind,
        .text = std::move(text),
        .indent = indent,
        .breakBefore = breakBefore,
    });
}

void appendSemanticRow(
    RenderedEffectDescriptionBlock& block,
    EffectDescriptionStyle style,
    std::string text,
    int indent = 0,
    EffectDescriptionSemanticBreak breakBefore = EffectDescriptionSemanticBreak::Block,
    bool heading = false)
{
    const int limit = style == EffectDescriptionStyle::Compact ? 72 : 120;
    const auto kind = heading
        ? EffectDescriptionRowKind::Heading
        : style == EffectDescriptionStyle::Compact
        ? EffectDescriptionRowKind::Summary
        : EffectDescriptionRowKind::Prose;
    assert(!text.empty());
    auto rows = wrapDisplayText(text, limit, false);
    assert(!rows.empty()
        && "單一效果描述語意片段超出列寬，必須在 typed renderer 中拆分");
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        while (rows[index].ends_with("，") || rows[index].ends_with("；"))
            rows[index].resize(rows[index].size() - std::string_view("，").size());
        appendRenderedRow(
            block,
            kind,
            rows[index],
            indent + (index == 0 ? 0 : 1),
            index == 0
                ? breakBefore
                : EffectDescriptionSemanticBreak::Qualifier);
    }
}

void appendSemanticHeadingRow(
    RenderedEffectDescriptionBlock& block,
    EffectDescriptionStyle style,
    std::string text,
    int indent = 0,
    EffectDescriptionSemanticBreak breakBefore = EffectDescriptionSemanticBreak::Block)
{
    appendSemanticRow(
        block,
        style,
        std::move(text),
        indent,
        breakBefore,
        true);
}

std::string descriptionBranchCondition(
    const DescriptionBranch& branch,
    EffectDescriptionStyle style,
    EffectEvent event)
{
    std::string result;
    for (const auto& fact : branch.conditions)
    {
        if (style != EffectDescriptionStyle::Detailed
            && !isProjected(fact.projection, style))
            continue;
        if (!result.empty()) result += "且";
        result += conditionLabel(
            std::get<DescriptionConditionFact>(fact.value).condition,
            style == EffectDescriptionStyle::Compact,
            event);
    }
    assert(!result.empty());
    return result;
}

void renderDetailedStatusApplication(
    RenderedEffectDescriptionBlock& rendered,
    const ApplyStatusAction& status,
    int indent,
    EffectDescriptionSemanticBreak breakBefore)
{
    const auto statusName = battleStatusLabel(status.status);
    std::visit([&](const auto& quantity)
    {
        using Q = std::decay_t<decltype(quantity)>;
        std::string action;
        if constexpr (std::is_same_v<Q, NoStatusQuantity>)
            action = std::format("動作：施加{}", statusName);
        else if constexpr (std::is_same_v<Q, AddStatusLayers>)
            action = std::format("動作：增加{}{}層", statusName, quantity.count);
        else if constexpr (std::is_same_v<Q, AddSharedStatusLayers>)
            action = std::format("動作：施加{}層{}", quantity.count, statusName);
        else if constexpr (std::is_same_v<Q, SetStatusMarks>)
            action = std::format("動作：將{}印記設為{}枚", statusName, quantity.count);
        else if constexpr (std::is_same_v<Q, AddDamageBlockCharges>)
            action = std::format("動作：增加{}{}次抵擋", statusName, quantity.count);
        else if constexpr (std::is_same_v<Q, SetDamageBlockCharges>)
            action = std::format("動作：將{}設為{}次抵擋", statusName, quantity.count);
        else if constexpr (std::is_same_v<Q, SetStatusTriggerCharges>)
            action = std::format("動作：設定{}可觸發{}次", statusName, quantity.count);
        appendRenderedRow(
            rendered,
            EffectDescriptionRowKind::ListItem,
            std::move(action),
            indent,
            breakBefore);

        if constexpr (std::is_same_v<Q, AddStatusLayers>)
            appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
                std::format("此效果提供的{}層數上限：{}層",
                    statusName, quantity.limit), indent);
        else if constexpr (std::is_same_v<Q, AddSharedStatusLayers>)
            appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
                std::format("目標共享的{}總層數上限：{}層",
                    statusName, quantity.targetTotalLimit), indent);
        else if constexpr (std::is_same_v<Q, AddDamageBlockCharges>)
            appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
                std::format("此效果提供的{}可抵擋次數上限：{}次",
                    statusName, quantity.limit), indent);
    }, status.quantity);

    const auto duration = statusDurationLabel(
        status,
        EffectDescriptionStyle::Detailed);
    if (!duration.empty())
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "持續時間：" + duration + "幀", indent);

    const auto reapplication = [&]() -> std::string
    {
        switch (status.reapplication)
        {
        case StatusReapplicationPolicy::Implicit:
            switch (statusCatalogEntry(status.status).reapplication)
            {
            case StatusReapplicationModel::CatalogKeepLongerDuration:
                return "保留較長持續時間";
            case StatusReapplicationModel::CatalogRefreshDuration:
                return "以新的持續時間取代剩餘時間";
            case StatusReapplicationModel::CatalogReplaceSelected:
                if (status.status == BattleStatusKind::SevenStarMark)
                    return "完整取代現有七星狀態包（可以用較少印記取代）";
                return "完整取代現有" + std::string(statusName) + "狀態包";
            case StatusReapplicationModel::Implicit:
            case StatusReapplicationModel::StunDuration:
            case StatusReapplicationModel::PoisonDamage:
            case StatusReapplicationModel::AuthoredRefreshDuration:
                break;
            }
            if (std::holds_alternative<SetStatusMarks>(status.quantity))
                return "完整取代現有七星狀態包（可以用較少印記取代）";
            if (std::holds_alternative<SetStatusTriggerCharges>(status.quantity)
                && statusCatalogEntry(status.status).storage
                    == StatusStorageModel::SelectedDebuffInstance)
                return "完整取代現有狀態包與可觸發次數";
            if (std::holds_alternative<SetDamageBlockCharges>(status.quantity))
                return "重新設定可抵擋次數";
            return {};
        case StatusReapplicationPolicy::ExtendDuration:
        case StatusReapplicationPolicy::KeepLongerDuration:
        case StatusReapplicationPolicy::RefreshDuration:
        case StatusReapplicationPolicy::KeepHigherDamage:
        case StatusReapplicationPolicy::ReplaceExistingPoison:
            return std::string(statusReapplicationPolicyLabel(status.reapplication));
        case StatusReapplicationPolicy::Count:
            break;
        }
        std::unreachable();
    }();
    if (!reapplication.empty())
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "重複套用：" + reapplication, indent);

    if (status.behavior)
    {
        auto rows = renderDetailedStatusBehaviorRows(status);
        assert(!rows.empty());
        for (auto& row : rows)
        {
            appendRenderedRow(
                rendered,
                EffectDescriptionRowKind::Field,
                std::move(row.text),
                indent + row.indent,
                row.breakBefore);
        }
    }
    if (status.poisonSameEventMerge == PoisonSameEventMerge::SumDamagePercent)
    {
        appendRenderedRow(
            rendered,
            EffectDescriptionRowKind::Field,
            "同事件合併：同一效果擁有者在同一事件對同一目標施加相容中毒時，先合計傷害百分比再比較較高傷害",
            indent);
    }
}

void renderDetailedActionGroups(
    RenderedEffectDescriptionBlock& rendered,
    std::span<const DescriptionActionGroup> groups,
    EffectEvent event,
    int indent)
{
    for (const auto& group : groups)
    {
        if (group.actions.size() > 1)
        {
            appendRenderedRow(
                rendered,
                EffectDescriptionRowKind::Field,
                group.sequential ? "動作關係：依序" : "動作關係：同時",
                indent,
                EffectDescriptionSemanticBreak::ActionGroup);
        }
        for (std::size_t index = 0; index < group.actions.size(); ++index)
        {
            const auto& action = group.actions[index];
            const auto actionBreak = group.sequential && index > 0
                ? EffectDescriptionSemanticBreak::Sequence
                : EffectDescriptionSemanticBreak::ActionGroup;
            if (const auto* status = descriptionStatusApplication(action))
            {
                renderDetailedStatusApplication(
                    rendered,
                    *status,
                    indent,
                    actionBreak);
                continue;
            }
            if (const auto* leaf = descriptionEffectAction(action))
            {
                if (const auto* status = std::get_if<ApplyStatusAction>(&leaf->value))
                {
                    renderDetailedStatusApplication(
                        rendered,
                        *status,
                        indent,
                        actionBreak);
                    continue;
                }
                appendRenderedRow(
                    rendered,
                    EffectDescriptionRowKind::ListItem,
                    renderActionDescription(
                        *leaf,
                        EffectDescriptionStyle::Detailed,
                        event,
                        false),
                    indent,
                    actionBreak);
                continue;
            }

            const auto& branch = *std::get<std::shared_ptr<DescriptionBranch>>(
                action.value);
            appendRenderedRow(
                rendered,
                EffectDescriptionRowKind::Field,
                "若" + descriptionBranchCondition(
                    branch, EffectDescriptionStyle::Detailed, event) + "：",
                indent,
                actionBreak);
            renderDetailedActionGroups(
                rendered, branch.whenTrue, event, indent + 1);
            if (!branch.whenFalse.empty())
            {
                appendRenderedRow(
                    rendered,
                    EffectDescriptionRowKind::Field,
                    "否則：",
                    indent,
                    EffectDescriptionSemanticBreak::Branch);
                renderDetailedActionGroups(
                    rendered, branch.whenFalse, event, indent + 1);
            }
        }
    }
}

void renderPlayerActionGroups(
    RenderedEffectDescriptionBlock& rendered,
    std::span<const DescriptionActionGroup> groups,
    EffectEvent event,
    EffectDescriptionStyle style,
    int indent,
    const EffectDescriptionPresentationContext& context)
{
    for (const auto& group : groups)
    {
        std::vector<const DescriptionAction*> projectedActions;
        for (const auto& action : group.actions)
        {
            if (isProjected(action.projection, style))
                projectedActions.push_back(&action);
        }
        if (projectedActions.empty()) continue;
        const bool playerCardCompact = style == EffectDescriptionStyle::Compact
            && context.compactPolicy == EffectDescriptionCompactPolicy::PlayerCard;
        if (style == EffectDescriptionStyle::Compact
            && (playerCardCompact || !group.sequential)
            && projectedActions.size() > 1
            && std::ranges::all_of(projectedActions, [](const DescriptionAction* action)
            {
                return descriptionEffectAction(*action) != nullptr;
            }))
        {
            std::string combined;
            bool singleRowPhrases = true;
            for (const auto* action : projectedActions)
            {
                auto rows = renderPlayerActionDescriptionRows(
                    *descriptionEffectAction(*action),
                    style,
                    event,
                    true,
                    context);
                if (rows.size() != 1)
                {
                    singleRowPhrases = false;
                    break;
                }
                if (!combined.empty())
                    combined += group.sequential ? "；" : "、";
                combined += rows.front().text;
            }
            if (singleRowPhrases
                && (playerCardCompact || displayTextWidth(combined) <= 72))
            {
                appendSemanticRow(
                    rendered,
                    style,
                    std::move(combined),
                    indent,
                    EffectDescriptionSemanticBreak::ActionGroup);
                continue;
            }
        }
        if (projectedActions.size() > 1)
        {
            appendSemanticHeadingRow(
                rendered,
                style,
                group.sequential ? "依序執行：" : "同時發生：",
                indent,
                EffectDescriptionSemanticBreak::ActionGroup);
        }
        for (std::size_t index = 0; index < projectedActions.size(); ++index)
        {
            const auto& action = *projectedActions[index];
            const auto actionBreak = group.sequential && index > 0
                ? EffectDescriptionSemanticBreak::Sequence
                : EffectDescriptionSemanticBreak::ActionGroup;
            if (const auto* leaf = descriptionEffectAction(action))
            {
                auto rows = renderPlayerActionDescriptionRows(
                    *leaf,
                    style,
                    event,
                    true,
                    context);
                assert(!rows.empty());
                for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
                {
                    appendSemanticRow(
                        rendered,
                        style,
                        std::move(rows[rowIndex].text),
                        indent + rows[rowIndex].indent,
                        rowIndex == 0 ? actionBreak : rows[rowIndex].breakBefore);
                }
                continue;
            }

            const auto& branch = *std::get<std::shared_ptr<DescriptionBranch>>(
                action.value);
            appendSemanticRow(
                rendered,
                style,
                style == EffectDescriptionStyle::Compact
                    ? "若" + descriptionBranchCondition(branch, style, event) + "："
                    : "若" + descriptionBranchCondition(branch, style, event)
                        + "，執行以下效果",
                indent,
                actionBreak);
            renderPlayerActionGroups(
                rendered, branch.whenTrue, event, style, indent + 1, context);
            if (!branch.whenFalse.empty())
            {
                appendSemanticRow(
                    rendered,
                    style,
                    style == EffectDescriptionStyle::Compact
                        ? "否則："
                        : "否則執行以下效果",
                    indent,
                    EffectDescriptionSemanticBreak::Branch);
                renderPlayerActionGroups(
                    rendered, branch.whenFalse, event, style, indent + 1, context);
            }
        }
    }
}

void renderDetailedBlock(
    RenderedEffectDescriptionBlock& rendered,
    const EffectDescriptionBlock& block,
    EffectDescriptionContainerKind kind)
{
    const auto& trigger = descriptionTrigger(block);
    const auto& selector = descriptionTarget(block);
    const auto conditions = descriptionConditions(block);
    appendRenderedRow(
        rendered,
        EffectDescriptionRowKind::Field,
        std::format("主詞：{}",
            observationSubjectDescriptionLabel(trigger.observation)));
    appendRenderedRow(
        rendered,
        EffectDescriptionRowKind::Field,
        std::format("觀察範圍：{}",
            observationScopeDescriptionLabel(trigger.observation)));
    appendRenderedRow(
        rendered,
        EffectDescriptionRowKind::Field,
        std::format("施放匹配：{}",
            trigger.castMatch == EffectCastMatch::BoundMagic ? "本容器綁定武功" : "效果持有者任意施放"));
    appendRenderedRow(
        rendered,
        EffectDescriptionRowKind::Field,
        "對象：" + selectorRoleLabel(selector, trigger.event, false, kind));
    for (const auto* condition : conditions)
        appendRenderedRow(
            rendered,
            EffectDescriptionRowKind::Field,
            "條件：" + conditionLabel(*condition, false, trigger.event));
    bool renderedStructuredActions = false;
    if (block.archetype == DescriptionArchetype::BorrowRules)
    {
        const auto* borrowed = borrowRulesAction(block);
        assert(borrowed);
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "候選：" + selectorRoleLabel(
                resolveDescriptionSelector(borrowed->sourceUnits),
                trigger.event,
                false,
                kind));
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "選擇數量：" + descriptionNumberLabel(
                borrowed->sourceCount, EffectDescriptionStyle::Detailed));
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            std::format("同順位：{}",
                borrowed->sourceUnits.tieBreak == EffectTieBreak::BattleRandom
                    ? "隨機決定"
                    : "依單位識別順序"));
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "動作：借用被選單位的大招規則");
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "允許類別：" + borrowedRuleFilterLabel(borrowed->filter));
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "遞迴限制：不借用複製或借用規則");
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "傳播政策：借用大招規則");
        renderedStructuredActions = true;
    }
    else if (block.archetype == DescriptionArchetype::CopyAttack)
    {
        const auto* copied = copyAttackAction(block);
        assert(copied);
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "候選：" + selectorRoleLabel(
                resolveDescriptionSelector(copied->sourceUnits),
                trigger.event,
                false,
                kind));
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "條件：" + copiedMagicFilterLabel(copied->filter));
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            std::format("選擇：{}名；同順位{}",
                copied->copyCount,
                copied->sourceUnits.tieBreak == EffectTieBreak::BattleRandom
                    ? "隨機決定"
                    : "依單位識別順序"));
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "動作：複製被選單位的絕招武功攻擊");
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "傳播：不複製該單位的大招規則");
        renderedStructuredActions = true;
    }
    else if (block.archetype == DescriptionArchetype::ConditionalAttack)
    {
        assert(block.actions.size() == 1);
        assert(block.actions.front().actions.size() == 1);
        const auto& branch = *std::get<std::shared_ptr<DescriptionBranch>>(
            block.actions.front().actions.front().value);
        const auto& whenTrue = std::get<ModifyAttackAction>(
            descriptionEffectAction(branch.whenTrue.front().actions.front())->value);
        const auto& whenFalse = std::get<ModifyAttackAction>(
            descriptionEffectAction(branch.whenFalse.front().actions.front())->value);
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "若另一名使用此武功的友軍存活：");
        appendRenderedRow(rendered, EffectDescriptionRowKind::ListItem,
            std::format("出手者：該名友軍；攻擊：沿用主彈樣式，{}枚，造成{}%傷害",
                whenTrue.pattern.projectileCount,
                whenTrue.strengthPct),
            1,
            EffectDescriptionSemanticBreak::Branch);
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "否則：");
        appendRenderedRow(rendered, EffectDescriptionRowKind::ListItem,
            std::format("出手者：本次施法者；攻擊：沿用副彈樣式，{}枚，造成{}%傷害",
                whenFalse.pattern.projectileCount,
                whenFalse.strengthPct),
            1,
            EffectDescriptionSemanticBreak::Branch);
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "共同限制：攻擊相同目標，且不觸發出手者的大招效果");
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "結算位置：加入本次基礎攻擊序列");
        renderedStructuredActions = true;
    }
    if (!renderedStructuredActions)
    {
        renderDetailedActionGroups(rendered, block.actions, trigger.event, 1);
    }
    const auto& qualifiers = ruleQualifiers(block);
    appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
        std::format("機率：{}%", qualifiers.chancePct));
    appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
        std::format("啟用上限：{}", qualifiers.maxActivations == 0 ? "無" : std::to_string(qualifiers.maxActivations)));
    appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
        std::format("共用冷卻：{}幀", qualifiers.sharedCooldownFrames));
    appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
        std::format("週期間隔：{}幀", qualifiers.intervalFrames));
    appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
        std::format("每N次事件：{}", qualifiers.everyNthEvent));
    if (qualifiers.repetitionCount)
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            "重複次數：" + descriptionNumberLabel(
                *qualifiers.repetitionCount,
                EffectDescriptionStyle::Detailed));
    if (qualifiers.activationLimit)
        appendRenderedRow(rendered, EffectDescriptionRowKind::Field,
            std::format("每次施放每目標判定上限：{}",
                qualifiers.activationLimit->maxEvaluations));
    appendRenderedRow(
        rendered,
        EffectDescriptionRowKind::Field,
        std::format("來源：容器規則 {}，識別 {}",
            block.sourceRuleOrder + 1,
            block.sourceRuleId.value));
    const auto dispositionLabel = [](DescriptionFieldDisposition disposition)
    {
        switch (disposition)
        {
        case DescriptionFieldDisposition::Visible: return "可見";
        case DescriptionFieldDisposition::AbsorbedByPhrase: return "由詞彙吸收";
        case DescriptionFieldDisposition::SchemaDefault: return "schema 預設";
        case DescriptionFieldDisposition::DeterminismOnly: return "僅決定性";
        case DescriptionFieldDisposition::SafetyInvariant: return "安全 invariant";
        }
        std::unreachable();
    };
    for (const auto& entry : block.coverage.fields)
    {
        appendRenderedRow(
            rendered,
            EffectDescriptionRowKind::Field,
            std::format("稽核 {} = {} [{}]",
                entry.source.path,
                entry.auditValue,
                dispositionLabel(entry.disposition)),
            1,
            EffectDescriptionSemanticBreak::Qualifier);
    }
}

bool renderBorrowRulesBlock(
    RenderedEffectDescriptionBlock& rendered,
    const EffectDescriptionBlock& block,
    EffectDescriptionStyle style,
    EffectDescriptionContainerKind kind,
    const EffectDescriptionPresentationContext& context)
{
    if (!matchesBorrowRulesArchetype(block)) return false;
    const auto* action = borrowRulesAction(block);
    assert(action);
    const auto count = action->sourceCount.minimum && action->sourceCount.maximum
        ? std::format("{}～{}", *action->sourceCount.minimum, *action->sourceCount.maximum)
        : descriptionNumberLabel(action->sourceCount, style);
    if (style == EffectDescriptionStyle::Full)
    {
        appendSemanticRow(rendered, style,
            fullTriggerPrefix(block, kind, context)
                + std::format("依星級隨機借用{}名敵人的大招效果；不會遞迴借用或複製", count));
    }
    else
    {
        appendSemanticRow(rendered, style,
            compactTriggerPrefix(block, kind, context)
                + std::format("依星級隨機借用{}名敵人的大招效果", count));
    }
    return true;
}

bool renderCopyAttackBlock(
    RenderedEffectDescriptionBlock& rendered,
    const EffectDescriptionBlock& block,
    EffectDescriptionStyle style,
    EffectDescriptionContainerKind kind,
    const EffectDescriptionPresentationContext& context)
{
    if (!matchesCopyAttackArchetype(block)) return false;
    if (style == EffectDescriptionStyle::Full)
        appendSemanticRow(rendered, style,
            fullTriggerPrefix(block, kind, context)
                + "隨機複製另一名存活單位的絕招攻擊，但不複製其大招效果");
    else
        appendSemanticRow(rendered, style,
            compactTriggerPrefix(block, kind, context)
                + "隨機複製另一名存活單位的絕招攻擊，不複製大招效果");
    return true;
}

bool renderConditionalAttackBlock(
    RenderedEffectDescriptionBlock& rendered,
    const EffectDescriptionBlock& block,
    EffectDescriptionStyle style)
{
    if (!matchesConditionalAttackArchetype(block)) return false;
    if (style == EffectDescriptionStyle::Full)
    {
        appendSemanticRow(rendered, style,
            "若另一名同武功友軍存活，該友軍會對本次絕招目標追加一枚100%傷害主彈",
            0,
            EffectDescriptionSemanticBreak::Branch);
        appendSemanticRow(rendered, style,
            "否則由施法者追加一枚50%傷害副彈",
            1,
            EffectDescriptionSemanticBreak::Branch);
        appendSemanticRow(rendered, style,
            "追加攻擊不觸發大招效果",
            1,
            EffectDescriptionSemanticBreak::Qualifier);
    }
    else
    {
        appendSemanticRow(rendered, style,
            "同武功友軍存活：由該友軍對絕招目標追加100%主彈");
        appendSemanticRow(rendered, style,
            "否則：由施法者追加50%副彈（不觸發大招效果）",
            1,
            EffectDescriptionSemanticBreak::Branch);
    }
    return true;
}

bool renderStatusLifecycleBlock(
    RenderedEffectDescriptionBlock& rendered,
    const EffectDescriptionBlock& block,
    EffectDescriptionStyle style,
    EffectDescriptionContainerKind kind,
    const EffectDescriptionPresentationContext& context)
{
    if (!matchesStatusLifecycleArchetype(block)) return false;
    const auto* applied = applyStatusAction(block);
    if (!applied || !applied->behavior) return false;
    if (!statusBehaviorHasExactShape(*applied, { 2 })) return false;
    const auto* marks = std::get_if<SetStatusMarks>(&applied->quantity);
    const auto* damage = findStatusBehaviorAction<ModifyDamageAction>(
        *applied,
        [](const ModifyDamageAction& modifier)
        {
            return modifier.operation == DamageModifierOperation::IgnoreDefensePercent;
        });
    const auto* consumed = findStatusBehaviorAction<ConsumeThisStatusAction>(*applied);
    if (!marks || !damage || !consumed || !consumed->whenDepleted) return false;
    const auto ignored = effectiveConstantEffectNumberValue(damage->amount);
    if (!ignored) return false;
    const auto status = battleStatusLabel(applied->status);
    const auto depleted = battleStatusLabel(consumed->whenDepleted->status);
    const auto depletedDuration = statusDurationLabel(
        *consumed->whenDepleted,
        style);
    if (style == EffectDescriptionStyle::Full)
    {
        auto application = fullTriggerPrefix(block, kind, context)
            + std::format("將該敵人的{}印記設為{}枚", status, marks->count);
        const auto duration = statusDurationLabel(
            *applied,
            EffectDescriptionStyle::Full);
        if (!duration.empty())
            application += std::format("，持續{}幀", duration);
        application += "；再次施加會完整取代現有七星，即使新印記較少亦然";
        appendSemanticRow(rendered, style, std::move(application));
        appendSemanticRow(rendered, style,
            std::format("任一友軍以招式命中帶有{}印記的敵人時，該次招式忽略{}%防禦並消耗{}枚印記",
                status,
                *ignored,
                consumed->quantity),
            0,
            EffectDescriptionSemanticBreak::Block);
            appendSemanticRow(rendered, style,
                std::format("印記耗盡時，使該敵人{}{}幀",
                depleted,
                depletedDuration),
            1,
            EffectDescriptionSemanticBreak::Branch);
    }
    else
    {
        auto application = compactTriggerPrefix(block, kind, context)
            + std::format("{}印記設為{}枚", status, marks->count);
        const auto duration = statusDurationLabel(
            *applied,
            EffectDescriptionStyle::Compact);
        if (!duration.empty())
            application += std::format("（{}幀）", duration);
        application += "；再施加取代現有七星";
        appendSemanticRow(rendered, style, std::move(application));
        appendSemanticRow(rendered, style,
            std::format("友軍招式命中{}目標：破防{}%、消耗{}枚",
                status,
                *ignored,
                consumed->quantity),
            0,
            EffectDescriptionSemanticBreak::Block);
            appendSemanticRow(rendered, style,
                std::format("耗盡時{}{}幀",
                depleted,
                depletedDuration),
            1,
            EffectDescriptionSemanticBreak::Branch);
    }
    return true;
}

bool renderStackExplosionBlock(
    RenderedEffectDescriptionBlock& rendered,
    const EffectDescriptionBlock& block,
    EffectDescriptionStyle style,
    EffectDescriptionContainerKind kind,
    const EffectDescriptionPresentationContext& context)
{
    if (!matchesStackExplosionArchetype(block)) return false;
    const auto* applied = applyStatusAction(block);
    if (!applied || !applied->behavior) return false;
    if (!statusBehaviorHasExactShape(*applied, { 2 })) return false;
    const auto* layers = std::get_if<AddStatusLayers>(&applied->quantity);
    const auto* explosion = findStatusBehaviorAction<DealDamageAction>(*applied);
    const auto* poison = findStatusBehaviorAction<ApplyStatusAction>(
        *applied,
        [](const ApplyStatusAction& nested)
        {
            return nested.status == BattleStatusKind::Poison;
        });
    if (!layers || !explosion || !poison || !poison->behavior
        || !statusBehaviorHasExactShape(*poison, { 2 })) return false;
    const auto* poisonDamage = findStatusBehaviorAction<DealDamageAction>(*poison);
    if (!poisonDamage) return false;
    const auto behaviorRule = std::ranges::find_if(
        applied->behavior->rules,
        [](const EffectRule& rule)
        {
            return std::ranges::any_of(rule.actions, [](const EffectAction& action)
            {
                return std::holds_alternative<DealDamageAction>(action.value);
            });
        });
    if (behaviorRule == applied->behavior->rules.end()) return false;

    auto explosionPerLayer = explosion->amount;
    explosionPerLayer.statusScale = StatusNumberScale::Once;
    const auto explosionValue = descriptionNumberLabel(explosionPerLayer, style);
    auto poisonPerTrigger = poisonDamage->amount;
    poisonPerTrigger.statusScale = StatusNumberScale::Once;
    const auto poisonValue = poisonPerTrigger.base == EffectNumberBase::TargetCurrentHp
        ? std::format("目前生命{}%", poisonPerTrigger.percent)
        : descriptionNumberLabel(poisonPerTrigger, style);
    const auto target = selectorLabel(behaviorRule->selector,
        style == EffectDescriptionStyle::Compact);
    const auto* charges = std::get_if<SetStatusTriggerCharges>(&poison->quantity);
    if (!charges) return false;

    if (style == EffectDescriptionStyle::Full)
    {
        appendSemanticRow(rendered, style,
            fullTriggerPrefix(block, kind, context)
                + std::format(
                    "獲得{}層毒爆；此效果提供的毒爆最多{}層。每層提供{}死亡爆炸{}傷害",
                    layers->count,
                    layers->limit,
                    explosionValue,
                    damageKindLabel(explosion->kind)));
        appendSemanticRow(rendered, style,
            std::format("施法者死亡時，逐層引爆{}", battleStatusLabel(applied->status)));
        appendSemanticRow(rendered, style,
            std::format(
                "每層對{}造成該層「死亡爆炸{}傷害」數值的{}傷害",
                target,
                damageKindLabel(explosion->kind),
                damageKindLabel(explosion->kind)),
            1);
        appendSemanticRow(rendered, style,
            std::format(
                "並施加可觸發{}次的中毒（每次造成{}傷害，持續{}幀）",
                charges->count,
                poisonValue,
                poison->durationFrames),
            1,
            EffectDescriptionSemanticBreak::Sequence);
    }
    else
    {
        appendSemanticRow(rendered, style,
            compactTriggerPrefix(block, kind, context)
                + std::format(
                    "毒爆+{}層（此來源最多{}層；每層爆炸傷害{}）",
                    layers->count,
                    layers->limit,
                    explosionValue));
        appendSemanticRow(rendered, style, "施法者死亡：逐層引爆");
        appendSemanticRow(rendered, style,
            std::format(
                "每層爆炸傷害{}並施加中毒（{}次、每次{}、{}幀）",
                target,
                charges->count,
                poisonValue,
                poison->durationFrames),
            1);
    }
    return true;
}

bool renderProfiledStatusBlock(
    RenderedEffectDescriptionBlock& rendered,
    const EffectDescriptionBlock& block,
    EffectDescriptionStyle style,
    EffectDescriptionContainerKind kind,
    const EffectDescriptionPresentationContext& context)
{
    const auto actions = descriptionActions(block);
    const ChangeResourceAction* healing = nullptr;
    const ApplyStatusAction* applied = nullptr;
    for (const auto* action : actions)
    {
        if (!healing)
        {
            const auto* candidate = std::get_if<ChangeResourceAction>(&action->value);
            if (candidate
                && candidate->resource == BattleResource::Hp
                && candidate->kind == ResourceChangeKind::Restore)
            {
                healing = candidate;
            }
        }
        if (!applied) applied = std::get_if<ApplyStatusAction>(&action->value);
    }
    if (!applied) return false;

    if (applied->status == BattleStatusKind::TrueQi
        && healing
        && actions.size() == 2
        && std::get_if<ChangeResourceAction>(&actions[0]->value) == healing
        && std::get_if<ApplyStatusAction>(&actions[1]->value) == applied
        && descriptionConditions(block).empty()
        && hasDefaultRuleQualifiers(ruleQualifiers(block))
        && statusBehaviorHasExactShape(*applied, { 1 }))
    {
        const auto* layers = std::get_if<AddStatusLayers>(&applied->quantity);
        const auto* damage = findStatusBehaviorAction<DealDamageAction>(*applied);
        if (!layers || !damage
            || healing->amount.base != EffectNumberBase::SourceMaxHp
            || healing->amount.multiplierBase)
        {
            return false;
        }
        auto perLayer = damage->amount;
        perLayer.statusScale = StatusNumberScale::Once;
        const auto damageValue = descriptionNumberLabel(perLayer, style);
        if (style == EffectDescriptionStyle::Full)
        {
            appendSemanticRow(rendered, style,
                fullTriggerPrefix(block, kind, context)
                    + std::format(
                        "回復{}點加最大生命{}%的生命，並獲得{}層真氣；此效果提供的真氣最多{}層",
                        healing->amount.flat,
                        healing->amount.percent,
                        layers->count,
                        layers->limit));
            appendSemanticRow(rendered, style,
                std::format(
                    "持有者命中時，每層真氣對命中目標附加{}點{}傷害",
                    damageValue,
                    damageKindLabel(damage->kind)));
        }
        else
        {
            appendSemanticRow(rendered, style,
                compactTriggerPrefix(block, kind, context)
                    + std::format(
                        "回血{}+最大生命{}%，真氣+{}層",
                        healing->amount.flat,
                        healing->amount.percent,
                        layers->count));
            appendSemanticRow(rendered, style,
                std::format(
                    "此來源最多{}層；每層命中附加{}{}傷害",
                    layers->limit,
                    damageValue,
                    damageKindLabel(damage->kind)),
                1);
        }
        return true;
    }

    if (applied->status == BattleStatusKind::WitheredBone
        && actions.size() == 1
        && std::get_if<ApplyStatusAction>(&actions.front()->value) == applied
        && descriptionTrigger(block).observation == EffectObservationScope::Owner
        && descriptionConditions(block).empty()
        && hasDefaultRuleQualifiers(ruleQualifiers(block))
        && statusBehaviorHasExactShape(*applied, { 2 }))
    {
        const auto* damage = findStatusBehaviorAction<ModifyDamageAction>(
            *applied,
            [](const ModifyDamageAction& modifier)
            {
                return modifier.perspective == DamageModifierPerspective::Incoming
                    && modifier.operation == DamageModifierOperation::PercentAdd;
            });
        const auto* healingModifier =
            findStatusBehaviorAction<ModifyHealTransactionAction>(*applied);
        if (!damage || !healingModifier) return false;
        const auto damageIncrease = effectiveConstantEffectNumberValue(damage->amount);
        if (!damageIncrease) return false;
        const auto healingReduction = 100 - healingModifier->percent;
        if (style == EffectDescriptionStyle::Full)
        {
            const auto duration = statusDurationLabel(
                *applied,
                EffectDescriptionStyle::Full);
            appendSemanticRow(rendered, style,
                fullTriggerPrefix(block, kind, context)
                    + std::format(
                        "使目標進入枯骨狀態{}幀；再次施加會以新的持續時間取代剩餘時間",
                        duration));
            appendSemanticRow(rendered, style,
                std::format(
                    "狀態期間，目標受到的傷害提高{}%、受到的治療降低{}%",
                    *damageIncrease,
                    healingReduction));
        }
        else
        {
            const auto duration = statusDurationLabel(
                *applied,
                EffectDescriptionStyle::Compact);
            appendSemanticRow(rendered, style,
                compactTriggerPrefix(block, kind, context)
                    + std::format(
                        "枯骨{}幀（再施加取代剩餘時間）",
                        duration));
            appendSemanticRow(rendered, style,
                std::format(
                    "目標受傷+{}%、受療-{}%",
                    *damageIncrease,
                    healingReduction),
                1);
        }
        return true;
    }

    return false;
}

std::string genericTriggerPrefix(
    const EffectDescriptionBlock& block,
    EffectDescriptionStyle style,
    const EffectDescriptionPresentationContext& context)
{
    const auto& trigger = descriptionTrigger(block);
    if (trigger.castMatch == EffectCastMatch::BoundMagic
        && context.enclosingDefaultEvent == trigger.event)
        return {};
    const bool compact = style == EffectDescriptionStyle::Compact;
    const auto conditions = projectedDescriptionConditionFacts(block, style);
    const bool ordinaryAcceptedHit = std::ranges::any_of(
        conditions,
        [](const EffectDescriptionFact* fact)
        {
            return fact->absorption
                == DescriptionPhraseAbsorption::OrdinaryHitTrigger;
        });
    const bool sameComboAllyDeath = std::ranges::any_of(
        conditions,
        [](const EffectDescriptionFact* fact)
        {
            return fact->absorption
                == DescriptionPhraseAbsorption::SameComboAllyDeathTrigger;
        });
    auto result = ordinaryAcceptedHit
        ? compact ? std::string{"命中後"} : std::string{"每次命中後"}
        : sameComboAllyDeath
        ? compact ? std::string{"同羈絆友軍死亡"} : std::string{"同羈絆友軍死亡時"}
        : ruleEventLabel(trigger.event, style);
    if (trigger.observation == EffectObservationScope::OwnerTeamEventSource)
        result += compact ? (result.empty() ? "同隊來源" : "（同隊來源）") : "（由效果持有者同隊事件來源觸發）";
    else if (trigger.observation == EffectObservationScope::EventTarget)
        result += compact
            ? (result.empty() ? "作用於持有者" : "（作用於持有者）")
            : "（該次效果作用於效果持有者時觸發）";
    if (trigger.castMatch == EffectCastMatch::OwnerAnyCast)
        result += compact ? (result.empty() ? "任意施放" : "（任意施放）") : "（效果持有者任意施放）";
    if (!result.empty() && !ordinaryAcceptedHit)
        result += compact ? "：" : "，";
    return result;
}

void renderGenericPlayerBlock(
    RenderedEffectDescriptionBlock& rendered,
    const EffectDescriptionBlock& block,
    EffectDescriptionStyle style,
    EffectDescriptionContainerKind kind,
    const EffectDescriptionPresentationContext& context)
{
    const auto& trigger = descriptionTrigger(block);
    const auto conditions = projectedDescriptionConditionFacts(block, style);
    DescriptionRuleQualifiersFact hiddenQualifiers;
    const auto qualifierFact = std::ranges::find_if(
        block.conditions,
        [](const EffectDescriptionFact& fact)
        {
            return std::holds_alternative<DescriptionRuleQualifiersFact>(fact.value);
        });
    assert(qualifierFact != block.conditions.end());
    const auto& qualifiers = isProjected(qualifierFact->projection, style)
        ? std::get<DescriptionRuleQualifiersFact>(qualifierFact->value)
        : hiddenQualifiers;
    auto triggerLead = genericTriggerPrefix(block, style, context);
    if (trigger.event == EffectEvent::FrameAdvanced
        && qualifiers.intervalFrames > 0)
    {
        triggerLead = style == EffectDescriptionStyle::Compact
            ? std::format("每{}幀：", qualifiers.intervalFrames)
            : std::format("每{}幀一次，", qualifiers.intervalFrames);
    }
    auto lead = triggerLead;
    std::string conditionText;
    std::vector<std::string> conditionRows;
    const bool compactPositiveDamagePerspective =
        style == EffectDescriptionStyle::Compact
        && std::ranges::any_of(conditions, [](const EffectDescriptionFact* fact)
        {
            return fact->absorption
                == DescriptionPhraseAbsorption::CompactPositiveDamagePerspective;
        });
    for (const auto* fact : conditions)
    {
        if (fact->absorption == DescriptionPhraseAbsorption::UltimateEvent
            || fact->absorption == DescriptionPhraseAbsorption::OrdinaryHitTrigger
            || fact->absorption == DescriptionPhraseAbsorption::SameComboAllyDeathTrigger)
            continue;
        const auto& condition = descriptionCondition(*fact);
        if (style == EffectDescriptionStyle::Compact
            && fact->absorption
                == DescriptionPhraseAbsorption::CompactPositiveDamagePerspective)
        {
            assert(std::holds_alternative<AcceptedHitCondition>(condition));
            continue;
        }
        auto label = [&]
        {
            if (const auto* perspective = std::get_if<DamagePerspectiveCondition>(&condition);
                compactPositiveDamagePerspective && perspective)
                return perspective->perspective == DamagePerspective::Dealt
                    ? std::string{"持有者造成正傷害"}
                    : std::string{"持有者承受正傷害"};
            if (const auto* accepted = std::get_if<AcceptedHitCondition>(&condition);
                accepted && accepted->requirePositiveDamage)
                return std::string{"正傷害"};
            return conditionLabel(
                condition,
                style == EffectDescriptionStyle::Compact,
                trigger.event);
        }();
        if (!conditionText.empty()) conditionText += "且";
        conditionText += label;
        conditionRows.push_back(std::move(label));
    }
    if (!conditionText.empty())
        lead += style == EffectDescriptionStyle::Compact
            ? "若" + conditionText + "："
            : "若" + conditionText + "，";
    if (qualifiers.chancePct < 100)
    {
        auto chance = style == EffectDescriptionStyle::Compact
            ? std::format("{}%機率", qualifiers.chancePct)
            : std::format("有{}%機率", qualifiers.chancePct);
        lead += chance + (style == EffectDescriptionStyle::Compact ? "：" : "，");
        conditionRows.push_back(std::move(chance));
    }
    std::string targetLead;
    const auto& selector = descriptionTarget(block);
    if (selector.role != DescriptionTargetRole::EffectOwner)
    {
        targetLead = "對" + selectorRoleLabel(
            selector,
            trigger.event,
            style == EffectDescriptionStyle::Compact,
            kind);
    }

    const int rowLimit = style == EffectDescriptionStyle::Compact ? 72 : 118;
    if (displayTextWidth(lead) > rowLimit)
    {
        while (triggerLead.ends_with("，"))
            triggerLead.resize(triggerLead.size() - std::string_view("，").size());
        if (!triggerLead.empty())
        {
            if (!triggerLead.ends_with("：")) triggerLead += "：";
            appendSemanticHeadingRow(
                rendered,
                style,
                std::move(triggerLead),
                0,
                EffectDescriptionSemanticBreak::Block);
        }
        for (auto& condition : conditionRows)
            appendSemanticHeadingRow(
                rendered,
                style,
                "條件：" + condition,
                1,
                EffectDescriptionSemanticBreak::Qualifier);
        lead.clear();
    }

    const DescriptionAction* singleProjectedAction = nullptr;
    std::size_t projectedActionCount{};
    for (const auto& group : block.actions)
    {
        for (const auto& action : group.actions)
        {
            if (!isProjected(action.projection, style)) continue;
            ++projectedActionCount;
            singleProjectedAction = &action;
        }
    }
    const bool singleLeaf = projectedActionCount == 1
        && descriptionEffectAction(*singleProjectedAction);
    if (singleLeaf)
    {
        const auto& action = *descriptionEffectAction(*singleProjectedAction);
        auto actionRows = renderPlayerActionDescriptionRows(
            action,
            style,
            trigger.event,
            true,
            context);
        assert(!actionRows.empty());
        int actionIndent = 0;
        auto text = std::move(actionRows.front().text);
        auto candidate = lead + targetLead + text;
        if (!lead.empty() && displayTextWidth(candidate) > rowLimit)
        {
            while (lead.ends_with("，"))
                lead.resize(lead.size() - std::string_view("，").size());
            if (!lead.ends_with("：")) lead += "：";
            appendSemanticHeadingRow(
                rendered,
                style,
                std::move(lead),
                0,
                EffectDescriptionSemanticBreak::Block);
            text = std::move(targetLead) + text;
            actionIndent = 1;
        }
        else
        {
            text = std::move(candidate);
        }
        appendSemanticRow(rendered, style, std::move(text), actionIndent);
        for (std::size_t index = 1; index < actionRows.size(); ++index)
        {
            appendSemanticRow(
                rendered,
                style,
                std::move(actionRows[index].text),
                actionIndent + actionRows[index].indent,
                actionRows[index].breakBefore);
        }
    }
    else
    {
        auto actionLead = std::move(lead) + std::move(targetLead);
        while (actionLead.ends_with("，") || actionLead.ends_with("："))
        {
            actionLead.resize(actionLead.size() - std::string_view("，").size());
        }
        const bool hasActionLead = !actionLead.empty();
        if (hasActionLead)
        {
            actionLead += "：";
            appendSemanticHeadingRow(
                rendered,
                style,
                std::move(actionLead),
                0,
                EffectDescriptionSemanticBreak::Block);
        }
        renderPlayerActionGroups(
            rendered,
            block.actions,
            trigger.event,
            style,
            hasActionLead ? 1 : 0,
            context);
    }

    for (auto text : ruleQualifierDescriptions(qualifiers, style, trigger.event))
    {
        appendSemanticRow(
            rendered,
            style,
            std::move(text),
            1,
            EffectDescriptionSemanticBreak::Qualifier);
    }
}

void renderPlayerBlock(
    RenderedEffectDescriptionBlock& rendered,
    const EffectDescriptionBlock& block,
    EffectDescriptionStyle style,
    EffectDescriptionContainerKind kind,
    const EffectDescriptionPresentationContext& context)
{
    if (block.archetype == DescriptionArchetype::Generic)
    {
        if (!playerCoverageProjectionIsComplete(block, style))
            throw std::logic_error("泛型玩家描述投影不完整");
    }
    else if (!specializedProjectionIsComplete(block, style))
    {
        throw std::logic_error("專用玩家描述投影不完整");
    }
    if (!std::ranges::all_of(block.trigger, [&](const EffectDescriptionFact& fact)
        {
            return isProjected(fact.projection, style);
        })
        || !std::ranges::all_of(block.targets, [&](const EffectDescriptionFact& fact)
        {
            return isProjected(fact.projection, style);
        }))
        return;
    if (block.archetype == DescriptionArchetype::Generic
        && renderProfiledStatusBlock(
            rendered,
            block,
            style,
            kind,
            context))
    {
        return;
    }
    bool renderedArchetype = false;
    switch (block.archetype)
    {
    case DescriptionArchetype::BorrowRules:
        renderedArchetype = renderBorrowRulesBlock(rendered, block, style, kind, context);
        break;
    case DescriptionArchetype::CopyAttack:
        renderedArchetype = renderCopyAttackBlock(rendered, block, style, kind, context);
        break;
    case DescriptionArchetype::ConditionalAttack:
        renderedArchetype = renderConditionalAttackBlock(rendered, block, style);
        break;
    case DescriptionArchetype::StatusLifecycle:
        renderedArchetype = renderStatusLifecycleBlock(
            rendered, block, style, kind, context);
        break;
    case DescriptionArchetype::StackExplosion:
        renderedArchetype = renderStackExplosionBlock(
            rendered, block, style, kind, context);
        break;
    default:
        break;
    }
    if (block.archetype == DescriptionArchetype::Generic)
        renderGenericPlayerBlock(rendered, block, style, kind, context);
    else if (!renderedArchetype)
        renderGenericPlayerBlock(rendered, block, style, kind, context);
}

bool playerCardBlockProjectionIsComplete(const EffectDescriptionBlock& block)
{
    return std::ranges::all_of(block.trigger,
        [](const EffectDescriptionFact& fact)
        {
            return isProjected(fact.projection, EffectDescriptionStyle::Compact);
        })
        && std::ranges::all_of(block.targets,
            [](const EffectDescriptionFact& fact)
            {
                return isProjected(fact.projection, EffectDescriptionStyle::Compact);
            })
        && std::ranges::all_of(block.conditions,
            [](const EffectDescriptionFact& fact)
            {
                return isProjected(fact.projection, EffectDescriptionStyle::Compact);
            })
        && allActionsProjected(block.actions, EffectDescriptionStyle::Compact)
        && playerCoverageProjectionIsComplete(
            block,
            EffectDescriptionStyle::Compact);
}

bool isConstantAmount(const EffectNumber& amount, int value)
{
    EffectNumber expected;
    expected.flat = value;
    return amount == expected;
}

bool matchesSanqingActions(const EffectDescriptionBlock& block)
{
    if (block.actions.size() != 1
        || block.actions.front().actions.size() != 2
        || !block.actions.front().sequential)
        return false;
    const auto& actions = block.actions.front().actions;
    const auto matches = [&](std::size_t index, BattleResource resource,
                             const EffectNumber& amount, bool requiresFullMp)
    {
        const auto* action = descriptionEffectAction(actions[index]);
        const auto* change = action ? std::get_if<ChangeResourceAction>(&action->value) : nullptr;
        return change && change->resource == resource
            && change->kind == ResourceChangeKind::Restore
            && change->amount == amount && !change->transferDestination
            && change->healKind == EffectHealKind::Direct
            && change->healSourcePolicy == EffectHealSourcePolicy::RequireAlive
            && change->healRequiresFullMp == requiresFullMp;
    };
    return matches(0, BattleResource::Mp, EffectNumber{ .flat = 20 }, false)
        && matches(1, BattleResource::Hp,
            EffectNumber{ .base = EffectNumberBase::SourceStar, .percent = 3300 }, true);
}

bool matchesSanqingBlock(
    const EffectDescriptionBlock& block,
    const EffectSelector& selector)
{
    const auto& trigger = descriptionTrigger(block);
    return block.archetype == DescriptionArchetype::Generic
        && playerCardBlockProjectionIsComplete(block)
        && trigger.event == EffectEvent::AttackCommitted
        && trigger.observation == EffectObservationScope::Owner
        && trigger.castMatch == EffectCastMatch::BoundMagic
        && descriptionTarget(block).selector == selector
        && descriptionConditions(block).empty()
        && hasDefaultRuleQualifiers(ruleQualifiers(block))
        && matchesSanqingActions(block);
}

std::optional<RenderedEffectDescription> renderPlayerCardSanqing(
    const EffectDescriptionDocument& document)
{
    if (document.sections.size() != 1) return std::nullopt;
    const auto& blocks = document.sections.front().blocks;
    if (blocks.size() != 2) return std::nullopt;

    const EffectSelector ownerSelector{};
    const EffectSelector allySelector{
        .kind = EffectSelectorKind::LowestMpAllies,
        .count = 2,
        .excludeOwner = true,
    };
    const auto owner = std::ranges::find_if(
        blocks,
        [&](const EffectDescriptionBlock& block)
        {
            return descriptionTarget(block).selector == ownerSelector;
        });
    const auto allies = std::ranges::find_if(
        blocks,
        [&](const EffectDescriptionBlock& block)
        {
            return descriptionTarget(block).selector == allySelector;
        });
    if (owner == blocks.end()
        || allies == blocks.end()
        || owner == allies
        || !matchesSanqingBlock(*owner, ownerSelector)
        || !matchesSanqingBlock(*allies, allySelector))
        return std::nullopt;

    RenderedEffectDescription result;
    RenderedEffectDescriptionSection section;
    RenderedEffectDescriptionBlock block;
    appendSemanticHeadingRow(
        block,
        EffectDescriptionStyle::Compact,
        "自身及內力最低的2名友軍（不含自身）：",
        0,
        EffectDescriptionSemanticBreak::Block);
    appendSemanticRow(
        block,
        EffectDescriptionStyle::Compact,
        "先內力+20；回內後滿內力→生命+星級×33",
        1,
        EffectDescriptionSemanticBreak::Branch);
    section.blocks.push_back(std::move(block));
    result.sections.push_back(std::move(section));
    return result;
}

template<typename Action>
const Action* singleStateMachineAction(const EffectDescriptionBlock& block)
{
    const auto* action = block.actions.size() == 1
        && block.actions.front().actions.size() == 1
        ? descriptionEffectAction(block.actions.front().actions.front())
        : nullptr;
    if (!action) return nullptr;
    const auto* machine = std::get_if<StateMachineAction>(&action->value);
    return machine ? std::get_if<Action>(machine) : nullptr;
}

bool matchesMemoryBlockBase(
    const EffectDescriptionBlock& block,
    EffectEvent event,
    const EffectSelector& selector)
{
    const auto& trigger = descriptionTrigger(block);
    return block.archetype == DescriptionArchetype::Generic
        && playerCardBlockProjectionIsComplete(block)
        && trigger.event == event
        && trigger.observation == EffectObservationScope::Owner
        && trigger.castMatch == EffectCastMatch::BoundMagic
        && descriptionTarget(block).selector == selector
        && hasDefaultRuleQualifiers(ruleQualifiers(block));
}

const EffectDescriptionBlock* blockForEvent(
    const EffectDescriptionDocument& document,
    EffectEvent event)
{
    const EffectDescriptionBlock* result = nullptr;
    for (const auto& section : document.sections)
    {
        if (section.event != event) continue;
        if (result || section.blocks.size() != 1) return nullptr;
        result = &section.blocks.front();
    }
    return result;
}

std::optional<RenderedEffectDescription> renderPlayerCardDamageMemory(
    const EffectDescriptionDocument& document)
{
    const EffectSelector self{};
    const EffectSelector hitTarget{
        .kind = EffectSelectorKind::HitTarget,
    };
    const auto* recordBlock = blockForEvent(document, EffectEvent::DamageResolved);
    const auto* commitBlock = blockForEvent(document, EffectEvent::AttackCommitted);
    const auto* hitBlock = blockForEvent(
        document,
        EffectEvent::MainProjectileBeforeDamage);
    const auto* settledBlock = blockForEvent(document, EffectEvent::CastSettled);

    const auto matchesRecord = [&](const EffectDescriptionBlock* block,
                                   EffectStateSlot slot,
                                   DamagePerspective perspective)
    {
        if (!block
            || !matchesMemoryBlockBase(
                *block,
                EffectEvent::DamageResolved,
                self)
            || descriptionConditions(*block).size() != 2)
            return false;
        const auto conditions = descriptionConditions(*block);
        bool hasExpectedPerspective{};
        bool hasAttackOrigin{};
        for (const auto* condition : conditions)
        {
            if (const auto* damagePerspective =
                    std::get_if<DamagePerspectiveCondition>(condition))
            {
                hasExpectedPerspective = damagePerspective->perspective == perspective;
            }
            else if (std::holds_alternative<DamageOriginIsAttackCondition>(*condition))
            {
                hasAttackOrigin = true;
            }
        }
        if (!hasExpectedPerspective || !hasAttackOrigin)
            return false;
        const auto* record = singleStateMachineAction<RecordMaximumDamageAction>(*block);
        return record
            && record->slot == slot
            && record->channel == DamageChannel::Skill;
    };
    const auto matchesTransfer = [&](const EffectDescriptionBlock* block)
    {
        if (!block
            || !matchesMemoryBlockBase(
                *block,
                EffectEvent::AttackCommitted,
                self)
            || !descriptionConditions(*block).empty())
            return false;
        const auto* transfer = singleStateMachineAction<TransferStateValueAction>(*block);
        return transfer
            && transfer->sourceSlot == EffectStateSlot::MaximumSkillHpDamage
            && transfer->destinationSlot == EffectStateSlot::CastMaximumHpDamage;
    };
    const auto matchesConsume = [&](const EffectDescriptionBlock* block,
                                    bool* clearAfterConsume)
    {
        if (!block
            || !matchesMemoryBlockBase(
                *block,
                EffectEvent::MainProjectileBeforeDamage,
                hitTarget)
            || !descriptionConditions(*block).empty())
            return false;
        const auto* consume = singleStateMachineAction<ConsumeRecordedMaximumAction>(*block);
        if (!consume
            || consume->slot != EffectStateSlot::CastMaximumHpDamage
            || consume->destination != StateValueDestination::DamageAmount
            || consume->percent != 100)
            return false;
        *clearAfterConsume = consume->clearAfterConsume;
        return true;
    };
    const auto matchesShieldRecord = [&](const EffectDescriptionBlock* block)
    {
        return matchesRecord(
            block,
            EffectStateSlot::CastMaximumHpDamage,
            DamagePerspective::Dealt);
    };
    const auto matchesShieldConsume = [&](const EffectDescriptionBlock* block)
    {
        if (!block
            || !matchesMemoryBlockBase(
                *block,
                EffectEvent::CastSettled,
                self)
            || !descriptionConditions(*block).empty())
            return false;
        const auto* consume = singleStateMachineAction<ConsumeRecordedMaximumAction>(*block);
        return consume
            && consume->slot == EffectStateSlot::CastMaximumHpDamage
            && consume->destination == StateValueDestination::ShieldAmount
            && consume->percent == 100
            && consume->clearAfterConsume;
    };

    bool clearAfterConsume{};
    const bool taiji = document.sections.size() == 3
        && matchesRecord(
            recordBlock,
            EffectStateSlot::MaximumSkillHpDamage,
            DamagePerspective::Received)
        && matchesTransfer(commitBlock)
        && matchesConsume(hitBlock, &clearAfterConsume)
        && !settledBlock;
    const bool ironPalm = document.sections.size() == 2
        && matchesShieldRecord(recordBlock)
        && matchesShieldConsume(settledBlock)
        && !commitBlock
        && !hitBlock;
    if (!taiji && !ironPalm) return std::nullopt;

    RenderedEffectDescription result;
    for (const auto& sourceSection : document.sections)
    {
        RenderedEffectDescriptionSection section;
        RenderedEffectDescriptionBlock block;
        if (sourceSection.event == EffectEvent::DamageResolved)
        {
            appendSemanticRow(
                block,
                EffectDescriptionStyle::Compact,
                taiji
                    ? "承受招式傷害後記錄最大單次招式生命傷害；初始0"
                    : "造成招式傷害後記錄本次施放最高單次招式生命傷害；初始0");
        }
        else if (sourceSection.event == EffectEvent::AttackCommitted)
        {
            appendSemanticRow(
                block,
                EffectDescriptionStyle::Compact,
                "施放時帶入本次施放記錄；原記錄清空");
        }
        else if (sourceSection.event == EffectEvent::MainProjectileBeforeDamage)
        {
            appendSemanticRow(
                block,
                EffectDescriptionStyle::Compact,
                clearAfterConsume
                    ? "主彈命中：消耗記錄值並附加100%純粹傷害；命中後清除記錄"
                    : "主彈命中：讀取記錄值並附加100%純粹傷害；保留記錄");
        }
        else
        {
            appendSemanticRow(
                block,
                EffectDescriptionStyle::Compact,
                "施放結算完成時消耗記錄值並獲得100%護盾；清除記錄");
        }
        section.blocks.push_back(std::move(block));
        result.sections.push_back(std::move(section));
    }
    return result;
}

std::optional<RenderedEffectDescription> renderPlayerCardDocument(
    const EffectDescriptionDocument& document)
{
    if (const auto sanqing = renderPlayerCardSanqing(document))
        return sanqing;
    return renderPlayerCardDamageMemory(document);
}

}  // namespace EffectDescriptionDetail

using namespace EffectDescriptionDetail;

RenderedEffectDescription renderEffectDescription(
    const EffectDescriptionDocument& document,
    EffectDescriptionStyle style,
    const EffectDescriptionPresentationContext& context)
{
    auto effectiveContext = context;
    if (document.kind == EffectDescriptionContainerKind::Magic
        && !effectiveContext.enclosingDefaultEvent)
    {
        effectiveContext.enclosingDefaultEvent = EffectEvent::AttackCommitted;
    }
    if (style == EffectDescriptionStyle::Compact
        && effectiveContext.compactPolicy
            == EffectDescriptionCompactPolicy::PlayerCard)
    {
        if (!document.cardSummary.empty())
        {
            RenderedEffectDescriptionBlock summary;
            for (const auto& sentence : document.cardSummary)
                summary.rows.push_back({
                    .kind = EffectDescriptionRowKind::Summary,
                    .text = sentence,
                    .wrapping = DisplayTextWrapping::Prose,
                });
            RenderedEffectDescriptionSection section;
            section.blocks.push_back(std::move(summary));
            return {.sections = {std::move(section)}};
        }
        if (const auto playerCard = renderPlayerCardDocument(document))
            return *playerCard;
    }
    RenderedEffectDescription result;
    for (const auto& section : document.sections)
    {
        RenderedEffectDescriptionSection renderedSection;
        if (style == EffectDescriptionStyle::Detailed && section.event)
        {
            assert(!section.blocks.empty());
            renderedSection.heading = eventHeading(
                *section.event,
                document.kind,
                descriptionTrigger(section.blocks.front()).observation);
        }
        for (const auto& block : section.blocks)
        {
            RenderedEffectDescriptionBlock renderedBlock;
            if (style == EffectDescriptionStyle::Detailed)
                renderDetailedBlock(renderedBlock, block, document.kind);
            else
                renderPlayerBlock(
                    renderedBlock,
                    block,
                    style,
                    document.kind,
                    effectiveContext);
            renderedSection.blocks.push_back(std::move(renderedBlock));
        }
        result.sections.push_back(std::move(renderedSection));
    }
    return result;
}

std::vector<std::string> effectDescriptionTextRows(
    const RenderedEffectDescription& rendered)
{
    std::vector<std::string> result;
    for (const auto& section : rendered.sections)
    {
        if (section.heading) result.push_back(*section.heading);
        for (const auto& block : section.blocks)
        {
            for (const auto& row : block.rows)
                result.push_back(std::string(static_cast<std::size_t>(row.indent) * 2, ' ') + row.text);
        }
    }
    return result;
}

std::string joinEffectDescriptionRows(
    const RenderedEffectDescription& rendered,
    std::string_view separator)
{
    std::string result;
    for (const auto& row : effectDescriptionTextRows(rendered))
    {
        if (!result.empty()) result += separator;
        result += row;
    }
    return result;
}

}  // namespace KysChess
