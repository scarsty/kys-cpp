#include "Console.h"
#include "PotConv.h"
#include "InputBox.h"
#include "InputAutoComplete.h"
#include "Save.h"
#include "MainScene.h"
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
    if (code == u8"teleport") {
        // 这是强制传送，什么都不管的
        // 致命问题，多个重复场景名，毫无解决方案，应改写可搜索多页MenuText
        std::unordered_map<std::string, int> intResult;
        std::vector<std::string> options;
        // 这个也可放进PotConv吧，暂时先留这里，我研究点别的再说
        opencc::SimpleConverter ccConv("s2t.json");
        for (const auto& info : Save::getInstance()->getSubMapInfos()) {
            intResult[info->Name] = info->ID;
            options.emplace_back(info->Name);
        }
        auto searchF = [&options, &ccConv](const std::string& text) {
            // 理论上可用Trie等加速，或其他更智能距离/LCS算法
            // 这里搜索时，先转utf8 再反复横跳 text默认因显示原因选择了cp936
            std::string u8Text = PotConv::cp936toutf8(text);
            std::string converted = ccConv.Convert(u8Text);
            std::string realText = PotConv::conv(converted, "utf-8", "cp936");
            std::vector<std::string> results;
            for (auto& opt : options) {
                auto size = std::min(opt.size(), realText.size());
                if (memcmp(realText.c_str(), opt.c_str(), size) == 0) {
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
}