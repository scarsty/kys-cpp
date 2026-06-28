#include "ChessContextMenu.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace KysChess;

namespace
{

std::vector<std::string> labelsOf(const std::vector<ChessContextMenuItem>& items)
{
    std::vector<std::string> labels;
    labels.reserve(items.size());
    for (const auto& item : items)
    {
        labels.push_back(item.label);
    }
    return labels;
}

}    // namespace

TEST_CASE("chess context menu keeps frequent actions at the first level", "[chess][context-menu]")
{
    const auto items = buildChessContextMenu();

    CHECK(labelsOf(items) == std::vector<std::string>{
                                "購買棋子",
                                "出售棋子",
                                "選擇出戰",
                                "進入戰鬥",
                                "購買經驗",
                                "裝備管理",
                                "棋局策略",
                                "情報一覽",
                                "遠征挑戰",
                                "系統設定",
                                "遊戲說明",
                            });
    REQUIRE(items.size() == 11);
    CHECK(items[0].action == ChessContextMenuAction::BuyChess);
    CHECK(items[4].action == ChessContextMenuAction::BuyExp);
    CHECK(items[6].action == ChessContextMenuAction::OpenStrategyMenu);
    CHECK(items[7].action == ChessContextMenuAction::OpenInfoMenu);
    CHECK(items[9].action == ChessContextMenuAction::ShowSystemSettings);
    CHECK(items[10].action == ChessContextMenuAction::ShowGameGuide);
}

TEST_CASE("strategy menu groups run-shaping actions", "[chess][context-menu]")
{
    CHECK(labelsOf(buildChessStrategyMenu(false)) == std::vector<std::string>{
                                                     "排兵佈陣",
                                                     "逆天改命",
                                                 });

    const auto items = buildChessStrategyMenu(true);
    CHECK(labelsOf(items) == std::vector<std::string>{
                               "排兵佈陣",
                               "逆天改命",
                               "禁棋管理",
                           });
    REQUIRE(items.size() == 3);
    CHECK(items[0].action == ChessContextMenuAction::ShowPositionSwap);
    CHECK(items[1].action == ChessContextMenuAction::RerollBattleSeed);
    CHECK(items[2].action == ChessContextMenuAction::ManageBans);
}

TEST_CASE("info menu groups reference catalog screens", "[chess][context-menu]")
{
    const auto items = buildChessInfoMenu();

    CHECK(labelsOf(items) == std::vector<std::string>{
                               "查看羈絆",
                               "棋子一覽",
                               "查看內功",
                           });
    REQUIRE(items.size() == 3);
    CHECK(items[0].action == ChessContextMenuAction::ViewCombos);
    CHECK(items[1].action == ChessContextMenuAction::ViewChessPool);
    CHECK(items[2].action == ChessContextMenuAction::ViewNeigong);
}

TEST_CASE("equipment menu puts legendary shop second once unlocked", "[chess][context-menu]")
{
    CHECK(labelsOf(buildChessEquipmentMenu(false)) == std::vector<std::string>{
                                                      "裝備一覽",
                                                  });

    const auto items = buildChessEquipmentMenu(true);
    CHECK(labelsOf(items) == std::vector<std::string>{
                               "裝備一覽",
                               "神兵商店",
                           });
    REQUIRE(items.size() == 2);
    CHECK(items[0].action == ChessContextMenuAction::ShowEquipmentInventory);
    CHECK(items[1].action == ChessContextMenuAction::BuyLegendaryEquipment);
}

TEST_CASE("context menu y anchor centers visible rows inside chess content", "[chess][context-menu]")
{
    CHECK(centerChessContextMenuY(11) == 112);
    CHECK(centerChessContextMenuY(3) == 292);
    CHECK(centerChessContextMenuY(20) == 45);
}
