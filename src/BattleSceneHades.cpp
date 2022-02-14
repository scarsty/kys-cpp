#include "BattleSceneHades.h"
#include "Audio.h"
#include "Font.h"
#include "GameUtil.h"
#include "Head.h"
#include "MainScene.h"
#include "TeamMenu.h"

BattleSceneHades::BattleSceneHades()
{
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

                    TextureManager::getInstance()->renderTexture(tex, d.p.x, d.p.y / 2 + yd, { 0,0,0,255 }, 128, scalex, scaley, d.rot);
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
            Font::getInstance()->draw(te.Text, 15, te.Pos.x, te.Pos.y / 2, te.Color, 255);
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
                if (engine->checkKeyPress(BPK_a))
                {
                    pos_.x -= speed;
                    r->FaceTowards = Towards_LeftDown;
                    r->OperationType = 0;
                }
                if (engine->checkKeyPress(BPK_d))
                {
                    pos_.x += speed;
                    r->FaceTowards = Towards_RightUp;
                    r->OperationType = 0;
                }
                if (engine->checkKeyPress(BPK_w))
                {
                    pos_.y -= speed;
                    r->FaceTowards = Towards_LeftUp;
                    r->OperationType = 0;
                }
                if (engine->checkKeyPress(BPK_s))
                {
                    pos_.y += speed;
                    r->FaceTowards = Towards_RightDown;
                    r->OperationType = 0;
                }
            }
            if (engine->checkKeyPress(BPK_1))
            {
                weapon_ = 1;
            }
            if (engine->checkKeyPress(BPK_2))
            {
                weapon_ = 2;
            }
            if (engine->checkKeyPress(BPK_3))
            {
                weapon_ = 3;
            }
            if (engine->checkKeyPress(BPK_4))
            {
                weapon_ = 4;
            }
        }
        if (engine->checkKeyPress(BPK_w) && engine->checkKeyPress(BPK_d))
        {
            r->FaceTowards = Towards_RightUp;
        }
        if (engine->checkKeyPress(BPK_s) && engine->checkKeyPress(BPK_a))
        {
            r->FaceTowards = Towards_LeftDown;
        }
        //实际的朝向可以不能走到
        if (pos_.x != r->Pos.x || pos_.y != r->Pos.y)
        {
            r->RealTowards = pos_ - r->Pos;
        }

        if (canWalk90(pos_.x, pos_.y, r))
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
            if (engine->checkKeyPress(BPK_j) && r->PhysicalPower >= 10) { index = 0; }
            if (engine->checkKeyPress(BPK_i) && r->PhysicalPower >= 30) { index = 1; }
            if (engine->checkKeyPress(BPK_k) && r->PhysicalPower >= 20) { index = 2; }
            if (engine->checkKeyPress(BPK_m) && r->PhysicalPower >= 10) { index = 3; }
            r->OperationType = index;
            if (index >= 0 && magic[index])
            {
                equip_magics_[index]->setState(NodePass);
                auto m = magic[index];
                r->ActType = m->MagicType;
                r->UsingMagic = m;
                r->ActFrame = 0;
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
        for (auto m : r->getLearnedMagic())
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
            if (canWalk90(p, r, TILE_W / 2))
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
            //帧数，动画，音效等
            if (r->ActType >= 0)
            {
                //音效和动画
                if (r->Acted == 0
                    && r->OperationType >= 0
                    //&& r->ActFrame == r->FightFrame[r->ActType] - 3
                    && r->ActFrame == 2)
                {
                    r->Acted = 1;
                    for (auto m : r->getLearnedMagic())
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
                    if (r->Team == 0)
                    {
                        ae.OperationType = r->OperationType;
                        //if (r->OperationType == 0 && ae.UsingMagic->AttackAreaType != 0)
                        //{
                        //    ae.OperationType = -1;
                        //}
                        if (r->OperationType == 2 && (ae.UsingMagic->AttackAreaType != 1 && ae.UsingMagic->AttackAreaType != 2))
                        {
                            ae.OperationType = -1;
                        }
                        if (r->OperationType == 1 && ae.UsingMagic->AttackAreaType != 3)
                        {
                            ae.OperationType = -1;
                        }
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
                    {
                        int index = r->getMagicOfRoleIndex(ae.UsingMagic);
                        if (index >= 0)
                        {
                            r->MagicLevel[index] = GameUtil::limit(r->MagicLevel[index] + rand_.rand() * 2 + 1, 0, 999);
                        }
                        if (ae.OperationType == 0)
                        {
                            ae.TotalFrame = 10;
                            attack_effects_.push_back(std::move(ae));
                            needMP *= 0.1;
                        }
                        else if (ae.OperationType == 1)
                        {
                            int range = magic->AttackDistance[level_index] + magic->SelectDistance[level_index];
                            int count = range;
                            auto p = ae.Pos;
                            ae.TotalFrame = 60;
                            double angle = r->RealTowards.getAngle();
                            for (int i = 0; i < count; i++)
                            {
                                ae.Pos = p;
                                ae.Speed = { cos(angle + i * 2 * M_PI / count), sin(angle + i * 2 * M_PI / count) };
                                ae.Speed.norm(3);
                                ae.Frame = rand_.rand() * 10;
                                attack_effects_.push_back(ae);
                            }
                            needMP *= 0.4;
                        }
                        else if (ae.OperationType == 2)
                        {
                            double dis = 4096;
                            auto r0 = findNearestEnemy(r->Team, r->Pos.x, r->Pos.y);
                            if (r0)
                            {
                                ae.Speed = r0->Pos - r->Pos;
                            }
                            else
                            {
                                ae.Speed = r->RealTowards;
                            }
                            ae.Speed.norm(5);
                            ae.TotalFrame = 30;
                            attack_effects_.push_back(std::move(ae));
                            needMP *= 0.2;
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
                    }
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
                }
                //动作帧数计算
                if (r->ActFrame >= r->FightFrame[r->ActType] - 2)
                {
                    if (r->CoolDown == 0)
                    {
                        r->ActType = -1;
                        r->ActFrame = 0;
                    }
                    else
                    {
                        r->ActFrame = r->FightFrame[r->ActType] - 2;    //cooldown结束前停在最后行动帧，最后一帧一般是无行动的图
                    }
                }
                else
                {
                    if (r->OperationType == 1)
                    {
                        //if (current_frame2 % 4 == 0)
                        {
                            r->ActFrame++;
                            if (r->ActFrame >= 7)
                            {
                                x_ = rand_.rand_int(2) - rand_.rand_int(2);
                                y_ = rand_.rand_int(2) - rand_.rand_int(2);
                            }
                        }
                    }
                    else
                    {
                        r->ActFrame++;
                    }
                }
            }
            else
            {
                r->ActFrame = 0;
            }

            //计算有人被打中
            for (auto& ae : attack_effects_)
            {
                if (!r->HurtFrame
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

                    ae.Attacker->ExpGot += hurt / 2;
                    r->HurtThisFrame += hurt;
                    //std::vector<std::string> = {};
                    fmt1::print("{} attack {} with {} as {}, hurt {}\n", ae.Attacker->Name, r->Name, ae.UsingMagic->Name, ae.OperationType, hurt);
                }
            }
            //ai
            if (r != role_ && r->Dead == 0)
            {
                if (r->CoolDown == 0)
                {
                    double dis = 4096;
                    Role* r0 = nullptr;
                    for (auto r1 : battle_roles_)
                    {
                        if (r1->Dead == 0 && r->Team != r1->Team)
                        {
                            auto dis1 = EuclidDis(r->Pos, r1->Pos);
                            if (dis1 < dis)
                            {
                                dis = dis1;
                                r0 = r1;
                            }
                        }
                    }
                    if (r0)
                    {
                        r->RealTowards = r0->Pos - r->Pos;
                        r->FaceTowards = Towards_RightDown;
                        if (r->RealTowards.x > 0 && r->RealTowards.y < 0) { r->FaceTowards = Towards_RightUp; }
                        if (r->RealTowards.x < 0 && r->RealTowards.y > 0) { r->FaceTowards = Towards_LeftDown; }
                        if (r->RealTowards.x < 0 && r->RealTowards.y < 0) { r->FaceTowards = Towards_LeftUp; }
                        r->RealTowards.norm(1);
                        if (EuclidDis(r->Pos, r0->Pos) > TILE_W * 3)
                        {
                            r->Pos += r->Speed / 20.0 * r->RealTowards;
                        }
                        else
                        {
                            //attack
                            if (r->PhysicalPower >= 30)
                            {
                                //r->PhysicalPower -= 5;
                                //for (int i = 1; i <= 4; i++)
                                //{
                                //    if (r->FightFrame[i] > 0)
                                //    {
                                int select_magic = rand_.rand() * r->getLearnedMagicCount();
                                auto m = Save::getInstance()->getMagic(r->MagicID[select_magic]);
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
                                break;
                                //}
                           //}
                            }
                        }
                    }
                }
            }
        }
    }

    for (auto r : battle_roles_)
    {
        //此处计算累积伤害
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
                r->Dead = 1;
                r->HP = 0;
                //r->Velocity = r->Pos - ae.Attacker->Pos;
                r->Velocity.norm(15);    //因为已经有击退速度，可以直接利用
                r->Velocity.z = 12;
                r->Velocity.norm(std::min(hurt / 2.5, 30.0));
                r->VelocitytFrame = 15;
                r->Frozen = 2;
                //x_ = rand_.rand_int(2) - rand_.rand_int(2);
                //y_ = rand_.rand_int(2) - rand_.rand_int(2);
                frozen_ = 2;
                slow_ = 5;
            }
        }
        r->HP = GameUtil::limit(r->HP, 0, r->MaxHP);
        r->MP = GameUtil::limit(r->MP, 0, r->MaxMP);
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower, 0, 100);
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
    //效果
    //if (current_frame_ % 2 == 0)
    {
        for (auto& ae : attack_effects_)
        {
            ae.Frame++;
            ae.Pos += ae.Speed;
            if (ae.OperationType == 1)
            {
                auto r = findNearestEnemy(ae.Attacker->Team, ae.Pos.x, ae.Pos.y);
                if (r)
                {
                    auto p = (r->Pos - ae.Pos).norm(0.2);
                    ae.Speed += p;
                    ae.Speed.norm(3);
                }

            }
        }
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
    for (auto& te : text_effects_)
    {
        te.Pos.y -= 2;
        text_effects_[0].Frame++;
    }
    //清理,应增加效果
    auto role_count = battle_roles_.size();
    //for (auto it = battle_roles_.begin(); it != battle_roles_.end();)
    //{
    //    if ((*it)->Dead)
    //    {
    //        it = battle_roles_.erase(it);
    //    }
    //    else
    //    {
    //        it++;
    //    }
    //}
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
    //if (role_count != battle_roles_.size())
    {
        //检测战斗结果
        int battle_result = checkResult();

        //我方胜
        if (battle_result >= 0)
        {
            if (frozen_ == 0 && result_ == -1)
            {
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

    // 正常战斗
    //设置全部角色的位置层，避免今后出错
    //for (auto r : Save::getInstance()->getRoles())
    //{
    //    //r->setRolePositionLayer(&role_layer_);
    //    r->resetBattleInfo();
    //}

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
    if (team0 == 0 && team1 > 0)
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
    r->Acceleration = { 0,0,-4 };
}

Role* BattleSceneHades::findNearestEnemy(int team, double x, double y)
{
    double dis = 4096;
    Role* r0 = nullptr;
    for (auto r1 : battle_roles_)
    {
        if (r1->Dead == 0 && team != r1->Team)
        {
            auto dis1 = EuclidDis(x - r1->Pos.x, y - r1->Pos.y);
            if (dis1 < dis)
            {
                dis = dis1;
                r0 = r1;
            }
        }
    }
    return r0;
}

int BattleSceneHades::calCoolDown(int act_type, int act_type2, Role* r)
{
    int v = 0;
    if (act_type == 1)
    {
        v = r->Fist;
    }
    if (act_type == 2)
    {
        v = r->Sword;
    }
    if (act_type == 3)
    {
        v = r->Knife;
    }
    if (act_type == 4)
    {
        v = r->Unusual;
    }
    int ret[4] = { 60 - v / 2,160 - v,70 - v / 2,10 };
    int m[4] = { 10, 45, 20, 10 };
    return std::max(m[act_type2], ret[act_type2]);
}

int BattleSceneHades::defaultMagicEffect(AttackEffect& ae, Role* r)
{
    int hurt = calMagicHurt(ae.Attacker, r, ae.UsingMagic, EuclidDis(r->Pos, -ae.Attacker->Pos) / TILE_W / 2);
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
        hurt /= 5;
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
    return hurt;
}

void BattleSceneHades::makeSpecialMagicEffect()
{
    //每一帧的效果，返回值暂时无定义
    special_magic_effect_every_frame_["紫霞神功"] =
        [this](Role* r)->int
    {
        if (current_frame_ % 3 == 1)
        {
            r->PhysicalPower++;
        }
        return 0;
    };
    special_magic_effect_every_frame_["十八泥偶"] =
        [this](Role* r)->int
    {
        if (current_frame_ % 600 == 0)
        {
            r->MP += r->MaxMP * 0.1;
        }
        return 0;
    };
    special_magic_effect_every_frame_["洗髓神功"] =
        [this](Role* r)->int
    {
        if (current_frame_ % 600 == 0)
        {
            r->HP += r->MaxHP * 0.05;
        }
        return 0;
    };
    //攻击时的效果，指发动攻击的时候，此时并不考虑防御者，返回值为消耗内力
    special_magic_effect_attack_["九陽神功"] =
        [this](Role* r)->int
    {
        r->MP += 50;    //定身
        return 0;
    };
    //打击时的效果，注意r是防御者，攻击者可以通过ae.Attacker取得，返回值为伤害
    special_magic_effect_beat_["葵花神功"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        r->Frozen += 20;    //定身
        r->MP -= hurt;    //消耗敌人內力
        return hurt;
    };
    special_magic_effect_beat_["獨孤九劍"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        r->Frozen += 10;    //定身
        r->ActType = -1;    //行动打断
        r->OperationType = -1;
        return hurt;
    };
    special_magic_effect_beat_["降龍十八掌"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        r->Velocity.norm(2);    //更强的击退
        r->VelocitytFrame = 20;
        return hurt;
    };
    special_magic_effect_beat_["小無相功"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        r->Frozen += 10;
        return hurt;
    };
    special_magic_effect_beat_["易筋神功"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        hurt *= 1.1;
        return hurt;
    };
    special_magic_effect_beat_["乾坤大挪移"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        ae.Attacker->HurtThisFrame += hurt * 0.05;
        return hurt;
    };
    special_magic_effect_beat_["太玄神功"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        if (ae.OperationType == 3) { hurt *= 2; }
        return hurt;
    };
}
