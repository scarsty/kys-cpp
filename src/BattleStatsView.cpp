#include "BattleStatsView.h"
#include "BattleRoleManager.h"
#include <algorithm>
#include "Engine.h"
#include "Font.h"
#include "Save.h"
#include "TextureManager.h"
#include "GameUtil.h"
#include "Audio.h"
#include "ChessUiCommon.h"
#include "ChessEftIds.h"
#include "ImGuiLayer.h"
#include "ScenePreloader.h"
#include <format>
#include <set>
#include <unordered_map>

namespace
{
constexpr int kMaxBattleLogEntries = 9999;

BattleLogTone teamToBattleLogTone(int team)
{
    if (team == 0) return BattleLogTone::Ally;
    if (team == 1) return BattleLogTone::Enemy;
    return BattleLogTone::Neutral;
}

BattleLogFieldTone teamToFieldTone(int team)
{
    if (team == 0) return BattleLogFieldTone::AllyName;
    if (team == 1) return BattleLogFieldTone::EnemyName;
    return BattleLogFieldTone::Default;
}

void appendBattleLogSegment(BattleLogLine& line, std::string text, BattleLogFieldTone tone = BattleLogFieldTone::Default)
{
    if (!text.empty())
    {
        line.segments.push_back({std::move(text), tone});
    }
}

std::string battleResultText(int battleResult)
{
    return battleResult == 0 ? "戰鬥勝利" : (battleResult == 1 ? "戰鬥失敗" : "戰鬥結束");
}

std::unordered_map<int, std::string> buildDistinctRoleLabels(const std::vector<BattleStatsView::RoleEntry>& team)
{
    std::unordered_map<std::string, int> totals;
    std::unordered_map<int, std::string> labels;
    for (const auto& entry : team)
    {
        if (entry.role)
        {
            totals[entry.role->Name]++;
        }
    }

    std::unordered_map<std::string, int> seen;
    for (const auto& entry : team)
    {
        if (!entry.role)
        {
            continue;
        }

        const std::string baseName = entry.role->Name;
        const int instanceIndex = ++seen[baseName];
        labels[entry.battleId] = totals[baseName] > 1
            ? std::format("{} [{}]", baseName, instanceIndex)
            : baseName;
    }

    return labels;
}

void setLineParticipants(BattleLogLine& line, const BattleLogEvent& event)
{
    line.sourceId = event.sourceId;
    line.targetId = event.targetId;
    line.sourceTeam = event.sourceTeam;
    line.targetTeam = event.targetTeam;
}

BattleLogLine buildBattleLogLine(const BattleLogEvent& event)
{
    BattleLogLine line;
    setLineParticipants(line, event);

    auto addFrame = [&]() {
        appendBattleLogSegment(line, std::format("[{:>4}F] ", event.frame), BattleLogFieldTone::SystemAccent);
    };

    switch (event.type)
    {
    case BattleLogEventType::Damage:
        line.tone = teamToBattleLogTone(event.sourceTeam);
        addFrame();
        appendBattleLogSegment(line, event.sourceName, teamToFieldTone(event.sourceTeam));
        appendBattleLogSegment(line, event.skillName.empty() ? " 攻擊 " : " 施放 ");
        if (!event.skillName.empty())
        {
            appendBattleLogSegment(line, event.skillName, BattleLogFieldTone::SkillName);
            appendBattleLogSegment(line, " 命中 ");
        }
        appendBattleLogSegment(line, event.targetName, teamToFieldTone(event.targetTeam));
        appendBattleLogSegment(line, " ，造成 ");
        appendBattleLogSegment(line, std::to_string(event.value), BattleLogFieldTone::DamageValue);
        appendBattleLogSegment(line, " 點傷害");
        if (!event.detailText.empty())
        {
            appendBattleLogSegment(line, "（");
            appendBattleLogSegment(line, event.detailText, BattleLogFieldTone::SkillName);
            appendBattleLogSegment(line, "）");
        }
        break;
    case BattleLogEventType::Heal:
        line.tone = teamToBattleLogTone(event.targetTeam >= 0 ? event.targetTeam : event.sourceTeam);
        addFrame();
        if (!event.sourceName.empty() && !event.targetName.empty() && event.sourceId != event.targetId)
        {
            appendBattleLogSegment(line, event.sourceName, teamToFieldTone(event.sourceTeam));
            appendBattleLogSegment(line, " 为 ");
            appendBattleLogSegment(line, event.targetName, teamToFieldTone(event.targetTeam));
        }
        else if (!event.targetName.empty())
        {
            appendBattleLogSegment(line, event.targetName, teamToFieldTone(event.targetTeam));
        }
        else
        {
            appendBattleLogSegment(line, event.sourceName, teamToFieldTone(event.sourceTeam));
        }
        appendBattleLogSegment(line, " 恢復 ");
        appendBattleLogSegment(line, std::to_string(event.value), BattleLogFieldTone::DamageValue);
        appendBattleLogSegment(line, " 點生命");
        if (!event.detailText.empty())
        {
            appendBattleLogSegment(line, "（");
            appendBattleLogSegment(line, event.detailText, BattleLogFieldTone::SkillName);
            appendBattleLogSegment(line, "）");
        }
        break;
    case BattleLogEventType::Status:
        line.tone = teamToBattleLogTone(event.targetTeam >= 0 ? event.targetTeam : event.sourceTeam);
        if (line.tone == BattleLogTone::Neutral)
        {
            line.tone = BattleLogTone::System;
        }
        addFrame();
        if (!event.sourceName.empty() && !event.targetName.empty() && event.sourceId != event.targetId)
        {
            appendBattleLogSegment(line, event.sourceName, teamToFieldTone(event.sourceTeam));
            appendBattleLogSegment(line, " 對 ");
            appendBattleLogSegment(line, event.targetName, teamToFieldTone(event.targetTeam));
            appendBattleLogSegment(line, "：");
        }
        else if (!event.targetName.empty())
        {
            appendBattleLogSegment(line, event.targetName, teamToFieldTone(event.targetTeam));
            appendBattleLogSegment(line, "：");
        }
        else if (!event.sourceName.empty())
        {
            appendBattleLogSegment(line, event.sourceName, teamToFieldTone(event.sourceTeam));
            appendBattleLogSegment(line, "：");
        }
        appendBattleLogSegment(line, event.detailText, BattleLogFieldTone::SkillName);
        break;
    case BattleLogEventType::Kill:
        line.tone = teamToBattleLogTone(event.sourceTeam);
        addFrame();
        appendBattleLogSegment(line, event.sourceName, teamToFieldTone(event.sourceTeam));
        appendBattleLogSegment(line, " 擊殺了 ");
        appendBattleLogSegment(line, event.targetName, teamToFieldTone(event.targetTeam));
        break;
    case BattleLogEventType::Death:
        line.tone = teamToBattleLogTone(event.targetTeam);
        addFrame();
        appendBattleLogSegment(line, event.targetName, teamToFieldTone(event.targetTeam));
        appendBattleLogSegment(line, " 倒下");
        break;
    case BattleLogEventType::BattleEnd:
        line.tone = BattleLogTone::System;
        addFrame();
        appendBattleLogSegment(line, battleResultText(event.value), BattleLogFieldTone::SystemAccent);
        break;
    }

    return line;
}
}

