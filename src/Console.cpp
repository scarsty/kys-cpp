#include "Console.h"
#include "BattleNetwork.h"
#include "BattleScene.h"
#include "ChessApplicationSessionHost.h"
#include "ChessBalance.h"
#include "ChessGuiSessionAdapter.h"
#include "ChessStandaloneBattle.h"
#include "DrawableOnCall.h"
#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "InputBox.h"
#include "MainScene.h"
#include "Save.h"
#include "SuperMenuText.h"
#include "TextBox.h"
#include "TextureManager.h"
#include "UISave.h"
#include "strfunc.h"
#include "yaml-cpp/yaml.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

constexpr int kShowMeTheMoneyGold = 100;

YAML::Node firstNode(const YAML::Node& root, std::initializer_list<const char*> keys)
{
    for (const auto* key : keys)
    {
        if (const auto node = root[key])
        {
            return node;
        }
    }
    return {};
}

bool loadStandaloneTeam(
    const YAML::Node& node,
    std::string_view label,
    std::vector<KysChess::ChessStandaloneBattlePiece>& pieces)
{
    if (!node || !node.IsSequence())
    {
        std::print("【測試】「{}」必須是棋子列表\n", label);
        return false;
    }
    for (const auto& entry : node)
    {
        KysChess::BattlePieceDef piece;
        const auto context = std::format(
            "測試配置{}棋子#{}",
            label,
            pieces.size() + 1);
        if (!KysChess::parseBattlePieceNode(entry, piece, context))
        {
            return false;
        }
        pieces.push_back({
            piece.roleId,
            piece.star,
            piece.weaponId,
            piece.armorId,
        });
    }
    return true;
}

void runConfiguredChessBattle()
{
    const std::string path = GameUtil::PATH() + "config/chess_test.yaml";
    YAML::Node config;
    try
    {
        config = YAML::LoadFile(path);
    }
    catch (const YAML::Exception& error)
    {
        std::print("【測試】無法讀取 {}：{}\n", path, error.what());
        return;
    }

    KysChess::ChessStandaloneBattleRequest request;
    request.profile = KysChess::ChessStandaloneBattleProfile::AutoChess;
    request.stableBattleId = "console:test";
    if (!loadStandaloneTeam(firstNode(config, {"我方"}), "我方", request.allies)
        || !loadStandaloneTeam(firstNode(config, {"敵方", "敌方"}), "敵方", request.enemies))
    {
        return;
    }

    const auto mapNode = firstNode(config, {"戰場ID", "战场ID"});
    const int mapId = mapNode ? mapNode.as<int>(-1) : -1;
    if (mapId >= 0)
    {
        request.mapId = mapId;
    }
    const auto seedNode = firstNode(config, {"隨機種子", "随机种子"});
    const int configuredSeed = seedNode ? seedNode.as<int>(-1) : -1;
    request.rootSeed = configuredSeed >= 0
        ? static_cast<std::uint64_t>(configuredSeed)
        : static_cast<std::uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
    if (configuredSeed >= 0)
    {
        request.battleSeed = static_cast<std::uint32_t>(configuredSeed);
    }

    if (const auto neigong = firstNode(config, {"內功", "内功"}))
    {
        for (const auto& magicId : neigong)
        {
            request.obtainedNeigongIds.insert(magicId.as<int>());
        }
    }

    auto& applicationSession = KysChess::applicationChessSession();
    request.options = applicationSession.state().options;
    std::string error;
    auto standalone = KysChess::ChessStandaloneBattle::prepare(
        applicationSession.sharedContent(),
        request,
        error);
    if (!standalone)
    {
        std::print("【測試】無法準備戰鬥：{}\n", error);
        return;
    }

    auto session = std::move(*standalone).createSession();
    KysChess::ChessGuiSessionAdapter(*session).runPreparedBattle();
    const bool victory = session->state().lastBattleOutcome
        == KysChess::Battle::BattleOutcome::PlayerVictory;
    std::print("【測試】戰鬥結束，結果：{}\n", victory ? "勝利" : "失敗");
}

}

Console::Console()
{
    std::string code;
    {
        auto input = std::make_shared<InputBox>("神秘代碼：", 30);
        input->setInputPosition(350, 300);
        input->run();
        if (input->getResult() >= 0)
        {
            code = input->getText();
        }
        else
        {
            return;
        }
    }
    // 捂脸
    code = strfunc::trim(code);
    auto splits = strfunc::splitString(code, " ");
    if (splits.empty())
    {
        splits.push_back("");
    }
    if (code == "showmethemoney")
    {
        KysChess::applicationChessSession().grantUnjournaledCheatMoney(kShowMeTheMoneyGold);
        std::print("金幣增加{}。\n", kShowMeTheMoneyGold);
    }
    else if (splits[0] == "item" || splits[0] == "裝備" || splits[0] == "物品")
    {
        std::print("此代碼會改寫可驗證遊戲狀態，現已停用；請使用獨立測試戰鬥。\n");
    }
    else if (code == "test")
    {
        runConfiguredChessBattle();
    }
    else
    {
        std::print("未知神秘代碼：{}\n", code);
    }
}
