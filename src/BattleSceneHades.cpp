#include "BattleSceneHades.h"
#include "Audio.h"
#include "Font.h"
#include "GameUtil.h"
#include "Head.h"
#include "MainScene.h"
#include "TeamMenu.h"

BattleSceneHades::BattleSceneHades()
{
    keys_ = *UIKeyConfig::getKeyConfig();
    full_window_ = 1;
    COORD_COUNT = BATTLEMAP_COORD_COUNT;

    earth_layer_.resize(COORD_COUNT);
    building_layer_.resize(COORD_COUNT);

    heads_.resize(TEAMMATE_COUNT);
    int i = 0;
    for (auto& h : heads_)
    {
        h = std::make_shared<Head>();
        h->setAlwaysLight(1);
        addChild(h, 10, 10 + (i++) * 80);
        h->setVisible(false);
    }
    heads_[0]->setVisible(true);

    menu_ = std::make_shared<Menu>();
    menu_->setPosition(300, 30);
    equip_magics_.resize(4);
    for (auto& em : equip_magics_)
    {
        em = std::make_shared<Button>();
    }
    equip_magics_[0]->setText("__________");
    menu_->addChild(equip_magics_[0], 0, 10);
    equip_magics_[1]->setText("__________");
    menu_->addChild(equip_magics_[1], 120, 0);
    equip_magics_[2]->setText("__________");
    menu_->addChild(equip_magics_[2], 240, 10);
    equip_magics_[3]->setText("__________");
    menu_->addChild(equip_magics_[3], 120, 25);
    addChild(menu_);
    menu_->setVisible(false);
    head_boss_ = std::make_shared<Head>();
    head_boss_->setStyle(1);
    head_boss_->setVisible(false);
    addChild(head_boss_);
    makeSpecialMagicEffect();
}

BattleSceneHades::BattleSceneHades(int id) : BattleSceneHades()
{
    setID(id);
}

BattleSceneHades::~BattleSceneHades()
{
}

void BattleSceneHades::setID(int id)
{
    battle_id_ = id;
    info_ = BattleMap::getInstance()->getBattleInfo(id);

    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 0, &earth_layer_);
    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 1, &building_layer_);
}

