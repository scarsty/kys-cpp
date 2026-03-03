#include "ChessSelector.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Audio.h"
#include "BattleStatsView.h"
#include "UISave.h"
#include "ChessBalance.h"
#include "ChessCombo.h"
#include "ChessNeigong.h"
#include "ChessPool.h"
#include "DynamicChessMap.h"
#include "DrawableOnCall.h"
#include "Font.h"
#include "InputBox.h"
#include "Menu.h"
#include "Random.h"
#include "Save.h"
#include "TempStore.h"
#include <Engine.h>
#include <SuperMenuText.h>
#include <TextureManager.h>
#include <UIRoleStatusMenu.h>
#include <string>
#include <vector>

namespace KysChess
{

int ChessSelector::calculateCost(int tier, int star, int count)
{
    auto& cfg = ChessBalance::config();
    return cfg.tierPrices[tier - 1] * static_cast<int>(std::pow(cfg.starCostMult, star - 1)) * count;
}

namespace
{

static std::string comboEffectDesc(const ComboEffect& eff)
{
    auto triggerPrefix = [&]() -> std::string {
        if (eff.trigger == Trigger::WhileLowHP) return std::format("血量<{}%: ", eff.triggerValue);
        if (eff.trigger == Trigger::AllyLowHPBurst) return std::format("血量<{}%狂暴({}幀): ", eff.triggerValue, eff.duration);
        if (eff.trigger == Trigger::LastAlive) return "最後存活: ";
        return "";
    };
    auto countSuffix = [&]() -> std::string {
        if (eff.maxCount > 0) return std::format(" ({}次)", eff.maxCount);
        return "";
    };
    std::string d;
    switch (eff.type)
    {
    case EffectType::FlatHP: d = std::format("生命+{}", eff.value); break;
    case EffectType::FlatATK: d = std::format("攻擊+{}", eff.value); break;
    case EffectType::FlatDEF: d = std::format("防禦+{}", eff.value); break;
    case EffectType::FlatSPD: d = std::format("速度+{}", eff.value); break;
    case EffectType::PctHP: d = std::format("生命+{}%", eff.value); break;
    case EffectType::PctATK: d = std::format("攻擊+{}%", eff.value); break;
    case EffectType::PctDEF: d = std::format("防禦+{}%", eff.value); break;
    case EffectType::NegPctDEF: d = std::format("防禦-{}%", eff.value); break;
    case EffectType::FlatDmgReduction: d = std::format("減傷{}", eff.value); break;
    case EffectType::BlockChance: d = std::format("{}%格擋", eff.value); break;
    case EffectType::DodgeChance: d = std::format("{}%閃避", eff.value); break;
    case EffectType::DodgeThenCrit: d = "閃避後暴擊"; break;
    case EffectType::CritChance: d = std::format("{}%暴擊", eff.value); break;
    case EffectType::CritMultiplier: d = std::format("暴擊{}%傷害", eff.value); break;
    case EffectType::EveryNthDouble: d = std::format("每{}次雙倍", eff.value); break;
    case EffectType::ArmorPenChance: d = std::format("{}%穿甲", eff.value); break;
    case EffectType::ArmorPenPct: d = std::format("無視%防禦", eff.value); break;
    case EffectType::ArmorPen: d = std::format("{}%穿甲(無視{}%防禦)", eff.value, eff.value2); break;
    case EffectType::StunChance: d = eff.value2 ? std::format("{}%眩暈({}幀)", eff.value, eff.value2) : std::format("{}%眩暈", eff.value); break;
    case EffectType::Stun: d = std::format("{}%眩暈({}幀)", eff.value, eff.value2); break;
    case EffectType::KnockbackChance: d = std::format("{}%擊退", eff.value); break;
    case EffectType::PoisonDOT: d = eff.value2 ? std::format("中毒{}%/幀({}幀)", eff.value, eff.value2) : std::format("中毒{}%/幀", eff.value); break;
    case EffectType::PoisonDmgAmp: d = std::format("中毒增傷{}%", eff.value); break;
    case EffectType::MPOnHit: d = std::format("命中回{}MP", eff.value); break;
    case EffectType::MPDrain: d = std::format("吸取{}MP", eff.value); break;
    case EffectType::MPRecoveryBonus: d = std::format("回藍+{}%", eff.value); break;
    case EffectType::SkillDmgPct: d = std::format("技能傷害+{}%", eff.value); break;
    case EffectType::SkillReflectPct: d = std::format("反彈{}%", eff.value); break;
    case EffectType::CDR: d = std::format("冷卻-{}%", eff.value); break;
    case EffectType::ShieldPctMaxHP: d = std::format("護盾{}%生命", eff.value); break;
    case EffectType::ShieldFreezeRes: d = std::format("護盾僵直抗性{}%", eff.value); break;
    case EffectType::HealAuraPct: d = eff.value2 ? std::format("治療光環{}%(每{}幀)", eff.value, eff.value2) : std::format("治療光環{}%", eff.value); break;
    case EffectType::HealAuraFlat: d = eff.value2 ? std::format("治療光環{}(每{}幀)", eff.value, eff.value2) : std::format("治療光環{}", eff.value); break;
    case EffectType::HealedATKSPDBoost: d = std::format("受治療加速{}%", eff.value); break;
    case EffectType::HPRegenPct: d = eff.value2 ? std::format("回血{}%(每{}幀)", eff.value, eff.value2) : std::format("回血{}%", eff.value); break;
    case EffectType::FreezeReductionPct: d = std::format("僵直-{}%", eff.value); break;
    case EffectType::ControlImmunityFrames: d = std::format("免控>{}幀", eff.value); break;
    case EffectType::KillHealPct: d = std::format("擊殺回{}%血", eff.value); break;
    case EffectType::KillInvincFrames: d = std::format("擊殺無敵{}幀", eff.value); break;
    case EffectType::PostSkillInvincFrames: d = std::format("技能後無敵{}幀", eff.value); break;
    case EffectType::DmgReductionPct: d = std::format("傷害減免{}%", eff.value); break;
    case EffectType::Bloodlust: d = std::format("嗜血+{}攻", eff.value); break;
    case EffectType::Adaptation: d = std::format("適應{}%({}層)", eff.value, eff.value2); break;
    case EffectType::RampingDmg: d = std::format("蓄力+{}%({}層)", eff.value, eff.value2); break;
    case EffectType::HealBurst: d = std::format("回血{}%", eff.value); break;
    default: d = std::format("效果({})", eff.value); break;
    }
    return triggerPrefix() + d + countSuffix();
}

// Calculate display width of a UTF-8 string
// Chinese characters and wide characters take 2 columns, ASCII takes 1
static int getDisplayWidth(const std::string& str)
{
    int width = 0;
    for (size_t i = 0; i < str.size();)
    {
        unsigned char c = str[i];
        if (c < 0x80)
        {
            // ASCII character - 1 column
            width += 1;
            i += 1;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            // 2-byte UTF-8 - typically 2 columns
            width += 2;
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            // 3-byte UTF-8 (includes Chinese characters and ★) - 2 columns
            width += 2;
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            // 4-byte UTF-8 - 2 columns
            width += 2;
            i += 4;
        }
        else
        {
            // Invalid UTF-8, skip
            i += 1;
        }
    }
    return width;
}

static std::pair<std::string, Color> formatChessName(Role* role, int tier, std::optional<int> starOpt, std::optional<int> countOpt, const std::string& prefix = "")
{
    std::string result;
    int displayWidth = 0;

    // Add prefix if present
    if (!prefix.empty())
    {
        result += prefix;
        displayWidth += getDisplayWidth(prefix);
    }

    // Add name and pad to fixed width (16 columns total including prefix)
    result += role->Name;
    displayWidth += getDisplayWidth(role->Name);
    int nameColumnWidth = 16;
    while (displayWidth < nameColumnWidth)
    {
        result += " ";
        displayWidth += 1;
    }

    // Add stars in fixed width column (8 columns for up to 4 stars)
    int starColumnWidth = 8;
    int starWidth = 0;
    if (auto star = starOpt)
    {
        for (int i = 0; i < *star; ++i)
        {
            result += "★";
            starWidth += 2;
        }
    }
    while (starWidth < starColumnWidth)
    {
        result += " ";
        starWidth += 1;
    }

    // Add quantity in fixed width column (6 columns)
    int quantityColumnWidth = 6;
    int quantityWidth = 0;
    if (auto count = countOpt)
    {
        std::string countStr = "x" + std::to_string(*count);
        result += countStr;
        quantityWidth += (int)countStr.size();
    }
    while (quantityWidth < quantityColumnWidth)
    {
        result += " ";
        quantityWidth += 1;
    }

    // Add cost right-aligned in fixed width column (6 columns for $XXX)
    int cost = ChessSelector::calculateCost(tier, starOpt.value_or(1), countOpt.value_or(1));
    std::string costStr = "$" + std::to_string(cost);
    int costColumnWidth = 6;
    int costWidth = (int)costStr.size();

    // Add leading spaces for right alignment
    while (costWidth < costColumnWidth)
    {
        result += " ";
        costWidth += 1;
    }
    result += costStr;

    return { result, ChessPool::GetTierColor(tier) };
}

static std::shared_ptr<DrawableOnCall> makeComboInfoPanel()
{
    return std::make_shared<DrawableOnCall>([](DrawableOnCall* self) {
        int rawId = self->getID();
        if (rawId < 0) return;
        int roleId = rawId / 10;

        auto roleCombos = ChessCombo::getCombosForRole(roleId);
        if (roleCombos.empty()) return;

        auto& allCombos = ChessCombo::getAllCombos();
        auto& gd = GameData::get();

        auto starByRole = ChessCombo::buildStarMap(gd.getSelectedForBattle());

        int px = 720, py = 475, fs = 20;
        int colW = 225, maxY = py + 225;
        Engine::getInstance()->fillRoundedRect({0, 0, 0, 160}, px, py, 460, 240, 8);
        Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, px, py, 460, 240, 8);
        Font::getInstance()->draw("羈絆資訊", fs + 4, px + 10, py + 5, {255, 255, 100, 255});

        int rx = px + 10, ry = py + 32;
        for (auto cid : roleCombos)
        {
            auto& c = allCombos[(int)cid];

            auto [owned, effective] = KysChess::computeOwnership(c, starByRole);

            // Find last active threshold and next inactive threshold (using effective count)
            const ComboThreshold* lastActive = nullptr;
            const ComboThreshold* nextThr = nullptr;
            for (auto& t : c.thresholds)
            {
                if (effective >= t.count) lastActive = &t;
                else if (!nextThr) nextThr = &t;
            }

            // Display: last active if any, otherwise next (greyed out)
            const ComboThreshold* showThr = lastActive ? lastActive : nextThr;
            if (!showThr && !c.thresholds.empty())
                showThr = &c.thresholds.back();

            int neededH = (fs + 4) + (showThr ? (int)showThr->effects.size() * fs : 0) + 4;
            if (ry + neededH > maxY + fs && ry > py + 32)
            {
                rx += colW;
                ry = py + 32;
                if (rx + colW > px + 460) break;
            }

            bool active = lastActive != nullptr;
            int denom = (nextThr ? nextThr : (lastActive ? lastActive : &c.thresholds.back()))->count;
            Color col = active ? Color{0, 255, 100, 255} : Color{200, 200, 200, 255};
            std::string countFmt = KysChess::formatComboCount(owned, effective, denom, c.starSynergyBonus, c.isAntiCombo);
            std::string header = std::format("{} ({})", c.name, countFmt);
            Font::getInstance()->draw(header, fs, rx, ry, col);
            ry += fs + 4;

            if (showThr)
            {
                Color effCol = active ? Color{180, 220, 255, 255} : Color{120, 120, 120, 255};
                for (auto& eff : showThr->effects)
                {
                    if (ry > maxY) break;
                    Font::getInstance()->draw("  " + comboEffectDesc(eff), fs - 2, rx, ry, effCol);
                    ry += fs;
                }
            }
            ry += 4;
        }
    });
}

static std::shared_ptr<DrawableOnCall> makeOwnedPanel()
{
    return std::make_shared<DrawableOnCall>([](DrawableOnCall*) {
        auto& gd = GameData::get();
        auto& chessMap = gd.collection.getChess();
        if (chessMap.empty()) return;

        auto* font = Font::getInstance();
        int px = 70, py = 420, fs = 20, lh = fs + 4;
        int pw = 600, ph = 290;
        Engine::getInstance()->fillRoundedRect({0, 0, 0, 160}, px, py, pw, ph, 8);
        Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, px, py, pw, ph, 8);
        font->draw("我方棋子", fs + 4, px + 10, py + 5, {255, 255, 100, 255});

        // Count how many of each chess type are deployed
        std::map<std::pair<int,int>, int> deployedCount;
        for (auto& s : gd.getSelectedForBattle())
            deployedCount[{s.role->ID, s.star}]++;

        // List each piece individually, deployed first (like sell/select menus)
        struct Entry { std::string label; Color color; };
        std::vector<Entry> deployed, bench;
        for (auto& [chess, count] : chessMap)
        {
            auto key = std::make_pair(chess.role->ID, chess.star);
            int dep = deployedCount[key];
            Color col = ChessPool::GetTierColor(ChessPool::GetChessTier(chess.role->ID));
            std::string stars;
            for (int i = 0; i < chess.star; i++) stars += "★";
            std::string baseName = std::format("{}{}", chess.role->Name, stars);

            for (int i = 0; i < count; ++i)
            {
                if (dep > 0)
                {
                    deployed.push_back({"▶" + baseName, col});
                    dep--;
                }
                else
                {
                    bench.push_back({" " + baseName, {(Uint8)(col.r/2+80), (Uint8)(col.g/2+80), (Uint8)(col.b/2+80), 200}});
                }
            }
        }

        int yTop = py + fs + 4 + 30;
        int x = px + 10, y = yTop;
        int colW = 200;
        auto drawEntry = [&](const Entry& e) {
            if (y + lh > py + ph) { x += colW; y = yTop; }
            if (x + colW > px + pw) return;
            font->draw(e.label, fs, x, y, e.color);
            y += lh;
        };
        for (auto& e : deployed) drawEntry(e);
        for (auto& e : bench) drawEntry(e);
    });
}

