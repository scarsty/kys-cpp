#include "BattleLogPresenter.h"

#include <format>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
constexpr int kMaxBattleLogEntries = 9999;

using KysChess::Battle::BattleLogCategory;
using KysChess::Battle::BattleLogPerspective;
using KysChess::Battle::BattleLogTextSegment;
using KysChess::Battle::BattleLogTextTone;

BattleLogEntryTone teamToEntryTone(int team)
{
    if (team == 0) return BattleLogEntryTone::Ally;
    if (team == 1) return BattleLogEntryTone::Enemy;
    return BattleLogEntryTone::Neutral;
}

BattleLogTextTone teamToTextTone(int team)
{
    if (team == 0) return BattleLogTextTone::AllyName;
    if (team == 1) return BattleLogTextTone::EnemyName;
    return BattleLogTextTone::Default;
}

void appendTextSegment(
    BattleLogEntryView& entry,
    std::string text,
    BattleLogTextTone tone = BattleLogTextTone::Default)
{
    if (!text.empty())
    {
        entry.segments.push_back({ std::move(text), tone });
    }
}

void appendTextSegments(
    BattleLogEntryView& entry,
    const std::vector<BattleLogTextSegment>& segments)
{
    for (const auto& segment : segments)
    {
        appendTextSegment(entry, segment.text, segment.tone);
    }
}

bool hasVisibleTextSegment(const std::vector<BattleLogTextSegment>& segments)
{
    for (const auto& segment : segments)
    {
        if (!segment.text.empty())
        {
            return true;
        }
    }
    return false;
}

BattleLogEntryCategory toEntryCategory(BattleLogCategory category)
{
    switch (category)
    {
    case BattleLogCategory::Cast:
        return BattleLogEntryCategory::Cast;
    case BattleLogCategory::ProjectileCancel:
        return BattleLogEntryCategory::ProjectileCancel;
    case BattleLogCategory::Status:
        return BattleLogEntryCategory::Status;
    }
    return BattleLogEntryCategory::Status;
}

std::string battleResultText(int battleResult)
{
    return battleResult == 0 ? "戰鬥勝利" : (battleResult == 1 ? "戰鬥失敗" : "戰鬥結束");
}

std::unordered_map<int, std::string> buildDistinctRoleLabels(
    const std::vector<BattlePostBattleUnitSummary>& team)
{
    std::unordered_map<std::string, int> totals;
    std::unordered_map<int, std::string> labels;
    for (const auto& unit : team)
    {
        totals[unit.identity.name]++;
    }

    std::unordered_map<std::string, int> seen;
    for (const auto& unit : team)
    {
        const std::string baseName = unit.identity.name;
        const int instanceIndex = ++seen[baseName];
        labels[unit.identity.battleId] = totals[baseName] > 1
            ? std::format("{} [{}]", baseName, instanceIndex)
            : baseName;
    }

    return labels;
}

void setEntryParticipants(BattleLogEntryView& entry, const BattleReportEvent& event)
{
    entry.sourceId = event.sourceId;
    entry.targetId = event.targetId;
    entry.sourceTeam = event.sourceTeam;
    entry.targetTeam = event.targetTeam;
}

