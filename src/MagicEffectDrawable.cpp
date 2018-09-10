#include "MagicEffectDrawable.h"
#include "BattleConfig.h"
#include "TextBoxRoll.h"

MagicEffectDrawable::MagicEffectDrawable(BattleMod::BattleConfManager & conf, Role * role, Role * opponent, int x, int y) :
    DrawableOnCall(nullptr), conf_(conf), role_(role), opponent_(opponent), x_(x), y_(y)
{
}

void MagicEffectDrawable::updateScreenWithID(int id)
{
    texts_.clear();
    if (id == -1)
    {
        return;
    }
    Magic* wg = Save::getInstance()->getMagic(id);
    if (!wg) return;
    conf_.simulation = true;
    auto desc = conf_.getMagicDescriptions(role_, opponent_, wg);
    conf_.simulation = false;
    for (auto& str : desc) {
        texts_.push_back({ {{ 0, 0, 0, 255 }, str} });
    }
}

void MagicEffectDrawable::draw()
{
    BP_Rect rect{ x_ - 30, y_ - 25, 550, 350 };
    TextureManager::getInstance()->renderTexture("title", 17, rect, { 255, 255, 255, 255 }, 255);
    TextBoxRoll tbr;
    tbr.setFontSize(20);
    tbr.setTexts(texts_);
    tbr.setPosition(x_, y_);
    tbr.draw();
}