BattleStatsView::~BattleStatsView()
{
    clearPostBattleBackground();
}

void BattleStatsView::clearPostBattleBackground()
{
    if (postBattleBackground_)
    {
        Engine::destroyTexture(postBattleBackground_);
        postBattleBackground_ = nullptr;
    }
}

void BattleStatsView::queuePostBattleLogOpen()
{
    if (!isPreBattle_ && !postBattleLogShown_ && postBattleLogOpenFrame_ < 0)
    {
        postBattleLogOpenFrame_ = current_frame_ + 1;
    }
}

void BattleTracker::recordDamage(Role* attacker, Role* defender, int damage, const std::string& skillName, int frame, const std::string& detailText)
{
    if (battleResult_ != -1) return;
    // if (damage <= 0) return;
    if (attacker) {
        auto& s = stats_[attacker->ID];
        s.damageDealt += damage;
        if (s.firstDamageFrame == 0) s.firstDamageFrame = frame;
        s.lastActiveFrame = frame;
        if (!skillName.empty())
            s.damagePerSkill[skillName] += damage;
    }
    if (defender)
        stats_[defender->ID].damageTaken += damage;

    BattleLogEvent event;
    event.type = BattleLogEventType::Damage;
    event.frame = frame;
    event.value = damage;
    if (attacker)
    {
        event.sourceId = attacker->ID;
        event.sourceTeam = attacker->Team;
        event.sourceName = attacker->Name;
    }
    if (defender)
    {
        event.targetId = defender->ID;
        event.targetTeam = defender->Team;
        event.targetName = defender->Name;
    }
    event.skillName = skillName;
    event.detailText = detailText;
    events_.push_back(std::move(event));
}

void BattleTracker::recordHeal(Role* source, Role* target, int amount, const std::string& reason, int frame)
{
    if (battleResult_ != -1) return;
    if (amount <= 0)
    {
        return;
    }

    if (source)
    {
        stats_[source->ID].lastActiveFrame = frame;
    }

    BattleLogEvent event;
    event.type = BattleLogEventType::Heal;
    event.frame = frame;
    event.value = amount;
    event.detailText = reason;
    if (source)
    {
        event.sourceId = source->ID;
        event.sourceTeam = source->Team;
        event.sourceName = source->Name;
    }
    if (target)
    {
        event.targetId = target->ID;
        event.targetTeam = target->Team;
        event.targetName = target->Name;
    }
    events_.push_back(std::move(event));
}