static std::shared_ptr<UIRoleStatusMenu> makeChessMenu(const std::string& title,
    std::vector<std::pair<int, std::string>>& items, std::vector<Color>& colors,
    int perPage, int fontSize, bool showNav = true, bool exitable = true,
    std::vector<Color> outlineColors = {}, std::vector<bool> animateOutlines = {},
    std::vector<int> outlineThicknesses = {})
{
    auto menu = std::make_shared<UIRoleStatusMenu>(title, items, colors, perPage, fontSize, false, exitable, outlineColors, animateOutlines, outlineThicknesses);
    menu->setInputPosition(80, 70);
    menu->getStatusDrawable().getUIStatus().setPosition(720, 40);
    if (!showNav) menu->setShowNavigationButtons(false);
    return menu;
}

static std::shared_ptr<DrawableOnCall> makeHeaderBar()
{
    return std::make_shared<DrawableOnCall>([](DrawableOnCall*) {
        auto& gd = GameData::get();
        auto& cfg = ChessBalance::config();
        auto* engine = Engine::getInstance();
        auto* font = Font::getInstance();
        int w = engine->getUIWidth(), barH = 32, fs = 18, x = 10, y = 6;

        engine->fillColor({0, 0, 0, 180}, 0, 0, w, barH);

        auto seg = [&](const std::string& s, Color c) {
            font->draw(s, fs, x, y, c);
            x += fs * Font::getTextDrawSize(s) / 2 + 12;
        };

        int fight = gd.battleProgress.getFight();
        seg(std::format("第{}關{}", fight + 1, gd.battleProgress.isBossFight() ? "(Boss)" : ""), {255, 200, 100, 255});
        seg(std::format("${}", gd.getMoney()), {255, 215, 0, 255});
        seg(std::format("Lv{} {}/{}", gd.getLevel() + 1, gd.getExp(), gd.getExpForNextLevel()), {100, 200, 255, 255});
        seg(std::format("出戰{}/{}", gd.getSelectedForBattle().size(), gd.getMaxDeploy()), {100, 255, 100, 255});
        seg(std::format("背包{}/{}", gd.getBenchCount(), cfg.benchSize), {200, 180, 255, 255});
    });
}

