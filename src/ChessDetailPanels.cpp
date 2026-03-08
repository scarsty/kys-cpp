#include "ChessDetailPanels.h"

#include "ChessPool.h"
#include "ChessScreenLayout.h"
#include "ChessUiCommon.h"
#include "Font.h"
#include "TextureManager.h"

#include <algorithm>
#include <format>

namespace KysChess
{

namespace
{

struct PanelTextCursor
{
    Font* font;
    int x;
    int y;

    void line(const std::string& text, int fontSize, Color color, int extraSpacing = 4, int indent = 0)
    {
        font->draw(text, fontSize, x + indent, y, color);
        y += fontSize + extraSpacing;
    }

    void skip(int spacing)
    {
        y += spacing;
    }
};

struct PanelColumnFlow
{
    Font* font;
    PanelFrame frame;
    int x;
    int y;
    int top;
    int bottom;
    int columnWidth;

    bool ensureSpace(int requiredHeight)
    {
        if (y + requiredHeight <= bottom)
        {
            return true;
        }
        x += columnWidth;
        y = top;
        return x + columnWidth <= frame.x + frame.w;
    }

    bool line(const std::string& text, int fontSize, Color color, int extraSpacing = 4, int indent = 0)
    {
        int requiredHeight = fontSize + extraSpacing;
        if (!ensureSpace(requiredHeight))
        {
            return false;
        }
        font->draw(text, fontSize, x + indent, y, color);
        y += requiredHeight;
        return true;
    }