void BattleTracker::recordStatus(Role* source, Role* target, const std::string& text, int frame)
{
    if (battleResult_ != -1) return;
    if (text.empty())
    {
        return;
    }

    if (source)
    {
        stats_[source->ID].lastActiveFrame = frame;
    }

    BattleLogEvent event;
    event.type = BattleLogEventType::Status;
    event.frame = frame;
    event.detailText = text;
    if (source)
    {
        event.sourceId = source->ID;
        event.sourceTeam = source->Team;
        event.sourceName = source->Name;
    }
    if (target)
    {
        event.targetId = target->ID;
        event.targetTeam = target->Team;
        event.targetName = target->Name;
    }
    events_.push_back(std::move(event));
}

void BattleTracker::recordKill(Role* killer, Role* victim, int frame)
{
    if (battleResult_ != -1) return;
    if (killer)
        stats_[killer->ID].kills++;

    BattleLogEvent event;
    event.type = BattleLogEventType::Kill;
    event.frame = frame;
    if (killer)
    {
        event.sourceId = killer->ID;
        event.sourceTeam = killer->Team;
        event.sourceName = killer->Name;
    }
    if (victim)
    {
        event.targetId = victim->ID;
        event.targetTeam = victim->Team;
        event.targetName = victim->Name;
    }
    events_.push_back(std::move(event));
}

void BattleTracker::recordDeath(Role* role, int frame)
{
    if (battleResult_ != -1) return;
    if (role)
        stats_[role->ID].lastActiveFrame = frame;

    if (!role)
    {
        return;
    }

    BattleLogEvent event;
    event.type = BattleLogEventType::Death;
    event.frame = frame;
    event.targetId = role->ID;
    event.targetTeam = role->Team;
    event.targetName = role->Name;
    events_.push_back(std::move(event));
}

void BattleTracker::recordBattleEnd(int frame, int battleResult)
{
    battleEndFrame_ = frame;
    battleResult_ = battleResult;

    BattleLogEvent event;
    event.type = BattleLogEventType::BattleEnd;
    event.frame = frame;
    event.value = battleResult;
    events_.push_back(std::move(event));
}

static BattleStatsView::RoleEntry makeEntry(Role* role, int star, int team, int chessInstanceId = -1)
{
    BattleStatsView::RoleEntry e;
    e.role = role;
    e.star = star;
    e.team = team;
    e.chessInstanceId = chessInstanceId;
    e.hpRemaining = role ? role->HP : 0;
    e.maxHpRemaining = role ? role->MaxHP : 0;
    e.dead = role ? role->Dead != 0 : false;
    auto s = KysChess::BattleRoleManager::computeStarStats(role, star);
    e.hp = s.hp;
    e.atk = s.atk;
    e.def = s.def;
    e.spd = s.spd;
    // Collect skill names
    auto magics = role->getLearnedMagics(star);
    for (auto m : magics)
    {
        if (!e.skillNames.empty()) e.skillNames += " ";
        e.skillNames += m->Name;
    }
    return e;
}

static BattleStatsView::ComboEntry makeComboEntry(const KysChess::ActiveCombo& ac)
{
    BattleStatsView::ComboEntry ce;
    auto& allCombos = KysChess::ChessCombo::getAllCombos();
    auto& combo = allCombos[(int)ac.id];
    ce.name = combo.name;
    ce.memberCount = ac.memberCount;
    ce.physicalMemberCount = ac.physicalMemberCount;
    ce.totalMembers = (int)combo.memberRoleIds.size();
    ce.starSynergyBonus = combo.starSynergyBonus;
    ce.isAntiCombo = combo.isAntiCombo;
    if (ac.activeThresholdIdx >= 0 && ac.activeThresholdIdx < (int)combo.thresholds.size())
    {
        auto& thresh = combo.thresholds[ac.activeThresholdIdx];
        ce.thresholdName = thresh.name;
        ce.thresholdCount = thresh.count;
    }
    return ce;
}

