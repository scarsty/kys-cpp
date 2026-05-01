#include "Console.h"
#include "BattleNetwork.h"
#include "BattleScene.h"
#include "ChessPool.h"
#include "ChessRewardFlow.h"
#include "ChessSelector.h"
#include "DrawableOnCall.h"
#include "DynamicChessMap.h"
#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "InputBox.h"
#include "MainScene.h"
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
    code = strfunc::trim(code);
    auto splits = strfunc::splitString(code, " ");
    if (splits.empty())
    {
        splits.push_back("");
    }
    if (code == "showmethemoney")
    {
        KysChess::GameState::get().economy().make(100);
    }
    else if (splits[0] == "item" || splits[0] == "裝備" || splits[0] == "物品")
    {
        auto& gd = KysChess::GameState::get();
        KysChess::ChessRewardFlow rewardFlow({
            gd.roleSave(),
            gd.equipmentInventory(),
            gd.roster(),
            gd.shop(),
            gd.progress(),
            gd.economy(),
            gd.random(),
        });
        KysChess::BalanceConfig::ChallengeReward reward{
            KysChess::BalanceConfig::ChallengeRewardType::GetEquipment,
            99,
            0,
        };
        if (rewardFlow.applyReward(reward))
        {
            UISave::autoSave();
        }
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
        auto loadTeam = [&](const YAML::Node& teamNode, bool isEnemy, const char* teamLabel) {
            if (!teamNode || !teamNode.IsSequence())
            {
                std::print("【测试】「{}」必须是棋子列表\n", teamLabel);
                return false;
            }

            for (const auto& entry : teamNode)
            {
                KysChess::BattlePieceDef piece;
                auto context = std::format("测试配置{}棋子#{}", teamLabel, isEnemy ? roles.enemy_ids.size() + 1 : roles.teammate_ids.size() + 1);
                if (!KysChess::parseBattlePieceNode(entry, piece, context))
                    return false;

                if (isEnemy)
                {
                    roles.enemy_ids.push_back(piece.roleId);
                    roles.enemy_stars.push_back(piece.star);
                    roles.enemy_weapons.push_back(piece.weaponId);
                    roles.enemy_armors.push_back(piece.armorId);
                }
                else
                {
                    roles.teammate_ids.push_back(piece.roleId);
                    roles.teammate_stars.push_back(piece.star);
                    roles.teammate_weapons.push_back(piece.weaponId);
                    roles.teammate_armors.push_back(piece.armorId);
                }
            }
            return true;
        };

        if (!loadTeam(cfg["我方"], false, "我方"))
        {
            return;
        }
        if (!loadTeam(cfg["敌方"], true, "敌方"))
        {
            return;
        }
        int battle_id = cfg["战场ID"].as<int>(-1);
        int seed = cfg["随机种子"].as<int>(-1);

        // 先保存原狀，測試用內功不能寫進自動存檔。
        auto& gd = KysChess::GameState::get();
        auto savedNeigong = gd.progress().getObtainedNeigong();
        UISave::autoSave();
        std::print("【测试】已自动存档\n");

        // 暫時覆蓋內功
        if (cfg["内功"])
        {
            std::vector<int> testNg;
            for (const auto& n : cfg["内功"])
            {
                testNg.push_back(n.as<int>());
            }
            gd.progress().setObtainedNeigong(std::move(testNg));
        }

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

        // 讀回自動存檔後再強制還原內功，避免測試戰鬥流程中途存到臨時進度。
        UISave::loadAuto();
        gd.progress().setObtainedNeigong(savedNeigong);
        std::print("【测试】已恢复存档\n");
    }
}