void BattleSceneHades::draw()
{
    //在这个模式下，使用的是直角坐标
    Engine::getInstance()->setRenderAssistTexture();
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);

    //以下是计算出需要画的区域，先画到一个大图上，再转贴到窗口
    {
        auto p = pos90To45(pos_.x, pos_.y);
        man_x_ = p.x;
        man_y_ = p.y;
    }
    //一整块地面
    if (earth_texture_)
    {
        Engine::getInstance()->setRenderTarget(earth_texture_);
        Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);
        for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
        {
            for (int i = -view_width_region_; i <= view_width_region_; i++)
            {
                int ix = man_x_ + i + (sum / 2);
                int iy = man_y_ - i + (sum - sum / 2);
                auto p = pos45To90(ix, iy);
                if (!isOutLine(ix, iy))
                {
                    int num = earth_layer_.data(ix, iy) / 2;
                    BP_Color color = { 255, 255, 255, 255 };
                    bool need_draw = true;
                    if (need_draw && num > 0)
                    {
                        TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y / 2, color);
                    }
                }
            }
        }

        struct DrawInfo
        {
            std::string path;
            int num;
            Pointf p;
            BP_Color color{ 255, 255, 255, 255 };
            uint8_t alpha = 255;
            int rot = 0;
            int shadow = 0;
            uint8_t white = 0;
        };
        std::vector<DrawInfo> building_vec;
        building_vec.reserve(10000);

        for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
        {
            for (int i = -view_width_region_; i <= view_width_region_; i++)
            {
                int ix = man_x_ + i + (sum / 2);
                int iy = man_y_ - i + (sum - sum / 2);
                auto p = pos45To90(ix, iy);
                if (!isOutLine(ix, iy))
                {
                    int num = building_layer_.data(ix, iy) / 2;
                    if (num > 0)
                    {
                        //TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y/2);
                        DrawInfo info;
                        info.path = "smap";
                        info.num = num;
                        info.p.x = p.x;
                        info.p.y = p.y;
                        info.shadow = 0;
                        building_vec.emplace_back(std::move(info));
                    }
                }
            }
        }

        for (auto r : battle_roles_)
        {
            //if (r->Dead) { continue; }
            DrawInfo info;
            info.path = fmt1::format("fight/fight{:03}", r->HeadID);
            info.color = { 255, 255, 255, 255 };
            info.alpha = 255;
            info.white = 0;
            if (battle_cursor_->isRunning() && !acting_role_->isAuto())
            {
                info.color = { 128, 128, 128, 255 };
                if (inEffect(acting_role_, r))
                {
                    info.color = { 255, 255, 255, 255 };
                }
            }
            info.p = r->Pos;
            info.num = calRolePic(r, r->ActType, r->ActFrame);
            //if (r->HurtFrame)
            //{
            //    if (r->HurtFrame % 2 == 1)
            //    {
            //        info.white = 128;
            //    }
            //}
            if (r->Dead)
            {
                if (r->Frozen == 0)
                {
                    if (r->FaceTowards >= 2)
                    {
                        info.rot = 90;
                    }
                    else
                    {
                        info.rot = 270;
                    }
                }
            }
            if (r->Attention)
            {
                info.alpha = 255 - r->Attention * 4;
            }
            info.shadow = 1;
            //TextureManager::getInstance()->renderTexture(path, pic, r->X1, r->Y1, color, alpha);
            building_vec.emplace_back(std::move(info));
        }

        //effects
        for (auto& ae : attack_effects_)
        {
            //for (auto r : ae.Defender)
            {
                DrawInfo info;
                info.path = ae.Path;
                if (ae.TotalEffectFrame > 0)
                {
                    info.num = ae.Frame % ae.TotalEffectFrame;
                }
                else
                {
                    info.num = 0;
                }
                info.p = ae.Pos;
                info.color = { 255, 255, 255, 255 };
                info.alpha = 192;
                if (ae.FollowRole)
                {
                    info.p = ae.FollowRole->Pos;
                }
                info.shadow = 1;
                if (ae.Attacker && ae.Attacker->Team == 0)
                {
                    info.shadow = 2;
                }
                building_vec.emplace_back(std::move(info));
                //TextureManager::getInstance()->renderTexture(ae.Path, ae.Frame % ae.TotalEffectFrame, ae.X1, ae.Y1 / 2, { 255, 255, 255, 255 }, 192);
            }
        }

        auto sort_building = [](DrawInfo& d1, DrawInfo& d2)
        {
            if (d1.p.y != d2.p.y)
            {
                return d1.p.y < d2.p.y;
            }
            else
            {
                return d1.p.x < d2.p.x;
            }
        };
        std::sort(building_vec.begin(), building_vec.end(), sort_building);

        //影子
        for (auto& d : building_vec)
        {
            if (d.shadow)
            {
                auto tex = TextureManager::getInstance()->getTexture(d.path, d.num);
                if (tex)
                {
                    double scalex = 1, scaley = 0.3;
                    int yd = tex->dy * 0.7;
                    if (d.rot)
                    {
                        scalex = 0.3;
                        scaley = 1;
                        yd = tex->dy * 0.1;
                    }
                    if (d.shadow == 1)
                    {
                        TextureManager::getInstance()->renderTexture(tex, d.p.x, d.p.y / 2 + yd, { 32,32,32,255 }, 128, scalex, scaley, d.rot);
                    }
                    if (d.shadow == 2)
                    {
                        TextureManager::getInstance()->renderTexture(tex, d.p.x, d.p.y / 2 + yd, { 128,128,128,255 }, 128, scalex, scaley, d.rot, 128);
                    }
                }
            }
        }
        for (auto& d : building_vec)
        {
            double scaley = 1;
            if (d.rot)
            {
                scaley = 0.5;
            }
            TextureManager::getInstance()->renderTexture(d.path, d.num, d.p.x, d.p.y / 2 - d.p.z, d.color, d.alpha, scaley, 1, d.rot, d.white);
        }

        for (auto r : battle_roles_)
        {
            renderExtraRoleInfo(r, r->Pos.x, r->Pos.y / 2);
        }

        BP_Color c = { 255, 255, 255, 255 };
        Engine::getInstance()->setColor(earth_texture_, c);
        int w = render_center_x_ * 2;
        int h = render_center_y_ * 2;
        //获取的是中心位置，如贴图应减掉屏幕尺寸的一半
        BP_Rect rect0 = { int(pos_.x - render_center_x_ - x_), int(pos_.y / 2 - render_center_y_ - y_), w, h }, rect1 = { 0, 0, w, h };

        for (auto& te : text_effects_)
        {
            Font::getInstance()->draw(te.Text, te.Size, te.Pos.x, te.Pos.y / 2, te.Color, 255);
        }

        Engine::getInstance()->setRenderAssistTexture();
        if (frozen_ && result_ >= 0)
        {
            rect0.w /= 2;
            rect0.h /= 2;
            rect0.x += rect0.w / 2;
            rect0.y += rect0.h / 2 - 20;
        }
        Engine::getInstance()->renderCopy(earth_texture_, &rect0, &rect1, 0);
    }

    Engine::getInstance()->renderAssistTextureToWindow();

    //if (result_ >= 0)
    //{
    //    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    //}
}