void BattleStatsView::setupPreBattle(
    const std::vector<KysChess::Chess>& allies,
    const std::vector<int>& enemyIds,
    const std::vector<int>& enemyStars,
    const std::vector<KysChess::ActiveCombo>& allyCombos,
    const std::vector<KysChess::ActiveCombo>& enemyCombos,
    int battleId,
    int musicId,
    const std::vector<int>& enemyWeapons,
    const std::vector<int>& enemyArmors,
    const std::vector<int>& allyWeapons,
    const std::vector<int>& allyArmors)
{
    isPreBattle_ = true;
    full_window_ = 0;
    assetsPreloaded_ = false;
    loadingTextRendered_ = false;
    battleId_ = battleId;
    musicId_ = musicId;
    clearPostBattleBackground();
    postBattleLogShown_ = false;
    postBattleLogOpenFrame_ = -1;
    postBattleMouseReleaseArmed_ = false;
    postBattleKeyboardReleaseArmed_ = false;
    postBattleGamepadReleaseArmed_ = false;
    battleLogTotalFrames_ = 0;
    battleLogEvents_.clear();
    allies_.clear(); enemies_.clear();
    for (size_t i = 0; i < allies.size(); ++i)
    {
        auto& c = allies[i];
        if (c.role) {
            auto e = makeEntry(c.role, c.star, 0, c.id.value);
            if (i < allyWeapons.size()) e.weaponId = allyWeapons[i];
            if (i < allyArmors.size()) e.armorId = allyArmors[i];
            allies_.push_back(e);
        }
    }
    for (size_t i = 0; i < enemyIds.size(); ++i)
    {
        auto r = roleSave_.getRole(enemyIds[i]);
        int star = (i < enemyStars.size()) ? enemyStars[i] : 1;
        if (r) {
            auto e = makeEntry(r, star, 1);
            if (i < enemyWeapons.size()) e.weaponId = enemyWeapons[i];
            if (i < enemyArmors.size()) e.armorId = enemyArmors[i];
            enemies_.push_back(e);
        }
    }
    allyCombos_.clear(); enemyCombos_.clear();
    for (auto& ac : allyCombos) {
        if (ac.activeThresholdIdx >= 0) allyCombos_.push_back(makeComboEntry(ac));
    }

    for (auto& ac : enemyCombos) {
        if (ac.activeThresholdIdx >= 0) enemyCombos_.push_back(makeComboEntry(ac));
    }
        
}

void BattleStatsView::setupPostBattle(
    const std::deque<Role>& allyBattleCopies,
    const std::deque<Role>& enemyBattleCopies,
    const BattleTracker& tracker,
    int battleResult)
{
    isPreBattle_ = false;
    full_window_ = 1;
    assetsPreloaded_ = false;
    loadingTextRendered_ = false;
    battleId_ = -1;
    battleResult_ = battleResult;
    clearPostBattleBackground();
    postBattleBackground_ = Engine::getInstance()->cloneTexture(Engine::getInstance()->getMainTexture());
    postBattleLogShown_ = false;
    postBattleLogOpenFrame_ = -1;
    postBattleMouseReleaseArmed_ = false;
    postBattleKeyboardReleaseArmed_ = false;
    postBattleGamepadReleaseArmed_ = false;
    battleLogEvents_ = tracker.getEvents();
    allies_.clear(); enemies_.clear();
    auto& stats = tracker.getStats();
    int endFrame = tracker.getBattleEndFrame();
    battleLogTotalFrames_ = endFrame;
    auto fillPost = [&](RoleEntry& e, int battleId) {
        e.battleId = battleId;
        auto it = stats.find(battleId);
        if (it != stats.end())
        {
            auto& s = it->second;
            e.damageDealt = s.damageDealt;
            e.damageTaken = s.damageTaken;
            e.kills = s.kills;
            int last = s.lastActiveFrame ? s.lastActiveFrame : endFrame;
            if (s.damageDealt > 0 && last > 0)
                e.dps = s.damageDealt * 60 / last;
            for (auto& [name, dmg] : s.damagePerSkill)
            {
                if (dmg > e.skill1Dmg)
                {
                    e.skill2 = e.skill1; e.skill2Dmg = e.skill1Dmg;
                    e.skill1 = name; e.skill1Dmg = dmg;
                }
                else if (dmg > e.skill2Dmg)
                {
                    e.skill2 = name; e.skill2Dmg = dmg;
                }
            }
        }
    };
    for (auto& r : allyBattleCopies)
    {
        auto orig = roleSave_.getRole(r.RealID);
        if (!orig) continue;
        auto e = makeEntry(orig, 1, 0);
        fillPost(e, r.ID);
        e.cancelDmg = r.CancelDmg;
        e.hpRemaining = r.HP;
        e.maxHpRemaining = r.MaxHP;
        e.dead = r.Dead != 0;
        allies_.push_back(e);
    }
    for (auto& r : enemyBattleCopies)
    {
        auto orig = roleSave_.getRole(r.RealID);
        if (!orig) continue;
        auto e = makeEntry(orig, 1, 1);
        fillPost(e, r.ID);
        e.cancelDmg = r.CancelDmg;
        e.hpRemaining = r.HP;
        e.maxHpRemaining = r.MaxHP;
        e.dead = r.Dead != 0;
        enemies_.push_back(e);
    }
    auto byDmg = [](const RoleEntry& a, const RoleEntry& b) { return a.damageDealt > b.damageDealt; };
    std::sort(allies_.begin(), allies_.end(), byDmg);
    std::sort(enemies_.begin(), enemies_.end(), byDmg);
}

