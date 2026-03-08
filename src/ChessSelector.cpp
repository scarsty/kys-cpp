#include "ChessSelector.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <assert.h>
#include "Audio.h"
#include "BattleMap.h"
#include "BattleStatsView.h"
#include "UISave.h"
#include "ChessBalance.h"
#include "ChessCombo.h"
#include "ChessNeigong.h"
#include "ChessEquipment.h"
#include "ChessPool.h"
#include "ChessManager.h"
#include "ChessSelectorPresenter.h"
#include "DynamicChessMap.h"
#include "DrawableOnCall.h"
#include "ChessDrawableOnCall.h"
#include "UIStatusDrawable.h"
#include "Font.h"
#include "GameUtil.h"
#include "InputBox.h"
#include "Menu.h"
#include "Random.h"
#include "Save.h"
#include "TextBox.h"
#include "Talk.h"
#include "GameState.h"
#include <Engine.h>
#include <numeric>
#include <SuperMenuText.h>
#include <TextureManager.h>
#include <string>
#include <vector>

namespace KysChess
{

namespace
{

static ChessSelectorPresenter& presenter()
{
    static ChessSelectorPresenter value;
    return value;
}

static void showChessMessage(const std::string& text, int fontSize = 32)
{
    auto box = std::make_shared<TextBox>();
    box->setText(text);
    box->setFontSize(fontSize);
    box->runCentered(Engine::getInstance()->getUIHeight() / 2);
}

static void playChessUpgradeSound()
{
    Audio::getInstance()->playESound(72);
}

static ChessManager makeChessManager(ChessRoster& roster, ChessEquipmentInventory& equipmentInventory, ChessEconomy& economy)
{
    return ChessManager(roster, equipmentInventory, economy);
}

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
    case EffectType::ArmorPen: d = std::format("{}%穿甲", eff.triggerValue, eff.value); break;
    case EffectType::Stun: d = std::format("{}%眩暈({}幀)", eff.triggerValue, eff.value); break;
    case EffectType::KnockbackChance: d = std::format("{}%擊退", eff.value); break;
    case EffectType::PoisonDOT: d = eff.value2 ? std::format("中毒{}%×{}次(每30幀)", eff.value, eff.value2) : std::format("中毒{}%", eff.value); break;
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
    case EffectType::ControlImmunityFrames: d = std::format("僵直吸收{}幀", eff.value); break;
    case EffectType::KillHealPct: d = std::format("擊殺回{}%血", eff.value); break;
    case EffectType::KillInvincFrames: d = std::format("擊殺無敵{}幀", eff.value); break;
    case EffectType::PostSkillInvincFrames: d = std::format("技能後無敵{}幀", eff.value); break;
    case EffectType::DmgReductionPct: d = std::format("傷害減免{}%", eff.value); break;
    case EffectType::Bloodlust: d = std::format("击杀增攻+{}攻", eff.value); break;
    case EffectType::Adaptation: d = std::format("同敌减伤{}%({}层)", eff.value, eff.value2); break;
    case EffectType::RampingDmg: d = std::format("连击增伤+{}%({}层)", eff.value, eff.value2); break;
    case EffectType::HealBurst: d = std::format("回血{}%", eff.value); break;
    default: d = std::format("效果({})", eff.value); break;
    }
    return triggerPrefix() + d + countSuffix();
}

struct IndexedMenuConfig {
    int perPage = 10;
    int fontSize = 36;
    int x = 80;
    int y = 70;
    bool showNav = true;
    bool needInputBox = false;
    bool confirmation = false;
    bool exitable = true;
    std::vector<Color> outlineColors;
    std::vector<bool> animateOutlines;
    std::vector<int> outlineThicknesses;
};

static std::vector<ChessInstanceID> collectInstanceIds(const std::vector<Chess>& chesses)
{
    std::vector<ChessInstanceID> instanceIds;
    instanceIds.reserve(chesses.size());
    for (auto& chess : chesses)
        instanceIds.push_back(chess.id);
    return instanceIds;
}