static void drawNeigongDetail(const NeigongDef& ng, int ownedState = -1)
{
    auto* font = Font::getInstance();
    int px = 480, py = 180, fs = 24;
    Engine::getInstance()->fillRoundedRect({0, 0, 0, 180}, px, py, 500, 400, 8);
    Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, px, py, 500, 400, 8);
    TextureManager::getInstance()->renderTexture("item", ng.itemId, px + 10, py + 10);
    int ty = py + 10;
    int dx = px + 100;
    font->draw(ng.name, fs + 4, dx, ty, {255, 255, 100, 255}); 
    ty += fs + 10;
    std::string tierName[] = {"初階", "中階", "高階"};
    font->draw(std::format("層級: {}", tierName[std::min(ng.tier - 1, 2)]), fs, dx, ty, {200, 200, 200, 255}); ty += fs + 4;
    if (ownedState >= 0)
    {
        bool owned = ownedState > 0;
        font->draw(owned ? "已獲得" : "未獲得", fs, dx, ty, owned ? Color{0, 255, 0, 255} : Color{180, 180, 180, 255});
        ty += fs + 8;
    }
    ty = py + 100;
    font->draw("效果:", fs, px + 10, ty, {200, 200, 200, 255}); ty += fs + 4;
    for (auto& eff : ng.effects)
    {
        font->draw("  " + comboEffectDesc(eff), fs, px + 10, ty, {220, 220, 220, 255});
        ty += fs + 2;
    }
}

static std::string challengeRewardDesc(const BalanceConfig::ChallengeReward& r)
{
    using RT = BalanceConfig::ChallengeRewardType;
    switch (r.type)
    {
    case RT::Gold: return std::format("獲取{}金幣", r.value);
    case RT::GetPiece: return std::format("獲取棋子(最高{}費)", r.value);
    case RT::GetNeigong: return std::format("獲取內功(最高{}階)", r.value);
    case RT::StarUp1to2: return std::format("升星★→★★(最高{}費)", r.value);
    case RT::StarUp2to3: return std::format("升星★★→★★★(最高{}費)", r.value);
    }
    return "未知獎勵";
}

}    //namespace

void ChessSelector::getChess()
{
    auto& gameData = GameData::get();
    for (;;)
    {
        auto rollOfChess = gameData.chessPool.getChessFromPool(gameData.getLevel());

        for (;;)
        {
            std::vector<std::pair<int, std::string>> rolePairs;
            std::vector<Color> roleColors;
            std::vector<Color> outlineColors;
            std::vector<bool> animateOutlines;
            std::vector<int> outlineThicknesses;
            auto& roles = Save::getInstance()->getRoles();

            rolePairs.emplace_back(-1, std::format("刷新 ${}", ChessBalance::config().refreshCost));
            roleColors.push_back({ 255, 204, 229, 255 });
            outlineColors.push_back({0, 0, 0, 0});
            animateOutlines.push_back(false);
            outlineThicknesses.push_back(1);

            rolePairs.emplace_back(-2, gameData.isShopLocked() ? "[已鎖定] 點擊解鎖" : "[未鎖定] 點擊鎖定");
            roleColors.push_back(gameData.isShopLocked() ? Color{ 255, 80, 80, 255 } : Color{ 128, 128, 128, 255 });
            outlineColors.push_back({0, 0, 0, 0});
            animateOutlines.push_back(false);
            outlineThicknesses.push_back(1);

            auto& chessMap = gameData.collection.getChess();

            auto addRoleWithTier = [&](Role* role, int tier)
            {
                auto [name, color] = formatChessName(role, tier, {}, {});
                rolePairs.emplace_back(role->ID * 10 + 1, name);
                roleColors.push_back(color);

                // Check ownership status across all star levels
                Chess c1 = { role, 1 };
                bool owned = false;
                for (auto& [ch, cnt] : chessMap)
                {
                    if (ch.role == role && cnt > 0)
                    {
                        // A single 3-star piece can't be starred up further, so don't mark as owned
                        if (ch.star == 3 && cnt == 1)
                        {
                            // Check if this is the ONLY entry for this role
                            bool hasOtherEntries = false;
                            for (auto& [ch2, cnt2] : chessMap)
                            {
                                if (ch2.role == role && ch2.star != 3 && cnt2 > 0) { hasOtherEntries = true; break; }
                            }
                            if (!hasOtherEntries) continue;
                        }
                        owned = true;
                        break;
                    }
                }
                bool wouldStarUp = gameData.collection.wouldMerge(c1);

                if (wouldStarUp)
                {
                    // Thick animated golden outline — buying this triggers a star-up!
                    outlineColors.push_back({255, 215, 0, 255});
                    animateOutlines.push_back(true);
                    outlineThicknesses.push_back(3);
                }
                else if (owned)
                {
                    // Animated blue outline — already owned (thinner, subtler)
                    outlineColors.push_back({100, 200, 255, 220});
                    animateOutlines.push_back(true);
                    outlineThicknesses.push_back(1);
                }
                else
                {
                    // Default — no outline
                    outlineColors.push_back({0, 0, 0, 0});
                    animateOutlines.push_back(false);
                    outlineThicknesses.push_back(1);
                }
            };

            for (auto [role, star] : rollOfChess)
            {
                addRoleWithTier(role, star);
            }

            auto menu = makeChessMenu(std::format("購買棋子 等級{} ${} 背包{}/{}", gameData.getLevel() + 1, gameData.getMoney(), gameData.getBenchCount(), ChessBalance::config().benchSize), rolePairs, roleColors, (int)rolePairs.size(), 32, false, true, outlineColors, animateOutlines, outlineThicknesses);

            menu->addDrawableOnCall(makeComboInfoPanel());
            menu->addDrawableOnCall(makeOwnedPanel());

            menu->run();

            int selectedId = menu->getResult();
            if (selectedId < 0)
            {
                return;
            }
            else if (selectedId == 0)
            {
                if (!gameData.spend(ChessBalance::config().refreshCost)) continue;
                gameData.chessPool.refresh();
                gameData.setShopLocked(false);
                break;
            }
            else if (selectedId == 1)
            {
                gameData.setShopLocked(!gameData.isShopLocked());
                continue;
            }
            else if (selectedId >= 2)
            {
                auto actualIdx = selectedId - 2;
                auto [role, tier] = rollOfChess[actualIdx];
                int cost = ChessBalance::config().tierPrices[tier - 1];
                if (gameData.isBenchFull() && !gameData.collection.wouldMerge({ role, 1 }))
                {
                    auto text = std::make_shared<TextBox>();
                    text->setText("背包已滿！請先出售棋子");
                    text->setFontSize(32);
                    text->runCentered(Engine::getInstance()->getUIHeight() / 2);
                    continue;
                }
                if (!gameData.spend(cost)) continue;
                bool willMerge = gameData.collection.wouldMerge({ role, 1 });
                gameData.chessPool.removeChessAt(actualIdx);
                gameData.addChessAndFixSelection({ role, 1 });
                auto text = std::make_shared<TextBox>();
                if (willMerge)
                {
                    text->setText(std::format("消費{}，{}升星！", cost, role->Name));
                    Audio::getInstance()->playESound(72);
                }
                else
                {
                    text->setText(std::format("消費{}，獲取{}", cost, role->Name));
                }
                text->setFontSize(32);
                text->runCentered(Engine::getInstance()->getUIHeight() / 2);
                break;
            }
        }
    }
}

void ChessSelector::sellChess()
{
    auto& gameData = GameData::get();

    for (;;)
    {
        auto& chessMap = gameData.collection.getChess();
        if (chessMap.empty()) return;

        std::vector<std::pair<int, std::string>> rolePairs;
        std::vector<Color> roleColors;
        std::vector<std::pair<Chess, bool>> chessList;

        std::map<std::pair<int,int>, int> equippedCount;
        for (auto& s : gameData.getSelectedForBattle())
            equippedCount[{s.role->ID, s.star}]++;

        for (const auto& [chess, count] : chessMap)
        {
            for (int i = 0; i < count; ++i)
            {
                auto key = std::make_pair(chess.role->ID, chess.star);
                bool equipped = equippedCount[key] > 0;
                if (equipped) equippedCount[key]--;
                auto [name, color] = formatChessName(chess.role, ChessPool::GetChessTier(chess.role->ID), chess.star, {}, equipped ? "[出戰]" : "");
                rolePairs.emplace_back(chess.role->ID * 10 + chess.star, name);
                roleColors.push_back(color);
                chessList.push_back({chess, equipped});
            }
        }

        auto menu = makeChessMenu(std::format("出售棋子 背包{}/{}", gameData.getBenchCount(), ChessBalance::config().benchSize), rolePairs, roleColors, 16, 32);
        menu->run();

        int selectedId = menu->getResult();
        if (selectedId < 0) break;

        auto [chess, wasEquipped] = chessList[selectedId];
        int tier = ChessPool::GetChessTier(chess.role->ID);
        int sellPrice = calculateCost(tier, chess.star, 1);
        gameData.collection.removeChess(chess);

        if (wasEquipped)
        {
            auto& selected = gameData.getSelectedForBattle();
            std::vector<Chess> newSelection;
            bool removedOne = false;
            for (const auto& s : selected)
            {
                if (!removedOne && s.role == chess.role && s.star == chess.star)
                    removedOne = true;
                else
                    newSelection.push_back(s);
            }
            gameData.setSelectedForBattle(newSelection);
        }
        gameData.make(sellPrice);

        auto text = std::make_shared<TextBox>();
        text->setText(std::format("售出棋子{}，獲得${}", chess.role->Name, sellPrice));
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);
    }
}

