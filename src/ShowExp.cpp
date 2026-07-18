#include "ShowExp.h"
#include "Engine.h"
#include "Font.h"
#include "TextureManager.h"

ShowExp::ShowExp()
{
    x_ = 100;
    y_ = 100;
    entered_ticks_ = Engine::getInstance()->getTicks();
}

ShowExp::~ShowExp()
{
}

void ShowExp::dealEvent(EngineEvent& e)
{
    if (!ok_enabled_)
    {
        // 战斗结束前可能残留空格/A的松开事件，进入经验界面后会立刻触发OK并跳过显示。
        // 先吞掉第一次松开事件；若玩家重新按下确认键，则正常允许下一次松开关闭界面。
        if ((e.type == EVENT_KEY_DOWN && (e.key.key == K_RETURN || e.key.key == K_SPACE))
            || (e.type == EVENT_MOUSE_BUTTON_DOWN && e.button.button == BUTTON_LEFT)
            || (e.type == EVENT_GAMEPAD_BUTTON_DOWN && e.gbutton.button == GAMEPAD_BUTTON_SOUTH))
        {
            ok_enabled_ = true;
        }
        else if ((e.type == EVENT_KEY_UP && (e.key.key == K_RETURN || e.key.key == K_SPACE))
            || (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_LEFT)
            || (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_SOUTH))
        {
            if (Engine::getInstance()->getTicks() - entered_ticks_ >= 150)
            {
                ok_enabled_ = true;
                return;
            }
            ok_enabled_ = true;
            e.type = EVENT_FIRST;
        }
    }
}

void ShowExp::onPressedOK()
{
    if (ok_enabled_)
    {
        exitWithResult(0);
    }
}

void ShowExp::draw()
{
    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    Font::getInstance()->draw(text_, 30, Engine::getInstance()->getUIWidth() / 2 - 15 * Font::getTextDrawSize(text_)/2, y_, { 255, 255, 255, 255 });
    for (int i = 0; i < roles_.size(); i++)
    {
        auto r = roles_[i];
        constexpr int row_count = 3;
        constexpr int item_width = 180;
        constexpr int row_height = 100;
        int row = i / row_count;
        int col = i % row_count;
        int count_in_row = int(roles_.size()) - row * row_count;
        if (count_in_row > row_count)
        {
            count_in_row = row_count;
        }
        int row_width = count_in_row * item_width;
        int row_x = Engine::getInstance()->getUIWidth() / 2 - row_width / 2;
        int x = row_x + col * item_width, y = y_ + 50 + row * row_height;
        TextureManager::getInstance()->renderTexture("head", r->HeadID, x, y, { { 255, 255, 255, 255 }, 255, 0.5, 0.5 });
        Font::getInstance()->draw(std::format("{}", r->Name), 20, x + 90, y + 30, { 255, 255, 255, 255 });
        Font::getInstance()->draw(std::format("{}", r->ExpGot), 20, x + 90, y + 55, { 255, 255, 255, 255 });
    }
}