void BattleStatsView::showPostBattleLog()
{
    BattleLogData data;
    data.title = "本場戰鬥日誌";
    data.resultText = battleResultText(battleResult_);
    data.totalFrames = battleLogTotalFrames_;
    const auto allyLabels = buildDistinctRoleLabels(allies_);
    const auto enemyLabels = buildDistinctRoleLabels(enemies_);

    auto resolveRoleLabel = [&](int team, int battleId, const std::string& fallback) -> std::string
    {
        const auto& labels = team == 0 ? allyLabels : enemyLabels;
        auto it = labels.find(battleId);
        return it != labels.end() ? it->second : fallback;
    };

    auto appendRoleRows = [&](const std::vector<RoleEntry>& team, std::vector<BattleLogRoleRow>& rows)
    {
        rows.reserve(team.size());
        for (const auto& entry : team)
        {
            if (!entry.role)
            {
                continue;
            }

            BattleLogRoleRow row;
            row.id = entry.battleId;
            row.name = resolveRoleLabel(entry.team, entry.battleId, entry.role->Name);
            row.team = entry.team;
            row.damageDealt = entry.damageDealt;
            row.damageTaken = entry.damageTaken;
            row.kills = entry.kills;
            row.cancelDmg = entry.cancelDmg;
            row.hpRemaining = entry.hpRemaining;
            row.maxHp = entry.maxHpRemaining;
            row.dead = entry.dead;
            rows.push_back(std::move(row));
        }
    };

    appendRoleRows(allies_, data.allies);
    appendRoleRows(enemies_, data.enemies);

    int omitted = 0;
    const int eventCount = static_cast<int>(battleLogEvents_.size());
    int startIndex = 0;
    if (eventCount > kMaxBattleLogEntries)
    {
        omitted = eventCount - kMaxBattleLogEntries;
        startIndex = omitted;
    }
    data.omittedEntries = omitted;

    if (omitted > 0)
    {
        BattleLogLine omittedLine;
        omittedLine.tone = BattleLogTone::System;
        appendBattleLogSegment(omittedLine, std::format("前 {} 條記錄已省略", omitted), BattleLogFieldTone::SystemAccent);
        data.entries.push_back(std::move(omittedLine));
    }

    data.entries.reserve(data.entries.size() + eventCount - startIndex);
    for (int i = startIndex; i < eventCount; ++i)
    {
        auto event = battleLogEvents_[i];
        if (event.sourceId >= 0)
        {
            event.sourceName = resolveRoleLabel(event.sourceTeam, event.sourceId, event.sourceName);
        }
        if (event.targetId >= 0)
        {
            event.targetName = resolveRoleLabel(event.targetTeam, event.targetId, event.targetName);
        }
        data.entries.push_back(buildBattleLogLine(event));
    }

    if (data.entries.empty())
    {
        BattleLogLine emptyLine;
        emptyLine.tone = BattleLogTone::System;
        appendBattleLogSegment(emptyLine, "本場戰鬥沒有產生可記錄事件。", BattleLogFieldTone::SystemAccent);
        data.entries.push_back(std::move(emptyLine));
    }

    Engine::getInstance()->showBattleLogWindow(data);
}