void ChessSelector::selectForBattle()
{
    auto& gameData = GameData::get();
    auto& chessMap = gameData.collection.getChess();

    if (chessMap.empty())
    {
        return;
    }

    int maxSelection = gameData.getMaxDeploy();
    std::vector<Chess> currentSelection = gameData.getSelectedForBattle();

    for (;;)
    {
        std::vector<std::pair<int, std::string>> rolePairs;
        std::vector<Color> roleColors;
        std::vector<Chess> chessList;

        for (const auto& chess : currentSelection)
        {
            auto [name, color] = formatChessName(chess.role, ChessPool::GetChessTier(chess.role->ID), chess.star, {}, "[出戰]");
            rolePairs.emplace_back(chess.role->ID * 10 + chess.star, name);
            roleColors.push_back({ 255, 215, 0, 255 });  // Gold for selected
            chessList.push_back(chess);
        }

        // Add available chess from collection - expand by count so each piece is listed individually
        for (const auto& [chess, count] : chessMap)
        {
            // Count how many of this chess are already selected
            int selectedCount = 0;
            for (const auto& selected : currentSelection)
            {
                if (selected.role == chess.role && selected.star == chess.star)
                {
                    selectedCount++;
                }
            }

            // Add remaining unselected pieces
            int remainingCount = count - selectedCount;
            for (int i = 0; i < remainingCount; ++i)
            {
                auto [name, color] = formatChessName(chess.role, ChessPool::GetChessTier(chess.role->ID), chess.star, {});
                rolePairs.emplace_back(chess.role->ID * 10 + chess.star, name);
                roleColors.push_back(color);
                chessList.push_back(chess);
            }
        }

        std::string menuTitle = std::format("選擇出戰棋子 {}/{} 背包{}/{}", currentSelection.size(), maxSelection, gameData.getBenchCount(), ChessBalance::config().benchSize);
        auto menu = makeChessMenu(menuTitle, rolePairs, roleColors, 16, 32);

        menu->addDrawableOnCall(makeComboInfoPanel());

        menu->run();

        int selectedId = menu->getResult();
        if (selectedId < 0)
        {
            // Save and exit
            gameData.setSelectedForBattle(currentSelection);
            return;
        }
        else
        {
            auto chess = chessList[selectedId];

            // Check if this is a selected chess (deselect one instance)
            if (selectedId < currentSelection.size())
            {
                // This is a selected chess, remove it
                currentSelection.erase(currentSelection.begin() + selectedId);
            }
            else
            {
                // Add to selection if not at max
                if (currentSelection.size() < maxSelection)
                {
                    currentSelection.push_back(chess);
                }
                else
                {
                    auto text = std::make_shared<TextBox>();
                    text->setText(std::format("最多只能選擇{}個棋子", maxSelection));
                    text->setFontSize(32);
                    text->runCentered(Engine::getInstance()->getUIHeight() / 2);
                }
            }
            // Update game data immediately so status panel reflects changes
            gameData.setSelectedForBattle(currentSelection);
        }
    }
}

void ChessSelector::enterBattle()
{
    auto& gameData = GameData::get();
    auto& progress = gameData.battleProgress;

    // Check if game is complete
    if (progress.isGameComplete())
    {
        auto text = std::make_shared<TextBox>();
        text->setText("恭喜通關！");
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);
        return;
    }

    // Check if we have selected chess
    auto& selectedChess = gameData.getSelectedForBattle();
    if (selectedChess.empty())
    {
        auto text = std::make_shared<TextBox>();
        text->setText("請先選擇出戰棋子");
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);
        return;
    }

    // Save enemy random counter before generation for retry determinism
    auto savedEnemyCallCount = gameData.enemyCallCount_;

    // Generate enemies based on progress
    std::vector<int> enemyIds;
    std::vector<int> enemyStars;
    int fightNum = progress.getFight();
    bool isBoss = progress.isBossFight();

    // Generate enemies from table
    auto& ecfg = ChessBalance::config();
    int tableIdx = std::min(fightNum, (int)ecfg.enemyTable.size() - 1);
    auto& slots = ecfg.enemyTable[tableIdx];

    for (auto& slot : slots)
    {
        auto role = ChessPool::selectEnemyFromPool(slot.tier);
        enemyIds.push_back(role->ID);
        enemyStars.push_back(std::min(slot.star, 3));
    }

    // Build roles from selected chess + generated enemies
    DynamicBattleRoles roles;
    for (auto& c : selectedChess)
    {
        roles.teammate_ids.push_back(c.role->ID);
        roles.teammate_stars.push_back(c.star);
    }
    roles.enemy_ids = enemyIds;
    roles.enemy_stars = enemyStars;

    // Seed battle RNG deterministically so retries produce same combat
    int battleSeed = static_cast<int>(gameData.enemyRandInt(INT_MAX));

    int result = runBattle(roles, selectedChess, -1, battleSeed);

    // Recover all HP for selected chess
    for (const auto& chess : selectedChess)
    {
        chess.role->HP = chess.role->MaxHP;
        chess.role->MP = chess.role->MaxMP;
    }

    if (result == 0)
    {
        int reward = ecfg.rewardBase + fightNum * ecfg.rewardGrowth / (ecfg.totalFights - 1) + (isBoss ? ecfg.bossRewardBonus : 0);
        int interest = std::min(gameData.getMoney() * ecfg.interestPercent / 100, ecfg.interestMax);
        gameData.make(reward + interest);
        int expGain = isBoss ? ecfg.bossBattleExp : ecfg.battleExp;
        int oldLvl = gameData.getLevel();
        gameData.increaseExp(expGain);
        if (isBoss) showNeigongReward();
        progress.advance();
        if (!gameData.isShopLocked())
        {
            gameData.chessPool.refresh();
        }
        auto text = std::make_shared<TextBox>();
        std::string lvlMsg = (gameData.getLevel() > oldLvl) ? std::format(" 升級！等級{}", gameData.getLevel() + 1) : "";
        std::string nextInfo = progress.isGameComplete() ? " 通關！"
            : progress.isBossFight() ? std::format(" 下一關：第{}關(Boss)", progress.getFight() + 1)
            : std::format(" 下一關：第{}關", progress.getFight() + 1);
        std::string interestMsg = interest > 0 ? std::format("(利息+${})", interest) : "";
        text->setText(std::format("勝利！獲得${}{} 經驗+{}{}{}", reward, interestMsg, expGain, lvlMsg, nextInfo));
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);
        UISave::autoSave();
    }
    else
    {
        // Reset enemy random counter so retry generates same enemies
        gameData.enemyCallCount_ = savedEnemyCallCount;
        gameData.restoreRand();
        auto text = std::make_shared<TextBox>();
        text->setText("戰鬥失敗！請調整陣容後再試");
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);
    }
}

int ChessSelector::runBattle(const DynamicBattleRoles& roles, const std::vector<Chess>& allyChess, int battle_id, int seed)
{
    // Build enemy Chess vector for combo detection
    std::vector<Chess> enemyChessVec;
    for (size_t i = 0; i < roles.enemy_ids.size(); i++)
    {
        auto r = Save::getInstance()->getRole(roles.enemy_ids[i]);
        if (r) enemyChessVec.push_back({r, i < roles.enemy_stars.size() ? roles.enemy_stars[i] : 1});
    }

    // Detect combos for both teams
    auto allyCombos = ChessCombo::detectCombos(allyChess);
    auto enemyCombos = ChessCombo::detectCombos(enemyChessVec);

    // Pre-battle stats
    {
        auto view = std::make_shared<BattleStatsView>();
        view->setupPreBattle(allyChess, roles.enemy_ids, roles.enemy_stars, allyCombos, enemyCombos);
        view->run();
    }

    // Create and run battle
    auto battle = DynamicChessMap::createBattle(roles, battle_id);
    if (seed >= 0)
        battle->rand_.set_seed(static_cast<unsigned int>(seed));
    battle->run();

    // Post-battle stats
    {
        auto view = std::make_shared<BattleStatsView>();
        view->setupPostBattle(battle->getFriendsObj(), battle->getEnemiesObj(), battle->getTracker(), battle->getResult());
        view->run();
    }

    return battle->getResult();
}

