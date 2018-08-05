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
    if (code == u8"teleport") {
        // ����ǿ�ƴ��ͣ�ʲô�����ܵ�
        // �������⣬����ظ������������޽��������Ӧ��д��������ҳMenuText
        std::unordered_map<std::string, int> intResult;
        std::vector<std::string> options;
        // ���Ҳ�ɷŽ�PotConv�ɣ���ʱ����������о�������˵
        opencc::SimpleConverter ccConv("s2t.json");
        for (const auto& info : Save::getInstance()->getSubMapInfos()) {
            intResult[info->Name] = info->ID;
            options.emplace_back(info->Name);
        }
        auto searchF = [&options, &ccConv](const std::string& text) {
            // �����Ͽ���Trie�ȼ��٣������������ܾ���/LCS�㷨
            // ��������ʱ����תutf8 �ٷ������� textĬ������ʾԭ��ѡ����cp936
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
        InputAutoComplete autoInput("Ոݔ����͵������ɰ��Ԅ��aȫ����", 28, limitedOptions);
        autoInput.setValidation(validateF);
        autoInput.setSearchFunction(searchF);
        autoInput.setInputPosition(150, 50);
        autoInput.run();
        int id = autoInput.getResult();
        if (id != -1)
        {
            auto scene = Save::getInstance()->getSubMapInfos()[id];
            
            MainScene::getInstance()->forceEnterSubScene(id, scene->EntranceX, scene->EntranceY);
            printf("���͵�%d\n", id);
        }
    }
}