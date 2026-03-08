#include "ChessInfoFlow.h"

#include "ChessDetailPanels.h"
#include "ChessMenuHelpers.h"
#include "ChessNeigong.h"
#include "ChessPool.h"
#include "ChessScreenLayout.h"
#include "TextBox.h"

#include <format>
#include <set>

namespace KysChess
{

ChessInfoFlow::ChessInfoFlow(const ChessSelectorServices& services)
    : services_(services)
{
}

void ChessInfoFlow::viewCombos()
{
    auto& combos = ChessCombo::getAllCombos();
    auto manager = makeChessManager(services_);
    auto starByRole = ChessCombo::buildStarMap(manager.getSelectedForBattle());

    IndexedMenuData menuData;
    for (int i = 0; i < static_cast<int>(combos.size()); ++i)
    {
        auto& combo = combos[i];
        auto [owned, effective] = computeOwnership(combo, starByRole);
        std::string padded = combo.name;
        while (padded.size() < 14) padded += "　";
        int extra = effective - owned;
        int denominator = combo.isAntiCombo ? (combo.thresholds.empty() ? 1 : combo.thresholds[0].count) : static_cast<int>(combo.memberRoleIds.size());
        std::string countPrefix = combo.isAntiCombo ? "独" : (combo.starSynergyBonus ? "★" : "　");
        std::string bonusPart = (combo.starSynergyBonus && extra > 0) ? std::format("+{:<2}", extra) : "   ";
        std::string countFmt = std::format("{}{:2}{}/{:2}", countPrefix, owned, bonusPart, denominator);
        menuData.labels.push_back(std::format("{}({})", padded, countFmt));
        menuData.colors.push_back(owned > 0 ? Color{0, 255, 0, 255} : Color{180, 180, 180, 255});
    }

    auto detailPanel = std::make_shared<ComboCatalogDetailPanel>(combos, services_.roleSave, starByRole);
    IndexedMenuConfig menuConfig;
    auto menuAnchor = ChessScreenLayout::browseMenuAnchor();
    menuConfig.x = menuAnchor.x;
    menuConfig.y = menuAnchor.y;
    auto menu = makeIndexedMenu("羈絆一覽", menuData, menuConfig, {detailPanel});
    menu->run();
}

void ChessInfoFlow::viewNeigong()
{
    auto& pool = ChessNeigong::getPool();
    if (pool.empty())
    {
        return;
    }

    auto& obtained = services_.progress.getObtainedNeigong();
    std::set<int> obtainedSet(obtained.begin(), obtained.end());
    IndexedMenuData menuData;
    std::vector<const NeigongDef*> detailNeigongs;
    std::string tierName[] = {"初階", "中階", "高階", "傳說"};
    for (int i = 0; i < static_cast<int>(pool.size()); ++i)
    {
        auto& ng = pool[i];
        bool owned = obtainedSet.count(ng.magicId) > 0;
        std::string padded = ng.name;
        while (padded.size() < 15) padded += "\xe3\x80\x80";
        menuData.labels.push_back(std::format("[{}] {}{}", tierName[std::min(ng.tier - 1, 3)], padded, owned ? " ✓" : "　 "));
        menuData.colors.push_back(owned ? Color{0, 255, 0, 255} : Color{120, 120, 120, 255});
        detailNeigongs.push_back(&ng);
    }

    auto detailPanel = std::make_shared<NeigongDetailPanel>(detailNeigongs, obtainedSet);
    IndexedMenuConfig menuConfig;
    auto menuAnchor = ChessScreenLayout::browseMenuAnchor();
    menuConfig.x = menuAnchor.x;
    menuConfig.y = menuAnchor.y;
    auto menu = makeIndexedMenu("内功一覽", menuData, menuConfig, {detailPanel});
    menu->run();
}

void ChessInfoFlow::showGameGuide()
{
    auto box = std::make_shared<TextBox>();
    box->setText("　");
    box->setHaveBox(false);
    box->setFontSize(1);
    box->addChild(std::make_shared<GuidePanel>());
    box->runAtPosition(0, 0);
}

void ChessInfoFlow::showPositionSwap()
{
    bool current = services_.progress.isPositionSwapEnabled();
    IndexedMenuData menuData{{"  關閉  ", "  開啟  "}, {}};
    IndexedMenuConfig menuConfig;
    menuConfig.perPage = 2;
    auto menu = makeIndexedMenu("", menuData, menuConfig, {std::make_shared<PositionSwapInfoPanel>(current)});
    menu->setShowNavigationButtons(false);
    auto menuAnchor = ChessScreenLayout::positionSwapMenuAnchor();
    menu->runAtPosition(menuAnchor.x, menuAnchor.y);
    int sel = menu->getResult();
    if (sel == 0 || sel == 1)
    {
        services_.progress.setPositionSwapEnabled(sel == 1);
    }
}

}    // namespace KysChess