void BattleStatsView::drawTeamTable(const std::vector<RoleEntry>& team, int x, int y, int w, const std::string& title, bool showPost)
{
    auto font = Font::getInstance();
    int fs = 20;
    int rowH = 44;
    Color cWhite = {255, 255, 255, 255};
    Color cGray = {180, 180, 180, 255};
    Color cGreen = {100, 255, 100, 255};
    Color cRed = {255, 100, 100, 255};

    font->draw(title, 24, x, y, {255, 215, 0, 255});
    y += 30;

    // Column offsets relative to x
    int cName = 46, cStar = 130, cHP = 160, cAtk = 220, cDef = 270, cSpd = 320, cEquip = 365, cSkill = 405;
    int cDmg = 130, cDps = 200, cTaken = 250, cKill = 310, cCancel = 340, cSk1 = 400;

    if (!showPost)
    {
        font->draw("名稱", fs, x + cName, y, cGray);
        font->draw("★", fs, x + cStar, y, cGray);
        font->draw("HP", fs, x + cHP, y, cGray);
        font->draw("攻", fs, x + cAtk, y, cGray);
        font->draw("防", fs, x + cDef, y, cGray);
        font->draw("速", fs, x + cSpd, y, cGray);
        font->draw("裝", fs, x + cEquip, y, cGray);
        font->draw("武學", fs, x + cSkill, y, cGray);
    }
    else
    {
        font->draw("名稱", fs, x + cName, y, cGray);
        font->draw("輸出", fs, x + cDmg, y, cGray);
        font->draw("DPS", fs, x + cDps, y, cGray);
        font->draw("承傷", fs, x + cTaken, y, cGray);
        font->draw("殺", fs, x + cKill, y, cGray);
        font->draw("抵消", fs, x + cCancel, y, cGray);
        font->draw("技能", fs - 2, x + cSk1, y, cGray);
    }
    y += 26;

    for (auto& e : team)
    {
        if (!e.role) continue;
        // Avatar scaled to ~40px tall
        auto tex = TextureManager::getInstance()->getTexture("head", e.role->HeadID);
        if (tex)
            TextureManager::getInstance()->renderTexture(tex, x, y - 2, TextureManager::RenderInfo{ cWhite, 255, 0.22, 0.22 });

        Color nameCol = e.team == 0 ? cGreen : cRed;
        font->draw(std::string(e.role->Name), fs, x + cName, y, nameCol);

        if (!showPost)
        {
            std::string stars = std::to_string(e.star);
            font->draw(stars, fs, x + cStar, y, {255, 215, 0, 255});
            font->draw(std::to_string(e.hp), fs, x + cHP, y, cWhite);
            font->draw(std::to_string(e.atk), fs, x + cAtk, y, cWhite);
            font->draw(std::to_string(e.def), fs, x + cDef, y, cWhite);
            font->draw(std::to_string(e.spd), fs, x + cSpd, y, cWhite);

            // Equipment icons
            auto chess = chessManager_.tryFindChessByInstanceId(KysChess::ChessInstanceID{e.chessInstanceId});
            if (chess)
            {
                int weaponId = e.weaponId >= 0 ? e.weaponId : chess->weaponInstance.itemId;
                int armorId = e.armorId >= 0 ? e.armorId : chess->armorInstance.itemId;
                if (weaponId >= 0)
                    TextureManager::getInstance()->renderTexture("item", weaponId, x + cEquip, y, TextureManager::RenderInfo{ cWhite, 255, 0.16, 0.16 });
                if (armorId >= 0)
                    TextureManager::getInstance()->renderTexture("item", armorId, x + cEquip + 18, y, TextureManager::RenderInfo{ cWhite, 255, 0.16, 0.16 });
            }
            else if (e.weaponId >= 0 || e.armorId >= 0)
            {
                if (e.weaponId >= 0)
                    TextureManager::getInstance()->renderTexture("item", e.weaponId, x + cEquip, y, TextureManager::RenderInfo{ cWhite, 255, 0.16, 0.16 });
                if (e.armorId >= 0)
                    TextureManager::getInstance()->renderTexture("item", e.armorId, x + cEquip + 18, y, TextureManager::RenderInfo{ cWhite, 255, 0.16, 0.16 });
            }

            font->draw(e.skillNames, fs - 4, x + cSkill, y + 2, cGray);
        }
        else
        {
            font->draw(std::to_string(e.damageDealt), fs, x + cDmg, y, cWhite);
            font->draw(std::to_string(e.dps), fs, x + cDps, y, {255, 200, 50, 255});
            font->draw(std::to_string(e.damageTaken), fs, x + cTaken, y, cWhite);
            font->draw(std::to_string(e.kills), fs, x + cKill, y, cWhite);
            if (e.cancelDmg > 0)
                font->draw(std::to_string(e.cancelDmg), fs, x + cCancel, y, {200, 200, 255, 255});
            if (!e.skill1.empty())
                font->draw(std::format("{}({})", e.skill1, e.skill1Dmg), fs - 4, x + cSk1, y, cGray);
            if (!e.skill2.empty())
                font->draw(std::format("{}({})", e.skill2, e.skill2Dmg), fs - 4, x + cSk1, y + 20, cGray);
        }
        y += rowH;
    }
}

