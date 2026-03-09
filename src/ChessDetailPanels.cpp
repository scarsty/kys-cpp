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
void drawWrappedCursorText(
    PanelTextCursor& cursor,
    const std::string& text,
    int fontSize,
    Color color,
    int maxPixelWidth,
    int extraSpacing = 0,
    int indentPixels = 0)
{
    int availablePixels = std::max(0, maxPixelWidth - indentPixels);
    int availableUnits = std::max(4, availablePixels * 2 / std::max(1, fontSize));
    auto wrappedLines = wrapDisplayText(text, availableUnits);
    for (const auto& line : wrappedLines)
    {
        cursor.line(line, fontSize, color, extraSpacing, indentPixels);
    }
}

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

void drawNeigongDetail(const NeigongDef& ng, PanelFrame frame, int ownedState = -1)
{
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

void drawEquipmentDetail(const EquipmentDef& eq, PanelFrame frame, int count, const std::vector<std::string>& equippedBy)
{
    ChessScreenLayout::drawPanel(frame);
    auto* item = eq.getItem();
    if (item)
    {
        TextureManager::getInstance()->renderTexture("item", eq.itemId, frame.x + 10, frame.y + 10);
    }
    int fs = 28;
    PanelTextCursor header{Font::getInstance(), frame.x + 150, frame.y + 10};
    header.line(item ? item->Name : "???", fs + 4, {255, 255, 100, 255}, 6);
    std::string tierName[] = {"初階", "中階", "高階", "傳說"};
    Color tierColors[] = {{100, 200, 100, 255}, {100, 150, 255, 255}, {255, 150, 50, 255}, {255, 100, 255, 255}};
    header.line(std::format("層級: {}", tierName[std::min(eq.tier - 1, 3)]), fs, tierColors[std::min(eq.tier - 1, 3)]);
    header.line(std::format("擁有: x{}", count), fs, count > 0 ? Color{0, 255, 0, 255} : Color{180, 180, 180, 255});

    PanelTextCursor body{Font::getInstance(), frame.x + 10, frame.y + 100};
    if (!eq.effects.empty())
    {
        body.line("特殊效果:", fs, {255, 200, 100, 255});
        for (auto& eff : eq.effects)
        {
            body.line(comboEffectDesc(eff), fs - 2, {220, 220, 100, 255}, 2);
        }
    }
    if (!equippedBy.empty())
    {
        if (!eq.effects.empty())
        {
            body.skip(16);
        }
        body.line("裝備棋子:", fs, {140, 220, 255, 255}, 6);
        for (const auto& name : equippedBy)
        {
            body.line(name, fs - 2, {210, 235, 255, 255}, 2, 12);
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

    struct ComboBlock
    {
        std::string header;
        Color headerColor;
        std::vector<std::string> effectLines;
        Color effectColor;
    };

    auto buildBlocks = [&](int columnWidth) {
        std::vector<ComboBlock> blocks;
        int headerUnits = std::max(12, (columnWidth - 20) * 2 / fs);
        int effectUnits = std::max(12, (columnWidth - 28) * 2 / (fs - 2));
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

            bool active = lastActive != nullptr;
            int denominator = (nextThreshold ? nextThreshold : (lastActive ? lastActive : &combo.thresholds.back()))->count;
            std::string countText = formatComboCount(owned, effective, denominator, combo.starSynergyBonus, combo.isAntiCombo);

            ComboBlock block;
            block.header = std::format("{} ({})", combo.name, countText);
            block.headerColor = active ? Color{0, 255, 100, 255} : Color{200, 200, 200, 255};
            block.effectColor = active ? Color{180, 220, 255, 255} : Color{120, 120, 120, 255};

            auto headerLines = wrapDisplayText(block.header, headerUnits);
            if (!headerLines.empty())
            {
                block.header = headerLines.front();
            }

            if (shownThreshold)
            {
                for (auto& effect : shownThreshold->effects)
                {
                    auto wrapped = wrapDisplayText(comboEffectCompactDesc(effect), effectUnits);
                    if (wrapped.empty())
                    {
                        continue;
                    }
                    for (size_t lineIndex = 0; lineIndex < wrapped.size(); ++lineIndex)
                    {
                        block.effectLines.push_back(lineIndex == 0 ? "  " + wrapped[lineIndex] : "    " + wrapped[lineIndex]);
                    }
                }
            }
            blocks.push_back(std::move(block));
        }
        return blocks;
    };

    auto blockHeight = [&](const ComboBlock& block) {
        return (fs + 4) + static_cast<int>(block.effectLines.size()) * fs + 4;
    };

    auto canFit = [&](const std::vector<ComboBlock>& blocks, int columns) {
        int availableHeight = frame.h - 47;
        std::vector<int> heights(columns, 0);
        int currentColumn = 0;
        for (const auto& block : blocks)
        {
            int needed = blockHeight(block);
            while (currentColumn < columns && heights[currentColumn] + needed > availableHeight)
            {
                ++currentColumn;
            }
            if (currentColumn >= columns)
            {
                return false;
            }
            heights[currentColumn] += needed;
        }
        return true;
    };

    int chosenColumns = 1;
    std::vector<ComboBlock> blocks;
    int maxColumns = std::min(3, std::max(1, static_cast<int>(roleCombos.size())));
    for (int columns = 1; columns <= maxColumns; ++columns)
    {
        int columnWidth = std::max(160, (frame.w - 20) / columns);
        auto candidateBlocks = buildBlocks(columnWidth);
        if (canFit(candidateBlocks, columns))
        {
            chosenColumns = columns;
            blocks = std::move(candidateBlocks);
            break;
        }
        if (columns == maxColumns)
        {
            chosenColumns = columns;
            blocks = std::move(candidateBlocks);
        }
    }

    int columnWidth = std::max(160, (frame.w - 20) / chosenColumns);
    PanelColumnFlow flow{font, frame, frame.x + 10, frame.y + 32, frame.y + 32, frame.y + frame.h - 15, columnWidth};
    for (const auto& block : blocks)
    {
        int neededHeight = blockHeight(block);
        if (!flow.ensureSpace(neededHeight))
        {
            break;
        }

        flow.line(block.header, fs, block.headerColor);
        for (const auto& line : block.effectLines)
        {
            if (!flow.line(line, fs - 2, block.effectColor, 2))
            {
                break;
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

    int columnWidth = std::max(150, (frame.w - 30) / 3);
    PanelColumnFlow flow{font, frame, frame.x + 10, frame.y + fs + 34, frame.y + fs + 34, frame.y + frame.h, columnWidth};
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
    : ComboCatalogDetailPanel(combos, roleSave, std::move(starByRole), ChessScreenLayout::comboCatalogDetailPanel())
{
}

ComboCatalogDetailPanel::ComboCatalogDetailPanel(const std::vector<ComboDef>& combos, ChessRoleSave& roleSave, std::map<int, int> starByRole, PanelFrame frame)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , combos_(combos)
    , roleSave_(roleSave)
    , starByRole_(std::move(starByRole))
    , frame_(frame)
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
    auto frame = frame_;
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
        thresholdCursor.line(std::move(synergyLine), fs - 4, {255, 200, 50, 255}, 2);
        thresholdCursor.line("  每額外★計入1羈絆人數", fs - 6, {180, 160, 80, 255}, 0);
    }
    thresholdCursor.skip(fs + 4);
    int effectFontSize = fs - 2;
    int effectIndent = effectFontSize;
    int effectPixelWidth = frame.x + frame.w - thresholdCursor.x - 10;
    for (auto& threshold : combo.thresholds)
    {
        bool tierActive = effective >= threshold.count;
        Color tierColor = tierActive ? Color{0, 255, 0, 255} : Color{255, 200, 100, 255};
        Color effectColor = tierActive ? Color{180, 220, 255, 255} : Color{200, 200, 200, 255};
        std::string tierMark = tierActive ? " ✓" : "";
        thresholdCursor.line(std::format("{}人: {}{}", threshold.count, threshold.name, tierMark), fs, tierColor, 1);
        for (auto& effect : threshold.effects)
        {
            drawWrappedCursorText(
                thresholdCursor,
                comboEffectDesc(effect),
                effectFontSize,
                effectColor,
                effectPixelWidth,
                0,
                effectIndent);
        }
        thresholdCursor.skip(8);
    }
}

NeigongDetailPanel::NeigongDetailPanel(std::vector<const NeigongDef*> neigongs)
    : NeigongDetailPanel(std::move(neigongs), ChessScreenLayout::neigongDetailPanel())
{
}

NeigongDetailPanel::NeigongDetailPanel(std::vector<const NeigongDef*> neigongs, PanelFrame frame)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , neigongs_(std::move(neigongs))
    , frame_(frame)
{
}

NeigongDetailPanel::NeigongDetailPanel(std::vector<const NeigongDef*> neigongs, std::set<int> ownedMagicIds)
    : NeigongDetailPanel(std::move(neigongs), std::move(ownedMagicIds), ChessScreenLayout::neigongDetailPanel())
{
}

NeigongDetailPanel::NeigongDetailPanel(std::vector<const NeigongDef*> neigongs, std::set<int> ownedMagicIds, PanelFrame frame)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , neigongs_(std::move(neigongs))
    , ownedMagicIds_(std::move(ownedMagicIds))
    , showOwnedState_(true)
    , frame_(frame)
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
    drawNeigongDetail(*ng, frame_, ownedState);
}

EquipmentDetailPanel::EquipmentDetailPanel(std::vector<const EquipmentDef*> equipments, EquipmentDetailProvider detailProvider)
    : EquipmentDetailPanel(std::move(equipments), std::move(detailProvider), ChessScreenLayout::equipmentDetailPanel())
{
}

EquipmentDetailPanel::EquipmentDetailPanel(std::vector<const EquipmentDef*> equipments, EquipmentDetailProvider detailProvider, PanelFrame frame)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , equipments_(std::move(equipments))
    , detailProvider_(std::move(detailProvider))
    , frame_(frame)
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
    drawEquipmentDetail(*equipment, frame_, detail.count, detail.equippedBy);
}

ChallengeDetailPanel::ChallengeDetailPanel(const std::vector<BalanceConfig::ChallengeDef>& challenges, ChessProgress& progress, ChessRoleSave& roleSave)
    : ChallengeDetailPanel(challenges, progress, roleSave, ChessScreenLayout::challengeDetailPanel())
{
}

ChallengeDetailPanel::ChallengeDetailPanel(const std::vector<BalanceConfig::ChallengeDef>& challenges, ChessProgress& progress, ChessRoleSave& roleSave, PanelFrame frame)
    : DrawableOnCall([this](DrawableOnCall*) { drawPanel(); })
    , challenges_(challenges)
    , progress_(progress)
    , roleSave_(roleSave)
    , frame_(frame)
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
    auto frame = frame_;
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