void BattleSceneHades::dealEvent(BP_Event& e)
{
    if (frozen_ > 0)
    {
        x_ = rand_.rand_int(3) - rand_.rand_int(3);
        y_ = rand_.rand_int(3) - rand_.rand_int(3);
        frozen_--;
        return;
    }

    auto engine = Engine::getInstance();
    auto r = role_;

    if (r->Dead)
    {
        for (auto r1 : battle_roles_)
        {
            if (r1->Team == 0 && r1->Dead == 0)
            {
                pos_ = r1->Pos;
            }
        }
    }
    else
    {
        pos_ = r->Pos;
    }
    double speed = std::min(4.0, r->Speed / 30.0);
    if (r->Dead == 0)
    {
        if (r->Frozen == 0 && r->CoolDown == 0)
        {
            //if (current_frame_ % 3 == 0)
            {
                auto axis_x = engine->gameControllerGetAxis(BP_CONTROLLER_AXIS_LEFTX);
                auto axis_y = engine->gameControllerGetAxis(BP_CONTROLLER_AXIS_LEFTY);
                if (abs(axis_x) < 2000) { axis_x = 0; }
                if (abs(axis_y) < 2000) { axis_y = 0; }
                if (axis_x != 0 || axis_y != 0)
                {
                    axis_x = GameUtil::limit(axis_x, -20000, 20000);
                    axis_y = GameUtil::limit(axis_y, -20000, 20000);
                    Pointf axis{ double(axis_x), double(axis_y) };
                    axis *= 1.0 / 20000 / sqrt(2.0);
                    r->RealTowards = axis;
                    r->FaceTowards = readTowardsToFaceTowards(r->RealTowards);
                    pos_ += speed * r->RealTowards;
                }
                Pointf direct;
                if (engine->checkKeyPress(keys_.Left) || engine->checkKeyPress(BPK_LEFT))
                {
                    direct.x = -1;
                    r->FaceTowards = Towards_LeftDown;
                }
                if (engine->checkKeyPress(keys_.Right) || engine->checkKeyPress(BPK_RIGHT))
                {
                    direct.x = 1;
                    r->FaceTowards = Towards_RightUp;
                }
                if (engine->checkKeyPress(keys_.Up) || engine->checkKeyPress(BPK_UP))
                {
                    direct.y = -1;
                    r->FaceTowards = Towards_LeftUp;
                }
                if (engine->checkKeyPress(keys_.Down) || engine->checkKeyPress(BPK_DOWN))
                {
                    direct.y = 1;
                    r->FaceTowards = Towards_RightDown;
                }
                direct.norm(speed);
                pos_ += direct;
                //这样来看同时用手柄和键盘会走得很快，就这样吧
            }
        }
        if (engine->checkKeyPress(keys_.Up) && engine->checkKeyPress(keys_.Right)
            || engine->checkKeyPress(BPK_UP) && engine->checkKeyPress(BPK_RIGHT))
        {
            r->FaceTowards = Towards_RightUp;
        }
        if (engine->checkKeyPress(keys_.Down) && engine->checkKeyPress(keys_.Left)
            || engine->checkKeyPress(BPK_DOWN) && engine->checkKeyPress(BPK_LEFT))
        {
            r->FaceTowards = Towards_LeftDown;
        }
        //实际的朝向可以不能走到
        if (pos_.x != r->Pos.x || pos_.y != r->Pos.y)
        {
            r->RealTowards = pos_ - r->Pos;
        }

        if (canWalk90(pos_, r))
        {
            r->Pos = pos_;
        }

        std::vector<Magic*> magic(4);
        for (int i = 0; i < 4; i++)
        {
            magic[i] = Save::getInstance()->getMagic(r->EquipMagic[i]);
            if (magic[i] && r->getMagicOfRoleIndex(magic[i]) < 0) { magic[i] = nullptr; }
            equip_magics_[i]->setState(NodeNormal);
        }
        if (r->Frozen == 0 && r->CoolDown == 0)
        {
            int index = -1;
            if (r->PhysicalPower >= 10
                && (engine->checkKeyPress(keys_.Light)
                    || engine->gameControllerGetButton(BP_CONTROLLER_BUTTON_X)
                    || (e.type == BP_MOUSEBUTTONDOWN && e.button.button == BP_BUTTON_LEFT)))
            {
                index = 0;
            }
            if (r->PhysicalPower >= 30
                && (engine->checkKeyPress(keys_.Heavy)
                    || engine->gameControllerGetButton(BP_CONTROLLER_BUTTON_Y)
                    || (e.type == BP_MOUSEWHEEL && e.wheel.y > 0)
                    || (e.type == BP_MOUSEBUTTONDOWN && e.button.button == BP_BUTTON_MIDDLE)))
            {
                index = 1;
            }
            if (r->PhysicalPower >= 20
                && (engine->checkKeyPress(keys_.Long)
                    || engine->gameControllerGetButton(BP_CONTROLLER_BUTTON_B)
                    || (e.type == BP_MOUSEBUTTONDOWN && e.button.button == BP_BUTTON_RIGHT)))
            {
                index = 2;
            }
            if (r->PhysicalPower >= 10
                && (engine->checkKeyPress(keys_.Slash)
                    || engine->gameControllerGetButton(BP_CONTROLLER_BUTTON_A)
                    || (e.type == BP_MOUSEWHEEL && e.wheel.y < 0)))
            {
                index = 3;
            }
            r->OperationType = index;
            if (index >= 0 && magic[index])
            {
                equip_magics_[index]->setState(NodePass);
                auto m = magic[index];
                r->ActType = m->MagicType;
                r->UsingMagic = m;
                r->ActFrame = 0;
                //r->Frozen = 5;
                if (index == 0)
                {
                    //轻击
                    //r->CoolDown = 10;
                }
                if (index == 1)
                {
                    //重击
                    //r->CoolDown = 60;
                }
                if (index == 2)
                {
                    //远程
                    //r->CoolDown = 20;
                }
                if (index == 3)
                {
                    //闪身
                    //r->CoolDown = 10;    //冷却更长，有收招硬直
                    r->Velocity = r->RealTowards;
                    r->Velocity.norm(speed * 3);
                    r->VelocitytFrame = 10;
                    r->PhysicalPower -= 2;
                    r->ActType = 0;
                }
                r->CoolDown = calCoolDown(magic[index]->MagicType, index, r);
            }
        }
    }
    backRun1();
}

void BattleSceneHades::dealEvent2(BP_Event& e)
{

}

