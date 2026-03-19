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
#include <format>
#include <set>

void BattleTracker::recordDamage(Role* attacker, Role* defender, int damage, const std::string& skillName, int frame)
{
    if (damage <= 0) return;
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
}

void BattleTracker::recordKill(Role* killer)
{
    if (killer)
        stats_[killer->ID].kills++;
}

void BattleTracker::recordDeath(Role* role, int frame)
{
    if (role)
        stats_[role->ID].lastActiveFrame = frame;
}

void BattleTracker::recordBattleEnd(int frame)
{
    battleEndFrame_ = frame;
}

static BattleStatsView::RoleEntry makeEntry(Role* role, int star, int team, int chessInstanceId = -1)
{
    BattleStatsView::RoleEntry e;
    e.role = role;
    e.star = star;
    e.team = team;
    e.chessInstanceId = chessInstanceId;
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
    int musicId,
    const std::vector<int>& enemyWeapons,
    const std::vector<int>& enemyArmors,
    const std::vector<int>& allyWeapons,
    const std::vector<int>& allyArmors)
{
    isPreBattle_ = true;
    musicId_ = musicId;
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
    battleResult_ = battleResult;
    allies_.clear(); enemies_.clear();
    auto& stats = tracker.getStats();
    int endFrame = tracker.getBattleEndFrame();
    auto fillPost = [&](RoleEntry& e, int battleId) {
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
        allies_.push_back(e);
    }
    for (auto& r : enemyBattleCopies)
    {
        auto orig = roleSave_.getRole(r.RealID);
        if (!orig) continue;
        auto e = makeEntry(orig, 1, 1);
        fillPost(e, r.ID);
        e.cancelDmg = r.CancelDmg;
        enemies_.push_back(e);
    }
    auto byDmg = [](const RoleEntry& a, const RoleEntry& b) { return a.damageDealt > b.damageDealt; };
    std::sort(allies_.begin(), allies_.end(), byDmg);
    std::sort(enemies_.begin(), enemies_.end(), byDmg);
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
            TextureManager::getInstance()->renderTexture(tex, x, y - 2, cWhite, 255, 0.22, 0.22);

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
                    TextureManager::getInstance()->renderTexture("item", weaponId, x + cEquip, y, cWhite, 255, 0.16, 0.16);
                if (armorId >= 0)
                    TextureManager::getInstance()->renderTexture("item", armorId, x + cEquip + 18, y, cWhite, 255, 0.16, 0.16);
            }
            else if (e.weaponId >= 0 || e.armorId >= 0)
            {
                if (e.weaponId >= 0)
                    TextureManager::getInstance()->renderTexture("item", e.weaponId, x + cEquip, y, cWhite, 255, 0.16, 0.16);
                if (e.armorId >= 0)
                    TextureManager::getInstance()->renderTexture("item", e.armorId, x + cEquip + 18, y, cWhite, 255, 0.16, 0.16);
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
                TextureManager::getInstance()->getTexture(path, frame);
        };
        auto loadForRoles = [&](const std::vector<RoleEntry>& roles)
        {
            for (auto& re : roles)
            {
                if (!re.role)
                {
                    return;
                }
                std::string text_group = std::format("fight/fight{:03}", re.role->HeadID);
                TextureManager::getInstance()->getTextureGroup(text_group);
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
            TextureManager::getInstance()->getTexture(std::format("eft/bld{:03}", i), 0);
        }
        for (auto eftId : KysChess::EFT_ALL)
        {
            preloadEffect(std::to_underlying(eftId));
        }
        Audio::getInstance()->preloadBattleAudio(musicId_, atkSounds, effSounds);

        assetsPreloaded_ = true;
        return;
    }

    if (e.type == EVENT_KEY_UP || e.type == EVENT_GAMEPAD_BUTTON_UP
        || (e.type == EVENT_MOUSE_BUTTON_UP))
    {
        setExit(true);
    }
}