void BattleStatsView::drawComboList(const std::vector<ComboEntry>& combos, int x, int y, int w)
{
    auto font = Font::getInstance();
    auto engine = Engine::getInstance();
    Color cActive = {0, 255, 100, 255};
    constexpr int kFontSize = 16;
    constexpr int kLineHeight = 20;
    constexpr int kColumnGap = 18;
    constexpr int kBottomReserve = 55;
    constexpr int kMinColumnWidth = 140;

    std::vector<std::string> comboLines;
    comboLines.reserve(combos.size());
    for (const auto& c : combos)
    {
        // For anti-combos use threshold count (1) as denominator; otherwise total pool size.
        int denom = c.isAntiCombo ? c.thresholdCount : c.totalMembers;
        std::string countStr = KysChess::formatComboCount(
            c.physicalMemberCount, c.memberCount, denom, c.starSynergyBonus, c.isAntiCombo);
        comboLines.push_back(std::format("{} ({}) {}", c.name, countStr, c.thresholdName));
    }

    int availableHeight = std::max(kLineHeight, engine->getUIHeight() - kBottomReserve - y);
    int maxColumns = std::min(
        static_cast<int>(comboLines.size()),
        std::max(1, (w + kColumnGap) / (kMinColumnWidth + kColumnGap)));

    auto buildWrappedEntries = [&](int columnWidth) {
        std::vector<std::vector<std::string>> entries;
        int availableUnits = std::max(6, (columnWidth - 8) * 2 / kFontSize);
        entries.reserve(comboLines.size());
        for (const auto& line : comboLines)
        {
            auto wrapped = KysChess::wrapDisplayText(line, availableUnits);
            if (wrapped.empty())
            {
                wrapped.push_back(line);
            }
            entries.push_back(std::move(wrapped));
        }
        return entries;
    };

    auto canFit = [&](const std::vector<std::vector<std::string>>& entries, int columns) {
        int usedColumns = 1;
        int currentHeight = 0;
        for (const auto& entry : entries)
        {
            int entryHeight = std::max(1, static_cast<int>(entry.size())) * kLineHeight;
            if (currentHeight > 0 && currentHeight + entryHeight > availableHeight)
            {
                ++usedColumns;
                currentHeight = 0;
            }
            if (usedColumns > columns)
            {
                return false;
            }
            currentHeight += entryHeight;
        }
        return true;
    };

    int chosenColumns = 1;
    std::vector<std::vector<std::string>> wrappedEntries;
    for (int columns = 1; columns <= maxColumns; ++columns)
    {
        int columnWidth = std::max(kMinColumnWidth, (w - (columns - 1) * kColumnGap) / columns);
        auto candidate = buildWrappedEntries(columnWidth);
        if (canFit(candidate, columns) || columns == maxColumns)
        {
            chosenColumns = columns;
            wrappedEntries = std::move(candidate);
            break;
        }
    }

    int columnWidth = std::max(kMinColumnWidth, (w - (chosenColumns - 1) * kColumnGap) / chosenColumns);
    int currentColumn = 0;
    int currentY = y;
    for (const auto& entry : wrappedEntries)
    {
        int entryHeight = std::max(1, static_cast<int>(entry.size())) * kLineHeight;
        if (currentY > y && currentY - y + entryHeight > availableHeight)
        {
            ++currentColumn;
            currentY = y;
        }
        if (currentColumn >= chosenColumns)
        {
            break;
        }

        int drawX = x + currentColumn * (columnWidth + kColumnGap);
        for (const auto& line : entry)
        {
            font->draw(line, kFontSize, drawX, currentY, cActive);
            currentY += kLineHeight;
        }
    }
}

void BattleStatsView::draw()
{
    auto engine = Engine::getInstance();
    int uiW = engine->getUIWidth();
    int uiH = engine->getUIHeight();

    if (!isPreBattle_ && postBattleBackground_)
    {
        engine->renderTexture(postBattleBackground_, 0, 0, uiW, uiH);
    }

    if (!isPreBattle_ && (postBattleLogShown_ || postBattleLogOpenFrame_ >= 0))
    {
        engine->fillColor({0, 0, 0, 120}, 0, 0, uiW, uiH);
        return;
    }

    // Full screen dark overlay
    engine->fillColor({0, 0, 0, 200}, 0, 0, uiW, uiH);

    auto font = Font::getInstance();
    int halfW = uiW / 2;

    // center text x position
    auto cx = [&](const std::string& text, int size) {
        return uiW / 2 - size * Font::getTextDrawSize(text) / 4;
    };

    if (isPreBattle_)
    {
        if (!assetsPreloaded_)
        {
            font->drawWithBoxCentered("載入戰鬥資源中...", 32, uiH / 2 - 16);
            loadingTextRendered_ = true;
        }

        std::string t = "戰前總覽";
        font->draw(t, 28, cx(t, 28), 10, {255, 215, 0, 255});

        // Left: allies
        drawTeamTable(allies_, 10, 45, halfW - 20, "我方", false);
        // Right: enemies
        drawTeamTable(enemies_, halfW + 10, 45, halfW - 20, "敵方", false);

        // Combos below tables
        int comboY = 45 + 30 + 26 + (int)std::max(allies_.size(), enemies_.size()) * 44 + 10;
        if (!allyCombos_.empty())
        {
            font->draw("我方羈絆", 20, 10, comboY, {255, 215, 0, 255});
            drawComboList(allyCombos_, 10, comboY + 24, halfW - 20);
        }
        if (!enemyCombos_.empty())
        {
            font->draw("敵方羈絆", 20, halfW + 10, comboY, {255, 215, 0, 255});
            drawComboList(enemyCombos_, halfW + 10, comboY + 24, halfW - 20);
        }

        {
            std::string ft = "按任意鍵開始戰鬥";
            font->draw(ft, 20, cx(ft, 20), uiH - 35, {200, 200, 200, 255});
        }
    }
    else
    {
        font->draw(std::format("戰鬥幀數 {}", battleLogTotalFrames_), 18, 20, 10, {180, 180, 180, 255});
        std::string titleText = battleResult_ == 0 ? "戰鬥勝利" : "戰鬥失敗";
        Color titleCol = battleResult_ == 0 ? Color{100, 255, 100, 255} : Color{255, 100, 100, 255};
        font->draw(titleText, 28, cx(titleText, 28), 10, titleCol);

        drawTeamTable(allies_, 10, 45, halfW - 20, "我方戰績", true);
        drawTeamTable(enemies_, halfW + 10, 45, halfW - 20, "敵方戰績", true);

        {
            std::string ft = postBattleLogShown_ ? "關閉戰鬥日誌後繼續" : "按任意鍵查看戰鬥日誌";
            font->draw(ft, 20, cx(ft, 20), uiH - 35, {200, 200, 200, 255});
        }
    }
}