void BattleSceneHades::backRun1()
{
    if (slow_ > 0)
    {
        if (current_frame_ % 4) { return; }
        //x_ = rand_.rand_int(2) - rand_.rand_int(2);
        //y_ = rand_.rand_int(2) - rand_.rand_int(2);
        slow_--;
    }
    for (auto r : battle_roles_)
    {
        r->HurtThisFrame = 0;
        for (auto m : r->getLearnedMagics())
        {
            if (special_magic_effect_every_frame_.count(m->Name))
            {
                special_magic_effect_every_frame_[m->Name](r);
            }
        }
        decreaseToZero(r->Frozen);
        if (r->Frozen > 0)
        {
            continue;
        }
        if (r->VelocitytFrame > 0)
        {
            if (r == role_ && r->Dead)
            {
                int i = 0;
            }
            auto p = r->Pos + r->Velocity;
            r->Velocity += r->Acceleration;
            int dis = -1;
            if (r->OperationType == 3) { dis = TILE_W / 4; }
            if (canWalk90(p, r, dis))
            {
                r->Pos = p;
            }
            r->VelocitytFrame--;
            //r->FaceTowards = rand_.rand() * 4;
            if (r->Pos.z < 0) { r->Pos.z = 0; }
        }
        else
        {
            r->Velocity = { 0,0 };
            if (r->HP <= 0)
            {
                r->Dead = 1;
            }
        }
        decreaseToZero(r->CoolDown);
        if (r->CoolDown == 0)
        {
            if (current_frame_ % 3 == 0)
            {
                r->PhysicalPower += 1;
            }
            r->MP += 1;
            r->Acted = 0;
            r->ActFrame = 0;
            r->OperationType = -1;
            r->ActType = -1;
        }
        decreaseToZero(r->HurtFrame);
        decreaseToZero(r->Attention);
        decreaseToZero(r->Invincible);
    }

    //if (current_frame_ % 2 == 0)
    {
        int current_frame2 = current_frame_;
        for (auto r : battle_roles_)
        {
            //有行动
            if (r->ActType >= 0)
            {
                //音效和动画
                if (r->Acted == 0
                    && r->OperationType >= 0
                    //&& r->ActFrame == r->FightFrame[r->ActType] - 3
                    && r->ActFrame == calCast(r->ActType, r->OperationType, r))
                {
                    r->Acted = 1;
                    for (auto m : r->getLearnedMagics())
                    {
                        if (special_magic_effect_attack_.count(m->Name))
                        {
                            special_magic_effect_attack_[m->Name](r);
                        }
                    }
                    Magic* magic = nullptr;
                    if (r->UsingMagic)
                    {
                        magic = r->UsingMagic;
                    }
                    else
                    {
                        std::vector<Magic*> v;
                        for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
                        {
                            if (r->MagicID[i] > 0)
                            {
                                auto m = Save::getInstance()->getMagic(r->MagicID[i]);
                                if (m->MagicType == r->ActType)
                                {
                                    v.push_back(m);
                                }
                            }
                        }
                        if (!v.empty())
                        {
                            magic = v[rand_.rand() * v.size()];
                        }
                    }
                    AttackEffect ae;
                    if (magic)
                    {
                        Audio::getInstance()->playESound(magic->SoundID);
                        ae.setEft(magic->EffectID);
                        ae.UsingMagic = magic;
                    }
                    else
                    {
                        Audio::getInstance()->playESound(r->ActType);
                        ae.setEft(11);
                        magic = Save::getInstance()->getMagic(1);
                        ae.UsingMagic = Save::getInstance()->getMagic(1);
                    }
                    r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
                    int level_index = r->getMagicLevelIndex(magic->ID);
                    int needMP = magic->calNeedMP(level_index);
                    ae.TotalFrame = 30;
                    //r->CoolDown += ae.TotalFrame;
                    ae.Attacker = r;
                    r->RealTowards.norm(1);
                    ae.Pos = r->Pos + TILE_W * 2.0 * r->RealTowards;
                    ae.Frame = 0;
                    if (r->Team == 0 && r == role_)
                    {
                        ae.OperationType = r->OperationType;
                        //if (r->OperationType == 0 && ae.UsingMagic->AttackAreaType != 0)
                        //{
                        //    ae.OperationType = -1;
                        //}
                        //if (r->OperationType == 2 && (ae.UsingMagic->AttackAreaType != 1 && ae.UsingMagic->AttackAreaType != 2))
                        //{
                        //    ae.OperationType = -1;
                        //}
                        //if (r->OperationType == 1 && ae.UsingMagic->AttackAreaType != 3)
                        //{
                        //    ae.OperationType = -1;
                        //}
                    }
                    else
                    {
                        if (ae.UsingMagic->AttackAreaType == 0)
                        {
                            ae.OperationType = 0;
                        }
                        else if (ae.UsingMagic->AttackAreaType == 1 || ae.UsingMagic->AttackAreaType == 2)
                        {
                            ae.OperationType = 2;
                        }
                        else if (ae.UsingMagic->AttackAreaType == 3)
                        {
                            ae.OperationType = 1;
                        }
                    }

                    int index = r->getMagicOfRoleIndex(ae.UsingMagic);
                    if (index >= 0)
                    {
                        r->MagicLevel[index] = GameUtil::limit(r->MagicLevel[index] + rand_.rand() * 2 + 1, 0, 999);
                    }
                    if (ae.OperationType == 0)
                    {
                        ae.TotalFrame = 5;
                        attack_effects_.push_back(std::move(ae));
                        needMP *= 0.1;
                    }
                    else if (ae.OperationType == 1)
                    {
                        int range = level_index + 1; //= magic->AttackDistance[level_index] + magic->SelectDistance[level_index];
                        int count = range;
                        auto p = ae.Pos;
                        ae.TotalFrame = 120;
                        double angle = r->RealTowards.getAngle();
                        for (int i = 0; i < count; i++)
                        {
                            double a = angle + i * 2 * M_PI / count;
                            ae.Pos = p;
                            ae.Velocity = { cos(a) ,sin(a) };
                            ae.Velocity.norm(3);
                            ae.Frame = rand_.rand() * 10;
                            attack_effects_.push_back(ae);
                        }
                        needMP *= 0.4;
                    }
                    else if (ae.OperationType == 2)
                    {
                        auto r0 = findNearestEnemy(r->Team, r->Pos);
                        if (r0)
                        {
                            ae.Velocity = r0->Pos - r->Pos;
                        }
                        else
                        {
                            ae.Velocity = r->RealTowards;
                        }
                        ae.Velocity.norm(5);
                        ae.TotalFrame = 90;
                        attack_effects_.push_back(ae);
                        needMP *= 0.2;
                        if (ae.UsingMagic->AttackAreaType == 2)
                        {
                            ae.Velocity.norm(3);
                            ae.TotalFrame = 150;
                            attack_effects_.push_back(ae);
                        }
                    }
                    else if (ae.OperationType == 3)
                    {
                        auto p = ae.Pos;
                        int count = std::min(3, (r->Speed + r->getActProperty(ae.UsingMagic->MagicType)) / 60);
                        for (int i = 0; i < count; i++)
                        {
                            ae.Pos = p + r->Velocity * (i - 1) * 2;
                            ae.Frame += 3;
                            attack_effects_.push_back(ae);
                        }
                        needMP *= 0.05;
                    }
                    fmt1::print("{} use {} as {}\n", ae.Attacker->Name, ae.UsingMagic->Name, ae.OperationType);
                    //else
                    //{
                    //    attack_effects_.push_back(ae);
                    //    int range = magic->AttackDistance[level_index] + magic->SelectDistance[level_index];
                    //    int count = range;
                    //    int x1 = ae.X1;
                    //    int y1 = ae.Y1;
                    //    for (int i = 0; i < count; i++)
                    //    {
                    //        ae.X1 = x1 + (rand_.rand() * 2 - 1) * range * TILE_W / 2;
                    //        ae.Y1 = y1 + (rand_.rand() * 2 - 1) * range * TILE_W / 2;
                    //        ae.Frame += rand_.rand() * 5;
                    //        attack_effects_.push_back(ae);
                    //    }
                    //}
                    r->MP = GameUtil::limit(r->MP - needMP, 0, r->MaxMP);
                    r->UsingMagic = nullptr;
                }
                if (r->OperationType == 1)
                {
                    r->ActFrame++;
                    if (r->ActFrame >= 7)
                    {
                        x_ = rand_.rand_int(2) - rand_.rand_int(2);
                        y_ = rand_.rand_int(2) - rand_.rand_int(2);
                    }
                }
                else
                {
                    r->ActFrame++;
                }
            }

            //ai策略
            if (r != role_ && r->Dead == 0)
            {
                if (r->CoolDown == 0)
                {
                    if (r->UsingMagic == nullptr)
                    {
                        auto v = r->getLearnedMagics();
                        if (!v.empty())
                        {
                            r->UsingMagic = v[rand_.rand()];
                        }
                    }
                    auto r0 = findNearestEnemy(r->Team, r->Pos);
                    if (r0)
                    {
                        r->RealTowards = r0->Pos - r->Pos;
                        r->FaceTowards = readTowardsToFaceTowards(r->RealTowards);
                        r->RealTowards.norm(1);
                        int dis = TILE_W * 3;
                        if (r->UsingMagic)
                        {
                            if (r->UsingMagic->AttackAreaType == 3) { dis = 180; }
                            if (r->UsingMagic->AttackAreaType == 1 || r->UsingMagic->AttackAreaType == 2) { dis = 300; }
                        }
                        if (EuclidDis(r->Pos, r0->Pos) > dis)
                        {
                            auto p = r->Pos + r->Speed / 20.0 * r->RealTowards;
                            if (canWalk90(p, r))
                            {
                                r->Pos = p;
                                r->TurnTowards = -1;
                            }
                            else
                            {
                                std::vector<double> angle[2] = { {M_PI / 3, -M_PI / 3, M_PI * 2 / 3, -M_PI * 2 / 3, M_PI}, {-M_PI / 3, M_PI / 3, -M_PI * 2 / 3, M_PI * 2 / 3, M_PI} };
                                auto rt0 = r->RealTowards;
                                if (r->TurnTowards < 0) { r->TurnTowards = int(rand_.rand() * 2); }
                                auto& angle_v = angle[r->TurnTowards];
                                for (auto a : angle_v)
                                {
                                    r->RealTowards = rt0;
                                    r->RealTowards.rotate(a);
                                    auto p = r->Pos + r->Speed / 20.0 * r->RealTowards;
                                    if (canWalk90(p, r))
                                    {
                                        r->Pos = p;
                                        break;
                                    }
                                }
                            }
                        }
                        else
                        {
                            //attack
                            if (r->PhysicalPower >= 30)
                            {
                                auto m = r->UsingMagic;
                                if (m)
                                {
                                    if (m->AttackAreaType == 0)
                                    {
                                        r->OperationType = 0;
                                    }
                                    else if (m->AttackAreaType == 1 || m->AttackAreaType == 2)
                                    {
                                        r->OperationType = 2;
                                    }
                                    else if (m->AttackAreaType == 3)
                                    {
                                        r->OperationType = 1;
                                    }
                                    r->CoolDown = calCoolDown(m->MagicType, r->OperationType, r);
                                    r->ActFrame = 0;
                                    r->ActType = m->MagicType;
                                    //TextEffect te;
                                    //te.Text = m->Name;
                                    //te.Size = 15;
                                    //te.Type = 1;
                                    //te.Pos.x = r->Pos.x - 15 * te.Text.size() / 3;
                                    //te.Pos.y = r->Pos.y;
                                    //te.Color = { 255, 0, 0, 255 };
                                    //te.Frame = 15;
                                    //text_effects_.push_back(te);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    //效果
    //if (current_frame_ % 2 == 0)
    {
        //帧数
        for (auto& ae : attack_effects_)
        {
            ae.Frame++;
            ae.Pos += ae.Velocity;
            Role* r = nullptr;
            if (ae.Attacker)
            {
                r = findNearestEnemy(ae.Attacker->Team, ae.Pos);
            }
            if (ae.OperationType == 1)
            {
                //auto r = ae.Attacker;
                //if (r)
                //{
                //    auto p = (r->Pos - ae.Pos).norm(9.0 / TILE_W / 2);
                //    ae.Velocity += p;
                //    ae.Velocity.norm(3);
                //}
                //简单的追踪                
                if (r)
                {
                    auto p = (r->Pos - ae.Pos).norm(0.3);
                    ae.Velocity += p;
                    ae.Velocity.norm(3);
                }
            }
            //是否打中了敌人
            if (r && !r->HurtFrame
                && !r->Invincible
                && r->Dead == 0
                && ae.Attacker
                && r->Team != ae.Attacker->Team
                && ae.Defender.count(r) == 0
                && EuclidDis(r->Pos, ae.Pos) <= TILE_W * 2)
            {
                ae.Defender[r]++;
                int hurt = 0;
                if (ae.OperationType >= 0)
                {
                    if (special_magic_effect_beat_.count(ae.UsingMagic->Name) == 0)
                    {
                        hurt = defaultMagicEffect(ae, r);
                    }
                    else
                    {
                        hurt = special_magic_effect_beat_[ae.UsingMagic->Name](ae, r);
                    }
                }
                hurt += 5 * (rand_.rand() - rand_.rand());
                if (hurt <= 0)
                {
                    hurt = 1 + rand_.rand() * 3;
                }
                ae.Frame = ae.TotalFrame;
                ae.Attacker->ExpGot += hurt / 2;
                r->HurtThisFrame += hurt;
                //std::vector<std::string> = {};
                fmt1::print("{} attack {} with {} as {}, hurt {}\n", ae.Attacker->Name, r->Name, ae.UsingMagic->Name, ae.OperationType, hurt);
            }
        }
        //效果间的互相抵消
        if (attack_effects_.size() >= 2)
        {
            for (int i = 0; i < attack_effects_.size() - 2; i++)
            {
                auto& ae1 = attack_effects_[i];
                for (int j = i + 1; j < attack_effects_.size() - 1; j++)
                {
                    auto& ae2 = attack_effects_[j];
                    if (ae1.Attacker && ae2.Attacker
                        && ae1.Attacker->Team != ae2.Attacker->Team && EuclidDis(ae1.Pos, ae2.Pos) < TILE_W * 4)
                    {
                        fmt1::print("{} beat {}, ", ae1.UsingMagic->Name, ae2.UsingMagic->Name);
                        int hurt1 = calMagicHurt(ae1.Attacker, ae2.Attacker, ae1.UsingMagic);
                        int hurt2 = calMagicHurt(ae2.Attacker, ae1.Attacker, ae2.UsingMagic);
                        ae1.Weaken += hurt2;
                        ae2.Weaken += hurt1;
                        if (ae1.Weaken > hurt1)
                        {
                            //直接设置帧数为一个大值，下面就会直接删掉了
                            ae1.Frame = ae1.TotalFrame;
                            fmt1::print("{} ", ae1.UsingMagic->Name);
                        }
                        if (ae2.Weaken > hurt2)
                        {
                            ae2.Frame = ae2.TotalFrame;
                            fmt1::print("{} ", ae2.UsingMagic->Name);
                        }
                        fmt1::print("loss\n");
                    }
                }
            }
        }
        //删除播放完毕的
        for (auto it = attack_effects_.begin(); it != attack_effects_.end();)
        {
            if (it->Frame >= it->TotalFrame)
            {
                it = attack_effects_.erase(it);
            }
            else
            {
                it++;
            }
        }
    }

    //此处计算累积伤害
    for (auto r : battle_roles_)
    {
        int hurt = r->HurtThisFrame;
        if (hurt > 0)
        {
            TextEffect te;
            te.Color = { 255,255,255,255 };
            if (r->Team == 0)
            {
                te.Color = { 255,0,0,255 };
            }
            te.Text = std::to_string(-hurt);
            te.Pos = r->Pos;
            te.Pos.x -= 7.5 * Font::getTextDrawSize(te.Text);
            te.Pos.y -= 50;
            text_effects_.push_back(std::move(te));
            AttackEffect ae1;
            ae1.FollowRole = r;
            //ae1.EffectNumber = eft[rand_.rand() * eft.size()];
            ae1.setPath(fmt1::format("eft/bld{:03}", int(rand_.rand() * 5)));
            ae1.TotalFrame = ae1.TotalEffectFrame;
            ae1.Frame = 0;
            attack_effects_.push_back(std::move(ae1));
            r->HP -= hurt;
            if (r->HP <= 0)
            {
                fmt1::print("{} has been beat\n", r->Name);
                r->Dead = 1;
                r->HP = 0;
                //r->Velocity = r->Pos - ae.Attacker->Pos;
                r->Velocity.norm(15);    //因为已经有击退速度，可以直接利用
                r->Velocity.z = 12;
                r->Velocity.norm(std::min(hurt / 2.5, 30.0));
                r->VelocitytFrame = 15;
                r->Frozen = 2;
                x_ = rand_.rand_int(2) - rand_.rand_int(2);
                y_ = rand_.rand_int(2) - rand_.rand_int(2);
                dying_ = r;
                //frozen_ = 2;
                //slow_ = 5;
            }
        }
        r->HP = GameUtil::limit(r->HP, 0, r->MaxHP);
        r->MP = GameUtil::limit(r->MP, 0, r->MaxMP);
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower, 0, 100);
    }

    //处理文字
    {
        for (auto& te : text_effects_)
        {
            if (te.Type == 0) { te.Pos.y -= 2; }
            te.Frame++;
        }
        for (auto it = text_effects_.begin(); it != text_effects_.end();)
        {
            if (it->Frame >= 30)
            {
                it = text_effects_.erase(it);
            }
            else
            {
                it++;
            }
        }
    }

    {
        //人物出场
        if (getTeamMateCount(1) < 5)
        {
            if (!enemies_.empty())
            {
                battle_roles_.push_back(enemies_.front());
                enemies_.pop_front();
                battle_roles_.back()->Attention = 30;
                battle_roles_.back()->CoolDown = 30;
                battle_roles_.back()->Invincible = 30;
            }
        }
        //最后一个人亮血条
        if (enemies_.empty())
        {
            head_boss_->setVisible(true);
        }
        //检测战斗结果
        int battle_result = checkResult();

        if (battle_result >= 0)
        {
            if (frozen_ == 0 && result_ == -1)
            {
                pos_ = dying_->Pos;
                frozen_ = 60;
                slow_ = 20;
                result_ = battle_result;
            }
            if (frozen_ == 0 && slow_ == 0 && (result_ == 0 || result_ == 1))
            {
                menu_->setVisible(false);
                calExpGot();
                setExit(true);
            }
        }
    }
}

void BattleSceneHades::onEntrance()
{
    calViewRegion();
    Audio::getInstance()->playMusic(info_->Music);
    //注意此时才能得到窗口的大小，用来设置头像的位置
    head_self_->setPosition(10, 10);
    head_boss_->setPosition(Engine::getInstance()->getWindowWidth() / 2 - head_boss_->getWidth() / 2, Engine::getInstance()->getWindowHeight() - 100);
    addChild(MainScene::getInstance()->getWeather());

    earth_texture_ = Engine::getInstance()->createARGBRenderedTexture(COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);

    //首先设置位置和阵营，其他的后面统一处理
    //敌方
    for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
    {
        auto r = Save::getInstance()->getRole(info_->Enemy[i]);
        if (r)
        {
            r->resetBattleInfo();
            enemies_.push_back(r);
            r->setPositionOnly(info_->EnemyX[i], info_->EnemyY[i]);
            r->Team = 1;
            readFightFrame(r);
            r->FaceTowards = rand_.rand_int(4);
            man_x_ = r->X();
            man_y_ = r->Y();
        }
    }

    //初始状态
    for (auto r : enemies_)
    {
        setRoleInitState(r);
    }
    pos_ = enemies_[0]->Pos;

    //敌人按能力从低到高，依次出场
    std::sort(enemies_.begin(), enemies_.end(), [](Role* l, Role* r) { return l->MaxHP + l->Attack < r->MaxHP + r->Attack; });
    if (!enemies_.empty()) { head_boss_->setRole(enemies_.back()); }
    for (int i = 0; i < 6; i++)
    {
        if (!enemies_.empty())
        {
            battle_roles_.push_back(enemies_.front());
            enemies_.pop_front();
        }
    }

    //判断是不是有自动战斗人物
    if (info_->AutoTeamMate[0] >= 0)
    {
        for (int i = 0; i < TEAMMATE_COUNT; i++)
        {
            auto r = Save::getInstance()->getRole(info_->AutoTeamMate[i]);
            if (r)
            {
                friends_.push_back(r);
                r->Auto = 2;    //由AI控制
            }
        }
    }
    else
    {
        auto team_menu = std::make_shared<TeamMenu>();
        team_menu->setMode(1);
        team_menu->setForceMainRole(true);
        team_menu->run();
        friends_ = team_menu->getRoles();
    }
    //队友
    role_ = Save::getInstance()->getRole(0);
    bool have_main = false;
    for (auto r : friends_)
    {
        if (r == role_) { have_main = true; }
    }
    if (!have_main)
    {
        friends_.insert(friends_.begin(), role_);
    }
    for (int i = 0; i < friends_.size(); i++)
    {
        auto r = friends_[i];
        if (r)
        {
            r->resetBattleInfo();
            battle_roles_.push_back(r);
            r->setPositionOnly(info_->TeamMateX[i], info_->TeamMateY[i]);
            r->Team = 0;
            setRoleInitState(r);
            heads_[i]->setRole(r);
        }
    }

    //head_self_->setRole(role_);

    for (int i = 0; i < equip_magics_.size(); i++)
    {
        auto m = Save::getInstance()->getMagic(role_->EquipMagic[i]);
        if (m && role_->getMagicOfRoleIndex(m) < 0) { m = nullptr; }
        if (m)
        {
            std::string text = m->Name;
            text += std::string(10 - Font::getTextDrawSize(text), ' ');
            equip_magics_[i]->setText(text);
        }
        else
        {
            equip_magics_[i]->setText("__________");
        }
    }
    menu_->setVisible(true);
}

void BattleSceneHades::onExit()
{
    Engine::getInstance()->destroyTexture(earth_texture_);
}

void BattleSceneHades::renderExtraRoleInfo(Role* r, double x, double y)
{
    if (r == nullptr || r->Dead)
    {
        return;
    }
    // 画个血条
    BP_Color outline_color = { 0, 0, 0, 128 };
    BP_Color background_color = { 0, 255, 0, 128 };    // 我方绿色
    if (r->Team == 1)
    {
        // 敌方红色
        background_color = { 255, 0, 0, 128 };
    }
    int hp_max_w = 24;
    int hp_x = x - hp_max_w / 2;
    int hp_y = y - 60;
    int hp_h = 3;
    double perc = ((double)r->HP / r->MaxHP);
    if (perc < 0)
    {
        perc = 0;
    }
    double alpha = 1;
    if (r->HP <= 0)
    {
        alpha = dead_alpha_ / 255.0;
    }
    BP_Rect r0 = { hp_x - 1, hp_y - 1, hp_max_w + 2, hp_h + 2 };
    Engine::getInstance()->renderSquareTexture(&r0, outline_color, 128 * alpha);
    BP_Rect r1 = { hp_x, hp_y, int(perc * hp_max_w), hp_h };
    Engine::getInstance()->renderSquareTexture(&r1, background_color, 192 * alpha);
}

int BattleSceneHades::checkResult()
{
    int team0 = getTeamMateCount(0);
    int team1 = enemies_.size() + getTeamMateCount(1);
    if (team0 > 0 && team1 == 0)
    {
        return 0;
    }
    if (team0 == 0 && team1 >= 0)
    {
        return 1;
    }
    return -1;
}

void BattleSceneHades::setRoleInitState(Role* r)
{
    BattleScene::setRoleInitState(r);
    if (r->Team == 0)
    {
        r->HP = r->MaxHP;
        r->MP = r->MaxMP;
    }
    else
    {
        r->HP = r->MaxHP = r->MaxHP * 1;
        r->MP = r->MaxMP = r->MaxMP * 1;
    }

    auto p = pos45To90(r->X(), r->Y());

    r->Pos.x = p.x;
    r->Pos.y = p.y;
    if (r->FaceTowards == Towards_RightDown)
    {
        r->RealTowards = { 1, 1 };
    }
    if (r->FaceTowards == Towards_RightUp)
    {
        r->RealTowards = { 1, -1 };
    }
    if (r->FaceTowards == Towards_LeftDown)
    {
        r->RealTowards = { -1, 1 };
    }
    if (r->FaceTowards == Towards_LeftUp)
    {
        r->RealTowards = { -1, -1 };
    }
    r->Acceleration = { 0,0,gravity_ };
}

Role* BattleSceneHades::findNearestEnemy(int team, Pointf p)
{
    double dis = 4096;
    Role* r0 = nullptr;
    for (auto r1 : battle_roles_)
    {
        if (r1->Dead == 0 && team != r1->Team)
        {
            auto dis1 = EuclidDis(p, r1->Pos);
            if (dis1 < dis)
            {
                dis = dis1;
                r0 = r1;
            }
        }
    }
    return r0;
}

//前摇
int BattleSceneHades::calCast(int act_type, int operation_type, Role* r)
{
    int v[4] = { 2, 20, 5, 3 };
    if (operation_type >= 0 && operation_type <= 3)
    {
        return v[operation_type];
    }
    return 0;
}

//冷却减去前摇就是后摇
//需注意攻击判定可能仍然存在，严格来说攻击判定存在的时间加上前摇应小于冷却
int BattleSceneHades::calCoolDown(int act_type, int operation_type, Role* r)
{
    int i = r->getWeapon(act_type);
    int v[4] = { 60 - i / 2, 160 - i, 70 - i / 2, 10 };
    int min_v[4] = { 10, 45, 15, 10 };
    if (operation_type >= 0 && operation_type <= 3)
    {
        int c = std::max(min_v[operation_type], v[operation_type]);
        if (r->AttackTwice > 0)
        {
            c *= 0.666;
        }
        return c;
    }
    else
    {
        return 0;
    }
}

int BattleSceneHades::defaultMagicEffect(AttackEffect& ae, Role* r)
{
    int hurt = calMagicHurt(ae.Attacker, r, ae.UsingMagic, EuclidDis(r->Pos, -ae.Attacker->Pos) / TILE_W / 2) - ae.Weaken / 2;
    //这个击退好像效果不太对
    r->Velocity = r->Pos - ae.Attacker->Pos;
    r->Velocity.norm(1);
    r->VelocitytFrame = 10;
    r->HurtFrame = 10;
    if (ae.OperationType == 0)
    {
        hurt /= 10;
    }
    if (ae.OperationType == 1)
    {
        hurt /= 20;
        //ae.Frame = ae.TotalFrame + 1;
    }
    if (ae.OperationType == 2)
    {
        hurt /= 20;
        //ae.Frame = ae.TotalFrame + 1;
    }
    if (ae.OperationType == 3)
    {
        hurt /= 30;
        r->Frozen += 5;
    }
    if (r->MP >= hurt / 10)
    {
        r->MP -= hurt / 10;    //用内力抵消硬直
    }
    else
    {
        r->Frozen += 10;    //硬直
        //r->ActType = -1;
        //r->ActType2 = -1;
    }
    if (ae.UsingMagic)
    {
        int act_type = ae.UsingMagic->MagicType;
        if (rand_.rand() < r->getWeapon(act_type) / 200.0)
        {
            if (act_type == 2)
            {
                r->Frozen += 10;    //拳法打硬直
            }
            if (act_type == 2)
            {
                ae.Attacker->CoolDown *= 0.5;    //剑法冷却缩短
            }
            if (act_type == 3)
            {
                hurt *= 1.5;    //刀法暴击
            }
            if (act_type == 4)
            {
                //特殊会随机附加行动方向
                Pointf p{ rand_.rand(), rand_.rand(), 0 };
                p.norm(1);
                r->Velocity += p;
            }
        }
    }
    return hurt;
}

int BattleSceneHades::calRolePic(Role* r, int style, int frame)
{
    if (r->FightFrame[style] <= 0)
    {
        style = -1;
    }
    if (style == -1)
    {
        for (int i = 0; i < 5; i++)
        {
            if (r->FightFrame[i] > 0)
            {
                return r->FightFrame[i] * r->FaceTowards;
            }
        }
    }
    else
    {
        int total = 0;
        for (int i = 0; i < 5; i++)
        {
            if (i == style)
            {
                if (frame < r->FightFrame[style] - 2)
                {
                    return total + r->FightFrame[style] * r->FaceTowards + frame;
                }
                else
                {
                    return total + r->FightFrame[style] * r->FaceTowards + r->FightFrame[style] - 2;
                }
            }
            total += r->FightFrame[i] * 4;
        }
    }
    return r->FaceTowards;
}

void BattleSceneHades::makeSpecialMagicEffect()
{
}