void ChessSelector::buyExp()
{
    auto& gd = GameData::get();
    auto& bcfg = ChessBalance::config();
    if (gd.getLevel() >= bcfg.maxLevel)
    {
        auto text = std::make_shared<TextBox>();
        text->setText("已達最高等級！");
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);
        return;
    }

    int curLvl = gd.getLevel();
    int nextLvl = curLvl + 1;
    int curPieces = gd.getMaxDeploy();
    int nextPieces = std::max(nextLvl + 1, bcfg.minBattleSize);
    bool atNextMax = nextLvl >= bcfg.maxLevel;

    auto weightStr = [&](int lvl) -> std::string {
        if (lvl >= (int)bcfg.shopWeights.size()) return "已滿級";
        auto& w = bcfg.shopWeights[lvl];
        return std::format("1費{}% 2費{}% 3費{}% 4費{}% 5費{}%", w[0], w[1], w[2], w[3], w[4]);
    };

    // Pre-compute strings so lambda only captures values
    std::string hdr = std::format("等級 {}    經驗 {}/{}    金幣 ${}",
        curLvl + 1, gd.getExp(), gd.getExpForNextLevel(), gd.getMoney());
    std::string costLine = std::format("花費 ${}  獲得 {} 經驗", bcfg.buyExpCost, bcfg.buyExpAmount);
    std::string pieceLine = std::format("出戰人數: {}", curPieces);
    std::string pieceNext = (!atNextMax && nextPieces > curPieces) ? std::format("  → {}", nextPieces) : "";
    std::string curW = "  " + weightStr(curLvl);
    std::string nextW = "  " + (atNextMax ? std::string("已滿級") : weightStr(nextLvl));

    // Show confirmation menu with info panel
    auto menu = std::make_shared<MenuText>(std::vector<std::string>{"確認購買", "取消"});
    menu->setFontSize(36);
    menu->arrange(0, 0, 0, 45);

    auto infoPanel = std::make_shared<DrawableOnCall>([=](DrawableOnCall*) {
        int px = 370, py = 195, fs = 20, lh = fs + 6;
        Engine::getInstance()->fillRoundedRect({0, 0, 0, 180}, px, py, 520, 280, 8);
        Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, px, py, 520, 280, 8);
        auto* font = Font::getInstance();
        int ly = py + 10, x = px + 10;

        font->draw(hdr, fs, x, ly, {255, 215, 0, 255}); ly += lh;
        font->draw(costLine, fs, x, ly, {255, 255, 255, 255}); ly += lh + 6;
        font->draw(pieceLine, fs, x, ly, {130, 220, 255, 255});
        if (!pieceNext.empty()) font->draw(pieceNext, fs, x + fs * 8, ly, {100, 255, 100, 255});
        ly += lh + 6;
        font->draw("當前商店權重:", fs, x, ly, {160, 160, 160, 255}); ly += lh;
        font->draw(curW, fs, x, ly, {255, 255, 255, 255}); ly += lh + 4;
        font->draw("下一級商店權重:", fs, x, ly, {160, 160, 160, 255}); ly += lh;
        font->draw(nextW, fs, x, ly, {100, 255, 100, 255});
    });
    menu->addChild(infoPanel);
    menu->runAtPosition(200, 200);

    if (menu->getResult() != 0) return;
    if (!gd.spend(bcfg.buyExpCost)) return;

    int oldLvl = gd.getLevel();
    gd.increaseExp(bcfg.buyExpAmount);
    auto text = std::make_shared<TextBox>();
    if (gd.getLevel() > oldLvl)
        text->setText(std::format("升級！等級{} 經驗{}/{}", gd.getLevel() + 1, gd.getExp(), gd.getExpForNextLevel()));
    else
        text->setText(std::format("經驗{}/{}", gd.getExp(), gd.getExpForNextLevel()));
    text->setFontSize(32);
    text->runCentered(Engine::getInstance()->getUIHeight() / 2);
}

void ChessSelector::showContextMenu()
{
    //ChessBalance::apply();
    auto& gd = GameData::get();
    auto menu = std::make_shared<MenuText>(std::vector<std::string>{ "購買棋子", "出售棋子", "選擇出戰", "進入戰鬥", "購買經驗", "查看羈絆", "查看內功", "遠征挑戰", "排兵佈陣", "遊戲說明" });
    menu->setFontSize(36);
    menu->arrange(0, 0, 0, 45);
    menu->runAtPosition(200, 120);

    switch (menu->getResult())
    {
    case 0: getChess(); break;
    case 1: sellChess(); break;
    case 2: selectForBattle(); break;
    case 3: enterBattle(); break;
    case 4: buyExp(); break;
    case 5: viewCombos(); break;
    case 6: viewNeigong(); break;
    case 7: showExpeditionChallenge(); break;
    case 8: showPositionSwap(); break;
    case 9: showGameGuide(); break;
    default: break;
    }
    UISave::autoSave();
}

void ChessSelector::viewCombos()
{
    auto& combos = ChessCombo::getAllCombos();
    auto& gameData = GameData::get();

    // Build star map (owned = present in map; value = star level)
    auto starByRole = ChessCombo::buildStarMap(gameData.getSelectedForBattle());

    // Build menu items: one per combo
    std::vector<std::pair<int, std::string>> items;
    std::vector<Color> colors;
    for (int i = 0; i < (int)combos.size(); i++)
    {
        auto& c = combos[i];
        auto [owned, effective] = KysChess::computeOwnership(c, starByRole);
        // Pad name to 14 bytes (7 Chinese chars) for consistent menu width.
        // PREFIX(3 bytes) + owned(2) + bonusPart(3) + "/" + denom(2) — all fixed width.
        // PREFIX: ★ star-synergy | 独 anti-combo | 　 regular
        // bonusPart: "+N " star bonus | "   " otherwise
        std::string padded = c.name;
        while (padded.size() < 14) padded += "　";  // fullwidth space
        int extra = effective - owned;
        // Anti-combo denominator is the threshold (1); regular is total pool size
        int denom = c.isAntiCombo ? (c.thresholds.empty() ? 1 : c.thresholds[0].count)
                                   : (int)c.memberRoleIds.size();
        std::string countPrefix = c.isAntiCombo ? "独" : (c.starSynergyBonus ? "★" : "　");
        std::string bonusPart   = (c.starSynergyBonus && extra > 0)
            ? std::format("+{:<2}", extra) : "   ";
        std::string countFmt = std::format("{}{:2}{}/{:2}", countPrefix, owned, bonusPart, denom);
        std::string label = std::format("{}({})", padded, countFmt);
        items.emplace_back(i, label);
        colors.push_back(owned > 0 ? Color{0, 255, 0, 255} : Color{180, 180, 180, 255});
    }

    // Detail panel drawable — two-column: members left, thresholds right
    auto detailDraw = std::make_shared<DrawableOnCall>([&combos, &starByRole](DrawableOnCall* self) {
        int idx = self->getID();
        if (idx < 0 || idx >= (int)combos.size()) return;
        auto& c = combos[idx];
        auto save = Save::getInstance();
        auto font = Font::getInstance();

        int px = 495, py = 60, fs = 24;
        Engine::getInstance()->fillRoundedRect({0, 0, 0, 180}, px, py, 760, 560, 8);
        Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, px, py, 760, 560, 8);

        font->draw(c.name, fs + 4, px + 10, py + 10, {255, 255, 100, 255});

        // Left column: members
        // Pre-compute effective count (star-augmented) for threshold coloring
        auto [ownedCount, effective] = KysChess::computeOwnership(c, starByRole);

        int y = py + 45;
        font->draw("成員:", fs, px + 10, y, {200, 200, 200, 255});
        y += fs + 4;
        int totalBonus = 0;
        for (int rid : c.memberRoleIds)
        {
            auto role = save->getRole(rid);
            if (!role) continue;
            bool owned = starByRole.count(rid) > 0;
            Color col = owned ? Color{0, 255, 0, 255} : Color{120, 120, 120, 255};
            std::string starSuffix;
            if (owned && c.starSynergyBonus)
            {
                int s = starByRole.count(rid) ? starByRole.at(rid) : 1;
                starSuffix = std::format(" ★{}", s);
                totalBonus += (s - 1);
            }
            font->draw(std::format("  {} ({}費){}{}", role->Name, ChessPool::GetChessTier(rid), owned ? " ✓" : "", starSuffix), fs, px + 10, y, col);
            y += fs + 1;
        }

        // Right column: thresholds
        int rx = px + 290, ry = py + 45;
        font->draw(c.isAntiCombo ? "條件:" : "閾值:", fs, rx, ry, {200, 200, 200, 255});
        if (c.starSynergyBonus)
        {
            ry += fs + 2;
            // One line: mechanic rule + current bonus contribution
            std::string synergyLine = (totalBonus > 0)
                ? std::format("★ 成員星級計人數，當前+{}人", totalBonus)
                : "★ 成員星級計人數（2★=2人）";
            font->draw(synergyLine, fs - 4, rx, ry, {255, 200, 50, 255});
            ry += (fs - 4) + 2;
            font->draw("  每額外★計入1羈絆人數", fs - 6, rx, ry, {180, 160, 80, 255});
        }
        ry += fs + 4;
        for (auto& t : c.thresholds)
        {
            bool tierActive = effective >= t.count;
            Color tierCol  = tierActive ? Color{0, 255, 0, 255} : Color{255, 200, 100, 255};
            Color effCol   = tierActive ? Color{180, 220, 255, 255} : Color{200, 200, 200, 255};
            std::string tierMark = tierActive ? " ✓" : "";
            font->draw(std::format("{}人: {}{}", t.count, t.name, tierMark), fs, rx, ry, tierCol);
            ry += fs + 1;
            for (auto& eff : t.effects)
            {
                font->draw("  " + comboEffectDesc(eff), fs - 2, rx, ry, effCol);
                ry += fs - 2;
            }
            ry += 8;
        }
    });

    SuperMenuTextExtraOptions opts;
    opts.itemColors_ = colors;
    opts.needInputBox_ = false;
    opts.exitable_ = true;
    auto menu = std::make_shared<SuperMenuText>("羈絆一覽", 36, items, 10, opts);
    menu->setInputPosition(80, 60);
    menu->addDrawableOnCall(detailDraw);
    menu->run();
}