void BattleStatsView::dealEvent(EngineEvent& e)
{
    if (isPreBattle_ && !assetsPreloaded_ && loadingTextRendered_)
    {
        std::vector<int> atkSounds, effSounds;
        auto preloadEffect = [&](int effectId)
        {
            if (effectId < 0)
                return;

            auto path = std::format("eft/eft{:03}", effectId);
            int frameCount = TextureManager::getInstance()->getTextureGroupCount(path);
            for (int frame = 0; frame < frameCount; ++frame)
            {
                auto* tex = TextureManager::getInstance()->getTexture(path, frame);
                if (tex)
                {
                    tex->load();
                }
            }
        };
        auto loadForRoles = [&](const std::vector<RoleEntry>& roles)
        {
            for (auto& re : roles)
            {
                if (!re.role)
                {
                    continue;
                }
                std::string text_group = std::format("fight/fight{:03}", re.role->HeadID);
                int frameCount = TextureManager::getInstance()->getTextureGroupCount(text_group);
                for (int frame = 0; frame < frameCount; ++frame)
                {
                    auto* tex = TextureManager::getInstance()->getTexture(text_group, frame);
                    if (tex)
                    {
                        tex->load();
                    }
                }
                auto magics = re.role->getLearnedMagics(re.star);
                for (auto m : magics) {
                    if (m->SoundID >= 0) atkSounds.push_back(m->SoundID);
                    if (m->EffectID >= 0)
                    {
                        effSounds.push_back(m->EffectID);
                        preloadEffect(m->EffectID);
                    }
                }
            }
        };

        loadForRoles(allies_);
        loadForRoles(enemies_);
        for (int i = 0; i < 5; i++)
        {
            auto* tex = TextureManager::getInstance()->getTexture(std::format("eft/bld{:03}", i), 0);
            if (tex)
            {
                tex->load();
            }
        }
        for (auto eftId : KysChess::EFT_ALL)
        {
            preloadEffect(std::to_underlying(eftId));
        }
        if (battleId_ >= 0)
        {
            ScenePreloader::preloadBattleAssets(battleId_);
        }
        Audio::getInstance()->preloadBattleAudio(musicId_, atkSounds, effSounds);

        assetsPreloaded_ = true;
        return;
    }

    if (isPreBattle_)
    {
        if (e.type == EVENT_KEY_UP || e.type == EVENT_GAMEPAD_BUTTON_UP
            || e.type == EVENT_MOUSE_BUTTON_UP)
        {
            setExit(true);
        }
        return;
    }

    if (postBattleLogShown_ || postBattleLogOpenFrame_ >= 0)
    {
        return;
    }

    if (e.type == EVENT_MOUSE_BUTTON_DOWN && e.button.button == BUTTON_LEFT)
    {
        postBattleMouseReleaseArmed_ = true;
        return;
    }

    if (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_LEFT)
    {
        if (postBattleMouseReleaseArmed_)
        {
            postBattleMouseReleaseArmed_ = false;
            queuePostBattleLogOpen();
        }
        return;
    }

    if (e.type == EVENT_KEY_DOWN && (e.key.key == K_RETURN || e.key.key == K_SPACE))
    {
        postBattleKeyboardReleaseArmed_ = true;
        return;
    }

    if (e.type == EVENT_KEY_UP && (e.key.key == K_RETURN || e.key.key == K_SPACE))
    {
        if (postBattleKeyboardReleaseArmed_)
        {
            postBattleKeyboardReleaseArmed_ = false;
            queuePostBattleLogOpen();
        }
        return;
    }

    if (e.type == EVENT_GAMEPAD_BUTTON_DOWN && e.gbutton.button == GAMEPAD_BUTTON_SOUTH)
    {
        postBattleGamepadReleaseArmed_ = true;
        return;
    }

    if (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_SOUTH)
    {
        if (postBattleGamepadReleaseArmed_)
        {
            postBattleGamepadReleaseArmed_ = false;
            queuePostBattleLogOpen();
        }
    }
}

void BattleStatsView::backRun()
{
    if (!isPreBattle_ && !postBattleLogShown_ && postBattleLogOpenFrame_ >= 0 && current_frame_ >= postBattleLogOpenFrame_)
    {
        postBattleLogShown_ = true;
        postBattleLogOpenFrame_ = -1;
        showPostBattleLog();
    }

    if (!isPreBattle_ && postBattleLogShown_ && !Engine::getInstance()->isBattleLogWindowOpen())
    {
        setExit(true);
    }
}
