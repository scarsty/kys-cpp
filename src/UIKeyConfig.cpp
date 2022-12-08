#include "UIKeyConfig.h"

#include "Button.h"
#include "INIReader.h"
#include "strfunc.h"
#include "GameUtil.h"

UIKeyConfig::UIKeyConfig()
{
    key_ = *getKeyConfig();
    setStrings({ "左" + keyToString(key_.Left), "上" + keyToString(key_.Up), "右" + keyToString(key_.Right), "下" + keyToString(key_.Down),
        "轻" + keyToString(key_.Light), "重" + keyToString(key_.Heavy), "远" + keyToString(key_.Long), "闪" + keyToString(key_.Slash), "器" + keyToString(key_.Item), 
        "確定", "取消" });
    setFontSize(30);
    childs_[0]->setPosition(0, 50);
    childs_[1]->setPosition(120, 0);
    childs_[2]->setPosition(240, 50);
    childs_[3]->setPosition(120, 100);

    childs_[4]->setPosition(400, 50);
    childs_[5]->setPosition(520, 0);
    childs_[6]->setPosition(640, 50);
    childs_[7]->setPosition(520, 100);
    childs_[8]->setPosition(760, 100);

    childs_[9]->setPosition(500, 200);
    childs_[10]->setPosition(600, 200);

}

void UIKeyConfig::onPressedOK()
{
    checkActiveToResult();
    if (result_ >= 0 && result_ < 9)
    {
        auto button = std::make_shared<ButtonGetKey>();
        int x, y;
        childs_[result_]->getPosition(x, y);
        button->setText("按下一個鍵");
        button->runAtPosition(x + 20, y + 20);
        int r = button->getResult();

        auto b = (Button*)childs_[result_].get();
        b->setText(b->getText().substr(0, 3) + keyToString(r));
        switch (result_)
        {
        case 0: key_.Left = r; break;
        case 1: key_.Up = r; break;
        case 2: key_.Right = r; break;
        case 3: key_.Down = r; break;
        case 4: key_.Light = r; break;
        case 5: key_.Heavy = r; break;
        case 6: key_.Long = r; break;
        case 7: key_.Slash = r; break;
        case 8: key_.Item = r; break;
        }
    }
    if (result_ == 9)
    {
        *getKeyConfig() = key_;
        GameUtil::getInstance()->setKey("game", "key", toString());
        setExit(true);
    }
    if (result_ == 10)
    {
        setExit(true);
    }
}

void UIKeyConfig::onPressedCancel()
{
    setExit(true);
}

void UIKeyConfig::readFromString(std::string str)
{
    auto& k = *getKeyConfig();
    auto v = strfunc::findNumbers<int>(str);
    if (v.size() >= 9)
    {
        k = Keys{ v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8] };
    }
}

std::string UIKeyConfig::toString()
{
    auto k = getKeyConfig();
    return fmt1::format("{},{},{},{},{},{},{},{},{}", k->Left, k->Up, k->Right, k->Down, k->Light, k->Heavy, k->Long, k->Slash, k->Item);
}

std::string UIKeyConfig::keyToString(int k)
{
    std::string str=R"([ ])";
    str[1] = char(k);
    return str;
}
