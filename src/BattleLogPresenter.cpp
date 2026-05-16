#include "BattleLogPresenter.h"

#include <format>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
constexpr int kMaxBattleLogEntries = 9999;

BattleLogEntryTone teamToEntryTone(int team)
{
    if (team == 0) return BattleLogEntryTone::Ally;
    if (team == 1) return BattleLogEntryTone::Enemy;
    return BattleLogEntryTone::Neutral;
}

BattleLogFieldTone teamToFieldTone(int team)
{
    if (team == 0) return BattleLogFieldTone::AllyName;
    if (team == 1) return BattleLogFieldTone::EnemyName;
    return BattleLogFieldTone::Default;
}

void appendBattleLogSegment(
    BattleLogEntryView& entry,
    std::string text,
    BattleLogFieldTone tone = BattleLogFieldTone::Default)
{
    if (!text.empty())
    {
        entry.segments.push_back({ std::move(text), tone });
    }
}

struct HighlightInterval
{
    std::size_t start = 0;
    std::size_t end = 0;
    BattleLogFieldTone tone = BattleLogFieldTone::Default;
};

bool overlapsExisting(const std::vector<HighlightInterval>& intervals, std::size_t start, std::size_t end)
{
    for (const auto& interval : intervals)
    {
        if (start < interval.end && end > interval.start)
        {
            return true;
        }
    }
    return false;
}

void collectRegexIntervals(
    std::vector<HighlightInterval>& intervals,
    const std::string& text,
    const std::regex& pattern,
    BattleLogFieldTone tone)
{
    for (auto it = std::sregex_iterator(text.begin(), text.end(), pattern);
        it != std::sregex_iterator();
        ++it)
    {
        const auto start = static_cast<std::size_t>(it->position());
        const auto end = start + static_cast<std::size_t>(it->length());
        if (!overlapsExisting(intervals, start, end))
        {
            intervals.push_back({ start, end, tone });
        }
    }
}

void appendHighlightedLogText(
    BattleLogEntryView& entry,
    const std::string& text,
    BattleLogFieldTone fallbackTone = BattleLogFieldTone::Default)
{
    static const std::regex formulaPattern(R"(\d+(?:\s*[-+*/=]\s*\d+)+)");
    static const std::regex projectilePattern(R"(#\d+)");
    static const std::regex durationPattern(R"(\d+\s*(幀|秒|s|S))");

    std::vector<HighlightInterval> intervals;
    collectRegexIntervals(intervals, text, formulaPattern, BattleLogFieldTone::FormulaValue);
    collectRegexIntervals(intervals, text, projectilePattern, BattleLogFieldTone::ProjectileId);
    collectRegexIntervals(intervals, text, durationPattern, BattleLogFieldTone::DurationValue);
    std::sort(
        intervals.begin(),
        intervals.end(),
        [](const HighlightInterval& lhs, const HighlightInterval& rhs)
        {
            return lhs.start < rhs.start;
        });

    std::size_t cursor = 0;
    for (const auto& interval : intervals)
    {
        if (cursor < interval.start)
        {
            appendBattleLogSegment(entry, text.substr(cursor, interval.start - cursor), fallbackTone);
        }
        appendBattleLogSegment(entry, text.substr(interval.start, interval.end - interval.start), interval.tone);
        cursor = interval.end;
    }
    if (cursor < text.size())
    {
        appendBattleLogSegment(entry, text.substr(cursor), fallbackTone);
    }
}

bool textStartsWith(const std::string& text, const char* prefix)
{
    return text.rfind(prefix, 0) == 0;
}

