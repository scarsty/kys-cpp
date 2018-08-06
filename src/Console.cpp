#include "Console.h"
#include "DrawableOnCall.h"
#include "PotConv.h"
#include "InputBox.h"
#include "InputAutoComplete.h"
#include "Save.h"
#include "MainScene.h"
#include "SuperMenuText.h"
#include "Font.h"
#include "libconvert.h"

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <functional>
#include <algorithm>


Console::Console() {
    std::string code;
    {
        InputBox input("神秘代碼：", 30);
        input.setInputPosition(350, 300);
        input.run();
        if (input.getResult() >= 0)
        {
            code = input.getText();
        }
    }
    // 捂脸
    code = PotConv::conv(code, "cp936", "utf-8");
    if (code == u8"teleport1") {
        // 这是强制传送，什么都不管的
        // 致命问题，多个重复场景名，毫无解决方案，应改写可搜索多页MenuText
        std::unordered_map<std::string, int> intResult;
        std::vector<std::string> options;
        for (const auto& info : Save::getInstance()->getSubMapInfos()) {
            intResult[info->Name] = info->ID;
            options.emplace_back(info->Name);
        }
        auto searchF = [&options](const std::string& text) {
            // 理论上可用Trie等加速，或其他更智能距离/LCS算法
            std::vector<std::string> results;
            for (auto& opt : options) {
                auto size = std::min(opt.size(), text.size());
                if (memcmp(text.c_str(), opt.c_str(), size) == 0) {
                    results.emplace_back(opt);
                }
            }
            return results;
        };
        auto validateF = [&intResult](const std::string& text) {
            auto iter = intResult.find(text);
            if (iter != intResult.end()) {
                return iter->second;
            }
            return -1;
        };
        auto limitedOptions = options;
        limitedOptions.resize(std::min(10u, options.size()));
        InputAutoComplete autoInput("請輸入傳送地名（可半自動補全）：", 28, limitedOptions);
        autoInput.setValidation(validateF);
        autoInput.setSearchFunction(searchF);
        autoInput.setInputPosition(150, 50);
        autoInput.run();
        int id = autoInput.getResult();
        if (id != -1)
        {
            auto scene = Save::getInstance()->getSubMapInfos()[id];
            
            MainScene::getInstance()->forceEnterSubScene(id, scene->EntranceX, scene->EntranceY);
            printf("傳送到%d\n", id);
        }
    }
    else if (code == u8"menutest") {
        std::vector<std::string> generated;
        for (int i = 0; i < 450; i++) {
            generated.push_back("a" + std::to_string(i));
        }
        SuperMenuText smt("少废话", 28, generated, 10);
        smt.setInputPosition(180, 80);
        smt.run();
        int id = smt.getResult();
        printf("result %d\n", id);
    }
    else if (code == u8"teleport") {
        // 如果需要过滤不能进入的，这里就会比较复杂了
        // 返回的idx需要再映射一边
        std::vector<std::string> locs;
        for (const auto& info : Save::getInstance()->getSubMapInfos()) {
            std::string name(info->Name);
            locs.push_back(name + " " + std::to_string(info->ID));
        }
        int dx = 180;
        int dy = 80;
        auto drawScene = [dx, dy](DrawableOnCall* d) {
            if (d->getID() == -1) return;
            auto scene = Save::getInstance()->getSubMapInfos()[d->getID()];
            int nx = dx + 350;
            int ny = dy + 100;
            int fontSize = 28;
            Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, nx, ny, 400, 400);
            Font::getInstance()->draw(scene->Name, fontSize, nx + 20, ny + 20);
            Font::getInstance()->draw(convert::formatString("(%d, %d)", scene->MainEntranceX1, scene->MainEntranceY1), 
                                      fontSize, nx + 20, ny + 20 + fontSize*1.5);

            int man_x_ = scene->MainEntranceX1;
            int man_y_ = scene->MainEntranceY1;
            auto mainScene = MainScene::getInstance();

            if (man_x_ == 0 && man_y_ == 0) return;
            // 不会画场景，需要慢慢学习，不行我复制个代码 强行搞

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
                        //根据图片的宽度计算图的中点, 为避免出现小数, 实际是中点坐标的2倍
                        //次要排序依据是y坐标
                        //直接设置z轴
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
        SuperMenuText smt("請輸入傳送地名（可半自動補全）：", 28, locs, 15);
        smt.setInputPosition(dx, dy);
        smt.addDrawableOnCall(doc);
        smt.run();
        int id = smt.getResult();
        if (id != -1)
        {
            auto scene = Save::getInstance()->getSubMapInfos()[id];

            MainScene::getInstance()->forceEnterSubScene(id, scene->EntranceX, scene->EntranceY);
            printf("傳送到%d\n", id);
        }
    }
}