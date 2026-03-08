#include "Console.h"
#include "BattleNetwork.h"
#include "BattleScene.h"
#include "ChessPool.h"
#include "ChessSelector.h"
#include "DrawableOnCall.h"
#include "DynamicChessMap.h"
#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "InputBox.h"
#include "MainScene.h"
#include "PotConv.h"
#include "Save.h"
#include "SuperMenuText.h"
#include "GameState.h"
#include "TextBox.h"
#include "TextureManager.h"
#include "UISave.h"
#include "strfunc.h"
#include "yaml-cpp/yaml.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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
    auto splits = strfunc::splitString(code, " ");
    if (splits.empty())
    {
        splits.push_back("");
    }
    if (code == "showmethemoney")
    {
        KysChess::GameState::get().economy().make(100);
    }
    else if (code == "test")
    {
        std::string path = GameUtil::PATH() + "config/chess_test.yaml";
        YAML::Node cfg;
        try
        {
            cfg = YAML::LoadFile(path);
        } catch (...)
        {
            std::print("【测试】无法读取 {}\n", path);
            return;
        }

        // Parse config
        DynamicBattleRoles roles;
        for (const auto& a : cfg["我方"])
        {
            roles.teammate_ids.push_back(a["角色"].as<int>());
            roles.teammate_stars.push_back(a["星级"].as<int>(1));
        }
        for (const auto& e : cfg["敌方"])
        {
            roles.enemy_ids.push_back(e["角色"].as<int>());
            roles.enemy_stars.push_back(e["星级"].as<int>(1));
        }
        int battle_id = cfg["战场ID"].as<int>(-1);
        int seed = cfg["随机种子"].as<int>(-1);

        // Temporarily override neigong
        auto& gd = KysChess::GameState::get();
        auto savedNeigong = gd.progress().getObtainedNeigong();
        if (cfg["内功"])
        {
            std::vector<int> testNg;
            for (const auto& n : cfg["内功"])
            {
                testNg.push_back(n.as<int>());
            }
            gd.progress().setObtainedNeigong(std::move(testNg));
        }

        // Auto-save
        UISave::autoSave();
        std::print("【测试】已自动存档\n");

        // Build ally Chess vector for runBattle
        std::vector<KysChess::Chess> allyChess;
        for (size_t i = 0; i < roles.teammate_ids.size(); i++)
        {
            auto r = KysChess::GameState::get().roleSave().getRole(roles.teammate_ids[i]);
            if (!r) continue;

            KysChess::Chess chess{ r, roles.teammate_stars[i] };
            if (i < roles.teammate_instances.size())
            {
                KysChess::ChessInstanceID chessInstanceId{roles.teammate_instances[i]};
                auto& collection = KysChess::GameState::get().roster().items();
                auto it = collection.find(chessInstanceId);
                if (it != collection.end())
                    chess = it->second;
                else
                    chess.id = chessInstanceId;
            }

            allyChess.push_back(chess);
        }

        KysChess::ChessSelector selector(
            gd.roleSave(),
            gd.equipmentInventory(),
            gd.roster(),
            gd.shop(),
            gd.progress(),
            gd.economy(),
            gd.random());
        int result = selector.runBattle(roles, allyChess, battle_id, seed);
        std::print("【测试】战斗结束，结果：{}\n", result == 0 ? "胜利" : "失败");

        // Restore neigong and reload auto-save
        gd.progress().setObtainedNeigong(std::move(savedNeigong));
        UISave::loadAuto();
        std::print("【测试】已恢复存档\n");
    }
}
