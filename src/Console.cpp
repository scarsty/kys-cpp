#include "Console.h"
#include "BattleNetwork.h"
#include "BattleRoleManager.h"
#include "DynamicChessMap.h"
#include "BattleScene.h"
#include "ChessPool.h"
#include "ChessSelector.h"
#include "TempStore.h"
#include "DrawableOnCall.h"
#include "Event.h"
#include "Font.h"
#include "InputBox.h"
#include "MainScene.h"
#include "PotConv.h"
#include "Save.h"
#include "SuperMenuText.h"
#include "TextBox.h"
#include "TextureManager.h"
#include "UIRoleStatusMenu.h"
#include "strfunc.h"
#include <algorithm>
#include <functional>
#include <iostream>
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
    if (code == "menutest")
    {
        std::vector<std::pair<int, std::string>> generated;
        for (int i = 0; i < 450; i++)
        {
            generated.emplace_back(i, "a" + std::to_string(i));
        }
        auto smt = std::make_shared<SuperMenuText>("少废话", 28, generated, 10);
        smt->setInputPosition(180, 80);
        smt->run();
        int id = smt->getResult();
        LOG("result %d\n", id);
    }
    else if (code == "showmethemoney")
    {
        KysChess::GameData::get().make(100);
    }
    else if (code == "exp") 
    {
        KysChess::GameData::get().increaseExp(100);
    }
    else if (code == "SR")
    {
        KysChess::ChessSelector::getChess();
    }
    else if (code == "VC")
    {
        KysChess::ChessSelector::sellChess();
    }
    else if (code == "BT")
    {
        KysChess::ChessSelector::enterBattle();
    }
    else if (code == "SB")
    {
        KysChess::ChessSelector::selectForBattle();
    }
    else if (code == "TEST")
    {
        // Generate test chess pieces for battle testing
        auto& gameData = KysChess::GameData::get();

        // Generate 10 random chess pieces with varying tiers and stars
        std::vector<KysChess::Chess> allChess;
        for (int i = 0; i < 10; i++)
        {
            // Cycle through tiers 0-3
            int tier = i % 4;
            // Add some stars (0-2)
            int stars = (i / 4) % 3;

            auto role = KysChess::ChessPool::selectEnemyFromPool(tier);
            if (role)
            {
                KysChess::Chess chess;
                chess.role = role;
                chess.star = stars;

                // Add to collection
                gameData.collection.addChess(chess);
                allChess.push_back(chess);
            }
        }

        // Sort by strength (price) in descending order
        std::sort(allChess.begin(), allChess.end(), [](const KysChess::Chess& a, const KysChess::Chess& b) {
            int tierA = KysChess::ChessPool::GetChessTier(a.role->ID);
            int tierB = KysChess::ChessPool::GetChessTier(b.role->ID);
            int priceA = KysChess::ChessSelector::calculateCost(tierA, a.star, 1);
            int priceB = KysChess::ChessSelector::calculateCost(tierB, b.star, 1);
            return priceA > priceB;
        });

        // Select the strongest chess pieces (up to level)
        int maxSelection = std::max(gameData.getLevel() + 1, 2);
        std::vector<KysChess::Chess> testChess;
        for (int i = 0; i < std::min(maxSelection, (int)allChess.size()); i++)
        {
            testChess.push_back(allChess[i]);
        }

        // Auto-select the strongest chess for battle
        gameData.setSelectedForBattle(testChess);

        auto text = std::make_shared<TextBox>();
        text->setText(std::format("已生成{}個測試棋子並選擇{}個用於戰鬥", 10, testChess.size()));
        text->setFontSize(32);
        text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 200, Engine::getInstance()->getUIHeight() / 2);
    }
    else if (code == "RANGED")
    {
        // 1v1 ranged test: 段譽(53) vs 黃藥師(57)
        auto ally = Save::getInstance()->getRole(53);
        auto enemy = Save::getInstance()->getRole(57);
        if (ally && enemy)
        {
            ally->HP = ally->MaxHP;
            enemy->HP = enemy->MaxHP;
            KysChess::Chess c{ally, 0};
            auto ids = KysChess::BattleRoleManager::applyEnhancements({c});
            DynamicBattleRoles roles;
            roles.teammate_ids = ids;
            roles.enemy_ids = {enemy->ID};
            auto battle = DynamicChessMap::createBattle(roles);
            battle->run();
            KysChess::BattleRoleManager::restoreRoles();
        }
    }
    else if (RunNode::getPointerFromRoot<SubScene>() == nullptr
        && (code == "chuansong" || code == "teleport" || code == "mache" || code == ""))
    {
        std::vector<std::pair<int, std::string>> locs;
        for (const auto& info : Save::getInstance()->getSubMapInfos())
        {
            // 还有其他要求 这里作为一个demo就意思意思
            if (info->MainEntranceX1 != 0 && info->MainEntranceY1 != 0)
            {
                std::string name(info->Name);
                // 有空格方便完成双击确认
                locs.emplace_back(info->ID, name + std::to_string(info->ID));
            }
        }
        int dx = 180;
        int dy = 80;
        auto drawScene = [dx, dy](DrawableOnCall* d)
        {
            if (d->getID() == -1)
            {
                return;
            }
            int id = d->getID();
            auto scene = Save::getInstance()->getSubMapInfos()[id];
            int nx = dx + 350;
            int ny = dy + 100;
            int fontSize = 28;
            TextureManager::getInstance()->renderTexture("title", 126, { nx, ny, 400, 400 }, { 192, 192, 192, 255 }, 255);
            //Engine::getInstance()->fillColor({ 0, 0, 0, 192 }, nx, ny, 400, 400);
            Font::getInstance()->draw(std::format("{}，{}", scene->Name, scene->ID), fontSize, nx + 20, ny + 20);
            Font::getInstance()->draw(std::format("（{}，{}）", scene->MainEntranceX1, scene->MainEntranceY1),
                fontSize, nx + 20, ny + 20 + fontSize * 1.5);

            int man_x_ = scene->MainEntranceX1;
            int man_y_ = scene->MainEntranceY1;
            auto mainScene = MainScene::getInstance();

            if (man_x_ == 0 && man_y_ == 0)
            {
                return;
            }
            // 不会画场景，需要慢慢学习，不行我复制个代码 强行搞

            struct DrawInfo
            {
                int index;
                TextureWarpper* t;
                Point p;
            };

            std::vector<DrawInfo> building_vec(1000);
            int building_count = 0;

            int hw = 2;
            for (int sum = -hw; sum <= hw + 10; sum++)
            {
                for (int i = -hw; i <= hw; i++)
                {
                    int ix = man_x_ + i + (sum / 2);
                    int iy = man_y_ - i + (sum - sum / 2);
                    auto p = mainScene->getPositionOnRender(ix, iy, man_x_, man_y_);
                    p.x += nx - 160;
                    p.y += ny;
                    if (mainScene->building_layer_.data(ix, iy).getTexture())
                    {
                        //根据图片的宽度计算图的中点, 为避免出现小数, 实际是中点坐标的2倍
                        //次要排序依据是y坐标
                        //直接设置z轴
                        auto tex = mainScene->building_layer_.data(ix, iy).getTexture();
                        if (tex == nullptr)
                        {
                            continue;
                        }
                        auto w = tex->w;
                        auto h = tex->h;
                        auto dy = tex->dy;
                        int c = ((ix + iy) - (w + 35) / 36 - (dy - h + 1) / 9) * 1024 + ix;
                        //map[2 * c + 1] = { 2*c+1, t, p };
                        building_vec[building_count++] = { 2 * c + 1, tex, p };
                    }

                    auto sort_building = [](DrawInfo& d1, DrawInfo& d2)
                    {
                        return d1.index < d2.index;
                    };
                    std::sort(building_vec.begin(), building_vec.begin() + building_count, sort_building);
                    for (int i = 0; i < building_count; i++)
                    {
                        auto& d = building_vec[i];
                        TextureManager::getInstance()->renderTexture(d.t, d.p.x, d.p.y);
                    }
                }
            }
        };
        std::shared_ptr<DrawableOnCall> doc = std::make_shared<DrawableOnCall>(drawScene);
        auto smt = std::make_shared<SuperMenuText>("可輸入傳送地名，編號或拼音搜索：", 28, locs, 15);
        smt->setInputPosition(dx, dy);
        smt->addDrawableOnCall(doc);

        smt->run();
        int id = smt->getResult();
        if (id != -1)
        {
            auto scene = Save::getInstance()->getSubMapInfos()[id];
            MainScene::getInstance()->forceEnterSubScene(id, scene->EntranceX, scene->EntranceY);
            MainScene::getInstance()->setManPosition(scene->MainEntranceX1, scene->MainEntranceY1);
            LOG("傳送到{}\n", id);
        }
    }
    /*
    else if (splits[0] == "newsave" && splits.size() >= 2)
    {
        int rec;
        try
        {
            rec = std::stoi(splits[1]);
        }
        catch (...)
        {
            return;
        }
        auto save = Save::getInstance();
        auto main_scene = MainScene::getInstance();
        main_scene->getManPosition(save->MainMapX, save->MainMapY);
        save->InSubMap = -1;
        Save::getInstance()->saveRToCSV(rec);
        Save::getInstance()->saveSD(rec);
    }
    else if (splits[0] == "newload" && splits.size() >= 2)
    {
        int rec;
        try
        {
            rec = std::stoi(splits[1]);
        }
        catch (...)
        {
            return;
        }
        auto save = Save::getInstance();
        auto main_scene = MainScene::getInstance();
        Save::getInstance()->loadRFromCSV(rec);
        Save::getInstance()->loadSD(rec);
        main_scene->setManPosition(save->MainMapX, save->MainMapY);
        if (save->InSubMap >= 0)
        {
            main_scene->forceEnterSubScene(save->InSubMap, save->SubMapX, save->SubMapY);
        }
    }
    else if (splits[0] == "rinsert" && splits.size() >= 3)
    {
        int idx;
        try
        {
            idx = std::stoi(splits[2]);
        }
        catch (...)
        {
            return;
        }
        Save::getInstance()->insertAt(splits[1], idx);
    }*/
    else if (splits.size() > 1 && splits[0] == "host")
    {
        Save::getInstance()->save(11);

        auto host = BattleNetworkFactory::MakeHost(splits[1]);
        if (!host)
        {
            return;
        }

        auto battle = std::make_shared<BattleScene>();
        battle->setupNetwork(std::move(host));
        battle->run();

        Save::getInstance()->load(11);
    }
    else if (splits.size() > 1 && splits[0] == "client")
    {
        Save::getInstance()->save(11);

        auto client = BattleNetworkFactory::MakeClient(splits[1]);
        if (!client)
        {
            return;
        }

        auto battle = std::make_shared<BattleScene>();
        battle->setupNetwork(std::move(client));
        battle->run();

        Save::getInstance()->load(11);
    }
    else if (splits.size() > 1 && (splits[0] == "battle" || splits[0] == "b"))
    {
        Save::getInstance()->save(11);
        int k = atoi(splits[1].c_str());
        Event::getInstance()->tryBattle(k, 0);
        //Save::getInstance()->load(11);
    }
}