void ChessSelector::showNeigongReward()
{
    auto& gd = GameData::get();
    auto& cfg = ChessBalance::config();
    auto& ngCfg = ChessNeigong::config();
    auto& pool = ChessNeigong::getPool();
    if (pool.empty()) return;

    // Determine boss index and available tiers
    int bossIdx = gd.battleProgress.getFight() / cfg.bossInterval;
    std::vector<int> availTiers;
    for (auto it = ngCfg.tiersByBoss.rbegin(); it != ngCfg.tiersByBoss.rend(); ++it)
    {
        if (it->first <= bossIdx) { availTiers = it->second; break; }
    }
    if (availTiers.empty()) availTiers = {1};

    // Filter: matching tiers, not already obtained
    auto& obtained = gd.getObtainedNeigong();
    std::set<int> obtainedSet(obtained.begin(), obtained.end());
    std::vector<const NeigongDef*> candidates;
    for (auto& ng : pool)
    {
        if (obtainedSet.count(ng.magicId)) continue;
        for (int t : availTiers)
            if (ng.tier == t) { candidates.push_back(&ng); break; }
    }
    if (candidates.empty()) return;

    bool rerolled = false;
    auto pickChoices = [&]() {
        std::vector<const NeigongDef*> choices;
        int n = std::min((int)candidates.size(), ngCfg.choiceCount);
        // Shuffle candidates using shop rand
        auto tmp = candidates;
        for (int i = (int)tmp.size() - 1; i > 0; --i)
            std::swap(tmp[i], tmp[gd.shopRandInt(i + 1)]);
        choices.assign(tmp.begin(), tmp.begin() + n);
        return choices;
    };

    auto choices = pickChoices();

    for (;;)
    {
        std::vector<std::pair<int, std::string>> items;
        std::vector<Color> colors;

        std::string tierName[] = {"初階", "中階", "高階"};

        for (int i = 0; i < (int)choices.size(); ++i)
        {
            auto* ng = choices[i];
            std::string padded = ng->name;
            while (padded.size() < 15) padded += "\xe3\x80\x80";
            std::string label = std::format("[{}] {}", tierName[std::min(ng->tier - 1, 2)], padded);
            items.emplace_back(i, label);
            Color tierColors[] = {{175, 238, 238}, {100, 255, 100}, {255, 200, 100}};
            colors.push_back(tierColors[std::min(ng->tier - 1, 2)]);
        }

        if (!rerolled)
        {
            items.emplace_back(-2, std::format("刷新 ${}", ngCfg.rerollCost));
            colors.push_back({128, 128, 128});
        }

        SuperMenuTextExtraOptions opts;
        opts.itemColors_ = colors;
        opts.needInputBox_ = false;
        opts.exitable_ = false;
        auto menu = std::make_shared<SuperMenuText>("選擇内功，内功將對所有成員自動生效", 36, items, (int)items.size(), opts);
        menu->setInputPosition(80, 70);

        // Detail panel
        auto iconPanel = std::make_shared<DrawableOnCall>([&choices](DrawableOnCall* self) {
            int idx = self->getID();
            if (idx < 0 || idx >= (int)choices.size()) return;
            drawNeigongDetail(*choices[idx]);
        });
        menu->addDrawableOnCall(iconPanel);

        menu->run();
        int sel = menu->getResult();
        if (sel == -2)
        {
            if (gd.spend(ngCfg.rerollCost))
            {
                rerolled = true;
                choices = pickChoices();
                continue;
            }
            continue;
        }

        if (sel < 0) continue;  // don't allow escape — must pick

        // Pick this neigong
        gd.addNeigong(choices[sel]->magicId);
        auto text = std::make_shared<TextBox>();
        text->setText(std::format("獲得內功：{}", choices[sel]->name));
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);
        return;
    }
}

void ChessSelector::viewNeigong()
{
    auto& pool = ChessNeigong::getPool();
    if (pool.empty()) return;

    auto& obtained = GameData::get().getObtainedNeigong();
    std::set<int> obtainedSet(obtained.begin(), obtained.end());

    std::vector<std::pair<int, std::string>> items;
    std::vector<Color> colors;
    std::string tierName[] = {"初階", "中階", "高階"};

    for (int i = 0; i < (int)pool.size(); ++i)
    {
        auto& ng = pool[i];
        bool owned = obtainedSet.count(ng.magicId) > 0;
        std::string padded = ng.name;
        while (padded.size() < 15) padded += "\xe3\x80\x80";  // fullwidth space
        std::string label = std::format("[{}] {}{}", tierName[std::min(ng.tier - 1, 2)], padded, owned ? " ✓" : "　 ");
        items.emplace_back(i, label);
        colors.push_back(owned ? Color{0, 255, 0, 255} : Color{120, 120, 120, 255});
    }

    auto detailDraw = std::make_shared<DrawableOnCall>([&pool, &obtainedSet](DrawableOnCall* self) {
        int idx = self->getID();
        if (idx < 0 || idx >= (int)pool.size()) return;
        auto& ng = pool[idx];
        drawNeigongDetail(ng, obtainedSet.count(ng.magicId) ? 1 : 0);
    });

    SuperMenuTextExtraOptions opts;
    opts.itemColors_ = colors;
    opts.needInputBox_ = false;
    opts.exitable_ = true;
    auto menu = std::make_shared<SuperMenuText>("内功一覽", 36, items, 10, opts);
    menu->setInputPosition(80, 70);
    menu->addDrawableOnCall(detailDraw);
    menu->run();
}

