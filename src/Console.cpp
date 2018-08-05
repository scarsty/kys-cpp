#include "Console.h"
#include "PotConv.h"
#include "InputBox.h"
#include "InputAutoComplete.h"
#include "Save.h"
#include "MainScene.h"
#include "SuperMenuText.h"

#include "opencc/opencc.h"

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
        smt.setInputPosition(150, 50);
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
        SuperMenuText smt("請輸入傳送地名（可半自動補全）：", 28, locs, 15);
        smt.setInputPosition(150, 50);
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