BattleLogEntryView buildBattleLogEntry(const BattleReportEvent& event)
{
    BattleLogEntryView entry;
    setEntryParticipants(entry, event);

    auto addFrame = [&]()
    {
        appendTextSegment(entry, "[", BattleLogTextTone::SystemAccent);
        appendTextSegment(entry, std::format("{:>4}F", event.frame), BattleLogTextTone::FrameValue);
        appendTextSegment(entry, "] ", BattleLogTextTone::SystemAccent);
    };

    switch (event.type)
    {
    case BattleReportEventType::Damage:
        entry.category = BattleLogEntryCategory::Damage;
        entry.tone = teamToEntryTone(event.sourceTeam);
        addFrame();
        appendTextSegment(entry, event.sourceName, teamToTextTone(event.sourceTeam));
        appendTextSegment(entry, event.skillName.empty() ? " 攻擊 " : " 施放 ");
        if (!event.skillName.empty())
        {
            appendTextSegment(entry, event.skillName, BattleLogTextTone::SkillName);
            appendTextSegment(entry, " 命中 ");
        }
        appendTextSegment(entry, event.targetName, teamToTextTone(event.targetTeam));
        appendTextSegment(entry, "，造成 ");
        appendTextSegment(entry, std::to_string(event.value), BattleLogTextTone::DamageValue);
        appendTextSegment(entry, " 點傷害");
        if (hasVisibleTextSegment(event.segments))
        {
            appendTextSegment(entry, "（");
            appendTextSegments(entry, event.segments);
            appendTextSegment(entry, "）");
        }
        break;
    case BattleReportEventType::Heal:
        entry.category = BattleLogEntryCategory::Heal;
        entry.tone = teamToEntryTone(event.targetTeam >= 0 ? event.targetTeam : event.sourceTeam);
        addFrame();
        if (event.perspective == BattleLogPerspective::SourceOnly && !event.sourceName.empty())
        {
            appendTextSegment(entry, event.sourceName, teamToTextTone(event.sourceTeam));
            appendTextSegment(entry, "：");
        }
        else if (!event.sourceName.empty() && !event.targetName.empty() && event.sourceId != event.targetId)
        {
            appendTextSegment(entry, event.sourceName, teamToTextTone(event.sourceTeam));
            appendTextSegment(entry, " 為 ");
            appendTextSegment(entry, event.targetName, teamToTextTone(event.targetTeam));
        }
        else if (!event.targetName.empty())
        {
            appendTextSegment(entry, event.targetName, teamToTextTone(event.targetTeam));
        }
        else
        {
            appendTextSegment(entry, event.sourceName, teamToTextTone(event.sourceTeam));
        }
        appendTextSegment(entry, " 恢復 ");
        appendTextSegment(entry, std::to_string(event.value), BattleLogTextTone::HealValue);
        appendTextSegment(entry, " 點生命");
        if (hasVisibleTextSegment(event.segments))
        {
            appendTextSegment(entry, "（");
            appendTextSegments(entry, event.segments);
            appendTextSegment(entry, "）");
        }
        break;
    case BattleReportEventType::Status:
        entry.category = toEntryCategory(event.category);
        entry.tone = teamToEntryTone(event.targetTeam >= 0 ? event.targetTeam : event.sourceTeam);
        if (entry.tone == BattleLogEntryTone::Neutral)
        {
            entry.tone = BattleLogEntryTone::System;
        }
        addFrame();
        if (event.perspective == BattleLogPerspective::SourceOnly && !event.sourceName.empty())
        {
            appendTextSegment(entry, event.sourceName, teamToTextTone(event.sourceTeam));
            appendTextSegment(entry, "：");
        }
        else if (!event.sourceName.empty() && !event.targetName.empty() && event.sourceId != event.targetId)
        {
            appendTextSegment(entry, event.sourceName, teamToTextTone(event.sourceTeam));
            appendTextSegment(entry, " 對 ");
            appendTextSegment(entry, event.targetName, teamToTextTone(event.targetTeam));
            appendTextSegment(entry, "：");
        }
        else if (!event.targetName.empty())
        {
            appendTextSegment(entry, event.targetName, teamToTextTone(event.targetTeam));
            appendTextSegment(entry, "：");
        }
        else if (!event.sourceName.empty())
        {
            appendTextSegment(entry, event.sourceName, teamToTextTone(event.sourceTeam));
            appendTextSegment(entry, "：");
        }
        appendTextSegments(entry, event.segments);
        break;
    case BattleReportEventType::Kill:
        entry.category = BattleLogEntryCategory::Lifecycle;
        entry.tone = teamToEntryTone(event.sourceTeam);
        addFrame();
        appendTextSegment(entry, event.sourceName, teamToTextTone(event.sourceTeam));
        appendTextSegment(entry, " 擊殺了 ");
        appendTextSegment(entry, event.targetName, teamToTextTone(event.targetTeam));
        break;
    case BattleReportEventType::Death:
        entry.category = BattleLogEntryCategory::Lifecycle;
        entry.tone = teamToEntryTone(event.targetTeam);
        addFrame();
        appendTextSegment(entry, event.targetName, teamToTextTone(event.targetTeam));
        appendTextSegment(entry, " 倒下");
        break;
    case BattleReportEventType::BattleEnd:
        entry.category = BattleLogEntryCategory::BattleEnd;
        entry.tone = BattleLogEntryTone::System;
        addFrame();
        appendTextSegment(entry, battleResultText(event.value), BattleLogTextTone::SystemAccent);
        break;
    }

    return entry;
}

void appendRoleRows(
    const std::vector<BattlePostBattleUnitSummary>& team,
    const std::unordered_map<int, std::string>& labels,
    std::vector<BattleLogRoleFilterRow>& rows)
{
    rows.reserve(team.size());
    for (const auto& unit : team)
    {
        BattleLogRoleFilterRow row;
        row.id = unit.identity.battleId;
        auto label = labels.find(unit.identity.battleId);
        row.name = label != labels.end() ? label->second : unit.identity.name;
        row.team = unit.identity.team;
        row.cancelDmg = unit.cancelDmg;
        row.hpRemaining = unit.hpRemaining;
        row.maxHp = unit.maxHpRemaining;
        row.dead = unit.dead;
        rows.push_back(std::move(row));
    }
}
}  // namespace

