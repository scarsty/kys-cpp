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
#include "NewSave.h"

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <sstream>


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
        std::vector<std::string> generated;
        for (int i = 0; i < 450; i++)
        {
            generated.push_back("a" + std::to_string(i));
        }
        SuperMenuText smt("�ٷϻ�", 28, generated, 10);
        smt.setInputPosition(180, 80);
        smt.run();
        int id = smt.getResult();
        printf("result %d\n", id);
    }
    else if (code == u8"chuansong" || code == u8"teleport") 
    {
        // ���ص�idx��Ҫ��ӳ��һ��
        std::vector<std::string> locs;
        std::unordered_map<int, int> realIdxMapping;
        for (const auto& info : Save::getInstance()->getSubMapInfos()) 
        {
            // ��������Ҫ�� ������Ϊһ��demo����˼��˼
            if (info->MainEntranceX1 != 0 && info->MainEntranceY1 != 0)
            {
                realIdxMapping[locs.size()] = info->ID;
                std::string name(info->Name);
                // �пո񷽱����˫��ȷ��
                locs.push_back(name + " ");
            }
        }
        int dx = 180;
        int dy = 80;
        auto drawScene = [dx, dy, &realIdxMapping](DrawableOnCall* d)
        {
            if (d->getID() == -1) return;
            int id = realIdxMapping[d->getID()];
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
        SuperMenuText smt("Ոݔ����͵������ɰ��Ԅ��aȫ����", 28, locs, 15);
        smt.setInputPosition(dx, dy);
        smt.addDrawableOnCall(doc);
        if (code == u8"chuansong") 
        {
            // ƴ������Ч����΢���˵㣬����Ҫ
            auto f = [](const std::string& input, const std::string& name) 
            {
                auto u8Name = PotConv::cp936toutf8(name);
                std::string pinyin;
                pinyin.resize(1024);
                int size = hanz2pinyin(u8Name.c_str(), u8Name.size(), &pinyin[0]);
                pinyin.resize(size);
                // ����Ϊֹ std::string ����split��join ����ʧ��
                // ����Ч���ж�
                auto pys = convert::splitString(pinyin, " ");
                // ���жϿ�ͷ
                if (input.size() <= pys.size())
                {
                    bool initialOk = true;
                    for (int i = 0; i < input.size(); i++)
                    {
                        if (input[i] != pys[i][0])
                        {
                            initialOk = false;
                        }
                    }
                    if (initialOk) return true;
                }
                // ��ȫ�ж�
                std::stringstream ss;
                for (auto& py : pys)
                {
                    ss << py;
                }
                std::string fullPy = ss.str();
                if (fullPy.size() < input.size()) return false;
                return (memcmp(fullPy.c_str(), input.c_str(), input.size()) == 0);
            };
            smt.setMatchFunction(f);
        }
        smt.run();
        int id = smt.getResult();
        if (id != -1)
        {
            id = realIdxMapping[id];
            auto scene = Save::getInstance()->getSubMapInfos()[id];
            MainScene::getInstance()->forceEnterSubScene(id, scene->EntranceX, scene->EntranceY);
            printf("���͵�%d\n", id);
        }
    }
    else if (splits[0] == u8"newsave" && splits.size() >= 2) {
        int rec;
        try {
            rec = std::stoi(splits[1]);
        }
        catch (...) {
            return;
        }
        Save::getInstance()->saveCSV(rec);
    }
    else if (splits[0] == u8"newload" && splits.size() >= 2) {
        int rec;
        try {
            rec = std::stoi(splits[1]);
        }
        catch (...) {
            return;
        }
        Save::getInstance()->loadCSV(rec);
    }
    else if (splits[0] == u8"rinsert" && splits.size() >= 3) {
        int idx;
        try {
            idx = std::stoi(splits[2]);
        }
        catch (...) {
            return;
        }
        Save::getInstance()->insertAt(splits[1], idx);
    }

}