void ChessSelector::showExpeditionChallenge()
{
    auto& cfg = ChessBalance::config();
    if (cfg.challenges.empty()) return;
    auto& gd = GameData::get();
    auto save = Save::getInstance();

    for (;;)
    {
        // Build challenge list menu
        std::vector<std::pair<int, std::string>> items;
        std::vector<Color> colors;
        for (int i = 0; i < (int)cfg.challenges.size(); ++i)
        {
            auto& ch = cfg.challenges[i];
            bool done = gd.isChallengeCompleted(i);
            std::string label = std::format("{}{}", done ? "[已通關] " : "", ch.name);
            items.emplace_back(i, label);
            colors.push_back(done ? Color{120, 120, 120, 255} : Color{255, 200, 100, 255});
        }

        // Detail panel showing enemies + rewards
        auto detailDraw = std::make_shared<DrawableOnCall>([&cfg, &gd, save](DrawableOnCall* self) {
            int idx = self->getID();
            if (idx < 0 || idx >= (int)cfg.challenges.size()) return;
            auto& ch = cfg.challenges[idx];
            auto* font = Font::getInstance();
            int px = 460, py = 60, fs = 22;
            Engine::getInstance()->fillRoundedRect({0, 0, 0, 180}, px, py, 500, 500, 8);
            Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, px, py, 500, 500, 8);

            int ty = py + 10;
            font->draw(ch.name, fs + 4, px + 10, ty, {255, 255, 100, 255}); ty += fs + 10;
            font->draw(ch.description, fs, px + 10, ty, {200, 200, 200, 255}); ty += fs + 8;
            if (gd.isChallengeCompleted(idx))
            {
                font->draw("[已通關]", fs, px + 10, ty, {0, 255, 0, 255}); ty += fs + 8;
            }

            // Show enemies
            font->draw("敵方陣容:", fs, px + 10, ty, {255, 150, 150, 255}); ty += fs + 4;
            for (auto& e : ch.enemies)
            {
                auto* role = save->getRole(e.roleId);
                if (!role) continue;
                int tier = ChessPool::GetChessTier(e.roleId);
                std::string stars;
                for (int s = 0; s < e.star; ++s) stars += "★";
                font->draw(std::format("  {} {} ({}費)", role->Name, stars, tier), fs - 2, px + 10, ty, {220, 180, 180, 255});
                ty += fs;
            }
            ty += 8;

            // Show rewards
            font->draw("通關獎勵(擇一):", fs, px + 10, ty, {100, 255, 100, 255}); ty += fs + 4;
            for (auto& r : ch.rewards)
            {
                font->draw("  " + challengeRewardDesc(r), fs - 2, px + 10, ty, {200, 255, 200, 255});
                ty += fs;
            }
        });

        SuperMenuTextExtraOptions opts;
        opts.itemColors_ = colors;
        opts.needInputBox_ = false;
        opts.exitable_ = true;
        auto menu = std::make_shared<SuperMenuText>("遠征挑戰", 36, items, (int)items.size(), opts);
        menu->setInputPosition(80, 70);
        menu->addDrawableOnCall(detailDraw);
        menu->run();

        int sel = menu->getResult();
        if (sel < 0) return;
        int chIdx = items[sel].first;
        auto& ch = cfg.challenges[chIdx];

        // Check if player has pieces selected
        auto& selectedChess = gd.getSelectedForBattle();
        if (selectedChess.empty())
        {
            auto text = std::make_shared<TextBox>();
            text->setText("請先選擇出戰棋子");
            text->setFontSize(32);
            text->runCentered(Engine::getInstance()->getUIHeight() / 2);
            continue;
        }

        // Save enemy random counter so challenge doesn't corrupt regular battle state
        auto savedEnemyCallCount = gd.enemyCallCount_;

        // Build enemy team from fixed role IDs
        std::vector<int> enemyIds;
        std::vector<int> enemyStars;
        for (auto& e : ch.enemies)
        {
            enemyIds.push_back(e.roleId);
            enemyStars.push_back(e.star);
        }

        // Build roles and run battle
        DynamicBattleRoles roles;
        for (auto& c : selectedChess)
        {
            roles.teammate_ids.push_back(c.role->ID);
            roles.teammate_stars.push_back(c.star);
        }
        roles.enemy_ids = enemyIds;
        roles.enemy_stars = enemyStars;

        int result = runBattle(roles, selectedChess);

        // Recover HP
        for (const auto& chess : selectedChess)
        {
            chess.role->HP = chess.role->MaxHP;
            chess.role->MP = chess.role->MaxMP;
        }

        if (result != 0)
        {
            // Restore enemy random counter so failed challenge doesn't corrupt regular battle state
            gd.enemyCallCount_ = savedEnemyCallCount;
            gd.restoreRand();
            auto text = std::make_shared<TextBox>();
            text->setText("挑戰失敗！請調整陣容後再試");
            text->setFontSize(32);
            text->runCentered(Engine::getInstance()->getUIHeight() / 2);
            continue;
        }

        // Win — check if already completed
        if (gd.isChallengeCompleted(chIdx))
        {
            auto text = std::make_shared<TextBox>();
            text->setText("挑戰勝利！(已領取過獎勵)");
            text->setFontSize(32);
            text->runCentered(Engine::getInstance()->getUIHeight() / 2);
            continue;
        }

        // First win — reward selection
        // Pre-check which rewards are actually available
        auto isRewardAvailable = [&](const BalanceConfig::ChallengeReward& r) -> bool {
            using RT = BalanceConfig::ChallengeRewardType;
            if (r.type == RT::StarUp1to2 || r.type == RT::StarUp2to3)
            {
                int fromStar = (r.type == RT::StarUp1to2) ? 1 : 2;
                for (auto& [chess, count] : gd.collection.getChess())
                {
                    if (chess.star != fromStar) continue;
                    int tier = ChessPool::GetChessTier(chess.role->ID);
                    if (tier >= 0 && tier <= r.value) return true;
                }
                return false;
            }
            if (r.type == RT::GetNeigong)
            {
                auto& pool = ChessNeigong::getPool();
                auto& obtained = gd.getObtainedNeigong();
                std::set<int> obtainedSet(obtained.begin(), obtained.end());
                for (auto& ng : pool)
                {
                    if (ng.tier <= r.value && !obtainedSet.count(ng.magicId)) return true;
                }
                return false;
            }
            return true;
        };

        std::vector<std::pair<int, std::string>> rewardItems;
        std::vector<Color> rewardColors;
        std::vector<bool> rewardAvail;
        for (int i = 0; i < (int)ch.rewards.size(); ++i)
        {
            bool avail = isRewardAvailable(ch.rewards[i]);
            rewardAvail.push_back(avail);
            std::string desc = challengeRewardDesc(ch.rewards[i]);
            if (!avail) desc += "（無可用）";
            rewardItems.emplace_back(i, desc);
            rewardColors.push_back(avail ? Color{255, 255, 100, 255} : Color{120, 120, 120, 255});
        }

        int rSel = 0;
        for (;;)
        {
            SuperMenuTextExtraOptions rOpts;
            rOpts.itemColors_ = rewardColors;
            rOpts.needInputBox_ = false;
            rOpts.exitable_ = false;
            auto rMenu = std::make_shared<SuperMenuText>("選擇獎勵", 36, rewardItems, (int)rewardItems.size(), rOpts);
            rMenu->setInputPosition(80, 70);
            rMenu->run();
            rSel = rMenu->getResult();
            if (rSel < 0) rSel = 0;
            if (rewardAvail[rSel]) break;
            // Picked an unavailable reward — prompt and loop
            auto text = std::make_shared<TextBox>();
            text->setText("該獎勵無可用項目，請重新選擇");
            text->setFontSize(32);
            text->runCentered(Engine::getInstance()->getUIHeight() / 2);
        }

        auto& reward = ch.rewards[rSel];
        using RT = BalanceConfig::ChallengeRewardType;

        if (reward.type == RT::Gold)
        {
            gd.make(reward.value);
            auto text = std::make_shared<TextBox>();
            text->setText(std::format("獲得{}金幣！", reward.value));
            text->setFontSize(32);
            text->runCentered(Engine::getInstance()->getUIHeight() / 2);
        }
        else if (reward.type == RT::GetPiece)
        {
            // Show all roles up to max tier
            std::vector<std::pair<int, std::string>> pieceItems;
            std::vector<Color> pieceColors;
            std::vector<int> pieceRoleIds;
            for (int t = 1; t <= reward.value && t <= 5; ++t)
            {
                for (int rid : ChessPool::getRolesOfTier(t))
                {
                    auto* role = save->getRole(rid);
                    if (!role) continue;
                    auto [name, color] = formatChessName(role, t, 1, {});
                    pieceItems.emplace_back(rid * 10 + 1, name);
                    pieceColors.push_back(color);
                    pieceRoleIds.push_back(rid);
                }
            }
            auto pMenu = makeChessMenu("選擇棋子", pieceItems, pieceColors, 16, 28, true, false);
            pMenu->addDrawableOnCall(makeComboInfoPanel());
            pMenu->run();
            int pSel = pMenu->getResult();
            if (pSel >= 0)
            {
                int rid = pieceRoleIds[pSel];
                auto* role = save->getRole(rid);
                if (role) gd.addChessAndFixSelection({role, 1});
                auto text = std::make_shared<TextBox>();
                text->setText(std::format("獲得棋子：{}", role ? role->Name : ""));
                text->setFontSize(32);
                text->runCentered(Engine::getInstance()->getUIHeight() / 2);
            }
        }
        else if (reward.type == RT::GetNeigong)
        {
            auto& pool = ChessNeigong::getPool();
            auto& obtained = gd.getObtainedNeigong();
            std::set<int> obtainedSet(obtained.begin(), obtained.end());

            std::vector<std::pair<int, std::string>> ngItems;
            std::vector<Color> ngColors;
            std::vector<const NeigongDef*> ngPtrs;
            for (auto& ng : pool)
            {
                if (ng.tier > reward.value || obtainedSet.count(ng.magicId)) continue;
                ngItems.emplace_back((int)ngPtrs.size(), ng.name);
                ngColors.push_back({100, 255, 100, 255});
                ngPtrs.push_back(&ng);
            }
            if (ngItems.empty())
            {
                auto text = std::make_shared<TextBox>();
                text->setText("沒有可選的內功");
                text->setFontSize(32);
                text->runCentered(Engine::getInstance()->getUIHeight() / 2);
            }
            else
            {
                auto ngDetail = std::make_shared<DrawableOnCall>([&ngPtrs](DrawableOnCall* self) {
                    int idx = self->getID();
                    if (idx >= 0 && idx < (int)ngPtrs.size())
                        drawNeigongDetail(*ngPtrs[idx]);
                });
                SuperMenuTextExtraOptions nOpts;
                nOpts.itemColors_ = ngColors;
                nOpts.needInputBox_ = false;
                nOpts.exitable_ = false;
                auto nMenu = std::make_shared<SuperMenuText>("選擇內功，内功將對所有成員自動生效", 36, ngItems, 16, nOpts);
                nMenu->setInputPosition(80, 70);
                nMenu->addDrawableOnCall(ngDetail);
                nMenu->run();
                int nSel = nMenu->getResult();
                if (nSel >= 0)
                {
                    gd.addNeigong(ngPtrs[nSel]->magicId);
                    auto text = std::make_shared<TextBox>();
                    text->setText(std::format("獲得內功：{}", ngPtrs[nSel]->name));
                    text->setFontSize(32);
                    text->runCentered(Engine::getInstance()->getUIHeight() / 2);
                }
            }
        }
        else if (reward.type == RT::StarUp1to2 || reward.type == RT::StarUp2to3)
        {
            int fromStar = (reward.type == RT::StarUp1to2) ? 1 : 2;
            int toStar = fromStar + 1;

            std::vector<std::pair<int, std::string>> upItems;
            std::vector<Color> upColors;
            std::vector<Chess> upChess;
            for (auto& [chess, count] : gd.collection.getChess())
            {
                if (chess.star != fromStar) continue;
                int tier = ChessPool::GetChessTier(chess.role->ID);
                if (tier < 0 || tier > reward.value) continue;
                auto [name, color] = formatChessName(chess.role, tier, chess.star, count);
                upItems.emplace_back(chess.role->ID * 10 + chess.star, name);
                upColors.push_back(color);
                upChess.push_back(chess);
            }
            if (upItems.empty())
            {
                auto text = std::make_shared<TextBox>();
                text->setText("沒有可升星的棋子");
                text->setFontSize(32);
                text->runCentered(Engine::getInstance()->getUIHeight() / 2);
            }
            else
            {
                auto uMenu = makeChessMenu(
                    std::format("選擇升星 {}★→{}★", fromStar, toStar), upItems, upColors, 16, 28, true, false);
                uMenu->run();
                int uSel = uMenu->getResult();
                if (uSel >= 0)
                {
                    auto& picked = upChess[uSel];
                    gd.collection.removeChess(picked);
                    Chess upgraded = {picked.role, toStar};
                    gd.collection.addChess(upgraded);
                    // Fix selection: upgrade one matching deployed piece
                    auto sel = gd.getSelectedForBattle();
                    for (auto& s : sel)
                    {
                        if (s.role == picked.role && s.star == fromStar)
                        {
                            s.star = toStar;
                            break;
                        }
                    }
                    gd.setSelectedForBattle(sel);
                    auto text = std::make_shared<TextBox>();
                    text->setText(std::format("{}升星至{}★！", picked.role->Name, toStar));
                    text->setFontSize(32);
                    text->runCentered(Engine::getInstance()->getUIHeight() / 2);
                }
            }
        }

        gd.completeChallenge(chIdx);
        UISave::autoSave();
    }
}

