#include "Head.h"
#include "Font.h"
#include "GameUtil.h"
#include "Save.h"
#include "TextureManager.h"

Head::Head(Role* r)
{
    role_ = r;
    setTextPosition(20, 65);
    setFontSize(20);
    setTextColor({ 255, 255, 255, 255 });
    setHaveBox(false);
}

Head::~Head()
{
}

void Head::setRole(Role* r)
{
    role_ = r;
    if (r)
    {
        HP_ = r->HP;
        MP_ = r->MP;
        PhysicalPower_ = r->PhysicalPower;
    }
    else
    {
        HP_ = 0;
        MP_ = 0;
        PhysicalPower_ = 0;
    }
}

void Head::backRun()
{
}

void Head::draw()
{
    w_ = 250;
    h_ = 90;
    if (role_ == nullptr) { return; }
    BP_Color color = { 255, 255, 255, 255 }, white = { 255, 255, 255, 255 };
    auto font = Font::getInstance();

    if (style_ == 0)
    {
        TextureManager::getInstance()->renderTexture("title", 102, x_, y_);
    }
    if (state_ == NodeNormal)
    {
        color = { 160, 160, 160, 255 };
    }
    if (always_light_)
    {
        color = { 255, 255, 255, 255 };
    }
    if (role_->HP <= 0)
    {
        color.r /= 4;
        color.g /= 4;
        color.b /= 4;
    }
    //中毒时突出绿色
    color.r -= 2 * role_->Poison;
    color.b -= 2 * role_->Poison;

    //下面都是画血条等
    if (style_ == 0)
    {
        TextureManager::getInstance()->renderTexture("head", role_->HeadID, x_ + 10, y_, color, 255, 0.5, 0.5);
        TextBox::draw();
        font->draw(role_->Name, 16, x_ + 117, y_ + 9, white);
        BP_Rect r1 = { 0, 0, 0, 0 };
        font->draw(fmt1::format("{}", role_->Level), 16, x_ + 99 - 4 * GameUtil::digit(role_->Level), y_ + 5, { 250, 200, 50, 255 });

        BP_Color c, c_text;
        if (role_->MaxHP > 0)
        {
            r1 = { x_ + 96, y_ + 32, 138 * role_->HP / role_->MaxHP, 9 };
        }
        else
        {
            r1 = { 0, 0, 0, 0 };
        }
        c = { 196, 25, 16, 255 };
        Engine::getInstance()->renderSquareTexture(&r1, c, 192);
        font->draw(fmt1::format("{:3}/{:3}", role_->HP, role_->MaxHP), 16, x_ + 138, y_ + 28, { 250, 200, 50, 255 });

        if (role_->MaxMP > 0)
        {
            r1 = { x_ + 96, y_ + 48, 138 * role_->MP / role_->MaxMP, 9 };
        }
        else
        {
            r1 = { 0, 0, 0, 0 };
        }
        c = { 200, 200, 200, 255 };
        c_text = white;
        if (role_->MPType == 0)
        {
            c = { 112, 12, 112, 255 };
            c_text = { 240, 150, 240, 255 };
        }
        else if (role_->MPType == 1)
        {
            c = { 224, 180, 32, 255 };
            c_text = { 250, 200, 50, 255 };
        }
        Engine::getInstance()->renderSquareTexture(&r1, c, 192);
        font->draw(fmt1::format("{:3}/{:3}", role_->MP, role_->MaxMP), 16, x_ + 138, y_ + 44, c_text);

        r1 = { x_ + 115, y_ + 65, 83 * role_->PhysicalPower / 100, 9 };
        c = { 128, 128, 255, 255 };
        Engine::getInstance()->renderSquareTexture(&r1, c, 192);
        font->draw(fmt1::format("{}", role_->PhysicalPower), 16, x_ + 154 - 4 * GameUtil::digit(role_->PhysicalPower), y_ + 61, { 250, 200, 50, 255 });
    }
    else if (style_ == 1)
    {
        //TextureManager::getInstance()->renderTexture("head", role_->HeadID, x_ - 10, y_ - 10, { 255, 255, 255, 255 }, 255, 0.15, 0.15);
        BP_Rect r1 = { x_ + 0, y_ + 0, width_, 11 }, r2;
        BP_Color c, c_text;
        Engine::getInstance()->fillColor({ 0, 0, 0, 168 }, r1.x, r1.y, r1.w, r1.h);
        int w = (width_ - 2) * role_->HP / role_->MaxHP;
        if (role_->MaxHP > 0)
        {
            r2 = { x_ + 1 + w, y_ + 1, (width_ - 2) * (HP_ - role_->HP) / role_->MaxHP, 9 };
            r1 = { x_ + 1, y_ + 1, w, 9 };
        }
        c = { 0xff, 0xb7, 0x4d, 255 };
        Engine::getInstance()->renderSquareTexture(&r2, c, 144);
        c = { 196, 25, 16, 255 };
        Engine::getInstance()->renderSquareTexture(&r1, c, 192);
        font->draw(role_->Name, 20, x_ - 10 - font->getTextDrawSize(role_->Name) * 10, y_ - 4, white);
        font->draw(fmt1::format("{}/{}", role_->HP, role_->MaxHP), 16, x_ + width_ + 10, y_ - 2, white);
    }
    else if (style_ == 2)
    {
        //TextureManager::getInstance()->renderTexture("head", role_->HeadID, x_ - 10, y_ - 10, { 255, 255, 255, 255 }, 255, 0.15, 0.15);
        width_ = 350.0 / 999 * role_->MaxHP;
        BP_Rect r1 = { x_ + 0, y_ + 25, width_, 11 }, r2;
        BP_Color c, c_text;
        Engine::getInstance()->fillColor({ 0, 0, 0, 168 }, r1.x, r1.y, r1.w, r1.h);
        int w = (width_ - 2) * role_->HP / role_->MaxHP;
        if (role_->MaxHP > 0)
        {
            r2 = { x_ + 1 + w, y_ + 26, (width_ - 2) * (HP_ - role_->HP) / role_->MaxHP, 9 };
            r1 = { x_ + 1, y_ + 26, w, 9 };
        }
        c = { 0xff, 0xb7, 0x4d, 255 };
        Engine::getInstance()->renderSquareTexture(&r2, c, 144);
        c = { 196, 25, 16, 255 };
        Engine::getInstance()->renderSquareTexture(&r1, c, 192);
        font->draw(role_->Name, 20, x_ + 10, y_, white);
        auto m = Save::getInstance()->getMagic(role_->EquipMagic[0]);
        if (m)
        {
            font->draw(m->Name, 15, x_ + Font::getTextDrawSize(role_->Name) * 10 + 30, y_ + 5, white);
        }
        font->draw(fmt1::format("{}/{}", role_->HP, role_->MaxHP), 12, x_ + width_ + 10, y_ + 25, white);
        int length = std::max(0.0, role_->Posture * 5);
        int w_tex = TextureManager::getInstance()->getTexture("title", 203)->w;
        int h_tex = TextureManager::getInstance()->getTexture("title", 203)->h;
        double zoomb_x = 1.0 * 450 / w_tex;
        double zoomb_y = 3.0 * 450 / w_tex;
        TextureManager::getInstance()->renderTexture("title", 203, Engine::getInstance()->getStartWindowWidth() / 2 - 450 / 2 + 20, y_ - 10, { 255, 128, 128, 255 }, 255, zoomb_x, zoomb_y);
        double zoom_x = 1.0 * length / w_tex;
        double zoom_y = 3.0 * length / w_tex;
        TextureManager::getInstance()->renderTexture("title", 203, Engine::getInstance()->getStartWindowWidth() / 2 - length / 2 + 20, y_ - 10 - h_tex * (zoom_y / 2 - zoomb_y / 2), { 255, 255, 255, 255 }, 255, zoom_x, zoom_y);
    }
    HP_ = std::max(HP_ - 1 - role_->MaxHP / 1000, role_->HP);
    MP_ = std::max(MP_ - 1 - role_->MaxMP / 1000, role_->MP);
    PhysicalPower_ = std::max(PhysicalPower_ - 1, role_->PhysicalPower);
}