static std::shared_ptr<ChessDrawableOnCall> makeComboInfoPanel(ChessManager manager)
{
    return std::make_shared<ChessDrawableOnCall>([manager](DrawableOnCall* self) {
        int roleId = -1;
        if (auto* chessDrawable = dynamic_cast<ChessDrawableOnCall*>(self)) {
            auto role = chessDrawable->getPreviewData().role;
            if (role) {
                roleId = role->ID;
            }
        }
        if (roleId < 0) return;

        auto roleCombos = ChessCombo::getCombosForRole(roleId);
        if (roleCombos.empty()) return;

        auto& allCombos = ChessCombo::getAllCombos();
        auto starByRole = ChessCombo::buildStarMap(manager.getSelectedForBattle());

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

static std::shared_ptr<DrawableOnCall> makeOwnedPanel(ChessRoster& roster, ChessManager manager)
{
    return std::make_shared<DrawableOnCall>([&roster, manager](DrawableOnCall*) {
        auto chessMap = roster.getChessCountMap();
        if (chessMap.empty()) return;

        auto* font = Font::getInstance();
        int px = 70, py = 420, fs = 20, lh = fs + 4;
        int pw = 600, ph = 290;
        Engine::getInstance()->fillRoundedRect({0, 0, 0, 160}, px, py, pw, ph, 8);
        Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, px, py, pw, ph, 8);
        font->draw("我方棋子", fs + 4, px + 10, py + 5, {255, 255, 100, 255});

        // Count how many of each chess type are deployed
        std::map<std::pair<int,int>, int> deployedCount;
        for (auto& s : manager.getSelectedForBattle())
            deployedCount[{s.role->ID, s.star}]++;

        // List each piece individually, deployed first (like sell/select menus)
        struct Entry { std::string label; Color color; };
        std::vector<Entry> deployed, bench;
        for (auto& [roleAndStar, count] : chessMap)
        {
            auto key = std::make_pair(roleAndStar.role->ID, roleAndStar.star);
            int dep = deployedCount[key];
            Color col = ChessPool::GetTierColor(ChessPool::GetChessTier(roleAndStar.role->ID));
            std::string stars;
            for (int i = 0; i < roleAndStar.star; i++) stars += "★";
            std::string baseName = std::format("{}{}", roleAndStar.role->Name, stars);

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

static SuperMenuTextExtraOptions makeMenuOptions(const IndexedMenuData& data, const IndexedMenuConfig& config)
{
    SuperMenuTextExtraOptions opts;
    opts.itemColors_ = data.colors;
    opts.outlineColors_ = config.outlineColors;
    opts.animateOutlines_ = config.animateOutlines;
    opts.outlineThicknesses_ = config.outlineThicknesses;
    opts.needInputBox_ = config.needInputBox;
    opts.confirmation_ = config.confirmation;
    opts.exitable_ = config.exitable;
    return opts;
}

static std::shared_ptr<SuperMenuText> makeIndexedMenu(
    const std::string& title,
    const IndexedMenuData& data,
    IndexedMenuConfig config = {},
    const std::vector<std::shared_ptr<DrawableOnCall>>& drawables = {},
    const std::vector<Chess>& itemPreviewData = {})
{
    auto menu = std::make_shared<SuperMenuText>(title, config.fontSize, data.labels, config.perPage, makeMenuOptions(data, config));
    menu->setInputPosition(config.x, config.y);

    if (!itemPreviewData.empty())
    {
        auto statusDrawable = std::make_shared<UIStatusDrawable>(itemPreviewData);
        statusDrawable->getUIStatus().setPosition(720, 40);
        menu->addDrawableOnCall(statusDrawable);
    }

    for (auto& drawable : drawables)
        menu->addDrawableOnCall(drawable);

    if (!config.showNav) menu->setShowNavigationButtons(false);
    menu->setDoubleTapMode(GameUtil::isMobileDevice());
    return menu;
}

static std::shared_ptr<DrawableOnCall> makeHeaderBar(ChessEconomy& economy, ChessProgress& progress, ChessManager manager)
{
    return std::make_shared<DrawableOnCall>([&economy, &progress, manager](DrawableOnCall*) {
        auto& cfg = ChessBalance::config();
        auto* engine = Engine::getInstance();
        auto* font = Font::getInstance();
        int w = engine->getUIWidth(), barH = 32, fs = 18, x = 10, y = 6;

        engine->fillColor({0, 0, 0, 180}, 0, 0, w, barH);

        auto seg = [&](const std::string& s, Color c) {
            font->draw(s, fs, x, y, c);
            x += fs * Font::getTextDrawSize(s) / 2 + 12;
        };

        int fight = progress.battleProgress().getFight();
        seg(std::format("第{}關{}", fight + 1, progress.battleProgress().isBossFight() ? "(Boss)" : ""), {255, 200, 100, 255});
        seg(std::format("${}", economy.getMoney()), {255, 215, 0, 255});
        seg(std::format("Lv{} {}/{}", economy.getLevel() + 1, economy.getExp(), economy.getExpForNextLevel()), {100, 200, 255, 255});
        seg(std::format("出戰{}/{}", manager.getSelectedCount(), economy.getMaxDeploy()), {100, 255, 100, 255});
        seg(std::format("背包{}/{}", manager.getBenchCount(), cfg.benchSize), {200, 180, 255, 255});
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
    std::string tierName[] = {"初階", "中階", "高階", "傳說"};
    font->draw(std::format("層級: {}", tierName[std::min(ng.tier - 1, 3)]), fs, dx, ty, {200, 200, 200, 255}); ty += fs + 4;
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

static void drawEquipmentDetail(const EquipmentDef& eq, int count, const std::string& equippedBy)
{
    auto* font = Font::getInstance();
    int px = 630, py = 170, fs = 28;
    Engine::getInstance()->fillRoundedRect({0, 0, 0, 180}, px, py, 500, 400, 8);
    Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, px, py, 500, 400, 8);
    auto* item = eq.getItem();
    if (item) TextureManager::getInstance()->renderTexture("item", eq.itemId, px + 10, py + 10);
    int ty = py + 10;
    int dx = px + 100;
    font->draw(item ? item->Name : "???", fs + 4, dx, ty, {255, 255, 100, 255}); ty += fs + 10;
    std::string tierName[] = {"初階", "中階", "高階", "傳說"};
    Color tierColors[] = {{100, 200, 100, 255}, {100, 150, 255, 255}, {255, 150, 50, 255}, {255, 100, 255, 255}};
    font->draw(std::format("層級: {}", tierName[std::min(eq.tier - 1, 3)]), fs, dx, ty, tierColors[std::min(eq.tier - 1, 3)]); ty += fs + 4;
    font->draw(std::format("擁有: x{}", count), fs, dx, ty, count > 0 ? Color{0, 255, 0, 255} : Color{180, 180, 180, 255}); ty += fs + 4;
    if (!equippedBy.empty())
        { font->draw(std::format("裝備於: {}", equippedBy), fs, dx, ty, {100, 200, 255, 255}); ty += fs + 4; }

    ty = py + 120;
    int col1 = px + 10;

    if (!eq.effects.empty())
    {
        font->draw("特殊效果:", fs, col1, ty, {255, 200, 100, 255}); ty += fs + 4;
        for (auto& eff : eq.effects)
        {
            font->draw(comboEffectDesc(eff), fs - 2, col1, ty, {220, 220, 100, 255});
            ty += fs;
        }
    }
}

}    //namespace

ChessSelector::ChessSelector(
    ChessRoleSave& roleSave,
    ChessEquipmentInventory& equipmentInventory,
    ChessRoster& roster,
    ChessShop& shop,
    ChessProgress& progress,
    ChessEconomy& economy,
    ChessRandom& random)
    : roleSave_(roleSave)
    , equipmentInventory_(equipmentInventory)
    , roster_(roster)
    , shop_(shop)
    , progress_(progress)
    , economy_(economy)
    , random_(random)
{
}

Role* ChessSelector::getRoleById(int roleId) const
{
    return roleSave_.getRole(roleId);
}

void ChessSelector::getChess()
{
    auto manager = makeChessManager(roster_, equipmentInventory_, economy_);
    for (;;)
    {
        auto rollOfChess = shop_.pool().getChessFromPool(economy_.getLevel());

        for (;;)
        {
            ChessMenuData menuData;
            IndexedMenuConfig menuConfig;
            menuConfig.fontSize = 32;
            menuConfig.showNav = false;
            menuData.labels.push_back(std::format("刷新               ${}", ChessBalance::config().refreshCost));
            menuData.colors.push_back({ 255, 204, 229, 255 });
            menuData.previewData.push_back({});
            menuConfig.outlineColors.push_back({0, 0, 0, 0});
            menuConfig.animateOutlines.push_back(false);
            menuConfig.outlineThicknesses.push_back(1);

            menuData.labels.push_back(shop_.isLocked() ? "[已鎖定] 點擊解鎖" : "[未鎖定] 點擊鎖定");
            menuData.colors.push_back(shop_.isLocked() ? Color{ 255, 80, 80, 255 } : Color{ 128, 128, 128, 255 });
            menuData.previewData.push_back({});
            menuConfig.outlineColors.push_back({0, 0, 0, 0});
            menuConfig.animateOutlines.push_back(false);
            menuConfig.outlineThicknesses.push_back(1);

            auto chessMap = roster_.getChessCountMap();

            auto addRoleWithTier = [&](Role* role, int tier)
            {
                auto [name, color] = presenter().formatChessName(role, tier, {}, {});
                menuData.labels.push_back(name);
                menuData.colors.push_back(color);
                menuData.previewData.push_back({ role, 1, -1 });

                // Check ownership status across all star levels
                Chess c1 = { role, 1 };
                bool owned = false;
                for (auto& [roleAndStar, cnt] : chessMap)
                {
                    if (roleAndStar.role == role && cnt > 0)
                    {
                        // A single 3-star piece can't be starred up further, so don't mark as owned
                        if (roleAndStar.star == 3 && cnt == 1)
                        {
                            // Check if this is the ONLY entry for this role
                            bool hasOtherEntries = false;
                            for (auto& [roleAndStar, cnt2] : chessMap)
                            {
                                if (roleAndStar.role == role && roleAndStar.star != 3 && cnt2 > 0) { hasOtherEntries = true; break; }
                            }
                            if (!hasOtherEntries) continue;
                        }
                        owned = true;
                        break;
                    }
                }
                bool wouldStarUp = roster_.wouldMerge(c1.role, c1.star);

                if (wouldStarUp)
                {
                    menuConfig.outlineColors.push_back({255, 215, 0, 255});
                    menuConfig.animateOutlines.push_back(true);
                    menuConfig.outlineThicknesses.push_back(3);
                }
                else if (owned)
                {
                    menuConfig.outlineColors.push_back({100, 200, 255, 220});
                    menuConfig.animateOutlines.push_back(true);
                    menuConfig.outlineThicknesses.push_back(1);
                }
                else
                {
                    menuConfig.outlineColors.push_back({0, 0, 0, 0});
                    menuConfig.animateOutlines.push_back(false);
                    menuConfig.outlineThicknesses.push_back(1);
                }
            };

            for (auto [role, star] : rollOfChess)
            {
                addRoleWithTier(role, star);
            }

            menuConfig.perPage = static_cast<int>(menuData.labels.size());
            auto menu = makeIndexedMenu(
                std::format("購買棋子 等級{} ${} 背包{}/{}", economy_.getLevel() + 1, economy_.getMoney(), manager.getBenchCount(), ChessBalance::config().benchSize),
                menuData,
                menuConfig,
                {makeComboInfoPanel(manager), makeOwnedPanel(roster_, manager)},
                menuData.previewData);

            menu->run();

            int selectedId = menu->getResult();
            if (selectedId < 0)
            {
                return;
            }
            else if (selectedId == 0)
            {
                if (!economy_.spend(ChessBalance::config().refreshCost)) continue;
                shop_.pool().refresh();
                shop_.setLocked(false);
                break;
            }
            else if (selectedId == 1)
            {
                shop_.setLocked(!shop_.isLocked());
                continue;
            }
            else if (selectedId >= 2)
            {
                auto actualIdx = selectedId - 2;
                auto [role, tier] = rollOfChess[actualIdx];
                if (manager.isBenchFull() && !roster_.wouldMerge(role, 1))
                {
                    showChessMessage("背包已滿！請先出售棋子");
                    continue;
                }
                auto result = manager.purchaseChess(role, tier);
                if (result.success) {
                    shop_.pool().removeChessAt(actualIdx);
                    if (result.merged) {
                        showChessMessage(std::format("消費{}，{}升星！", result.cost, role->Name));
                        playChessUpgradeSound();
                    } else {
                        showChessMessage(std::format("消費{}，獲取{}", result.cost, role->Name));
                    }
                    break;
                }
                continue;
            }
        }
    }
}

void ChessSelector::sellChess()
{
    auto manager = makeChessManager(roster_, equipmentInventory_, economy_);

    for (;;)
    {
        auto chessList = roster_.items();
        if (chessList.empty()) return;

        std::vector<ChessMenuEntry> entries;
        for (const auto& [instanceId, chess] : chessList)
        {
            entries.push_back({chess, chess.selectedForBattle ? "[出戰]" : ""});
        }

        auto menuData = presenter().buildChessMenuData(entries);
        IndexedMenuConfig menuConfig;
        menuConfig.perPage = 12;
        menuConfig.fontSize = 32;
        auto menu = makeIndexedMenu(
            std::format("出售棋子 背包{}/{}", manager.getBenchCount(), ChessBalance::config().benchSize),
            menuData,
            menuConfig,
            {},
            menuData.previewData);
        menu->run();

        // Result returns index only, so will index into the inital list
        int selectedId = menu->getResult();
        if (selectedId < 0) break;

        auto result = manager.sellChess(entries[selectedId].chess.id);
        showChessMessage(std::format("售出棋子{}，獲得${}", result.role->Name, result.price));
    }
}

void ChessSelector::selectForBattle()
{
    auto manager = makeChessManager(roster_, equipmentInventory_, economy_);
    auto& pieces = roster_.items();

    if (pieces.empty())
    {
        return;
    }
    int maxSelection = economy_.getMaxDeploy();

    for (;;)
    {
        std::vector<ChessMenuEntry> entries;

        int selectedCount{};
        for (const auto& [id, chess] : roster_.items()) {
            entries.push_back({chess, chess.selectedForBattle ? "[出戰]" : ""});
            if (chess.selectedForBattle) {
                selectedCount += 1;
            }
        }
        std::sort(entries.begin(), entries.end(), [](const ChessMenuEntry& left, const ChessMenuEntry& right){
            // if selected for Battle then its prioritized
            return std::make_pair(left.chess.selectedForBattle ? 0 : 1, left.chess.id) <
                 std::make_pair(right.chess.selectedForBattle ? 0 : 1, right.chess.id);
        });

        auto menuData = presenter().buildChessMenuData(entries);
        for (size_t i = 0; i < selectedCount; ++i)
            menuData.colors[i] = { 255, 215, 0, 255 };

        std::string menuTitle = std::format("選擇出戰棋子 {}/{} 背包{}/{}", selectedCount, maxSelection, manager.getBenchCount(), ChessBalance::config().benchSize);
        IndexedMenuConfig menuConfig;
        menuConfig.perPage = 12;
        menuConfig.fontSize = 32;
        auto menu = makeIndexedMenu(menuTitle, menuData, menuConfig, {makeComboInfoPanel(manager)}, menuData.previewData);

        menu->run();

        int selectedIdx = menu->getResult();
        if (selectedIdx < 0)
        {
            return;
        }
        else
        {
            // Just toggle and done.
            auto chess = entries[selectedIdx].chess;
            if (chess.selectedForBattle) {
                manager.setSelectedForBattle(chess.id, false);
            } else if (selectedCount >= maxSelection) {
                showChessMessage(std::format("最多只能選擇{}個棋子", maxSelection));
            } else {
                manager.setSelectedForBattle(chess.id, true);
            }
        }
    }
}

void ChessSelector::enterBattle()
{
    auto manager = makeChessManager(roster_, equipmentInventory_, economy_);
    auto& battleProgress = progress_.battleProgress();

    // Check if game is complete
    if (battleProgress.isGameComplete())
    {
        showChessMessage("恭喜通關！");
        return;
    }

    // Check if we have selected chess
    auto selectedChess = manager.getSelectedForBattle();
    if (selectedChess.empty())
    {
        showChessMessage("請先選擇出戰棋子");
        return;
    }

    // Save enemy random counter before generation for retry determinism
    auto savedEnemyCallCount = random_.getEnemyCallCount();

    // Generate enemies based on progress
    std::vector<int> enemyIds;
    std::vector<int> enemyStars;
    int fightNum = battleProgress.getFight();
    bool isBoss = battleProgress.isBossFight();

    // Generate enemies from table
    auto& ecfg = ChessBalance::config();
    int tableIdx = std::min(fightNum, (int)ecfg.enemyTable.size() - 1);
    auto& slots = ecfg.enemyTable[tableIdx];

    for (auto& slot : slots)
    {
        auto role = shop_.pool().selectEnemyFromPool(slot.tier);
        enemyIds.push_back(role->ID);
        enemyStars.push_back(std::min(slot.star, 3));
    }

    // Build roles from selected chess + generated enemies
    // This could be improved, but let it be for now since we
    // allow test battles where "our side" can be config driven.
    DynamicBattleRoles roles;
    for (auto& c : selectedChess)
    {
        roles.teammate_ids.push_back(c.role->ID);
        roles.teammate_stars.push_back(c.star);
        roles.teammate_instances.push_back(c.id.value);
    }
    roles.enemy_ids = enemyIds;
    roles.enemy_stars = enemyStars;

    // Generate enemy equipment
    roles.enemy_weapons.resize(enemyIds.size(), -1);
    roles.enemy_armors.resize(enemyIds.size(), -1);
    int maxTier = 0, equipCount = 0;
    for (auto& level : ecfg.enemyEquipmentLevels)
        if (fightNum >= level.fight) { maxTier = level.maxTier; equipCount = level.count; }
    if (maxTier > 0 && equipCount > 0)
    {
        auto tierEquip = ChessEquipment::getByTier(maxTier);
        if (!tierEquip.empty())
        {
            std::vector<int> indices(enemyStars.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::sort(indices.begin(), indices.end(), [&](int a, int b) { return enemyStars[a] > enemyStars[b]; });
            for (int i = 0; i < std::min(equipCount, (int)indices.size()); i++)
            {
                auto* equip = tierEquip[random_.enemyRandInt(static_cast<int>(tierEquip.size()))];
                if (equip->equipType == 0) roles.enemy_weapons[indices[i]] = equip->itemId;
                else roles.enemy_armors[indices[i]] = equip->itemId;
            }
        }
    }

    // Seed battle RNG deterministically so retries produce same combat
    int battleSeed = static_cast<int>(random_.enemyRandInt(INT_MAX));

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
        int interest = std::min(economy_.getMoney() * ecfg.interestPercent / 100, ecfg.interestMax);
        economy_.make(reward + interest);
        int expGain = isBoss ? ecfg.bossBattleExp : ecfg.battleExp;
        int oldLvl = economy_.getLevel();
        economy_.increaseExp(expGain);
        if (isBoss) showNeigongReward();
        battleProgress.advance();
        if (!shop_.isLocked())
        {
            shop_.pool().refresh();
        }
        auto text = std::make_shared<TextBox>();
        std::string lvlMsg = (economy_.getLevel() > oldLvl) ? std::format(" 升級！等級{}", economy_.getLevel() + 1) : "";
        std::string nextInfo = battleProgress.isGameComplete() ? " 通關！"
            : battleProgress.isBossFight() ? std::format(" 下一關：第{}關(Boss)", battleProgress.getFight() + 1)
            : std::format(" 下一關：第{}關", battleProgress.getFight() + 1);
        std::string interestMsg = interest > 0 ? std::format("(利息+${})", interest) : "";
        text->setText(std::format("勝利！獲得${}{} 經驗+{}{}{}", reward, interestMsg, expGain, lvlMsg, nextInfo));
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);

        if (battleProgress.isGameComplete())
        {
            const char* diffName = (ChessBalance::getDifficulty() == KysChess::Difficulty::Normal) ? "正常" : "挑戰";
            auto outro = std::make_shared<Talk>(
                std::format("少俠果然不凡！珍瓏棋局已破({}難度)。"
                "若有興趣，可嘗試「遠征挑戰」，那裡有更強的對手等著你。"
                "當然，你也可以嘗試其他羈絆組合，探索更多可能。", diffName), 115);
            outro->run();
        }

        UISave::autoSave();
    }
    else
    {
        // Reset enemy random counter so retry generates same enemies
        random_.setEnemyCallCount(savedEnemyCallCount);
        random_.restore();
        auto text = std::make_shared<TextBox>();
        text->setText("戰鬥失敗！請調整陣容後再試");
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);
    }
}

int ChessSelector::runBattle(const DynamicBattleRoles& roles, const std::vector<Chess>& allyChess, int battle_id, int seed)
{
    ChessManager chessManager(roster_, equipmentInventory_, economy_);

    // Build enemy Chess vector for combo detection
    std::vector<Chess> enemyChessVec;
    for (size_t i = 0; i < roles.enemy_ids.size(); i++)
    {
        auto r = getRoleById(roles.enemy_ids[i]);
        if (r) enemyChessVec.push_back({r, i < roles.enemy_stars.size() ? roles.enemy_stars[i] : 1});
    }

    // Detect combos for both teams
    auto allyCombos = ChessCombo::detectCombos(allyChess);
    auto enemyCombos = ChessCombo::detectCombos(enemyChessVec);

    // Pre-battle stats
    {
        auto info = BattleMap::getInstance()->getBattleInfo(battle_id);
        int musicId = info ? info->Music : -1;
        auto view = std::make_shared<BattleStatsView>(roleSave_, chessManager);
        view->setupPreBattle(allyChess, roles.enemy_ids, roles.enemy_stars, allyCombos, enemyCombos, musicId);
        view->run();
    }

    // Create and run battle
    auto battle = DynamicChessMap::createBattle(
        roles,
        random_,
        roleSave_,
        progress_,
        chessManager,
        battle_id);
    if (seed >= 0)
        battle->rand_.set_seed(static_cast<unsigned int>(seed));
    battle->run();

    // Post-battle stats
    {
        auto view = std::make_shared<BattleStatsView>(roleSave_, chessManager);
        view->setupPostBattle(battle->getFriendsObj(), battle->getEnemiesObj(), battle->getTracker(), battle->getResult());
        view->run();
    }

    return battle->getResult();
}

void ChessSelector::buyExp()
{
    auto& bcfg = ChessBalance::config();
    if (economy_.getLevel() >= bcfg.maxLevel)
    {
        showChessMessage("已達最高等級！");
        return;
    }

    int curLvl = economy_.getLevel();
    int nextLvl = curLvl + 1;
    int curPieces = economy_.getMaxDeploy();
    int nextPieces = std::max(nextLvl + 1, bcfg.minBattleSize);
    bool atNextMax = nextLvl >= bcfg.maxLevel;

    auto weightStr = [&](int lvl) -> std::string {
        if (lvl >= (int)bcfg.shopWeights.size()) return "已滿級";
        auto& w = bcfg.shopWeights[lvl];
        return std::format("1費{}% 2費{}% 3費{}% 4費{}% 5費{}%", w[0], w[1], w[2], w[3], w[4]);
    };

    // Pre-compute strings so lambda only captures values
    std::string hdr = std::format("等級 {}    經驗 {}/{}    金幣 ${}",
        curLvl + 1, economy_.getExp(), economy_.getExpForNextLevel(), economy_.getMoney());
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
    if (!economy_.spend(bcfg.buyExpCost)) return;

    int oldLvl = economy_.getLevel();
    economy_.increaseExp(bcfg.buyExpAmount);
    if (economy_.getLevel() > oldLvl)
        showChessMessage(std::format("升級！等級{} 經驗{}/{}", economy_.getLevel() + 1, economy_.getExp(), economy_.getExpForNextLevel()));
    else
        showChessMessage(std::format("經驗{}/{}", economy_.getExp(), economy_.getExpForNextLevel()));
}

void ChessSelector::showContextMenu()
{
    //ChessBalance::apply();
    while (1) {
        auto menu = std::make_shared<MenuText>(std::vector<std::string>{ "購買棋子", "出售棋子", "選擇出戰", "進入戰鬥", "購買經驗", "查看羈絆", "查看內功", "遠征挑戰", "排兵佈陣", "裝備管理", "遊戲說明" });
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
        case 9: manageEquipment(); break;
        case 10: showGameGuide(); break;
        case -1: return;
        }
        UISave::autoSave();
    }
}

void ChessSelector::viewCombos()
{
    auto& combos = ChessCombo::getAllCombos();
    auto manager = makeChessManager(roster_, equipmentInventory_, economy_);

    // Build star map (owned = present in map; value = star level)
    auto starByRole = ChessCombo::buildStarMap(manager.getSelectedForBattle());

    // Build menu items: one per combo
    IndexedMenuData menuData;
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
        menuData.labels.push_back(label);
        menuData.colors.push_back(owned > 0 ? Color{0, 255, 0, 255} : Color{180, 180, 180, 255});
    }

    // Detail panel drawable — two-column: members left, thresholds right
    auto& roleSave = roleSave_;
    auto detailDraw = std::make_shared<DrawableOnCall>([&combos, &starByRole, &roleSave](DrawableOnCall* self) {
        int idx = self->getItemIndex();
        if (idx < 0 || idx >= (int)combos.size()) return;
        auto& c = combos[idx];
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
            auto role = roleSave.getRole(rid);
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

    IndexedMenuConfig menuConfig;
    menuConfig.x = 80;
    menuConfig.y = 60;
    auto menu = makeIndexedMenu("羈絆一覽", menuData, menuConfig, {detailDraw});
    menu->run();
}

void ChessSelector::showNeigongReward()
{
    auto& cfg = ChessBalance::config();
    auto& ngCfg = ChessNeigong::config();
    auto& pool = ChessNeigong::getPool();
    if (pool.empty()) return;

    // Determine boss index and available tiers
    int bossIdx = progress_.battleProgress().getFight() / cfg.bossInterval;
    std::vector<int> availTiers;
    for (auto it = ngCfg.tiersByBoss.rbegin(); it != ngCfg.tiersByBoss.rend(); ++it)
    {
        if (it->first <= bossIdx) { availTiers = it->second; break; }
    }
    if (availTiers.empty()) availTiers = {1};

    // Filter: matching tiers, not already obtained
    auto& obtained = progress_.getObtainedNeigong();
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
            std::swap(tmp[i], tmp[random_.shopRandInt(i + 1)]);
        choices.assign(tmp.begin(), tmp.begin() + n);
        return choices;
    };

    auto choices = pickChoices();

    for (;;)
    {
        IndexedMenuData menuData;

        std::string tierName[] = {"初階", "中階", "高階", "傳說"};

        for (int i = 0; i < (int)choices.size(); ++i)
        {
            auto* ng = choices[i];
            std::string padded = ng->name;
            while (padded.size() < 15) padded += "\xe3\x80\x80";
            std::string label = std::format("[{}] {}", tierName[std::min(ng->tier - 1, 2)], padded);
            menuData.labels.push_back(label);
            Color tierColors[] = {{175, 238, 238}, {100, 255, 100}, {255, 200, 100}};
            menuData.colors.push_back(tierColors[std::min(ng->tier - 1, 2)]);
        }

        if (!rerolled)
        {
            menuData.labels.push_back(std::format("刷新             ${}", ngCfg.rerollCost));
            menuData.colors.push_back({128, 128, 128});
        }

        // Detail panel
        auto iconPanel = std::make_shared<DrawableOnCall>([&choices](DrawableOnCall* self) {
            int idx = self->getItemIndex();
            if (idx < 0 || idx >= (int)choices.size()) return;
            drawNeigongDetail(*choices[idx]);
        });

        IndexedMenuConfig menuConfig;
        menuConfig.perPage = static_cast<int>(menuData.labels.size());
        menuConfig.exitable = false;
        auto menu = makeIndexedMenu("選擇内功，内功將對所有成員自動生效", menuData, menuConfig, {iconPanel});

        menu->run();
        int sel = menu->getResult();
        if (!rerolled && sel == static_cast<int>(choices.size()))
        {
            if (economy_.spend(ngCfg.rerollCost))
            {
                rerolled = true;
                choices = pickChoices();
                continue;
            }
            continue;
        }

        if (sel < 0) continue;  // don't allow escape — must pick

        // Pick this neigong
        progress_.addNeigong(choices[sel]->magicId);
        showChessMessage(std::format("獲得內功：{}", choices[sel]->name));
        return;
    }
}

void ChessSelector::viewNeigong()
{
    auto& pool = ChessNeigong::getPool();
    if (pool.empty()) return;

    auto& obtained = progress_.getObtainedNeigong();
    std::set<int> obtainedSet(obtained.begin(), obtained.end());

    IndexedMenuData menuData;
    std::string tierName[] = {"初階", "中階", "高階", "傳說"};

    for (int i = 0; i < (int)pool.size(); ++i)
    {
        auto& ng = pool[i];
        bool owned = obtainedSet.count(ng.magicId) > 0;
        std::string padded = ng.name;
        while (padded.size() < 15) padded += "\xe3\x80\x80";  // fullwidth space
        std::string label = std::format("[{}] {}{}", tierName[std::min(ng.tier - 1, 3)], padded, owned ? " ✓" : "　 ");
        menuData.labels.push_back(label);
        menuData.colors.push_back(owned ? Color{0, 255, 0, 255} : Color{120, 120, 120, 255});
    }

    auto detailDraw = std::make_shared<DrawableOnCall>([&pool, &obtainedSet](DrawableOnCall* self) {
        int idx = self->getItemIndex();
        if (idx < 0 || idx >= (int)pool.size()) return;
        auto& ng = pool[idx];
        drawNeigongDetail(ng, obtainedSet.count(ng.magicId) ? 1 : 0);
    });

    auto menu = makeIndexedMenu("内功一覽", menuData, {}, {detailDraw});
    menu->run();
}

void ChessSelector::manageEquipment()
{
    auto manager = makeChessManager(roster_, equipmentInventory_, economy_);
    auto& allEquip = ChessEquipment::getAll();
    if (allEquip.empty()) return;

    std::string tierName[] = {"初階", "中階", "高階", "傳說"};

    struct EquipmentListEntry {
        int index;
        std::string label;
        Color color;
        int inventoryCount;
    };

    while (true)
    {
        std::vector<EquipmentListEntry> entries;

        for (size_t i = 0; i < allEquip.size(); ++i)
        {
            auto& eq = allEquip[i];
            auto stats = equipmentInventory_.getItemStats(eq.itemId);
            auto* item = eq.getItem();
            std::string name = item ? item->Name : "???";
            while (name.size() < 15) name += "\xe3\x80\x80";
            std::string label = std::format("[{}] {} x{} (已裝:{})", tierName[std::min(eq.tier - 1, 3)], name, stats.totalCount, stats.equippedCount);
            entries.push_back({
                static_cast<int>(i),
                std::move(label),
                stats.totalCount > 0 ? Color{0, 255, 0, 255} : Color{120, 120, 120, 255},
                stats.totalCount
            });
        }

        std::sort(entries.begin(), entries.end(), [&](const auto& a, const auto& b) {
            if ((a.inventoryCount > 0) != (b.inventoryCount > 0))
                return a.inventoryCount > 0;
            return a.index < b.index;
        });

        IndexedMenuData menuData;
        menuData.labels.reserve(entries.size());
        menuData.colors.reserve(entries.size());
        for (auto& entry : entries)
        {
            menuData.labels.push_back(entry.label);
            menuData.colors.push_back(entry.color);
        }

        auto detailDraw = std::make_shared<DrawableOnCall>([this, &entries, &allEquip](DrawableOnCall* self) {
            int idx = self->getItemIndex();
            if (idx < 0 || idx >= (int)entries.size()) return;
            auto& eq = allEquip[entries[idx].index];
            auto stats = equipmentInventory_.getItemStats(eq.itemId);
            std::string equippedBy = presenter().buildEquippedBy(roster_.items(), eq.itemId);
            drawEquipmentDetail(eq, stats.totalCount, equippedBy);
        });

        auto menu = makeIndexedMenu("裝備一覽", menuData, {}, {detailDraw});
        menu->run();
        int sel = menu->getResult();
        if (sel < 0) break;

        auto& eq = allEquip[entries[sel].index];

        auto stats = equipmentInventory_.getItemStats(eq.itemId);
        if (stats.totalCount <= 0) continue;
        assert(stats.availableInstanceId != k_nonExistentItem);
        std::vector<ChessMenuEntry> chessEntries;

        for (const auto& [instanceId, chess] : roster_.items())
        {
            chessEntries.push_back({chess, chess.selectedForBattle ? "[出戰] " : ""});
        }

        if (chessEntries.empty())
        {
            showChessMessage("沒有可裝備的棋子");
            continue;
        }

        auto menuData2 = presenter().buildChessMenuData(chessEntries);
        IndexedMenuConfig menuConfig2;
        menuConfig2.perPage = 12;
        menuConfig2.fontSize = 32;
        auto menu2 = makeIndexedMenu("選擇棋子", menuData2, menuConfig2, {}, menuData2.previewData);
        menu2->run();
        int selectedIdx = menu2->getResult();
        if (selectedIdx < 0) continue;
        
        manager.equipItem(chessEntries[selectedIdx].chess.id, eq, stats.availableInstanceId);
    }
}

// Helper: Select reward from available options
int ChessSelector::selectChallengeReward(const std::vector<BalanceConfig::ChallengeReward>& rewards)
{
    auto isRewardAvailable = [&](const BalanceConfig::ChallengeReward& r) -> bool {
        using RT = BalanceConfig::ChallengeRewardType;
        if (r.type == RT::StarUp1to2 || r.type == RT::StarUp2to3)
        {
            int fromStar = (r.type == RT::StarUp1to2) ? 1 : 2;
            for (auto& [instanceId, chess] : roster_.items())
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
            auto& obtained = progress_.getObtainedNeigong();
            std::set<int> obtainedSet(obtained.begin(), obtained.end());
            for (auto& ng : pool)
            {
                if (ng.tier <= r.value && !obtainedSet.count(ng.magicId)) return true;
            }
            return false;
        }
        return true;
    };

    IndexedMenuData menuData;
    std::vector<bool> avail;
    for (int i = 0; i < (int)rewards.size(); ++i)
    {
        bool available = isRewardAvailable(rewards[i]);
        avail.push_back(available);
        std::string desc = presenter().challengeRewardDesc(rewards[i]);
        if (!available) desc += "（無可用）";
        menuData.labels.push_back(desc);
        menuData.colors.push_back(available ? Color{255, 255, 100, 255} : Color{120, 120, 120, 255});
    }

    for (;;)
    {
        IndexedMenuConfig menuConfig;
        menuConfig.perPage = static_cast<int>(menuData.labels.size());
        menuConfig.exitable = false;
        auto menu = makeIndexedMenu("選擇獎勵", menuData, menuConfig);
        menu->run();
        int sel = menu->getResult();
        if (sel < 0) sel = 0;
        if (avail[sel]) return sel;

        showChessMessage("該獎勵無可用項目，請重新選擇");
    }
}

bool ChessSelector::rewardGold(int amount)
{
    makeChessManager(roster_, equipmentInventory_, economy_).applyGoldReward(amount);
    showChessMessage(std::format("獲得{}金幣！", amount));
    return true;
}

bool ChessSelector::addChessPiece(Role* role, bool showMessage)
{
    auto manager = makeChessManager(roster_, equipmentInventory_, economy_);
    auto result = manager.grantChess(role);
    if (!result.success) {
        if (showMessage) {
            showChessMessage("背包已滿！請先出售棋子");
        }
        return false;
    }

    if (showMessage) {
        if (result.merged) {
            showChessMessage(std::format("{}升星！", role->Name));
            playChessUpgradeSound();
        } else {
            showChessMessage(std::format("獲得棋子：{}", role->Name));
        }
    }
    return true;
}

bool ChessSelector::rewardPiece(int maxTier)
{
    std::vector<std::string> items;
    std::vector<Color> colors;
    std::vector<Chess> previewData;
    std::vector<int> roleIds;
    for (int t = 1; t <= maxTier && t <= 5; ++t)
    {
        for (int rid : ChessPool::getRolesOfTier(t))
        {
            auto* role = this->getRoleById(rid);
            if (!role) continue;
            auto [name, color] = presenter().formatChessName(role, t, 1, {});
            items.push_back(name);
            colors.push_back(color);
            previewData.push_back({ role, 1, -1 });
            roleIds.push_back(rid);
        }
    }
    IndexedMenuData menuData{items, colors};
    IndexedMenuConfig menuConfig;
    menuConfig.perPage = 12;
    menuConfig.fontSize = 32;
    auto menu = makeIndexedMenu("選擇棋子", menuData, menuConfig, {makeComboInfoPanel(makeChessManager(roster_, equipmentInventory_, economy_))}, previewData);
    menu->run();
    int sel = menu->getResult();
    if (sel >= 0)
    {
        auto* role = this->getRoleById(roleIds[sel]);
        if (role)
            return addChessPiece(role, true);
    }
    return false;
}

bool ChessSelector::rewardNeigong(int maxTier)
{
    auto& pool = ChessNeigong::getPool();
    auto& obtained = progress_.getObtainedNeigong();
    std::set<int> obtainedSet(obtained.begin(), obtained.end());

    IndexedMenuData menuData;
    std::vector<const NeigongDef*> ptrs;
    for (auto& ng : pool)
    {
        if (ng.tier > maxTier || obtainedSet.count(ng.magicId)) continue;
        menuData.labels.push_back(ng.name);
        menuData.colors.push_back({100, 255, 100, 255});
        ptrs.push_back(&ng);
    }
    if (menuData.labels.empty())
    {
        showChessMessage("沒有可選的內功");
        return false;
    }

    auto detail = std::make_shared<DrawableOnCall>([&ptrs](DrawableOnCall* self) {
        int idx = self->getItemIndex();
        if (idx >= 0 && idx < (int)ptrs.size()) drawNeigongDetail(*ptrs[idx]);
    });
    IndexedMenuConfig menuConfig;
    menuConfig.perPage = 16;
    menuConfig.exitable = false;
    auto menu = makeIndexedMenu("選擇內功，内功將對所有成員自動生效", menuData, menuConfig, {detail});
    menu->run();
    int sel = menu->getResult();
    if (sel >= 0)
    {
        progress_.addNeigong(ptrs[sel]->magicId);
        showChessMessage(std::format("獲得內功：{}", ptrs[sel]->name));
        return true;
    }
    return false;
}

bool ChessSelector::rewardStarUp(int fromStar, int maxTier)
{
    auto manager = makeChessManager(roster_, equipmentInventory_, economy_);
    int toStar = fromStar + 1;

    if (!manager.applyStarUpReward(fromStar, maxTier))
    {
        showChessMessage("沒有可升星的棋子");
        return false;
    }

    auto chesses = roster_.getByStarAndTier(fromStar, maxTier);

    std::vector<std::string> items;
    std::vector<Color> colors;
    std::vector<Chess> previewData;
    for (auto& chess : chesses)
    {
        int tier = ChessPool::GetChessTier(chess.role->ID);
        auto [name, color] = presenter().formatChessName(chess.role, tier, chess.star, {});
        items.push_back(name);
        colors.push_back(color);
        previewData.push_back(chess);
    }

    IndexedMenuData menuData{items, colors};
    IndexedMenuConfig menuConfig;
    menuConfig.perPage = 12;
    menuConfig.fontSize = 32;
    auto menu = makeIndexedMenu(std::format("選擇升星 {}★→{}★", fromStar, toStar), menuData, menuConfig, {}, previewData);
    menu->run();
    int sel = menu->getResult();
    if (sel >= 0)
    {
        auto& picked = chesses[sel];
        manager.upgradeChess(picked.id, toStar);
        showChessMessage(std::format("{}升星至{}★！", picked.role->Name, toStar));
        return true;
    }
    return false;
}

bool ChessSelector::rewardEquipment(int maxTier, int specificId)
{
    auto manager = makeChessManager(roster_, equipmentInventory_, economy_);
    if (specificId >= 0)
    {
        if (!manager.applyEquipmentReward(maxTier, specificId))
            return false;

        auto* eq = ChessEquipment::getById(specificId);
        auto* item = eq ? eq->getItem() : nullptr;
        showChessMessage(std::format("獲得裝備：{}", item ? item->Name : "???"));
        return true;
    }

    auto& allEquip = ChessEquipment::getAll();
    IndexedMenuData menuData;
    std::vector<const EquipmentDef*> ptrs;
    std::string tierName[] = {"初階", "中階", "高階", "傳說"};

    for (auto& eq : allEquip)
    {
        if (eq.tier > maxTier) continue;
        auto* item = eq.getItem();
        std::string name = item ? item->Name : "???";
        while (name.size() < 15) name += "\xe3\x80\x80";
        std::string label = std::format("[{}] {}", tierName[std::min(eq.tier - 1, 3)], name);
        menuData.labels.push_back(label);
        Color tierColors[] = {{100, 200, 100, 255}, {100, 150, 255, 255}, {255, 150, 50, 255}, {255, 100, 255, 255}};
        menuData.colors.push_back(tierColors[std::min(eq.tier - 1, 3)]);
        ptrs.push_back(&eq);
    }

    if (menuData.labels.empty()) return false;

    auto detail = std::make_shared<DrawableOnCall>([&ptrs](DrawableOnCall* self) {
        int idx = self->getItemIndex();
        if (idx >= 0 && idx < (int)ptrs.size()) drawEquipmentDetail(*ptrs[idx], 0, "");
    });

    IndexedMenuConfig menuConfig;
    menuConfig.exitable = false;
    auto menu = makeIndexedMenu("選擇裝備獎勵", menuData, menuConfig, {detail});
    menu->run();
    int sel = menu->getResult();
    if (sel >= 0)
    {
        equipmentInventory_.storeItem(ptrs[sel]->itemId);
        auto* item = ptrs[sel]->getItem();
        showChessMessage(std::format("獲得裝備：{}", item ? item->Name : "???"));
        return true;
    }
    return false;
}

bool ChessSelector::applyReward(const BalanceConfig::ChallengeReward& reward)
{
    using RT = BalanceConfig::ChallengeRewardType;

    if (reward.type == RT::Gold) return rewardGold(reward.value);
    if (reward.type == RT::GetPiece) return rewardPiece(reward.value);
    if (reward.type == RT::GetNeigong) return rewardNeigong(reward.value);
    if (reward.type == RT::StarUp1to2) return rewardStarUp(1, reward.value);
    if (reward.type == RT::StarUp2to3) return rewardStarUp(2, reward.value);
    if (reward.type == RT::GetSpecificEquipment) return rewardEquipment(99, reward.value);
    if (reward.type == RT::GetEquipment) return rewardEquipment(reward.value);
    return false;
}

void ChessSelector::showExpeditionChallenge()
{
    auto& cfg = ChessBalance::config();
    if (cfg.challenges.empty()) return;
    auto manager = makeChessManager(roster_, equipmentInventory_, economy_);
    auto& progress = progress_;
    auto& roleSave = roleSave_;

    for (;;)
    {
        // Build challenge list menu
        IndexedMenuData menuData;
        for (int i = 0; i < (int)cfg.challenges.size(); ++i)
        {
            auto& ch = cfg.challenges[i];
            bool done = progress.isChallengeCompleted(i);
            std::string label = std::format("{}{}", done ? "[已通關] " : "", ch.name);
            menuData.labels.push_back(label);
            menuData.colors.push_back(done ? Color{120, 120, 120, 255} : Color{255, 200, 100, 255});
        }

        // Detail panel showing enemies + rewards
        auto detailDraw = std::make_shared<DrawableOnCall>([&cfg, &progress, &roleSave](DrawableOnCall* self) {
            int idx = self->getItemIndex();
            if (idx < 0 || idx >= (int)cfg.challenges.size()) return;
            auto& ch = cfg.challenges[idx];
            auto* font = Font::getInstance();
            int px = 460, py = 60, fs = 22;
            Engine::getInstance()->fillRoundedRect({0, 0, 0, 180}, px, py, 500, 500, 8);
            Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, px, py, 500, 500, 8);

            int ty = py + 10;
            font->draw(ch.name, fs + 4, px + 10, ty, {255, 255, 100, 255}); ty += fs + 10;
            font->draw(ch.description, fs, px + 10, ty, {200, 200, 200, 255}); ty += fs + 8;
            if (progress.isChallengeCompleted(idx))
            {
                font->draw("[已通關]", fs, px + 10, ty, {0, 255, 0, 255}); ty += fs + 8;
            }

            // Show enemies
            font->draw("敵方陣容:", fs, px + 10, ty, {255, 150, 150, 255}); ty += fs + 4;
            for (auto& e : ch.enemies)
            {
                auto* role = roleSave.getRole(e.roleId);
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
                font->draw("  " + presenter().challengeRewardDesc(r), fs - 2, px + 10, ty, {200, 255, 200, 255});
                ty += fs;
            }
        });

        IndexedMenuConfig menuConfig;
        menuConfig.perPage = static_cast<int>(menuData.labels.size());
        auto menu = makeIndexedMenu("遠征挑戰", menuData, menuConfig, {detailDraw});
        menu->run();

        int sel = menu->getResult();
        if (sel < 0) return;
        int chIdx = sel;
        auto& ch = cfg.challenges[chIdx];

        // Check if player has pieces selected
        auto selectedChess = manager.getSelectedForBattle();
        if (selectedChess.empty())
        {
            showChessMessage("請先選擇出戰棋子");
            continue;
        }

        // Save enemy random counter so challenge doesn't corrupt regular battle state
        auto savedEnemyCallCount = random_.getEnemyCallCount();

        // Build enemy team from fixed role IDs
        std::vector<int> enemyIds;
        std::vector<int> enemyStars;
        for (auto& e : ch.enemies)
        {
            enemyIds.push_back(e.roleId);
            enemyStars.push_back(e.star);
        }

        // Build roles and run battle
        // This needs to be simplified and refactored
        DynamicBattleRoles roles;
        for (auto& c : selectedChess)
        {
            roles.teammate_ids.push_back(c.role->ID);
            roles.teammate_stars.push_back(c.star);
            roles.teammate_instances.push_back(c.id.value);
        }
        roles.enemy_ids = enemyIds;
        roles.enemy_stars = enemyStars;
        roles.enemy_weapons.reserve(ch.enemies.size());
        roles.enemy_armors.reserve(ch.enemies.size());
        for (auto& e : ch.enemies)
        {
            roles.enemy_weapons.push_back(e.weaponId);
            roles.enemy_armors.push_back(e.armorId);
        }

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
            random_.setEnemyCallCount(savedEnemyCallCount);
            random_.restore();
            showChessMessage("挑戰失敗！請調整陣容後再試");
            continue;
        }

        // Win — check if already completed
        if (progress_.isChallengeCompleted(chIdx))
        {
            showChessMessage("挑戰勝利！(已領取過獎勵)");
            continue;
        }

        int rSel = selectChallengeReward(ch.rewards);
        if (!applyReward(ch.rewards[rSel]))
            continue;

        progress_.completeChallenge(chIdx);
        UISave::autoSave();
    }
}

void ChessSelector::showPositionSwap()
{
    bool cur = progress_.isPositionSwapEnabled();
    IndexedMenuData menuData{{"  關閉  ", "  開啟  "}, {}};
    IndexedMenuConfig menuConfig;
    menuConfig.perPage = 2;
    auto sub = makeIndexedMenu("", menuData, menuConfig);
    sub->setShowNavigationButtons(false);
    auto panel = std::make_shared<DrawableOnCall>([cur](DrawableOnCall* self) {
        int px = 320, py = 200, fs = 32, lh = fs + 6;
        Engine::getInstance()->fillRoundedRect({0, 0, 0, 180}, px, py, 600, 250, 6);
        Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, px, py, 600, 250, 6);
        auto* font = Font::getInstance();
        int id = self->getItemIndex();
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
        progress_.setPositionSwapEnabled(sel == 1);
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