void ChessSelector::showPositionSwap()
{
    auto& gd = GameData::get();
    bool cur = gd.isPositionSwapEnabled();
    std::vector<std::pair<int, std::string>> items = { {0, "  關閉  "}, {1, "  開啟  "} };
    SuperMenuTextExtraOptions opts;
    opts.needInputBox_ = false;
    opts.exitable_ = true;
    auto sub = std::make_shared<SuperMenuText>(cur ? "" : "", 36, items, 2, opts);
    sub->setShowNavigationButtons(false);
    auto panel = std::make_shared<DrawableOnCall>([cur](DrawableOnCall* self) {
        int px = 320, py = 200, fs = 32, lh = fs + 6;
        Engine::getInstance()->fillRoundedRect({0, 0, 0, 180}, px, py, 600, 250, 6);
        Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, px, py, 600, 250, 6);
        auto* font = Font::getInstance();
        int id = self->getID();
        bool willEnable = id >= 0 ? (id == 1) : cur;
        font->draw(willEnable ? "狀態：開啟" : "狀態：關閉", fs, px + 10, py + 10, willEnable ? Color{0, 255, 0, 255} : Color{255, 80, 80, 255});
        font->draw("戰鬥開始前可交換我方棋子位置", fs, px + 10, py + 10 + lh, {255, 215, 0, 255});
        font->draw("點擊兩個我方棋子即可互換位置", fs, px + 10, py + 10 + lh * 2, {255, 255, 255, 255});
        font->draw("右鍵確認完成佈陣", fs, px + 10, py + 10 + lh * 3, {255, 255, 255, 255});
    });
    sub->addDrawableOnCall(panel);
    sub->runAtPosition(150, 200);
    int sel = sub->getResult();
    if (sel == 0 || sel == 1)
        gd.setPositionSwapEnabled(sel == 1);
}

void ChessSelector::showGameGuide()
{
    auto box = std::make_shared<TextBox>();
    box->setText("　");
    box->setHaveBox(false);
    box->setFontSize(1);

    auto panel = std::make_shared<DrawableOnCall>([](DrawableOnCall*) {
        auto* font = Font::getInstance();
        int px = 90, py = 50, fs = 20, lh = fs + 6;
        Engine::getInstance()->fillRoundedRect({0, 0, 0, 180}, px, py, 1100, 620, 8);
        Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, px, py, 1100, 620, 8);

        int y = py + 15, x = px + 30;
        auto title = [&](const std::string& s) {
            font->draw(s, fs + 2, x, y, {255, 255, 100, 255}); y += lh + 4;
        };
        auto line = [&](const std::string& s, Color c = {220, 220, 220, 255}) {
            font->draw(s, fs, x, y, c); y += lh;
        };

        title("珍瓏棋局 · 遊戲說明");
        y += 4;
        line("這是一場自走棋對弈。招募武林中人，排兵佈陣，棋子上場後將自動戰鬥。");
        y += 6;

        title("基本流程");
        line("· 每回合在商店中花費金幣招募棋子並部署上陣");
        line("· 戰鬥自動進行，獲勝後進入下一回合");
        line("· 每四回合遭遇一位強敵(Boss)，擊敗可獲內功");
        line("· 共二十八回合，存活到最後即為通關");
        y += 6;

        title("棋子與升星");
        line("· 棋子分為1至5費，費用越高越稀有");
        line("· 集齊三個相同棋子自動合成二星，三個二星合成三星，升星後屬性大幅提升");
        y += 6;

        title("經濟系統");
        line("· 每回合獲得基礎金幣，存款產生利息(每10金幣額外+1，上限3)");
        line("· 金幣用於：招募棋子、刷新商店($2)、購買經驗($5)");
        line("· 提升等級可增加上陣人數與高費棋子出現概率");
        y += 6;

        title("羈絆");
        line("· 同門派棋子上陣達到一定數量可激活羈絆效果");
        line("· 羈絆提供攻擊、防禦、生命等多種加成，合理搭配羈絆是制勝關鍵");
        y += 12;

        font->draw("點擊任意處返回", fs - 2, px + 30, y, {150, 150, 150, 255});
    });
    box->addChild(panel);
    box->runAtPosition(0, 0);
}

}    //namespace KysChess