std::string BattleLogEntryView::plainText() const
{
    std::string text;
    for (const auto& segment : segments)
    {
        text += segment.text;
    }
    return text;
}

BattleLogViewModel BattleLogPresenter::present(
    const BattlePostBattleSummary& summary,
    const BattleReport& report) const
{
    BattleLogViewModel model;
    model.title = "本場戰鬥日誌";
    model.resultText = battleResultText(summary.battleResult);
    model.totalFrames = report.battleEndFrame();

    const auto allyLabels = buildDistinctRoleLabels(summary.allies);
    const auto enemyLabels = buildDistinctRoleLabels(summary.enemies);

    auto resolveRoleLabel = [&](int team, int battleId, const std::string& fallback) -> std::string
    {
        const auto& labels = team == 0 ? allyLabels : enemyLabels;
        const auto it = labels.find(battleId);
        return it != labels.end() ? it->second : fallback;
    };

    appendRoleRows(summary.allies, allyLabels, model.allies);
    appendRoleRows(summary.enemies, enemyLabels, model.enemies);

    for (auto& row : model.allies)
    {
        const auto stat = report.stats().find(row.id);
        if (stat == report.stats().end())
        {
            continue;
        }
        row.damageDealt = stat->second.damageDealt;
        row.damageTaken = stat->second.damageTaken;
        row.kills = stat->second.kills;
    }
    for (auto& row : model.enemies)
    {
        const auto stat = report.stats().find(row.id);
        if (stat == report.stats().end())
        {
            continue;
        }
        row.damageDealt = stat->second.damageDealt;
        row.damageTaken = stat->second.damageTaken;
        row.kills = stat->second.kills;
    }

    const int eventCount = static_cast<int>(report.events().size());
    int startIndex = 0;
    if (eventCount > kMaxBattleLogEntries)
    {
        model.omittedEntries = eventCount - kMaxBattleLogEntries;
        startIndex = model.omittedEntries;
    }

    if (model.omittedEntries > 0)
    {
        BattleLogEntryView omitted;
        omitted.tone = BattleLogEntryTone::System;
        omitted.category = BattleLogEntryCategory::Status;
        appendTextSegment(
            omitted,
            std::format("前 {} 條記錄已省略", model.omittedEntries),
            BattleLogTextTone::SystemAccent);
        model.entries.push_back(std::move(omitted));
    }

    model.entries.reserve(model.entries.size() + eventCount - startIndex);
    for (int i = startIndex; i < eventCount; ++i)
    {
        auto event = report.events()[i];
        if (event.sourceId >= 0)
        {
            event.sourceName = resolveRoleLabel(event.sourceTeam, event.sourceId, event.sourceName);
        }
        if (event.targetId >= 0)
        {
            event.targetName = resolveRoleLabel(event.targetTeam, event.targetId, event.targetName);
        }
        model.entries.push_back(buildBattleLogEntry(event));
    }

    if (model.entries.empty())
    {
        BattleLogEntryView empty;
        empty.tone = BattleLogEntryTone::System;
        empty.category = BattleLogEntryCategory::Status;
        appendTextSegment(empty, "本場戰鬥沒有產生可記錄事件。", BattleLogTextTone::SystemAccent);
        model.entries.push_back(std::move(empty));
    }

    return model;
}

bool battleLogEntryMatchesFilter(const BattleLogEntryView& entry, const BattleLogFilter& filter)
{
    if (filter.category != BattleLogEntryCategory::Any && entry.category != filter.category)
    {
        return false;
    }
    if (entry.sourceId < 0 && entry.targetId < 0)
    {
        return true;
    }

    auto matchesTeamFilter = [&](int filterId, int team)
    {
        if (filterId < 0)
        {
            return true;
        }
        return (entry.sourceTeam == team && entry.sourceId == filterId)
            || (entry.targetTeam == team && entry.targetId == filterId);
    };

    return matchesTeamFilter(filter.allyUnitId, 0)
        && matchesTeamFilter(filter.enemyUnitId, 1);
}

int countVisibleBattleLogEntries(const BattleLogViewModel& model, const BattleLogFilter& filter)
{
    int count = 0;
    for (const auto& entry : model.entries)
    {
        if (battleLogEntryMatchesFilter(entry, filter))
        {
            ++count;
        }
    }
    return count;
}
