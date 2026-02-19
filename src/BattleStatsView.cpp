#include "BattleStatsView.h"
#include "BattleRoleManager.h"
#include "ChessBalance.h"
#include "Engine.h"
#include "Font.h"
#include "Save.h"
#include "TextureManager.h"

void BattleTracker::recordDamage(Role* attacker, Role* defender, int damage, const std::string& skillName)
{
    if (damage <= 0) return;
    if (attacker) {
        stats_[attacker->ID].damageDealt += damage;
        if (!skillName.empty())
            stats_[attacker->ID].damagePerSkill[skillName] += damage;
    }
    if (defender)
        stats_[defender->ID].damageTaken += damage;
}

void BattleTracker::recordKill(Role* killer)
{
    if (killer)
        stats_[killer->ID].kills++;
}

static BattleStatsView::RoleEntry makeEntry(Role* role, int star, int team)
{
    BattleStatsView::RoleEntry e;
    e.role = role;
    e.star = star;
    e.team = team;
    auto& cfg = KysChess::ChessBalance::config();
    double hpAtkM = 1.0 + cfg.starHPAtkMult * star;
    double defM = 1.0 + cfg.starDefMult * star;
    double spdM = 1.0 + cfg.starSpdMult * star;
    e.hp = static_cast<int>(role->MaxHP * hpAtkM);
    e.atk = static_cast<int>(role->Attack * hpAtkM);
    e.def = static_cast<int>(role->Defence * defM);
    e.spd = static_cast<int>(role->Speed * spdM);
    // Collect skill names
    for (int i = 0; i < 4; i++)
    {
        auto m = Save::getInstance()->getRoleLearnedMagic(role, i);
        if (m)
        {
            if (!e.skillNames.empty()) e.skillNames += " ";
            e.skillNames += m->Name;
        }
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
    ce.totalMembers = combo.memberRoleIds.size();
    if (ac.activeThresholdIdx >= 0 && ac.activeThresholdIdx < (int)combo.thresholds.size())
        ce.thresholdName = combo.thresholds[ac.activeThresholdIdx].name;
    return ce;
}

void BattleStatsView::setupPreBattle(
    const std::vector<KysChess::Chess>& allies,
    const std::vector<int>& enemyIds,
    const std::vector<KysChess::ActiveCombo>& allyC,
    const std::vector<KysChess::ActiveCombo>& enemyC)
{
    isPreBattle_ = true;
    allies_.clear(); enemies_.clear();
    for (auto& c : allies)
        if (c.role) allies_.push_back(makeEntry(c.role, c.star, 0));
    for (int id : enemyIds)
    {
        auto r = Save::getInstance()->getRole(id);
        if (r) enemies_.push_back(makeEntry(r, 0, 1));
    }
    allyCombos_.clear(); enemyCombos_.clear();
    for (auto& ac : allyC)
        if (ac.activeThresholdIdx >= 0) allyCombos_.push_back(makeComboEntry(ac));
    for (auto& ac : enemyC)
        if (ac.activeThresholdIdx >= 0) enemyCombos_.push_back(makeComboEntry(ac));
}

void BattleStatsView::setupPostBattle(
    const std::vector<KysChess::Chess>& allies,
    const std::vector<int>& enemyIds,
    const BattleTracker& tracker,
    int battleResult)
{
    isPreBattle_ = false;
    battleResult_ = battleResult;
    allies_.clear(); enemies_.clear();
    auto& stats = tracker.getStats();
    auto fillPost = [&](RoleEntry& e) {
        auto it = stats.find(e.role->ID);
        if (it != stats.end())
        {
            e.damageDealt = it->second.damageDealt;
            e.damageTaken = it->second.damageTaken;
            e.kills = it->second.kills;
            // Top 2 skills by damage
            for (auto& [name, dmg] : it->second.damagePerSkill)
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
    for (auto& c : allies)
    {
        if (!c.role) continue;
        auto e = makeEntry(c.role, c.star, 0);
        fillPost(e);
        allies_.push_back(e);
    }
    for (int id : enemyIds)
    {
        auto r = Save::getInstance()->getRole(id);
        if (!r) continue;
        auto e = makeEntry(r, 0, 1);
        fillPost(e);
        enemies_.push_back(e);
    }
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
    int cName = 46, cStar = 130, cHP = 160, cAtk = 220, cDef = 270, cSpd = 320, cSkill = 365;
    int cDmg = 130, cTaken = 200, cKill = 270, cSk1 = 300;

    if (!showPost)
    {
        font->draw("名稱", fs, x + cName, y, cGray);
        font->draw("★", fs, x + cStar, y, cGray);
        font->draw("HP", fs, x + cHP, y, cGray);
        font->draw("攻", fs, x + cAtk, y, cGray);
        font->draw("防", fs, x + cDef, y, cGray);
        font->draw("速", fs, x + cSpd, y, cGray);
        font->draw("武學", fs, x + cSkill, y, cGray);
    }
    else
    {
        font->draw("名稱", fs, x + cName, y, cGray);
        font->draw("輸出", fs, x + cDmg, y, cGray);
        font->draw("承傷", fs, x + cTaken, y, cGray);
        font->draw("殺", fs, x + cKill, y, cGray);
        font->draw("技能", fs - 2, x + cSk1, y, cGray);
    }
    y += 26;

    for (auto& e : team)
    {
        if (!e.role) continue;
        // Avatar scaled to ~40px tall
        auto tex = TextureManager::getInstance()->getTexture("head", e.role->HeadID);
        if (tex)
            TextureManager::getInstance()->renderTexture(tex, x, y - 2, cWhite, 255, 0.22, 0.22);

        Color nameCol = e.team == 0 ? cGreen : cRed;
        font->draw(std::string(e.role->Name), fs, x + cName, y, nameCol);

        if (!showPost)
        {
            std::string stars = e.star > 0 ? std::string(e.star, '*') : "-";
            font->draw(stars, fs, x + cStar, y, {255, 215, 0, 255});
            font->draw(std::to_string(e.hp), fs, x + cHP, y, cWhite);
            font->draw(std::to_string(e.atk), fs, x + cAtk, y, cWhite);
            font->draw(std::to_string(e.def), fs, x + cDef, y, cWhite);
            font->draw(std::to_string(e.spd), fs, x + cSpd, y, cWhite);
            font->draw(e.skillNames, fs - 4, x + cSkill, y + 2, cGray);
        }
        else
        {
            font->draw(std::to_string(e.damageDealt), fs, x + cDmg, y, cWhite);
            font->draw(std::to_string(e.damageTaken), fs, x + cTaken, y, cWhite);
            font->draw(std::to_string(e.kills), fs, x + cKill, y, cWhite);
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
    Color cGold = {255, 215, 0, 255};
    Color cActive = {0, 255, 100, 255};
    for (auto& c : combos)
    {
        font->draw(std::format("{} ({}/{}) {}", c.name, c.memberCount, c.totalMembers, c.thresholdName),
            16, x, y, cActive);
        y += 20;
    }
}

void BattleStatsView::draw()
{
    auto engine = Engine::getInstance();
    int uiW = engine->getUIWidth();
    int uiH = engine->getUIHeight();

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
        std::string titleText = battleResult_ == 0 ? "戰鬥勝利" : "戰鬥失敗";
        Color titleCol = battleResult_ == 0 ? Color{100, 255, 100, 255} : Color{255, 100, 100, 255};
        font->draw(titleText, 28, cx(titleText, 28), 10, titleCol);

        drawTeamTable(allies_, 10, 45, halfW - 20, "我方戰績", true);
        drawTeamTable(enemies_, halfW + 10, 45, halfW - 20, "敵方戰績", true);

        {
            std::string ft = "按任意鍵繼續";
            font->draw(ft, 20, cx(ft, 20), uiH - 35, {200, 200, 200, 255});
        }
    }
}

void BattleStatsView::dealEvent(EngineEvent& e)
{
    if (e.type == EVENT_KEY_UP || e.type == EVENT_GAMEPAD_BUTTON_UP
        || (e.type == EVENT_MOUSE_BUTTON_UP))
    {
        setExit(true);
    }
}