BattleLogEntryCategory classifyStatusEvent(const std::string& text)
{
    if (text == "出手" || textStartsWith(text, "施放"))
    {
        return BattleLogEntryCategory::Cast;
    }
    if (textStartsWith(text, "抵消彈道") || textStartsWith(text, "彈道取消"))
    {
        return BattleLogEntryCategory::ProjectileCancel;
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
        appendBattleLogSegment(entry, "[", BattleLogFieldTone::SystemAccent);
        appendBattleLogSegment(entry, std::format("{:>4}F", event.frame), BattleLogFieldTone::FrameValue);
        appendBattleLogSegment(entry, "] ", BattleLogFieldTone::SystemAccent);
    };

    switch (event.type)
    {
    case BattleReportEventType::Damage:
        entry.category = BattleLogEntryCategory::Damage;
        entry.tone = teamToEntryTone(event.sourceTeam);
        addFrame();
        appendBattleLogSegment(entry, event.sourceName, teamToFieldTone(event.sourceTeam));
        appendBattleLogSegment(entry, event.skillName.empty() ? " 攻擊 " : " 施放 ");
        if (!event.skillName.empty())
        {
            appendBattleLogSegment(entry, event.skillName, BattleLogFieldTone::SkillName);
            appendBattleLogSegment(entry, " 命中 ");
        }
        appendBattleLogSegment(entry, event.targetName, teamToFieldTone(event.targetTeam));
        appendBattleLogSegment(entry, "，造成 ");
        appendBattleLogSegment(entry, std::to_string(event.value), BattleLogFieldTone::DamageValue);
        appendBattleLogSegment(entry, " 點傷害");
        if (!event.detailText.empty())
        {
            appendBattleLogSegment(entry, "（");
            appendHighlightedLogText(entry, event.detailText, BattleLogFieldTone::SkillName);
            appendBattleLogSegment(entry, "）");
        }
        break;
    case BattleReportEventType::Heal:
        entry.category = BattleLogEntryCategory::Heal;
        entry.tone = teamToEntryTone(event.targetTeam >= 0 ? event.targetTeam : event.sourceTeam);
        addFrame();
        if (!event.sourceName.empty() && !event.targetName.empty() && event.sourceId != event.targetId)
        {
            appendBattleLogSegment(entry, event.sourceName, teamToFieldTone(event.sourceTeam));
            appendBattleLogSegment(entry, " 為 ");
            appendBattleLogSegment(entry, event.targetName, teamToFieldTone(event.targetTeam));
        }
        else if (!event.targetName.empty())
        {
            appendBattleLogSegment(entry, event.targetName, teamToFieldTone(event.targetTeam));
        }
        else
        {
            appendBattleLogSegment(entry, event.sourceName, teamToFieldTone(event.sourceTeam));
        }
        appendBattleLogSegment(entry, " 恢復 ");
        appendBattleLogSegment(entry, std::to_string(event.value), BattleLogFieldTone::DamageValue);
        appendBattleLogSegment(entry, " 點生命");
        if (!event.detailText.empty())
        {
            appendBattleLogSegment(entry, "（");
            appendHighlightedLogText(entry, event.detailText, BattleLogFieldTone::SkillName);
            appendBattleLogSegment(entry, "）");
        }
        break;
    case BattleReportEventType::Status:
        entry.category = classifyStatusEvent(event.detailText);
        entry.tone = teamToEntryTone(event.targetTeam >= 0 ? event.targetTeam : event.sourceTeam);
        if (entry.tone == BattleLogEntryTone::Neutral)
        {
            entry.tone = BattleLogEntryTone::System;
        }
        addFrame();
        if (!event.sourceName.empty() && !event.targetName.empty() && event.sourceId != event.targetId)
        {
            appendBattleLogSegment(entry, event.sourceName, teamToFieldTone(event.sourceTeam));
            appendBattleLogSegment(entry, " 對 ");
            appendBattleLogSegment(entry, event.targetName, teamToFieldTone(event.targetTeam));
            appendBattleLogSegment(entry, "：");
        }
        else if (!event.targetName.empty())
        {
            appendBattleLogSegment(entry, event.targetName, teamToFieldTone(event.targetTeam));
            appendBattleLogSegment(entry, "：");
        }
        else if (!event.sourceName.empty())
        {
            appendBattleLogSegment(entry, event.sourceName, teamToFieldTone(event.sourceTeam));
            appendBattleLogSegment(entry, "：");
        }
        appendHighlightedLogText(entry, event.detailText, BattleLogFieldTone::SkillName);
        break;
    case BattleReportEventType::Kill:
        entry.category = BattleLogEntryCategory::Lifecycle;
        entry.tone = teamToEntryTone(event.sourceTeam);
        addFrame();
        appendBattleLogSegment(entry, event.sourceName, teamToFieldTone(event.sourceTeam));
        appendBattleLogSegment(entry, " 擊殺了 ");
        appendBattleLogSegment(entry, event.targetName, teamToFieldTone(event.targetTeam));
        break;
    case BattleReportEventType::Death:
        entry.category = BattleLogEntryCategory::Lifecycle;
        entry.tone = teamToEntryTone(event.targetTeam);
        addFrame();
        appendBattleLogSegment(entry, event.targetName, teamToFieldTone(event.targetTeam));
        appendBattleLogSegment(entry, " 倒下");
        break;
    case BattleReportEventType::BattleEnd:
        entry.category = BattleLogEntryCategory::BattleEnd;
        entry.tone = BattleLogEntryTone::System;
        addFrame();
        appendBattleLogSegment(entry, battleResultText(event.value), BattleLogFieldTone::SystemAccent);
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
        appendBattleLogSegment(
            omitted,
            std::format("前 {} 條記錄已省略", model.omittedEntries),
            BattleLogFieldTone::SystemAccent);
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
        appendBattleLogSegment(empty, "本場戰鬥沒有產生可記錄事件。", BattleLogFieldTone::SystemAccent);
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
