#include "Console.h"
#include "DrawableOnCall.h"
#include "PotConv.h"
#include "InputBox.h"
#include "Save.h"
#include "MainScene.h"
#include "SuperMenuText.h"
#include "Font.h"
#include "libconvert.h"
#include "hanzi2pinyin.h"
#include "BattleNetwork.h"
#include "BattleScene.h"

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <algorithm>
#include <sstream>
#include <iostream>


Console::Console()
{
    std::string code;
    {
        InputBox input("���ش��a��", 30);
        input.setInputPosition(350, 300);
        input.run();
        if (input.getResult() >= 0)
        {
            code = input.getText();
        }
    }
    // ����
    code = PotConv::conv(code, "cp936", "utf-8");
    auto splits = convert::splitString(code, " ");
    if (splits.empty()) return;
    if (code == u8"menutest")
    {
        std::vector<std::pair<int, std::string>> generated;
        for (int i = 0; i < 450; i++)
        {
            generated.emplace_back(i, "a" + std::to_string(i));
        }
        SuperMenuText smt("�ٷϻ�", 28, generated, 10);
        smt.setInputPosition(180, 80);
        smt.run();
        int id = smt.getResult();
        printf("result %d\n", id);
    }
    else if (code == u8"chuansong" || code == u8"teleport" || code == u8"mache") 
    {
        std::vector<std::pair<int, std::string>> locs;
        for (const auto& info : Save::getInstance()->getSubMapInfos()) 
        {
            // ��������Ҫ�� ������Ϊһ��demo����˼��˼
            if (info->MainEntranceX1 != 0 && info->MainEntranceY1 != 0)
            {
                std::string name(info->Name);
                // �пո񷽱����˫��ȷ��
                locs.emplace_back(info->ID, name);
            }
        }
        int dx = 180;
        int dy = 80;
        auto drawScene = [dx, dy](DrawableOnCall* d)
        {
            if (d->getID() == -1) return;
            int id = d->getID();
            auto scene = Save::getInstance()->getSubMapInfos()[id];
            int nx = dx + 350;
            int ny = dy + 100;
            int fontSize = 28;
            Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, nx, ny, 400, 400);
            Font::getInstance()->draw(convert::formatString("%s��%d", scene->Name, scene->ID), fontSize, nx + 20, ny + 20);
            Font::getInstance()->draw(convert::formatString("��%d��%d��", scene->MainEntranceX1, scene->MainEntranceY1), 
                                      fontSize, nx + 20, ny + 20 + fontSize*1.5);

            int man_x_ = scene->MainEntranceX1;
            int man_y_ = scene->MainEntranceY1;
            auto mainScene = MainScene::getInstance();

            if (man_x_ == 0 && man_y_ == 0) return;
            // ���ử��������Ҫ����ѧϰ�������Ҹ��Ƹ����� ǿ�и�

            struct DrawInfo
            {
                int index;
                int i;
                Point p;
            };
            
            std::vector<DrawInfo> building_vec(1000);
            int building_count = 0;

            int hw = 2;
            for (int sum = -hw; sum <= hw + 15; sum++)
            {
                for (int i = -hw; i <= hw; i++)
                {
                    int ix = man_x_ + i + (sum / 2);
                    int iy = man_y_ - i + (sum - sum / 2);
                    auto p = mainScene->getPositionOnRender(ix, iy, man_x_, man_y_);
                    p.x += nx - 160;
                    p.y += ny;
                    if (mainScene->building_layer_->data(ix, iy) > 0)
                    {
                        auto t = mainScene->building_layer_->data(ix, iy);
                        //����ͼƬ�Ŀ�ȼ���ͼ���е�, Ϊ�������С��, ʵ�����е������2��
                        //��Ҫ����������y����
                        //ֱ������z��
                        auto tex = TextureManager::getInstance()->loadTexture("mmap", t);
                        if (tex == nullptr) continue;
                        auto w = tex->w;
                        auto h = tex->h;
                        auto dy = tex->dy;
                        int c = ((ix + iy) - (w + 35) / 36 - (dy - h + 1) / 9) * 1024 + ix;
                        //map[2 * c + 1] = { 2*c+1, t, p };
                        building_vec[building_count++] = { 2 * c + 1, t, p };
                    }

                    auto sort_building = [](DrawInfo& d1, DrawInfo& d2)
                    {
                        return d1.index < d2.index;
                    };
                    std::sort(building_vec.begin(), building_vec.begin() + building_count, sort_building);
                    for (int i = 0; i < building_count; i++)
                    {
                        auto& d = building_vec[i];
                        TextureManager::getInstance()->renderTexture("mmap", d.i, d.p.x, d.p.y);
                    }
                }
            }
        };
        auto doc = new DrawableOnCall(drawScene);
        SuperMenuText smt("��ݔ����͵�������̖��ƴ��������", 28, locs, 15);
        smt.setInputPosition(dx, dy);
        smt.addDrawableOnCall(doc);

        smt.run();
        int id = smt.getResult();
        if (id != -1)
        {
            auto scene = Save::getInstance()->getSubMapInfos()[id];
            MainScene::getInstance()->forceEnterSubScene(id, scene->EntranceX, scene->EntranceY);
            printf("���͵�%d\n", id);
        }
    }
    else if (splits[0] == u8"newsave" && splits.size() >= 2) 
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
    else if (splits[0] == u8"newload" && splits.size() >= 2) 
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
    else if (splits[0] == u8"rinsert" && splits.size() >= 3) 
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
    }
    else if (splits[0] == u8"host" && splits.size() >= 1) 
    {
        Save::getInstance()->save(11);

        auto host = BattleNetworkFactory::MakeHost(splits[1]);
        if (!host) return;

        BattleScene battle;
        battle.setupNetwork(std::move(host));
        battle.run();

        Save::getInstance()->load(11);
    }
    else if (splits[0] == u8"client" && splits.size() >= 1) 
    {
        Save::getInstance()->save(11);

        auto client = BattleNetworkFactory::MakeClient(splits[1]);
        if (!client) return;

        BattleScene battle;
        battle.setupNetwork(std::move(client));
        battle.run();

        Save::getInstance()->load(11);
    }
}