    bool skip(int spacing)
    {
        if (!ensureSpace(spacing))
        {
            return false;
        }
        y += spacing;
        return true;
    }
};

void drawNeigongDetail(const NeigongDef& ng, int ownedState = -1)
{
    auto frame = ChessScreenLayout::neigongDetailPanel();
    ChessScreenLayout::drawPanel(frame);
    TextureManager::getInstance()->renderTexture("item", ng.itemId, frame.x + 10, frame.y + 10);
    int fs = 24;
    PanelTextCursor header{Font::getInstance(), frame.x + 100, frame.y + 10};
    header.line(ng.name, fs + 4, {255, 255, 100, 255}, 6);
    std::string tierName[] = {"初階", "中階", "高階", "傳說"};
    header.line(std::format("層級: {}", tierName[std::min(ng.tier - 1, 3)]), fs, {200, 200, 200, 255});
    if (ownedState >= 0)
    {
        bool owned = ownedState > 0;
        header.line(owned ? "已獲得" : "未獲得", fs, owned ? Color{0, 255, 0, 255} : Color{180, 180, 180, 255}, 8);
    }
    PanelTextCursor body{Font::getInstance(), frame.x + 10, frame.y + 100};
    body.line("效果:", fs, {200, 200, 200, 255});
    for (auto& eff : ng.effects)
    {
        body.line("  " + comboEffectDesc(eff), fs, {220, 220, 220, 255}, 2);
    }
}

void drawEquipmentDetail(const EquipmentDef& eq, int count, const std::string& equippedBy)
{
    auto frame = ChessScreenLayout::equipmentDetailPanel();
    ChessScreenLayout::drawPanel(frame);
    auto* item = eq.getItem();
    if (item)
    {
        TextureManager::getInstance()->renderTexture("item", eq.itemId, frame.x + 10, frame.y + 10);
    }
    int fs = 28;
    PanelTextCursor header{Font::getInstance(), frame.x + 100, frame.y + 10};
    header.line(item ? item->Name : "???", fs + 4, {255, 255, 100, 255}, 6);
    std::string tierName[] = {"初階", "中階", "高階", "傳說"};
    Color tierColors[] = {{100, 200, 100, 255}, {100, 150, 255, 255}, {255, 150, 50, 255}, {255, 100, 255, 255}};
    header.line(std::format("層級: {}", tierName[std::min(eq.tier - 1, 3)]), fs, tierColors[std::min(eq.tier - 1, 3)]);
    header.line(std::format("擁有: x{}", count), fs, count > 0 ? Color{0, 255, 0, 255} : Color{180, 180, 180, 255});
    if (!equippedBy.empty())
    {
        header.line(std::format("裝備於: {}", equippedBy), fs, {100, 200, 255, 255});
    }

    PanelTextCursor body{Font::getInstance(), frame.x + 10, frame.y + 120};
    if (!eq.effects.empty())
    {
        body.line("特殊效果:", fs, {255, 200, 100, 255});
        for (auto& eff : eq.effects)
        {
            body.line(comboEffectDesc(eff), fs - 2, {220, 220, 100, 255}, 2);
        }
    }
}

}    // namespace

ComboInfoPanel::ComboInfoPanel(ChessManager manager)
    : ChessDrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , manager_(std::move(manager))
{
}

void ComboInfoPanel::drawPanel()
{
    auto role = getPreviewData().role;
    if (!role)
    {
        return;
    }

    auto roleCombos = ChessCombo::getCombosForRole(role->ID);
    if (roleCombos.empty())
    {
        return;
    }

    auto& allCombos = ChessCombo::getAllCombos();
    auto starByRole = ChessCombo::buildStarMap(manager_.getSelectedForBattle());
    auto frame = ChessScreenLayout::comboInfoPanel();
    ChessScreenLayout::drawPanel(frame, {0, 0, 0, 160});

    int fs = 20;
    auto* font = Font::getInstance();
    font->draw("羈絆資訊", fs + 4, frame.x + 10, frame.y + 5, {255, 255, 100, 255});
    PanelColumnFlow flow{font, frame, frame.x + 10, frame.y + 32, frame.y + 32, frame.y + frame.h - 15, 225};

    for (auto cid : roleCombos)
    {
        auto& combo = allCombos[static_cast<int>(cid)];
        auto [owned, effective] = computeOwnership(combo, starByRole);

        const ComboThreshold* lastActive = nullptr;
        const ComboThreshold* nextThreshold = nullptr;
        for (auto& threshold : combo.thresholds)
        {
            if (effective >= threshold.count)
            {
                lastActive = &threshold;
            }
            else if (!nextThreshold)
            {
                nextThreshold = &threshold;
            }
        }

        const ComboThreshold* shownThreshold = lastActive ? lastActive : nextThreshold;
        if (!shownThreshold && !combo.thresholds.empty())
        {
            shownThreshold = &combo.thresholds.back();
        }

        int neededHeight = (fs + 4) + (shownThreshold ? static_cast<int>(shownThreshold->effects.size()) * fs : 0) + 4;
        if (!flow.ensureSpace(neededHeight))
        {
            break;
        }

        bool active = lastActive != nullptr;
        int denominator = (nextThreshold ? nextThreshold : (lastActive ? lastActive : &combo.thresholds.back()))->count;
        Color headerColor = active ? Color{0, 255, 100, 255} : Color{200, 200, 200, 255};
        std::string countText = formatComboCount(owned, effective, denominator, combo.starSynergyBonus, combo.isAntiCombo);
        flow.line(std::format("{} ({})", combo.name, countText), fs, headerColor);

        if (shownThreshold)
        {
            Color effectColor = active ? Color{180, 220, 255, 255} : Color{120, 120, 120, 255};
            for (auto& effect : shownThreshold->effects)
            {
                if (!flow.line("  " + comboEffectDesc(effect), fs - 2, effectColor, 2))
                {
                    break;
                }
            }
        }
        if (!flow.skip(4))
        {
            break;
        }
    }
}

OwnedRosterPanel::OwnedRosterPanel(ChessRoster& roster, ChessManager manager)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , roster_(roster)
    , manager_(std::move(manager))
{
}

void OwnedRosterPanel::drawPanel()
{
    auto chessMap = roster_.getChessCountMap();
    if (chessMap.empty())
    {
        return;
    }

    auto* font = Font::getInstance();
    auto frame = ChessScreenLayout::shopOwnedPanel();
    ChessScreenLayout::drawPanel(frame, {0, 0, 0, 160});
    int fs = 20;
    font->draw("我方棋子", fs + 4, frame.x + 10, frame.y + 5, {255, 255, 100, 255});

    std::map<std::pair<int, int>, int> deployedCount;
    for (auto& chess : manager_.getSelectedForBattle())
    {
        deployedCount[{chess.role->ID, chess.star}]++;
    }

    struct Entry
    {
        std::string label;
        Color color;
    };

    std::vector<Entry> deployed;
    std::vector<Entry> bench;
    for (auto& [roleAndStar, count] : chessMap)
    {
        auto key = std::make_pair(roleAndStar.role->ID, roleAndStar.star);
        int deployedForRole = deployedCount[key];
        Color color = ChessPool::GetTierColor(ChessPool::GetChessTier(roleAndStar.role->ID));
        std::string stars;
        for (int i = 0; i < roleAndStar.star; ++i)
        {
            stars += "★";
        }
        std::string baseName = std::format("{}{}", roleAndStar.role->Name, stars);

        for (int i = 0; i < count; ++i)
        {
            if (deployedForRole > 0)
            {
                deployed.push_back({"▶" + baseName, color});
                deployedForRole--;
            }
            else
            {
                bench.push_back({" " + baseName, {(Uint8)(color.r / 2 + 80), (Uint8)(color.g / 2 + 80), (Uint8)(color.b / 2 + 80), 200}});
            }
        }
    }

    PanelColumnFlow flow{font, frame, frame.x + 10, frame.y + fs + 34, frame.y + fs + 34, frame.y + frame.h, 200};
    auto drawEntry = [&](const Entry& entry) {
        flow.line(entry.label, fs, entry.color);
    };

    for (auto& entry : deployed)
    {
        drawEntry(entry);
    }
    for (auto& entry : bench)
    {
        drawEntry(entry);
    }
}

ComboCatalogDetailPanel::ComboCatalogDetailPanel(const std::vector<ComboDef>& combos, ChessRoleSave& roleSave, std::map<int, int> starByRole)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , combos_(combos)
    , roleSave_(roleSave)
    , starByRole_(std::move(starByRole))
{
}

void ComboCatalogDetailPanel::drawPanel()
{
    int idx = getItemIndex();
    if (idx < 0 || idx >= static_cast<int>(combos_.size()))
    {
        return;
    }

    auto& combo = combos_[idx];
    auto frame = ChessScreenLayout::comboCatalogDetailPanel();
    ChessScreenLayout::drawPanel(frame);
    int fs = 24;
    auto* font = Font::getInstance();
    font->draw(combo.name, fs + 4, frame.x + 10, frame.y + 10, {255, 255, 100, 255});

    auto [ownedCount, effective] = computeOwnership(combo, starByRole_);
    (void)ownedCount;

    PanelTextCursor memberCursor{font, frame.x + 10, frame.y + 45};
    memberCursor.line("成員:", fs, {200, 200, 200, 255});
    int totalBonus = 0;
    for (int roleId : combo.memberRoleIds)
    {
        auto* role = roleSave_.getRole(roleId);
        if (!role)
        {
            continue;
        }
        bool owned = starByRole_.count(roleId) > 0;
        Color color = owned ? Color{0, 255, 0, 255} : Color{120, 120, 120, 255};
        std::string starSuffix;
        if (owned && combo.starSynergyBonus)
        {
            int star = starByRole_.count(roleId) ? starByRole_.at(roleId) : 1;
            starSuffix = std::format(" ★{}", star);
            totalBonus += (star - 1);
        }
        memberCursor.line(std::format("  {} ({}費){}{}", role->Name, ChessPool::GetChessTier(roleId), owned ? " ✓" : "", starSuffix), fs, color, 1);
    }

    PanelTextCursor thresholdCursor{font, frame.x + 290, frame.y + 45};
    thresholdCursor.line(combo.isAntiCombo ? "條件:" : "閾值:", fs, {200, 200, 200, 255}, 0);
    if (combo.starSynergyBonus)
    {
        std::string synergyLine = (totalBonus > 0)
            ? std::format("★ 成員星級計人數，當前+{}人", totalBonus)
            : "★ 成員星級計人數（2★=2人）";
        thresholdCursor.skip(fs + 2);
        thresholdCursor.line(synergyLine, fs - 4, {255, 200, 50, 255}, 2);
        thresholdCursor.line("  每額外★計入1羈絆人數", fs - 6, {180, 160, 80, 255}, 0);
    }
    thresholdCursor.skip(fs + 4);
    for (auto& threshold : combo.thresholds)
    {
        bool tierActive = effective >= threshold.count;
        Color tierColor = tierActive ? Color{0, 255, 0, 255} : Color{255, 200, 100, 255};
        Color effectColor = tierActive ? Color{180, 220, 255, 255} : Color{200, 200, 200, 255};
        std::string tierMark = tierActive ? " ✓" : "";
        thresholdCursor.line(std::format("{}人: {}{}", threshold.count, threshold.name, tierMark), fs, tierColor, 1);
        for (auto& effect : threshold.effects)
        {
            thresholdCursor.line("  " + comboEffectDesc(effect), fs - 2, effectColor, 0);
        }
        thresholdCursor.skip(8);
    }
}

NeigongDetailPanel::NeigongDetailPanel(std::vector<const NeigongDef*> neigongs)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , neigongs_(std::move(neigongs))
{
}

NeigongDetailPanel::NeigongDetailPanel(std::vector<const NeigongDef*> neigongs, std::set<int> ownedMagicIds)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , neigongs_(std::move(neigongs))
    , ownedMagicIds_(std::move(ownedMagicIds))
    , showOwnedState_(true)
{
}

void NeigongDetailPanel::drawPanel()
{
    int idx = getItemIndex();
    if (idx < 0 || idx >= static_cast<int>(neigongs_.size()))
    {
        return;
    }
    auto* ng = neigongs_[idx];
    if (!ng)
    {
        return;
    }
    int ownedState = -1;
    if (showOwnedState_)
    {
        ownedState = ownedMagicIds_.contains(ng->magicId) ? 1 : 0;
    }
    drawNeigongDetail(*ng, ownedState);
}

EquipmentDetailPanel::EquipmentDetailPanel(std::vector<const EquipmentDef*> equipments, EquipmentDetailProvider detailProvider)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , equipments_(std::move(equipments))
    , detailProvider_(std::move(detailProvider))
{
}

void EquipmentDetailPanel::drawPanel()
{
    int idx = getItemIndex();
    if (idx < 0 || idx >= static_cast<int>(equipments_.size()))
    {
        return;
    }
    auto* equipment = equipments_[idx];
    if (!equipment)
    {
        return;
    }
    auto detail = detailProvider_ ? detailProvider_(*equipment) : EquipmentDetailState{};
    drawEquipmentDetail(*equipment, detail.count, detail.equippedBy);
}

ChallengeDetailPanel::ChallengeDetailPanel(const std::vector<BalanceConfig::ChallengeDef>& challenges, ChessProgress& progress, ChessRoleSave& roleSave)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , challenges_(challenges)
    , progress_(progress)
    , roleSave_(roleSave)
{
}

void ChallengeDetailPanel::drawPanel()
{
    int idx = getItemIndex();
    if (idx < 0 || idx >= static_cast<int>(challenges_.size()))
    {
        return;
    }

    auto& challenge = challenges_[idx];
    auto frame = ChessScreenLayout::challengeDetailPanel();
    ChessScreenLayout::drawPanel(frame);
    int fs = 22;
    PanelTextCursor cursor{Font::getInstance(), frame.x + 10, frame.y + 10};
    cursor.line(challenge.name, fs + 4, {255, 255, 100, 255}, 6);
    cursor.line(challenge.description, fs, {200, 200, 200, 255}, 8);
    if (progress_.isChallengeCompleted(idx))
    {
        cursor.line("[已通關]", fs, {0, 255, 0, 255}, 8);
    }

    cursor.line("敵方陣容:", fs, {255, 150, 150, 255});
    for (auto& enemy : challenge.enemies)
    {
        auto* role = roleSave_.getRole(enemy.roleId);
        if (!role)
        {
            continue;
        }
        int tier = ChessPool::GetChessTier(enemy.roleId);
        std::string stars;
        for (int s = 0; s < enemy.star; ++s)
        {
            stars += "★";
        }
        cursor.line(std::format("  {} {} ({}費)", role->Name, stars, tier), fs - 2, {220, 180, 180, 255}, 2);
    }
    cursor.skip(8);

    cursor.line("通關獎勵(擇一):", fs, {100, 255, 100, 255});
    for (auto& reward : challenge.rewards)
    {
        cursor.line("  " + chessPresenter().challengeRewardDesc(reward), fs - 2, {200, 255, 200, 255}, 2);
    }
}

BuyExpPreviewPanel::BuyExpPreviewPanel(
    std::string header,
    std::string costLine,
    std::string pieceLine,
    std::string pieceNext,
    std::string currentWeights,
    std::string nextWeights)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , header_(std::move(header))
    , costLine_(std::move(costLine))
    , pieceLine_(std::move(pieceLine))
    , pieceNext_(std::move(pieceNext))
    , currentWeights_(std::move(currentWeights))
    , nextWeights_(std::move(nextWeights))
{
}

void BuyExpPreviewPanel::drawPanel()
{
    auto frame = ChessScreenLayout::buyExpPreviewPanel();
    ChessScreenLayout::drawPanel(frame);
    int fs = 20;
    PanelTextCursor cursor{Font::getInstance(), frame.x + 10, frame.y + 10};

    cursor.line(header_, fs, {255, 215, 0, 255}, 6);
    cursor.line(costLine_, fs, {255, 255, 255, 255}, 12);
    int pieceLineY = cursor.y;
    cursor.line(pieceLine_, fs, {130, 220, 255, 255}, 12);
    if (!pieceNext_.empty())
    {
        cursor.font->draw(pieceNext_, fs, cursor.x + fs * 8, pieceLineY, {100, 255, 100, 255});
    }
    cursor.line("當前商店權重:", fs, {160, 160, 160, 255}, 6);
    cursor.line(currentWeights_, fs, {255, 255, 255, 255}, 10);
    cursor.line("下一級商店權重:", fs, {160, 160, 160, 255}, 6);
    cursor.line(nextWeights_, fs, {100, 255, 100, 255}, 6);
}

PositionSwapInfoPanel::PositionSwapInfoPanel(bool currentEnabled)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , currentEnabled_(currentEnabled)
{
}

void PositionSwapInfoPanel::drawPanel()
{
    auto frame = ChessScreenLayout::positionSwapPanel();
    ChessScreenLayout::drawPanel(frame, {0, 0, 0, 180}, {180, 170, 140, 200}, 6);
    int id = getItemIndex();
    bool willEnable = id >= 0 ? (id == 1) : currentEnabled_;
    int fs = 32;
    PanelTextCursor cursor{Font::getInstance(), frame.x + 10, frame.y + 10};
    cursor.line(willEnable ? "狀態：開啟" : "狀態：關閉", fs, willEnable ? Color{0, 255, 0, 255} : Color{255, 80, 80, 255}, 6);
    cursor.line("戰鬥開始前可交換我方棋子位置", fs, {255, 215, 0, 255}, 6);
    cursor.line("點擊兩個我方棋子即可互換位置", fs, {255, 255, 255, 255}, 6);
    cursor.line("右鍵確認完成佈陣", fs, {255, 255, 255, 255}, 6);
}

GuidePanel::GuidePanel()
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
{
}

void GuidePanel::drawPanel()
{
    auto frame = ChessScreenLayout::guidePanel();
    ChessScreenLayout::drawPanel(frame);
    int fs = 20;
    PanelTextCursor cursor{Font::getInstance(), frame.x + 30, frame.y + 15};
    auto title = [&](const std::string& text) {
        cursor.line(text, fs + 2, {255, 255, 100, 255}, 8);
    };
    auto line = [&](const std::string& text, Color color = {220, 220, 220, 255}) {
        cursor.line(text, fs, color, 6);
    };

    title("珍瓏棋局 · 遊戲說明");
    cursor.skip(4);
    line("這是一場自走棋對弈。招募武林中人，排兵佈陣，棋子上場後將自動戰鬥。");
    cursor.skip(6);

    title("基本流程");
    line("· 每回合在商店中花費金幣招募棋子並部署上陣");
    line("· 戰鬥自動進行，獲勝後進入下一回合");
    line("· 每四回合遭遇一位強敵(Boss)，擊敗可獲內功");
    line("· 共二十八回合，存活到最後即為通關");
    cursor.skip(6);

    title("棋子與升星");
    line("· 棋子分為1至5費，費用越高越稀有");
    line("· 集齊三個相同棋子自動合成二星，三個二星合成三星，升星後屬性大幅提升");
    cursor.skip(6);

    title("經濟系統");
    line("· 每回合獲得基礎金幣，存款產生利息(每10金幣額外+1，上限3)");
    line("· 金幣用於：招募棋子、刷新商店($2)、購買經驗($5)");
    line("· 提升等級可增加上陣人數與高費棋子出現概率");
    cursor.skip(6);

    title("羈絆");
    line("· 同門派棋子上陣達到一定數量可激活羈絆效果");
    line("· 羈絆提供攻擊、防禦、生命等多種加成，合理搭配羈絆是制勝關鍵");
    cursor.skip(12);

    cursor.line("點擊任意處返回", fs - 2, {150, 150, 150, 255}, 0);
}

}    // namespace KysChess